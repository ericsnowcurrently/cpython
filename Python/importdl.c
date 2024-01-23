
/* Support for dynamic loading of extension modules */

#include "Python.h"
#include "pycore_call.h"
#include "pycore_import.h"
#include "pycore_pyerrors.h"      // _PyErr_FormatFromCause()
#include "pycore_pystate.h"
#include "pycore_runtime.h"

/* ./configure sets HAVE_DYNAMIC_LOADING if dynamic loading of modules is
   supported on this platform. configure will then compile and link in one
   of the dynload_*.c files, as appropriate. We will call a function in
   those modules to get a function pointer to the module's init function.
*/
#ifdef HAVE_DYNAMIC_LOADING

#include "pycore_importdl.h"

#ifdef MS_WINDOWS
extern dl_funcptr _PyImport_FindSharedFuncptrWindows(const char *prefix,
                                                     const char *shortname,
                                                     PyObject *pathname,
                                                     FILE *fp,
                                                     MODULE_HANDLE);
#else
extern dl_funcptr _PyImport_FindSharedFuncptr(const char *prefix,
                                              const char *shortname,
                                              const char *pathname, FILE *fp,
                                              MODULE_HANDLE);
#endif

static const char * const ascii_only_prefix = "PyInit";
static const char * const nonascii_prefix = "PyInitU";

/* Get the variable part of a module's export symbol name.
 * Returns a bytes instance. For non-ASCII-named modules, the name is
 * encoded as per PEP 489.
 * The hook_prefix pointer is set to either ascii_only_prefix or
 * nonascii_prefix, as appropriate.
 */
static PyObject *
get_encoded_name(PyObject *name, const char **hook_prefix) {
    PyObject *tmp;
    PyObject *encoded = NULL;
    PyObject *modname = NULL;
    Py_ssize_t name_len, lastdot;

    /* Get the short name (substring after last dot) */
    name_len = PyUnicode_GetLength(name);
    if (name_len < 0) {
        return NULL;
    }
    lastdot = PyUnicode_FindChar(name, '.', 0, name_len, -1);
    if (lastdot < -1) {
        return NULL;
    } else if (lastdot >= 0) {
        tmp = PyUnicode_Substring(name, lastdot + 1, name_len);
        if (tmp == NULL)
            return NULL;
        name = tmp;
        /* "name" now holds a new reference to the substring */
    } else {
        Py_INCREF(name);
    }

    /* Encode to ASCII or Punycode, as needed */
    encoded = PyUnicode_AsEncodedString(name, "ascii", NULL);
    if (encoded != NULL) {
        *hook_prefix = ascii_only_prefix;
    } else {
        if (PyErr_ExceptionMatches(PyExc_UnicodeEncodeError)) {
            PyErr_Clear();
            encoded = PyUnicode_AsEncodedString(name, "punycode", NULL);
            if (encoded == NULL) {
                goto error;
            }
            *hook_prefix = nonascii_prefix;
        } else {
            goto error;
        }
    }

    /* Replace '-' by '_' */
    modname = _PyObject_CallMethod(encoded, &_Py_ID(replace), "cc", '-', '_');
    if (modname == NULL)
        goto error;

    Py_DECREF(name);
    Py_DECREF(encoded);
    return modname;
error:
    Py_DECREF(name);
    Py_XDECREF(encoded);
    return NULL;
}

struct spec_info {
    const char *prefix;
    const char *name;
    PyObject *nameobj;
    PyObject *_namebytes;
    PyObject *path;
};

static void
clear_spec_info(struct spec_info *info)
{
    Py_DECREF(info->nameobj);
    Py_DECREF(info->_namebytes);
    Py_DECREF(info->path);
}

static int
get_spec_info(PyObject *spec, struct spec_info *info)
{
    PyObject *nameobj, *namebytes, *path;
    const char *name, *prefix;

    nameobj = PyObject_GetAttrString(spec, "name");
    if (nameobj == NULL) {
        return -1;
    }
    if (!PyUnicode_Check(nameobj)) {
        PyErr_SetString(PyExc_TypeError,
                        "spec.name must be a string");
        Py_DECREF(nameobj);
        return -1;
    }

    namebytes = get_encoded_name(nameobj, &prefix);
    if (namebytes == NULL) {
        Py_DECREF(nameobj);
        return -1;
    }
    name = PyBytes_AS_STRING(namebytes);

    path = PyObject_GetAttrString(spec, "origin");
    if (path == NULL) {
        Py_DECREF(nameobj);
        Py_DECREF(namebytes);
        return -1;
    }

    *info = (struct spec_info){
        .prefix=prefix,
        .name=name,
        .nameobj=nameobj,
        ._namebytes=namebytes,
        .path=path,
    };
    return 0;
}

