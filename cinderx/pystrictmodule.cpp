// Copyright (c) Meta Platforms, Inc. and affiliates.
#include "cinderx/pystrictmodule.h"

#include "cinderx/StrictModules/pycore_dependencies.h"
#include "structmember.h"

#ifndef Py_LIMITED_API

// Analysis Result
static PyObject* AnalysisResult_new(PyTypeObject* type, PyObject*, PyObject*) {
  StrictModuleAnalysisResult* self;
  self = (StrictModuleAnalysisResult*)type->tp_alloc(type, 0);
  if (self == NULL)
    return NULL;
  self->valid_module = 0;
  self->module_name = NULL;
  self->file_name = NULL;
  self->module_kind = 0;
  self->stub_kind = 0;
  self->ast = NULL;
  self->symtable = NULL;
  self->errors = NULL;
  return (PyObject*)self;
}

static int AnalysisResult_init(
    StrictModuleAnalysisResult* self,
    PyObject* args,
    PyObject*) {
  PyObject* module_name;
  PyObject* file_name;
  int module_kind;
  int stub_kind;
  PyObject* ast;
  PyObject* symtable;
  PyObject* errors;

  if (!PyArg_ParseTuple(
          args,
          "UUiiOOO",
          &module_name,
          &file_name,
          &module_kind,
          &stub_kind,
          &ast,
          &symtable,
          &errors)) {
    return -1;
  }
  self->valid_module = 1;
  self->module_name = module_name;
  Py_INCREF(self->module_name);
  self->file_name = file_name;
  Py_INCREF(self->file_name);
  self->module_kind = module_kind;
  self->stub_kind = stub_kind;
  self->ast = ast;
  Py_INCREF(self->ast);
  self->symtable = symtable;
  Py_INCREF(self->symtable);
  self->errors = errors;
  Py_INCREF(self->errors);

  return 0;
}

static PyObject* create_AnalysisResult_Helper(
    int valid_module,
    PyObject* module_name,
    PyObject* file_name,
    int module_kind,
    int stub_kind,
    PyObject* ast,
    PyObject* symtable,
    PyObject* errors) {
  StrictModuleAnalysisResult* self;
  self = (StrictModuleAnalysisResult*)PyObject_GC_New(
      StrictModuleAnalysisResult, &Ci_StrictModuleAnalysisResult_Type);
  self->valid_module = valid_module;
  self->module_name = module_name;
  self->file_name = file_name;
  self->module_kind = module_kind;
  self->stub_kind = stub_kind;
  self->ast = ast;
  self->symtable = symtable;
  self->errors = errors;
  PyObject_GC_Track(self);
  return (PyObject*)self;
}

static PyObject* create_AnalysisResult(
    strictmod::compiler::AnalyzedModule* mod,
    PyObject* module_name,
    PyObject* errors,
    PyArena* arena) {
  if (mod == NULL) {
    Py_INCREF(module_name);
    Py_INCREF(errors);
    return create_AnalysisResult_Helper(
        0, module_name, NULL, 0, 0, NULL, NULL, errors);
  }
  // all interface functions return new references
  const auto& fileStr = mod->getModuleInfo().getFilename();
  PyObject* filename = PyUnicode_FromString(fileStr.c_str());

  int mod_kind = mod->getModKindAsInt();
  int stub_kind = mod->getStubKindAsInt();
    
  Ref<> result = mod->getPyAst(arena);
  PyObject* ast = result.release();

  PyObject* symtable = nullptr;
  if (auto st = mod->getModuleInfo().getSymtable()) {
    symtable = (PyObject*)st->st_top;
    Py_INCREF(symtable);
  }
    
  Py_INCREF(module_name);
  Py_INCREF(errors);
  return create_AnalysisResult_Helper(
      1, module_name, filename, mod_kind, stub_kind, ast, symtable, errors);
}

static void AnalysisResult_dealloc(StrictModuleAnalysisResult* self) {
  if (self != NULL) {
    PyObject_GC_UnTrack(self);
    Py_XDECREF(self->module_name);
    Py_XDECREF(self->file_name);
    Py_XDECREF(self->ast);
    Py_XDECREF(self->symtable);
    Py_XDECREF(self->errors);
    PyObject_GC_Del(self);
  }
}

static int AnalysisResult_traverse(
    StrictModuleAnalysisResult* self,
    visitproc visit,
    void* arg) {
  Py_VISIT(self->module_name);
  Py_VISIT(self->file_name);
  Py_VISIT(self->ast);
  Py_VISIT(self->symtable);
  Py_VISIT(self->errors);
  return 0;
}

