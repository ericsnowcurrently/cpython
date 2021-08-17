
/* Thread and interpreter state structures and their interfaces */

#include "Python.h"
#include "pycore_ceval.h"
#include "pycore_frame.h"
#include "pycore_initconfig.h"
#include "pycore_object.h"        // _PyType_InitCache()
#include "pycore_pyerrors.h"
#include "pycore_pylifecycle.h"
#include "pycore_pymem.h"         // _PyMem_SetDefaultAllocator()
#include "pycore_pystate.h"       // _PyThreadState_GET()
#include "pycore_sysmodule.h"
#include "pycore_thread.h"        // _PyThread_init_lock()
#include <stdbool.h>

/* --------------------------------------------------------------------------
CAUTION

Always use PyMem_RawMalloc() and PyMem_RawFree() directly in this file.  A
number of these functions are advertised as safe to call when the GIL isn't
held, and in a debug build Python redirects (e.g.) PyMem_NEW (etc) to Python's
debugging obmalloc functions.  Those aren't thread-safe (they rely on the GIL
to avoid the expense of doing their own locking).
-------------------------------------------------------------------------- */

#ifdef HAVE_DLOPEN
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif
#if !HAVE_DECL_RTLD_LAZY
#define RTLD_LAZY 1
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline int
init_lock_data(_PyThread_type_lock *lock)
{
    return _PyThread_init_lock((PyThread_type_lock *)&lock);
}

static void
init_runtime(_PyRuntimeState *runtime, _PyRuntimeState *preserved)
{
    assert(runtime->preinitializing == 0);
    assert(runtime->preinitialized == 0);
    assert(runtime->core_initialized == 0);
    assert(runtime->initialized == 0);
    assert(_PyRuntimeState_GetFinalizing(runtime) == NULL);

    PyPreConfig_InitPythonConfig(&runtime->preconfig);

    /* audit hooks */
    runtime->open_code_hook = preserved->open_code_hook;
    runtime->open_code_userdata = preserved->open_code_userdata;
    runtime->audit_hook_head = preserved->audit_hook_head;

    /* interpreters */
    // This is set to 0 in _PyInterpreterState_Enable().
    runtime->interpreters.next_id = -1;
    // For now we (mostly) do not initialize the main interpreter here.
    runtime->interpreters.main.initializing = false;

    /* XID registry */
    assert(runtime->xidregistry.head == NULL);

    /* global ceval */
    _PyEval_InitRuntimeState(&runtime->ceval);
    assert(runtime->nexitfuncs == 0);
    runtime->gilstate.check_enabled = 1;
    // A TSS key must be initialized with Py_tss_NEEDS_INIT
    // in accordance with the specification.
    Py_tss_t initial = Py_tss_NEEDS_INIT;
    runtime->gilstate.autoTSSkey = initial;

    /* unicode IDs */
    runtime->unicode_ids.next_index = preserved->unicode_ids.next_index;
}

static PyStatus
init_runtime_threads(_PyRuntimeState *runtime)
{
    // Set it to the ID of the main thread of the main interpreter.
    runtime->main_thread = PyThread_get_thread_ident();

    /* interpreters */
    // We init interpreters.mutex in _PyInterpreterState_Enable().
    assert(runtime->interpreters.next_id < 0);
    // For now we do not initialize the main interpreter locks here.

    /* XID registry */
    if (init_lock_data(&runtime->xidregistry.mutex) != 0) {
        return _PyStatus_NO_MEMORY();
    }

    /* unicode IDs */
    if (init_lock_data(&runtime->unicode_ids.lock) != 0) {
        return _PyStatus_NO_MEMORY();
    }

    return _PyStatus_OK();
}

#ifdef HAVE_FORK
static PyStatus reinit_interpreter_threads(PyInterpreterState *);

static PyStatus
reinit_runtime_threads(_PyRuntimeState *runtime)
{
    // This was initially set in _PyRuntimeState_Init().
    runtime->main_thread = PyThread_get_thread_ident();

    PyStatus status;

    /* interpreters */
    if(runtime->interpreters.next_id >= 0) {
        // _PyInterpreterState_Enable() has been called.
        if (init_lock_data(&runtime->interpreters.mutex) != 0) {
            return _PyStatus_NO_MEMORY();
        }
    }
    PyInterpreterState *main_interp = &runtime->interpreters.main;
    assert(main_interp != NULL);
    status = reinit_interpreter_threads(main_interp);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    /* XID registry */
    if (init_lock_data(&runtime->xidregistry.mutex) != 0) {
        return _PyStatus_NO_MEMORY();
    }

    /* unicode IDs */
    if (init_lock_data(&runtime->unicode_ids.lock) != 0) {
        return _PyStatus_NO_MEMORY();
    }

    return _PyStatus_OK();
}
#endif

PyStatus
_PyRuntimeState_Init(_PyRuntimeState *runtime)
{
    /* Force default allocator, since _PyRuntimeState_Fini() must
       use the same allocator than this function. */
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    _PyRuntimeState preserved = {
        .xidregistry = {
            .head = runtime->xidregistry.head,
        },
        /* We preserve the hook across init, because there is
           currently no public API to set it between runtime
           initialization and interpreter initialization. */
        .open_code_hook = runtime->open_code_hook,
        .open_code_userdata = runtime->open_code_userdata,
        .audit_hook_head = runtime->audit_hook_head,
        // bpo-42882: Preserve next_index value if Py_Initialize()/Py_Finalize()
        // is called multiple times.
        .unicode_ids = {
            .next_index = runtime->unicode_ids.next_index,
        },
    };
    memset(runtime, 0, sizeof(*runtime));
    init_runtime(runtime, &preserved);
    //memcpy(runtime, preserved, sizeof(*runtime);
    PyStatus status = init_runtime_threads(runtime);

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
    return status;
}

void
_PyRuntimeState_Fini(_PyRuntimeState *runtime)
{
    /* Force the allocator used by _PyRuntimeState_Init(). */
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    _PyThread_clear_lock(&runtime->interpreters.mutex);
    _PyThread_clear_lock(&runtime->xidregistry.mutex);
    _PyThread_clear_lock(&runtime->unicode_ids.lock);

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
}

#ifdef HAVE_FORK
/* This function is called from PyOS_AfterFork_Child to ensure that
   newly created child processes do not share locks with the parent. */
PyStatus
_PyRuntimeState_ReInitThreads(_PyRuntimeState *runtime)
{
    /* Force default allocator, since _PyRuntimeState_Fini() must
       use the same allocator than this function. */
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    PyStatus status = reinit_runtime_threads(runtime);

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
    return status;
}
#endif

#define INTERP_HEAD_LOCK(runtime) \
    PyThread_acquire_lock(&(runtime)->interpreters.mutex, WAIT_LOCK)
#define INTERP_HEAD_UNLOCK(runtime) \
    PyThread_release_lock(&(runtime)->interpreters.mutex)

/* Forward declaration */
static void _PyGILState_NoteThreadState(
    struct _gilstate_runtime_state *gilstate, PyThreadState* tstate);

PyStatus
_PyInterpreterState_Enable(_PyRuntimeState *runtime)
{
    struct pyinterpreters *interpreters = &runtime->interpreters;
    interpreters->next_id = 0;

    PyStatus status = _PyStatus_OK();

    /* Py_Finalize() calls _PyRuntimeState_Fini() which clears the mutex.
       Init the mutex if needed. */

    /* Force default allocator, since _PyRuntimeState_Fini() must
       use the same allocator than this function. */
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    if (init_lock_data(&runtime->interpreters.mutex) != 0) {
        status = _PyStatus_NO_MEMORY();
    }

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    return status;
}

/* Set all fields that do not start out as 0/NULL, or are not otherwise
 * guaranteed to be set later. . */
static void
init_interpreter(PyInterpreterState *interp,
                 _PyRuntimeState *runtime, int64_t id)
{
    interp->next = NULL;
    interp->pythreads.head = NULL;

    interp->runtime = runtime;

    interp->id = id;
    interp->id_refcount = -1;
    /* id_mutex is initialized in init_interpreter_threads(). */

    interp->pythreads.num_threads = 0;

    _PyEval_InitState(&interp->ceval);
    interp->eval_frame = _PyEval_EvalFrameDefault;

    _PyGC_InitState(&interp->gc);

    PyConfig_InitPythonConfig(&interp->config);

    _PyType_InitCache(interp);

#ifdef HAVE_DLOPEN
#if HAVE_DECL_RTLD_NOW
    interp->dlopenflags = RTLD_NOW;
#else
    interp->dlopenflags = RTLD_LAZY;
#endif
#endif

    interp->pythreads.next_id = -1;

    interp->audit_hooks = NULL;
}

static PyStatus
init_interpreter_threads(PyInterpreterState *interp)
{
    PyStatus status;

    status =_PyEval_InitThreads(&interp->ceval);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    assert(interp->id_refcount < 0);
    /* id_mutex is initialized in _PyInterpreterState_IDInitref(). */

    // We init pythreads.mutex in _PyThreadState_Enable().
    assert(interp->pythreads.next_id < 0);
    // For now we do not initialize the main interpreter locks here.

    return _PyStatus_OK();
}

