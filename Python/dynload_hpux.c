
/* Support for dynamic loading of extension modules */

#include "dl.h"
#include <errno.h>

#include "Python.h"
#include "pycore_importdl.h"

#if defined(__hp9000s300)
#define FUNCNAME_PATTERN "_%.20s_%.200s"
#else
#define FUNCNAME_PATTERN "%.20s_%.200s"
#endif

const char *_PyImport_DynLoadFiletab[] = {SHLIB_EXT, ".sl", NULL};

dl_funcptr _PyImport_FindSharedFuncptr(const char *prefix,
                                       const char *shortname,
                                       const char *pathname, FILE *fp,
                                       MODULE_HANDLE *p_handle)
{
    int flags = BIND_FIRST | BIND_DEFERRED;
    int verbose = _Py_GetConfig()->verbose;
    if (verbose) {
        flags = BIND_FIRST | BIND_IMMEDIATE |
            BIND_NONFATAL | BIND_VERBOSE;
        printf("shl_load %s\n",pathname);
    }

    shl_t lib = shl_load(pathname, flags, 0);
    /* XXX Chuck Blake once wrote that 0 should be BIND_NOSTART? */
    if (lib == NULL) {
        if (verbose) {
            perror(pathname);
        }
        char buf[256];
        PyOS_snprintf(buf, sizeof(buf), "Failed to load %.200s",
                      pathname);
        PyObject *buf_ob = PyUnicode_DecodeFSDefault(buf);
        if (buf_ob == NULL)
            return NULL;
        PyObject *shortname_ob = PyUnicode_FromString(shortname);
        if (shortname_ob == NULL) {
            Py_DECREF(buf_ob);
            return NULL;
        }
        PyObject *pathname_ob = PyUnicode_DecodeFSDefault(pathname);
        if (pathname_ob == NULL) {
            Py_DECREF(buf_ob);
            Py_DECREF(shortname_ob);
            return NULL;
        }
        PyErr_SetImportError(buf_ob, shortname_ob, pathname_ob);
        Py_DECREF(buf_ob);
        Py_DECREF(shortname_ob);
        Py_DECREF(pathname_ob);
        return NULL;
    }

    char funcname[258];
    PyOS_snprintf(funcname, sizeof(funcname), FUNCNAME_PATTERN,
                  prefix, shortname);
    if (verbose) {
        printf("shl_findsym %s\n", funcname);
    }

    dl_funcptr p;
    if (shl_findsym(&lib, funcname, TYPE_UNDEFINED, (void *) &p) == -1) {
        shl_unload(lib);
        p = NULL;
    }
    if (p != NULL) {
        *p_handle = lib;
    }
    else if (verbose) {
        perror(funcname);
    }
    return p;
}

int
_PyImport_ReleaseDynamicModule(MODULE_HANDLE handle)
{
    if (shl_unload((shl_t)handle) != 0) {
        PyErr_SetString(PyExc_ImportError,
                        "error while unloading shared library");
        return -1;
    }
    return 0;
}
