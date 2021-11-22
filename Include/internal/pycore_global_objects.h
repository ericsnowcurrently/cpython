#ifndef Py_INTERNAL_GLOBAL_OBJECTS_H
#define Py_INTERNAL_GLOBAL_OBJECTS_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

struct _Py_global_objects {
    // All field names here have an extra leading dollar sign.
    // This helps avoid collisions with keywords, etc.
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


#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_GLOBAL_OBJECTS_H */
