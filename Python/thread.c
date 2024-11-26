
/* Thread package.
   This is intended to be usable independently from Python.
   The implementation for system foobar is in a file thread_foobar.h
   which is included by this file dependent on config settings.
   Stuff shared by all thread_*.h files is collected here. */

#include "Python.h"
#include "pycore_ceval.h"         // _PyEval_MakePendingCalls()
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "pycore_structseq.h"     // _PyStructSequence_FiniBuiltin()
#include "pycore_pylifecycle.h"   // _PyThreadState_SetCurrent()
#include "pycore_pythread.h"      // _POSIX_THREADS

#ifndef DONT_HAVE_STDIO_H
#  include <stdio.h>
#endif

#include <stdlib.h>


// ThreadError is just an alias to PyExc_RuntimeError
#define ThreadError PyExc_RuntimeError


// Define PY_TIMEOUT_MAX constant.
#ifdef _POSIX_THREADS
   // PyThread_acquire_lock_timed() uses (us * 1000) to convert microseconds
   // to nanoseconds.
#  define PY_TIMEOUT_MAX_VALUE (LLONG_MAX / 1000)
#elif defined (NT_THREADS)
   // WaitForSingleObject() accepts timeout in milliseconds in the range
   // [0; 0xFFFFFFFE] (DWORD type). INFINITE value (0xFFFFFFFF) means no
   // timeout. 0xFFFFFFFE milliseconds is around 49.7 days.
#  if 0xFFFFFFFELL < LLONG_MAX / 1000
#    define PY_TIMEOUT_MAX_VALUE (0xFFFFFFFELL * 1000)
#  else
#    define PY_TIMEOUT_MAX_VALUE LLONG_MAX
#  endif
#else
#  define PY_TIMEOUT_MAX_VALUE LLONG_MAX
#endif
const long long PY_TIMEOUT_MAX = PY_TIMEOUT_MAX_VALUE;


static void PyThread__init_thread(void); /* Forward */

#define initialized _PyRuntime.threads.initialized

void
PyThread_init_thread(void)
{
    if (initialized) {
        return;
    }
    initialized = 1;
    PyThread__init_thread();
}

#if defined(HAVE_PTHREAD_STUBS)
#   define PYTHREAD_NAME "pthread-stubs"
#   include "thread_pthread_stubs.h"
#elif defined(_USE_PTHREADS)  /* AKA _PTHREADS */
#   if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
#     define PYTHREAD_NAME "pthread-stubs"
#   else
#     define PYTHREAD_NAME "pthread"
#   endif
#   include "thread_pthread.h"
#elif defined(NT_THREADS)
#   define PYTHREAD_NAME "nt"
#   include "thread_nt.h"
#else
#   error "Require native threads. See https://bugs.python.org/issue31370"
#endif


/* return the current thread stack size */
size_t
PyThread_get_stacksize(void)
{
    return _PyInterpreterState_GET()->threads.stacksize;
}

/* Only platforms defining a THREAD_SET_STACKSIZE() macro
   in thread_<platform>.h support changing the stack size.
   Return 0 if stack size is valid,
      -1 if stack size value is invalid,
      -2 if setting stack size is not supported. */
int
PyThread_set_stacksize(size_t size)
{
#if defined(THREAD_SET_STACKSIZE)
    return THREAD_SET_STACKSIZE(size);
#else
    return -2;
#endif
}


int
PyThread_ParseTimeoutArg(PyObject *arg, int blocking, PY_TIMEOUT_T *timeout_p)
{
    assert(_PyTime_FromSeconds(-1) == PyThread_UNSET_TIMEOUT);
    if (arg == NULL || arg == Py_None) {
        *timeout_p = blocking ? PyThread_UNSET_TIMEOUT : 0;
        return 0;
    }
    if (!blocking) {
        PyErr_SetString(PyExc_ValueError,
                        "can't specify a timeout for a non-blocking call");
        return -1;
    }

    PyTime_t timeout;
    if (_PyTime_FromSecondsObject(&timeout, arg, _PyTime_ROUND_TIMEOUT) < 0) {
        return -1;
    }
    if (timeout < 0) {
        PyErr_SetString(PyExc_ValueError,
                        "timeout value must be a non-negative number");
        return -1;
    }

    if (_PyTime_AsMicroseconds(timeout,
                               _PyTime_ROUND_TIMEOUT) > PY_TIMEOUT_MAX) {
        PyErr_SetString(PyExc_OverflowError,
                        "timeout value is too large");
        return -1;
    }
    *timeout_p = timeout;
    return 0;
}