static PyStatus
reinit_interpreter_threads(PyInterpreterState *interp)
{
    // Note that _PyEval_ReInitThreads() is called directly
    // in _PyEval_ReInitState() rather than here.

    if (interp->id_refcount >= 0) {
        // It was initialized in _PyInterpreterState_IDInitref().
        if (init_lock_data(&interp->id_mutex) != 0) {
            return _PyStatus_NO_MEMORY();
        }
    }

    if (interp->pythreads.next_id >= 0) {
        // _PyThreadState_Enable() has been called.
        if (init_lock_data(&interp->pythreads.mutex) != 0) {
            return _PyStatus_NO_MEMORY();
        }
    }

    return _PyStatus_OK();
}

static void
set_next_interpreter(PyInterpreterState *interp)
{
    _PyRuntimeState *runtime = interp->runtime;
    assert(runtime != NULL);

    assert(runtime->interpreters.next_id >= 0);
    assert(interp->id < 0);
    assert(interp->next == NULL);

    INTERP_HEAD_LOCK(runtime);
    interp->id = runtime->interpreters.next_id;
    runtime->interpreters.next_id += 1;

    interp->next = runtime->interpreters.head;
    runtime->interpreters.head = interp;

    if (interp == &runtime->interpreters.main) {
        assert(interp->id == 0);
    }
    INTERP_HEAD_UNLOCK(runtime);
}

PyInterpreterState *
_PyInterpreterState_New(_PyRuntimeState *runtime,
                        PyThreadState *current_tstate)
{
    /* current_tstate is NULL when Py_InitializeFromConfig() calls
       _PyInterpreterState_New() to create the main interpreter. */
    if (_PySys_Audit(current_tstate,
                     "cpython.PyInterpreterState_New", NULL) < 0) {
        return NULL;
    }

    if (runtime->interpreters.next_id < 0) {
        /* overflow or Py_Initialize() not called! */
        if (current_tstate != NULL) {
            _PyErr_SetString(current_tstate, PyExc_RuntimeError,
                             "failed to get an interpreter ID");
        }
        return NULL;
    }

    PyInterpreterState *interp;
    INTERP_HEAD_LOCK(runtime);
    if (runtime->interpreters.head == NULL) {
        interp = &runtime->interpreters.main;
        assert(!interp->initializing);
        interp->initializing = true;
        INTERP_HEAD_UNLOCK(runtime);
        interp->needs_free = false;
    }
    else {
        INTERP_HEAD_UNLOCK(runtime);
        interp = PyMem_RawCalloc(1, sizeof(PyInterpreterState));
        if (interp == NULL) {
            return NULL;
        }
        interp->initializing = true;
        interp->needs_free = true;
    }

    init_interpreter(interp, runtime, -1);

    set_next_interpreter(interp);

    PyStatus status = init_interpreter_threads(interp);
    interp->initializing = false;
    if (_PyStatus_EXCEPTION(status)) {
        _PyErr_SetFromPyStatus(status);
        PyInterpreterState_Delete(interp);
        return NULL;
    }

    return interp;
}

PyInterpreterState *
PyInterpreterState_New(void)
{
    PyThreadState *current_tstate = _PyThreadState_GET();
    _PyRuntimeState *runtime = &_PyRuntime;
    if (current_tstate != NULL) {
        runtime = current_tstate->interp->runtime;
    }
    return _PyInterpreterState_New(runtime, current_tstate);
}

#define TSTATE_HEAD_LOCK(interp) \
    PyThread_acquire_lock(&(interp)->pythreads.mutex, WAIT_LOCK)
#define TSTATE_HEAD_UNLOCK(interp) \
    PyThread_release_lock(&(interp)->pythreads.mutex)

PyStatus
_PyThreadState_Enable(PyInterpreterState *interp)
{
    interp->pythreads.next_id = 0;

    PyStatus status = _PyStatus_OK();

    /* Force default allocator, since _PyRuntimeState_Fini() must
       use the same allocator than this function. */
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    if (init_lock_data(&interp->pythreads.mutex) != 0) {
        status = _PyStatus_NO_MEMORY();
    }

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    return status;
}


static void
interpreter_clear(PyInterpreterState *interp, PyThreadState *tstate)
{
    _PyRuntimeState *runtime = interp->runtime;

    if (_PySys_Audit(tstate, "cpython.PyInterpreterState_Clear", NULL) < 0) {
        _PyErr_Clear(tstate);
    }

    INTERP_HEAD_LOCK(runtime);
    for (PyThreadState *p = interp->pythreads.head; p != NULL; p = p->next) {
        PyThreadState_Clear(p);
    }
    INTERP_HEAD_UNLOCK(runtime);

    Py_CLEAR(interp->audit_hooks);

    PyConfig_Clear(&interp->config);
    Py_CLEAR(interp->codec_search_path);
    Py_CLEAR(interp->codec_search_cache);
    Py_CLEAR(interp->codec_error_registry);
    Py_CLEAR(interp->modules);
    Py_CLEAR(interp->modules_by_index);
    Py_CLEAR(interp->builtins_copy);
    Py_CLEAR(interp->importlib);
    Py_CLEAR(interp->import_func);
    Py_CLEAR(interp->dict);
#ifdef HAVE_FORK
    Py_CLEAR(interp->before_forkers);
    Py_CLEAR(interp->after_forkers_parent);
    Py_CLEAR(interp->after_forkers_child);
#endif

    _PyAST_Fini(interp);
    _PyWarnings_Fini(interp);
    _PyAtExit_Fini(interp);

    // All Python types must be destroyed before the last GC collection. Python
    // types create a reference cycle to themselves in their in their
    // PyTypeObject.tp_mro member (the tuple contains the type).

    /* Last garbage collection on this interpreter */
    _PyGC_CollectNoFail(tstate);
    _PyGC_Fini(interp);

    /* We don't clear sysdict and builtins until the end of this function.
       Because clearing other attributes can execute arbitrary Python code
       which requires sysdict and builtins. */
    PyDict_Clear(interp->sysdict);
    PyDict_Clear(interp->builtins);
    Py_CLEAR(interp->sysdict);
    Py_CLEAR(interp->builtins);

    // XXX Once we have one allocator per interpreter (i.e.
    // per-interpreter GC) we must ensure that all of the interpreter's
    // objects have been cleaned up at the point.
}


void
PyInterpreterState_Clear(PyInterpreterState *interp)
{
    // Use the current Python thread state to call audit hooks and to collect
    // garbage. It can be different than the current Python thread state
    // of 'interp'.
    PyThreadState *current_tstate = _PyThreadState_GET();

    interpreter_clear(interp, current_tstate);
}


void
_PyInterpreterState_Clear(PyThreadState *tstate)
{
    interpreter_clear(tstate->interp, tstate);
}


static void _PyThreadState_Delete(PyThreadState *tstate, int check_current);

static void
zapthreads(PyInterpreterState *interp, int check_current)
{
    PyThreadState *tstate;
    /* No need to lock the mutex here because this should only happen
       when the threads are all really dead (XXX famous last words). */
    while ((tstate = interp->pythreads.head) != NULL) {
        _PyThreadState_Delete(tstate, check_current);
    }
}


void
PyInterpreterState_Delete(PyInterpreterState *interp)
{
    _PyRuntimeState *runtime = interp->runtime;
    struct pyinterpreters *interpreters = &runtime->interpreters;
    zapthreads(interp, 0);

    _PyEval_FiniState(&interp->ceval);

    /* Delete current thread. After this, many C API calls become crashy. */
    _PyThreadState_Swap(&runtime->gilstate, NULL);

    INTERP_HEAD_LOCK(runtime);
    PyInterpreterState **p;
    for (p = &interpreters->head; ; p = &(*p)->next) {
        if (*p == NULL) {
            Py_FatalError("NULL interpreter");
        }
        if (*p == interp) {
            break;
        }
    }
    if (interp->pythreads.head != NULL) {
        Py_FatalError("remaining threads");
    }
    *p = interp->next;

    if (&interpreters->main == interp) {
        if (interpreters->head != NULL) {
            Py_FatalError("remaining subinterpreters");
        }
    }
    INTERP_HEAD_UNLOCK(runtime);

    if (interp->id_refcount >= 0) {
        _PyThread_clear_lock(&interp->id_mutex);
    }

    if (interp->needs_free) {
        PyMem_RawFree(interp);
    }
}


#ifdef HAVE_FORK
/*
 * Delete all interpreter states except the main interpreter.  If there
 * is a current interpreter state, it *must* be the main interpreter.
 */
