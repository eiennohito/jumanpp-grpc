//
// Created by Arseny Tolmachev on 2018/03/08.
//

#include "analyzer_cache.h"
#include "util/logging.hpp"

namespace jumanpp {
namespace grpc {

Status CachedAnalyzer::readInput(const AnalysisRequest &req, const AnalyzerCache &cache) {
  if (!reader_ || lastRequestType != req.type()) {
    switch(req.type()) {
      case RequestType::Normal: {
        reader_.reset(new core::input::PlainStreamReader);
        break;
      }
      case RequestType::PartialAnnotation: {
        auto reader = new core::input::PexStreamReader;
        reader_.reset(reader);
        JPP_RETURN_IF_ERROR(reader->initialize(cache.cachedReader()));
        break;
      }
      default:
        return JPPS_NOT_IMPLEMENTED;
    }
  }

  std::istringstream ss{req.sentence()};
  JPP_RETURN_IF_ERROR(reader_->readExample(&ss));
  comment_ = req.key();
  lastUsage_ = Clock::now();

  return Status::Ok();
}

Status CachedAnalyzer::analyze() {
  JPP_RETURN_IF_ERROR(reader_->analyzeWith(&analyzer_));
  lastUsage_ = Clock::now();
  state_ = AnalyzerState::WithResult;
  return Status::Ok();
}

bool CachedAnalyzer::isAvailableFor(const JumanppConfig &cfg, const AnalysisRequest &req) const {
  auto st = state_;
  if (st == AnalyzerState::InUse || st == AnalyzerState::WithResult) {
    return false;
  }

  if (lastRequestType != req.type()) {
    return false;
  }

  if (scoringConfig.beamSize != cfg.local_beam()) {
    return false;
  }

  if (analyzerConfig.globalBeamSize != cfg.global_beam_left()) {
    return false;
  }

  if (analyzerConfig.rightGbeamSize != cfg.global_beam_right()) {
    return false;
  }

  if (analyzerConfig.rightGbeamCheck != cfg.global_beam_check()) {
    return false;
  }

  return true;
}

void CachedAnalyzer::setBaseConfig(const core::analysis::AnalyzerConfig &anaconf, const core::JumanppEnv &env) {
  analyzerConfig = anaconf;
  scoringConfig.numScorers = env.scorers()->numScorers();
}

Status CachedAnalyzer::buildAnalyzer(const core::JumanppEnv &env) {
  JPP_RETURN_IF_ERROR(analyzer_.initialize(env.coreHolder(), analyzerConfig, scoringConfig, env.scorers()));
  lastUsage_ = Clock::now();
  return Status::Ok();
}

void AnalyzerCache::release(CachedAnalyzer *analyzer) {
  std::lock_guard<std::mutex> guard{mutex_};
  analyzer->state_ = AnalyzerState::NotInUse;
}

CachedAnalyzer *AnalyzerCache::acquire(const JumanppConfig &cfg, const AnalysisRequest &req) {
  std::lock_guard<std::mutex> guard{mutex_};

  CachedAnalyzer* worst = nullptr;
  auto lastTime = TimePoint::max();

  for (auto& v: cache_) {
    if (v->isAvailableFor(cfg, req)) {
      v->state_ = AnalyzerState::InUse;
      return v.get();
    }

    if (v->state_ != AnalyzerState::InUse && lastTime > v->lastUsage_) {
      worst = v.get();
    }
  }

  if (worst == nullptr) {
    return nullptr;
  }

  worst->setBaseConfig(defaultCfg_, *env_);
  worst->setProtoConfig(cfg);
  Status s = worst->buildAnalyzer(*env_);
  if (!s) {
    LOG_ERROR() << "Failed to init analyzer: " << s;
    return nullptr;
  }

  return worst;
}


} // namespace grpc
} // namespace jumanpp