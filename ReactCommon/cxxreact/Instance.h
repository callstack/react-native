// Copyright (c) Facebook, Inc. and its affiliates.

// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <condition_variable>
#include <memory>

#include "BundleRegistry.h"

#ifndef RN_EXPORT
#define RN_EXPORT __attribute__((visibility("default")))
#endif

namespace folly {
struct dynamic;
}

namespace facebook {
namespace react {

class JSBigString;
class JSExecutorFactory;
class MessageQueueThread;
class ModuleRegistry;
// class RAMBundleRegistry;

struct InstanceCallback {
  virtual ~InstanceCallback() {}
  virtual void onBatchComplete() {}
  virtual void incrementPendingJSCalls() {}
  virtual void decrementPendingJSCalls() {}
};

class RN_EXPORT Instance {
public:
  ~Instance();
  void initializeBridge(std::unique_ptr<InstanceCallback> callback,
                        std::shared_ptr<JSExecutorFactory> jsef,
                        std::shared_ptr<MessageQueueThread> jsQueue,
                        std::shared_ptr<ModuleRegistry> moduleRegistry);

  void loadBundle(std::unique_ptr<const Bundle> bundle, bool loadSynchronously);

  bool supportsProfiling();
  void setGlobalVariable(std::string propName,
                         std::unique_ptr<const JSBigString> jsonValue);
  void *getJavaScriptContext();
  bool isInspectable();
  bool isBatchActive();
  void callJSFunction(std::string &&module, std::string &&method,
                      folly::dynamic &&params);
  void callJSCallback(uint64_t callbackId, folly::dynamic &&params);

  const ModuleRegistry &getModuleRegistry() const;
  ModuleRegistry &getModuleRegistry();

  void handleMemoryPressure(int pressureLevel);

private:
  void callNativeModules(folly::dynamic &&calls, bool isEndOfBatch);
  void loadBundleAsync(std::unique_ptr<const Bundle> bundle);
  void loadBundleSync(std::unique_ptr<const Bundle> bundle);

  std::shared_ptr<InstanceCallback> callback_;
  std::shared_ptr<ModuleRegistry> moduleRegistry_;
  std::unique_ptr<BundleRegistry> bundleRegistry_;

  std::string defaultEnvironmentId_ = "default";

  std::mutex m_syncMutex;
  std::condition_variable m_syncCV;
  bool m_syncReady = false;
};

} // namespace react
} // namespace facebook
