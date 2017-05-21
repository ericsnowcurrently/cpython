/* Main program tailored to ignore environmental & per-user settings */

#include "Python.h"
#include <locale.h>

#ifdef __FreeBSD__
#include <floatingpoint.h>
#endif

#define _SET_CONFIG_NONE(target) \
    do {                     \
        Py_INCREF(Py_None);  \
        (target) = Py_None;  \
    } while (0)

#define _SET_CONFIG_TEXT(target, value) \
    do {                     \
        PyUnicodeObject *_config_tmp = (value);    \
        Py_INCREF(_config_tmp);  \
        (target) = _config_tmp;  \
    } while (0)

static int
isolated_main(int argc, wchar_t **argv)
{
    /* No customisation yet, just use Py_Main */
    return Py_Main(argc, argv);

    /* Desired end state:
    _PyCoreConfig core_config = _PyCoreConfig_INIT;
    _PyMainInterpreterConfig config = _PyMainInterpreterConfig_INIT;
    PyObject *encoding = NULL;
    PyObject *errors = NULL;
    core_config.ignore_environment = 1;
    core_config.use_hash_seed = 0;
    _Py_InitializeCore(&core_config);
    // TODO: Report exception details & return an error code from here on out
    config.no_user_site = 1;
    config.dont_write_bytecode = 1;
    config.run_implicit_code = 0;
    // TODO: Consider handling of prefix and exec_prefix
    // TODO: Consider handling of pyvenv.cfg (or lack thereof)
    // TODO?: Read encoding setting from /etc/locale.d?
    encoding = PyUnicode_InternFromString("utf-8");
    if (encoding == NULL) {
        Py_FatalError("system-python: failed to create encoding string");
    }
    errors = PyUnicode_InternFromString("surrogateescape");
    if (errors == NULL) {
        Py_FatalError("system-python: failed to create errors string");
    }
    _SET_CONFIG_TEXT(config.stdin_encoding, encoding);
    _SET_CONFIG_TEXT(config.stdin_errors, errors);
    _SET_CONFIG_TEXT(config.stdout_encoding, encoding);
    _SET_CONFIG_TEXT(config.stdout_errors, errors);
    _SET_CONFIG_TEXT(config.stderr_encoding, encoding);
    _SET_CONFIG_TEXT(config.stderr_errors, errors);
    _SET_CONFIG_TEXT(config.fs_encoding, encoding);
    if (_Py_ReadMainInterpreterConfig(&config)) {
        Py_FatalError("system-python: Py_ReadMainInterpreterConfig failed");
    }
    if (_Py_InitializeMainInterpreter(&config)) {
        Py_FatalError("system-python: Py_InitializeMainInterpreter failed");
    }
    if (_Py_RunPrepareMain()) {
        Py_FatalError("system-python: Py_RunPrepareMain failed");
    }
    if (_Py_RunExecMain()) {
        Py_FatalError("system-python: Py_RunExecMain failed");
    }
    */

}

#ifdef MS_WINDOWS
int
wmain(int argc, wchar_t **argv)
{
    return isolated_main(argc, argv);
}
#else

int
main(int argc, char **argv)
{
    wchar_t **argv_copy;
    /* We need a second copy, as Python might modify the first one. */
    wchar_t **argv_copy2;
    int i, res;
    char *oldloc;
#ifdef __FreeBSD__
    fp_except_t m;
#endif

    argv_copy = (wchar_t **)PyMem_RawMalloc(sizeof(wchar_t*) * (argc+1));
    argv_copy2 = (wchar_t **)PyMem_RawMalloc(sizeof(wchar_t*) * (argc+1));
    if (!argv_copy || !argv_copy2) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    /* 754 requires that FP exceptions run in "no stop" mode by default,
     * and until C vendors implement C99's ways to control FP exceptions,
     * Python requires non-stop mode.  Alas, some platforms enable FP
     * exceptions by default.  Here we disable them.
     */
#ifdef __FreeBSD__
    m = fpgetmask();
    fpsetmask(m & ~FP_X_OFL);
#endif

    oldloc = _PyMem_RawStrdup(setlocale(LC_ALL, NULL));
    if (!oldloc) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    setlocale(LC_ALL, "");
    for (i = 0; i < argc; i++) {
        argv_copy[i] = Py_DecodeLocale(argv[i], NULL);
        if (!argv_copy[i]) {
            PyMem_RawFree(oldloc);
            fprintf(stderr, "Fatal Python error: "
                            "unable to decode the command line argument #%i\n",
                            i + 1);
            return 1;
        }
        argv_copy2[i] = argv_copy[i];
    }
    argv_copy2[argc] = argv_copy[argc] = NULL;

    setlocale(LC_ALL, oldloc);
    PyMem_RawFree(oldloc);
    res = isolated_main(argc, argv_copy);
    for (i = 0; i < argc; i++) {
        PyMem_RawFree(argv_copy2[i]);
    }
    PyMem_RawFree(argv_copy);
    PyMem_RawFree(argv_copy2);
    return res;
}
#endif
