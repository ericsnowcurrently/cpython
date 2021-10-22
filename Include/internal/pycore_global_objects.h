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
    PyObject *_PyFilter_Type;
    PyObject *_PyMap_Type;
    PyObject *_PyZip_Type;
    // Include/boolobject.h
    PyObject *_PyBool_Type;
    // Include/bytearrayobject.h
    PyObject *_PyByteArray_Type;
    PyObject *_PyByteArrayIter_Type;
    // Include/bytesobject.h
    PyObject *_PyBytes_Type;
    PyObject *_PyBytesIter_Type;
    // Include/complexobject.h
    PyObject *_PyComplex_Type;
    // Include/descrobject.h
    PyObject *_PyClassMethodDescr_Type;
    PyObject *_PyGetSetDescr_Type;
    PyObject *_PyMemberDescr_Type;
    PyObject *_PyMethodDescr_Type;
    PyObject *_PyWrapperDescr_Type;
    PyObject *_PyDictProxy_Type;
    PyObject *_PyProperty_Type;
    // Include/dictobject.h
    PyObject *_PyDict_Type;
    PyObject *_PyDictKeys_Type;
    PyObject *_PyDictValues_Type;
    PyObject *_PyDictItems_Type;
    PyObject *_PyDictIterKey_Type;
    PyObject *_PyDictIterValue_Type;
    PyObject *_PyDictIterItem_Type;
    PyObject *_PyDictRevIterKey_Type;
    PyObject *_PyDictRevIterItem_Type;
    PyObject *_PyDictRevIterValue_Type;
    // Include/enumobject.h
    PyObject *_PyEnum_Type;
    PyObject *_PyReversed_Type;
    // Include/floatobject.h
    PyObject *_PyFloat_Type;
    // Include/genericaliasobject.h
    PyObject *_Py_GenericAliasType;
    // Include/iterobject.h
    PyObject *_PySeqIter_Type;
    PyObject *_PyCallIter_Type;
    // Include/listobject.h
    PyObject *_PyList_Type;
    PyObject *_PyListIter_Type;
    PyObject *_PyListRevIter_Type;
    // Include/longobject.h
    PyObject *_PyLong_Type;
    // Include/memoryobject.h
    PyObject *_PyMemoryView_Type;
    // Include/methodobject.h
    PyObject *_PyCFunction_Type;
    // Include/moduleobject.h
    PyObject *_PyModule_Type;
    PyObject *_PyModuleDef_Type;
    // Include/object.h
    PyObject *_PyType_Type;
    PyObject *_PyBaseObject_Type;
    PyObject *_PySuper_Type;
    // Include/pycapsule.h
    PyObject *_PyCapsule_Type;
    // Include/rangeobject.h
    PyObject *_PyRange_Type;
    PyObject *_PyRangeIter_Type;
    PyObject *_PyLongRangeIter_Type;
    // Include/setobject.h
    PyObject *_PySet_Type;
    PyObject *_PyFrozenSet_Type;
    PyObject *_PySetIter_Type;
    // Include/sliceobject.h
    PyObject *_PySlice_Type;
    PyObject *_PyEllipsis_Type;
    // Include/traceback.h
    PyObject *_PyTraceBack_Type;
    // Include/tupleobject.h
    PyObject *_PyTuple_Type;
    PyObject *_PyTupleIter_Type;
    // Include/unicodeobject.h
    PyObject *_PyUnicode_Type;
    PyObject *_PyUnicodeIter_Type;

    /* types in the public C-API */
    // Include/cpython/cellobject.h
    PyObject *_PyCell_Type;
    // Include/cpython/classobject.h
    PyObject *_PyMethod_Type;
    PyObject *_PyInstanceMethod_Type;
    // Include/cpython/code.h
    PyObject *_PyCode_Type;
    // Include/cpython/context.h
    PyObject *_PyContext_Type;
    PyObject *_PyContextVar_Type;
    PyObject *_PyContextToken_Type;
    // Include/cpython/fileobject.h
    PyObject *_PyStdPrinter_Type;
    // Include/cpython/frameobject.h
    PyObject *_PyFrame_Type;
    // Include/cpython/funcobject.h
    PyObject *_PyFunction_Type;
    PyObject *_PyClassMethod_Type;
    PyObject *_PyStaticMethod_Type;
    // Include/cpython/genobject.h
    PyObject *_PyGen_Type;
    PyObject *_PyCoro_Type;
    PyObject *_PyAsyncGen_Type;
    // Include/cpython/methodobject.h
    PyObject *_PyCMethod_Type;
    // Include/cpython/odictobject.h
    PyObject *_PyODict_Type;
    PyObject *_PyODictIter_Type;
    PyObject *_PyODictKeys_Type;
    PyObject *_PyODictItems_Type;
    PyObject *_PyODictValues_Type;
    // Include/cpython/picklebufobject.h
    PyObject *_PyPickleBuffer_Type;

    /* types in the "private" C-API */
    // Include/descrobject.h
    PyObject *__PyMethodWrapper_Type;
    // Include/memoryobject.h
    PyObject *__PyManagedBuffer_Type;
    // Include/weakrefobject.h
    PyObject *__PyWeakref_RefType;
    PyObject *__PyWeakref_ProxyType;
    PyObject *__PyWeakref_CallableProxyType;
    // Include/cpython/genobject.h
    PyObject *__PyCoroWrapper_Type;
    PyObject *__PyAsyncGenASend_Type;
    PyObject *__PyAsyncGenWrappedValue_Type;
    PyObject *__PyAsyncGenAThrow_Type;
    // Include/cpython/object.h
    PyObject *__PyNone_Type;
    PyObject *__PyNotImplemented_Type;

    /* types in the internal C-API */
    // XXX Ignore them?)
    // Include/internal/pycore_hamt.h
    PyObject *__PyHamt_Type;
    PyObject *__PyHamt_ArrayNode_Type;
    PyObject *__PyHamt_BitmapNode_Type;
    PyObject *__PyHamt_CollisionNode_Type;
    PyObject *__PyHamtKeys_Type;
    PyObject *__PyHamtValues_Type;
    PyObject *__PyHamtItems_Type;
    // Include/internal/pycore_interpreteridobject.h
    PyObject *__PyInterpreterID_Type;
    // Include/internal/pycore_namespace.h
    PyObject *__PyNamespace_Type;

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

/* low-level helpers for mapping object names to interpreter state */

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

/* dealing with the C-API */

#define _PyAPI_DEFINE_GLOBAL_GETTER(NAME) \
    PyObject * \
    _PyInterpreterState_GetObject_ ## NAME(PyInterpreterState *interp) \
    { \
        return (interp)->global_objects._ ## NAME; \
    }

/* high-level helpers for managing object lifecycle */

PyAPI_FUNC(PyTypeObject *) _PyTypeObject_CopyRaw(PyTypeObject *ob,
                                                 PyTypeObject *base);

#define _Py_INIT_GLOBAL_TYPE(interp, NAME, BASE, static_p) \
    do { \
        PyTypeObject *base = \
            (PyTypeObject *)_PyInterpreterState_GET_OBJECT(interp, BASE); \
        PyTypeObject *typeobj = _PyTypeObject_CopyRaw(static_p, base); \
        if (typeobj == NULL) { \
            return _PyStatus_ERR("bootstrapping error"); \
        } \
        _PyInterpreterState_SET_OBJECT(interp, NAME, (PyObject *)typeobj); \
    } while (0)


#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_CAPI_OBJECTS_H */