PyStatus
_PyInterpreterState_DeleteExceptMain(_PyRuntimeState *runtime)
{
    struct _gilstate_runtime_state *gilstate = &runtime->gilstate;
    struct pyinterpreters *interpreters = &runtime->interpreters;

    PyThreadState *tstate = _PyThreadState_Swap(gilstate, NULL);
    PyInterpreterState *main_interp = &interpreters->main;
    if (tstate != NULL && tstate->interp != main_interp) {
        return _PyStatus_ERR("not main interpreter");
    }

    INTERP_HEAD_LOCK(runtime);
    PyInterpreterState *interp = interpreters->head;
    interpreters->head = NULL;
    while (interp != NULL) {
        if (interp == main_interp) {
            interp->next = NULL;
            interpreters->head = interp;
            interp = interp->next;
            continue;
        }

        PyInterpreterState_Clear(interp);  // XXX must activate?
        zapthreads(interp, 1);
        if (interp->id_refcount >= 0) {
            _PyThread_clear_lock(&interp->id_mutex);
        }
        PyInterpreterState *prev_interp = interp;
        interp = interp->next;
        if (prev_interp->needs_free) {
            PyMem_RawFree(prev_interp);
        }
    }
    INTERP_HEAD_UNLOCK(runtime);

    if (interpreters->head == NULL) {
        return _PyStatus_ERR("missing main interpreter");
    }
    _PyThreadState_Swap(gilstate, tstate);
    return _PyStatus_OK();
}
#endif


PyInterpreterState *
PyInterpreterState_Get(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _Py_EnsureTstateNotNULL(tstate);
    PyInterpreterState *interp = tstate->interp;
    if (interp == NULL) {
        Py_FatalError("no current interpreter");
    }
    return interp;
}


int64_t
PyInterpreterState_GetID(PyInterpreterState *interp)
{
    if (interp == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "no interpreter provided");
        return -1;
    }
    assert(interp->id >= 0);
    return interp->id;
}


static PyInterpreterState *
interp_look_up_id(_PyRuntimeState *runtime, int64_t requested_id)
{
    PyInterpreterState *interp = runtime->interpreters.head;
    while (interp != NULL) {
        int64_t id = PyInterpreterState_GetID(interp);
        if (id < 0) {
            return NULL;
        }
        if (requested_id == id) {
            return interp;
        }
        interp = PyInterpreterState_Next(interp);
    }
    return NULL;
}

PyInterpreterState *
_PyInterpreterState_LookUpID(int64_t requested_id)
{
    PyInterpreterState *interp = NULL;
    if (requested_id >= 0) {
        _PyRuntimeState *runtime = &_PyRuntime;
        INTERP_HEAD_LOCK(runtime);
        interp = interp_look_up_id(runtime, requested_id);
        INTERP_HEAD_UNLOCK(runtime);
    }
    if (interp == NULL && !PyErr_Occurred()) {
        PyErr_Format(PyExc_RuntimeError,
                     "unrecognized interpreter ID %lld", requested_id);
    }
    return interp;
}

int
_PyInterpreterState_IDInitref(PyInterpreterState *interp)
{
    if (interp->id_refcount >= 0) {
        return 0;
    }
    if (init_lock_data(&interp->id_mutex) != 0) {
        PyErr_SetString(PyExc_RuntimeError,
                        "failed to create interpreter ID mutex");
        return -1;
    }
    interp->id_refcount = 0;
    return 0;
}


int
_PyInterpreterState_IDIncref(PyInterpreterState *interp)
{
    if (_PyInterpreterState_IDInitref(interp) < 0) {
        return -1;
    }

    PyThread_acquire_lock(&interp->id_mutex, WAIT_LOCK);
    interp->id_refcount += 1;
    PyThread_release_lock(&interp->id_mutex);
    return 0;
}


void
_PyInterpreterState_IDDecref(PyInterpreterState *interp)
{
    assert(interp->id_refcount >= 0);

    struct _gilstate_runtime_state *gilstate = &_PyRuntime.gilstate;
    PyThread_acquire_lock(&interp->id_mutex, WAIT_LOCK);
    assert(interp->id_refcount != 0);
    interp->id_refcount -= 1;
    int64_t refcount = interp->id_refcount;
    PyThread_release_lock(&interp->id_mutex);

    if (refcount == 0 && interp->requires_idref) {
        // XXX Using the "head" thread isn't strictly correct.
        PyThreadState *tstate = PyInterpreterState_ThreadHead(interp);
        // XXX Possible GILState issues?
        PyThreadState *save_tstate = _PyThreadState_Swap(gilstate, tstate);
        Py_EndInterpreter(tstate);
        _PyThreadState_Swap(gilstate, save_tstate);
    }
}

int
_PyInterpreterState_RequiresIDRef(PyInterpreterState *interp)
{
    return interp->requires_idref;
}

void
_PyInterpreterState_RequireIDRef(PyInterpreterState *interp, int required)
{
    interp->requires_idref = required ? 1 : 0;
}

PyObject *
_PyInterpreterState_GetMainModule(PyInterpreterState *interp)
{
    if (interp->modules == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "interpreter not initialized");
        return NULL;
    }
    return PyMapping_GetItemString(interp->modules, "__main__");
}

PyObject *
PyInterpreterState_GetDict(PyInterpreterState *interp)
{
    if (interp->dict == NULL) {
        interp->dict = PyDict_New();
        if (interp->dict == NULL) {
            PyErr_Clear();
        }
    }
    /* Returning NULL means no per-interpreter dict is available. */
    return interp->dict;
}

static void
init_chunk(_PyStackChunk *chunk, int size_in_bytes)
{
    chunk->previous = NULL;
    chunk->size = size_in_bytes;
    chunk->top = 0;
    // We leave chunk->data alone.
}

static _PyStackChunk*
allocate_chunk(int size_in_bytes)
{
    assert(size_in_bytes % sizeof(PyObject **) == 0);
    _PyStackChunk *res = _PyObject_VirtualAlloc(size_in_bytes);
    if (res == NULL) {
        return NULL;
    }
    init_chunk(res, size_in_bytes);
    return res;
}

static void
init_threadstate(PyThreadState *tstate, PyInterpreterState *interp)
{
    tstate->interp = interp;

    tstate->frame = NULL;
    tstate->recursion_depth = 0;
    tstate->recursion_headroom = 0;
    tstate->stackcheck_counter = 0;
    tstate->tracing = 0;
    tstate->root_cframe.use_tracing = 0;
    tstate->cframe = &tstate->root_cframe;
    tstate->gilstate_counter = 0;
    tstate->async_exc = NULL;
    tstate->thread_id = PyThread_get_thread_ident();
#ifdef PY_HAVE_THREAD_NATIVE_ID
    tstate->native_thread_id = PyThread_get_thread_native_id();
#else
    tstate->native_thread_id = 0;
#endif

    tstate->dict = NULL;

    tstate->curexc_type = NULL;
    tstate->curexc_value = NULL;
    tstate->curexc_traceback = NULL;

    tstate->exc_state.exc_type = NULL;
    tstate->exc_state.exc_value = NULL;
    tstate->exc_state.exc_traceback = NULL;
    tstate->exc_state.previous_item = NULL;
    tstate->exc_info = &tstate->exc_state;

    tstate->c_profilefunc = NULL;
    tstate->c_tracefunc = NULL;
    tstate->c_profileobj = NULL;
    tstate->c_traceobj = NULL;

    tstate->trash_delete_nesting = 0;
    tstate->trash_delete_later = NULL;
    tstate->on_delete = NULL;
    tstate->on_delete_data = NULL;

    tstate->coroutine_origin_tracking_depth = 0;

    tstate->async_gen_firstiter = NULL;
    tstate->async_gen_finalizer = NULL;

    tstate->context = NULL;
    tstate->context_ver = 1;

    tstate->datastack_chunk = (_PyStackChunk *)&tstate->datastack_initial;
    Py_ssize_t initial_size = sizeof(tstate->datastack_initial);
    init_chunk(tstate->datastack_chunk, initial_size);
    /* If top points to entry 0, then _PyThreadState_PopFrame will try to pop this chunk */
    tstate->datastack_top = &tstate->datastack_chunk->data[1];
    tstate->datastack_limit = (PyObject **)(((char *)tstate->datastack_chunk) + initial_size);
    /* Mark trace_info as uninitialized */
    tstate->trace_info.code = NULL;
}

static void
set_next_threadstate(PyThreadState *tstate)
{
    assert(tstate->interp && tstate->interp->runtime);
    PyInterpreterState *interp = tstate->interp;

    TSTATE_HEAD_LOCK(interp);
    assert(interp->pythreads.next_id >= 0);
    tstate->id = ++interp->pythreads.next_id;

    tstate->prev = NULL;
    tstate->next = interp->pythreads.head;
    if (tstate->next)
        tstate->next->prev = tstate;
    interp->pythreads.head = tstate;
    TSTATE_HEAD_UNLOCK(interp);

    if (tstate == &interp->pythreads.main) {
        assert(tstate->id == 1);
    }
}

