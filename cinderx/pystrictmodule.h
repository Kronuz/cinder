// Copyright (c) Meta Platforms, Inc. and affiliates.
#pragma once

#include "Python.h"

#include "cinderx/StrictModules/Compiler/module_loader.h"

#ifndef Py_LIMITED_API
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  PyObject_HEAD strictmod::compiler::ModuleLoader* checker;
} StrictModuleLoaderObject;

typedef struct {
  PyObject_HEAD int valid_module;
  PyObject* module_name;
  PyObject* file_name;
  int module_kind;
  int stub_kind;
  PyObject* ast;
  PyObject* symtable;
  PyObject* errors;
} StrictModuleAnalysisResult;

CiAPI_DATA(PyTypeObject) Ci_StrictModuleLoader_Type;
CiAPI_DATA(PyTypeObject) Ci_StrictModuleAnalysisResult_Type;

#define StrictModuleLoaderObject_Check(v) \
  (Py_TYPE(v) == &Ci_StrictModuleLoader_Type)

#ifdef __cplusplus
}
#endif
#endif /* Py_LIMITED_API */
