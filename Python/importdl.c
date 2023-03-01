
/* Support for dynamic loading of extension modules */

#include "Python.h"
#include "pycore_call.h"
#include "pycore_pystate.h"
#include "pycore_runtime.h"

/* ./configure sets HAVE_DYNAMIC_LOADING if dynamic loading of modules is
   supported on this platform. configure will then compile and link in one
   of the dynload_*.c files, as appropriate. We will call a function in
   those modules to get a function pointer to the module's init function.
*/
#ifdef HAVE_DYNAMIC_LOADING

#include "importdl.h"

#ifdef MS_WINDOWS
extern dl_funcptr _PyImport_FindSharedFuncptrWindows(const char *prefix,
                                                     const char *shortname,
                                                     PyObject *pathname,
                                                     FILE *fp);
#else
extern dl_funcptr _PyImport_FindSharedFuncptr(const char *prefix,
                                              const char *shortname,
                                              const char *pathname, FILE *fp);
#endif

static const char * const ascii_only_prefix = "PyInit";
static const char * const nonascii_prefix = "PyInitU";

/* Get the variable part of a module's export symbol name.
 * Returns a bytes instance. For non-ASCII-named modules, the name is
 * encoded as per PEP 489.
 * The hook_prefix pointer is set to either ascii_only_prefix or
 * nonascii_prefix, as appropriate.
 */
int
_PyImport_GetDynamicModuleEncodedName(
        PyObject *fullname, struct module_name *result)
{
    int rc = -1;
    PyObject *modname = NULL;
    int asciionly = -1;

    PyObject *name;
    PyObject *encoded = NULL;
    Py_ssize_t name_len, lastdot;

    if (!PyUnicode_Check(fullname)) {
        PyErr_SetString(PyExc_TypeError, "spec.name must be a string");
        return -1;
    }

    /* Get the short name (substring after last dot) */
    name_len = PyUnicode_GetLength(fullname);
    if (name_len < 0) {
        return -1;
    }
    lastdot = PyUnicode_FindChar(fullname, '.', 0, name_len, -1);
    if (lastdot < -1) {
        return -1;
    }
    else if (lastdot >= 0) {
        name = PyUnicode_Substring(fullname, lastdot + 1, name_len);
        if (name == NULL) {
            return -1;
        }
        /* "name" now holds a new reference to the substring */
    }
    else {
        name = Py_NewRef(fullname);
    }

    /* Encode to ASCII or Punycode, as needed */
    encoded = PyUnicode_AsEncodedString(name, "ascii", NULL);
    if (encoded != NULL) {
        asciionly = 1;
    } else {
        if (PyErr_ExceptionMatches(PyExc_UnicodeEncodeError)) {
            PyErr_Clear();
            encoded = PyUnicode_AsEncodedString(name, "punycode", NULL);
            if (encoded == NULL) {
                goto finally;
            }
            asciionly = 0;
        } else {
            goto finally;
        }
    }

    /* Replace '-' by '_' */
    modname = _PyObject_CallMethod(encoded, &_Py_ID(replace), "cc", '-', '_');
    if (modname == NULL) {
        goto finally;
    }

    result->full = fullname;
    result->encoded = modname;
    result->asciionly = asciionly;
    result->hook_prefix = asciionly ? ascii_only_prefix : nonascii_prefix;
    rc = 0;

finally:
    Py_DECREF(name);
    Py_XDECREF(encoded);
    return rc;
}

extern PyModInitFunction
_PyImport_LoadDynamicModuleInitFunc(
        struct module_name *name, PyObject *path, FILE *fp)
{
    dl_funcptr exportfunc = NULL;
#ifdef MS_WINDOWS
    exportfunc = _PyImport_FindSharedFuncptrWindows(
            name->hook_prefix,
            PyBytes_AS_STRING(name->encoded),
            path, fp);
#else
    PyObject *pathbytes = PyUnicode_EncodeFSDefault(path);
    if (pathbytes == NULL) {
        return NULL;
    }
    exportfunc = _PyImport_FindSharedFuncptr(
            name->hook_prefix,
            PyBytes_AS_STRING(name->encoded),
            PyBytes_AS_STRING(pathbytes),
            fp);
    Py_DECREF(pathbytes);
#endif

    if (exportfunc == NULL) {
        if (!PyErr_Occurred()) {
            PyObject *msg;
            msg = PyUnicode_FromFormat(
                "dynamic module does not define "
                "module export function (%s_%s)",
                name->hook_prefix,
                PyBytes_AS_STRING(name->encoded));
            if (msg != NULL) {
                return NULL;
            }
            PyErr_SetImportError(msg, name->full, path);
            Py_DECREF(msg);
        }
        return NULL;
    }

    return (PyModInitFunction)exportfunc;
}

#endif /* HAVE_DYNAMIC_LOADING */
