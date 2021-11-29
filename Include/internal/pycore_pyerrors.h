#ifndef Py_INTERNAL_PYERRORS_H
#define Py_INTERNAL_PYERRORS_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include "pycore_interp.h"          // struct _is
#include "pycore_global_objects.h"  // _PyInterpreterState_SET_OBJECT()

static inline PyObject* _PyErr_Occurred(PyThreadState *tstate)
{
    assert(tstate != NULL);
    return tstate->curexc_type;
}

static inline void _PyErr_ClearExcState(_PyErr_StackItem *exc_state)
{
    PyObject *t, *v, *tb;
    t = exc_state->exc_type;
    v = exc_state->exc_value;
    tb = exc_state->exc_traceback;
    exc_state->exc_type = NULL;
    exc_state->exc_value = NULL;
    exc_state->exc_traceback = NULL;
    Py_XDECREF(t);
    Py_XDECREF(v);
    Py_XDECREF(tb);
}


PyAPI_FUNC(void) _PyErr_Fetch(
    PyThreadState *tstate,
    PyObject **type,
    PyObject **value,
    PyObject **traceback);

PyAPI_FUNC(int) _PyErr_ExceptionMatches(
    PyThreadState *tstate,
    PyObject *exc);

PyAPI_FUNC(void) _PyErr_Restore(
    PyThreadState *tstate,
    PyObject *type,
    PyObject *value,
    PyObject *traceback);

PyAPI_FUNC(void) _PyErr_SetObject(
    PyThreadState *tstate,
    PyObject *type,
    PyObject *value);

PyAPI_FUNC(void) _PyErr_ChainStackItem(
    _PyErr_StackItem *exc_info);

PyAPI_FUNC(void) _PyErr_Clear(PyThreadState *tstate);

PyAPI_FUNC(void) _PyErr_SetNone(PyThreadState *tstate, PyObject *exception);

PyAPI_FUNC(PyObject *) _PyErr_NoMemory(PyThreadState *tstate);

PyAPI_FUNC(void) _PyErr_SetString(
    PyThreadState *tstate,
    PyObject *exception,
    const char *string);

PyAPI_FUNC(PyObject *) _PyErr_Format(
    PyThreadState *tstate,
    PyObject *exception,
    const char *format,
    ...);

PyAPI_FUNC(void) _PyErr_NormalizeException(
    PyThreadState *tstate,
    PyObject **exc,
    PyObject **val,
    PyObject **tb);

PyAPI_FUNC(PyObject *) _PyErr_FormatFromCauseTstate(
    PyThreadState *tstate,
    PyObject *exception,
    const char *format,
    ...);

PyAPI_FUNC(int) _PyErr_CheckSignalsTstate(PyThreadState *tstate);

PyAPI_FUNC(void) _Py_DumpExtensionModules(int fd, PyInterpreterState *interp);

extern PyObject* _Py_Offer_Suggestions(PyObject* exception);
PyAPI_FUNC(Py_ssize_t) _Py_UTF8_Edit_Cost(PyObject *str_a, PyObject *str_b,
                                          Py_ssize_t max_cost);

PyAPI_FUNC(void) _Py_NO_RETURN _Py_FatalRefcountErrorFunc(
    const char *func,
    const char *message);

#define _Py_FatalRefcountError(message) _Py_FatalRefcountErrorFunc(__func__, message)


//////////////////////////////////
// C-API exceptions

#define DEFINE_GETTER(NAME) \
    static inline PyObject * \
    _PyInterpreterState_GetObject_ ## NAME(PyInterpreterState *interp) \
    { \
        return _PyInterpreterState_GET_OBJECT(interp, NAME); \
    }

DEFINE_GETTER(ArithmeticError);
DEFINE_GETTER(AssertionError);
DEFINE_GETTER(AttributeError);
DEFINE_GETTER(BaseException);
DEFINE_GETTER(BaseExceptionGroup);
DEFINE_GETTER(BlockingIOError);
DEFINE_GETTER(BrokenPipeError);
DEFINE_GETTER(BufferError);
DEFINE_GETTER(ChildProcessError);
DEFINE_GETTER(ConnectionAbortedError);
DEFINE_GETTER(ConnectionError);
DEFINE_GETTER(ConnectionRefusedError);
DEFINE_GETTER(ConnectionResetError);
DEFINE_GETTER(EOFError);
DEFINE_GETTER(Exception);
DEFINE_GETTER(FileExistsError);
DEFINE_GETTER(FileNotFoundError);
DEFINE_GETTER(FloatingPointError);
DEFINE_GETTER(GeneratorExit);
DEFINE_GETTER(ImportError);
DEFINE_GETTER(IndentationError);
DEFINE_GETTER(IndexError);
DEFINE_GETTER(InterruptedError);
DEFINE_GETTER(IsADirectoryError);
DEFINE_GETTER(KeyError);
DEFINE_GETTER(KeyboardInterrupt);
DEFINE_GETTER(LookupError);
DEFINE_GETTER(MemoryError);
DEFINE_GETTER(ModuleNotFoundError);
DEFINE_GETTER(NameError);
DEFINE_GETTER(NotADirectoryError);
DEFINE_GETTER(NotImplementedError);
DEFINE_GETTER(OSError);
DEFINE_GETTER(OverflowError);
DEFINE_GETTER(PermissionError);
DEFINE_GETTER(ProcessLookupError);
DEFINE_GETTER(RecursionError);
DEFINE_GETTER(ReferenceError);
DEFINE_GETTER(RuntimeError);
DEFINE_GETTER(StopAsyncIteration);
DEFINE_GETTER(StopIteration);
DEFINE_GETTER(SyntaxError);
DEFINE_GETTER(SystemError);
DEFINE_GETTER(SystemExit);
DEFINE_GETTER(TabError);
DEFINE_GETTER(TimeoutError);
DEFINE_GETTER(TypeError);
DEFINE_GETTER(UnboundLocalError);
DEFINE_GETTER(UnicodeDecodeError);
DEFINE_GETTER(UnicodeEncodeError);
DEFINE_GETTER(UnicodeError);
DEFINE_GETTER(UnicodeTranslateError);
DEFINE_GETTER(ValueError);
DEFINE_GETTER(ZeroDivisionError);

DEFINE_GETTER(BytesWarning);
DEFINE_GETTER(DeprecationWarning);
DEFINE_GETTER(EncodingWarning);
DEFINE_GETTER(FutureWarning);
DEFINE_GETTER(ImportWarning);
DEFINE_GETTER(PendingDeprecationWarning);
DEFINE_GETTER(ResourceWarning);
DEFINE_GETTER(RuntimeWarning);
DEFINE_GETTER(SyntaxWarning);
DEFINE_GETTER(UnicodeWarning);
DEFINE_GETTER(UserWarning);
DEFINE_GETTER(Warning);

#undef DEFINE_GETTER


#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_PYERRORS_H */
