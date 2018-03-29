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

bool CachedAnalyzer::isAvailableFor(const JumanppConfig &cfg, const AnalysisRequest &req, bool allFeatures) const {
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

  if (scoringConfig.numScorers > 1 && cfg.ignore_rnn()) {
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

  if (analyzerConfig.storeAllPatterns != allFeatures) {
    return false;
  }

  return true;
}

void CachedAnalyzer::setBaseConfig(const core::analysis::AnalyzerConfig &anaconf, const core::JumanppEnv &env, bool allFeatures) {
  analyzerConfig = anaconf;
  analyzerConfig.storeAllPatterns = allFeatures;
  scoringConfig.numScorers = env.scorers()->numScorers();
}

Status CachedAnalyzer::buildAnalyzer(const core::JumanppEnv &env) {
  auto scorer = env.scorers();
  if (scoringConfig.numScorers != scorer->numScorers()) {
    cachedDef_.feature = scorer->feature;
    cachedDef_.scoreWeights.clear();
    cachedDef_.scoreWeights.push_back(scorer->scoreWeights.front());
    JPP_RETURN_IF_ERROR(analyzer_.initialize(env.coreHolder(), analyzerConfig, scoringConfig, &cachedDef_));
  } else {
    JPP_RETURN_IF_ERROR(analyzer_.initialize(env.coreHolder(), analyzerConfig, scoringConfig, scorer));
  }
  lastUsage_ = Clock::now();
  return Status::Ok();
}

void AnalyzerCache::release(CachedAnalyzer *analyzer) {
  std::lock_guard<std::mutex> guard{mutex_};
  analyzer->state_ = AnalyzerState::NotInUse;
}

CachedAnalyzer *AnalyzerCache::acquire(const JumanppConfig &cfg, const AnalysisRequest &req, bool allFeatures) {
  std::lock_guard<std::mutex> guard{mutex_};

  CachedAnalyzer* available = nullptr;
  TimePoint lastUsage = TimePoint::max();

  for (auto& v: cache_) {
    if (v->isAvailableFor(cfg, req, allFeatures)) {
      // non-used compatible analyzer is returned immediately
      v->state_ = AnalyzerState::InUse;
      return v.get();
    }

    if (v->state_ == AnalyzerState::Uninitialized) {
      // non-initialized analyzer is returned with the highest priority
      available = v.get();
      break;
    }

    if (v->state_ != AnalyzerState::InUse && lastUsage > v->lastUsage_) {
      // otherwise try to find one not used for a longest time
      available = v.get();
      lastUsage = v->lastUsage_;
    }
  }

  if (available == nullptr) {
    return nullptr;
  }

  available->setBaseConfig(defaultCfg_, *env_, allFeatures);
  available->setProtoConfig(cfg);
  Status s = available->buildAnalyzer(*env_);
  if (!s) {
    LOG_ERROR() << "Failed to init analyzer: " << s;
    return nullptr;
  }

  available->state_ = AnalyzerState::InUse;
  return available;
}


} // namespace grpc
} // namespace jumanpp