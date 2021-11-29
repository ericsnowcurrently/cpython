#ifndef Py_INTERNAL_SLICEOBJECT_H
#define Py_INTERNAL_SLICEOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include "pycore_global_objects.h"  // _PyInterpreterState_GET_OBJECT()
#include "pycore_pystate.h"       // _Py_IsMainInterpreter()


//////////////////////////////////
// singletons

static inline void
_PyEllipsis_Init(PyInterpreterState *interp)
{
    PyObject *ob;
    if (_Py_IsMainInterpreter(interp)) {
        ob = &_Py_EllipsisObject;
    }
    else {
        /* XXX Make a per-interpreter copy. */
        PyInterpreterState *main_interp = PyInterpreterState_Main();
        ob = _PyInterpreterState_GET_OBJECT(main_interp, Ellipsis);
    }
    _PyInterpreterState_SET_OBJECT(interp, Ellipsis, ob);
}

static inline PyObject *
_PyInterpreterState_GetObject_Ellipsis(PyInterpreterState *interp)
{
    return _PyInterpreterState_GET_OBJECT(interp, Ellipsis);
}


#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_SLICEOBJECT_H */
