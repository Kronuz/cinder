// Copyright (c) Meta Platforms, Inc. and affiliates. (http://www.meta.com)

#include "Python.h"
#include "cinder/hooks.h"

#include "Jit/pyjit.h"
#include "Common/watchers.h"
#include "ParallelGC/parallel_gc.h"
#include "Shadowcode/shadowcode.h"
#include "StaticPython/classloader.h"
#include "StaticPython/descrobject_vectorcall.h"
#include "StaticPython/methodobject_vectorcall.h"
#include "internal/pycore_shadow_frame.h"


static void init_already_existing_funcs() {
  PyUnstable_GC_VisitObjects([](PyObject* obj, void*){
    if (PyFunction_Check(obj)) {
      PyEntry_init((PyFunctionObject*)obj);
    }
    return 1;
  }, nullptr);
}

static void init_already_existing_types() {
  PyUnstable_GC_VisitObjects([](PyObject* obj, void*) {
    if (PyType_Check(obj) && PyType_HasFeature((PyTypeObject*)obj, Py_TPFLAGS_READY)) {
      _PyJIT_TypeCreated((PyTypeObject*)obj);
    }
    return 1;
  }, nullptr);
}

static void shadowcode_code_sizeof(struct _PyShadowCode *shadow, Py_ssize_t *res) {
    res += sizeof(_PyShadowCode);
    res += sizeof(PyObject *) * shadow->l1_cache.size;
    res += sizeof(PyObject *) * shadow->cast_cache.size;
    res += sizeof(PyObject **) * shadow->globals_size;
    res += sizeof(_PyShadow_InstanceAttrEntry **) *
            shadow->polymorphic_caches_size;
    res += sizeof(_FieldCache) * shadow->field_cache_size;
    res += sizeof(_Py_CODEUNIT) * shadow->len;
}

static inline int _PyStrictModule_Check(PyObject* obj) {
  return PyStrictModule_Check(obj);
}

static int cinder_init() {
  Ci_hook_type_created = _PyJIT_TypeCreated;
  Ci_hook_type_destroyed = _PyJIT_TypeDestroyed;
  Ci_hook_type_name_modified = _PyJIT_TypeNameModified;
  Ci_hook_type_pre_setattr = _PyClassLoader_InitTypeForPatching;
  Ci_hook_type_setattr = _PyClassLoader_UpdateSlot;
  Ci_hook_JIT_GetProfileNewInterpThread = _PyJIT_GetProfileNewInterpThreads;
  Ci_hook_JIT_GetFrame = _PyJIT_GetFrame;
  Ci_hook_PyCMethod_New = Ci_PyCMethod_New_METH_TYPED;
  Ci_hook_PyDescr_NewMethod = Ci_PyDescr_NewMethod_METH_TYPED;
  Ci_hook_WalkStack = Ci_WalkStack;
  Ci_hook_code_sizeof_shadowcode = shadowcode_code_sizeof;
  Ci_hook_PyShadowFrame_HasGen = _PyShadowFrame_HasGen;
  Ci_hook_PyShadowFrame_GetGen = _PyShadowFrame_GetGen;
  Ci_hook_PyJIT_GenVisitRefs = _PyJIT_GenVisitRefs;
  Ci_hook_PyJIT_GenDealloc = _PyJIT_GenDealloc;
  Ci_hook_PyJIT_GenSend = _PyJIT_GenSend;
  Ci_hook_PyJIT_GenYieldFromValue = _PyJIT_GenYieldFromValue;
  Ci_hook_PyJIT_GenMaterializeFrame = _PyJIT_GenMaterializeFrame;
  Ci_hook__PyShadow_FreeAll = _PyShadow_FreeAll;
  Ci_hook_PyStrictModule_Check = _PyStrictModule_Check;

  init_already_existing_types();

  // Prevent the linker from omitting the object file containing the parallel
  // GC implementation. This is the only reference from another translation
  // unit to symbols defined in the file. Without it the linker will omit the
  // object file when linking the libpython archive into the main python
  // binary.
  //
  // TODO(T168696266): Remove this once we migrate to cinderx.
  PyObject *res = Cinder_GetParallelGCSettings();
  if (res == nullptr) {
    return -1;
  }
  Py_DECREF(res);

  if (Ci_Watchers_Init()) {
    return -1;
  }

  if (_PyJIT_Initialize()) {
    return -1;
  }
  init_already_existing_funcs();

  Ci_cinderx_initialized = 1;

  return 0;
}

