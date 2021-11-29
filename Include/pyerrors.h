#ifndef Py_ERRORS_H
#define Py_ERRORS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>               // va_list

/* Error handling definitions */

PyAPI_FUNC(void) PyErr_SetNone(PyObject *);
PyAPI_FUNC(void) PyErr_SetObject(PyObject *, PyObject *);
PyAPI_FUNC(void) PyErr_SetString(
    PyObject *exception,
    const char *string   /* decoded from utf-8 */
    );
PyAPI_FUNC(PyObject *) PyErr_Occurred(void);
PyAPI_FUNC(void) PyErr_Clear(void);
PyAPI_FUNC(void) PyErr_Fetch(PyObject **, PyObject **, PyObject **);
PyAPI_FUNC(void) PyErr_Restore(PyObject *, PyObject *, PyObject *);
#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03030000
PyAPI_FUNC(void) PyErr_GetExcInfo(PyObject **, PyObject **, PyObject **);
PyAPI_FUNC(void) PyErr_SetExcInfo(PyObject *, PyObject *, PyObject *);
#endif

/* Defined in Python/pylifecycle.c

   The Py_FatalError() function is replaced with a macro which logs
   automatically the name of the current function, unless the Py_LIMITED_API
   macro is defined. */
PyAPI_FUNC(void) _Py_NO_RETURN Py_FatalError(const char *message);

/* Error testing and normalization */
PyAPI_FUNC(int) PyErr_GivenExceptionMatches(PyObject *, PyObject *);
PyAPI_FUNC(int) PyErr_ExceptionMatches(PyObject *);
PyAPI_FUNC(void) PyErr_NormalizeException(PyObject**, PyObject**, PyObject**);

/* Traceback manipulation (PEP 3134) */
PyAPI_FUNC(int) PyException_SetTraceback(PyObject *, PyObject *);
PyAPI_FUNC(PyObject *) PyException_GetTraceback(PyObject *);

/* Cause manipulation (PEP 3134) */
PyAPI_FUNC(PyObject *) PyException_GetCause(PyObject *);
PyAPI_FUNC(void) PyException_SetCause(PyObject *, PyObject *);

/* Context manipulation (PEP 3134) */
PyAPI_FUNC(PyObject *) PyException_GetContext(PyObject *);
PyAPI_FUNC(void) PyException_SetContext(PyObject *, PyObject *);

/* */

#define PyExceptionClass_Check(x)                                       \
    (PyType_Check((x)) &&                                               \
     PyType_FastSubclass((PyTypeObject*)(x), Py_TPFLAGS_BASE_EXC_SUBCLASS))

#define PyExceptionInstance_Check(x)                    \
    PyType_FastSubclass(Py_TYPE(x), Py_TPFLAGS_BASE_EXC_SUBCLASS)

PyAPI_FUNC(const char *) PyExceptionClass_Name(PyObject *);

#define PyExceptionInstance_Class(x) ((PyObject*)Py_TYPE(x))

#define _PyBaseExceptionGroup_Check(x)                   \
    PyObject_TypeCheck(x, (PyTypeObject *)PyExc_BaseExceptionGroup)

/* Predefined exceptions */

#if Py_LIMITED_API+0 < 0x03110000

PyAPI_DATA(PyObject *) PyExc_BaseException;
PyAPI_DATA(PyObject *) PyExc_Exception;
PyAPI_DATA(PyObject *) PyExc_BaseExceptionGroup;
#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03050000
PyAPI_DATA(PyObject *) PyExc_StopAsyncIteration;
#endif
PyAPI_DATA(PyObject *) PyExc_StopIteration;
PyAPI_DATA(PyObject *) PyExc_GeneratorExit;
PyAPI_DATA(PyObject *) PyExc_ArithmeticError;
PyAPI_DATA(PyObject *) PyExc_LookupError;

