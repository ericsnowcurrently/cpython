#ifndef Py_IMPORTDL_H
#define Py_IMPORTDL_H

#ifdef __cplusplus
extern "C" {
#endif

/* The following is also used for builtin modules. */
typedef PyObject *(*PyModInitFunction)(void);


#ifdef HAVE_DYNAMIC_LOADING

extern const char *_PyImport_DynLoadFiletab[];

struct module_name {
    PyObject *full;
    PyObject *encoded;
    int asciionly;
    const char *hook_prefix;
};

extern int _PyImport_GetDynamicModuleEncodedName(
        PyObject *fullname, struct module_name *result);

extern PyModInitFunction _PyImport_LoadDynamicModuleInitFunc(
        struct module_name *name, PyObject *path, FILE *fp);

/* Max length of module suffix searched for -- accommodates "module.slb" */
#define MAXSUFFIXSIZE 12

#ifdef MS_WINDOWS
#include <windows.h>
typedef FARPROC dl_funcptr;
#else
typedef void (*dl_funcptr)(void);
#endif

#endif /* HAVE_DYNAMIC_LOADING */


#ifdef __cplusplus
}
#endif
#endif /* !Py_IMPORTDL_H */
