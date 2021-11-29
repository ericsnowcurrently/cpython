#ifndef Py_INTERNAL_OBJECT_H
#define Py_INTERNAL_OBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include "pycore_interp.h"        // _PyInterpreterState_GET_OBJECT()
#include "pycore_pystate.h"       // _Py_IsMainInterpreter()


//////////////////////////////////
// singletons

static inline void
_PyFalse_Init(PyInterpreterState *interp)
{
    PyObject *ob;
    if (_Py_IsMainInterpreter(interp)) {
        ob = (PyObject *)&_Py_FalseStruct;
    }
    else {
        /* XXX Make a per-interpreter copy. */
        PyInterpreterState *main_interp = PyInterpreterState_Main();
        ob = _PyInterpreterState_GET_OBJECT(main_interp, False);
    }
    _PyInterpreterState_SET_OBJECT(interp, False, ob);
}

static inline PyObject *
_PyInterpreterState_GetObject_False(PyInterpreterState *interp)
{
    return _PyInterpreterState_GET_OBJECT(interp, False);
}

static inline void
_PyTrue_Init(PyInterpreterState *interp)
{
    PyObject *ob;
    if (_Py_IsMainInterpreter(interp)) {
        ob = (PyObject *)&_Py_TrueStruct;
    }
    else {
        /* XXX Make a per-interpreter copy. */
        PyInterpreterState *main_interp = PyInterpreterState_Main();
        ob = _PyInterpreterState_GET_OBJECT(main_interp, True);
    }
    _PyInterpreterState_SET_OBJECT(interp, True, ob);
}

static inline PyObject *
_PyInterpreterState_GetObject_True(PyInterpreterState *interp)
{
    return _PyInterpreterState_GET_OBJECT(interp, True);
}


#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_OBJECT_H */
