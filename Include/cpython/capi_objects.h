#ifndef Py_CPYTHON_CAPI_OBJECTS_H
#  error "this header file must not be included directly"
#endif

// Get the corresponding interpreter-specific "global" object.
#define _PyInterpreterState_LOOK_UP_OBJECT(interp, NAME) \
    (_PyinterpreterState_LookUp ## NAME (interp))

// Get the corresponding interpreter-specific "global" object.
#define _PyInterpreterState_LOOK_UP_OBJECT_CURRENT(NAME) \
    _PyInterpreterState_LOOK_UP_OBJECT(PyInterpreterState_Get(), NAME)

#define getter(NAME) \
    PyAPI_FUNC(PyObject *) _PyInterpreterState_LookUp ## NAME (PyInterpreterState *)

#define exc(NAME) getter(PyExc_ ## NAME)

// exception types in the public C-API
//exc(BaseException);
//exc(Exception);
//exc(TypeError);
//exc(StopAsyncIteration);
//exc(StopIteration);
//exc(GeneratorExit);
//exc(SystemExit);
//exc(KeyboardInterrupt);
//exc(ImportError);
//exc(ModuleNotFoundError);
//exc(OSError);
//exc(EOFError);
//exc(RuntimeError);
//exc(RecursionError);
//exc(NotImplementedError);
//exc(NameError);
//exc(UnboundLocalError);
//exc(AttributeError);
//exc(SyntaxError);
//exc(IndentationError);
//exc(TabError);
//exc(LookupError);
//exc(IndexError);
//exc(KeyError);
//exc(ValueError);
//exc(UnicodeError);
//exc(UnicodeEncodeError);
//exc(UnicodeDecodeError);
//exc(UnicodeTranslateError);
//exc(AssertionError);
//exc(ArithmeticError);
//exc(FloatingPointError);
//exc(OverflowError);
//exc(ZeroDivisionError);
//exc(SystemError);
//exc(ReferenceError);
//exc(MemoryError);
//exc(BufferError);
//exc(ConnectionError);
//exc(BlockingIOError);
//exc(BrokenPipeError);
//exc(ChildProcessError);
//exc(ConnectionAbortedError);
//exc(ConnectionRefusedError);
//exc(ConnectionResetError);
//exc(FileExistsError);
//exc(FileNotFoundError);
//exc(IsADirectoryError);
//exc(NotADirectoryError);
//exc(InterruptedError);
//exc(PermissionError);
//exc(ProcessLookupError);
//exc(TimeoutError);

// warning category types in the public C-API
//exc(Warning);
//exc(UserWarning);
//exc(EncodingWarning);
//exc(DeprecationWarning);
//exc(PendingDeprecationWarning);
//exc(SyntaxWarning);
//exc(RuntimeWarning);
//exc(FutureWarning);
//exc(ImportWarning);
//exc(UnicodeWarning);
//exc(BytesWarning);
//exc(ResourceWarning);

#undef exc

// types in the limited C-API
//getter(PyFilter_Type);
//getter(PyMap_Type);
//getter(PyZip_Type);
//getter(PyBool_Type);
//getter(PyByteArray_Type);
//getter(PyByteArrayIter_Type);
//getter(PyBytes_Type);
//getter(PyBytesIter_Type);
//getter(PyComplex_Type);
//getter(PyClassMethodDescr_Type);
//getter(PyGetSetDescr_Type);
//getter(PyMemberDescr_Type);
//getter(PyMethodDescr_Type);
//getter(PyWrapperDescr_Type);
//getter(PyDictProxy_Type);
//getter(PyProperty_Type);
//getter(PyDict_Type);
//getter(PyDictKeys_Type);
//getter(PyDictValues_Type);
//getter(PyDictItems_Type);
//getter(PyDictIterKey_Type);
//getter(PyDictIterValue_Type);
//getter(PyDictIterItem_Type);
//getter(PyDictRevIterKey_Type);
//getter(PyDictRevIterItem_Type);
//getter(PyDictRevIterValue_Type);
//getter(PyEnum_Type);
//getter(PyReversed_Type);
//getter(PyFloat_Type);
//getter(Py_GenericAliasType);
//getter(PySeqIter_Type);
//getter(PyCallIter_Type);
//getter(PyList_Type);
//getter(PyListIter_Type);
//getter(PyListRevIter_Type);
//getter(PyLong_Type);
//getter(PyMemoryView_Type);
//getter(PyCFunction_Type);
//getter(PyModule_Type);
//getter(PyModuleDef_Type);
//getter(PyType_Type);
//getter(PyBaseObject_Type);
//getter(PySuper_Type);
//getter(PyCapsule_Type);
//getter(PyRange_Type);
//getter(PyRangeIter_Type);
//getter(PyLongRangeIter_Type);
//getter(PySet_Type);
//getter(PyFrozenSet_Type);
//getter(PySetIter_Type);
//getter(PySlice_Type);
//getter(PyEllipsis_Type);
//getter(PyTraceBack_Type);
//getter(PyTuple_Type);
//getter(PyTupleIter_Type);
//getter(PyUnicode_Type);
//getter(PyUnicodeIter_Type);

// types in the public C-API
//getter(PyCell_Type);
//getter(PyMethod_Type);
//getter(PyInstanceMethod_Type);
//getter(PyCode_Type);
//getter(PyContext_Type);
//getter(PyContextVar_Type);
//getter(PyContextToken_Type);
//getter(PyStdPrinter_Type);
//getter(PyFrame_Type);
//getter(PyFunction_Type);
//getter(PyClassMethod_Type);
//getter(PyStaticMethod_Type);
//getter(PyGen_Type);
//getter(PyCoro_Type);
//getter(PyAsyncGen_Type);
//getter(PyCMethod_Type);
//getter(PyODict_Type);
//getter(PyODictIter_Type);
//getter(PyODictKeys_Type);
//getter(PyODictItems_Type);
//getter(PyODictValues_Type);
//getter(PyPickleBuffer_Type);

// types in the "private" C-API
//getter(_PyMethodWrapper_Type);
//getter(_PyManagedBuffer_Type);
//getter(_PyWeakref_RefType);
//getter(_PyWeakref_ProxyType);
//getter(_PyWeakref_CallableProxyType);
//getter(_PyCoroWrapper_Type);
//getter(_PyAsyncGenASend_Type);
//getter(_PyAsyncGenWrappedValue_Type);
//getter(_PyAsyncGenAThrow_Type);
//getter(_PyNone_Type);
//getter(_PyNotImplemented_Type);

// types in the internal C-API  (XXX Ignore them?)
//getter(_PyHamt_Type);
//getter(_PyHamt_ArrayNode_Type);
//getter(_PyHamt_BitmapNode_Type);
//getter(_PyHamt_CollisionNode_Type);
//getter(_PyHamtKeys_Type);
//getter(_PyHamtValues_Type);
//getter(_PyHamtItems_Type);
//getter(_PyInterpreterID_Type);
//getter(_PyNamespace_Type);

// singletons in the public C-API
//getter(_Py_NoneStruct);
//getter(_Py_NotImplementedStruct);
//getter(_Py_EllipsisObject);
//struct _longobject _Py_FalseStruct;
//struct _longobject _Py_TrueStruct;

// singletons in the "private" C-API
//getter(*_PySet_Dummy);

#undef getter
