#include "FileBundleLoader.h"
#include <cxxreact/RecoverableError.h>
#include <cxxreact/IndexedRAMBundle.h>
#include <cxxreact/BasicBundle.h>

namespace facebook {
namespace react {

  std::unique_ptr<const Bundle> FileBundleLoader::getBundle(std::string bundleURL) const {
    bundlesPath_ = bundleURL.substr(0, bundleURL.find_last_of("/") + 1);
    if (IndexedRAMBundle::isIndexedRAMBundle(bundleURL.c_str())) {
      return std::make_unique<IndexedRAMBundle>(bundleURL, bundlesPath_);
    } else {
      std::unique_ptr<const JSBigFileString> script;
      RecoverableError::runRethrowingAsRecoverable<std::system_error>(
        [&bundleURL, &script]() {
          script = JSBigFileString::fromPath(bundleURL);
        }
      );
      return std::make_unique<BasicBundle>(std::move(script),bundleURL);
    }
  }

  std::string FileBundleLoader::getBundleURLFromName(std::string bundleName) const {
    return bundlesPath_ + bundleName + ".android.bundle";
  }

} // namespace react
} // namespace facebook