static PyThreadState *
new_threadstate(PyInterpreterState *interp, int init)
{
    if (interp->pythreads.next_id < 0) {
        /* overflow or init_interpreter() not called! */
        PyThreadState *current_tstate = _PyThreadState_GET();
        if (current_tstate != NULL) {
            _PyErr_SetString(current_tstate, PyExc_RuntimeError,
                             "failed to get a threadstate ID");
        }
        return NULL;
    }

    PyThreadState *tstate;
    TSTATE_HEAD_LOCK(interp);
    if (interp->pythreads.next_id == 0) {
        tstate = &interp->pythreads.main;
        assert(!tstate.initializing);
        tstate->initializing = true;
        TSTATE_HEAD_UNLOCK(interp);
        tstate->needs_free = false;
    }
    else {
        TSTATE_HEAD_UNLOCK(interp);
        tstate = (PyThreadState *)PyMem_RawMalloc(sizeof(PyThreadState));
        if (tstate == NULL) {
            return NULL;
        }
        tstate->initializing = true;
        tstate->needs_free = true;
    }

    init_threadstate(tstate, interp);
    if (init) {
        _PyThreadState_Init(tstate);
    }
    set_next_threadstate(tstate);
    tstate->initializing = false;

    return tstate;
}

PyThreadState *
PyThreadState_New(PyInterpreterState *interp)
{
    return new_threadstate(interp, 1);
}

PyThreadState *
_PyThreadState_Prealloc(PyInterpreterState *interp)
{
    return new_threadstate(interp, 0);
}

void
_PyThreadState_Init(PyThreadState *tstate)
{
    _PyGILState_NoteThreadState(&tstate->interp->runtime->gilstate, tstate);
}

PyObject*
PyState_FindModule(struct PyModuleDef* module)
{
    Py_ssize_t index = module->m_base.m_index;
    PyInterpreterState *state = _PyInterpreterState_GET();
    PyObject *res;
    if (module->m_slots) {
        return NULL;
    }
    if (index == 0)
        return NULL;
    if (state->modules_by_index == NULL)
        return NULL;
    if (index >= PyList_GET_SIZE(state->modules_by_index))
        return NULL;
    res = PyList_GET_ITEM(state->modules_by_index, index);
    return res==Py_None ? NULL : res;
}

int
_PyState_AddModule(PyThreadState *tstate, PyObject* module, struct PyModuleDef* def)
{
    if (!def) {
        assert(_PyErr_Occurred(tstate));
        return -1;
    }
    if (def->m_slots) {
        _PyErr_SetString(tstate,
                         PyExc_SystemError,
                         "PyState_AddModule called on module with slots");
        return -1;
    }

    PyInterpreterState *interp = tstate->interp;
    if (!interp->modules_by_index) {
        interp->modules_by_index = PyList_New(0);
        if (!interp->modules_by_index) {
            return -1;
        }
    }

    while (PyList_GET_SIZE(interp->modules_by_index) <= def->m_base.m_index) {
        if (PyList_Append(interp->modules_by_index, Py_None) < 0) {
            return -1;
        }
    }

    Py_INCREF(module);
    return PyList_SetItem(interp->modules_by_index,
                          def->m_base.m_index, module);
}

int
PyState_AddModule(PyObject* module, struct PyModuleDef* def)
{
    if (!def) {
        Py_FatalError("module definition is NULL");
        return -1;
    }

    PyThreadState *tstate = _PyThreadState_GET();
    PyInterpreterState *interp = tstate->interp;
    Py_ssize_t index = def->m_base.m_index;
    if (interp->modules_by_index &&
        index < PyList_GET_SIZE(interp->modules_by_index) &&
        module == PyList_GET_ITEM(interp->modules_by_index, index))
    {
        _Py_FatalErrorFormat(__func__, "module %p already added", module);
        return -1;
    }
    return _PyState_AddModule(tstate, module, def);
}

int
PyState_RemoveModule(struct PyModuleDef* def)
{
    PyThreadState *tstate = _PyThreadState_GET();
    PyInterpreterState *interp = tstate->interp;

    if (def->m_slots) {
        _PyErr_SetString(tstate,
                         PyExc_SystemError,
                         "PyState_RemoveModule called on module with slots");
        return -1;
    }

    Py_ssize_t index = def->m_base.m_index;
    if (index == 0) {
        Py_FatalError("invalid module index");
    }
    if (interp->modules_by_index == NULL) {
        Py_FatalError("Interpreters module-list not accessible.");
    }
    if (index > PyList_GET_SIZE(interp->modules_by_index)) {
        Py_FatalError("Module index out of bounds.");
    }

    Py_INCREF(Py_None);
    return PyList_SetItem(interp->modules_by_index, index, Py_None);
}

// Used by finalize_modules()
void
_PyInterpreterState_ClearModules(PyInterpreterState *interp)
{
    if (!interp->modules_by_index) {
        return;
    }

    Py_ssize_t i;
    for (i = 0; i < PyList_GET_SIZE(interp->modules_by_index); i++) {
        PyObject *m = PyList_GET_ITEM(interp->modules_by_index, i);
        if (PyModule_Check(m)) {
            /* cleanup the saved copy of module dicts */
            PyModuleDef *md = PyModule_GetDef(m);
            if (md) {
                Py_CLEAR(md->m_base.m_copy);
            }
        }
    }

    /* Setting modules_by_index to NULL could be dangerous, so we
       clear the list instead. */
    if (PyList_SetSlice(interp->modules_by_index,
                        0, PyList_GET_SIZE(interp->modules_by_index),
                        NULL)) {
        PyErr_WriteUnraisable(interp->modules_by_index);
    }
}

void
PyThreadState_Clear(PyThreadState *tstate)
{
    int verbose = _PyInterpreterState_GetConfig(tstate->interp)->verbose;

    if (verbose && tstate->frame != NULL) {
        /* bpo-20526: After the main thread calls
           _PyRuntimeState_SetFinalizing() in Py_FinalizeEx(), threads must
           exit when trying to take the GIL. If a thread exit in the middle of
           _PyEval_EvalFrameDefault(), tstate->frame is not reset to its
           previous value. It is more likely with daemon threads, but it can
           happen with regular threads if threading._shutdown() fails
           (ex: interrupted by CTRL+C). */
        fprintf(stderr,
          "PyThreadState_Clear: warning: thread still has a frame\n");
    }

    /* Don't clear tstate->pyframe: it is a borrowed reference */

    Py_CLEAR(tstate->dict);
    Py_CLEAR(tstate->async_exc);

    Py_CLEAR(tstate->curexc_type);
    Py_CLEAR(tstate->curexc_value);
    Py_CLEAR(tstate->curexc_traceback);

    Py_CLEAR(tstate->exc_state.exc_type);
    Py_CLEAR(tstate->exc_state.exc_value);
    Py_CLEAR(tstate->exc_state.exc_traceback);

    /* The stack of exception states should contain just this thread. */
    if (verbose && tstate->exc_info != &tstate->exc_state) {
        fprintf(stderr,
          "PyThreadState_Clear: warning: thread still has a generator\n");
    }

    tstate->c_profilefunc = NULL;
    tstate->c_tracefunc = NULL;
    Py_CLEAR(tstate->c_profileobj);
    Py_CLEAR(tstate->c_traceobj);

    Py_CLEAR(tstate->async_gen_firstiter);
    Py_CLEAR(tstate->async_gen_finalizer);

    Py_CLEAR(tstate->context);

    if (tstate->on_delete != NULL) {
        tstate->on_delete(tstate->on_delete_data);
    }
}


/* Common code for PyThreadState_Delete() and PyThreadState_DeleteCurrent() */
static void
tstate_delete_common(PyThreadState *tstate,
                     struct _gilstate_runtime_state *gilstate)
{
    _Py_EnsureTstateNotNULL(tstate);
    PyInterpreterState *interp = tstate->interp;
    if (interp == NULL) {
        Py_FatalError("NULL interpreter");
    }

    TSTATE_HEAD_LOCK(interp);
    if (tstate->prev) {
        tstate->prev->next = tstate->next;
    }
    else {
        interp->pythreads.head = tstate->next;
    }
    if (tstate->next) {
        tstate->next->prev = tstate->prev;
    }
    TSTATE_HEAD_UNLOCK(interp);

    if (gilstate->autoInterpreterState &&
        PyThread_tss_get(&gilstate->autoTSSkey) == tstate)
    {
        PyThread_tss_set(&gilstate->autoTSSkey, NULL);
    }
    _PyStackChunk *chunk = tstate->datastack_chunk;
    tstate->datastack_chunk = NULL;
    while (chunk != NULL) {
        _PyStackChunk *prev = chunk->previous;
        if (chunk != (_PyStackChunk *)&tstate->datastack_initial) {
            _PyObject_VirtualFree(chunk, chunk->size);
        }
        chunk = prev;
    }
}

#define _PyRuntimeGILState_GetThreadState(gilstate) \
    ((PyThreadState*)_Py_atomic_load_relaxed(&(gilstate)->tstate_current))
#define _PyRuntimeGILState_SetThreadState(gilstate, value) \
    _Py_atomic_store_relaxed(&(gilstate)->tstate_current, \
                             (uintptr_t)(value))

static void
_PyThreadState_Delete(PyThreadState *tstate, int check_current)
{
    struct _gilstate_runtime_state *gilstate = &tstate->interp->runtime->gilstate;
    if (check_current) {
        if (tstate == _PyRuntimeGILState_GetThreadState(gilstate)) {
            _Py_FatalErrorFormat(__func__, "tstate %p is still current", tstate);
        }
    }
    tstate_delete_common(tstate, gilstate);
    if (tstate->needs_free) {
        PyMem_RawFree(tstate);
    }
}