static PyModInitFunction
get_init_func(const char *prefix, const char *name, PyObject *nameobj,
              PyObject *path, FILE *fp, MODULE_HANDLE *p_handle)
{
    dl_funcptr exportfunc;
#ifdef MS_WINDOWS
    exportfunc = _PyImport_FindSharedFuncptrWindows(prefix, name, path, fp,
                                                    p_handle);
#else
    PyObject *pathbytes = PyUnicode_EncodeFSDefault(path);
    if (pathbytes == NULL) {
        return NULL;
    }
    exportfunc = _PyImport_FindSharedFuncptr(prefix, name,
                                             PyBytes_AS_STRING(pathbytes), fp,
                                             p_handle);
    Py_DECREF(pathbytes);
#endif

    if (exportfunc == NULL) {
        if (!PyErr_Occurred()) {
            PyObject *msg;
            msg = PyUnicode_FromFormat(
                "dynamic module does not define "
                "module export function (%s_%s)",
                prefix, name);
            if (msg == NULL) {
                return NULL;
            }
            PyErr_SetImportError(msg, nameobj, path);
            Py_DECREF(msg);
        }
        return NULL;
    }
    return (PyModInitFunction)exportfunc;
}

static PyObject *
load_module(PyModInitFunction p0, const char *name, PyObject *nameobj)
{
    PyObject *m = NULL;

    /* Package context is needed for single-phase init */
    const char *newcontext = PyUnicode_AsUTF8(nameobj);
    if (newcontext == NULL) {
        return NULL;
    }
    const char *oldcontext = _PyImport_SwapPackageContext(newcontext);
    m = p0();
    _PyImport_SwapPackageContext(oldcontext);

    if (m == NULL) {
        if (!PyErr_Occurred()) {
            PyErr_Format(
                PyExc_SystemError,
                "initialization of %s failed without raising an exception",
                name);
        }
        return NULL;
    }
    else if (PyErr_Occurred()) {
        _PyErr_FormatFromCause(
            PyExc_SystemError,
            "initialization of %s raised unreported exception",
            name);
        // XXX Py_DECREF(m)?
        return NULL;
    }
    else if (Py_IS_TYPE(m, NULL)) {
        /* This can happen when a PyModuleDef is returned without calling
         * PyModuleDef_Init on it
         */
        PyErr_Format(PyExc_SystemError,
                     "init function of %s returned uninitialized object",
                     name);
        // XXX Py_DECREF(m)?
        return NULL;
    }
    return m;
}

static int
finish_single_phase_init(PyObject *m, PyModInitFunction p0, PyObject *path,
                         const char *prefix, const char *name, PyObject *nameobj)
{
    if (_PyImport_CheckSubinterpIncompatibleExtensionAllowed(name) < 0) {
        return -1;
    }

    if (prefix == nonascii_prefix) {
        /* don't allow legacy init for non-ASCII module names */
        PyErr_Format(
            PyExc_SystemError,
            "initialization of %s did not return PyModuleDef",
            name);
        return -1;
    }

    /* Remember pointer to module init function. */
    PyModuleDef *def = PyModule_GetDef(m);
    if (def == NULL) {
        PyErr_Format(PyExc_SystemError,
                     "initialization of %s did not return an extension "
                     "module", name);
        return -1;
    }
    def->m_base.m_init = p0;

    /* Remember the filename as the __file__ attribute */
    if (PyModule_AddObjectRef(m, "__file__", path) < 0) {
        PyErr_Clear(); /* Not important enough to report */
    }

    PyObject *modules = PyImport_GetModuleDict();
    if (_PyImport_FixupExtensionObject(m, nameobj, path, modules) < 0) {
        return -1;
    }

    return 0;
}

PyObject *
_PyImport_LoadDynamicModuleWithSpec(PyObject *spec, FILE *fp)
{
    struct spec_info info;
    MODULE_HANDLE handle = NULL;
    PyModInitFunction p0;
    PyObject *m = NULL;

    if (get_spec_info(spec, &info) < 0) {
        return NULL;
    }

    if (PySys_Audit("import", "OOOOO", info.nameobj, info.path,
                    Py_None, Py_None, Py_None) < 0)
    {
        goto error;
    }

    p0 = get_init_func(info.prefix, info.name, info.nameobj, info.path, fp,
                       &handle);
    if (p0 == NULL) {
        goto error;
    }

    m = load_module(p0, info.name, info.nameobj);
    if (m == NULL) {
        goto error;
    }
    if (PyObject_TypeCheck(m, &PyModuleDef_Type)) {
        PyModuleDef *def = (PyModuleDef *)m;
        m = PyModule_FromDefAndSpec(def, spec);
        // XXX Py_DECREF(def)?
        if (m == NULL) {
            goto error;
        }
    }
    /* Fall back to single-phase init mechanism */
    else if (finish_single_phase_init(m, p0, info.path, info.prefix, info.name,
                                      info.nameobj) < 0)
    {
        goto error;
    }

    ((PyModuleObject *)m)->md_handle = handle;
    // XXX Also track the handle globally, to ensure it gets closed?

    goto finally;

error:
    Py_CLEAR(m);

finally:
    clear_spec_info(&info);
    return m;
}

#endif /* HAVE_DYNAMIC_LOADING */
