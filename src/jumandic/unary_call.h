//
// Created by Arseny Tolmachev on 2018/03/09.
//

#ifndef JUMANPP_GRPC_UNARY_CALL_H
#define JUMANPP_GRPC_UNARY_CALL_H

#include <atomic>
#include <grpc++/grpc++.h>
#include <grpc++/impl/codegen/async_unary_call.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <jumandic-svc.pb.h>
#include "interfaces.h"
#include "service_env.h"

namespace jumanpp {
namespace grpc {

template <typename Reply, typename Child>
class BaseUnaryCall: public CallImpl {
  enum State {
    Initial,
    Compute,
    Finished
  };

  std::atomic<State> state_{Initial};
protected:

  ::grpc::ServerContext context_;
  ::grpc::ServerAsyncResponseWriter<Reply> replier_{&context_};
  JumanppGrpcEnv* env_;
  JumanppConfig config_;

public:

  BaseUnaryCall(JumanppGrpcEnv* env): env_{env} {}

  void Handle() override {
    auto state = state_.load(std::memory_order_acquire);
    if (state == Initial) {
      child().startCall();
      state_ = Compute;
    } else if (state == Compute) {
      auto copy = new Child(env_);
      copy->Handle(); //fork call

      config_.CopyFrom(env_->defaultConfig());
      auto& clientMeta = context_.client_metadata();
      auto iter = clientMeta.find("jumanpp-config-bin");
      if (iter != clientMeta.end()) {
        auto& data = iter->second;
        ::google::protobuf::io::ArrayInputStream is(data.data(), data.size());
        ::google::protobuf::io::CodedInputStream cis(&is);
        if (!config_.MergeFromCodedStream(&cis)) {
          state_.store(Finished, std::memory_order_release);
          replier_.FinishWithError(::grpc::Status{::grpc::StatusCode::INVALID_ARGUMENT, "invalid config header"}, this);
          return;
        }
      }

      child().handleCall(); //actual logic
      state_.store(Finished, std::memory_order_release);
    } else if (state == Finished) {
      delete this;
    }
  }

  Child& child() { return static_cast<Child&>(*this); }
};


template <typename Reply, typename Child>
class AnaReqBasedUnaryCall: public BaseUnaryCall<Reply, Child> {
protected:
  AnalysisRequest req_;
  bool allFeatures_ = false;
public:
  explicit AnaReqBasedUnaryCall(JumanppGrpcEnv* env): BaseUnaryCall<Reply, Child>::BaseUnaryCall(env) {}

  void handleCall() {
    ScopedAnalyzer ana{this->env_->analyzers(), this->config_, req_, allFeatures_};
    if (!ana) {
      this->replier_.FinishWithError(
        ::grpc::Status{::grpc::StatusCode::INTERNAL, "failed to acquire analyzer"},
        this);
      return;
    }

    Status s = ana.value()->readInput(req_, this->env_->analyzers());

    if (req_.has_config()) {
      auto& cfg = req_.config();
      this->config_.MergeFrom(cfg);
    }

    if (!s) {
      this->replier_.FinishWithError(
        ::grpc::Status{::grpc::StatusCode::INVALID_ARGUMENT, s.message().str()},
        this);
      return;
    }

    s = ana.value()->analyze();
    if (!s) { //failed to analyze
      this->replier_.FinishWithError(
        ::grpc::Status{::grpc::StatusCode::INTERNAL, s.message().str()},
        this);
      return;
    }

    static_cast<Child*>(this)->handleOutput(ana.value());
  }
};

} // namespace grpc
} // namespace jumanpp

#endif //JUMANPP_GRPC_UNARY_CALL_H