void
PyThreadState_Delete(PyThreadState *tstate)
{
    _PyThreadState_Delete(tstate, 1);
}


void
_PyThreadState_DeleteCurrent(PyThreadState *tstate)
{
    _Py_EnsureTstateNotNULL(tstate);
    struct _gilstate_runtime_state *gilstate = &tstate->interp->runtime->gilstate;
    tstate_delete_common(tstate, gilstate);
    _PyRuntimeGILState_SetThreadState(gilstate, NULL);
    _PyEval_ReleaseLock(tstate);
    if (tstate->needs_free) {
        PyMem_RawFree(tstate);
    }
}

void
PyThreadState_DeleteCurrent(void)
{
    struct _gilstate_runtime_state *gilstate = &_PyRuntime.gilstate;
    PyThreadState *tstate = _PyRuntimeGILState_GetThreadState(gilstate);
    _PyThreadState_DeleteCurrent(tstate);
}


/*
 * Delete all thread states except the one passed as argument.
 * Note that, if there is a current thread state, it *must* be the one
 * passed as argument.  Also, this won't touch any other interpreters
 * than the current one, since we don't know which thread state should
 * be kept in those other interpreters.
 */
void
_PyThreadState_DeleteExcept(_PyRuntimeState *runtime, PyThreadState *tstate)
{
    PyInterpreterState *interp = tstate->interp;

    TSTATE_HEAD_LOCK(interp);
    /* Remove all thread states, except tstate, from the linked list of
       thread states.  This will allow calling PyThreadState_Clear()
       without holding the lock. */
    PyThreadState *list = interp->pythreads.head;
    if (list == tstate) {
        list = tstate->next;
    }
    if (tstate->prev) {
        tstate->prev->next = tstate->next;
    }
    if (tstate->next) {
        tstate->next->prev = tstate->prev;
    }
    tstate->prev = tstate->next = NULL;
    interp->pythreads.head = tstate;
    TSTATE_HEAD_UNLOCK(interp);

    /* Clear and deallocate all stale thread states.  Even if this
       executes Python code, we should be safe since it executes
       in the current thread, not one of the stale threads. */
    PyThreadState *p, *next;
    for (p = list; p; p = next) {
        next = p->next;
        PyThreadState_Clear(p);
        if (p->needs_free) {
            PyMem_RawFree(p);
        }
    }
}


#ifdef EXPERIMENTAL_ISOLATED_SUBINTERPRETERS
PyThreadState*
_PyThreadState_GetTSS(void) {
    return PyThread_tss_get(&_PyRuntime.gilstate.autoTSSkey);
}
#endif


PyThreadState *
_PyThreadState_UncheckedGet(void)
{
    return _PyThreadState_GET();
}


PyThreadState *
PyThreadState_Get(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _Py_EnsureTstateNotNULL(tstate);
    return tstate;
}


static PyThreadState *_PyGILState_GetThisThreadState(struct _gilstate_runtime_state *gilstate);

PyThreadState *
_PyThreadState_Swap(struct _gilstate_runtime_state *gilstate, PyThreadState *newts)
{
#ifdef EXPERIMENTAL_ISOLATED_SUBINTERPRETERS
    PyThreadState *oldts = _PyThreadState_GetTSS();
#else
    PyThreadState *oldts = _PyRuntimeGILState_GetThreadState(gilstate);
#endif

    _PyRuntimeGILState_SetThreadState(gilstate, newts);
    /* It should not be possible for more than one thread state
       to be used for a thread.  Check this the best we can in debug
       builds.
    */
#if defined(Py_DEBUG)
    if (newts) {
        /* This can be called from PyEval_RestoreThread(). Similar
           to it, we need to ensure errno doesn't change.
        */
        int err = errno;
        PyThreadState *check = _PyGILState_GetThisThreadState(gilstate);
        if (check && check->interp == newts->interp && check != newts)
            Py_FatalError("Invalid thread state for this thread");
        errno = err;
    }
#endif
#ifdef EXPERIMENTAL_ISOLATED_SUBINTERPRETERS
    PyThread_tss_set(&gilstate->autoTSSkey, newts);
#endif
    return oldts;
}

PyThreadState *
PyThreadState_Swap(PyThreadState *newts)
{
    return _PyThreadState_Swap(&_PyRuntime.gilstate, newts);
}

/* An extension mechanism to store arbitrary additional per-thread state.
   PyThreadState_GetDict() returns a dictionary that can be used to hold such
   state; the caller should pick a unique key and store its state there.  If
   PyThreadState_GetDict() returns NULL, an exception has *not* been raised
   and the caller should assume no per-thread state is available. */

PyObject *
_PyThreadState_GetDict(PyThreadState *tstate)
{
    assert(tstate != NULL);
    if (tstate->dict == NULL) {
        tstate->dict = PyDict_New();
        if (tstate->dict == NULL) {
            _PyErr_Clear(tstate);
        }
    }
    return tstate->dict;
}


PyObject *
PyThreadState_GetDict(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    if (tstate == NULL) {
        return NULL;
    }
    return _PyThreadState_GetDict(tstate);
}


PyInterpreterState *
PyThreadState_GetInterpreter(PyThreadState *tstate)
{
    assert(tstate != NULL);
    return tstate->interp;
}


PyFrameObject*
PyThreadState_GetFrame(PyThreadState *tstate)
{
    assert(tstate != NULL);
    if (tstate->frame == NULL) {
        return NULL;
    }
    PyFrameObject *frame = _PyFrame_GetFrameObject(tstate->frame);
    if (frame == NULL) {
        PyErr_Clear();
    }
    Py_XINCREF(frame);
    return frame;
}


uint64_t
PyThreadState_GetID(PyThreadState *tstate)
{
    assert(tstate != NULL);
    return tstate->id;
}


/* Asynchronously raise an exception in a thread.
   Requested by Just van Rossum and Alex Martelli.
   To prevent naive misuse, you must write your own extension
   to call this, or use ctypes.  Must be called with the GIL held.
   Returns the number of tstates modified (normally 1, but 0 if `id` didn't
   match any known thread id).  Can be called with exc=NULL to clear an
   existing async exception.  This raises no exceptions. */

int
PyThreadState_SetAsyncExc(unsigned long id, PyObject *exc)
{
    _PyRuntimeState *runtime = &_PyRuntime;
    PyInterpreterState *interp = _PyRuntimeState_GetThreadState(runtime)->interp;

    /* Although the GIL is held, a few C API functions can be called
     * without the GIL held, and in particular some that create and
     * destroy thread and interpreter states.  Those can mutate the
     * list of thread states we're traversing, so to prevent that we lock
     * head_mutex for the duration.
     */
    TSTATE_HEAD_LOCK(interp);
    for (PyThreadState *tstate = interp->pythreads.head; tstate != NULL; tstate = tstate->next) {
        if (tstate->thread_id != id) {
            continue;
        }

        /* Tricky:  we need to decref the current value
         * (if any) in tstate->async_exc, but that can in turn
         * allow arbitrary Python code to run, including
         * perhaps calls to this function.  To prevent
         * deadlock, we need to release head_mutex before
         * the decref.
         */
        PyObject *old_exc = tstate->async_exc;
        Py_XINCREF(exc);
        tstate->async_exc = exc;
        TSTATE_HEAD_UNLOCK(interp);

        Py_XDECREF(old_exc);
        _PyEval_SignalAsyncExc(tstate->interp);
        return 1;
    }
    TSTATE_HEAD_UNLOCK(interp);
    return 0;
}


/* Routines for advanced debuggers, requested by David Beazley.
   Don't use unless you know what you are doing! */

PyInterpreterState *
PyInterpreterState_Head(void)
{
    return _PyRuntime.interpreters.head;
}

PyInterpreterState *
PyInterpreterState_Main(void)
{
    return _Py_GetMainInterpreter();
}

PyInterpreterState *
PyInterpreterState_Next(PyInterpreterState *interp) {
    return interp->next;
}

PyThreadState *
PyInterpreterState_ThreadHead(PyInterpreterState *interp) {
    return interp->pythreads.head;
}

PyThreadState *
PyThreadState_Next(PyThreadState *tstate) {
    return tstate->next;
}