PyLockStatus
PyThread_acquire_lock_timed_with_retries(PyThread_type_lock lock,
                                         PY_TIMEOUT_T timeout)
{
    PyThreadState *tstate = _PyThreadState_GET();
    PyTime_t endtime = 0;
    if (timeout > 0) {
        endtime = _PyDeadline_Init(timeout);
    }

    PyLockStatus r;
    do {
        PyTime_t microseconds;
        microseconds = _PyTime_AsMicroseconds(timeout, _PyTime_ROUND_CEILING);

        /* first a simple non-blocking try without releasing the GIL */
        r = PyThread_acquire_lock_timed(lock, 0, 0);
        if (r == PY_LOCK_FAILURE && microseconds != 0) {
            Py_BEGIN_ALLOW_THREADS
            r = PyThread_acquire_lock_timed(lock, microseconds, 1);
            Py_END_ALLOW_THREADS
        }

        if (r == PY_LOCK_INTR) {
            /* Run signal handlers if we were interrupted.  Propagate
             * exceptions from signal handlers, such as KeyboardInterrupt, by
             * passing up PY_LOCK_INTR.  */
            if (_PyEval_MakePendingCalls(tstate) < 0) {
                return PY_LOCK_INTR;
            }

            /* If we're using a timeout, recompute the timeout after processing
             * signals, since those can take time.  */
            if (timeout > 0) {
                timeout = _PyDeadline_Get(endtime);

                /* Check for negative values, since those mean block forever.
                 */
                if (timeout < 0) {
                    r = PY_LOCK_FAILURE;
                }
            }
        }
    } while (r == PY_LOCK_INTR);  /* Retry if we were interrupted. */

    return r;
}


/* Thread Specific Storage (TSS) API

   Cross-platform components of TSS API implementation.
*/

Py_tss_t *
PyThread_tss_alloc(void)
{
    Py_tss_t *new_key = (Py_tss_t *)PyMem_RawMalloc(sizeof(Py_tss_t));
    if (new_key == NULL) {
        return NULL;
    }
    new_key->_is_initialized = 0;
    return new_key;
}

void
PyThread_tss_free(Py_tss_t *key)
{
    if (key != NULL) {
        PyThread_tss_delete(key);
        PyMem_RawFree((void *)key);
    }
}

int
PyThread_tss_is_created(Py_tss_t *key)
{
    assert(key != NULL);
    return key->_is_initialized;
}


PyDoc_STRVAR(threadinfo__doc__,
"sys.thread_info\n\
\n\
A named tuple holding information about the thread implementation.");

static PyStructSequence_Field threadinfo_fields[] = {
    {"name",    "name of the thread implementation"},
    {"lock",    "name of the lock implementation"},
    {"version", "name and version of the thread library"},
    {0}
};

static PyStructSequence_Desc threadinfo_desc = {
    "sys.thread_info",           /* name */
    threadinfo__doc__,           /* doc */
    threadinfo_fields,           /* fields */
    3
};

static PyTypeObject ThreadInfoType;

PyObject*
PyThread_GetInfo(void)
{
    PyObject *threadinfo, *value;
    int pos = 0;
#if (defined(_POSIX_THREADS) && defined(HAVE_CONFSTR) \
     && defined(_CS_GNU_LIBPTHREAD_VERSION))
    char buffer[255];
    int len;
#endif

    PyInterpreterState *interp = _PyInterpreterState_GET();
    if (_PyStructSequence_InitBuiltin(interp, &ThreadInfoType, &threadinfo_desc) < 0) {
        return NULL;
    }

    threadinfo = PyStructSequence_New(&ThreadInfoType);
    if (threadinfo == NULL)
        return NULL;

    value = PyUnicode_FromString(PYTHREAD_NAME);
    if (value == NULL) {
        Py_DECREF(threadinfo);
        return NULL;
    }
    PyStructSequence_SET_ITEM(threadinfo, pos++, value);

