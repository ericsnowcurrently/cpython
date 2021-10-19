#ifndef Py_INTERNAL_CAPI_OBJECTS_H
#define Py_INTERNAL_CAPI_OBJECTS_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

struct _Py_capi_objects {
    // XXX Add all PyAPI_DATA(PyObject *) here.
    // ~67 exception types in the public C-API.
    // ~80 types in the public C-API.
    // ~17 types in the "private" C-API.
    // ~8 types in the internal C-API.  (XXX Ignore them?)
    // 5 singletons in the public C-API.
    // 1 singleton in the "private" C-API.
};

#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_CAPI_OBJECTS_H */