static int AnalysisResult_clear(StrictModuleAnalysisResult* self) {
  Py_CLEAR(self->module_name);
  Py_CLEAR(self->file_name);
  Py_CLEAR(self->ast);
  Py_CLEAR(self->symtable);
  Py_CLEAR(self->errors);
  return 0;
}

static PyMemberDef AnalysisResult_members[] = {
    {"is_valid",
     T_BOOL,
     offsetof(StrictModuleAnalysisResult, valid_module),
     READONLY,
     "whether the analyzed module is found or valid"},
    {"module_name",
     T_OBJECT,
     offsetof(StrictModuleAnalysisResult, module_name),
     READONLY,
     "module name"},
    {"file_name",
     T_OBJECT,
     offsetof(StrictModuleAnalysisResult, file_name),
     READONLY,
     "file name"},
    {"module_kind",
     T_INT,
     offsetof(StrictModuleAnalysisResult, module_kind),
     READONLY,
     "whether module is strict (1), static (2) or neither (0)"},
    {"stub_kind",
     T_INT,
     offsetof(StrictModuleAnalysisResult, stub_kind),
     READONLY,
     "stub kind represented as a bit mask."},
    {"ast",
     T_OBJECT,
     offsetof(StrictModuleAnalysisResult, ast),
     READONLY,
     "original AST of the module"},
    {"symtable",
     T_OBJECT,
     offsetof(StrictModuleAnalysisResult, symtable),
     READONLY,
     "symbol table of the module"},
    {"errors",
     T_OBJECT,
     offsetof(StrictModuleAnalysisResult, errors),
     0,
     "list of errors"},
    {NULL, 0, 0, 0, NULL} /* Sentinel */
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
PyTypeObject Ci_StrictModuleAnalysisResult_Type = {
    PyVarObject_HEAD_INIT(
        NULL,
        0) "strictmodule.StrictModuleAnalysisResult", /* tp_name */
    sizeof(StrictModuleAnalysisResult), /* tp_basicsize */
    0, /* tp_itemsize */
    (destructor)AnalysisResult_dealloc, /* tp_dealloc */
    0, /* tp_vectorcall_offset */
    0, /* tp_getattr */
    0, /* tp_setattr */
    0, /* tp_as_async */
    0, /* tp_repr */
    0, /* tp_as_number */
    0, /* tp_as_sequence */
    0, /* tp_as_mapping */
    0, /* tp_hash */
    0, /* tp_call */
    0, /* tp_str */
    0, /* tp_getattro */
    0, /* tp_setattro */
    0, /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC, /* tp_flags */
    "Analysis result of strict module loader", /* tp_doc */
    (traverseproc)AnalysisResult_traverse, /* tp_traverse */
    (inquiry)AnalysisResult_clear, /* tp_clear */
    0, /* tp_richcompare */
    0, /* tp_weaklistoffset */
    0, /* tp_iter */
    0, /* tp_iternext */
    0, /* tp_methods */
    AnalysisResult_members, /* tp_members */
    0, /* tp_getset */
    0, /* tp_base */
    0, /* tp_dict */
    0, /* tp_descr_get */
    0, /* tp_descr_set */
    0, /* tp_dictoffset */
    (initproc)AnalysisResult_init, /* tp_init */
    0, /* tp_alloc */
    AnalysisResult_new, /* tp_new */
};
#pragma GCC diagnostic pop

// Module Loader
static PyObject*
StrictModuleLoaderObject_new(PyTypeObject* type, PyObject*, PyObject*) {
  StrictModuleLoaderObject* self;
  self = (StrictModuleLoaderObject*)type->tp_alloc(type, 0);
  if (self == NULL)
    return NULL;
  self->checker = new strictmod::compiler::ModuleLoader();
  return (PyObject*)self;
}

static std::vector<std::string>
PyListToVector(PyObject* pyList) {
  Py_ssize_t size = PyList_GET_SIZE(pyList);
  std::vector<std::string> vector(size);
  for (int i = 0; i < size; ++i) {
    PyObject* elem = PyList_GetItem(pyList, i);
    if (!PyUnicode_Check(elem)) {
        throw std::runtime_error(
             fmt::format("import path is expect to be str, but got {} object",
                         elem->ob_type->tp_name));
    }
    vector.emplace_back(PyUnicode_AsUTF8(elem));
  }
  return vector;
}

static int StrictModuleLoaderObject_init(
    StrictModuleLoaderObject* self,
    PyObject* args,
    PyObject*) {
  PyObject* import_paths_obj;
  PyObject* stub_import_path_obj;
  PyObject* allow_list_obj;
  PyObject* allow_list_exact_obj;
  PyObject* load_strictmod_builtin;
  PyObject* allow_list_regex_obj = nullptr;
  load_strictmod_builtin = Py_True;
  PyObject* verbose_logging = Py_False;
  PyObject* disable_analysis = Py_False;
  if (!PyArg_ParseTuple(
          args,
          "OOOO|OOOO",
          &import_paths_obj,
          &stub_import_path_obj,
          &allow_list_obj,
          &allow_list_exact_obj,
          &load_strictmod_builtin,
          &allow_list_regex_obj,
          &verbose_logging,
          &disable_analysis)) {
    return -1;
  }

  if (!PyList_Check(import_paths_obj)) {
    PyErr_Format(
        PyExc_TypeError,
        "import_paths is expect to be list, but got %S object",
        import_paths_obj);
    return -1;
  }
  if (!PyList_Check(allow_list_obj)) {
    PyErr_Format(
        PyExc_TypeError,
        "allow_list is expect to be list, but got %S object",
        allow_list_obj);
    return -1;
  }
  if (!PyList_Check(allow_list_exact_obj)) {
    PyErr_Format(
        PyExc_TypeError,
        "allow_list_exact is expect to be list, but got %S object",
        allow_list_exact_obj);
    return -1;
  }
  if (!PyUnicode_Check(stub_import_path_obj)) {
    PyErr_Format(
        PyExc_TypeError,
        "stub_import_path is expect to be str, but got %S object",
        stub_import_path_obj);
    return -1;
  }

  if (!PyBool_Check(verbose_logging)) {
    PyErr_Format(
        PyExc_TypeError,
        "verbose_logging is expect to be bool, but got %S object",
        stub_import_path_obj);
    return -1;
  }

  if (!PyBool_Check(disable_analysis)) {
    PyErr_Format(
        PyExc_TypeError,
        "disable_analysis is expect to be bool, but got %S object",
        stub_import_path_obj);
    return -1;
  }
  
  try {
    // Import paths
    self->checker->setImportPath(PyListToVector(import_paths_obj));
    
    // allowlist for module names
    self->checker->setAllowListPrefix(PyListToVector(allow_list_obj));
    
    // allowlist for exact module names
    self->checker->setAllowListExact(PyListToVector(allow_list_exact_obj));
    
    if (allow_list_regex_obj != nullptr) {
      // allowlist for regex module names
      self->checker->setAllowListRegex(PyListToVector(allow_list_regex_obj));
    }
    
    // stub paths
    self->checker->setStubImportPath(PyUnicode_AsUTF8(stub_import_path_obj));
    
    // enable verbose logging
    int should_enable_verbose_logging = PyObject_IsTrue(verbose_logging);
    if (should_enable_verbose_logging < 0) {
      PyErr_SetString(PyExc_RuntimeError,
                      "error checking 'verbose_logging' on StrictModuleLoader");
      return -1;
    }
    if (should_enable_verbose_logging) {
      self->checker->enableVerboseLogging();
    }
    
    // disable analysis
    int should_disable_analysis = PyObject_IsTrue(disable_analysis);
    if (should_disable_analysis < 0) {
      PyErr_SetString(PyExc_RuntimeError,
                      "error checking 'disable_analysis' on StrictModuleLoader");
      return -1;
    }
    if (should_disable_analysis) {
      self->checker->disableAnalysis();
    }
    
    // load strict module builtins
    int should_load = PyObject_IsTrue(load_strictmod_builtin);
    if (should_load < 0) {
      PyErr_SetString(PyExc_RuntimeError,
                      "error checking 'should_load_builtin' on StrictModuleLoader");
      return -1;
    }
    if (should_load) {
      if (!self->checker->loadStrictModuleModule()) {
        PyErr_SetString(PyExc_RuntimeError,
                        "failed to load the strict module builtins on StrictModuleLoader object");
        return -1;
      }
    }
    
  } catch(const std::exception& ex) {
    PyErr_SetString(PyExc_RuntimeError, ex.what());
    return -1;
  }
  return 0;
}

/* StrictModuleLoader methods */

static void StrictModuleLoader_dealloc(StrictModuleLoaderObject* self) {
  delete self->checker;
  PyObject_Del(self);
}

/* Returns new reference */
static PyObject* errorInfoToTuple(const std::unique_ptr<strictmod::StrictModuleException>& error) {
  PyObject* py_lineno = NULL;
  PyObject* py_col = NULL;
  PyObject* py_msg = NULL;
  PyObject* py_file = NULL;
  PyObject* result = NULL;
  py_lineno = PyLong_FromLong(error->getLineno());
  if (!py_lineno) {
    goto err_cleanup;
  }
  py_col = PyLong_FromLong(error->getCol());
  if (!py_col) {
    goto err_cleanup;
  }
  py_msg = PyUnicode_FromString(error->getMsg().c_str());
  if (!py_msg) {
    goto err_cleanup;
  }
  py_file = PyUnicode_FromString(error->getFilename().c_str());
  if (!py_file) {
    goto err_cleanup;
  }
  result = PyTuple_Pack(4, py_msg, py_file, py_lineno, py_col);
err_cleanup:
  Py_XDECREF(py_lineno);
  Py_XDECREF(py_col);
  Py_XDECREF(py_msg);
  Py_XDECREF(py_file);
  return result;
}

static PyObject* StrictModuleLoader_check(
    StrictModuleLoaderObject* self,
    PyObject* args) {
  PyObject* mod_name;
  PyObject* errors = NULL;
  PyArena* arena;
  PyObject* result;
  int error_count = 0;
  if (!PyArg_ParseTuple(args, "U", &mod_name)) {
    return NULL;
  }
  auto *mod = self->checker->loadModule(PyUnicode_AsUTF8(mod_name));
  if (!mod) {
      goto err_cleanup;
  }
  error_count = mod->getErrorSink().getErrorCount();
  errors = PyList_New(error_count);
  if (!errors) {
    goto err_cleanup;
  }
  for (int i = 0; const auto& error : mod->getErrorSink().getErrors()) {
      PyObject *py_err_tuple = errorInfoToTuple(error);
      if (!py_err_tuple) {
        goto err_cleanup;
      }
      PyList_SET_ITEM(errors, i++, py_err_tuple);
  }
  if (PyErr_Occurred()) {
    goto err_cleanup;
  }
  arena = self->checker->getArena();
  result = create_AnalysisResult(mod, mod_name, errors, arena);
  Py_XDECREF(errors);
  return result;

err_cleanup:
  Py_XDECREF(errors);
  return NULL;
}

static PyObject* StrictModuleLoader_check_source(
    StrictModuleLoaderObject* self,
    PyObject* args) {
  // args to parse, do not decref since these are borrowed
  PyObject* source; // str or bytes
  PyObject* file_name;
  PyObject* mod_name;
  PyObject* submodule_search_locations; // list of string

  // outputs
  PyObject* errors = NULL;
  PyArena* arena;
  PyObject* result;

  // parameter parsing
  if (!PyArg_ParseTuple(
          args,
          "OUUO",
          &source,
          &file_name,
          &mod_name,
          &submodule_search_locations)) {
    return NULL;
  }
  // source str
  const char* source_str;
  // verify search locations
  if (!PyList_Check(submodule_search_locations)) {
    PyErr_Format(
        PyExc_TypeError,
        "submodule_search_locations is expect to be list, but got %S object",
        submodule_search_locations);
    return NULL;
  }
  // verify source
  PyCompilerFlags cf = _PyCompilerFlags_INIT;
  // buffer containing source str
  PyObject* source_copy;
  source_str =
      _Py_SourceAsString(source, "parse", "str or bytes", &cf, &source_copy);
  if (source_str == NULL) {
    return NULL;
  }

  int error_count = 0;
  auto *mod = self->checker->loadModuleFromSource(
      source_str,
      PyUnicode_AsUTF8(mod_name),
      PyUnicode_AsUTF8(file_name),
      PyListToVector(submodule_search_locations));
  if (!mod) {
      goto err_cleanup;
  }
  error_count = mod->getErrorSink().getErrorCount();
  errors = PyList_New(error_count);
  if (!errors) {
    goto err_cleanup;
  }
  for (int i = 0; const auto& error : mod->getErrorSink().getErrors()) {
      PyObject *py_err_tuple = errorInfoToTuple(error);
      if (!py_err_tuple) {
        goto err_cleanup;
      }
      PyList_SET_ITEM(errors, i++, py_err_tuple);
  }
  if (PyErr_Occurred()) {
    goto err_cleanup;
  }
  arena = self->checker->getArena();
  result = create_AnalysisResult(mod, mod_name, errors, arena);
  Py_XDECREF(errors);
  Py_XDECREF(source_copy);
  return result;
err_cleanup:
  Py_XDECREF(errors);
  Py_XDECREF(source_copy);
  return NULL;
}

static PyObject* StrictModuleLoader_set_force_strict(
    StrictModuleLoaderObject* self,
    PyObject* args) {
  PyObject* force_strict;
  if (!PyArg_ParseTuple(args, "O", &force_strict)) {
    return NULL;
  }
  if (!PyBool_Check(force_strict)) {
    return NULL;
  }
  bool forceStrictBool = force_strict == Py_True;
  self->checker->setForceStrict(forceStrictBool);
  Py_RETURN_TRUE;
}

static PyObject* StrictModuleLoader_set_force_strict_by_name(
    StrictModuleLoaderObject* self,
    PyObject* args) {
  const char* forced_strict_module;
  if (!PyArg_ParseTuple(args, "s", &forced_strict_module)) {
    return NULL;
  }
  self->checker->setForceStrictFunc(
    [forced_strict_module](const std::string& modName, const std::string&) {
       return modName == forced_strict_module;
    });
  Py_RETURN_TRUE;
}

static PyObject* StrictModuleLoader_get_analyzed_count(
    StrictModuleLoaderObject* self) {
  int count = self->checker->getAnalyzedModuleCount();
  return PyLong_FromLong(count);
}

static PyObject* StrictModuleLoader_delete_module(
    StrictModuleLoaderObject* self,
    PyObject* args) {
  PyObject* name;
  if (!PyArg_ParseTuple(args, "U", &name)) {
    return NULL;
  }
  self->checker->deleteModule(PyUnicode_AsUTF8(name));
  Py_RETURN_TRUE;
}

static PyMethodDef StrictModuleLoader_methods[] = {
    {"check",
     (PyCFunction)StrictModuleLoader_check,
     METH_VARARGS,
     PyDoc_STR("check(mod_name: str) -> StrictAnalysisResult")},
    {"check_source",
     (PyCFunction)StrictModuleLoader_check_source,
     METH_VARARGS,
     PyDoc_STR("check_source("
               "source:str | bytes, file_name: str, mod_name: str, "
               "submodule_search_locations:List[str])"
               " -> StrictAnalysisResult")},
    {"set_force_strict",
     (PyCFunction)StrictModuleLoader_set_force_strict,
     METH_VARARGS,
     PyDoc_STR("set_force_strict(force: bool) -> bool")},
    {"set_force_strict_by_name",
     (PyCFunction)StrictModuleLoader_set_force_strict_by_name,
     METH_VARARGS,
     PyDoc_STR("set_force_strict(modname: str) -> bool")},
    {"get_analyzed_count",
     (PyCFunction)StrictModuleLoader_get_analyzed_count,
     METH_NOARGS,
     PyDoc_STR("get_analyzed_count() -> int")},
    {"delete_module",
     (PyCFunction)StrictModuleLoader_delete_module,
     METH_VARARGS,
     PyDoc_STR("delete_module(name: str) -> bool")},
    {NULL, NULL, 0, NULL} /* sentinel */
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
PyTypeObject Ci_StrictModuleLoader_Type = {
    PyVarObject_HEAD_INIT(
        NULL,
        0) "strictmodule.StrictModuleLoader", /* tp_name */
    sizeof(StrictModuleLoaderObject), /* tp_basicsize */
    0, /* tp_itemsize */
    (destructor)StrictModuleLoader_dealloc, /* tp_dealloc */
    0, /* tp_vectorcall_offset */
    0, /* tp_getattr */
    0, /* tp_setattr */
    0, /* tp_as_async */
    0, /* tp_repr */
    0, /* tp_as_number */
    0, /* tp_as_sequence */
    0, /* tp_as_mapping */
    0, /* tp_hash */
    0, /* tp_call */
    0, /* tp_str */
    0, /* tp_getattro */
    0, /* tp_setattro */
    0, /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT, /* tp_flags */
    "Cinder implementation of strict module checker", /* tp_doc */
    0, /* tp_traverse */
    0, /* tp_clear */
    0, /* tp_richcompare */
    0, /* tp_weaklistoffset */
    0, /* tp_iter */
    0, /* tp_iternext */
    StrictModuleLoader_methods, /* tp_methods */
    0, /* tp_members */
    0, /* tp_getset */
    0, /* tp_base */
    0, /* tp_dict */
    0, /* tp_descr_get */
    0, /* tp_descr_set */
    0, /* tp_dictoffset */
    (initproc)StrictModuleLoaderObject_init, /* tp_init */
    0, /* tp_alloc */
    StrictModuleLoaderObject_new, /* tp_new */
};
#pragma GCC diagnostic pop

#endif /* Py_LIMITED_API */
