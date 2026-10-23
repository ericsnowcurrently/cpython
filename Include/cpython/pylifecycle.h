#ifndef Py_CPYTHON_PYLIFECYCLE_H
#  error "this header file must not be included directly"
#endif

/* Py_FrozenMain is kept out of the Limited API until documented and present
   in all builds of Python */
PyAPI_FUNC(int) Py_FrozenMain(int argc, char **argv);

/* PEP 432 Multi-phase initialization API (Private while provisional!) */

PyAPI_FUNC(PyStatus) Py_PreInitialize(
    const PyPreConfig *src_config);
PyAPI_FUNC(PyStatus) Py_PreInitializeFromBytesArgs(
    const PyPreConfig *src_config,
    Py_ssize_t argc,
    char **argv);
PyAPI_FUNC(PyStatus) Py_PreInitializeFromArgs(
    const PyPreConfig *src_config,
    Py_ssize_t argc,
    wchar_t **argv);


/* Initialization and finalization */

PyAPI_FUNC(PyStatus) Py_InitializeFromConfig(
    const PyConfig *config);

// Python 3.8 provisional API (PEP 587)
PyAPI_FUNC(PyStatus) _Py_InitializeMain(void);

PyAPI_FUNC(int) Py_RunMain(void);


PyAPI_FUNC(void) _Py_NO_RETURN Py_ExitStatusException(PyStatus err);

PyAPI_FUNC(int) Py_FdIsInteractive(FILE *, const char *);

/* --- PyInterpreterConfig ------------------------------------ */

#define PyInterpreterConfig_DEFAULT_GIL (0)
#define PyInterpreterConfig_SHARED_GIL (1)
#define PyInterpreterConfig_OWN_GIL (2)

typedef struct {
    // XXX "allow_object_sharing"?  "own_objects"?
    int use_main_obmalloc;
    int allow_fork;
    int allow_exec;
    int allow_threads;
    int allow_daemon_threads;
    int check_multi_interp_extensions;
    int gil;
} PyInterpreterConfig;

#define _PyInterpreterConfig_INIT \
    { \
        .use_main_obmalloc = 0, \
        .allow_fork = 0, \
        .allow_exec = 0, \
        .allow_threads = 1, \
        .allow_daemon_threads = 0, \
        .check_multi_interp_extensions = 1, \
        .gil = PyInterpreterConfig_OWN_GIL, \
    }

// gh-117649: The free-threaded build does not currently support single-phase
// init extensions in subinterpreters. For now, we ensure that
// `check_multi_interp_extensions` is always `1`, even in the legacy config.
#ifdef Py_GIL_DISABLED
#  define _PyInterpreterConfig_LEGACY_CHECK_MULTI_INTERP_EXTENSIONS 1
#else
#  define _PyInterpreterConfig_LEGACY_CHECK_MULTI_INTERP_EXTENSIONS 0
#endif

#define _PyInterpreterConfig_LEGACY_INIT \
    { \
        .use_main_obmalloc = 1, \
        .allow_fork = 1, \
        .allow_exec = 1, \
        .allow_threads = 1, \
        .allow_daemon_threads = 1, \
        .check_multi_interp_extensions = _PyInterpreterConfig_LEGACY_CHECK_MULTI_INTERP_EXTENSIONS, \
        .gil = PyInterpreterConfig_SHARED_GIL, \
    }

PyAPI_FUNC(PyStatus) Py_NewInterpreterFromConfig(
    PyThreadState **tstate_p,
    const PyInterpreterConfig *config);

/* --- entering/leaving Python -------------------------------- */

// Call into Python without an interpreter.

//typedef struct {
//    unsigned long tid;
//    uint64_t interpid;
//    uint64_t tstateid;
////    PyInterpreterState *interp;
//    PyThreadState *tstate;
//} Py_paused_in_thread_t;

int Py_RuntimeEntered(void);
PyStatus Py_EnterRuntime(void);
PyStatus Py_LeaveRuntime(void);
int Py_RuntimePausedInThread(void);
//PyStatus Py_PauseRuntimeInThread(Py_paused_in_thread_t **);
//PyStatus Py_ResumeRuntimeInThread(Py_paused_in_thread_t *);
PyStatus Py_PauseRuntimeInThread(void);
PyStatus Py_ResumeRuntimeInThread(void);

#define Py_ENTER_RUNTIME(RET_STATUS) \
    { \
        int _entered = PyRuntimeEntered(); \
        int _paused = Py_RuntimePausedInThread(); \
        if (_entered) { \
            if (_paused) { \
                RET_STATUS = Py_ResumeRuntimeInThread(); \
            } \
            else { \
                RET_STATUS = PyStatus_Ok(); \
            } \
        } \
        else { \
            RET_STATUS = Py_EnterRuntime(); \
        }
#define Py_LEAVE_RUNTIME(RET_STATUS) \
        if (!_entered) { \
            RET_STATUS = Py_LeaveRuntime(); \
        } \
        else if (_paused) { \
            RET_STATUS = Py_PauseRuntimeInThread(); \
        } \
    }

// Call into Python with an interpreter but not a tstate.

//int Py_InterpreterEntered(uint64_t *p_interpid, PyInterpreterState **p_interp);
//PyStatus Py_EnterInterpreter(uint64_t interpid, PyInterpreterState **p_interp);
//PyStatus Py_LeaveInterpreter(void);
//int Py_InterpreterPausedInThread(void);
////PyStatus Py_PauseInterpreterInThread(Py_paused_in_thread_t **);
////PyStatus Py_ResumeInterpreterInThread(Py_paused_in_thread_t *);
//PyStatus Py_PauseInterpreterInThread(void);
//PyStatus Py_ResumeInterpreterInThread(void);
////PyStatus Py_SwitchInterpreter(...);

// Call into Python with a tstate.

//int Py_PythonEntered(PyThreadState **p_tstate);
//PyStatus Py_EnterPython(PyThreadState *tstate);
//PyStatus Py_LeavePython(void);
//int Py_PythonPausedInThread(void);
////PyStatus Py_PausePythonInThread(Py_paused_in_thread_t **);
////PyStatus Py_ResumePythonInThread(Py_paused_in_thread_t *);
//PyStatus Py_PausePythonInThread(void);
//PyStatus Py_ResumePythonInThread(void);

//PyStatus Py_PauseRuntimeDueToExternal(void);
//PyStatus Py_ResumeRuntimeDueToExternal(void);
//PyStatus Py_PauseInterpreterDueToExternal(void);
//PyStatus Py_ResumeInterpreterDueToExternal(void);
//PyStatus Py_PausePythonDueToExternal(void);
//PyStatus Py_ResumePythonDueToExternal(void);

/* --- other -------------------------------------------------- */

typedef void (*atexit_datacallbackfunc)(void *);
PyAPI_FUNC(int) PyUnstable_AtExit(
        PyInterpreterState *, atexit_datacallbackfunc, void *);