/* The implementation of sys._current_frames().  This is intended to be
   called with the GIL held, as it will be when called via
   sys._current_frames().  It's possible it would work fine even without
   the GIL held, but haven't thought enough about that.
*/
PyObject *
_PyThread_CurrentFrames(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    if (_PySys_Audit(tstate, "sys._current_frames", NULL) < 0) {
        return NULL;
    }

    PyObject *result = PyDict_New();
    if (result == NULL) {
        return NULL;
    }

    /* for i in all interpreters:
     *     for t in all of i's thread states:
     *          if t's frame isn't NULL, map t's id to its frame
     * Because these lists can mutate even when the GIL is held, we
     * need to grab head_mutex for the duration.
     */
    _PyRuntimeState *runtime = tstate->interp->runtime;
    INTERP_HEAD_LOCK(runtime);
    PyInterpreterState *i;
    for (i = runtime->interpreters.head; i != NULL; i = i->next) {
        TSTATE_HEAD_LOCK(i);
        PyThreadState *t;
        for (t = i->pythreads.head; t != NULL; t = t->next) {
            InterpreterFrame *frame = t->frame;
            if (frame == NULL) {
                continue;
            }
            PyObject *id = PyLong_FromUnsignedLong(t->thread_id);
            if (id == NULL) {
                goto fail;
            }
            int stat = PyDict_SetItem(result, id, (PyObject *)_PyFrame_GetFrameObject(frame));
            Py_DECREF(id);
            if (stat < 0) {
                goto fail;
            }
        }
        TSTATE_HEAD_UNLOCK(i);
    }
    goto done;

fail:
    TSTATE_HEAD_UNLOCK(i);
    Py_CLEAR(result);

done:
    INTERP_HEAD_UNLOCK(runtime);
    return result;
}

PyObject *
_PyThread_CurrentExceptions(void)
{
    PyThreadState *tstate = _PyThreadState_GET();

    _Py_EnsureTstateNotNULL(tstate);

    if (_PySys_Audit(tstate, "sys._current_exceptions", NULL) < 0) {
        return NULL;
    }

    PyObject *result = PyDict_New();
    if (result == NULL) {
        return NULL;
    }

    /* for i in all interpreters:
     *     for t in all of i's thread states:
     *          if t's frame isn't NULL, map t's id to its frame
     * Because these lists can mutate even when the GIL is held, we
     * need to grab head_mutex for the duration.
     */
    _PyRuntimeState *runtime = tstate->interp->runtime;
    INTERP_HEAD_LOCK(runtime);
    PyInterpreterState *i;
    for (i = runtime->interpreters.head; i != NULL; i = i->next) {
        TSTATE_HEAD_LOCK(i);
        PyThreadState *t;
        for (t = i->pythreads.head; t != NULL; t = t->next) {
            _PyErr_StackItem *err_info = _PyErr_GetTopmostException(t);
            if (err_info == NULL) {
                continue;
            }
            PyObject *id = PyLong_FromUnsignedLong(t->thread_id);
            if (id == NULL) {
                goto fail;
            }
            PyObject *exc_info = PyTuple_Pack(
                3,
                err_info->exc_type != NULL ? err_info->exc_type : Py_None,
                err_info->exc_value != NULL ? err_info->exc_value : Py_None,
                err_info->exc_traceback != NULL ? err_info->exc_traceback : Py_None);
            if (exc_info == NULL) {
                Py_DECREF(id);
                goto fail;
            }
            int stat = PyDict_SetItem(result, id, exc_info);
            Py_DECREF(id);
            Py_DECREF(exc_info);
            if (stat < 0) {
                goto fail;
            }
        }
        TSTATE_HEAD_UNLOCK(i);
    }
    goto done;

fail:
    TSTATE_HEAD_UNLOCK(i);
    Py_CLEAR(result);

done:
    INTERP_HEAD_UNLOCK(runtime);
    return result;
}

/* Python "auto thread state" API. */

/* Keep this as a static, as it is not reliable!  It can only
   ever be compared to the state for the *current* thread.
   * If not equal, then it doesn't matter that the actual
     value may change immediately after comparison, as it can't
     possibly change to the current thread's state.
   * If equal, then the current thread holds the lock, so the value can't
     change until we yield the lock.
*/
static int
PyThreadState_IsCurrent(PyThreadState *tstate)
{
    /* Must be the tstate for this thread */
    struct _gilstate_runtime_state *gilstate = &_PyRuntime.gilstate;
    assert(_PyGILState_GetThisThreadState(gilstate) == tstate);
    return tstate == _PyRuntimeGILState_GetThreadState(gilstate);
}

/* Internal initialization/finalization functions called by
   Py_Initialize/Py_FinalizeEx
*/
PyStatus
_PyGILState_Init(_PyRuntimeState *runtime)
{
    struct _gilstate_runtime_state *gilstate = &runtime->gilstate;
    if (PyThread_tss_create(&gilstate->autoTSSkey) != 0) {
        return _PyStatus_NO_MEMORY();
    }
    // PyThreadState_New() calls _PyGILState_NoteThreadState() which does
    // nothing before autoInterpreterState is set.
    assert(gilstate->autoInterpreterState == NULL);
    return _PyStatus_OK();
}


PyStatus
_PyGILState_SetTstate(PyThreadState *tstate)
{
    if (!_Py_IsMainInterpreter(tstate->interp)) {
        /* Currently, PyGILState is shared by all interpreters. The main
         * interpreter is responsible to initialize it. */
        return _PyStatus_OK();
    }

    /* must init with valid states */
    assert(tstate != NULL);
    assert(tstate->interp != NULL);

    struct _gilstate_runtime_state *gilstate = &tstate->interp->runtime->gilstate;

    gilstate->autoInterpreterState = tstate->interp;
    assert(PyThread_tss_get(&gilstate->autoTSSkey) == NULL);
    assert(tstate->gilstate_counter == 0);

    _PyGILState_NoteThreadState(gilstate, tstate);
    return _PyStatus_OK();
}

PyInterpreterState *
_PyGILState_GetInterpreterStateUnsafe(void)
{
    return _PyRuntime.gilstate.autoInterpreterState;
}

void
_PyGILState_Fini(PyInterpreterState *interp)
{
    struct _gilstate_runtime_state *gilstate = &interp->runtime->gilstate;
    PyThread_tss_delete(&gilstate->autoTSSkey);
    gilstate->autoInterpreterState = NULL;
}

#ifdef HAVE_FORK
/* Reset the TSS key - called by PyOS_AfterFork_Child().
 * This should not be necessary, but some - buggy - pthread implementations
 * don't reset TSS upon fork(), see issue #10517.
 */
PyStatus
_PyGILState_Reinit(_PyRuntimeState *runtime)
{
    struct _gilstate_runtime_state *gilstate = &runtime->gilstate;
    PyThreadState *tstate = _PyGILState_GetThisThreadState(gilstate);

    PyThread_tss_delete(&gilstate->autoTSSkey);
    if (PyThread_tss_create(&gilstate->autoTSSkey) != 0) {
        return _PyStatus_NO_MEMORY();
    }

    /* If the thread had an associated auto thread state, reassociate it with
     * the new key. */
    if (tstate &&
        PyThread_tss_set(&gilstate->autoTSSkey, (void *)tstate) != 0)
    {
        return _PyStatus_ERR("failed to set autoTSSkey");
    }
    return _PyStatus_OK();
}
#endif

/* When a thread state is created for a thread by some mechanism other than
   PyGILState_Ensure, it's important that the GILState machinery knows about
   it so it doesn't try to create another thread state for the thread (this is
   a better fix for SF bug #1010677 than the first one attempted).
*/
static void
_PyGILState_NoteThreadState(struct _gilstate_runtime_state *gilstate, PyThreadState* tstate)
{
    /* If autoTSSkey isn't initialized, this must be the very first
       threadstate created in Py_Initialize().  Don't do anything for now
       (we'll be back here when _PyGILState_Init is called). */
    if (!gilstate->autoInterpreterState) {
        return;
    }

    /* Stick the thread state for this thread in thread specific storage.

       The only situation where you can legitimately have more than one
       thread state for an OS level thread is when there are multiple
       interpreters.

       You shouldn't really be using the PyGILState_ APIs anyway (see issues
       #10915 and #15751).

       The first thread state created for that given OS level thread will
       "win", which seems reasonable behaviour.
    */
    if (PyThread_tss_get(&gilstate->autoTSSkey) == NULL) {
        if ((PyThread_tss_set(&gilstate->autoTSSkey, (void *)tstate)) != 0) {
            Py_FatalError("Couldn't create autoTSSkey mapping");
        }
    }

    /* PyGILState_Release must not try to delete this thread state. */
    tstate->gilstate_counter = 1;
}

/* The public functions */
static PyThreadState *
_PyGILState_GetThisThreadState(struct _gilstate_runtime_state *gilstate)
{
    if (gilstate->autoInterpreterState == NULL)
        return NULL;
    return (PyThreadState *)PyThread_tss_get(&gilstate->autoTSSkey);
}

PyThreadState *
PyGILState_GetThisThreadState(void)
{
    return _PyGILState_GetThisThreadState(&_PyRuntime.gilstate);
}

int
PyGILState_Check(void)
{
    struct _gilstate_runtime_state *gilstate = &_PyRuntime.gilstate;
    if (!gilstate->check_enabled) {
        return 1;
    }

    if (!PyThread_tss_is_created(&gilstate->autoTSSkey)) {
        return 1;
    }

    PyThreadState *tstate = _PyRuntimeGILState_GetThreadState(gilstate);
    if (tstate == NULL) {
        return 0;
    }

    return (tstate == _PyGILState_GetThisThreadState(gilstate));
}

