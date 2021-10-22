#ifndef Py_INTERNAL_CAPI_OBJECTS_H
#define Py_INTERNAL_CAPI_OBJECTS_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

struct _Py_global_objects {
    // All fields have a leading understcore to avoid collisions with macros.

    /* exception types in the public C-API */
    // Include/pyerrors.h
    PyObject *_PyExc_BaseException;
    PyObject *_PyExc_Exception;
    PyObject *_PyExc_StopAsyncIteration;
    PyObject *_PyExc_StopIteration;
    PyObject *_PyExc_GeneratorExit;
    PyObject *_PyExc_ArithmeticError;
    PyObject *_PyExc_LookupError;
    PyObject *_PyExc_AssertionError;
    PyObject *_PyExc_AttributeError;
    PyObject *_PyExc_BufferError;
    PyObject *_PyExc_EOFError;
    PyObject *_PyExc_FloatingPointError;
    PyObject *_PyExc_OSError;
    PyObject *_PyExc_ImportError;
    PyObject *_PyExc_ModuleNotFoundError;
    PyObject *_PyExc_IndexError;
    PyObject *_PyExc_KeyError;
    PyObject *_PyExc_KeyboardInterrupt;
    PyObject *_PyExc_MemoryError;
    PyObject *_PyExc_NameError;
    PyObject *_PyExc_OverflowError;
    PyObject *_PyExc_RuntimeError;
    PyObject *_PyExc_RecursionError;
    PyObject *_PyExc_NotImplementedError;
    PyObject *_PyExc_SyntaxError;
    PyObject *_PyExc_IndentationError;
    PyObject *_PyExc_TabError;
    PyObject *_PyExc_ReferenceError;
    PyObject *_PyExc_SystemError;
    PyObject *_PyExc_SystemExit;
    PyObject *_PyExc_TypeError;
    PyObject *_PyExc_UnboundLocalError;
    PyObject *_PyExc_UnicodeError;
    PyObject *_PyExc_UnicodeEncodeError;
    PyObject *_PyExc_UnicodeDecodeError;
    PyObject *_PyExc_UnicodeTranslateError;
    PyObject *_PyExc_ValueError;
    PyObject *_PyExc_ZeroDivisionError;
    PyObject *_PyExc_BlockingIOError;
    PyObject *_PyExc_BrokenPipeError;
    PyObject *_PyExc_ChildProcessError;
    PyObject *_PyExc_ConnectionError;
    PyObject *_PyExc_ConnectionAbortedError;
    PyObject *_PyExc_ConnectionRefusedError;
    PyObject *_PyExc_ConnectionResetError;
    PyObject *_PyExc_FileExistsError;
    PyObject *_PyExc_FileNotFoundError;
    PyObject *_PyExc_InterruptedError;
    PyObject *_PyExc_IsADirectoryError;
    PyObject *_PyExc_NotADirectoryError;
    PyObject *_PyExc_PermissionError;
    PyObject *_PyExc_ProcessLookupError;
    PyObject *_PyExc_TimeoutError;

    /* warning category types in the public C-API */
    // Include/pyerrors.h
    PyObject *_PyExc_Warning;
    PyObject *_PyExc_UserWarning;
    PyObject *_PyExc_DeprecationWarning;
    PyObject *_PyExc_PendingDeprecationWarning;
    PyObject *_PyExc_SyntaxWarning;
    PyObject *_PyExc_RuntimeWarning;
    PyObject *_PyExc_FutureWarning;
    PyObject *_PyExc_ImportWarning;
    PyObject *_PyExc_UnicodeWarning;
    PyObject *_PyExc_BytesWarning;
    PyObject *_PyExc_EncodingWarning;
    PyObject *_PyExc_ResourceWarning;

