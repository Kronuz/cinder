// Copyright (c) Meta Platforms, Inc. and affiliates.
#pragma once

#include "cinderx/StrictModules/Compiler/analyzed_module.h"
#include "cinderx/StrictModules/Compiler/module_info.h"
#include "cinderx/StrictModules/analyzer.h"
#include "cinderx/StrictModules/error_sink.h"

#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace strictmod::compiler {
enum class FileSuffixKind { kPythonFile, kStrictStubFile, kTypingStubFile };

enum class AllowListKind { kPrefix, kExact };

const std::string& getFileSuffixKindName(FileSuffixKind kind);

class ModuleLoader {
  static std::shared_ptr<BaseErrorSink> defaultErrorSinkFactory() {
    static std::shared_ptr<BaseErrorSink> factory = std::make_shared<CollectingErrorSink>();
    return factory;
  }

 public:
  typedef std::function<bool(const std::string&, const std::string&)>
      ForceStrictFunc;
  typedef std::function<std::shared_ptr<BaseErrorSink>()> ErrorSinkFactory;
  typedef std::vector<std::pair<std::string, AllowListKind>> AllowListType;

  ModuleLoader() : ModuleLoader({}, {}) {}

  ModuleLoader(
      std::vector<std::string> importPath,
      std::vector<std::string> stubImportPath,
      AllowListType allowList = {})
      : ModuleLoader(
            std::move(importPath),
            std::move(stubImportPath),
            std::move(allowList),
            std::nullopt,
            defaultErrorSinkFactory) {}

  ModuleLoader(
      std::vector<std::string> importPath,
      std::vector<std::string> stubImportPath,
      AllowListType allowList,
      ForceStrictFunc forceStrict)
      : ModuleLoader(
            std::move(importPath),
            std::move(stubImportPath),
            std::move(allowList),
            forceStrict,
            defaultErrorSinkFactory) {}

  ModuleLoader(
      std::vector<std::string> importPath,
      std::vector<std::string> stubImportPath,
      AllowListType allowList,
      std::optional<ForceStrictFunc> forceStrict,
      ErrorSinkFactory factory)
      : importPath_(std::move(importPath)),
        stubImportPath_(std::move(stubImportPath)),
        allowList_(std::move(allowList)),
        modules_(),
        lazy_modules_(),
        forceStrict_(forceStrict),
        errorSinkFactory_(factory),
        deletedModules_() {
    arena_ = _PyArena_New();
    if (arena_ == nullptr) {
      throw std::runtime_error(kArenaNewErrorMsg);
    }
  }

  ~ModuleLoader() {
    // free all scopes owned by analyzed modules
    for (auto& am : modules_) {
      // since passModule could be used in tests, the values
      // of modules could be nullptr
      if (am.second) {
        am.second->cleanModuleContent();
      }
    }
    for (auto& am : deletedModules_) {
      if (am) {
        am->cleanModuleContent();
      }
    }
    _PyArena_Free(arena_);
  }

  /**
  pass ownership to caller of an already analyzed module.
  Return nullptr if module is not loaded
  */
  std::shared_ptr<AnalyzedModule> passModule(const std::string& modName) {
    return modules_[modName];
  }

  /**
  Load a module named `modName`.
  The returned pointer maybe null, which indicates that the module is
  not found.
  Note that this is different from the value of the analyzed module
  being nullptr, indicating that the analysis failed/module is not strict
  */
  std::shared_ptr<AnalyzedModule> loadModule(const std::string& modName);
  /**
  Remove a module from checked modules
  */
  void deleteModule(const std::string& modName);
  void recordLazyModule(const std::string& modName);

  std::shared_ptr<StrictModuleObject> loadModuleValue(
      const std::string& modName);

  // return module value if module is already loaded, nullptr otherwise
  std::shared_ptr<StrictModuleObject> tryGetModuleValue(
      const std::string& modName);

  std::shared_ptr<AnalyzedModule> loadModuleFromSource(
      const std::string& source,
      const std::string& name,
      const std::string& filename,
      std::vector<std::string> searchLocations);
  std::unique_ptr<ModuleInfo> findModule(
      const std::string& modName,
      const std::vector<std::string>& searchLocations,
      FileSuffixKind suffixKind);
  std::unique_ptr<ModuleInfo> findModule(
      const std::string& modName,
      FileSuffixKind suffixKind);
  std::unique_ptr<ModuleInfo> findModuleFromSource(
      const std::string& source,
      const std::string& modName,
      const std::string& filename,
      int mode);

  std::shared_ptr<AnalyzedModule> loadSingleModule(const std::string& modName);

  void setImportPath(std::vector<std::string> importPath);
  void setStubImportPath(std::string importPath);
  void setStubImportPath(std::vector<std::string> importPath);
  void setForceStrict(bool force);
  void setForceStrictFunc(ForceStrictFunc forceFunc);
  void clearAllowList();
  void setAllowListPrefix(std::vector<std::string> allowList);
  void setAllowListExact(std::vector<std::string> allowList);
  void setAllowListRegex(std::vector<std::string> allowList);

  int getAnalyzedModuleCount() const;

  /** load in the __strict__ module builtin into the loader
   *  return true if new module is added
   */
  bool loadStrictModuleModule();

  bool isModuleLoaded(const std::string& modName);
  bool enableVerboseLogging();
  bool disableAnalysis();

  PyArena* getArena() {
    return arena_;
  }

  void log(const char* format_string, ...) {
    va_list args;
    va_start(args, format_string);
    if (verbose_) {
      fprintf(stderr, "STRICT: ");
      vfprintf(stderr, format_string, args);
      fprintf(stderr, "\n");
      fflush(stderr);
    }
    va_end(args);
  }

 private:
  static const std::string kArenaNewErrorMsg;
  std::vector<std::string> importPath_;
  std::vector<std::string> stubImportPath_;
  // list of module names that's allowed for strict analysis
  // even if they are not otherwise marked as strict
  AllowListType allowList_;
  PyArena* arena_;
  // the loader owns all analyzed module produced during the analysis
  std::unordered_map<std::string, std::shared_ptr<AnalyzedModule>> modules_;
  // modules that are lazily imported but not evaluated yet
  std::unordered_set<std::string> lazy_modules_;
  std::optional<ForceStrictFunc> forceStrict_;
  ErrorSinkFactory errorSinkFactory_;
  std::unordered_set<std::shared_ptr<AnalyzedModule>> deletedModules_;
  std::vector<std::regex> allowListRegexes_;
  bool verbose_ = false;
  bool disableAnalysis_ = false;

  std::shared_ptr<AnalyzedModule> analyze(std::unique_ptr<ModuleInfo>&& modInfo);
  bool isAllowListed(const std::string& modName);
  bool isForcedStrict(const std::string& modName, const std::string& fileName);
  bool hasAllowListedParent(const std::string& modName);
  void publishOnParent(const std::string& childName);
};

} // namespace strictmod::compiler
