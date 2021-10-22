#ifndef Py_PYSTATE_MINIMAL_H
#define Py_PYSTATE_MINIMAL_H
#ifdef __cplusplus
extern "C" {
#endif

/* forward declarations for PyThreadState and PyInterpreterState */
struct _ts;
struct _is;

// struct _ts is defined in cpython/pystate.h
typedef struct _ts PyThreadState;
// struct _is is defined in internal/pycore_interp.h
typedef struct _is PyInterpreterState;

#ifdef __cplusplus
}
#endif
#endif /* !Py_PYSTATE_MINIMAL_H */