    /* types in the limited C-API */
    // Include/bltinmodule.h
    PyTypeObject *_PyFilter_Type;
    PyTypeObject *_PyMap_Type;
    PyTypeObject *_PyZip_Type;
    // Include/boolobject.h
    PyTypeObject *_PyBool_Type;
    // Include/bytearrayobject.h
    PyTypeObject *_PyByteArray_Type;
    PyTypeObject *_PyByteArrayIter_Type;
    // Include/bytesobject.h
    PyTypeObject *_PyBytes_Type;
    PyTypeObject *_PyBytesIter_Type;
    // Include/complexobject.h
    PyTypeObject *_PyComplex_Type;
    // Include/descrobject.h
    PyTypeObject *_PyClassMethodDescr_Type;
    PyTypeObject *_PyGetSetDescr_Type;
    PyTypeObject *_PyMemberDescr_Type;
    PyTypeObject *_PyMethodDescr_Type;
    PyTypeObject *_PyWrapperDescr_Type;
    PyTypeObject *_PyDictProxy_Type;
    PyTypeObject *_PyProperty_Type;
    // Include/dictobject.h
    PyTypeObject *_PyDict_Type;
    PyTypeObject *_PyDictKeys_Type;
    PyTypeObject *_PyDictValues_Type;
    PyTypeObject *_PyDictItems_Type;
    PyTypeObject *_PyDictIterKey_Type;
    PyTypeObject *_PyDictIterValue_Type;
    PyTypeObject *_PyDictIterItem_Type;
    PyTypeObject *_PyDictRevIterKey_Type;
    PyTypeObject *_PyDictRevIterItem_Type;
    PyTypeObject *_PyDictRevIterValue_Type;
    // Include/enumobject.h
    PyTypeObject *_PyEnum_Type;
    PyTypeObject *_PyReversed_Type;
    // Include/floatobject.h
    PyTypeObject *_PyFloat_Type;
    // Include/genericaliasobject.h
    PyTypeObject *_Py_GenericAliasType;
    // Include/iterobject.h
    PyTypeObject *_PySeqIter_Type;
    PyTypeObject *_PyCallIter_Type;
    // Include/listobject.h
    PyTypeObject *_PyList_Type;
    PyTypeObject *_PyListIter_Type;
    PyTypeObject *_PyListRevIter_Type;
    // Include/longobject.h
    PyTypeObject *_PyLong_Type;
    // Include/memoryobject.h
    PyTypeObject *_PyMemoryView_Type;
    // Include/methodobject.h
    PyTypeObject *_PyCFunction_Type;
    // Include/moduleobject.h
    PyTypeObject *_PyModule_Type;
    PyTypeObject *_PyModuleDef_Type;
    // Include/object.h
    PyTypeObject *_PyType_Type;
    PyTypeObject *_PyBaseObject_Type;
    PyTypeObject *_PySuper_Type;
    // Include/pycapsule.h
    PyTypeObject *_PyCapsule_Type;
    // Include/rangeobject.h
    PyTypeObject *_PyRange_Type;
    PyTypeObject *_PyRangeIter_Type;
    PyTypeObject *_PyLongRangeIter_Type;
    // Include/setobject.h
    PyTypeObject *_PySet_Type;
    PyTypeObject *_PyFrozenSet_Type;
    PyTypeObject *_PySetIter_Type;
    // Include/sliceobject.h
    PyTypeObject *_PySlice_Type;
    PyTypeObject *_PyEllipsis_Type;
    // Include/traceback.h
    PyTypeObject *_PyTraceBack_Type;
    // Include/tupleobject.h
    PyTypeObject *_PyTuple_Type;
    PyTypeObject *_PyTupleIter_Type;
    // Include/unicodeobject.h
    PyTypeObject *_PyUnicode_Type;
    PyTypeObject *_PyUnicodeIter_Type;

    /* types in the public C-API */
    // Include/cpython/cellobject.h
    PyTypeObject *_PyCell_Type;
    // Include/cpython/classobject.h
    PyTypeObject *_PyMethod_Type;
    PyTypeObject *_PyInstanceMethod_Type;
    // Include/cpython/code.h
    PyTypeObject *_PyCode_Type;
    // Include/cpython/context.h
    PyTypeObject *_PyContext_Type;
    PyTypeObject *_PyContextVar_Type;
    PyTypeObject *_PyContextToken_Type;
    // Include/cpython/fileobject.h
    PyTypeObject *_PyStdPrinter_Type;
    // Include/cpython/frameobject.h
    PyTypeObject *_PyFrame_Type;
    // Include/cpython/funcobject.h
    PyTypeObject *_PyFunction_Type;
    PyTypeObject *_PyClassMethod_Type;
    PyTypeObject *_PyStaticMethod_Type;
    // Include/cpython/genobject.h
    PyTypeObject *_PyGen_Type;
    PyTypeObject *_PyCoro_Type;
    PyTypeObject *_PyAsyncGen_Type;
    // Include/cpython/methodobject.h
    PyTypeObject *_PyCMethod_Type;
    // Include/cpython/odictobject.h
    PyTypeObject *_PyODict_Type;
    PyTypeObject *_PyODictIter_Type;
    PyTypeObject *_PyODictKeys_Type;
    PyTypeObject *_PyODictItems_Type;
    PyTypeObject *_PyODictValues_Type;
    // Include/cpython/picklebufobject.h
    PyTypeObject *_PyPickleBuffer_Type;

