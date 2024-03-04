// Copyright (c) Meta Platforms, Inc. and affiliates.
#include <Python.h>

#include "cinderx/StrictModules/Compiler/module_loader.h"

int main() {
  Py_SetPythonHome(L"/Users/kronuz/Development/cinder/Lib");
  Py_SetPath(L"/Users/kronuz/Development/cinder/Lib");
  Py_Initialize();
  
  const char *module_name = "manifold.clients.cli.manifold";
  
  auto loader = std::make_unique<strictmod::compiler::ModuleLoader>();
  
  // Import paths
  loader->setImportPath({
    "/Users/kronuz/Development/cinder/Lib",
    "/Users/kronuz/manifold",
  });
  
  // Allowlist for module names
  loader->setAllowListPrefix({
    "pytest",
    "ctypes",
  });
  
  // allowlist for exact module names
  loader->setAllowListExact({
    "",
  });
  
  // allowlist for regex module names
  loader->setAllowListRegex({
    ".*",
  });
  
  // stub paths
  loader->setStubImportPath("/Users/kronuz/Development/cinder/cinderx/cinderx/compiler/strict/stubs");
  
  // verbose logging
  int should_enable_verbose_logging = 1;
  if (should_enable_verbose_logging) {
    loader->enableVerboseLogging();
  }
  
  // disable analysis
  int should_disable_analysis = 0;
  if (should_disable_analysis) {
    loader->disableAnalysis();
  }
  
  // load_strict_module_builtins
  int should_load_strict_module_builtins = 0;
  if (should_load_strict_module_builtins) {
    loader->loadStrictModuleModule();
  }
  
  // force strict
  loader->setForceStrict(true);
  
  auto analyzedModule = loader->loadModule(module_name);
  if (analyzedModule == NULL) {
    fprintf(stderr, "failed to check\n");
    return -1;
  }
  
  for (const auto& error : analyzedModule->getErrorSink().getErrors()) {
    fprintf(stderr, "%s (%s) at %s:%d:%d\n",
            error->getMsg().c_str(),
            error->getScopeName().c_str(),
            error->getFilename().c_str(),
            error->getLineno(),
            error->getCol());
  }
  
  return 0;
}