#ifdef HAVE_PTHREAD_STUBS
    value = Py_NewRef(Py_None);
#elif defined(_POSIX_THREADS)
#ifdef USE_SEMAPHORES
    value = PyUnicode_FromString("semaphore");
#else
    value = PyUnicode_FromString("mutex+cond");
#endif
    if (value == NULL) {
        Py_DECREF(threadinfo);
        return NULL;
    }
#else
    value = Py_NewRef(Py_None);
#endif
    PyStructSequence_SET_ITEM(threadinfo, pos++, value);

#if (defined(_POSIX_THREADS) && defined(HAVE_CONFSTR) \
     && defined(_CS_GNU_LIBPTHREAD_VERSION))
    value = NULL;
    len = confstr(_CS_GNU_LIBPTHREAD_VERSION, buffer, sizeof(buffer));
    if (1 < len && (size_t)len < sizeof(buffer)) {
        value = PyUnicode_DecodeFSDefaultAndSize(buffer, len-1);
        if (value == NULL)
            PyErr_Clear();
    }
    if (value == NULL)
#endif
    {
        value = Py_NewRef(Py_None);
    }
    PyStructSequence_SET_ITEM(threadinfo, pos++, value);
    return threadinfo;
}


void
_PyThread_FiniType(PyInterpreterState *interp)
{
    _PyStructSequence_FiniBuiltin(interp, &ThreadInfoType);
}


/* the PyThread_handle_t struct */

// Handles state transitions according to the following diagram:
//
//     NOT_STARTED -> STARTING -> RUNNING -> DONE
//                       |                    ^
//                       |                    |
//                       +----- error --------+
typedef enum {
    THREAD_HANDLE_NOT_STARTED = 1,
    THREAD_HANDLE_STARTING = 2,
    THREAD_HANDLE_RUNNING = 3,
    THREAD_HANDLE_DONE = 4,
} _PyThread_handle_state_t;

// A handle to wait for thread completion.
//
// This may be used to wait for threads that were spawned by the threading
// module as well as for the "main" thread of the threading module. In the
// former case an OS thread, identified by the `os_handle` field, will be
// associated with the handle. The handle "owns" this thread and ensures that
// the thread is either joined or detached after the handle is destroyed.
//
// Joining the handle is idempotent; the underlying OS thread, if any, is
// joined or detached only once. Concurrent join operations are serialized
// until it is their turn to execute or an earlier operation completes
// successfully. Once a join has completed successfully all future joins
// complete immediately.
//
// This must be separately reference counted because it may be destroyed
// in `thread_run()` after the PyThreadState has been destroyed.
struct pythread_handle {
    struct llist_node node;  // linked list node (see _pythread_runtime_state)

    // linked list node (see thread_module_state)
    struct llist_node shutdown_node;

    int wait_at_shutdown;
    // The `ident`, `os_handle`, `has_os_handle`, and `state` fields are
    // protected by `mutex`.
    PyThread_ident_t ident;
    PyThread_os_handle_t os_handle;
    int has_os_handle;

    _PyThread_handle_state_t state;

    PyMutex mutex;

    // Set immediately before `thread_run` returns to indicate that the OS
    // thread is about to exit. This is used to avoid false positives when
    // detecting self-join attempts. See the comment in `_PyThreadHandle_Join()`
    // for a more detailed explanation.
    PyEvent thread_is_exiting;

    // Serializes calls to `join` and `thandle_set_done`.
    _PyOnceFlag once;

    Py_ssize_t refcount;
};

static inline _PyThread_handle_state_t
thandle_get_state(PyThread_handle_t *handle)
{
    PyMutex_Lock(&handle->mutex);
    _PyThread_handle_state_t state = handle->state;
    PyMutex_Unlock(&handle->mutex);
    return state;
}

static inline void
thandle_set_state(PyThread_handle_t *handle, _PyThread_handle_state_t state)
{
    PyMutex_Lock(&handle->mutex);
    handle->state = state;
    PyMutex_Unlock(&handle->mutex);
}