PyAPI_DATA(PyObject *) PyExc_AssertionError;
PyAPI_DATA(PyObject *) PyExc_AttributeError;
PyAPI_DATA(PyObject *) PyExc_BufferError;
PyAPI_DATA(PyObject *) PyExc_EOFError;
PyAPI_DATA(PyObject *) PyExc_FloatingPointError;
PyAPI_DATA(PyObject *) PyExc_OSError;
PyAPI_DATA(PyObject *) PyExc_ImportError;
#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03060000
PyAPI_DATA(PyObject *) PyExc_ModuleNotFoundError;
#endif
PyAPI_DATA(PyObject *) PyExc_IndexError;
PyAPI_DATA(PyObject *) PyExc_KeyError;
PyAPI_DATA(PyObject *) PyExc_KeyboardInterrupt;
PyAPI_DATA(PyObject *) PyExc_MemoryError;
PyAPI_DATA(PyObject *) PyExc_NameError;
PyAPI_DATA(PyObject *) PyExc_OverflowError;
PyAPI_DATA(PyObject *) PyExc_RuntimeError;
#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03050000
PyAPI_DATA(PyObject *) PyExc_RecursionError;
#endif
PyAPI_DATA(PyObject *) PyExc_NotImplementedError;
PyAPI_DATA(PyObject *) PyExc_SyntaxError;
PyAPI_DATA(PyObject *) PyExc_IndentationError;
PyAPI_DATA(PyObject *) PyExc_TabError;
PyAPI_DATA(PyObject *) PyExc_ReferenceError;
PyAPI_DATA(PyObject *) PyExc_SystemError;
PyAPI_DATA(PyObject *) PyExc_SystemExit;
PyAPI_DATA(PyObject *) PyExc_TypeError;
PyAPI_DATA(PyObject *) PyExc_UnboundLocalError;
PyAPI_DATA(PyObject *) PyExc_UnicodeError;
PyAPI_DATA(PyObject *) PyExc_UnicodeEncodeError;
PyAPI_DATA(PyObject *) PyExc_UnicodeDecodeError;
PyAPI_DATA(PyObject *) PyExc_UnicodeTranslateError;
PyAPI_DATA(PyObject *) PyExc_ValueError;
PyAPI_DATA(PyObject *) PyExc_ZeroDivisionError;

#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03030000
PyAPI_DATA(PyObject *) PyExc_BlockingIOError;
PyAPI_DATA(PyObject *) PyExc_BrokenPipeError;
PyAPI_DATA(PyObject *) PyExc_ChildProcessError;
PyAPI_DATA(PyObject *) PyExc_ConnectionError;
PyAPI_DATA(PyObject *) PyExc_ConnectionAbortedError;
PyAPI_DATA(PyObject *) PyExc_ConnectionRefusedError;
PyAPI_DATA(PyObject *) PyExc_ConnectionResetError;
PyAPI_DATA(PyObject *) PyExc_FileExistsError;
PyAPI_DATA(PyObject *) PyExc_FileNotFoundError;
PyAPI_DATA(PyObject *) PyExc_InterruptedError;
PyAPI_DATA(PyObject *) PyExc_IsADirectoryError;
PyAPI_DATA(PyObject *) PyExc_NotADirectoryError;
PyAPI_DATA(PyObject *) PyExc_PermissionError;
PyAPI_DATA(PyObject *) PyExc_ProcessLookupError;
PyAPI_DATA(PyObject *) PyExc_TimeoutError;
#endif


/* Compatibility aliases */
PyAPI_DATA(PyObject *) PyExc_EnvironmentError;
PyAPI_DATA(PyObject *) PyExc_IOError;
#ifdef MS_WINDOWS
PyAPI_DATA(PyObject *) PyExc_WindowsError;
#endif

/* Predefined warning categories */
PyAPI_DATA(PyObject *) PyExc_Warning;
PyAPI_DATA(PyObject *) PyExc_UserWarning;
PyAPI_DATA(PyObject *) PyExc_DeprecationWarning;
PyAPI_DATA(PyObject *) PyExc_PendingDeprecationWarning;
PyAPI_DATA(PyObject *) PyExc_SyntaxWarning;
PyAPI_DATA(PyObject *) PyExc_RuntimeWarning;
PyAPI_DATA(PyObject *) PyExc_FutureWarning;
PyAPI_DATA(PyObject *) PyExc_ImportWarning;
PyAPI_DATA(PyObject *) PyExc_UnicodeWarning;
PyAPI_DATA(PyObject *) PyExc_BytesWarning;
PyAPI_DATA(PyObject *) PyExc_EncodingWarning;
PyAPI_DATA(PyObject *) PyExc_ResourceWarning;

