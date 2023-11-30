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

extern PyTypeObject _PySimpleID_Type_Type;
extern PyTypeObject PySimpleID_Type;

extern PyObject * PySimpleID_New(simpleid_t id);


#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_SIMPLEID_H */
