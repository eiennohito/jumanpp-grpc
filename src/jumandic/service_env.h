//
// Created by Arseny Tolmachev on 2018/03/09.
//

#ifndef JUMANPP_GRPC_SERVICE_ENV_H
#define JUMANPP_GRPC_SERVICE_ENV_H

#include <grpc++/grpc++.h>
#include <thread>
#include <mutex>
#include <atomic>
#include "util/types.hpp"
#include "util/bounded_queue.h"
#include "core/env.h"
#include "util/lazy.h"
#include "jumandic-svc.grpc.pb.h"
#include "interfaces.h"
#include "analyzer_cache.h"
#include "jumandic/shared/jumandic_id_resolver.h"

namespace jumanpp {
namespace grpc {

class CQThreadPool {
  ::grpc::ServerCompletionQueue* queue_ = nullptr;
  std::atomic<bool> continue_{true};
  std::vector<std::thread> threads_;

  static void Run(CQThreadPool* inst) {
    void* tag = nullptr;
    bool ok = false;
    auto q = inst->queue_;
    if (!q) {
      return;
    }

    while (inst->continue_.load(std::memory_order_consume) && q->Next(&tag, &ok)) {
      if (ok) {
        static_cast<CallImpl*>(tag)->Handle();
      } else {
        delete static_cast<CallImpl*>(tag);
      }
    }
  }

public:
  void start(::grpc::ServerCompletionQueue* queue, int nthreads) {
    queue_ = queue;
    for (int i = 0; i < nthreads; ++i) {
      threads_.emplace_back(&Run, this);
    }
  }

  void stop() {
    continue_ = false;
  }

  ~CQThreadPool() {
    for (auto &t: threads_) {
      t.join();
    }
  }
};

void drainQueue(::grpc::ServerCompletionQueue* queue);

class JumanppGrpcEnv {
  core::JumanppEnv jppEnv_;
  CQThreadPool threadpool_;
  JumanppJumandic::AsyncService asyncService_;
  std::unique_ptr<::grpc::ServerCompletionQueue> mainQueue_;
  std::unique_ptr<::grpc::ServerCompletionQueue> poolQueue_;
  JumanppConfig defaultConfig_;
  AnalyzerCache cache_;
  core::analysis::AnalyzerConfig defaultAconf_;
  jumandic::JumandicIdResolver idResolver_;

public:
  JumanppJumandic::AsyncService& service() { return asyncService_; }
  const JumanppConfig& defaultConfig() const { return defaultConfig_; }
  AnalyzerCache& analyzers() { return cache_; }
  ::grpc::ServerCompletionQueue* mainQueue() { return mainQueue_.get(); }
  ::grpc::ServerCompletionQueue* poolQueue() { return poolQueue_.get(); }
  const jumandic::JumandicIdResolver* idResolver() const { return &idResolver_; }
  const core::CoreHolder& core() const { return *jppEnv_.coreHolder(); }

  void registerService(::grpc::ServerBuilder* bldr) {
    bldr->RegisterService(&asyncService_);
    mainQueue_ = bldr->AddCompletionQueue(true);
    poolQueue_ = bldr->AddCompletionQueue(true);
  }

  template<typename Call, typename... Args>
  void callImpl(Args&&... args) {
    auto call = new Call(this, std::forward<Args>(args)...);
    auto cnv = static_cast<CallImpl*>(call);
    cnv->Handle();
  }

  void start(int poolThreads) {
    threadpool_.start(poolQueue_.get(), poolThreads);

    void* msg;
    bool ok = false;
    while (mainQueue_->Next(&msg, &ok)) {
      auto typed = static_cast<CallImpl*>(msg);
      if (ok) {
        typed->Handle();
      } else {
        delete typed;
      }
    }
  }

  void printVersion();

  Status loadConfig(StringPiece configPath);

  ~JumanppGrpcEnv() {
    threadpool_.stop();
    if (mainQueue_) {
      mainQueue_->Shutdown();
      drainQueue(mainQueue());
    }
    if (poolQueue_) {
      poolQueue_->Shutdown();
      drainQueue(poolQueue());
    }
  }
};

} // namespace grpc
} // namespace jumanpp

#endif //JUMANPP_GRPC_SERVICE_ENV_H
