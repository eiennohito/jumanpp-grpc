//
// Created by Arseny Tolmachev on 2018/03/09.
//

#ifndef JUMANPP_GRPC_CALLS_IMPL_H
#define JUMANPP_GRPC_CALLS_IMPL_H

#include "stream_call.h"
#include "unary_call.h"
#include "core/proto/lattice_dump_output.h"
#include "jumandic/shared/juman_pb_format.h"
#include "jumandic/shared/jumanpp_pb_format.h"

namespace jumanpp {
namespace grpc {

class DefaultConfigCall : public BaseUnaryCall<JumanppConfig, DefaultConfigCall> {
  JumanppConfig topConf_;
public:
  explicit DefaultConfigCall(JumanppGrpcEnv2* env): BaseUnaryCall(env) {}

  void startCall() {
    env_->service().RequestDefaultConfig(&context_, &topConf_, &replier_, env_->poolQueue(), env_->poolQueue(), this);
  }

  void handleCall() {
    topConf_.CopyFrom(env_->defaultConfig());
    replier_.Finish(config_, ::grpc::Status::OK, this);
  }
};

class JumanUnaryCall : public AnaReqBasedUnaryCall<JumanSentence, JumanUnaryCall> {
  jumandic::JumanPbFormat output_;

public:
  explicit JumanUnaryCall(JumanppGrpcEnv2* env): AnaReqBasedUnaryCall(env) {}

  void startCall() {
    env_->service().RequestJuman(&context_, &req_, &replier_, env_->poolQueue(), env_->poolQueue(), this);
  }

  void handleOutput(CachedAnalyzer* ana) {
    Status s = output_.initialize(ana->analyzer()->output(), env_->idResolver(), false);
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

class JumanStreamCall : public BidiStreamCallBase<JumanSentence, JumanStreamCall> {
public:
  jumandic::JumanPbFormat output_;

  explicit JumanStreamCall(JumanppGrpcEnv2* env): BidiStreamCallBase(env) {}

  void startRequest() {
    env_->service().RequestJumanStream(&context_, &rw_, env_->mainQueue(), env_->poolQueue(), this);
  }

  void sendReply(CachedAnalyzer* an) {
    if (!output_.isInitialized()) {
      auto s = output_.initialize(an->analyzer()->output(), env_->idResolver(), false);
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

class TopNUnaryCall : public AnaReqBasedUnaryCall<Lattice, TopNUnaryCall> {
  jumandic::JumanppProtobufOutput output_;

public:
  explicit TopNUnaryCall(JumanppGrpcEnv2* env): AnaReqBasedUnaryCall(env) {}

  void startCall() {
    env_->service().RequestTopN(&context_, &req_, &replier_, env_->poolQueue(), env_->poolQueue(), this);
  }

  void handleOutput(CachedAnalyzer* ana) {
    int topN = req_.top_n();
    if (topN == 0) {
      topN = ana->localBeam();
    }

    Status s = output_.initialize(ana->analyzer()->output(), env_->idResolver(), topN, false);
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

class TopNStreamCall : public BidiStreamCallBase<Lattice, TopNStreamCall> {
public:
  jumandic::JumanppProtobufOutput output_;

  explicit TopNStreamCall(JumanppGrpcEnv2* env): BidiStreamCallBase(env) {}

  void startRequest() {
    env_->service().RequestTopNStream(&context_, &rw_, env_->mainQueue(), env_->poolQueue(), this);
  }

  void sendReply(CachedAnalyzer* an) {
    if (!output_.isInitialized()) {
      Status s = output_.initialize(an->analyzer()->output(), env_->idResolver(), 1, false);
      if (!s) {
        rw_.Finish(::grpc::Status{::grpc::StatusCode::INTERNAL, s.message().str()}, &outputTag_);
        state_ = Failed;
        return;
      }
    }

    int topN = input_.top_n();
    if (topN == 0) {
      topN = an->localBeam();
    }

    output_.setTopN(topN);
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

class FullLatticeDumpUnaryCall : public AnaReqBasedUnaryCall<LatticeDump, FullLatticeDumpUnaryCall> {
public:
  explicit FullLatticeDumpUnaryCall(JumanppGrpcEnv2* env): AnaReqBasedUnaryCall(env) {
    allFeatures_ = true;
  }

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

class LatticeDumpStreamFullImpl: public BidiStreamCallBase<LatticeDump, LatticeDumpStreamFullImpl> {
public:
  core::output::LatticeDumpOutput output_{true, false};

  explicit LatticeDumpStreamFullImpl(JumanppGrpcEnv2* env): BidiStreamCallBase(env) {}

  void startRequest() {
    env_->service().RequestLatticeDumpWithFeaturesStream(&context_, &rw_, env_->mainQueue(), env_->poolQueue(), this);
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

} // namespace grpc
} // namespace jumanpp

#endif //JUMANPP_GRPC_CALLS_IMPL_H