static inline PyThread_ident_t
thandle_get_ident(PyThread_handle_t *handle)
{
    PyMutex_Lock(&handle->mutex);
    PyThread_ident_t ident = handle->ident;
    PyMutex_Unlock(&handle->mutex);
    return ident;
}

static inline int
thandle_get_os_handle(PyThread_handle_t *handle, PyThread_os_handle_t *os_handle)
{
    PyMutex_Lock(&handle->mutex);
    int has_os_handle = handle->has_os_handle;
    if (has_os_handle) {
        *os_handle = handle->os_handle;
    }
    PyMutex_Unlock(&handle->mutex);
    return has_os_handle;
}

static inline void
thandle_incref(PyThread_handle_t *self)
{
    _Py_atomic_add_ssize(&self->refcount, 1);
}

static inline Py_ssize_t
thandle_decref(PyThread_handle_t *self)
{
    return _Py_atomic_add_ssize(&self->refcount, -1) - 1;
}

static void
thandle_mark_running(PyThread_handle_t *self,
                     PyThread_ident_t ident, PyThread_os_handle_t os_handle)
{
    PyMutex_Lock(&self->mutex);
    assert(self->state == THREAD_HANDLE_STARTING);
    self->ident = ident;
    self->has_os_handle = 1;
    self->os_handle = os_handle;
    self->state = THREAD_HANDLE_RUNNING;
    PyMutex_Unlock(&self->mutex);
}

static inline int
thandle_is_exiting(PyThread_handle_t *handle)
{
    return _PyEvent_IsSet(&handle->thread_is_exiting);
}

static inline void
thandle_set_exiting(PyThread_handle_t *handle)
{
    _PyEvent_Notify(&handle->thread_is_exiting);
}

static inline int
thandle_wait_for_exiting(PyThread_handle_t *handle, PyTime_t timeout_ns)
{
    int detach = 1;
    return PyEvent_WaitTimed(&handle->thread_is_exiting, timeout_ns, detach);
}

static int
thandle_force_done(PyThread_handle_t *handle)
{
    assert(thandle_get_state(handle) == THREAD_HANDLE_STARTING);
    thandle_set_exiting(handle);
    thandle_set_state(handle, THREAD_HANDLE_DONE);
    return 0;
}


/* PyThread_handle_t linked lists */

static inline void
init_handles(PyThread_handles_t *handles)
{
    PyMutex_Lock(&handles->mutex);
    llist_init(&handles->head);
    PyMutex_Unlock(&handles->mutex);
}

void
clear_handles(PyThread_handles_t *handles)
{
    PyMutex_Lock(&handles->mutex);
    struct llist_node *node;
    llist_for_each_safe(node, &handles->head) {
        llist_remove(node);
    }
    PyMutex_Unlock(&handles->mutex);
}

static inline void
add_global_handle(PyThread_handle_t *handle)
{
    PyThread_handles_t *handles = &_PyRuntime.threads.handles;
    PyMutex_Lock(&handles->mutex);
    llist_insert_tail(&handles->head, &handle->node);
    PyMutex_Unlock(&handles->mutex);
}

static inline void
remove_global_handle(PyThread_handle_t *handle)
{
    PyThread_handles_t *handles = &_PyRuntime.threads.handles;
    PyMutex_Lock(&handles->mutex);
    if (handle->node.next != NULL) {
        llist_remove(&handle->node);
    }
    PyMutex_Unlock(&handles->mutex);
}

static inline void
add_shutdown_handle(PyThread_handles_t *handles, PyThread_handle_t *handle)
{
    PyMutex_Lock(&handles->mutex);
    llist_insert_tail(&handles->head, &handle->shutdown_node);
    PyMutex_Unlock(&handles->mutex);
}

static inline void
remove_shutdown_handle(PyThread_handles_t *handles, PyThread_handle_t *handle)
{
    PyMutex_Lock(&handles->mutex);
    if (handle->shutdown_node.next != NULL) {
        llist_remove(&handle->shutdown_node);
    }
    PyMutex_Unlock(&handles->mutex);
}

