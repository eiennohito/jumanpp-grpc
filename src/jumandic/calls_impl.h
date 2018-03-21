//
// Created by Arseny Tolmachev on 2018/03/09.
//

#ifndef JUMANPP_GRPC_CALLS_IMPL_H
#define JUMANPP_GRPC_CALLS_IMPL_H

#include "stream_call.h"
#include "unary_call.h"
#include "core/proto/lattice_dump_output.h"

namespace jumanpp {
namespace grpc {

class LatticeDumpStreamImpl: public BidiStreamCallBase<LatticeDump, LatticeDumpStreamImpl> {
public:
  core::output::LatticeDumpOutput output_{false, false};

  explicit LatticeDumpStreamImpl(JumanppGrpcEnv2* env): BidiStreamCallBase(env) {}

  void startRequest() {
    env_->service().RequestLatticeDumpStream(&context_, &rw_, env_->mainQueue(), env_->poolQueue(), this);
  }

  void sendReply(CachedAnalyzer* an) {
    if (!output_.wasInitialized()) {
      auto aimpl = an->impl();
      auto weights = an->weights();
      auto s = output_.initialize(aimpl, weights);
      if (!s) {
        rw_.Finish(::grpc::Status{::grpc::StatusCode::INTERNAL, s.message().str()}, &outputTag_);
        state_ = Failed;
        return;
      }
    }

    Status s = output_.format(*an->analyzer(), an->comment());

    if (!s) {
      rw_.Finish(::grpc::Status{::grpc::StatusCode::INTERNAL, s.message().str()}, &outputTag_);
      state_ = Failed;
      return;
    }

    rw_.Write(*output_.objectPtr(), &outputTag_);
  }
};

class LatticeDumpUnaryCall : public  AnaReqBasedUnaryCall<LatticeDump, LatticeDumpUnaryCall> {
public:
  explicit LatticeDumpUnaryCall(JumanppGrpcEnv2* env): AnaReqBasedUnaryCall(env) {}


  core::output::LatticeDumpOutput output_{false, false};

  void startCall() {
    env_->service().RequestLatticeDump(&context_, &req_, &replier_, env_->poolQueue(), env_->poolQueue(), this);
  }

  void handleOutput(CachedAnalyzer* ana) {
    Status s = output_.initialize(ana->impl(), &ana->analyzer()->scorer()->feature->weights());
    if (!s) {
      replier_.FinishWithError(
        ::grpc::Status{::grpc::StatusCode::INTERNAL, s.message().str()},
        this);
      return;
    }

    s = output_.format(*ana->analyzer(), req_.key());
    if (!s) {
      replier_.FinishWithError(
        ::grpc::Status{::grpc::StatusCode::INTERNAL, s.message().str()},
        this);
      return;
    }

    replier_.Finish(*output_.objectPtr(), ::grpc::Status::OK, this);
  }
};

class FullLatticeDumpUnaryCall : public AnaReqBasedUnaryCall<LatticeDump, FullLatticeDumpUnaryCall> {
public:
  explicit FullLatticeDumpUnaryCall(JumanppGrpcEnv2* env): AnaReqBasedUnaryCall(env) {}

  core::output::LatticeDumpOutput output_{true, false};

  void startCall() {
    env_->service().RequestLatticeDumpWithFeatures(&context_, &req_, &replier_, env_->poolQueue(), env_->poolQueue(), this);
  }

  void handleOutput(CachedAnalyzer* ana) {
    Status s = output_.initialize(ana->impl(), &ana->analyzer()->scorer()->feature->weights());
    if (!s) {
      replier_.FinishWithError(
        ::grpc::Status{::grpc::StatusCode::INTERNAL, s.message().str()},
        this);
      return;
    }

    s = output_.format(*ana->analyzer(), req_.key());
    if (!s) {
      replier_.FinishWithError(
        ::grpc::Status{::grpc::StatusCode::INTERNAL, s.message().str()},
        this);
      return;
    }

    replier_.Finish(*output_.objectPtr(), ::grpc::Status::OK, this);
  }
};

} // namespace grpc
} // namespace jumanpp

#endif //JUMANPP_GRPC_CALLS_IMPL_H