// Attempts to shutdown CinderX. This is very much a best-effort with the
// primary goals being to ensure Python shuts down without crashing, and
// tests which do some kind of re-initialization continue to work. A secondary
// goal is to one day make it possible to arbitrarily load/relaod CinderX at
// runtime. However, for now the only thing we truly support is loading
// CinderX once ASAP on start-up, and then never unloading it until complete
// process shutdown.
static int cinder_fini() {
  _PyClassLoader_ClearCache();

  if (PyThreadState_Get()->shadow_frame) {
    // If any Python code is running we can't tell if JIT code is in use. Even
    // if every frame in the callstack is interpreter-owned, some of them could
    // be the result of deopt and JIT code may still be on the native stack.
    JIT_DABORT("Python code still running on CinderX unload");
    JIT_LOG("Python code is executing, cannot cleanly shutdown CinderX.");
    return -1;
  }

  if (_PyJIT_Finalize()) {
    return -1;
  }

  if (Ci_cinderx_initialized && Ci_hook__PyShadow_FreeAll()) {
    return -1;
  }

  Ci_hook_type_created = nullptr;
  Ci_hook_type_destroyed = nullptr;
  Ci_hook_type_name_modified = nullptr;
  Ci_hook_type_pre_setattr = nullptr;
  Ci_hook_type_setattr = nullptr;
  Ci_hook_JIT_GetProfileNewInterpThread = nullptr;
  Ci_hook_JIT_GetFrame = nullptr;
  Ci_hook_PyDescr_NewMethod = nullptr;
  Ci_hook_WalkStack = nullptr;
  Ci_hook_code_sizeof_shadowcode = nullptr;
  Ci_hook_PyShadowFrame_HasGen = nullptr;
  Ci_hook_PyShadowFrame_GetGen = nullptr;
  Ci_hook_PyJIT_GenVisitRefs = nullptr;
  Ci_hook_PyJIT_GenDealloc = nullptr;
  Ci_hook_PyJIT_GenSend = nullptr;
  Ci_hook_PyJIT_GenYieldFromValue = nullptr;
  Ci_hook_PyJIT_GenMaterializeFrame = nullptr;
  Ci_hook__PyShadow_FreeAll = nullptr;

  Ci_cinderx_initialized = 0;

  return 0;
}

static bool g_was_initialized = false;

static PyObject* init(PyObject * /*self*/, PyObject * /*obj*/) {
  if (g_was_initialized) {
    Py_RETURN_FALSE;
  }
  if (cinder_init()) {
    PyErr_SetString(PyExc_RuntimeError, "Failed to initialize CinderX");
    return NULL;
  }
  g_was_initialized = true;
  Py_RETURN_TRUE;
}

static void module_free(void *) {
  if (g_was_initialized) {
    g_was_initialized = false;
    JIT_CHECK(cinder_fini() == 0, "Failed to finalize CinderX");
  }
}

static PyMethodDef _cinderx_methods[] = {
    {"init", init, METH_NOARGS,
     "This must be called early. Preferably before any user code is run."},
    {nullptr, nullptr, 0, nullptr}};

static struct PyModuleDef _cinderx_module = {
    PyModuleDef_HEAD_INIT,  "_cinderx", "The internal CinderX extension module",
    /*m_size=*/-1, // Doesn't support sub-interpreters
    _cinderx_methods,
    /*m_slots=*/nullptr,
    /*m_traverse=*/nullptr,
    /*m_clear=*/nullptr,
    /*m_free=*/module_free,
};

PyMODINIT_FUNC PyInit__cinderx(void) {
  // Deliberate single-phase initialization.
  return PyModule_Create(&_cinderx_module);
}
