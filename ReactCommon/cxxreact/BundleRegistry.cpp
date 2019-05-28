#include "BundleRegistry.h"

namespace facebook {
namespace react {

BundleRegistry::BundleRegistry(JSExecutorFactory* jsExecutorFactory,
                               std::shared_ptr<ModuleRegistry> moduleRegistry,
                               std::shared_ptr<InstanceCallback> callback,
                               std::function<std::shared_ptr<MessageQueueThread>()> jsQueueFactory) {
  jsExecutorFactory_ = jsExecutorFactory;
  moduleRegistry_ = moduleRegistry;
  callback_ = callback;
  jsQueueFactory_ = jsQueueFactory;
}

BundleRegistry::~BundleRegistry() {
  bundleExecutionEnvironments_.clear();
  bundles_.clear();
}

void BundleRegistry::runNewExecutionEnvironment(std::unique_ptr<const Bundle> initialBundle,
                                                std::function<void()> callback) {
  std::shared_ptr<BundleExecutionEnvironment> execEnv = std::make_shared<BundleExecutionEnvironment>();
  execEnv->valid = false;
  execEnv->jsQueue = jsQueueFactory_();
  bundles_.push_back(std::move(initialBundle));
  execEnv->initialBundle = std::weak_ptr<const Bundle>(bundles_.back());
  bundleExecutionEnvironments_.push_back(std::move(execEnv));

  execEnv = bundleExecutionEnvironments_.back();
  execEnv->jsQueue->runOnQueueSync([this, execEnv, callback]() mutable {
    execEnv->nativeToJsBridge = std::make_unique<NativeToJsBridge>(jsExecutorFactory_,
                                                                   moduleRegistry_,
                                                                   execEnv->jsQueue,
                                                                   callback_);

    auto bundle = execEnv->initialBundle.lock();
    if (bundle->getBundleType() == BundleType::FileRAMBundle ||
        bundle->getBundleType() == BundleType::IndexedRAMBundle) {
      std::shared_ptr<const RAMBundle> ramBundle
        = std::dynamic_pointer_cast<const RAMBundle>(bundle);
      // TODO: check if ramBundle is not empty or throw exception

      LoadBundleLambda loadBundle = [](std::string p, bool n) {}; // TODO: provide actual impl
      auto getModule = folly::Optional<GetModuleLambda>([ramBundle](uint32_t moduleId) {
        return ramBundle->getModule(moduleId);
      });
      
      // TODO: figure out if we can get away from copying
      std::unique_ptr<const JSBigString> startupScript = std::make_unique<const JSBigStdString>(
        std::string(ramBundle->getStartupScript()->c_str()));
      
      evalInitialBundle(std::move(execEnv),
                        std::move(startupScript),
                        ramBundle->getSourceURL(),
                        loadBundle,
                        getModule);
    } else {
      std::shared_ptr<const BasicBundle> basicBundle
        = std::dynamic_pointer_cast<const BasicBundle>(bundle);
      // TODO: check if ramBundle is not empty or throw exception

      // TODO: setupEnvironment + loadScript
    }

    execEnv->valid = true;
    callback();
  });
}

void BundleRegistry::evalInitialBundle(std::shared_ptr<BundleExecutionEnvironment> execEnv,
                                       std::unique_ptr<const JSBigString> startupScript,
                                       std::string sourceURL,
                                       LoadBundleLambda loadBundle,
                                       folly::Optional<GetModuleLambda> getModule) {
  execEnv->nativeToJsBridge->setupEnvironmentSync(loadBundle, getModule);
  execEnv->nativeToJsBridge->loadScriptSync(std::move(startupScript),
                                            sourceURL);
}

void BundleRegistry::disposeExecutionEnvironments() {
  for (auto execEnv : bundleExecutionEnvironments_) {
    execEnv->nativeToJsBridge->destroy();
  }
}

std::weak_ptr<BundleRegistry::BundleExecutionEnvironment> BundleRegistry::getFirstExecutionEnvironment() {
  if (bundleExecutionEnvironments_.size() == 0) {
    throw std::runtime_error("Cannot get first BundleExecutionEnvironment");
  }

  return std::weak_ptr<BundleExecutionEnvironment>(bundleExecutionEnvironments_[0]);
}

bool BundleRegistry::hasExecutionEnvironment() {
  return bundleExecutionEnvironments_.size() > 0;
}

} // react
} // facebook
