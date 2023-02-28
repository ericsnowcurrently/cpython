
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

struct module_name {
    PyObject *full;
    PyObject *encoded;
    int asciionly;
    const char *hook_prefix;
};

/* Get the variable part of a module's export symbol name.
 * Returns a bytes instance. For non-ASCII-named modules, the name is
 * encoded as per PEP 489.
 * The hook_prefix pointer is set to either ascii_only_prefix or
 * nonascii_prefix, as appropriate.
 */
static int
get_encoded_name(PyObject *fullname, struct module_name *result) {
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

static PyModInitFunction
get_initfunc(struct module_name *name, PyObject *path, FILE *fp)
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

static PyModuleDef *
run_initfunc(PyModInitFunction initfunc,
             PyObject *fullname, PyObject *encoded, PyObject *path,
             PyObject **mod_p)
{
    PyObject *m;

    if (PySys_Audit("import", "OOOOO", fullname, path,
                    Py_None, Py_None, Py_None) < 0) {
        return NULL;
    }

    const char *oldcontext;
    const char *newcontext = PyUnicode_AsUTF8(fullname);
    if (newcontext == NULL) {
        return NULL;
    }

    /* Package context is needed for single-phase init */
    oldcontext = _PyImport_SwapPackageContext(newcontext);
    m = _PyImport_InitFunc_TrampolineCall(initfunc);
    _PyImport_SwapPackageContext(oldcontext);

    if (m == NULL) {
        if (!PyErr_Occurred()) {
            PyErr_Format(
                PyExc_SystemError,
                "initialization of %s failed without raising an exception",
                PyBytes_AS_STRING(encoded));
        }
        return NULL;
    } else if (PyErr_Occurred()) {
        _PyErr_FormatFromCause(
            PyExc_SystemError,
            "initialization of %s raised unreported exception",
            PyBytes_AS_STRING(encoded));
        // XXX decref m?
        return NULL;
    }
    if (Py_IS_TYPE(m, NULL)) {
        /* This can happen when a PyModuleDef is returned without calling
         * PyModuleDef_Init on it
         */
        PyErr_Format(PyExc_SystemError,
                     "init function of %s returned uninitialized object",
                     PyBytes_AS_STRING(encoded));
        // We do not decref m, to prevent a segfault.
        return NULL;
    }

    /* Try multi-phase init first. */
    if (PyObject_TypeCheck(m, &PyModuleDef_Type)) {
        *mod_p = NULL;
        return (PyModuleDef *)m;
    }

    /* Fall back to single-phase init mechanism. */
    PyModuleDef *def = PyModule_GetDef(m);
    if (def == NULL) {
        PyErr_Format(PyExc_SystemError,
                     "initialization of %s did not return an extension "
                     "module", PyBytes_AS_STRING(encoded));
        Py_DECREF(m);
        return NULL;
    }

    *mod_p = m;
    return def;
}

static int
handle_legacy_extension(PyObject *mod, PyModuleDef *def,
                        struct module_name *name, PyObject *path,
                        PyModInitFunction initfunc)
{
    if (!name->asciionly) {
        /* don't allow legacy init for non-ASCII module names */
        PyErr_Format(
            PyExc_SystemError,
            "initialization of %s did not return PyModuleDef",
            PyBytes_AS_STRING(name->encoded));
        return -1;
    }

    /* Remember pointer to module init function. */
    def->m_base.m_init = initfunc;

    /* Remember the filename as the __file__ attribute */
    if (PyModule_AddObjectRef(mod, "__file__", path) < 0) {
        PyErr_Clear(); /* Not important enough to report */
    }

    PyObject *modules = PyImport_GetModuleDict();
    if (_PyImport_FixupExtensionObject(mod, name->full, path, modules) < 0) {
        return -1;
    }

    return 0;
}

PyObject *
_PyImport_LoadDynamicModuleWithSpec(PyObject *spec, PyObject *fullname,
                                    PyObject *path,  FILE *fp)
{
    PyObject *m = NULL;
    PyModuleDef *def;
    PyModInitFunction p0;

    struct module_name name;
    if (get_encoded_name(fullname, &name) < 0) {
        goto finally;
    }

    p0 = get_initfunc(&name, path, fp);
    if (p0 == NULL) {
        goto finally;
    }

    def = run_initfunc(p0, fullname, name.encoded, path, &m);
    if (def == NULL) {
        goto finally;
    }
    else if (m == NULL) {
        /* multi-phase init */
        m = PyModule_FromDefAndSpec(def, spec);
    }
    else {
        /* single-phase init */
        if (handle_legacy_extension(m, def, &name, path, p0) < 0) {
            Py_CLEAR(m);
            goto finally;
        }
    }

finally:
    Py_DECREF(name.encoded);
    return m;
}

#endif /* HAVE_DYNAMIC_LOADING */
