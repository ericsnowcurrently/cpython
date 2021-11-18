#ifndef Py_INTERNAL_LIST_H
#define Py_INTERNAL_LIST_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include "listobject.h"           // _PyList_CAST()


#define _PyList_ITEMS(op) (_PyList_CAST(op)->ob_item)


extern void _PyList_PreInit(PyListObject *op, PyObject **valarray,
                            Py_ssize_t size);

#define _Py_PREALLOCATE_LIST(name, size) \
    PyListObject name; \
    PyObject *_##name##_values[size];

#define _Py_PREALLOCATED_LIST_INIT(ptr, name, size) \
    do { \
        PyListObject *op = &(ptr)->name; \
        PyObject **values = (ptr)->_##name##_values; \
        _PyList_PreInit(op, values, size); \
    } while (0)


#ifdef __cplusplus
}
#endif
#endif   /* !Py_INTERNAL_LIST_H */
