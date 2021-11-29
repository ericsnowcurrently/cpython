#ifndef Py_INTERNAL_GLOBAL_OBJECTS_H
#define Py_INTERNAL_GLOBAL_OBJECTS_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

/* objects exposed in the C-API:

(Macro names are parenthesized.)

#################################################
# limited API

#----------------------------
# singletons

# Include/object.h
PyObject        _Py_NoneStruct
PyObject *      (Py_None)
PyObject        _Py_NotImplementedStruct
PyObject *      (Py_NotImplemented)

# Include/sliceobject.h
PyObject        _Py_EllipsisObject
PyObject *      (Py_Ellipsis)

# Include/boolobject.h
PyLongObject    _Py_FalseStruct
PyLongObject    _Py_TrueStruct
PyObject *      (Py_False)
PyObject *      (Py_True)

#----------------------------
# exception types

# Include/pyerrors.h
PyObject *      PyExc_BaseException
PyObject *      PyExc_Exception
PyObject *      PyExc_StopAsyncIteration
PyObject *      PyExc_StopIteration
PyObject *      PyExc_GeneratorExit
PyObject *      PyExc_ArithmeticError
PyObject *      PyExc_LookupError
PyObject *      PyExc_AssertionError
PyObject *      PyExc_AttributeError
PyObject *      PyExc_BufferError
PyObject *      PyExc_EOFError
PyObject *      PyExc_FloatingPointError
PyObject *      PyExc_OSError
PyObject *      PyExc_ImportError
PyObject *      PyExc_ModuleNotFoundError
PyObject *      PyExc_IndexError
PyObject *      PyExc_KeyError
PyObject *      PyExc_KeyboardInterrupt
PyObject *      PyExc_MemoryError
PyObject *      PyExc_NameError
PyObject *      PyExc_OverflowError
PyObject *      PyExc_RuntimeError
PyObject *      PyExc_RecursionError
PyObject *      PyExc_NotImplementedError
PyObject *      PyExc_SyntaxError
PyObject *      PyExc_IndentationError
PyObject *      PyExc_TabError
PyObject *      PyExc_ReferenceError
PyObject *      PyExc_SystemError
PyObject *      PyExc_SystemExit
PyObject *      PyExc_TypeError
PyObject *      PyExc_UnboundLocalError
PyObject *      PyExc_UnicodeError
PyObject *      PyExc_UnicodeEncodeError
PyObject *      PyExc_UnicodeDecodeError
PyObject *      PyExc_UnicodeTranslateError
PyObject *      PyExc_ValueError
PyObject *      PyExc_ZeroDivisionError
PyObject *      PyExc_BlockingIOError
PyObject *      PyExc_BrokenPipeError
PyObject *      PyExc_ChildProcessError
PyObject *      PyExc_ConnectionError
PyObject *      PyExc_ConnectionAbortedError
PyObject *      PyExc_ConnectionRefusedError
PyObject *      PyExc_ConnectionResetError
PyObject *      PyExc_FileExistsError
PyObject *      PyExc_FileNotFoundError
PyObject *      PyExc_InterruptedError
PyObject *      PyExc_IsADirectoryError
PyObject *      PyExc_NotADirectoryError
PyObject *      PyExc_PermissionError
PyObject *      PyExc_ProcessLookupError
PyObject *      PyExc_TimeoutError

#----------------------------
# warning category types

# Include/pyerrors.h
PyObject *      PyExc_Warning
PyObject *      PyExc_UserWarning
PyObject *      PyExc_DeprecationWarning
PyObject *      PyExc_PendingDeprecationWarning
PyObject *      PyExc_SyntaxWarning
PyObject *      PyExc_RuntimeWarning
PyObject *      PyExc_FutureWarning
PyObject *      PyExc_ImportWarning
PyObject *      PyExc_UnicodeWarning
PyObject *      PyExc_BytesWarning
PyObject *      PyExc_EncodingWarning
PyObject *      PyExc_ResourceWarning

#----------------------------
# other types

# Include/bltinmodule.h
PyTypeObject    PyFilter_Type
PyTypeObject    PyMap_Type
PyTypeObject    PyZip_Type

# Include/boolobject.h
PyTypeObject    PyBool_Type

# Include/bytearrayobject.h
PyTypeObject    PyByteArray_Type
PyTypeObject    PyByteArrayIter_Type

# Include/bytesobject.h
PyTypeObject    PyBytes_Type
PyTypeObject    PyBytesIter_Type

# Include/complexobject.h
PyTypeObject    PyComplex_Type

# Include/descrobject.h
PyTypeObject    PyClassMethodDescr_Type
PyTypeObject    PyGetSetDescr_Type
PyTypeObject    PyMemberDescr_Type
PyTypeObject    PyMethodDescr_Type
PyTypeObject    PyWrapperDescr_Type
PyTypeObject    PyDictProxy_Type
PyTypeObject    PyProperty_Type

# Include/dictobject.h
PyTypeObject    PyDict_Type
PyTypeObject    PyDictKeys_Type
PyTypeObject    PyDictValues_Type
PyTypeObject    PyDictItems_Type
PyTypeObject    PyDictIterKey_Type
PyTypeObject    PyDictIterValue_Type
PyTypeObject    PyDictIterItem_Type
PyTypeObject    PyDictRevIterKey_Type
PyTypeObject    PyDictRevIterItem_Type
PyTypeObject    PyDictRevIterValue_Type

# Include/enumobject.h
PyTypeObject    PyEnum_Type
PyTypeObject    PyReversed_Type

# Include/floatobject.h
PyTypeObject    PyFloat_Type

# Include/genericaliasobject.h
PyTypeObject    Py_GenericAliasType

# Include/iterobject.h
PyTypeObject    PySeqIter_Type
PyTypeObject    PyCallIter_Type

# Include/listobject.h
PyTypeObject    PyList_Type
PyTypeObject    PyListIter_Type
PyTypeObject    PyListRevIter_Type

# Include/longobject.h
PyTypeObject    PyLong_Type

# Include/memoryobject.h
PyTypeObject    PyMemoryView_Type

# Include/methodobject.h
PyTypeObject    PyCFunction_Type

# Include/moduleobject.h
PyTypeObject    PyModule_Type
PyTypeObject    PyModuleDef_Type

# Include/object.h
PyTypeObject    PyType_Type
PyTypeObject    PyBaseObject_Type
PyTypeObject    PySuper_Type

# Include/pycapsule.h
PyTypeObject    PyCapsule_Type

# Include/rangeobject.h
PyTypeObject    PyRange_Type
PyTypeObject    PyRangeIter_Type
PyTypeObject    PyLongRangeIter_Type

# Include/setobject.h
PyTypeObject    PySet_Type
PyTypeObject    PyFrozenSet_Type
PyTypeObject    PySetIter_Type

# Include/sliceobject.h
PyTypeObject    PySlice_Type
PyTypeObject    PyEllipsis_Type

# Include/traceback.h
PyTypeObject    PyTraceBack_Type

# Include/tupleobject.h
PyTypeObject    PyTuple_Type
PyTypeObject    PyTupleIter_Type

# Include/unicodeobject.h
PyTypeObject    PyUnicode_Type
PyTypeObject    PyUnicodeIter_Type

#################################################
# public API

#----------------------------
# types

# Include/cpython/cellobject.h
PyTypeObject    PyCell_Type

# Include/cpython/classobject.h
PyTypeObject    PyMethod_Type
PyTypeObject    PyInstanceMethod_Type

# Include/cpython/code.h
PyTypeObject    PyCode_Type

# Include/cpython/context.h
PyTypeObject    PyContext_Type
PyTypeObject    PyContextVar_Type
PyTypeObject    PyContextToken_Type

# Include/cpython/fileobject.h
PyTypeObject    PyStdPrinter_Type

# Include/cpython/frameobject.h
PyTypeObject    PyFrame_Type

# Include/cpython/funcobject.h
PyTypeObject    PyFunction_Type
PyTypeObject    PyClassMethod_Type
PyTypeObject    PyStaticMethod_Type

# Include/cpython/genobject.h
PyTypeObject    PyGen_Type
PyTypeObject    PyCoro_Type
PyTypeObject    PyAsyncGen_Type

# Include/cpython/methodobject.h
PyTypeObject    PyCMethod_Type

# Include/cpython/odictobject.h
PyTypeObject    PyODict_Type
PyTypeObject    PyODictIter_Type
PyTypeObject    PyODictKeys_Type
PyTypeObject    PyODictItems_Type
PyTypeObject    PyODictValues_Type

# Include/cpython/picklebufobject.h
PyTypeObject    PyPickleBuffer_Type

#################################################
# "private" API

#----------------------------
# singletons

# Include/setobject.h
PyObject *      _PySet_Dummy

#----------------------------
# types

# Include/descrobject.h
PyTypeObject    _PyMethodWrapper_Type

# Include/memoryobject.h
PyTypeObject    _PyManagedBuffer_Type

# Include/weakrefobject.h
PyTypeObject    _PyWeakref_RefType
PyTypeObject    _PyWeakref_ProxyType
PyTypeObject    _PyWeakref_CallableProxyType

# Include/cpython/genobject.h
PyTypeObject    _PyCoroWrapper_Type
PyTypeObject    _PyAsyncGenASend_Type
PyTypeObject    _PyAsyncGenWrappedValue_Type
PyTypeObject    _PyAsyncGenAThrow_Type

# Include/cpython/object.h
PyTypeObject    _PyNone_Type
PyTypeObject    _PyNotImplemented_Type

#################################################
# internal API

#----------------------------
# types

# Include/internal/pycore_hamt.h
PyTypeObject    _PyHamt_Type
PyTypeObject    _PyHamt_ArrayNode_Type
PyTypeObject    _PyHamt_BitmapNode_Type
PyTypeObject    _PyHamt_CollisionNode_Type
PyTypeObject    _PyHamtKeys_Type
PyTypeObject    _PyHamtValues_Type
PyTypeObject    _PyHamtItems_Type

# Include/internal/pycore_interpreteridobject.h
PyTypeObject    _PyInterpreterID_Type

# Include/internal/pycore_namespace.h
PyTypeObject    _PyNamespace_Type

*/