static PyThread_handle_t *
next_other_shutdown_handle(PyThread_handles_t *handles, PyThread_ident_t ident)
{
    PyThread_handle_t *handle = NULL;

    PyMutex_Lock(&handles->mutex);
    struct llist_node *node;
    llist_for_each_safe(node, &handles->head) {
        PyThread_handle_t *cur = llist_data(node, PyThread_handle_t, shutdown_node);
        if (cur->ident != ident) {
            thandle_incref(cur);
            handle = cur;
            break;
        }
    }
    PyMutex_Unlock(&handles->mutex);

    return handle;
}


/* running in a thread */

// bootstate is used to "bootstrap" new threads. Any arguments needed by
// `thread_run()`, which can only take a single argument due to platform
// limitations, are contained in bootstate.
struct bootstate {
    PyThreadState *tstate;
    PyObject *func;
    PyObject *args;
    PyObject *kwargs;
    PyThread_handle_t *handle;
    PyEvent handle_ready;
};

static void thandle_release(PyThread_handle_t *);

static void
thread_bootstate_free(struct bootstate *boot, int decref)
{
    if (decref) {
        Py_DECREF(boot->func);
        Py_DECREF(boot->args);
        Py_XDECREF(boot->kwargs);
    }
    thandle_release(boot->handle);
    PyMem_RawFree(boot);
}

static void
thread_run(void *boot_raw)
{
    struct bootstate *boot = (struct bootstate *) boot_raw;
    PyThreadState *tstate = boot->tstate;

    // Wait until the handle is marked as running
    PyEvent_Wait(&boot->handle_ready);

    // `handle` needs to be manipulated after bootstate has been freed
    PyThread_handle_t *handle = boot->handle;
    thandle_incref(handle);

    // gh-108987: If _thread.start_new_thread() is called before or while
    // Python is being finalized, thread_run() can called *after*.
    // _PyRuntimeState_SetFinalizing() is called. At this point, all Python
    // threads must exit, except of the thread calling Py_Finalize() which
    // holds the GIL and must not exit.
    //
    // At this stage, tstate can be a dangling pointer (point to freed memory),
    // it's ok to call _PyThreadState_MustExit() with a dangling pointer.
    if (_PyThreadState_MustExit(tstate)) {
        // Don't call PyThreadState_Clear() nor _PyThreadState_DeleteCurrent().
        // These functions are called on tstate indirectly by Py_Finalize()
        // which calls _PyInterpreterState_Clear().
        //
        // Py_DECREF() cannot be called because the GIL is not held: leak
        // references on purpose. Python is being finalized anyway.
        thread_bootstate_free(boot, 0);
        goto exit;
    }

    _PyThreadState_Bind(tstate);
    PyEval_AcquireThread(tstate);
    _Py_atomic_add_ssize(&tstate->interp->threads.count, 1);

    PyObject *res = PyObject_Call(boot->func, boot->args, boot->kwargs);
    if (res == NULL) {
        if (PyErr_ExceptionMatches(PyExc_SystemExit))
            /* SystemExit is ignored silently */
            PyErr_Clear();
        else {
            PyErr_FormatUnraisable(
                "Exception ignored in thread started by %R", boot->func);
        }
    }
    else {
        Py_DECREF(res);
    }

    thread_bootstate_free(boot, 1);

    _Py_atomic_add_ssize(&tstate->interp->threads.count, -1);
    PyThreadState_Clear(tstate);
    _PyThreadState_DeleteCurrent(tstate);

exit:
    // Don't need to wait for this thread anymore
    // XXX Passing NULL isn't right.
    remove_shutdown_handle(NULL, handle);

    thandle_set_exiting(handle);
    thandle_release(handle);

    // bpo-44434: Don't call explicitly PyThread_exit_thread(). On Linux with
    // the glibc, pthread_exit() can abort the whole process if dlopen() fails
    // to open the libgcc_s.so library (ex: EMFILE error).
    return;
}


/* OS thread operations for handles */

