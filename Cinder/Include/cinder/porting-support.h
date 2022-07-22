#pragma once

#include "Python.h"

#include "internal/pycore_moduleobject.h"

#include "Jit/log.h"

#include "cinder/port-assert.h"

/*
 * Collection of misc. features implemented for Cinder which were previously
 * spread throughout the CPython code. Stubbed versions of these functions are
 * in stubs.cpp.
 *
 * Where possible please avoid re-implementing these back in the CPython sources.
 * Instead, make appropriate new .c/.cpp/.h files here. This will help simplify
 * future upstream merges, and make our additions/alternations clear and explicit.
 *
 * The file references indicate where these functions can be found in the 3.8
 * Cinder sources.
 */


// When implementing these functions, this may not be needed in new headers if
// the functions are only used by Cinder C++ code.
#ifdef __cplusplus
extern "C" {
#endif


// Include/bltinmodule.h
PyObject *builtin_next(PyObject *self, PyObject *const *args, Py_ssize_t nargs);
PyObject *_PyBuiltin_Next(PyObject *it, PyObject *def);


// Include/cpython/dictobject.h
PyObject *_PyCheckedDict_New(PyTypeObject *type);
PyObject *_PyCheckedDict_NewPresized(PyTypeObject *type, Py_ssize_t minused);

int _PyCheckedDict_Check(PyObject *x);
PyAPI_FUNC(int) _PyCheckedDict_TypeCheck(PyTypeObject *type);


// Include/listobject.h
PyAPI_FUNC(PyObject *) _PyCheckedList_GetItem(PyObject *self, Py_ssize_t);
PyAPI_FUNC(PyObject *) _PyCheckedList_New(PyTypeObject *type, Py_ssize_t);
PyAPI_FUNC(int) _PyCheckedList_TypeCheck(PyTypeObject *type);

PyAPI_FUNC(PyObject *) _PyList_Repeat(PyListObject *, Py_ssize_t);
PyAPI_FUNC(int) _PyList_APPEND(PyObject *, PyObject *);


// Include/code.h
#define CO_STATICALLY_COMPILED   0x4000000
#define CO_FUTURE_LAZY_IMPORTS   0x8000000
#define CO_SHADOW_FRAME          0x10000000
#define CO_NORMAL_FRAME          0x20000000
#define CO_SUPPRESS_JIT          0x40000000


// Include/cpython/object.h
PyObject **
_PyObject_GetDictPtrAtOffset(PyObject *obj, Py_ssize_t dictoffset);


// Python/ceval.h
PyAPI_FUNC(int) _Py_DoRaise(PyThreadState *, PyObject *, PyObject *);

PyAPI_FUNC(PyObject *) _PyEval_SuperLookupMethodOrAttr(
    PyThreadState *tstate,
    PyObject *super_globals,
    PyTypeObject *type,
    PyObject *self,
    PyObject *name,
    int call_no_args,
    int *meth_found);

PyAPI_FUNC(PyObject *) _PyEval_GetAIter(PyObject *obj);
PyAPI_FUNC(PyObject *) _PyEval_GetANext(PyObject *aiter);

PyObject *special_lookup(PyThreadState *tstate, PyObject *o, _Py_Identifier *id);
int check_args_iterable(PyThreadState *tstate, PyObject *func, PyObject *args);
void format_kwargs_error(PyThreadState *tstate, PyObject *func, PyObject *kwargs);
void format_awaitable_error(PyThreadState *tstate, PyTypeObject *type, int prevopcode);


// Include/genobject.h
typedef enum {
    /* Generator has freshly been returned from a call to the function itself.
       Execution of user code has not yet begun. */
    _PyJitGenState_JustStarted,
    /* Execution is in progress and is currently active or the generator is
       suspended. */
    _PyJitGenState_Running,
    /* Generator has completed execution and should not be resumed again. */
    _PyJitGenState_Completed,
} _PyJitGenState;

typedef struct {
    PyObject_HEAD
    PyObject *wh_coro_or_result_NOT_IMPLEMENTED;
    PyObject *wh_waiter_NOT_IMPLEMENTED;
} PyWaitHandleObject;

PyAPI_DATA(PyTypeObject) PyWaitHandle_Type;

#define _PyWaitHandle_CheckExact(op) (Py_TYPE(op) == &PyWaitHandle_Type)

PyAPI_FUNC(PyObject *)
    _PyWaitHandle_New(PyObject *coro_or_result, PyObject *waiter);
PyAPI_FUNC(void) _PyWaitHandle_Release(PyObject *wait_handle);
PyAPI_FUNC(PyObject *) _PyCoro_NewNoFrame(
    PyThreadState *tstate, PyCodeObject *code);
PyAPI_FUNC(PyObject *) _PyAsyncGen_NewNoFrame(PyCodeObject *code);
PyAPI_FUNC(PyObject *) _PyGen_NewNoFrame(PyCodeObject *code);

// This needs to be "static inline" when implemented.
void _PyAwaitable_SetAwaiter(PyObject *receiver, PyObject *awaiter);


// Include/object.h

// Bit 22 is no longer available
#define Py_TPFLAGS_NO_SHADOWING_INSTANCES [](){PORT_ASSERT("need a new bit for flag"); return 0;}()

PyAPI_FUNC(void) _PyType_ClearNoShadowingInstances(struct _typeobject *, PyObject *obj);
PyAPI_FUNC(void) _PyType_SetNoShadowingInstances(struct _typeobject *);
PyAPI_FUNC(PyObject *) _PyType_GetMethodCacheStats(void);
PyAPI_FUNC(void) _PyType_ResetMethodCacheStats(void);
PyAPI_FUNC(void) _PyType_SetReadonlyProperties(struct _typeobject *);
PyAPI_FUNC(PyObject *) _PyType_GetSwitchboard(void);

// This will probably end up back as a macro
int Py_IS_IMMORTAL(PyObject*);


// Include/moduleobject.h
typedef struct {
    PyModuleObject base;
    PyObject *globals;
    PyObject *global_setter;
    PyObject *originals;
    PyObject *static_thunks;
    PyObject *imported_from;
} PyStrictModuleObject;

#define PyModule_Dict(op) (PyStrictModule_Check(op) ? ((PyStrictModuleObject *)op)->globals : ((PyModuleObject *)op)->md_dict)
extern Py_ssize_t strictmodule_dictoffset;

int strictmodule_is_unassigned(PyObject *dict, PyObject *name);
PyObject * PyStrictModule_GetOriginal(PyStrictModuleObject *self, PyObject *name);
int _Py_do_strictmodule_patch(PyObject *self, PyObject *name, PyObject *value);


// Originally in Include/object.h
/* This is a generic type instantiation */
#define Py_TPFLAGS_GENERIC_TYPE_INST (1UL << 6)
/* This type is a generic type definition */
#define Py_TPFLAGS_GENERIC_TYPE_DEF (1UL << 7)


// Include/cpython/abstract.h
#define PY_VECTORCALL_ARGUMENTS_OFFSET ((size_t)1 << (8 * sizeof(size_t) - 1))
#define _Py_AWAITED_CALL_MARKER  ((size_t)1 << (8 * sizeof(size_t) - 2))
#define _Py_VECTORCALL_INVOKED_STATICALLY_BIT_POS  (8 * sizeof(size_t) - 3)
#define _Py_VECTORCALL_INVOKED_STATICALLY                                     \
    ((size_t)1 << _Py_VECTORCALL_INVOKED_STATICALLY_BIT_POS)
#define _Py_VECTORCALL_INVOKED_CLASSMETHOD ((size_t)1 << (8 * sizeof(size_t) - 5))
#define PY_VECTORCALL_ARGUMENT_MASK                                           \
    ~(PY_VECTORCALL_ARGUMENTS_OFFSET | _Py_AWAITED_CALL_MARKER |              \
      _Py_VECTORCALL_INVOKED_STATICALLY | _Py_VECTORCALL_INVOKED_METHOD |     \
      _Py_VECTORCALL_INVOKED_CLASSMETHOD)


// This needs to be "static inline" when implemented.
Py_ssize_t PyVectorcall_FLAGS(size_t n);

PyObject *_PyVectorcall_Call(PyObject *callable, PyObject *tuple, PyObject *kwargs, size_t flags);

PyObject *_PyObject_Call1Arg(PyObject *func, PyObject *arg0);


// Include/cpython/tupleobject.h
PyAPI_FUNC(PyObject *) _PyTuple_Repeat(PyTupleObject *, Py_ssize_t);


// Include/cpython/dictobject.h
int _PyDict_SetItem(PyObject *op, PyObject *key, PyObject *value);
//PyAPI_FUNC(int) _PyDict_DelItemId(PyObject *mp, struct _Py_Identifier *key);
PyAPI_FUNC(PyObject *) _PyDict_GetItemId(PyObject *dp, struct _Py_Identifier *key);


// Include/frameobject.h
PyFrameObject * _PyFrame_NewWithBuiltins_NoTrack(PyThreadState *,
                                                 PyCodeObject *,
                                                 PyObject *, PyObject *,
                                                 PyObject *);

// Include/errors.h
int _PyErr_OCCURRED();


// Include/pystate.h
void _PyThreadState_SetProfileInterpAll(int enabled);


// Include/funcobject.h
PyAPI_FUNC(PyObject *) _PyFunction_GetSwitchboard(void);
PyAPI_FUNC(PyObject *) _PyFunction_GetBuiltins(PyFunctionObject *func);


// Include/methodobject.h
/* This function has a statically typed function, and ml_meth is the
 * descriptor for it */
#define METH_TYPED 0x0200

/* Flag marks this as optional */
#define _Py_SIG_OPTIONAL 0x01
/* Flag marks this a type param, high bits are type index */
#define _Py_SIG_TYPE_PARAM 0x02
#define _Py_SIG_TYPE_PARAM_IDX(x) ((x << 2) | _Py_SIG_TYPE_PARAM)
#define _Py_SIG_TYPE_PARAM_OPT(x)                                             \
    ((x << 2) | _Py_SIG_TYPE_PARAM | _Py_SIG_OPTIONAL)


typedef struct _Py_SigElement {
    int se_argtype;
    PyObject *se_default_value;
    const char *se_name;
} _Py_SigElement;

typedef struct {
    void *tmd_meth; /* The C function that implements it */
    const _Py_SigElement *const *tmd_sig; /* The function signature */
    int tmd_ret;
} _PyTypedMethodDef;

#define _Py_TYPED_SIGNATURE(name, ret_type, ...)                              \
    static const _Py_SigElement *const name##_sig[] = {__VA_ARGS__};          \
    static _PyTypedMethodDef name##_def = {                                   \
        name,                                                                 \
        name##_sig,                                                           \
        ret_type,                                                             \
    }

PyObject *_PyMethodDef_GetTypedSignature(PyMethodDef *method);

#ifdef __cplusplus
} // extern "C"
#endif