#else  // Py_LIMITED_API

#define DECLARE_GETTER(NAME) \
    PyAPI_FUNC(PyObject *) PyInterpreterState_GetObject_ ## NAME( \
        PyInterpreterState *interp)

DECLARE_GETTER(ArithmeticError);
DECLARE_GETTER(AssertionError);
DECLARE_GETTER(AttributeError);
DECLARE_GETTER(BaseException);
DECLARE_GETTER(BaseExceptionGroup);
DECLARE_GETTER(BlockingIOError);
DECLARE_GETTER(BrokenPipeError);
DECLARE_GETTER(BufferError);
DECLARE_GETTER(ChildProcessError);
DECLARE_GETTER(ConnectionAbortedError);
DECLARE_GETTER(ConnectionError);
DECLARE_GETTER(ConnectionRefusedError);
DECLARE_GETTER(ConnectionResetError);
DECLARE_GETTER(EOFError);
DECLARE_GETTER(Exception);
DECLARE_GETTER(FileExistsError);
DECLARE_GETTER(FileNotFoundError);
DECLARE_GETTER(FloatingPointError);
DECLARE_GETTER(GeneratorExit);
DECLARE_GETTER(ImportError);
DECLARE_GETTER(IndentationError);
DECLARE_GETTER(IndexError);
DECLARE_GETTER(InterruptedError);
DECLARE_GETTER(IsADirectoryError);
DECLARE_GETTER(KeyError);
DECLARE_GETTER(KeyboardInterrupt);
DECLARE_GETTER(LookupError);
DECLARE_GETTER(MemoryError);
DECLARE_GETTER(ModuleNotFoundError);
DECLARE_GETTER(NameError);
DECLARE_GETTER(NotADirectoryError);
DECLARE_GETTER(NotImplementedError);
DECLARE_GETTER(OSError);
DECLARE_GETTER(OverflowError);
DECLARE_GETTER(PermissionError);
DECLARE_GETTER(ProcessLookupError);
DECLARE_GETTER(RecursionError);
DECLARE_GETTER(ReferenceError);
DECLARE_GETTER(RuntimeError);
DECLARE_GETTER(StopAsyncIteration);
DECLARE_GETTER(StopIteration);
DECLARE_GETTER(SyntaxError);
DECLARE_GETTER(SystemError);
DECLARE_GETTER(SystemExit);
DECLARE_GETTER(TabError);
DECLARE_GETTER(TimeoutError);
DECLARE_GETTER(TypeError);
DECLARE_GETTER(UnboundLocalError);
DECLARE_GETTER(UnicodeDecodeError);
DECLARE_GETTER(UnicodeEncodeError);
DECLARE_GETTER(UnicodeError);
DECLARE_GETTER(UnicodeTranslateError);
DECLARE_GETTER(ValueError);
DECLARE_GETTER(ZeroDivisionError);

