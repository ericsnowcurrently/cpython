/* Boolean object interface */

#ifndef Py_BOOLOBJECT_H
#define Py_BOOLOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif


PyAPI_DATA(PyTypeObject) PyBool_Type;

#define PyBool_Check(x) Py_IS_TYPE(x, &PyBool_Type)

/* Py_False and Py_True are the only two bools in existence.
Don't forget to apply Py_INCREF() when returning either!!! */

#if Py_LIMITED_API+0 < 0x03110000
/* Don't use these directly */
PyAPI_DATA(struct _longobject) _Py_FalseStruct;
PyAPI_DATA(struct _longobject) _Py_TrueStruct;

/* Use these macros */
#define Py_False ((PyObject *) &_Py_FalseStruct)
#define Py_True ((PyObject *) &_Py_TrueStruct)

#else
PyAPI_FUNC(PyObject *) PyInterpreterState_GetObject_False(
        PyInterpreterState *interp);
PyAPI_FUNC(PyObject *) PyInterpreterState_GetObject_True(
        PyInterpreterState *interp);
#define Py_False (PyInterpreterState_GetObject_False(_PyInterpreterState_GET())
#define Py_True (PyInterpreterState_GetObject_True(_PyInterpreterState_GET())
#endif

// Test if an object is the True singleton, the same as "x is True" in Python.
PyAPI_FUNC(int) Py_IsTrue(PyObject *x);
#define Py_IsTrue(x) Py_Is((x), Py_True)

// Test if an object is the False singleton, the same as "x is False" in Python.
PyAPI_FUNC(int) Py_IsFalse(PyObject *x);
#define Py_IsFalse(x) Py_Is((x), Py_False)

/* Macros for returning Py_True or Py_False, respectively */
#define Py_RETURN_TRUE return Py_NewRef(Py_True)
#define Py_RETURN_FALSE return Py_NewRef(Py_False)

/* Function to return a bool from a C long */
PyAPI_FUNC(PyObject *) PyBool_FromLong(long);

#ifdef __cplusplus
}
#endif
#endif /* !Py_BOOLOBJECT_H */
