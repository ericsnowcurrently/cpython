#ifndef Py_INTERNAL_SIMPLEID_H
#define Py_INTERNAL_SIMPLEID_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif


typedef int64_t simpleid_t;
#define Py_SIMPLEID_MAX INT64_MAX

extern PyTypeObject PySimpleID_Type;

/* subclasses with lifetimes */

struct simpleid_lifetime_t {
    void *ctx;
    int (*init)(void *ctx, simpleid_t id, void **value);
    void (*incref)(void *ctx, simpleid_t id, void **value);
    void (*decref)(void *ctx, simpleid_t id, void **value);
};

extern PyTypeObject * _PySimpleID_NewSubclass(
    const char *name,
    PyObject *module,
    const char *doc,
    struct simpleid_lifetime_t *lifetime);
// 4 helpers for static types to match _PySimpleID_NewSubclass().
extern int _PySimpleID_SubclassInitialized(PyTypeObject *cls);
extern int _PySimpleID_InitSubclass(
    PyTypeObject *cls,
    struct simpleid_lifetime_t *lifetime);
extern PyObject * _PySimpleID_tp_new(PyTypeObject *, PyObject *, PyObject *);
extern void _PySimpleID_tp_dealloc(PyObject *);

/* other API */

extern int _PySimpleID_converter(PyObject *arg, void *ptr);

extern PyObject * PySimpleID_New(simpleid_t id, PyTypeObject *subclass);


#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_SIMPLEID_H */