PyGILState_STATE
PyGILState_Ensure(void)
{
    _PyRuntimeState *runtime = &_PyRuntime;
    struct _gilstate_runtime_state *gilstate = &runtime->gilstate;

    /* Note that we do not auto-init Python here - apart from
       potential races with 2 threads auto-initializing, pep-311
       spells out other issues.  Embedders are expected to have
       called Py_Initialize(). */

    /* Ensure that _PyEval_InitThreads() and _PyGILState_Init() have been
       called by Py_Initialize() */
#ifndef EXPERIMENTAL_ISOLATED_SUBINTERPRETERS
    assert(_PyEval_ThreadsInitialized(runtime));
#endif
    assert(gilstate->autoInterpreterState);

    PyThreadState *tcur = (PyThreadState *)PyThread_tss_get(&gilstate->autoTSSkey);
    int current;
    if (tcur == NULL) {
        /* Create a new Python thread state for this thread */
        tcur = PyThreadState_New(gilstate->autoInterpreterState);
        if (tcur == NULL) {
            Py_FatalError("Couldn't create thread-state for new thread");
        }

        /* This is our thread state!  We'll need to delete it in the
           matching call to PyGILState_Release(). */
        tcur->gilstate_counter = 0;
        current = 0; /* new thread state is never current */
    }
    else {
        current = PyThreadState_IsCurrent(tcur);
    }

    if (current == 0) {
        PyEval_RestoreThread(tcur);
    }

    /* Update our counter in the thread-state - no need for locks:
       - tcur will remain valid as we hold the GIL.
       - the counter is safe as we are the only thread "allowed"
         to modify this value
    */
    ++tcur->gilstate_counter;

    return current ? PyGILState_LOCKED : PyGILState_UNLOCKED;
}

void
PyGILState_Release(PyGILState_STATE oldstate)
{
    _PyRuntimeState *runtime = &_PyRuntime;
    PyThreadState *tstate = PyThread_tss_get(&runtime->gilstate.autoTSSkey);
    if (tstate == NULL) {
        Py_FatalError("auto-releasing thread-state, "
                      "but no thread-state for this thread");
    }

    /* We must hold the GIL and have our thread state current */
    /* XXX - remove the check - the assert should be fine,
       but while this is very new (April 2003), the extra check
       by release-only users can't hurt.
    */
    if (!PyThreadState_IsCurrent(tstate)) {
        _Py_FatalErrorFormat(__func__,
                             "thread state %p must be current when releasing",
                             tstate);
    }
    assert(PyThreadState_IsCurrent(tstate));
    --tstate->gilstate_counter;
    assert(tstate->gilstate_counter >= 0); /* illegal counter value */

    /* If we're going to destroy this thread-state, we must
     * clear it while the GIL is held, as destructors may run.
     */
    if (tstate->gilstate_counter == 0) {
        /* can't have been locked when we created it */
        assert(oldstate == PyGILState_UNLOCKED);
        PyThreadState_Clear(tstate);
        /* Delete the thread-state.  Note this releases the GIL too!
         * It's vital that the GIL be held here, to avoid shutdown
         * races; see bugs 225673 and 1061968 (that nasty bug has a
         * habit of coming back).
         */
        assert(_PyRuntimeGILState_GetThreadState(&runtime->gilstate) == tstate);
        _PyThreadState_DeleteCurrent(tstate);
    }
    /* Release the lock if necessary */
    else if (oldstate == PyGILState_UNLOCKED)
        PyEval_SaveThread();
}


/**************************/
/* cross-interpreter data */
/**************************/

/* cross-interpreter data */

crossinterpdatafunc _PyCrossInterpreterData_Lookup(PyObject *);

/* This is a separate func from _PyCrossInterpreterData_Lookup in order
   to keep the registry code separate. */
static crossinterpdatafunc
_lookup_getdata(PyObject *obj)
{
    crossinterpdatafunc getdata = _PyCrossInterpreterData_Lookup(obj);
    if (getdata == NULL && PyErr_Occurred() == 0)
        PyErr_Format(PyExc_ValueError,
                     "%S does not support cross-interpreter data", obj);
    return getdata;
}

int
_PyObject_CheckCrossInterpreterData(PyObject *obj)
{
    crossinterpdatafunc getdata = _lookup_getdata(obj);
    if (getdata == NULL) {
        return -1;
    }
    return 0;
}

static int
_check_xidata(PyThreadState *tstate, _PyCrossInterpreterData *data)
{
    // data->data can be anything, including NULL, so we don't check it.

    // data->obj may be NULL, so we don't check it.

    if (data->interp < 0) {
        _PyErr_SetString(tstate, PyExc_SystemError, "missing interp");
        return -1;
    }

    if (data->new_object == NULL) {
        _PyErr_SetString(tstate, PyExc_SystemError, "missing new_object func");
        return -1;
    }

    // data->free may be NULL, so we don't check it.

    return 0;
}

int
_PyObject_GetCrossInterpreterData(PyObject *obj, _PyCrossInterpreterData *data)
{
    // PyThreadState_Get() aborts if tstate is NULL.
    PyThreadState *tstate = PyThreadState_Get();
    PyInterpreterState *interp = tstate->interp;

    // Reset data before re-populating.
    *data = (_PyCrossInterpreterData){0};
    data->free = PyMem_RawFree;  // Set a default that may be overridden.

    // Call the "getdata" func for the object.
    Py_INCREF(obj);
    crossinterpdatafunc getdata = _lookup_getdata(obj);
    if (getdata == NULL) {
        Py_DECREF(obj);
        return -1;
    }
    int res = getdata(obj, data);
    Py_DECREF(obj);
    if (res != 0) {
        return -1;
    }

    // Fill in the blanks and validate the result.
    data->interp = interp->id;
    if (_check_xidata(tstate, data) != 0) {
        _PyCrossInterpreterData_Release(data);
        return -1;
    }

    return 0;
}

static void
_release_xidata(void *arg)
{
    _PyCrossInterpreterData *data = (_PyCrossInterpreterData *)arg;
    if (data->free != NULL) {
        data->free(data->data);
    }
    Py_XDECREF(data->obj);
}

static void
_call_in_interpreter(struct _gilstate_runtime_state *gilstate,
                     PyInterpreterState *interp,
                     void (*func)(void *), void *arg)
{
    /* We would use Py_AddPendingCall() if it weren't specific to the
     * main interpreter (see bpo-33608).  In the meantime we take a
     * naive approach.
     */
    PyThreadState *save_tstate = NULL;
    if (interp != _PyRuntimeGILState_GetThreadState(gilstate)->interp) {
        // XXX Using the "head" thread isn't strictly correct.
        PyThreadState *tstate = PyInterpreterState_ThreadHead(interp);
        // XXX Possible GILState issues?
        save_tstate = _PyThreadState_Swap(gilstate, tstate);
    }

    func(arg);

    // Switch back.
    if (save_tstate != NULL) {
        _PyThreadState_Swap(gilstate, save_tstate);
    }
}

void
_PyCrossInterpreterData_Release(_PyCrossInterpreterData *data)
{
    if (data->data == NULL && data->obj == NULL) {
        // Nothing to release!
        return;
    }

    // Switch to the original interpreter.
    PyInterpreterState *interp = _PyInterpreterState_LookUpID(data->interp);
    if (interp == NULL) {
        // The interpreter was already destroyed.
        if (data->free != NULL) {
            // XXX Someone leaked some memory...
        }
        return;
    }

    // "Release" the data and/or the object.
    struct _gilstate_runtime_state *gilstate = &_PyRuntime.gilstate;
    _call_in_interpreter(gilstate, interp, _release_xidata, data);
}

PyObject *
_PyCrossInterpreterData_NewObject(_PyCrossInterpreterData *data)
{
    return data->new_object(data);
}

/* registry of {type -> crossinterpdatafunc} */

/* For now we use a global registry of shareable classes.  An
   alternative would be to add a tp_* slot for a class's
   crossinterpdatafunc. It would be simpler and more efficient. */

static int
_register_xidata(struct _xidregistry *xidregistry, PyTypeObject *cls,
                 crossinterpdatafunc getdata)
{
    // Note that we effectively replace already registered classes
    // rather than failing.
    struct _xidregitem *newhead = PyMem_RawMalloc(sizeof(struct _xidregitem));
    if (newhead == NULL)
        return -1;
    newhead->cls = cls;
    newhead->getdata = getdata;
    newhead->next = xidregistry->head;
    xidregistry->head = newhead;
    return 0;
}

static void _register_builtins_for_crossinterpreter_data(struct _xidregistry *xidregistry);

int
_PyCrossInterpreterData_RegisterClass(PyTypeObject *cls,
                                       crossinterpdatafunc getdata)
{
    if (!PyType_Check(cls)) {
        PyErr_Format(PyExc_ValueError, "only classes may be registered");
        return -1;
    }
    if (getdata == NULL) {
        PyErr_Format(PyExc_ValueError, "missing 'getdata' func");
        return -1;
    }

    // Make sure the class isn't ever deallocated.
    Py_INCREF((PyObject *)cls);

    struct _xidregistry *xidregistry = &_PyRuntime.xidregistry ;
    PyThread_acquire_lock(&xidregistry->mutex, WAIT_LOCK);
    if (xidregistry->head == NULL) {
        _register_builtins_for_crossinterpreter_data(xidregistry);
    }
    int res = _register_xidata(xidregistry, cls, getdata);
    PyThread_release_lock(&xidregistry->mutex);
    return res;
}

