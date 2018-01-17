#include <iostream>
#include "jumandic-svc.grpc.pb.h"
#include <grpc++/grpc++.h>
#include "jumandic/shared/jumandic_env.h"
#include "jumandic/shared/jumanpp_args.h"
#include "util/bounded_queue.h"
#include "core/proto/lattice_dump_output.h"
#include <thread>
#include <stack>

using jumanpp::grpc::JumanppJumandic;

struct CallImpl {
  virtual ~CallImpl() = default;
  virtual void Handle() = 0;
};

class JumanppGrpcEnv {
  jumanpp::grpc::JumanppJumandic::AsyncService service_;
  grpc::ServerBuilder bldr_;
  std::unique_ptr<grpc::Server> server_;
  jumanpp::util::bounded_queue<std::function<void()>> work_;
  std::vector<std::thread> threads_;
  std::atomic<bool> continue_{true};
  std::unique_ptr<grpc::ServerCompletionQueue> queue_;
  jumanpp::jumandic::JumanppConf jumandicConf;
  jumanpp::jumandic::JumanppExec jppEnv;

  static void RunThread(JumanppGrpcEnv *env) {
    while (env->continue_.load(std::memory_order_consume)) {
      auto f = env->work_.waitFor();
      f();
    }
  }

  std::stack<jumanpp::core::analysis::Analyzer*> analyzerCache_;
  std::mutex dataMutex_;
  std::vector<CallImpl*> calls_;

public:

  JumanppGrpcEnv(jumanpp::StringPiece path, int port) {
    auto s = jumanpp::jumandic::parseCfgFile(path, &jumandicConf, 1);
    if (!s) {
      std::cerr << s;
      exit(1);
    }

    s = jppEnv.init(jumandicConf);
    if (!s) {
       std::cerr << s;
      exit(1);
    }


    bldr_.RegisterService(&service_);
    std::string host{"[::]:"};
    host += std::to_string(port);
    int selectedPort;
    auto creds = grpc::InsecureServerCredentials();
    bldr_.AddListeningPort(host, creds, &selectedPort);
    queue_ = bldr_.AddCompletionQueue(true);
  }

  void Start(int threads) {
    server_ = bldr_.BuildAndStart();
    work_.initialize(static_cast<unsigned int>(threads * 2));
    for (int i = 0; i < threads; ++i) {
      threads_.emplace_back(RunThread, this);
    }

    for (auto call: calls_) {
      call->Handle();
    }

    bool ok = false;
    void *ptr;

    while (queue_->Next(&ptr, &ok) && ok) {
      static_cast<CallImpl *>(ptr)->Handle();
    }
  }

  template<typename Fn>
  void push(Fn &&fn) {
    auto func = std::function<void()>{std::move(fn)}; // NOLINT
    while (!work_.offer(std::move(func))) { // NOLINT
      std::this_thread::yield();
    }
  }

  jumanpp::grpc::JumanppJumandic::AsyncService *service() {
    return &service_;
  }

  jumanpp::core::analysis::Analyzer *analyzer() {
    {
      std::unique_lock<std::mutex> lock{dataMutex_};
      if (!analyzerCache_.empty()) {
        auto ptr = analyzerCache_.top();
        analyzerCache_.pop();
        return ptr;
      }
      auto a = new jumanpp::core::analysis::Analyzer{};
      auto s = jppEnv.initAnalyzer(a);
      if (!s) {
        std::cerr << s;
        exit(1);
      }
      return a;
    }
  }

  void returnAnalyzer(jumanpp::core::analysis::Analyzer* value) {
    std::unique_lock<std::mutex> lock{dataMutex_};
    analyzerCache_.push(value);
  }


  ~JumanppGrpcEnv() {
    server_->Shutdown();
    queue_->Shutdown();

    void *ignoredTag;
    bool ignoredStatus;
    while (queue_->Next(&ignoredTag, &ignoredStatus)) {
      delete static_cast<CallImpl *>(ignoredTag);
    }
    continue_.store(false);
    for (auto &t: threads_) {
      auto f = []() {};
      work_.offer(f);
    }
    for (auto &t: threads_) {
      t.join();
    }
    while (!analyzerCache_.empty()) {
      delete analyzerCache_.top();
      analyzerCache_.pop();
    }
  }

  template <typename T, typename... Args>
  void RegisterCall(Args&&... args) {
    auto call = new T{this, queue_.get(), std::forward<Args>(args)...};
    calls_.push_back(call);
  };

};

class ScopedAnalyzer {
  JumanppGrpcEnv* env_;
  jumanpp::core::analysis::Analyzer* analyzer_;
public:
  explicit ScopedAnalyzer(JumanppGrpcEnv* env): env_{env}, analyzer_{env->analyzer()} {}
  ~ScopedAnalyzer() {
    env_->returnAnalyzer(analyzer_);
  }

