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
PyObject *      BaseExceptionGroup
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

# Include/internal/pycore_unionobject.h
PyTypeObject    _PyUnion_Type

# Include/internal/pycore_symtable.h
PyTypeObject    PySTEntry_Type

# Include/iterobject.h  (#ifdef Py_BUILD_CORE)
PyTypeObject    _PyAnextAwaitable_Type

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

    /* exception types */
    PyObject *$ArithmeticError;
    PyObject *$AssertionError;
    PyObject *$AttributeError;
    PyObject *$BaseException;
    PyObject *$BaseExceptionGroup;
    PyObject *$BlockingIOError;
    PyObject *$BrokenPipeError;
    PyObject *$BufferError;
    PyObject *$ChildProcessError;
    PyObject *$ConnectionAbortedError;
    PyObject *$ConnectionError;
    PyObject *$ConnectionRefusedError;
    PyObject *$ConnectionResetError;
    PyObject *$EOFError;
    PyObject *$Exception;
    PyObject *$FileExistsError;
    PyObject *$FileNotFoundError;
    PyObject *$FloatingPointError;
    PyObject *$GeneratorExit;
    PyObject *$ImportError;
    PyObject *$IndentationError;
    PyObject *$IndexError;
    PyObject *$InterruptedError;
    PyObject *$IsADirectoryError;
    PyObject *$KeyError;
    PyObject *$KeyboardInterrupt;
    PyObject *$LookupError;
    PyObject *$MemoryError;
    PyObject *$ModuleNotFoundError;
    PyObject *$NameError;
    PyObject *$NotADirectoryError;
    PyObject *$NotImplementedError;
    PyObject *$OSError;
    PyObject *$OverflowError;
    PyObject *$PermissionError;
    PyObject *$ProcessLookupError;
    PyObject *$RecursionError;
    PyObject *$ReferenceError;
    PyObject *$RuntimeError;
    PyObject *$StopAsyncIteration;
    PyObject *$StopIteration;
    PyObject *$SyntaxError;
    PyObject *$SystemError;
    PyObject *$SystemExit;
    PyObject *$TabError;
    PyObject *$TimeoutError;
    PyObject *$TypeError;
    PyObject *$UnboundLocalError;
    PyObject *$UnicodeDecodeError;
    PyObject *$UnicodeEncodeError;
    PyObject *$UnicodeError;
    PyObject *$UnicodeTranslateError;
    PyObject *$ValueError;
    PyObject *$ZeroDivisionError;

    /* warning category types */
    PyObject *$BytesWarning;
    PyObject *$DeprecationWarning;
    PyObject *$EncodingWarning;
    PyObject *$FutureWarning;
    PyObject *$ImportWarning;
    PyObject *$PendingDeprecationWarning;
    PyObject *$ResourceWarning;
    PyObject *$RuntimeWarning;
    PyObject *$SyntaxWarning;
    PyObject *$UnicodeWarning;
    PyObject *$UserWarning;
    PyObject *$Warning;

    /* other __builtins__ */
    PyObject *$bool;
    PyObject *$bytearray;
    PyObject *$bytes;
    PyObject *$classmethod;
    PyObject *$complex;
    PyObject *$dict;
    PyObject *$enumerate;
    PyObject *$filter;
    PyObject *$float;
    PyObject *$frozenset;
    PyObject *$int;
    PyObject *$list;
    PyObject *$map;
    PyObject *$memoryview;
    PyObject *$object;
    PyObject *$property;
    PyObject *$range;
    PyObject *$reversed;
    PyObject *$set;
    PyObject *$staticmethod;
    PyObject *$str;
    PyObject *$super;
    PyObject *$tuple;
    PyObject *$type;
    PyObject *$zip;

    /* singleton types */
    PyObject *$EllipsisType;
    PyObject *$_NoneType;
    PyObject *$_NotImplementedType;

    /* module-specific types */
    PyObject *$OrderedDict;  // collections
    PyObject *$PickleBufferType;  // pickle
    PyObject *$_WeakrefCallableProxyType;  // weakref
    PyObject *$_WeakrefProxyType;  // weakref
    PyObject *$_WeakrefRefType;  // weakref

    /* runtime init only types */
    // XXX Keep on _PyRuntimeState?
    PyObject *$StdPrinterType;

    /* adapter types */
    PyObject *$CallIterType;  // iter(callable, sentinel)
    PyObject *$GenericAliasType;  // typing
    PyObject *$InstanceMethodType;
    PyObject *$SeqIterType;  // old iterator protocol
    PyObject *$_AsyncGenASendType;
    PyObject *$_AsyncGenAThrowType;
    PyObject *$_AsyncGenWrappedValueType;
    PyObject *$_CoroutineWrapperType;  // _PyCoroWrapper_Type
    // ...exposed in types module:
    PyObject *$MappingProxyType;  // PyDictProxy_Type
    PyObject *$MethodType;

    /* other C wrapper types */
    PyObject *$CapsuleType;
    PyObject *$ModuleDefType;
    // ...exposed in types module:
    PyObject *$BuiltinFunctionType;  // PyCFunction_Type
    PyObject *$BuiltinMethodType;  // PyCMethod_Type
    PyObject *$ClassMethodDescriptorType;
    PyObject *$GetSetDescriptorType;
    PyObject *$MemberDescriptorType;
    PyObject *$MethodDescriptorType;
    PyObject *$WrapperDescriptorType;
    PyObject *$_MethodWrapperType;

    /* other types */
    PyObject *$ContextTokenType;
    PyObject *$ContextType;
    PyObject *$ContextVarType;
    PyObject *$STEntryType;
    PyObject *$SliceType;
    PyObject *$_AnextAwaitableType;
    PyObject *$_HamtType;
    PyObject *$_Hamt_ArrayNodeType;
    PyObject *$_Hamt_BitmapNodeType;
    PyObject *$_Hamt_CollisionNodeType;
    PyObject *$_InterpreterIDType;
    PyObject *$_ManagedBufferType;
    PyObject *$_UnionType;
    // ...exposed in types module:
    PyObject *$AsyncGeneratorType;  // PyAsyncGen_Type
    PyObject *$CellType;
    PyObject *$CodeType;
    PyObject *$CoroutineType;  // PyCoro_Type
    PyObject *$FrameType;
    PyObject *$FunctionType;
    PyObject *$GeneratorType;  // PyGen_Type
    PyObject *$ModuleType;
    PyObject *$SimpleNamespace;  // _PyNamespace_Type
    PyObject *$TracebackType;

    /* type views */
    PyObject *$OrderedDict_items;
    PyObject *$OrderedDict_keys;
    PyObject *$OrderedDict_values;
    PyObject *$_Hamt_items;
    PyObject *$_Hamt_keys;
    PyObject *$_Hamt_values;
    PyObject *$dict_items;
    PyObject *$dict_keys;
    PyObject *$dict_values;

    /* type iterators */
    PyObject *$LongRangeIterType;
    PyObject *$OrderedDict_iter;
    PyObject *$bytearray_iter;
    PyObject *$bytes_iter;
    PyObject *$dict_items_iter;
    PyObject *$dict_items_reversed;
    PyObject *$dict_keys_iter;
    PyObject *$dict_keys_reversed;
    PyObject *$dict_values_iter;
    PyObject *$dict_values_reversed;
    PyObject *$list_iter;
    PyObject *$list_reversed;
    PyObject *$range_iter;
    PyObject *$set_iter;
    PyObject *$str_iter;
    PyObject *$tuple_iter;
};

/* low-level helpers for mapping object names to interpreter state */

#define _PyInterpreterState_GET_OBJECT(interp, NAME) \
    (assert(interp->global_objects.$ ## NAME == NULL), \
     interp->global_objects.$ ## NAME)

#define _PyInterpreterState_SET_OBJECT(interp, NAME, ob) \
    do { \
        assert(interp->global_objects.$ ## NAME == NULL); \
        assert(ob != NULL); \
        Py_INCREF(ob); \
        interp->global_objects.$ ## NAME = (PyObject *)(ob); \
    } while (0)

#define _PyInterpreterState_CLEAR_OBJECT(interp, NAME) \
    Py_XSETREF(interp->global_objects.$ ## NAME, NULL)


/* legacy C-API (symbols exposed for stable ABI < 3.11) */

PyAPI_DATA(struct _longobject) _Py_FalseStruct;
PyAPI_DATA(struct _longobject) _Py_TrueStruct;
PyAPI_DATA(PyObject) _Py_NoneStruct;
PyAPI_DATA(PyObject) _Py_NotImplementedStruct;


#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_GLOBAL_OBJECTS_H */
