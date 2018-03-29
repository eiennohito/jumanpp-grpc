//
// Created by Arseny Tolmachev on 2018/03/09.
//

#include "service_env.h"
#include "jumandic/shared/jumanpp_args.h"
#include "jumandic/shared/jumandic_env.h"

namespace jumanpp {
namespace grpc {

Status JumanppGrpcEnv2::loadConfig(StringPiece configPath) {
  jumandic::JumanppConf conf;
  JPP_RETURN_IF_ERROR(jumandic::parseCfgFile(configPath, &conf, 1));
  JPP_RETURN_IF_ERROR(jppEnv_.loadModel(conf.modelFile.value()));
  jppEnv_.setRnnConfig(conf.rnnConfig);
  JPP_RETURN_IF_ERROR(jppEnv_.initFeatures(jumandic::jumandicStaticFeatures()));
  defaultConfig_.set_local_beam(conf.beamSize);
  defaultConfig_.set_global_beam_left(conf.globalBeam);
  defaultConfig_.set_global_beam_check(conf.rightCheck);
  defaultConfig_.set_global_beam_right(conf.rightBeam);
  defaultAconf_.globalBeamSize = conf.globalBeam;
  defaultAconf_.rightGbeamCheck = conf.rightCheck;
  defaultAconf_.rightGbeamSize = conf.rightBeam;
  JPP_RETURN_IF_ERROR(cache_.initialize(&jppEnv_, defaultAconf_, 40));
  JPP_RETURN_IF_ERROR(idResolver_.initialize(jppEnv_.coreHolder()->dic()));
  return Status::Ok();
}

void drainQueue(::grpc::ServerCompletionQueue *queue) {
  void* ignored;
  bool ignored_res;
  while (queue->Next(&ignored, &ignored_res)) {
    //do nothing
  }
}

}
}