#define PyExc_BaseException \
    (PyInterpreterState_GetObject_BaseException(_PyInterpreterState_GET())
#define PyExc_Exception \
    (PyInterpreterState_GetObject_Exception(_PyInterpreterState_GET())
#define PyExc_BaseExceptionGroup \
    (PyInterpreterState_GetObject_BaseExceptionGroup(_PyInterpreterState_GET())
#define PyExc_StopAsyncIteration \
    (PyInterpreterState_GetObject_StopAsyncIteration(_PyInterpreterState_GET())
#define PyExc_StopIteration \
    (PyInterpreterState_GetObject_StopIteration(_PyInterpreterState_GET())
#define PyExc_GeneratorExit \
    (PyInterpreterState_GetObject_GeneratorExit(_PyInterpreterState_GET())
#define PyExc_ArithmeticError \
    (PyInterpreterState_GetObject_ArithmeticError(_PyInterpreterState_GET())
#define PyExc_LookupError \
    (PyInterpreterState_GetObject_LookupError(_PyInterpreterState_GET())

#define PyExc_AssertionError \
    (PyInterpreterState_GetObject_AssertionError(_PyInterpreterState_GET())
#define PyExc_AttributeError \
    (PyInterpreterState_GetObject_AttributeError(_PyInterpreterState_GET())
#define PyExc_BufferError \
    (PyInterpreterState_GetObject_BufferError(_PyInterpreterState_GET())
#define PyExc_EOFError \
    (PyInterpreterState_GetObject_EOFError(_PyInterpreterState_GET())
#define PyExc_FloatingPointError \
    (PyInterpreterState_GetObject_FloatingPointError(_PyInterpreterState_GET())
#define PyExc_OSError \
    (PyInterpreterState_GetObject_OSError(_PyInterpreterState_GET())
#define PyExc_ImportError \
    (PyInterpreterState_GetObject_ImportError(_PyInterpreterState_GET())
#define PyExc_ModuleNotFoundError \
    (PyInterpreterState_GetObject_ModuleNotFoundError(_PyInterpreterState_GET())
#define PyExc_IndexError \
    (PyInterpreterState_GetObject_IndexError(_PyInterpreterState_GET())
#define PyExc_KeyError \
    (PyInterpreterState_GetObject_KeyError(_PyInterpreterState_GET())
#define PyExc_KeyboardInterrupt \
    (PyInterpreterState_GetObject_KeyboardInterrupt(_PyInterpreterState_GET())
#define PyExc_MemoryError \
    (PyInterpreterState_GetObject_MemoryError(_PyInterpreterState_GET())
#define PyExc_NameError \
    (PyInterpreterState_GetObject_NameError(_PyInterpreterState_GET())
#define PyExc_OverflowError \
    (PyInterpreterState_GetObject_OverflowError(_PyInterpreterState_GET())
#define PyExc_RuntimeError \
    (PyInterpreterState_GetObject_RuntimeError(_PyInterpreterState_GET())
#define PyExc_RecursionError \
    (PyInterpreterState_GetObject_RecursionError(_PyInterpreterState_GET())
#define PyExc_NotImplementedError \
    (PyInterpreterState_GetObject_NotImplementedError(_PyInterpreterState_GET())
#define PyExc_SyntaxError \
    (PyInterpreterState_GetObject_SyntaxError(_PyInterpreterState_GET())
#define PyExc_IndentationError \
    (PyInterpreterState_GetObject_IndentationError(_PyInterpreterState_GET())
#define PyExc_TabError \
    (PyInterpreterState_GetObject_TabError(_PyInterpreterState_GET())
#define PyExc_ReferenceError \
    (PyInterpreterState_GetObject_ReferenceError(_PyInterpreterState_GET())
#define PyExc_SystemError \
    (PyInterpreterState_GetObject_SystemError(_PyInterpreterState_GET())
#define PyExc_SystemExit \
    (PyInterpreterState_GetObject_SystemExit(_PyInterpreterState_GET())
#define PyExc_TypeError \
    (PyInterpreterState_GetObject_TypeError(_PyInterpreterState_GET())
#define PyExc_UnboundLocalError \
    (PyInterpreterState_GetObject_UnboundLocalError(_PyInterpreterState_GET())
#define PyExc_UnicodeError \
    (PyInterpreterState_GetObject_UnicodeError(_PyInterpreterState_GET())
#define PyExc_UnicodeEncodeError \
    (PyInterpreterState_GetObject_UnicodeEncodeError(_PyInterpreterState_GET())
#define PyExc_UnicodeDecodeError \
    (PyInterpreterState_GetObject_UnicodeDecodeError(_PyInterpreterState_GET())
#define PyExc_UnicodeTranslateError \
    (PyInterpreterState_GetObject_UnicodeTranslateError(_PyInterpreterState_GET())
#define PyExc_ValueError \
    (PyInterpreterState_GetObject_ValueError(_PyInterpreterState_GET())
#define PyExc_ZeroDivisionError \
    (PyInterpreterState_GetObject_ZeroDivisionError(_PyInterpreterState_GET())

#define PyExc_BlockingIOError \
    (PyInterpreterState_GetObject_BlockingIOError(_PyInterpreterState_GET())
#define PyExc_BrokenPipeError \
    (PyInterpreterState_GetObject_BrokenPipeError(_PyInterpreterState_GET())
#define PyExc_ChildProcessError \
    (PyInterpreterState_GetObject_ChildProcessError(_PyInterpreterState_GET())
#define PyExc_ConnectionError \
    (PyInterpreterState_GetObject_ConnectionError(_PyInterpreterState_GET())
#define PyExc_ConnectionAbortedError \
    (PyInterpreterState_GetObject_ConnectionAbortedError(_PyInterpreterState_GET())
#define PyExc_ConnectionRefusedError \
    (PyInterpreterState_GetObject_ConnectionRefusedError(_PyInterpreterState_GET())
#define PyExc_ConnectionResetError \
    (PyInterpreterState_GetObject_ConnectionResetError(_PyInterpreterState_GET())
#define PyExc_FileExistsError \
    (PyInterpreterState_GetObject_FileExistsError(_PyInterpreterState_GET())
#define PyExc_FileNotFoundError \
    (PyInterpreterState_GetObject_FileNotFoundError(_PyInterpreterState_GET())
#define PyExc_InterruptedError \
    (PyInterpreterState_GetObject_InterruptedError(_PyInterpreterState_GET())
#define PyExc_IsADirectoryError \
    (PyInterpreterState_GetObject_IsADirectoryError(_PyInterpreterState_GET())
#define PyExc_NotADirectoryError \
    (PyInterpreterState_GetObject_NotADirectoryError(_PyInterpreterState_GET())
#define PyExc_PermissionError \
    (PyInterpreterState_GetObject_PermissionError(_PyInterpreterState_GET())
#define PyExc_ProcessLookupError \
    (PyInterpreterState_GetObject_ProcessLookupError(_PyInterpreterState_GET())
#define PyExc_TimeoutError \
    (PyInterpreterState_GetObject_TimeoutError(_PyInterpreterState_GET())

/* Compatibility aliases */
#define PyExc_EnvironmentError \
    (PyInterpreterState_GetObject_OSError(_PyInterpreterState_GET())
#define PyExc_IOError \
    (PyInterpreterState_GetObject_OSError(_PyInterpreterState_GET())
#ifdef MS_WINDOWS
#define PyExc_WindowsError \
    (PyInterpreterState_GetObject_OSError(_PyInterpreterState_GET())
#endif

/* Predefined warning categories */

DECLARE_GETTER(BytesWarning);
DECLARE_GETTER(DeprecationWarning);
DECLARE_GETTER(EncodingWarning);
DECLARE_GETTER(FutureWarning);
DECLARE_GETTER(ImportWarning);
DECLARE_GETTER(PendingDeprecationWarning);
DECLARE_GETTER(ResourceWarning);
DECLARE_GETTER(RuntimeWarning);
DECLARE_GETTER(SyntaxWarning);
DECLARE_GETTER(UnicodeWarning);
DECLARE_GETTER(UserWarning);
DECLARE_GETTER(Warning);

#define PyExc_Warning \
    (PyInterpreterState_GetObject_Warning(_PyInterpreterState_GET())
#define PyExc_UserWarning \
    (PyInterpreterState_GetObject_UserWarning(_PyInterpreterState_GET())
#define PyExc_DeprecationWarning \
    (PyInterpreterState_GetObject_DeprecationWarning(_PyInterpreterState_GET())
#define PyExc_PendingDeprecationWarning \
    (PyInterpreterState_GetObject_PendingDeprecationWarning(_PyInterpreterState_GET())
#define PyExc_SyntaxWarning \
    (PyInterpreterState_GetObject_SyntaxWarning(_PyInterpreterState_GET())
#define PyExc_RuntimeWarning \
    (PyInterpreterState_GetObject_RuntimeWarning(_PyInterpreterState_GET())
#define PyExc_FutureWarning \
    (PyInterpreterState_GetObject_FutureWarning(_PyInterpreterState_GET())
#define PyExc_ImportWarning \
    (PyInterpreterState_GetObject_ImportWarning(_PyInterpreterState_GET())
#define PyExc_UnicodeWarning \
    (PyInterpreterState_GetObject_UnicodeWarning(_PyInterpreterState_GET())
#define PyExc_BytesWarning \
    (PyInterpreterState_GetObject_BytesWarning(_PyInterpreterState_GET())
#define PyExc_EncodingWarning \
    (PyInterpreterState_GetObject_EncodingWarning(_PyInterpreterState_GET())
#define PyExc_ResourceWarning \
    (PyInterpreterState_GetObject_ResourceWarning(_PyInterpreterState_GET())

#undef DECLARE_GETTER

#endif // Py_LIMITED_API


/* Convenience functions */

PyAPI_FUNC(int) PyErr_BadArgument(void);
PyAPI_FUNC(PyObject *) PyErr_NoMemory(void);
PyAPI_FUNC(PyObject *) PyErr_SetFromErrno(PyObject *);
PyAPI_FUNC(PyObject *) PyErr_SetFromErrnoWithFilenameObject(
    PyObject *, PyObject *);
#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03040000
PyAPI_FUNC(PyObject *) PyErr_SetFromErrnoWithFilenameObjects(
    PyObject *, PyObject *, PyObject *);
#endif
PyAPI_FUNC(PyObject *) PyErr_SetFromErrnoWithFilename(
    PyObject *exc,
    const char *filename   /* decoded from the filesystem encoding */
    );

PyAPI_FUNC(PyObject *) PyErr_Format(
    PyObject *exception,
    const char *format,   /* ASCII-encoded string  */
    ...
    );
#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03050000
PyAPI_FUNC(PyObject *) PyErr_FormatV(
    PyObject *exception,
    const char *format,
    va_list vargs);
#endif

#ifdef MS_WINDOWS
PyAPI_FUNC(PyObject *) PyErr_SetFromWindowsErrWithFilename(
    int ierr,
    const char *filename        /* decoded from the filesystem encoding */
    );
PyAPI_FUNC(PyObject *) PyErr_SetFromWindowsErr(int);
PyAPI_FUNC(PyObject *) PyErr_SetExcFromWindowsErrWithFilenameObject(
    PyObject *,int, PyObject *);
#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03040000
PyAPI_FUNC(PyObject *) PyErr_SetExcFromWindowsErrWithFilenameObjects(
    PyObject *,int, PyObject *, PyObject *);
#endif
PyAPI_FUNC(PyObject *) PyErr_SetExcFromWindowsErrWithFilename(
    PyObject *exc,
    int ierr,
    const char *filename        /* decoded from the filesystem encoding */
    );
PyAPI_FUNC(PyObject *) PyErr_SetExcFromWindowsErr(PyObject *, int);
#endif /* MS_WINDOWS */

#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03060000
PyAPI_FUNC(PyObject *) PyErr_SetImportErrorSubclass(PyObject *, PyObject *,
    PyObject *, PyObject *);
#endif
#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03030000
PyAPI_FUNC(PyObject *) PyErr_SetImportError(PyObject *, PyObject *,
    PyObject *);
#endif

/* Export the old function so that the existing API remains available: */
PyAPI_FUNC(void) PyErr_BadInternalCall(void);
PyAPI_FUNC(void) _PyErr_BadInternalCall(const char *filename, int lineno);
/* Mask the old API with a call to the new API for code compiled under
   Python 2.0: */
#define PyErr_BadInternalCall() _PyErr_BadInternalCall(__FILE__, __LINE__)

/* Function to create a new exception */
PyAPI_FUNC(PyObject *) PyErr_NewException(
    const char *name, PyObject *base, PyObject *dict);
PyAPI_FUNC(PyObject *) PyErr_NewExceptionWithDoc(
    const char *name, const char *doc, PyObject *base, PyObject *dict);
PyAPI_FUNC(void) PyErr_WriteUnraisable(PyObject *);


/* In signalmodule.c */
PyAPI_FUNC(int) PyErr_CheckSignals(void);
PyAPI_FUNC(void) PyErr_SetInterrupt(void);
#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x030A0000
PyAPI_FUNC(int) PyErr_SetInterruptEx(int signum);
#endif

/* Support for adding program text to SyntaxErrors */
PyAPI_FUNC(void) PyErr_SyntaxLocation(
    const char *filename,       /* decoded from the filesystem encoding */
    int lineno);
PyAPI_FUNC(void) PyErr_SyntaxLocationEx(
    const char *filename,       /* decoded from the filesystem encoding */
    int lineno,
    int col_offset);
PyAPI_FUNC(PyObject *) PyErr_ProgramText(
    const char *filename,       /* decoded from the filesystem encoding */
    int lineno);

/* The following functions are used to create and modify unicode
   exceptions from C */

/* create a UnicodeDecodeError object */
PyAPI_FUNC(PyObject *) PyUnicodeDecodeError_Create(
    const char *encoding,       /* UTF-8 encoded string */
    const char *object,
    Py_ssize_t length,
    Py_ssize_t start,
    Py_ssize_t end,
    const char *reason          /* UTF-8 encoded string */
    );

/* get the encoding attribute */
PyAPI_FUNC(PyObject *) PyUnicodeEncodeError_GetEncoding(PyObject *);
PyAPI_FUNC(PyObject *) PyUnicodeDecodeError_GetEncoding(PyObject *);

/* get the object attribute */
PyAPI_FUNC(PyObject *) PyUnicodeEncodeError_GetObject(PyObject *);
PyAPI_FUNC(PyObject *) PyUnicodeDecodeError_GetObject(PyObject *);
PyAPI_FUNC(PyObject *) PyUnicodeTranslateError_GetObject(PyObject *);

/* get the value of the start attribute (the int * may not be NULL)
   return 0 on success, -1 on failure */
PyAPI_FUNC(int) PyUnicodeEncodeError_GetStart(PyObject *, Py_ssize_t *);
PyAPI_FUNC(int) PyUnicodeDecodeError_GetStart(PyObject *, Py_ssize_t *);
PyAPI_FUNC(int) PyUnicodeTranslateError_GetStart(PyObject *, Py_ssize_t *);

/* assign a new value to the start attribute
   return 0 on success, -1 on failure */
PyAPI_FUNC(int) PyUnicodeEncodeError_SetStart(PyObject *, Py_ssize_t);
PyAPI_FUNC(int) PyUnicodeDecodeError_SetStart(PyObject *, Py_ssize_t);
PyAPI_FUNC(int) PyUnicodeTranslateError_SetStart(PyObject *, Py_ssize_t);

/* get the value of the end attribute (the int *may not be NULL)
 return 0 on success, -1 on failure */
PyAPI_FUNC(int) PyUnicodeEncodeError_GetEnd(PyObject *, Py_ssize_t *);
PyAPI_FUNC(int) PyUnicodeDecodeError_GetEnd(PyObject *, Py_ssize_t *);
PyAPI_FUNC(int) PyUnicodeTranslateError_GetEnd(PyObject *, Py_ssize_t *);

/* assign a new value to the end attribute
   return 0 on success, -1 on failure */
PyAPI_FUNC(int) PyUnicodeEncodeError_SetEnd(PyObject *, Py_ssize_t);
PyAPI_FUNC(int) PyUnicodeDecodeError_SetEnd(PyObject *, Py_ssize_t);
PyAPI_FUNC(int) PyUnicodeTranslateError_SetEnd(PyObject *, Py_ssize_t);

/* get the value of the reason attribute */
PyAPI_FUNC(PyObject *) PyUnicodeEncodeError_GetReason(PyObject *);
PyAPI_FUNC(PyObject *) PyUnicodeDecodeError_GetReason(PyObject *);
PyAPI_FUNC(PyObject *) PyUnicodeTranslateError_GetReason(PyObject *);

/* assign a new value to the reason attribute
   return 0 on success, -1 on failure */
PyAPI_FUNC(int) PyUnicodeEncodeError_SetReason(
    PyObject *exc,
    const char *reason          /* UTF-8 encoded string */
    );
PyAPI_FUNC(int) PyUnicodeDecodeError_SetReason(
    PyObject *exc,
    const char *reason          /* UTF-8 encoded string */
    );
PyAPI_FUNC(int) PyUnicodeTranslateError_SetReason(
    PyObject *exc,
    const char *reason          /* UTF-8 encoded string */
    );

PyAPI_FUNC(int) PyOS_snprintf(char *str, size_t size, const char  *format, ...)
                        Py_GCC_ATTRIBUTE((format(printf, 3, 4)));
PyAPI_FUNC(int) PyOS_vsnprintf(char *str, size_t size, const char  *format, va_list va)
                        Py_GCC_ATTRIBUTE((format(printf, 3, 0)));

#ifndef Py_LIMITED_API
#  define Py_CPYTHON_ERRORS_H
#  include "cpython/pyerrors.h"
#  undef Py_CPYTHON_ERRORS_H
#endif

#ifdef __cplusplus
}
#endif
#endif /* !Py_ERRORS_H */