    /* types in the "private" C-API */
    // Include/descrobject.h
    PyTypeObject *__PyMethodWrapper_Type;
    // Include/memoryobject.h
    PyTypeObject *__PyManagedBuffer_Type;
    // Include/weakrefobject.h
    PyTypeObject *__PyWeakref_RefType;
    PyTypeObject *__PyWeakref_ProxyType;
    PyTypeObject *__PyWeakref_CallableProxyType;
    // Include/cpython/genobject.h
    PyTypeObject *__PyCoroWrapper_Type;
    PyTypeObject *__PyAsyncGenASend_Type;
    PyTypeObject *__PyAsyncGenWrappedValue_Type;
    PyTypeObject *__PyAsyncGenAThrow_Type;
    // Include/cpython/object.h
    PyTypeObject *__PyNone_Type;
    PyTypeObject *__PyNotImplemented_Type;

    /* types in the internal C-API */
    // XXX Ignore them?)
    // Include/internal/pycore_hamt.h
    PyTypeObject *__PyHamt_Type;
    PyTypeObject *__PyHamt_ArrayNode_Type;
    PyTypeObject *__PyHamt_BitmapNode_Type;
    PyTypeObject *__PyHamt_CollisionNode_Type;
    PyTypeObject *__PyHamtKeys_Type;
    PyTypeObject *__PyHamtValues_Type;
    PyTypeObject *__PyHamtItems_Type;
    // Include/internal/pycore_interpreteridobject.h
    PyTypeObject *__PyInterpreterID_Type;
    // Include/internal/pycore_namespace.h
    PyTypeObject *__PyNamespace_Type;

    /* singletons in the public C-API */
    // Include/object.h
    //PyObject _Py_NoneStruct;
    //PyObject _Py_NotImplementedStruct;
    PyObject *_Py_None;
    PyObject *_Py_NotImplemented;
    // Include/sliceobject.h
    //PyObject _Py_EllipsisObject;
    PyObject *_Py_Ellipsis;
    // Include/boolobject.h
    //struct _longobject _Py_FalseStruct;
    //struct _longobject _Py_TrueStruct;
    PyObject *_Py_False;
    PyObject *_Py_True;

    /* singletons in the "private" C-API */
    // Include/setobject.h
    //PyObject *_PySet_Dummy;
    PyObject *__PySet_Dummy;

    // XXX What about types not exposed in the C-API?
};

PyAPI_FUNC(PyTypeObject *) _PyTypeObject_CopyRaw(PyTypeObject *ob,
                                                 PyTypeObject *base);

#define _PyInterpreterState_GET_OBJECT(interp, NAME) \
    ((interp)->global_objects._ ## NAME)

#define _PyInterpreterState_SET_OBJECT(interp, NAME, ob) \
    do { \
        (interp)->global_objects._ ## NAME = (ob); \
    } while (0)

#define _PyInterpreterState_CLEAR_OBJECT(interp, NAME) \
    do { \
        Py_XDECREF((interp)->global_objects._ ## NAME); \
        (interp)->global_objects._ ## NAME = NULL; \
    } while (0)

#define _PyAPI_DEFINE_GLOBAL_GETTER(NAME) \
    PyObject * \
    _PyInterpreterState_GetObject_ ## NAME(PyInterpreterState *interp) \
    { \
        return (interp)->global_objects._ ## NAME; \
    }

#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_CAPI_OBJECTS_H */
