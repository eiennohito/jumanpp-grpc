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

  State state_ = State::Initial;
protected:

  ::grpc::ServerContext context_;
  ::grpc::ServerAsyncResponseWriter<Reply> replier_{&context_};
  JumanppGrpcEnv2* env_;
  JumanppConfig config_;

public:

  BaseUnaryCall(JumanppGrpcEnv2* env): env_{env} {}

  void Handle() override {
    auto state = state_;
    if (state == Initial) {
      child().startCall();
      state_ = Compute;
    } else if (state == Compute) {
      auto chld = new Child(env_);
      chld->Handle(); //fork call

      config_.CopyFrom(env_->defaultConfig());
      auto& clientMeta = context_.client_metadata();
      auto iter = clientMeta.find("jumanpp-config-bin");
      if (iter != clientMeta.end()) {
        auto& data = iter->second;
        ::google::protobuf::io::ArrayInputStream is(data.data(), data.size());
        ::google::protobuf::io::CodedInputStream cis(&is);
        if (!config_.MergeFromCodedStream(&cis)) {
          state_ = Finished;
          replier_.FinishWithError(::grpc::Status{::grpc::StatusCode::INVALID_ARGUMENT, "invalid config header"}, this);
          return;
        }
      }

      child().handleCall(); //actual logic
      state_ = Finished;
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
public:
  explicit AnaReqBasedUnaryCall(JumanppGrpcEnv2* env): BaseUnaryCall<Reply, Child>::BaseUnaryCall(env) {}

  void handleCall() {
    ScopedAnalyzer ana{this->env_->analyzers(), this->config_, req_};
    if (!ana) {
      this->replier_.FinishWithError(
        ::grpc::Status{::grpc::StatusCode::INTERNAL, "failed to acquire analyzer"},
        this);
      return;
    }

    Status s = ana.value()->readInput(req_, this->env_->analyzers());
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