static int
thandle_start(PyThread_handle_t *self, struct bootstate *boot)
{
    assert(boot->handle == self);

    // Mark the handle as starting to prevent any other threads from doing so
    PyMutex_Lock(&self->mutex);
    if (self->state != THREAD_HANDLE_NOT_STARTED) {
        PyMutex_Unlock(&self->mutex);
        PyErr_SetString(ThreadError, "thread already started");
        return -1;
    }
    self->state = THREAD_HANDLE_STARTING;
    PyMutex_Unlock(&self->mutex);

    // Do all the heavy lifting outside of the mutex. All other operations on
    // the handle should fail since the handle is in the starting state.

    // gh-109795: Use PyMem_RawMalloc() instead of PyMem_Malloc(),
    // because it should be possible to call thread_bootstate_free()
    // without holding the GIL.

    PyThread_ident_t ident;
    PyThread_os_handle_t os_handle;
    if (PyThread_start_joinable_thread(thread_run, boot, &ident, &os_handle)) {
        PyErr_SetString(ThreadError, "can't start new thread");
        _PyOnceFlag_CallOnce(
                &self->once, (_Py_once_fn_t *)thandle_force_done, self);
        return -1;
    }

    thandle_mark_running(self, ident, os_handle);

    // Unblock the thread
    _PyEvent_Notify(&boot->handle_ready);

    return 0;
}

static int
thandle_join(PyThread_handle_t *handle)
{
    assert(thandle_get_state(handle) == THREAD_HANDLE_RUNNING);
    PyThread_os_handle_t os_handle;
    if (thandle_get_os_handle(handle, &os_handle)) {
        int err = 0;
        Py_BEGIN_ALLOW_THREADS
        err = PyThread_join_thread(os_handle);
        Py_END_ALLOW_THREADS
        if (err) {
            PyErr_SetString(ThreadError, "Failed joining thread");
            return -1;
        }
    }
    thandle_set_state(handle, THREAD_HANDLE_DONE);
    return 0;
}

static int
thandle_detach_thread(PyThread_handle_t *self)
{
    if (!self->has_os_handle) {
        return 0;
    }
    // This is typically short so no need to release the GIL
    if (PyThread_detach_thread(self->os_handle)) {
        fprintf(stderr, "thandle_detach_thread: failed detaching thread\n");
        return -1;
    }
    return 0;
}


// NB: This may be called after the PyThreadState in `thread_run` has been
// deleted; it cannot call anything that relies on a valid PyThreadState
// existing.
static void
thandle_release(PyThread_handle_t *self)
{
    if (thandle_decref(self) > 0) {
        return;
    }

    remove_global_handle(self);

    assert(self->shutdown_node.next == NULL);

    // It's safe to access state non-atomically:
    //   1. This is the destructor; nothing else holds a reference.
    //   2. The refcount going to zero is a "synchronizes-with" event; all
    //      changes from other threads are visible.
    if (self->state == THREAD_HANDLE_RUNNING && !thandle_detach_thread(self)) {
        self->state = THREAD_HANDLE_DONE;
    }

    PyMem_RawFree(self);
}


/* PyThread_handle_t API functions */

