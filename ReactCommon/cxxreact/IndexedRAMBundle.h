#pragma once

#include <memory>
#include <istream>
#include "RAMBundle.h"
#include "JSBigString.h"

namespace facebook {
namespace react {

class IndexedRAMBundle : public RAMBundle {
public:
  IndexedRAMBundle(std::string sourcePath, std::string sourceURL);

  // For Android IndexedRAMBundle loaded from Assets
  IndexedRAMBundle(std::string sourcePath,
                  std::string sourceURL,
                  std::unique_ptr<const JSBigString> script);

  std::string getSourceURL() const override;
  std::string getSourcePath() const override;
  std::shared_ptr<const JSBigString> getStartupScript() const override;
  Module getModule(uint32_t moduleId) const override;
  
  BundleType bundleType = BundleType::IndexedRAMBundle;

private:
  struct ModuleData {
    uint32_t offset;
    uint32_t length;
  };
  static_assert(
    sizeof(ModuleData) == 8,
    "ModuleData must not have any padding and use sizes matching input files");

  struct ModuleTable {
    size_t numEntries;
    std::unique_ptr<ModuleData[]> data;
    ModuleTable() : numEntries(0) {};
    ModuleTable(size_t entries) :
      numEntries(entries),
      data(std::unique_ptr<ModuleData[]>(new ModuleData[numEntries])) {};
    size_t byteLength() const {
      return numEntries * sizeof(ModuleData);
    }
  };

  void init();
  std::string getModuleCode(const uint32_t id) const;
  void readBundle(char *buffer, const std::streamsize bytes) const;
  void readBundle(
    char *buffer, const
    std::streamsize bytes,
    const std::istream::pos_type position) const;

  std::string sourceURL_;
  std::string sourcePath_;
  std::shared_ptr<JSBigBufferString> startupScript_;
  mutable std::unique_ptr<std::istream> bundle_;
  ModuleTable table_;
  size_t baseOffset_;
};

} // react
} // facebook
