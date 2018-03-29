//
// Created by Arseny Tolmachev on 2018/03/08.
//

#ifndef JUMANPP_GRPC_ANALYZER_CACHE_H
#define JUMANPP_GRPC_ANALYZER_CACHE_H

#include "core/analysis/analyzer.h"
#include "core/input/pex_stream_reader.h"
#include "core/env.h"
#include "jumandic-svc.pb.h"
#include <chrono>

namespace jumanpp {
namespace grpc {

class AnalyzerCache;

enum class AnalyzerState {
  Uninitialized,
  InUse,
  WithResult,
  NotInUse
};

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

class CachedAnalyzer {
  core::analysis::AnalyzerConfig analyzerConfig;
  core::ScoringConfig scoringConfig;
  core::analysis::Analyzer analyzer_;
  RequestType lastRequestType = RequestType::Normal;
  std::unique_ptr<core::input::StreamReader> reader_;
  std::string comment_;
  TimePoint lastUsage_ = TimePoint::min();
  AnalyzerState state_ = AnalyzerState::Uninitialized;
  core::analysis::ScorerDef cachedDef_;

  void setBaseConfig(const core::analysis::AnalyzerConfig &global, const core::JumanppEnv &env, bool allFeatures);

  void setProtoConfig(const JumanppConfig &cfg) {
    scoringConfig.beamSize = cfg.local_beam();
    analyzerConfig.globalBeamSize = cfg.global_beam_left();
    analyzerConfig.rightGbeamSize = cfg.global_beam_right();
    analyzerConfig.rightGbeamCheck = cfg.global_beam_check();
    if (cfg.ignore_rnn()) {
      scoringConfig.numScorers = 1;
    }
  }

  Status buildAnalyzer(const core::JumanppEnv& env);

public:
  bool isAvailableFor(const JumanppConfig& cfg, const AnalysisRequest& req, bool allFeatures) const;
  Status readInput(const AnalysisRequest& req, const AnalyzerCache& cache);
  Status analyze();
  bool hasResult() const { return state_ == AnalyzerState::WithResult; }
  core::analysis::AnalyzerImpl* impl() { return analyzer_.impl(); }
  core::analysis::Analyzer* analyzer() { return &analyzer_; }
  const core::analysis::WeightBuffer* weights() const { return &analyzer_.scorer()->feature->weights(); }
  StringPiece comment() const { return comment_; }
  friend class AnalyzerCache;
  int localBeam() const { return scoringConfig.beamSize; }
};

class AnalyzerCache {
  core::input::PexStreamReader cachedReader_;
  std::vector<std::unique_ptr<CachedAnalyzer>> cache_;
  core::analysis::AnalyzerConfig defaultCfg_;
  const core::JumanppEnv* env_ = nullptr;
  std::mutex mutex_;

public:
  Status initialize(const core::JumanppEnv* env, const core::analysis::AnalyzerConfig& defaultConfig, int capacity) {
    env_ = env;
    defaultCfg_ = defaultConfig;
    JPP_RETURN_IF_ERROR(cachedReader_.initialize(*env_->coreHolder()));
    for (int i = 0; i < capacity; ++i) {
      cache_.emplace_back(new CachedAnalyzer);
    }
    return Status::Ok();
  }

  const core::input::PexStreamReader& cachedReader() const { return cachedReader_; }
  CachedAnalyzer* acquire(const JumanppConfig& cfg, const AnalysisRequest& req, bool allFeatures);
  void release(CachedAnalyzer* analyzer);
};

class ScopedAnalyzer {
  AnalyzerCache& cache_;
  CachedAnalyzer* analyzer_;
public:

  ScopedAnalyzer(AnalyzerCache& cache, const JumanppConfig& cfg, const AnalysisRequest& req, bool allFeatures): cache_{cache},
                                                                                              analyzer_{cache.acquire(cfg, req, allFeatures)} {}
  ~ScopedAnalyzer() {
    if (analyzer_ != nullptr) {
      cache_.release(analyzer_);
    }
  }

  CachedAnalyzer* value() { return analyzer_; }

  explicit operator bool() const { return analyzer_ != nullptr; }
};

} // namespace grpc
} // namespace jumanpp

#endif //JUMANPP_GRPC_ANALYZER_CACHE_H
