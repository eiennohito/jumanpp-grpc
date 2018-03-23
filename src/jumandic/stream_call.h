//
// Created by Arseny Tolmachev on 2018/03/09.
//

#ifndef JUMANPP_GRPC_REPLY_QUEUE_H
#define JUMANPP_GRPC_REPLY_QUEUE_H

#include <deque>
#include "service_env.h"
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

namespace jumanpp {
namespace grpc {

template <typename T, void (T::*Fn)()>
class Forwarder: public CallImpl {
  T* impl_;

public:
  explicit Forwarder(T* ptr): impl_{ptr} {}

  void Handle() override {
    (impl_->*Fn)();
  }
};

template <typename Out, typename Child>
struct BidiStreamCallBase: public CallImpl {
  JumanppGrpcEnv2* env_;
  ::grpc::ServerContext context_;
  ::grpc::ServerAsyncReaderWriter<Out, AnalysisRequest> rw_{&context_};
  AnalysisRequest input_;
  std::deque<CachedAnalyzer*> analyzers_;
  JumanppConfig config_;
  bool allFeatures_ = false;

  enum CallState {
    Initial,
    WaitCall,
    Working,
    Replying,
    Failed
  };

  std::atomic<CallState> state_{Initial};
  std::mutex mutex_;

public:

  BidiStreamCallBase(JumanppGrpcEnv2* env): env_{env} {}

  // Will be called for new calls
  void Handle() override {
    if (state_ == WaitCall) {
      Child* cld = new Child{env_}; //fork call
      state_ = Working;
      rw_.Read(&input_, &inputTag_);
      cld->Handle();
    } else {
      state_ = WaitCall;
      child().startRequest();
    }
  }

  void InputReady() {
    config_.CopyFrom(env_->defaultConfig());
    auto& clientMeta = context_.client_metadata();
    auto iter = clientMeta.find("jumanpp-config-bin");
    if (iter != clientMeta.end()) {
      auto& data = iter->second;
      ::google::protobuf::io::ArrayInputStream is(data.data(), data.size());
      ::google::protobuf::io::CodedInputStream cis(&is);
      if (!config_.MergeFromCodedStream(&cis)) {
        state_ = Failed;
        rw_.Finish(::grpc::Status{::grpc::StatusCode::INVALID_ARGUMENT, "invalid config header"}, &outputTag_);
        return;
      }
    }

    auto an = env_->analyzers().acquire(config_, input_, allFeatures_);
    if (an == nullptr) {
      state_ = Failed;
      rw_.Finish(::grpc::Status{::grpc::StatusCode::ABORTED, "no available analyzer"}, &outputTag_);
      return;
    }

    Status s = an->readInput(input_, env_->analyzers());
    if (!s) {
      state_ = Failed;
      env_->analyzers().release(an); //Release analyzer
      rw_.Finish(::grpc::Status{::grpc::StatusCode::INVALID_ARGUMENT, s.message().str()}, &outputTag_);
      return;
    }

    {
      std::lock_guard<std::mutex> guard(mutex_);
      analyzers_.push_front(an);
    }

    rw_.Read(&input_, &inputTag_); // allow to acquire a new message
    // after this line there could be a new parallel request on this call

    s = an->analyze(); //the heaviest operation is this, it is parallel

    if (!s) {
      std::lock_guard<std::mutex> guard(mutex_);
      state_ = Failed;
      rw_.Finish(::grpc::Status{::grpc::StatusCode::INTERNAL, s.message().str()}, &outputTag_);
      auto iter = std::find(analyzers_.begin(), analyzers_.end(), an);
      analyzers_.erase(iter);
      env_->analyzers().release(an);
    } else if (state_ != Replying) {
      std::lock_guard<std::mutex> guard(mutex_);
      auto an2 = analyzers_.back(); // always should be at least 1 element here
      if (an2->hasResult()) { //always true for ourselves, can be false if we are faster
        child().sendReply(an2);
        state_ = Replying;
        analyzers_.pop_back();
        env_->analyzers().release(an2);
      }
    }
  }

  void OutputReady() {
    if (state_ == Failed) {
      for (auto& an: analyzers_) {
        env_->analyzers().release(an); //release all current analyzers
      }
      delete this; //all finished, bye
      return;
    }

    std::lock_guard<std::mutex> guard(mutex_);
    if (analyzers_.empty()) {
      state_ = Working;
      return;
    }

    auto an = analyzers_.back();
    if (an->hasResult()) {
      child().sendReply(an);
      analyzers_.pop_back();
      env_->analyzers().release(an);
    } else {
      state_ = Working;
    }
  }

protected:
  Forwarder<BidiStreamCallBase, &BidiStreamCallBase<Out, Child>::InputReady> inputTag_{this};
  Forwarder<BidiStreamCallBase, &BidiStreamCallBase<Out, Child>::OutputReady> outputTag_{this};
  Child& child() { return static_cast<Child&>(*this); }
};

} // namespace grpc
} // namespace jumanpp

#endif //JUMANPP_GRPC_REPLY_QUEUE_H