struct _Py_global_objects {
    // All field names here have an extra leading dollar sign.
    // This helps avoid collisions with keywords, etc.

    /* singletons */
    PyObject *$True;
    PyObject *$False;
    PyObject *$None;
    PyObject *$NotImplemented;
    PyObject *$Ellipsis;
};

/* low-level helpers for mapping object names to interpreter state */

#define _PyInterpreterState_GET_OBJECT(interp, NAME) \
    ((interp)->global_objects.$ ## NAME)

#define _PyInterpreterState_SET_OBJECT(interp, NAME, ob) \
    do { \
        assert((interp)->global_objects._ ## NAME == NULL); \
        assert(ob != NULL); \
        Py_INCREF(ob); \
        (interp)->global_objects.$ ## NAME = (PyObject *)(ob); \
    } while (0)

#define _PyInterpreterState_CLEAR_OBJECT(interp, NAME) \
    do { \
        Py_XDECREF((interp)->global_objects._ ## NAME); \
        (interp)->global_objects.$ ## NAME = NULL; \
    } while (0)


/* legacy C-API (symbols exposed for stable ABI < 3.11) */

PyAPI_DATA(struct _longobject) _Py_FalseStruct;
PyAPI_DATA(struct _longobject) _Py_TrueStruct;
PyAPI_DATA(PyObject) _Py_NoneStruct;
PyAPI_DATA(PyObject) _Py_NotImplementedStruct;


#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_GLOBAL_OBJECTS_H */