PyThread_handle_t *
_PyThreadHandle_New(int wait_at_shutdown)
{
    PyThread_handle_t *self =
        (PyThread_handle_t *)PyMem_RawCalloc(1, sizeof(PyThread_handle_t));
    if (self == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    *self = (PyThread_handle_t){
        .wait_at_shutdown = wait_at_shutdown,
        .state = THREAD_HANDLE_NOT_STARTED,
        .refcount = 1,
    };

    add_global_handle(self);

    return self;
}

PyThread_handle_t *
_PyThreadHandle_FromIdent(PyThread_ident_t ident)
{
    PyThread_handle_t *self = _PyThreadHandle_New(0);
    if (self == NULL) {
        return NULL;
    }
    PyMutex_Lock(&self->mutex);
    self->ident = ident;
    self->state = THREAD_HANDLE_RUNNING;
    PyMutex_Unlock(&self->mutex);
    return self;
}

PyThread_handle_t *
_PyThreadHandle_NewRef(PyThread_handle_t *handle)
{
    thandle_incref(handle);
    return handle;
}

void
_PyThreadHandle_Release(PyThread_handle_t *self)
{
    thandle_release(self);
}

int
_PyThreadHandle_GetWaitAtShutdown(PyThread_handle_t *handle)
{
    return handle->wait_at_shutdown;
}

void
_PyThreadHandle_SetWaitAtShutdown(PyThread_handle_t *handle,
                                  int wait_at_shutdown)
{
    handle->wait_at_shutdown = wait_at_shutdown;
}

PyThread_ident_t
_PyThreadHandle_GetIdent(PyThread_handle_t *handle)
{
    return thandle_get_ident(handle);
}

PyThread_handles_t *
_PyThreadHandles_New(void)
{
    PyThread_handles_t *handles = PyMem_RawMalloc(sizeof(PyThread_handles_t));
    if (handles == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    *handles = (PyThread_handles_t){0};
    init_handles(handles);
    return handles;
}

void
_PyThreadHandles_Free(PyThread_handles_t *handles)
{
    clear_handles(handles);
    PyMem_RawFree(handles);
}

void
_PyThread_AddShutdownHandle(PyThread_handles_t *handles,
                            PyThread_handle_t *handle)
{
    add_shutdown_handle(handles, handle);
}

void
_PyThread_RemoveShutdownHandle(PyThread_handles_t *handles,
                               PyThread_handle_t *handle)
{
    remove_shutdown_handle(handles, handle);
}

int
_PyThreadHandle_IsExiting(PyThread_handle_t *handle)
{
    return thandle_is_exiting(handle);
}

void
_PyThread_AfterFork(struct _pythread_runtime_state *state)
{
    // gh-115035: We mark ThreadHandles as not joinable early in the child's
    // after-fork handler. We do this before calling any Python code to ensure
    // that it happens before any ThreadHandles are deallocated, such as by a
    // GC cycle.
    PyThread_ident_t current = PyThread_get_thread_ident_ex();

    PyThread_handles_t *handles = &state->handles;

    handles->mutex = (PyMutex){_Py_UNLOCKED};
    // The current thread is the only one, so we don't need to actually
    // acquire the lock.

    struct llist_node *node;
    llist_for_each_safe(node, &handles->head) {
        PyThread_handle_t *handle = llist_data(node, PyThread_handle_t, node);
        if (handle->ident == current) {
            continue;
        }

        // Mark all threads as done. Any attempts to join or detach the
        // underlying OS thread (if any) could crash. We are the only thread;
        // it's safe to set this non-atomically.
        handle->state = THREAD_HANDLE_DONE;
        handle->once = (_PyOnceFlag){_Py_ONCE_INITIALIZED};
        handle->mutex = (PyMutex){_Py_UNLOCKED};
        thandle_set_exiting(handle);
        llist_remove(node);
        // XXX NULL isn't right.
        remove_shutdown_handle(NULL, handle);
    }
}

int
_PyThreadHandle_Start(PyThread_handle_t *handle,
                        PyObject *func, PyObject *args, PyObject *kwargs)
{
    PyInterpreterState *interp = _PyInterpreterState_GET();
    if (!_PyInterpreterState_HasFeature(interp, Py_RTFLAGS_THREADS)) {
        PyErr_SetString(PyExc_RuntimeError,
                        "thread is not supported for isolated subinterpreters");
        return -1;
    }
    if (_PyInterpreterState_GetFinalizing(interp) != NULL) {
        PyErr_SetString(PyExc_PythonFinalizationError,
                        "can't create new thread at interpreter shutdown");
        return -1;
    }

    PyThreadState *tstate = _PyThreadState_New(
                                interp, _PyThreadState_WHENCE_THREADING);
    if (tstate == NULL) {
        if (!PyErr_Occurred()) {
            PyErr_NoMemory();
        }
        return -1;
    }

    // gh-109795: Use PyMem_RawMalloc() instead of PyMem_Malloc(),
    // because it should be possible to call thread_bootstate_free()
    // without holding the GIL.
    struct bootstate *boot = PyMem_RawMalloc(sizeof(struct bootstate));
    if (boot == NULL) {
        PyThreadState_Clear(tstate);
        PyThreadState_Delete(tstate);
        PyErr_NoMemory();
        return -1;
    }
    *boot = (struct bootstate){
        .tstate = tstate,
        .func = Py_NewRef(func),
        .args = Py_NewRef(args),
        .kwargs = Py_XNewRef(kwargs),
        .handle = _PyThreadHandle_NewRef(handle),
        .handle_ready = (PyEvent){0},
    };

    if (thandle_start(handle, boot) < 0) {
        PyThreadState_Clear(boot->tstate);
        PyThreadState_Delete(boot->tstate);
        thread_bootstate_free(boot, 1);
        return -1;
    }

    return 0;
}


/* waiting for threads to finish */

static int
thandle_check_started(PyThread_handle_t *self)
{
    _PyThread_handle_state_t state = thandle_get_state(self);
    if (state < THREAD_HANDLE_RUNNING) {
        PyErr_SetString(ThreadError, "thread not started");
        return -1;
    }
    return 0;
}

int
_PyThreadHandle_Join(PyThread_handle_t *self, PyTime_t timeout_ns)
{
    if (thandle_check_started(self) < 0) {
        return -1;
    }

    // We want to perform this check outside of the `_PyOnceFlag` to prevent
    // deadlock in the scenario where another thread joins us and we then
    // attempt to join ourselves. However, it's not safe to check thread
    // identity once the handle's os thread has finished. We may end up reusing
    // the identity stored in the handle and erroneously think we are
    // attempting to join ourselves.
    //
    // To work around this, we set `thread_is_exiting` immediately before
    // `thread_run` returns.  We can be sure that we are not attempting to join
    // ourselves if the handle's thread is about to exit.
    if (!thandle_is_exiting(self) &&
        thandle_get_ident(self) == PyThread_get_thread_ident_ex()) {
        // PyThread_join_thread() would deadlock or error out.
        PyErr_SetString(ThreadError, "Cannot join current thread");
        return -1;
    }

    // Wait until the deadline for the thread to exit.
    PyTime_t deadline = timeout_ns != -1 ? _PyDeadline_Init(timeout_ns) : 0;
    while (!thandle_wait_for_exiting(self, timeout_ns)) {
        if (deadline) {
            // _PyDeadline_Get will return a negative value if the deadline has
            // been exceeded.
            timeout_ns = Py_MAX(_PyDeadline_Get(deadline), 0);
        }

        if (timeout_ns) {
            // Interrupted
            if (Py_MakePendingCalls() < 0) {
                return -1;
            }
        }
        else {
            // Timed out
            return 0;
        }
    }

    if (_PyOnceFlag_CallOnce(&self->once, (_Py_once_fn_t *)thandle_join,
                             self) == -1) {
        return -1;
    }
    assert(thandle_get_state(self) == THREAD_HANDLE_DONE);
    return 0;
}

static int
thandle_set_done(PyThread_handle_t *handle)
{
    assert(thandle_get_state(handle) == THREAD_HANDLE_RUNNING);
    if (thandle_detach_thread(handle) < 0) {
        PyErr_SetString(ThreadError, "failed detaching handle");
        return -1;
    }
    thandle_set_exiting(handle);
    thandle_set_state(handle, THREAD_HANDLE_DONE);
    return 0;
}

int
_PyThreadHandle_SetDone(PyThread_handle_t *self)
{
    if (thandle_check_started(self) < 0) {
        return -1;
    }

    if (_PyOnceFlag_CallOnce(&self->once, (_Py_once_fn_t *)thandle_set_done, self) ==
        -1) {
        return -1;
    }
    assert(thandle_get_state(self) == THREAD_HANDLE_DONE);
    return 0;
}

int
_PyThread_Shutdown(PyThread_handles_t *handles)
{
    PyThread_ident_t ident = PyThread_get_thread_ident_ex();

    for (;;) {
        PyThread_handle_t *handle = next_other_shutdown_handle(handles, ident);
        if (!handle) {
            // No more threads to wait on!
            break;
        }

        // Wait for the thread to finish. If we're interrupted, such
        // as by a ctrl-c we print the error and exit early.
        if (_PyThreadHandle_Join(handle, -1) < 0) {
            PyErr_WriteUnraisable(NULL);
            _PyThreadHandle_Release(handle);
            return 0;
        }

        _PyThreadHandle_Release(handle);
    }
    return 1;
}
