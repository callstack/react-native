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
  bundleEnvironments_.clear();
  bundles_.clear();
}

void BundleRegistry::preloadEnvironment(std::string environmentId, std::function<void()> callback) {
  if (hasEnvironment(environmentId)) {
    throw std::runtime_error(
      folly::to<std::string>("Environment with id = ", environmentId, " already exists")
    );
  }
  std::shared_ptr<BundleExecutionEnvironment> execEnv = std::make_shared<BundleExecutionEnvironment>();
  execEnv->valid = false;
  execEnv->jsQueue = jsQueueFactory_();
  execEnv->initialBundle = std::weak_ptr<const Bundle>();
  bundleEnvironments_[environmentId] = std::move(execEnv);

  execEnv = bundleEnvironments_[environmentId];
  execEnv->jsQueue->runOnQueueSync([this, execEnv, callback]() mutable {
    execEnv->nativeToJsBridge = std::make_unique<NativeToJsBridge>(jsExecutorFactory_,
                                                                   moduleRegistry_,
                                                                   execEnv->jsQueue,
                                                                   callback_);
    callback();
  });
}

void BundleRegistry::runInPreloadedEnvironment(std::string environmentId,
                                               std::string initialBundleURL,
                                               std::unique_ptr<BundleLoader> bundleLoader) {
  if (!bundleLoader_) {
    bundleLoader_ = std::move(bundleLoader);
  }
 
  std::shared_ptr<BundleExecutionEnvironment> execEnv = getEnvironment(environmentId).lock();
  auto initialBundle = bundleLoader_->getBundle(initialBundleURL);
  bundles_[initialBundleURL] = std::move(initialBundle);
  execEnv->initialBundle = std::weak_ptr<const Bundle>(bundles_[initialBundleURL]);

  execEnv->jsQueue->runOnQueueSync([this, execEnv, environmentId]() mutable {
    auto bundle = execEnv->initialBundle.lock();
    std::unique_ptr<const JSBigString> script = getScriptFromBundle(bundle);
    GetModuleLambda getModule = makeGetModuleLambda();
    LoadBundleLambda loadBundle = makeLoadBundleLambda(environmentId);

    evalInitialBundle(execEnv,
                      std::move(script),
                      bundle->getSourceURL(),
                      loadBundle,
                      getModule);

    execEnv->valid = true;
  });
}

void BundleRegistry::disposeEnvironments() {
  for (auto environment : bundleEnvironments_) {
    environment.second->nativeToJsBridge->destroy();
  }
}

std::weak_ptr<BundleRegistry::BundleExecutionEnvironment> BundleRegistry::getEnvironment(std::string environmentId) {
  if (!hasEnvironment(environmentId)) {
    throw std::runtime_error(
      folly::to<std::string>("Cannot get environment with id = ", environmentId)
    );
  }

  return std::weak_ptr<BundleExecutionEnvironment>(bundleEnvironments_[environmentId]);
}

bool BundleRegistry::hasEnvironment(std::string environmentId) {
  return bundleEnvironments_.find(environmentId) != bundleEnvironments_.end();
}

void BundleRegistry::evalInitialBundle(std::shared_ptr<BundleExecutionEnvironment> execEnv,
                                       std::unique_ptr<const JSBigString> startupScript,
                                       std::string sourceURL,
                                       LoadBundleLambda loadBundle,
                                       GetModuleLambda getModule) {
  // `nativeRequire`, which uses `getModule` must be always set on global
  // in `JSExecutor`, since even if the initial bundle is not RAM, we don't
  // know the format of other bundles.
  execEnv->nativeToJsBridge->setupEnvironmentSync(loadBundle, getModule);
  execEnv->nativeToJsBridge->loadScriptSync(std::move(startupScript),
                                            sourceURL);
}


std::unique_ptr<const JSBigString> BundleRegistry::getScriptFromBundle(std::shared_ptr<const Bundle> bundle) {
  if (bundle->getBundleType() == BundleType::FileRAMBundle ||
      bundle->getBundleType() == BundleType::IndexedRAMBundle) {
      std::shared_ptr<const RAMBundle> ramBundle
        = std::dynamic_pointer_cast<const RAMBundle>(bundle);
      if (!ramBundle) {
        throw std::runtime_error("Cannot cast Bundle to RAMBundle");
      }
      
      return ramBundle->getStartupScript();
    } else {
      std::shared_ptr<const BasicBundle> basicBundle
        = std::dynamic_pointer_cast<const BasicBundle>(bundle);
      if (!basicBundle) {
        throw std::runtime_error("Cannot cast Bundle to BasicBundle");
      }

      return basicBundle->getScript();
    }
}

BundleRegistry::GetModuleLambda BundleRegistry::makeGetModuleLambda() {
  return [this](uint32_t moduleId, std::string bundleName) {
    std::string bundleURL = bundleLoader_->getBundleURLFromName(bundleName);
    std::shared_ptr<const RAMBundle> ramBundle;
    if (bundles_.find(bundleURL) != bundles_.end()) {
      ramBundle = std::dynamic_pointer_cast<const RAMBundle>(bundles_[bundleURL]);
      if (!ramBundle) {
        throw std::runtime_error("Bundle " +
                                 bundleURL +
                                 " is not a RAM bundle - GetModuleLambda cannot be used on it");
      }
    } else {
      throw std::runtime_error("Cannot find RAM bundle " + bundleURL);
    }

    return ramBundle->getModule(moduleId);
  };
}

BundleRegistry::LoadBundleLambda BundleRegistry::makeLoadBundleLambda(std::string environmentId) {
  return [this, environmentId](std::string bundleName, bool inCurrentEnvironment) mutable {
    std::shared_ptr<BundleExecutionEnvironment> execEnv = getEnvironment(environmentId).lock();
    std::string bundleURL = bundleLoader_->getBundleURLFromName(bundleName);
    execEnv->jsQueue->runOnQueueSync([this, bundleURL, execEnv]() mutable {
      std::unique_ptr<const Bundle> additionalBundle = bundleLoader_->getBundle(bundleURL);
      bundles_[bundleURL] = std::move(additionalBundle);
      std::shared_ptr<const Bundle> bundle = bundles_[bundleURL];
      std::unique_ptr<const JSBigString> script = getScriptFromBundle(bundle);

      execEnv->nativeToJsBridge->loadScriptSync(std::move(script),
                                                bundle->getSourceURL());
    });
  };
}

} // react
} // facebook
