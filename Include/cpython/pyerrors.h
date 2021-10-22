#ifndef Py_CPYTHON_ERRORS_H
#  error "this header file must not be included directly"
#endif

//#include "pystate.h"    // _PyinterpreterState_LOOK_UP_OBJECT_CURRENT()

/* Error objects */

/* PyException_HEAD defines the initial segment of every exception class. */
#define PyException_HEAD PyObject_HEAD PyObject *dict;\
             PyObject *args; PyObject *traceback;\
             PyObject *context; PyObject *cause;\
             char suppress_context;

typedef struct {
    PyException_HEAD
} PyBaseExceptionObject;

typedef struct {
    PyException_HEAD
    PyObject *msg;
    PyObject *filename;
    PyObject *lineno;
    PyObject *offset;
    PyObject *end_lineno;
    PyObject *end_offset;
    PyObject *text;
    PyObject *print_file_and_line;
} PySyntaxErrorObject;

typedef struct {
    PyException_HEAD
    PyObject *msg;
    PyObject *name;
    PyObject *path;
} PyImportErrorObject;

typedef struct {
    PyException_HEAD
    PyObject *encoding;
    PyObject *object;
    Py_ssize_t start;
    Py_ssize_t end;
    PyObject *reason;
} PyUnicodeErrorObject;

typedef struct {
    PyException_HEAD
    PyObject *code;
} PySystemExitObject;

typedef struct {
    PyException_HEAD
    PyObject *myerrno;
    PyObject *strerror;
    PyObject *filename;
    PyObject *filename2;
#ifdef MS_WINDOWS
    PyObject *winerror;
#endif
    Py_ssize_t written;   /* only for BlockingIOError, -1 otherwise */
} PyOSErrorObject;

typedef struct {
    PyException_HEAD
    PyObject *value;
} PyStopIterationObject;

typedef struct {
    PyException_HEAD
    PyObject *name;
} PyNameErrorObject;

typedef struct {
    PyException_HEAD
    PyObject *obj;
    PyObject *name;
} PyAttributeErrorObject;

/* Compatibility typedefs */
typedef PyOSErrorObject PyEnvironmentErrorObject;
#ifdef MS_WINDOWS
typedef PyOSErrorObject PyWindowsErrorObject;
#endif

/* Error handling definitions */

PyAPI_FUNC(void) _PyErr_SetKeyError(PyObject *);
PyAPI_FUNC(_PyErr_StackItem*) _PyErr_GetTopmostException(PyThreadState *tstate);
PyAPI_FUNC(void) _PyErr_GetExcInfo(PyThreadState *, PyObject **, PyObject **, PyObject **);

/* Context manipulation (PEP 3134) */

PyAPI_FUNC(void) _PyErr_ChainExceptions(PyObject *, PyObject *, PyObject *);

/* Like PyErr_Format(), but saves current exception as __context__ and
   __cause__.
 */
PyAPI_FUNC(PyObject *) _PyErr_FormatFromCause(
    PyObject *exception,
    const char *format,   /* ASCII-encoded string  */
    ...
    );

/* In exceptions.c */

/* Helper that attempts to replace the current exception with one of the
 * same type but with a prefix added to the exception text. The resulting
 * exception description looks like:
 *
 *     prefix (exc_type: original_exc_str)
 *
 * Only some exceptions can be safely replaced. If the function determines
 * it isn't safe to perform the replacement, it will leave the original
 * unmodified exception in place.
 *
 * Returns a borrowed reference to the new exception (if any), NULL if the
 * existing exception was left in place.
 */
PyAPI_FUNC(PyObject *) _PyErr_TrySetFromCause(
    const char *prefix_format,   /* ASCII-encoded string  */
    ...
    );

/* In signalmodule.c */

int PySignal_SetWakeupFd(int fd);
PyAPI_FUNC(int) _PyErr_CheckSignals(void);

/* Support for adding program text to SyntaxErrors */

PyAPI_FUNC(void) PyErr_SyntaxLocationObject(
    PyObject *filename,
    int lineno,
    int col_offset);

PyAPI_FUNC(void) PyErr_RangedSyntaxLocationObject(
    PyObject *filename,
    int lineno,
    int col_offset,
    int end_lineno,
    int end_col_offset);

PyAPI_FUNC(PyObject *) PyErr_ProgramTextObject(
    PyObject *filename,
    int lineno);

PyAPI_FUNC(PyObject *) _PyUnicodeTranslateError_Create(
    PyObject *object,
    Py_ssize_t start,
    Py_ssize_t end,
    const char *reason          /* UTF-8 encoded string */
    );