  jumanpp::core::analysis::Analyzer* value() const { return analyzer_; }
};

template<typename T, typename Child>
struct CallImplBase : public CallImpl {
  JumanppGrpcEnv *env_;
  grpc::ServerCompletionQueue *queue;

  grpc::ServerContext context_;
  jumanpp::grpc::AnalysisRequest request_;
  grpc::ServerAsyncResponseWriter<T> writer_{&context_};

  CallImplBase(JumanppGrpcEnv *env, grpc::ServerCompletionQueue *queue) : env_{env}, queue{queue} {}

  CallImplBase(const CallImplBase &o) : env_{o.env_}, queue{o.queue} {}

  enum CallStatus {
    Created, Processing, Finished
  };

  CallStatus state = Created;

  void Handle() override {
    auto cld = static_cast<Child *>(this);
    switch (state) {
      case Created: {
        cld->Request();
        state = Processing;
        break;
      }
      case Processing: {
        auto copy = new Child{*cld};
        copy->Handle();
        cld->state = Finished;
        env_->push([cld]() {
          cld->Process();
        });
        break;
      }
      case Finished: {
        delete cld;
      }
    }
  }
};

struct DumpCallImpl : public CallImplBase<jumanpp::LatticeDump, DumpCallImpl> {
  DumpCallImpl(JumanppGrpcEnv *env, grpc::ServerCompletionQueue *queue) : CallImplBase::CallImplBase(env, queue) {}

  void Request() {
    env_->service()->RequestLatticeDump(&context_, &request_, &writer_, queue, queue, this);
  }

  void Process() {
    ScopedAnalyzer sa{env_};
    auto an = sa.value();
    jumanpp::core::output::LatticeDumpOutput out{false, false};
    auto s = out.initialize(an->impl(), &an->scorer()->feature->weights());
    if (!s) {
      std::cerr << s;
      writer_.FinishWithError(grpc::Status{grpc::StatusCode::INTERNAL, "failed to initialize output"}, this);
      return;
    }

    s = an->analyze(request_.sentence());
    if (!s) {
      std::cerr << s;
      writer_.FinishWithError(grpc::Status{grpc::StatusCode::INTERNAL,
                                           "failed to analyze sentence key:" + request_.key() + " sentence: " +
                                           request_.sentence()}, this);
      return;
    }

    s = out.format(*an, request_.key());
    if (!s) {
      std::cerr << s;
      writer_.FinishWithError(grpc::Status{grpc::StatusCode::INTERNAL, "failed to format a reply"}, this);
      return;
    }

    writer_.Finish(*out.objectPtr(), grpc::Status::OK, this);
  }
};

struct DumpWithFeaturesCallImpl : public CallImplBase<jumanpp::LatticeDump, DumpWithFeaturesCallImpl> {
  DumpWithFeaturesCallImpl(JumanppGrpcEnv *env, grpc::ServerCompletionQueue *queue) : CallImplBase::CallImplBase(env,
                                                                                                                 queue) {}

  void Request() {
    env_->service()->RequestLatticeDumpWithFeatures(&context_, &request_, &writer_, queue, queue, this);
  }

  void Process() {
    ScopedAnalyzer sa{env_};
    auto an = sa.value();
    jumanpp::core::output::LatticeDumpOutput out{true, false};
    auto s = out.initialize(an->impl(), &an->scorer()->feature->weights());
    if (!s) {
      std::cerr << s;
      writer_.FinishWithError(grpc::Status{grpc::StatusCode::INTERNAL, "failed to initialize output"}, this);
      return;
    }

    s = an->analyze(request_.sentence());
    if (!s) {
      std::cerr << s;
      writer_.FinishWithError(grpc::Status{grpc::StatusCode::INTERNAL,
                                           "failed to analyze sentence key:" + request_.key() + " sentence: " +
                                           request_.sentence()}, this);
      return;
    }

    s = out.format(*an, request_.key());
    if (!s) {
      std::cerr << s;
      writer_.FinishWithError(grpc::Status{grpc::StatusCode::INTERNAL, "failed to format a reply"}, this);
      return;
    }

    writer_.Finish(*out.objectPtr(), grpc::Status::OK, this);
  }
};


int main(int argc, char const *argv[]) {
  if (argc != 4) {
    std::cerr << "Must pass 3 arguments: config file location, port and number of threads\n";
  }

  jumanpp::StringPiece filename = jumanpp::StringPiece::fromCString(argv[1]);
  int port = std::atoi(argv[2]);
  int nthreads = std::atoi(argv[3]);
  JumanppGrpcEnv env{filename, port};
  env.RegisterCall<DumpCallImpl>();
  env.RegisterCall<DumpWithFeaturesCallImpl>();
  env.Start(nthreads);
  return 0;
}