/* Cross-interpreter objects are looked up by exact match on the class.
   We can reassess this policy when we move from a global registry to a
   tp_* slot. */

crossinterpdatafunc
_PyCrossInterpreterData_Lookup(PyObject *obj)
{
    struct _xidregistry *xidregistry = &_PyRuntime.xidregistry ;
    PyObject *cls = PyObject_Type(obj);
    crossinterpdatafunc getdata = NULL;
    PyThread_acquire_lock(&xidregistry->mutex, WAIT_LOCK);
    struct _xidregitem *cur = xidregistry->head;
    if (cur == NULL) {
        _register_builtins_for_crossinterpreter_data(xidregistry);
        cur = xidregistry->head;
    }
    for(; cur != NULL; cur = cur->next) {
        if (cur->cls == (PyTypeObject *)cls) {
            getdata = cur->getdata;
            break;
        }
    }
    Py_DECREF(cls);
    PyThread_release_lock(&xidregistry->mutex);
    return getdata;
}

/* cross-interpreter data for builtin types */

struct _shared_bytes_data {
    char *bytes;
    Py_ssize_t len;
};

static PyObject *
_new_bytes_object(_PyCrossInterpreterData *data)
{
    struct _shared_bytes_data *shared = (struct _shared_bytes_data *)(data->data);
    return PyBytes_FromStringAndSize(shared->bytes, shared->len);
}

static int
_bytes_shared(PyObject *obj, _PyCrossInterpreterData *data)
{
    struct _shared_bytes_data *shared = PyMem_NEW(struct _shared_bytes_data, 1);
    if (PyBytes_AsStringAndSize(obj, &shared->bytes, &shared->len) < 0) {
        return -1;
    }
    data->data = (void *)shared;
    Py_INCREF(obj);
    data->obj = obj;  // Will be "released" (decref'ed) when data released.
    data->new_object = _new_bytes_object;
    data->free = PyMem_Free;
    return 0;
}

struct _shared_str_data {
    int kind;
    const void *buffer;
    Py_ssize_t len;
};

static PyObject *
_new_str_object(_PyCrossInterpreterData *data)
{
    struct _shared_str_data *shared = (struct _shared_str_data *)(data->data);
    return PyUnicode_FromKindAndData(shared->kind, shared->buffer, shared->len);
}

static int
_str_shared(PyObject *obj, _PyCrossInterpreterData *data)
{
    struct _shared_str_data *shared = PyMem_NEW(struct _shared_str_data, 1);
    shared->kind = PyUnicode_KIND(obj);
    shared->buffer = PyUnicode_DATA(obj);
    shared->len = PyUnicode_GET_LENGTH(obj);
    data->data = (void *)shared;
    Py_INCREF(obj);
    data->obj = obj;  // Will be "released" (decref'ed) when data released.
    data->new_object = _new_str_object;
    data->free = PyMem_Free;
    return 0;
}

static PyObject *
_new_long_object(_PyCrossInterpreterData *data)
{
    return PyLong_FromSsize_t((Py_ssize_t)(data->data));
}

static int
_long_shared(PyObject *obj, _PyCrossInterpreterData *data)
{
    /* Note that this means the size of shareable ints is bounded by
     * sys.maxsize.  Hence on 32-bit architectures that is half the
     * size of maximum shareable ints on 64-bit.
     */
    Py_ssize_t value = PyLong_AsSsize_t(obj);
    if (value == -1 && PyErr_Occurred()) {
        if (PyErr_ExceptionMatches(PyExc_OverflowError)) {
            PyErr_SetString(PyExc_OverflowError, "try sending as bytes");
        }
        return -1;
    }
    data->data = (void *)value;
    data->obj = NULL;
    data->new_object = _new_long_object;
    data->free = NULL;
    return 0;
}

static PyObject *
_new_none_object(_PyCrossInterpreterData *data)
{
    // XXX Singleton refcounts are problematic across interpreters...
    Py_INCREF(Py_None);
    return Py_None;
}

static int
_none_shared(PyObject *obj, _PyCrossInterpreterData *data)
{
    data->data = NULL;
    // data->obj remains NULL
    data->new_object = _new_none_object;
    data->free = NULL;  // There is nothing to free.
    return 0;
}

static void
_register_builtins_for_crossinterpreter_data(struct _xidregistry *xidregistry)
{
    // None
    if (_register_xidata(xidregistry, (PyTypeObject *)PyObject_Type(Py_None), _none_shared) != 0) {
        Py_FatalError("could not register None for cross-interpreter sharing");
    }

    // int
    if (_register_xidata(xidregistry, &PyLong_Type, _long_shared) != 0) {
        Py_FatalError("could not register int for cross-interpreter sharing");
    }

    // bytes
    if (_register_xidata(xidregistry, &PyBytes_Type, _bytes_shared) != 0) {
        Py_FatalError("could not register bytes for cross-interpreter sharing");
    }

    // str
    if (_register_xidata(xidregistry, &PyUnicode_Type, _str_shared) != 0) {
        Py_FatalError("could not register str for cross-interpreter sharing");
    }
}


_PyFrameEvalFunction
_PyInterpreterState_GetEvalFrameFunc(PyInterpreterState *interp)
{
    return interp->eval_frame;
}


void
_PyInterpreterState_SetEvalFrameFunc(PyInterpreterState *interp,
                                     _PyFrameEvalFunction eval_frame)
{
    interp->eval_frame = eval_frame;
}


const PyConfig*
_PyInterpreterState_GetConfig(PyInterpreterState *interp)
{
    return &interp->config;
}


int
_PyInterpreterState_GetConfigCopy(PyConfig *config)
{
    PyInterpreterState *interp = PyInterpreterState_Get();

    PyStatus status = _PyConfig_Copy(config, &interp->config);
    if (PyStatus_Exception(status)) {
        _PyErr_SetFromPyStatus(status);
        return -1;
    }
    return 0;
}


const PyConfig*
_Py_GetConfig(void)
{
    assert(PyGILState_Check());
    PyThreadState *tstate = _PyThreadState_GET();
    return _PyInterpreterState_GetConfig(tstate->interp);
}

#define MINIMUM_OVERHEAD 1000

static PyObject **
push_chunk(PyThreadState *tstate, int size)
{
    assert(tstate->datastack_top + size >= tstate->datastack_limit);

    int allocate_size = DATA_STACK_CHUNK_SIZE;
    while (allocate_size < (int)sizeof(PyObject*)*(size + MINIMUM_OVERHEAD)) {
        allocate_size *= 2;
    }
    _PyStackChunk *new = allocate_chunk(allocate_size);
    if (new == NULL) {
        return NULL;
    }
    new->previous = tstate->datastack_chunk;
    tstate->datastack_chunk->top = tstate->datastack_top - &tstate->datastack_chunk->data[0];
    tstate->datastack_chunk = new;
    tstate->datastack_limit = (PyObject **)(((char *)new) + allocate_size);
    PyObject **res = &new->data[0];
    tstate->datastack_top = res + size;
    return res;
}

InterpreterFrame *
_PyThreadState_PushFrame(PyThreadState *tstate, PyFrameConstructor *con, PyObject *locals)
{
    PyCodeObject *code = (PyCodeObject *)con->fc_code;
    int nlocalsplus = code->co_nlocalsplus;
    size_t size = nlocalsplus + code->co_stacksize +
        FRAME_SPECIALS_SIZE;
    assert(size < INT_MAX/sizeof(PyObject *));
    PyObject **localsarray = tstate->datastack_top;
    PyObject **top = localsarray + size;
    if (top >= tstate->datastack_limit) {
        localsarray = push_chunk(tstate, (int)size);
        if (localsarray == NULL) {
            return NULL;
        }
    }
    else {
        tstate->datastack_top = top;
    }
    InterpreterFrame * frame = (InterpreterFrame *)(localsarray + nlocalsplus);
    _PyFrame_InitializeSpecials(frame, con, locals, nlocalsplus);
    for (int i=0; i < nlocalsplus; i++) {
        localsarray[i] = NULL;
    }
    return frame;
}

void
_PyThreadState_PopFrame(PyThreadState *tstate, InterpreterFrame * frame)
{
    PyObject **locals = _PyFrame_GetLocalsArray(frame);
    if (locals == &tstate->datastack_chunk->data[0]) {
        _PyStackChunk *chunk = tstate->datastack_chunk;
        _PyStackChunk *previous = chunk->previous;
        tstate->datastack_top = &previous->data[previous->top];
        tstate->datastack_chunk = previous;
        if (chunk != (_PyStackChunk *)&tstate->datastack_initial) {
            _PyObject_VirtualFree(chunk, chunk->size);
        }
        tstate->datastack_limit = (PyObject **)(((char *)previous) + previous->size);
    }
    else {
        assert(tstate->datastack_top >= locals);
        tstate->datastack_top = locals;
    }
}


#ifdef __cplusplus
}
#endif