PyAPI_FUNC(void) _PyErr_WriteUnraisableMsg(
    const char *err_msg,
    PyObject *obj);

PyAPI_FUNC(void) _Py_NO_RETURN _Py_FatalErrorFunc(
    const char *func,
    const char *message);

PyAPI_FUNC(void) _Py_NO_RETURN _Py_FatalErrorFormat(
    const char *func,
    const char *format,
    ...);

#define Py_FatalError(message) _Py_FatalErrorFunc(__func__, message)


/* exception types */

#define GETTER(PYNAME) \
    _PyAPI_DECLARE_GLOBAL_GETTER(PyExc_ ## PYNAME)

#define _Py_EXC_TYPE(PYNAME) \
    _Py_CURRENT_GLOBAL_OBJECT(PyExc_ ## PYNAME)

// getters
GETTER(BaseException);
GETTER(Exception);
GETTER(TypeError);
GETTER(StopAsyncIteration);
GETTER(StopIteration);
GETTER(GeneratorExit);
GETTER(SystemExit);
GETTER(KeyboardInterrupt);
GETTER(ImportError);
GETTER(ModuleNotFoundError);
GETTER(OSError);
GETTER(EOFError);
GETTER(RuntimeError);
GETTER(RecursionError);
GETTER(NotImplementedError);
GETTER(NameError);
GETTER(UnboundLocalError);
GETTER(AttributeError);
GETTER(SyntaxError);
GETTER(IndentationError);
GETTER(TabError);
GETTER(LookupError);
GETTER(IndexError);
GETTER(KeyError);
GETTER(ValueError);
GETTER(UnicodeError);
GETTER(UnicodeEncodeError);
GETTER(UnicodeDecodeError);
GETTER(UnicodeTranslateError);
GETTER(AssertionError);
GETTER(ArithmeticError);
GETTER(FloatingPointError);
GETTER(OverflowError);
GETTER(ZeroDivisionError);
GETTER(SystemError);
GETTER(ReferenceError);
GETTER(MemoryError);
GETTER(BufferError);
GETTER(ConnectionError);
GETTER(BlockingIOError);
GETTER(BrokenPipeError);
GETTER(ChildProcessError);
GETTER(ConnectionAbortedError);
GETTER(ConnectionRefusedError);
GETTER(ConnectionResetError);
GETTER(FileExistsError);
GETTER(FileNotFoundError);
GETTER(IsADirectoryError);
GETTER(NotADirectoryError);
GETTER(InterruptedError);
GETTER(PermissionError);
GETTER(ProcessLookupError);
GETTER(TimeoutError);

// compatibility shims
#define PyExc_BaseException _Py_EXC_TYPE(BaseException)
#define PyExc_Exception _Py_EXC_TYPE(Exception)
#define PyExc_StopAsyncIteration _Py_EXC_TYPE(StopAsyncIteration)
#define PyExc_StopIteration _Py_EXC_TYPE(StopIteration)
#define PyExc_GeneratorExit _Py_EXC_TYPE(GeneratorExit)
#define PyExc_ArithmeticError _Py_EXC_TYPE(ArithmeticError)
#define PyExc_LookupError _Py_EXC_TYPE(LookupError)
#define PyExc_AssertionError _Py_EXC_TYPE(AssertionError)
#define PyExc_AttributeError _Py_EXC_TYPE(AttributeError)
#define PyExc_BufferError _Py_EXC_TYPE(BufferError)
#define PyExc_EOFError _Py_EXC_TYPE(EOFError)
#define PyExc_FloatingPointError _Py_EXC_TYPE(FloatingPointError)
#define PyExc_OSError _Py_EXC_TYPE(OSError)
#define PyExc_ImportError _Py_EXC_TYPE(ImportError)
#define PyExc_ModuleNotFoundError _Py_EXC_TYPE(ModuleNotFoundError)
#define PyExc_IndexError _Py_EXC_TYPE(IndexError)
#define PyExc_KeyError _Py_EXC_TYPE(KeyError)
#define PyExc_KeyboardInterrupt _Py_EXC_TYPE(KeyboardInterrupt)
#define PyExc_MemoryError _Py_EXC_TYPE(MemoryError)
#define PyExc_NameError _Py_EXC_TYPE(NameError)
#define PyExc_OverflowError _Py_EXC_TYPE(OverflowError)
#define PyExc_RuntimeError _Py_EXC_TYPE(RuntimeError)
#define PyExc_RecursionError _Py_EXC_TYPE(RecursionError)
#define PyExc_NotImplementedError _Py_EXC_TYPE(NotImplementedError)
#define PyExc_SyntaxError _Py_EXC_TYPE(SyntaxError)
#define PyExc_IndentationError _Py_EXC_TYPE(IndentationError)
#define PyExc_TabError _Py_EXC_TYPE(TabError)
#define PyExc_ReferenceError _Py_EXC_TYPE(ReferenceError)
#define PyExc_SystemError _Py_EXC_TYPE(SystemError)
#define PyExc_SystemExit _Py_EXC_TYPE(SystemExit)
#define PyExc_TypeError _Py_EXC_TYPE(TypeError)
#define PyExc_UnboundLocalError _Py_EXC_TYPE(UnboundLocalError)
#define PyExc_UnicodeError _Py_EXC_TYPE(UnicodeError)
#define PyExc_UnicodeEncodeError _Py_EXC_TYPE(UnicodeEncodeError)
#define PyExc_UnicodeDecodeError _Py_EXC_TYPE(UnicodeDecodeError)
#define PyExc_UnicodeTranslateError _Py_EXC_TYPE(UnicodeTranslateError)
#define PyExc_ValueError _Py_EXC_TYPE(ValueError)
#define PyExc_ZeroDivisionError _Py_EXC_TYPE(ZeroDivisionError)
#define PyExc_BlockingIOError _Py_EXC_TYPE(BlockingIOError)
#define PyExc_BrokenPipeError _Py_EXC_TYPE(BrokenPipeError)
#define PyExc_ChildProcessError _Py_EXC_TYPE(ChildProcessError)
#define PyExc_ConnectionError _Py_EXC_TYPE(ConnectionError)
#define PyExc_ConnectionAbortedError _Py_EXC_TYPE(ConnectionAbortedError)
#define PyExc_ConnectionRefusedError _Py_EXC_TYPE(ConnectionRefusedError)
#define PyExc_ConnectionResetError _Py_EXC_TYPE(ConnectionResetError)
#define PyExc_FileExistsError _Py_EXC_TYPE(FileExistsError)
#define PyExc_FileNotFoundError _Py_EXC_TYPE(FileNotFoundError)
#define PyExc_InterruptedError _Py_EXC_TYPE(InterruptedError)
#define PyExc_IsADirectoryError _Py_EXC_TYPE(IsADirectoryError)
#define PyExc_NotADirectoryError _Py_EXC_TYPE(NotADirectoryError)
#define PyExc_PermissionError _Py_EXC_TYPE(PermissionError)
#define PyExc_ProcessLookupError _Py_EXC_TYPE(ProcessLookupError)
#define PyExc_TimeoutError _Py_EXC_TYPE(TimeoutError)

/* compatibility aliases */

#define PyExc_EnvironmentError _Py_EXC_TYPE(OSError)
#define PyExc_IOError _Py_EXC_TYPE(OSError)
#ifdef MS_WINDOWS
#define PyExc_WindowsError _Py_EXC_TYPE(OSError)
#endif

/* warning category types */

// getters
GETTER(Warning);
GETTER(UserWarning);
GETTER(EncodingWarning);
GETTER(DeprecationWarning);
GETTER(PendingDeprecationWarning);
GETTER(SyntaxWarning);
GETTER(RuntimeWarning);
GETTER(FutureWarning);
GETTER(ImportWarning);
GETTER(UnicodeWarning);
GETTER(BytesWarning);
GETTER(ResourceWarning);

// compatibility shims
#define PyExc_Warning _Py_EXC_TYPE(Warning)
#define PyExc_UserWarning _Py_EXC_TYPE(UserWarning)
#define PyExc_DeprecationWarning _Py_EXC_TYPE(DeprecationWarning)
#define PyExc_PendingDeprecationWarning _Py_EXC_TYPE(PendingDeprecationWarning)
#define PyExc_SyntaxWarning _Py_EXC_TYPE(SyntaxWarning)
#define PyExc_RuntimeWarning _Py_EXC_TYPE(RuntimeWarning)
#define PyExc_FutureWarning _Py_EXC_TYPE(FutureWarning)
#define PyExc_ImportWarning _Py_EXC_TYPE(ImportWarning)
#define PyExc_UnicodeWarning _Py_EXC_TYPE(UnicodeWarning)
#define PyExc_BytesWarning _Py_EXC_TYPE(BytesWarning)
#define PyExc_EncodingWarning _Py_EXC_TYPE(EncodingWarning)
#define PyExc_ResourceWarning _Py_EXC_TYPE(ResourceWarning)

#undef GETTER
