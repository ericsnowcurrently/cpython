/* Thread module */
/* Interface to Sjoerd's portable C thread library */

#include "Python.h"
#include "pycore_interp.h"        // _PyInterpreterState.threads.count
#include "pycore_lock.h"
#include "pycore_moduleobject.h"  // _PyModule_GetState()
#include "pycore_modsupport.h"    // _PyArg_NoKeywords()
#include "pycore_pylifecycle.h"   // _PyErr_Display()
#include "pycore_pystate.h"       // _PyThreadState_SetCurrent()
#include "pycore_sysmodule.h"     // _PySys_GetAttr()
#include "pycore_time.h"          // _PyTime_FromSeconds()
#include "pycore_weakref.h"       // _PyWeakref_GET_REF()

#include <stddef.h>               // offsetof()
#ifdef HAVE_SIGNAL_H
#  include <signal.h>             // SIGINT
#endif


// ThreadError is just an alias to PyExc_RuntimeError
#define ThreadError PyExc_RuntimeError

// Forward declarations
static struct PyModuleDef thread_module;

// Module state
typedef struct {
    PyTypeObject *excepthook_type;
    PyTypeObject *lock_type;
    PyTypeObject *local_type;
    PyTypeObject *local_dummy_type;
    PyTypeObject *thread_handle_type;

    // Set of handles to all non-daemon threads created by the
    // threading module. We wait for these to finish at shutdown.
    PyThread_handles_t *shutdown_handles;
} thread_module_state;

static inline thread_module_state*
get_thread_state(PyObject *module)
{
    void *state = _PyModule_GetState(module);
    assert(state != NULL);
    return (thread_module_state *)state;
}

static void
init_shutdown_handles(thread_module_state *state)
{
    state->shutdown_handles = _PyThreadHandles_New();
}

static inline void
clear_shutdown_handles(thread_module_state *state)
{
    if (state->shutdown_handles == NULL) {
        return;
    }
    _PyThreadHandles_Free(state->shutdown_handles);
    state->shutdown_handles = NULL;
}

static inline void
add_to_shutdown_handles(thread_module_state *state, PyThread_handle_t *handle)
{
    _PyThread_AddShutdownHandle(state->shutdown_handles, handle);
}

static inline void
remove_from_shutdown_handles(thread_module_state *state,
                             PyThread_handle_t *handle)
{
    _PyThread_RemoveShutdownHandle(state->shutdown_handles, handle);
}


// A wrapper around a PyThread_handle_t.
typedef struct {
    PyObject_HEAD

    PyThread_handle_t *handle;
} PyThreadHandleObject;

static PyThreadHandleObject *
PyThreadHandleObject_new(PyTypeObject *type, PyThread_handle_t *handle)
{
    PyThreadHandleObject *self =
        (PyThreadHandleObject *)type->tp_alloc(type, 0);
    if (self == NULL) {
        return NULL;
    }

    self->handle = _PyThreadHandle_NewRef(handle);

    return self;
}

static PyObject *
PyThreadHandleObject_tp_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    static char *keywords[] = {"wait_at_shutdown", NULL};
    int wait_at_shutdown = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "$p:ThreadHandle.__new__", keywords,
                                     &wait_at_shutdown)) {
        return NULL;
    }

    PyThread_handle_t *handle = _PyThreadHandle_New(wait_at_shutdown);
    if (handle == NULL) {
        return NULL;
    }
    PyThreadHandleObject *self = PyThreadHandleObject_new(type, handle);
    _PyThreadHandle_Release(handle);
    return (PyObject *)self;
}

static int
PyThreadHandleObject_traverse(PyThreadHandleObject *self, visitproc visit,
                              void *arg)
{
    Py_VISIT(Py_TYPE(self));
    return 0;
}

static void
PyThreadHandleObject_dealloc(PyObject *op)
{
    PyThreadHandleObject *self = (PyThreadHandleObject*)op;
    PyObject_GC_UnTrack(self);
    PyTypeObject *tp = Py_TYPE(self);
    _PyThreadHandle_Release(self->handle);
    tp->tp_free(self);
    Py_DECREF(tp);
}

static PyObject *
PyThreadHandleObject_repr(PyObject *op)
{
    PyThreadHandleObject *self = (PyThreadHandleObject*)op;
    PyThread_ident_t ident = _PyThreadHandle_GetIdent(self->handle);
    return PyUnicode_FromFormat("<%s object: ident=%" PY_FORMAT_THREAD_IDENT_T ">",
                                Py_TYPE(self)->tp_name, ident);
}

static PyObject *
PyThreadHandleObject_get_ident(PyObject *op, void *Py_UNUSED(ignored))
{
    PyThreadHandleObject *self = (PyThreadHandleObject*)op;
    return PyLong_FromUnsignedLongLong(_PyThreadHandle_GetIdent(self->handle));
}

static PyObject *
PyThreadHandleObject_get_wait_at_shutdown(PyObject *op,
                                          void *Py_UNUSED(ignored))
{
    PyThreadHandleObject *self = (PyThreadHandleObject*)op;
    return PyBool_FromLong(_PyThreadHandle_GetWaitAtShutdown(self->handle));
}

static int
PyThreadHandleObject_set_wait_at_shutdown(PyObject *op, PyObject *value,
                                          void *Py_UNUSED(ignored))
{
    int wait_at_shutdown = PyObject_IsTrue(value);
    if (wait_at_shutdown < 0) {
        return -1;
    }
    PyThreadHandleObject *self = (PyThreadHandleObject*)op;
    _PyThreadHandle_SetWaitAtShutdown(self->handle, wait_at_shutdown);
    return 0;
}

static PyObject *
PyThreadHandleObject_join(PyObject *op, PyObject *args)
{
    PyThreadHandleObject *self = (PyThreadHandleObject*)op;

    PyObject *timeout_obj = NULL;
    if (!PyArg_ParseTuple(args, "|O:join", &timeout_obj)) {
        return NULL;
    }

    PyTime_t timeout_ns = -1;
    if (timeout_obj != NULL && timeout_obj != Py_None) {
        if (_PyTime_FromSecondsObject(&timeout_ns, timeout_obj,
                                      _PyTime_ROUND_TIMEOUT) < 0) {
            return NULL;
        }
    }

    if (_PyThreadHandle_Join(self->handle, timeout_ns) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
PyThreadHandleObject_is_done(PyObject *op, PyObject *Py_UNUSED(ignored))
{
    PyThreadHandleObject *self = (PyThreadHandleObject*)op;
    if (_PyThreadHandle_IsExiting(self->handle)) {
        Py_RETURN_TRUE;
    }
    else {
        Py_RETURN_FALSE;
    }
}

static PyObject *
PyThreadHandleObject_set_done(PyObject *op, PyObject *Py_UNUSED(ignored))
{
    PyThreadHandleObject *self = (PyThreadHandleObject*)op;
    if (_PyThreadHandle_SetDone(self->handle) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyGetSetDef ThreadHandle_getsetlist[] = {
    {"ident", PyThreadHandleObject_get_ident, NULL, NULL},
    {"wait_at_shutdown", PyThreadHandleObject_get_wait_at_shutdown,
                         PyThreadHandleObject_set_wait_at_shutdown,
                         NULL},
    {0},
};

static PyMethodDef ThreadHandle_methods[] = {
    {"join", PyThreadHandleObject_join, METH_VARARGS, NULL},
    {"_set_done", PyThreadHandleObject_set_done, METH_NOARGS, NULL},
    {"is_done", PyThreadHandleObject_is_done, METH_NOARGS, NULL},
    {0, 0}
};

static PyType_Slot ThreadHandle_Type_slots[] = {
    {Py_tp_dealloc, PyThreadHandleObject_dealloc},
    {Py_tp_repr, PyThreadHandleObject_repr},
    {Py_tp_getset, ThreadHandle_getsetlist},
    {Py_tp_traverse, PyThreadHandleObject_traverse},
    {Py_tp_methods, ThreadHandle_methods},
    {Py_tp_new, PyThreadHandleObject_tp_new},
    {0, 0}
};

static PyType_Spec ThreadHandle_Type_spec = {
    "_thread._ThreadHandle",
    sizeof(PyThreadHandleObject),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE | Py_TPFLAGS_HAVE_GC,
    ThreadHandle_Type_slots,
};

/* Lock objects */

typedef struct {
    PyObject_HEAD
    PyMutex lock;
} lockobject;

static int
lock_traverse(PyObject *self, visitproc visit, void *arg)
{
    Py_VISIT(Py_TYPE(self));
    return 0;
}

static void
lock_dealloc(PyObject *op)
{
    lockobject *self = (lockobject*)op;
    PyObject_GC_UnTrack(self);
    PyObject_ClearWeakRefs((PyObject *) self);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free((PyObject*)self);
    Py_DECREF(tp);
}


static int
lock_acquire_parse_args(PyObject *args, PyObject *kwds,
                        PyTime_t *timeout)
{
    char *kwlist[] = {"blocking", "timeout", NULL};
    int blocking = 1;
    PyObject *timeout_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|pO:acquire", kwlist,
                                     &blocking, &timeout_obj))
        return -1;

    // XXX Use PyThread_ParseTimeoutArg().

    const PyTime_t unset_timeout = _PyTime_FromSeconds(-1);
    *timeout = unset_timeout;

    if (timeout_obj
        && _PyTime_FromSecondsObject(timeout,
                                     timeout_obj, _PyTime_ROUND_TIMEOUT) < 0)
        return -1;

    if (!blocking && *timeout != unset_timeout ) {
        PyErr_SetString(PyExc_ValueError,
                        "can't specify a timeout for a non-blocking call");
        return -1;
    }
    if (*timeout < 0 && *timeout != unset_timeout) {
        PyErr_SetString(PyExc_ValueError,
                        "timeout value must be a non-negative number");
        return -1;
    }
    if (!blocking)
        *timeout = 0;
    else if (*timeout != unset_timeout) {
        PyTime_t microseconds;

        microseconds = _PyTime_AsMicroseconds(*timeout, _PyTime_ROUND_TIMEOUT);
        if (microseconds > PY_TIMEOUT_MAX) {
            PyErr_SetString(PyExc_OverflowError,
                            "timeout value is too large");
            return -1;
        }
    }
    return 0;
}

static PyObject *
lock_PyThread_acquire_lock(PyObject *op, PyObject *args, PyObject *kwds)
{
    lockobject *self = (lockobject*)op;

    PyTime_t timeout;
    if (lock_acquire_parse_args(args, kwds, &timeout) < 0) {
        return NULL;
    }

    PyLockStatus r = _PyMutex_LockTimed(&self->lock, timeout,
                                        _PY_LOCK_HANDLE_SIGNALS | _PY_LOCK_DETACH);
    if (r == PY_LOCK_INTR) {
        return NULL;
    }

    return PyBool_FromLong(r == PY_LOCK_ACQUIRED);
}

PyDoc_STRVAR(acquire_doc,
"acquire($self, /, blocking=True, timeout=-1)\n\
--\n\
\n\
Lock the lock.  Without argument, this blocks if the lock is already\n\
locked (even by the same thread), waiting for another thread to release\n\
the lock, and return True once the lock is acquired.\n\
With an argument, this will only block if the argument is true,\n\
and the return value reflects whether the lock is acquired.\n\
The blocking operation is interruptible.");

PyDoc_STRVAR(acquire_lock_doc,
"acquire_lock($self, /, blocking=True, timeout=-1)\n\
--\n\
\n\
An obsolete synonym of acquire().");

PyDoc_STRVAR(enter_doc,
"__enter__($self, /)\n\
--\n\
\n\
Lock the lock.");

static PyObject *
lock_PyThread_release_lock(PyObject *op, PyObject *Py_UNUSED(ignored))
{
    lockobject *self = (lockobject*)op;
    /* Sanity check: the lock must be locked */
    if (_PyMutex_TryUnlock(&self->lock) < 0) {
        PyErr_SetString(ThreadError, "release unlocked lock");
        return NULL;
    }

    Py_RETURN_NONE;
}

PyDoc_STRVAR(release_doc,
"release($self, /)\n\
--\n\
\n\
Release the lock, allowing another thread that is blocked waiting for\n\
the lock to acquire the lock.  The lock must be in the locked state,\n\
but it needn't be locked by the same thread that unlocks it.");

PyDoc_STRVAR(release_lock_doc,
"release_lock($self, /)\n\
--\n\
\n\
An obsolete synonym of release().");

PyDoc_STRVAR(lock_exit_doc,
"__exit__($self, /, *exc_info)\n\
--\n\
\n\
Release the lock.");

static PyObject *
lock_locked_lock(PyObject *op, PyObject *Py_UNUSED(ignored))
{
    lockobject *self = (lockobject*)op;
    return PyBool_FromLong(PyMutex_IsLocked(&self->lock));
}

PyDoc_STRVAR(locked_doc,
"locked($self, /)\n\
--\n\
\n\
Return whether the lock is in the locked state.");

PyDoc_STRVAR(locked_lock_doc,
"locked_lock($self, /)\n\
--\n\
\n\
An obsolete synonym of locked().");

static PyObject *
lock_repr(PyObject *op)
{
    lockobject *self = (lockobject*)op;
    return PyUnicode_FromFormat("<%s %s object at %p>",
        PyMutex_IsLocked(&self->lock) ? "locked" : "unlocked", Py_TYPE(self)->tp_name, self);
}

#ifdef HAVE_FORK
static PyObject *
lock__at_fork_reinit(PyObject *op, PyObject *Py_UNUSED(args))
{
    lockobject *self = (lockobject *)op;
    _PyMutex_at_fork_reinit(&self->lock);
    Py_RETURN_NONE;
}
#endif  /* HAVE_FORK */

static lockobject *newlockobject(PyObject *module);

static PyObject *
lock_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    // convert to AC?
    if (!_PyArg_NoKeywords("lock", kwargs)) {
        goto error;
    }
    if (!_PyArg_CheckPositional("lock", PyTuple_GET_SIZE(args), 0, 0)) {
        goto error;
    }

    PyObject *module = PyType_GetModuleByDef(type, &thread_module);
    assert(module != NULL);
    return (PyObject *)newlockobject(module);

error:
    return NULL;
}


static PyMethodDef lock_methods[] = {
    {"acquire_lock", _PyCFunction_CAST(lock_PyThread_acquire_lock),
     METH_VARARGS | METH_KEYWORDS, acquire_lock_doc},
    {"acquire",      _PyCFunction_CAST(lock_PyThread_acquire_lock),
     METH_VARARGS | METH_KEYWORDS, acquire_doc},
    {"release_lock", lock_PyThread_release_lock,
     METH_NOARGS, release_lock_doc},
    {"release",      lock_PyThread_release_lock,
     METH_NOARGS, release_doc},
    {"locked_lock",  lock_locked_lock,
     METH_NOARGS, locked_lock_doc},
    {"locked",       lock_locked_lock,
     METH_NOARGS, locked_doc},
    {"__enter__",    _PyCFunction_CAST(lock_PyThread_acquire_lock),
     METH_VARARGS | METH_KEYWORDS, enter_doc},
    {"__exit__",    lock_PyThread_release_lock,
     METH_VARARGS, lock_exit_doc},
#ifdef HAVE_FORK
    {"_at_fork_reinit", lock__at_fork_reinit,
     METH_NOARGS, NULL},
#endif
    {NULL,           NULL}              /* sentinel */
};

PyDoc_STRVAR(lock_doc,
"lock()\n\
--\n\
\n\
A lock object is a synchronization primitive.  To create a lock,\n\
call threading.Lock().  Methods are:\n\
\n\
acquire() -- lock the lock, possibly blocking until it can be obtained\n\
release() -- unlock of the lock\n\
locked() -- test whether the lock is currently locked\n\
\n\
A lock is not owned by the thread that locked it; another thread may\n\
unlock it.  A thread attempting to lock a lock that it has already locked\n\
will block until another thread unlocks it.  Deadlocks may ensue.");

static PyType_Slot lock_type_slots[] = {
    {Py_tp_dealloc, lock_dealloc},
    {Py_tp_repr, lock_repr},
    {Py_tp_doc, (void *)lock_doc},
    {Py_tp_methods, lock_methods},
    {Py_tp_traverse, lock_traverse},
    {Py_tp_new, lock_new},
    {0, 0}
};

static PyType_Spec lock_type_spec = {
    .name = "_thread.lock",
    .basicsize = sizeof(lockobject),
    .flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
              Py_TPFLAGS_IMMUTABLETYPE | Py_TPFLAGS_MANAGED_WEAKREF),
    .slots = lock_type_slots,
};

/* Recursive lock objects */

typedef struct {
    PyObject_HEAD
    _PyRecursiveMutex lock;
} rlockobject;

static int
rlock_traverse(rlockobject *self, visitproc visit, void *arg)
{
    Py_VISIT(Py_TYPE(self));
    return 0;
}


static void
rlock_dealloc(PyObject *op)
{
    rlockobject *self = (rlockobject*)op;
    PyObject_GC_UnTrack(self);
    PyObject_ClearWeakRefs((PyObject *) self);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
    Py_DECREF(tp);
}


static PyObject *
rlock_acquire(PyObject *op, PyObject *args, PyObject *kwds)
{
    rlockobject *self = (rlockobject*)op;
    PyTime_t timeout;

    if (lock_acquire_parse_args(args, kwds, &timeout) < 0) {
        return NULL;
    }

    PyLockStatus r = _PyRecursiveMutex_LockTimed(&self->lock, timeout,
                                                 _PY_LOCK_HANDLE_SIGNALS | _PY_LOCK_DETACH);
    if (r == PY_LOCK_INTR) {
        return NULL;
    }

    return PyBool_FromLong(r == PY_LOCK_ACQUIRED);
}

PyDoc_STRVAR(rlock_acquire_doc,
"acquire($self, /, blocking=True, timeout=-1)\n\
--\n\
\n\
Lock the lock.  `blocking` indicates whether we should wait\n\
for the lock to be available or not.  If `blocking` is False\n\
and another thread holds the lock, the method will return False\n\
immediately.  If `blocking` is True and another thread holds\n\
the lock, the method will wait for the lock to be released,\n\
take it and then return True.\n\
(note: the blocking operation is interruptible.)\n\
\n\
In all other cases, the method will return True immediately.\n\
Precisely, if the current thread already holds the lock, its\n\
internal counter is simply incremented. If nobody holds the lock,\n\
the lock is taken and its internal counter initialized to 1.");

PyDoc_STRVAR(rlock_enter_doc,
"__enter__($self, /)\n\
--\n\
\n\
Lock the lock.");

static PyObject *
rlock_release(PyObject *op, PyObject *Py_UNUSED(ignored))
{
    rlockobject *self = (rlockobject*)op;

    if (_PyRecursiveMutex_TryUnlock(&self->lock) < 0) {
        PyErr_SetString(PyExc_RuntimeError,
                        "cannot release un-acquired lock");
        return NULL;
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(rlock_release_doc,
"release($self, /)\n\
--\n\
\n\
Release the lock, allowing another thread that is blocked waiting for\n\
the lock to acquire the lock.  The lock must be in the locked state,\n\
and must be locked by the same thread that unlocks it; otherwise a\n\
`RuntimeError` is raised.\n\
\n\
Do note that if the lock was acquire()d several times in a row by the\n\
current thread, release() needs to be called as many times for the lock\n\
to be available for other threads.");

PyDoc_STRVAR(rlock_exit_doc,
"__exit__($self, /, *exc_info)\n\
--\n\
\n\
Release the lock.");

static PyObject *
rlock_acquire_restore(PyObject *op, PyObject *args)
{
    rlockobject *self = (rlockobject*)op;
    PyThread_ident_t owner;
    Py_ssize_t count;

    if (!PyArg_ParseTuple(args, "(n" Py_PARSE_THREAD_IDENT_T "):_acquire_restore",
            &count, &owner))
        return NULL;

    _PyRecursiveMutex_Lock(&self->lock);
    _Py_atomic_store_ullong_relaxed(&self->lock.thread, owner);
    self->lock.level = (size_t)count - 1;
    Py_RETURN_NONE;
}

PyDoc_STRVAR(rlock_acquire_restore_doc,
"_acquire_restore($self, state, /)\n\
--\n\
\n\
For internal use by `threading.Condition`.");

static PyObject *
rlock_release_save(PyObject *op, PyObject *Py_UNUSED(ignored))
{
    rlockobject *self = (rlockobject*)op;

    if (!_PyRecursiveMutex_IsLockedByCurrentThread(&self->lock)) {
        PyErr_SetString(PyExc_RuntimeError,
                        "cannot release un-acquired lock");
        return NULL;
    }

    PyThread_ident_t owner = self->lock.thread;
    Py_ssize_t count = self->lock.level + 1;
    self->lock.level = 0;  // ensure the unlock releases the lock
    _PyRecursiveMutex_Unlock(&self->lock);
    return Py_BuildValue("n" Py_PARSE_THREAD_IDENT_T, count, owner);
}

PyDoc_STRVAR(rlock_release_save_doc,
"_release_save($self, /)\n\
--\n\
\n\
For internal use by `threading.Condition`.");

static PyObject *
rlock_recursion_count(PyObject *op, PyObject *Py_UNUSED(ignored))
{
    rlockobject *self = (rlockobject*)op;
    if (_PyRecursiveMutex_IsLockedByCurrentThread(&self->lock)) {
        return PyLong_FromSize_t(self->lock.level + 1);
    }
    return PyLong_FromLong(0);
}

PyDoc_STRVAR(rlock_recursion_count_doc,
"_recursion_count($self, /)\n\
--\n\
\n\
For internal use by reentrancy checks.");

static PyObject *
rlock_is_owned(PyObject *op, PyObject *Py_UNUSED(ignored))
{
    rlockobject *self = (rlockobject*)op;
    long owned = _PyRecursiveMutex_IsLockedByCurrentThread(&self->lock);
    return PyBool_FromLong(owned);
}

PyDoc_STRVAR(rlock_is_owned_doc,
"_is_owned($self, /)\n\
--\n\
\n\
For internal use by `threading.Condition`.");

static PyObject *
rlock_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    rlockobject *self = (rlockobject *) type->tp_alloc(type, 0);
    if (self == NULL) {
        return NULL;
    }
    self->lock = (_PyRecursiveMutex){0};
    return (PyObject *) self;
}

static PyObject *
rlock_repr(PyObject *op)
{
    rlockobject *self = (rlockobject*)op;
    PyThread_ident_t owner = self->lock.thread;
    size_t count = self->lock.level + 1;
    return PyUnicode_FromFormat(
        "<%s %s object owner=%" PY_FORMAT_THREAD_IDENT_T " count=%zu at %p>",
        owner ? "locked" : "unlocked",
        Py_TYPE(self)->tp_name, owner,
        count, self);
}


#ifdef HAVE_FORK
static PyObject *
rlock__at_fork_reinit(rlockobject *self, PyObject *Py_UNUSED(args))
{
    self->lock = (_PyRecursiveMutex){0};
    Py_RETURN_NONE;
}
#endif  /* HAVE_FORK */


static PyMethodDef rlock_methods[] = {
    {"acquire",      _PyCFunction_CAST(rlock_acquire),
     METH_VARARGS | METH_KEYWORDS, rlock_acquire_doc},
    {"release",      rlock_release,
     METH_NOARGS, rlock_release_doc},
    {"_is_owned",     rlock_is_owned,
     METH_NOARGS, rlock_is_owned_doc},
    {"_acquire_restore", rlock_acquire_restore,
     METH_VARARGS, rlock_acquire_restore_doc},
    {"_release_save", rlock_release_save,
     METH_NOARGS, rlock_release_save_doc},
    {"_recursion_count", rlock_recursion_count,
     METH_NOARGS, rlock_recursion_count_doc},
    {"__enter__",    _PyCFunction_CAST(rlock_acquire),
     METH_VARARGS | METH_KEYWORDS, rlock_enter_doc},
    {"__exit__",    rlock_release,
     METH_VARARGS, rlock_exit_doc},
#ifdef HAVE_FORK
    {"_at_fork_reinit",    (PyCFunction)rlock__at_fork_reinit,
     METH_NOARGS, NULL},
#endif
    {NULL,           NULL}              /* sentinel */
};


static PyType_Slot rlock_type_slots[] = {
    {Py_tp_dealloc, rlock_dealloc},
    {Py_tp_repr, rlock_repr},
    {Py_tp_methods, rlock_methods},
    {Py_tp_alloc, PyType_GenericAlloc},
    {Py_tp_new, rlock_new},
    {Py_tp_traverse, rlock_traverse},
    {0, 0},
};

static PyType_Spec rlock_type_spec = {
    .name = "_thread.RLock",
    .basicsize = sizeof(rlockobject),
    .flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE |
              Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_IMMUTABLETYPE | Py_TPFLAGS_MANAGED_WEAKREF),
    .slots = rlock_type_slots,
};

static lockobject *
newlockobject(PyObject *module)
{
    thread_module_state *state = get_thread_state(module);

    PyTypeObject *type = state->lock_type;
    lockobject *self = (lockobject *)type->tp_alloc(type, 0);
    if (self == NULL) {
        return NULL;
    }
    self->lock = (PyMutex){0};
    return self;
}

/* Thread-local objects */

/* Quick overview:

   We need to be able to reclaim reference cycles as soon as possible
   (both when a thread is being terminated, or a thread-local object
    becomes unreachable from user data).  Constraints:
   - it must not be possible for thread-state dicts to be involved in
     reference cycles (otherwise the cyclic GC will refuse to consider
     objects referenced from a reachable thread-state dict, even though
     local_dealloc would clear them)
   - the death of a thread-state dict must still imply destruction of the
     corresponding local dicts in all thread-local objects.

   Our implementation uses small "localdummy" objects in order to break
   the reference chain. These trivial objects are hashable (using the
   default scheme of identity hashing) and weakrefable.

   Each thread-state holds two separate localdummy objects:

   - `threading_local_key` is used as a key to retrieve the locals dictionary
     for the thread in any `threading.local` object.
   - `threading_local_sentinel` is used to signal when a thread is being
     destroyed. Consequently, the associated thread-state must hold the only
     reference.

   Each `threading.local` object contains a dict mapping localdummy keys to
   locals dicts and a set containing weak references to localdummy
   sentinels. Each sentinel weak reference has a callback that removes itself
   and the locals dict for the key from the `threading.local` object when
   called.

   Therefore:
   - The thread-state only holds strong references to localdummy objects, which
     cannot participate in cycles.
   - Only outside objects (application- or library-level) hold strong
     references to the thread-local objects.
   - As soon as thread-state's sentinel dummy is destroyed the callbacks for
     all weakrefs attached to the sentinel are called, and destroy the
     corresponding local dicts from thread-local objects.
   - As soon as a thread-local object is destroyed, its local dicts are
     destroyed.
   - The GC can do its work correctly when a thread-local object is dangling,
     without any interference from the thread-state dicts.

   This dual key arrangement is necessary to ensure that `threading.local`
   values can be retrieved from finalizers. If we were to only keep a mapping
   of localdummy weakrefs to locals dicts it's possible that the weakrefs would
   be cleared before finalizers were called (GC currently clears weakrefs that
   are garbage before invoking finalizers), causing lookups in finalizers to
   fail.
*/

typedef struct {
    PyObject_HEAD
    PyObject *weakreflist;      /* List of weak references to self */
} localdummyobject;

static void
localdummy_dealloc(PyObject *op)
{
    localdummyobject *self = (localdummyobject*)op;
    if (self->weakreflist != NULL)
        PyObject_ClearWeakRefs((PyObject *) self);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free((PyObject*)self);
    Py_DECREF(tp);
}

static PyMemberDef local_dummy_type_members[] = {
    {"__weaklistoffset__", Py_T_PYSSIZET, offsetof(localdummyobject, weakreflist), Py_READONLY},
    {NULL},
};

static PyType_Slot local_dummy_type_slots[] = {
    {Py_tp_dealloc, localdummy_dealloc},
    {Py_tp_doc, "Thread-local dummy"},
    {Py_tp_members, local_dummy_type_members},
    {0, 0}
};

static PyType_Spec local_dummy_type_spec = {
    .name = "_thread._localdummy",
    .basicsize = sizeof(localdummyobject),
    .flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION |
              Py_TPFLAGS_IMMUTABLETYPE),
    .slots = local_dummy_type_slots,
};


typedef struct {
    PyObject_HEAD
    PyObject *args;
    PyObject *kw;
    PyObject *weakreflist;      /* List of weak references to self */
    /* A {localdummy -> localdict} dict */
    PyObject *localdicts;
    /* A set of weakrefs to thread sentinels localdummies*/
    PyObject *thread_watchdogs;
} localobject;

/* Forward declaration */
static int create_localsdict(localobject *self, thread_module_state *state,
                             PyObject **localsdict, PyObject **sentinel_wr);
static PyObject *clear_locals(PyObject *meth_self, PyObject *dummyweakref);

/* Create a weakref to the sentinel localdummy for the current thread */
static PyObject *
create_sentinel_wr(localobject *self)
{
    static PyMethodDef wr_callback_def = {
        "clear_locals", (PyCFunction) clear_locals, METH_O
    };

    PyThreadState *tstate = PyThreadState_Get();

    /* We use a weak reference to self in the callback closure
       in order to avoid spurious reference cycles */
    PyObject *self_wr = PyWeakref_NewRef((PyObject *) self, NULL);
    if (self_wr == NULL) {
        return NULL;
    }

    PyObject *args = PyTuple_New(2);
    if (args == NULL) {
        Py_DECREF(self_wr);
        return NULL;
    }
    PyTuple_SET_ITEM(args, 0, self_wr);
    PyTuple_SET_ITEM(args, 1, Py_NewRef(tstate->threading_local_key));

    PyObject *cb = PyCFunction_New(&wr_callback_def, args);
    Py_DECREF(args);
    if (cb == NULL) {
        return NULL;
    }

    PyObject *wr = PyWeakref_NewRef(tstate->threading_local_sentinel, cb);
    Py_DECREF(cb);

    return wr;
}

static PyObject *
local_new(PyTypeObject *type, PyObject *args, PyObject *kw)
{
    if (type->tp_init == PyBaseObject_Type.tp_init) {
        int rc = 0;
        if (args != NULL)
            rc = PyObject_IsTrue(args);
        if (rc == 0 && kw != NULL)
            rc = PyObject_IsTrue(kw);
        if (rc != 0) {
            if (rc > 0) {
                PyErr_SetString(PyExc_TypeError,
                          "Initialization arguments are not supported");
            }
            return NULL;
        }
    }

    PyObject *module = PyType_GetModuleByDef(type, &thread_module);
    assert(module != NULL);
    thread_module_state *state = get_thread_state(module);

    localobject *self = (localobject *)type->tp_alloc(type, 0);
    if (self == NULL) {
        return NULL;
    }

    self->args = Py_XNewRef(args);
    self->kw = Py_XNewRef(kw);

    self->localdicts = PyDict_New();
    if (self->localdicts == NULL) {
        goto err;
    }

    self->thread_watchdogs = PySet_New(NULL);
    if (self->thread_watchdogs == NULL) {
        goto err;
    }

    PyObject *localsdict = NULL;
    PyObject *sentinel_wr = NULL;
    if (create_localsdict(self, state, &localsdict, &sentinel_wr) < 0) {
        goto err;
    }
    Py_DECREF(localsdict);
    Py_DECREF(sentinel_wr);

    return (PyObject *)self;

  err:
    Py_DECREF(self);
    return NULL;
}

static int
local_traverse(localobject *self, visitproc visit, void *arg)
{
    Py_VISIT(Py_TYPE(self));
    Py_VISIT(self->args);
    Py_VISIT(self->kw);
    Py_VISIT(self->localdicts);
    Py_VISIT(self->thread_watchdogs);
    return 0;
}

static int
local_clear(localobject *self)
{
    Py_CLEAR(self->args);
    Py_CLEAR(self->kw);
    Py_CLEAR(self->localdicts);
    Py_CLEAR(self->thread_watchdogs);
    return 0;
}

static void
local_dealloc(localobject *self)
{
    /* Weakrefs must be invalidated right now, otherwise they can be used
       from code called below, which is very dangerous since Py_REFCNT(self) == 0 */
    if (self->weakreflist != NULL) {
        PyObject_ClearWeakRefs((PyObject *) self);
    }

    PyObject_GC_UnTrack(self);

    local_clear(self);

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free((PyObject*)self);
    Py_DECREF(tp);
}

/* Create the TLS key and sentinel if they don't exist */
static int
create_localdummies(thread_module_state *state)
{
    PyThreadState *tstate = _PyThreadState_GET();

    if (tstate->threading_local_key != NULL) {
        return 0;
    }

    PyTypeObject *ld_type = state->local_dummy_type;
    tstate->threading_local_key = ld_type->tp_alloc(ld_type, 0);
    if (tstate->threading_local_key == NULL) {
        return -1;
    }

    tstate->threading_local_sentinel = ld_type->tp_alloc(ld_type, 0);
    if (tstate->threading_local_sentinel == NULL) {
        Py_CLEAR(tstate->threading_local_key);
        return -1;
    }

    return 0;
}

/* Insert a localsdict and sentinel weakref for the current thread, placing
   strong references in localsdict and sentinel_wr, respectively.
*/
static int
create_localsdict(localobject *self, thread_module_state *state,
                  PyObject **localsdict, PyObject **sentinel_wr)
{
    PyThreadState *tstate = _PyThreadState_GET();
    PyObject *ldict = NULL;
    PyObject *wr = NULL;

    if (create_localdummies(state) < 0) {
        goto err;
    }

    /* Create and insert the locals dict and sentinel weakref */
    ldict = PyDict_New();
    if (ldict == NULL) {
        goto err;
    }

    if (PyDict_SetItem(self->localdicts, tstate->threading_local_key, ldict) <
        0) {
        goto err;
    }

    wr = create_sentinel_wr(self);
    if (wr == NULL) {
        PyObject *exc = PyErr_GetRaisedException();
        if (PyDict_DelItem(self->localdicts, tstate->threading_local_key) <
            0) {
            PyErr_WriteUnraisable((PyObject *)self);
        }
        PyErr_SetRaisedException(exc);
        goto err;
    }

    if (PySet_Add(self->thread_watchdogs, wr) < 0) {
        PyObject *exc = PyErr_GetRaisedException();
        if (PyDict_DelItem(self->localdicts, tstate->threading_local_key) <
            0) {
            PyErr_WriteUnraisable((PyObject *)self);
        }
        PyErr_SetRaisedException(exc);
        goto err;
    }

    *localsdict = ldict;
    *sentinel_wr = wr;
    return 0;

err:
    Py_XDECREF(ldict);
    Py_XDECREF(wr);
    return -1;
}

/* Return a strong reference to the locals dict for the current thread,
   creating it if necessary.
*/
static PyObject *
_ldict(localobject *self, thread_module_state *state)
{
    if (create_localdummies(state) < 0) {
        return NULL;
    }

    /* Check if a localsdict already exists */
    PyObject *ldict;
    PyThreadState *tstate = _PyThreadState_GET();
    if (PyDict_GetItemRef(self->localdicts, tstate->threading_local_key,
                          &ldict) < 0) {
        return NULL;
    }
    if (ldict != NULL) {
        return ldict;
    }

    /* threading.local hasn't been instantiated for this thread */
    PyObject *wr;
    if (create_localsdict(self, state, &ldict, &wr) < 0) {
        return NULL;
    }

    /* run __init__ if we're a subtype of `threading.local` */
    if (Py_TYPE(self)->tp_init != PyBaseObject_Type.tp_init &&
        Py_TYPE(self)->tp_init((PyObject *)self, self->args, self->kw) < 0) {
        /* we need to get rid of ldict from thread so
           we create a new one the next time we do an attr
           access */
        PyObject *exc = PyErr_GetRaisedException();
        if (PyDict_DelItem(self->localdicts, tstate->threading_local_key) <
            0) {
            PyErr_WriteUnraisable((PyObject *)self);
            PyErr_Clear();
        }
        if (PySet_Discard(self->thread_watchdogs, wr) < 0) {
            PyErr_WriteUnraisable((PyObject *)self);
        }
        PyErr_SetRaisedException(exc);
        Py_DECREF(ldict);
        Py_DECREF(wr);
        return NULL;
    }
    Py_DECREF(wr);

    return ldict;
}

static int
local_setattro(localobject *self, PyObject *name, PyObject *v)
{
    PyObject *module = PyType_GetModuleByDef(Py_TYPE(self), &thread_module);
    assert(module != NULL);
    thread_module_state *state = get_thread_state(module);

    PyObject *ldict = _ldict(self, state);
    if (ldict == NULL) {
        goto err;
    }

    int r = PyObject_RichCompareBool(name, &_Py_ID(__dict__), Py_EQ);
    if (r == -1) {
        goto err;
    }
    if (r == 1) {
        PyErr_Format(PyExc_AttributeError,
                     "'%.100s' object attribute '%U' is read-only",
                     Py_TYPE(self)->tp_name, name);
        goto err;
    }

    int st =
        _PyObject_GenericSetAttrWithDict((PyObject *)self, name, v, ldict);
    Py_DECREF(ldict);
    return st;

err:
    Py_XDECREF(ldict);
    return -1;
}

static PyObject *local_getattro(localobject *, PyObject *);

static PyMemberDef local_type_members[] = {
    {"__weaklistoffset__", Py_T_PYSSIZET, offsetof(localobject, weakreflist), Py_READONLY},
    {NULL},
};

static PyType_Slot local_type_slots[] = {
    {Py_tp_dealloc, (destructor)local_dealloc},
    {Py_tp_getattro, (getattrofunc)local_getattro},
    {Py_tp_setattro, (setattrofunc)local_setattro},
    {Py_tp_doc, "_local()\n--\n\nThread-local data"},
    {Py_tp_traverse, (traverseproc)local_traverse},
    {Py_tp_clear, (inquiry)local_clear},
    {Py_tp_new, local_new},
    {Py_tp_members, local_type_members},
    {0, 0}
};

static PyType_Spec local_type_spec = {
    .name = "_thread._local",
    .basicsize = sizeof(localobject),
    .flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC |
              Py_TPFLAGS_IMMUTABLETYPE),
    .slots = local_type_slots,
};

static PyObject *
local_getattro(localobject *self, PyObject *name)
{
    PyObject *module = PyType_GetModuleByDef(Py_TYPE(self), &thread_module);
    assert(module != NULL);
    thread_module_state *state = get_thread_state(module);

    PyObject *ldict = _ldict(self, state);
    if (ldict == NULL)
        return NULL;

    int r = PyObject_RichCompareBool(name, &_Py_ID(__dict__), Py_EQ);
    if (r == 1) {
        return ldict;
    }
    if (r == -1) {
        Py_DECREF(ldict);
        return NULL;
    }

    if (!Py_IS_TYPE(self, state->local_type)) {
        /* use generic lookup for subtypes */
        PyObject *res =
            _PyObject_GenericGetAttrWithDict((PyObject *)self, name, ldict, 0);
        Py_DECREF(ldict);
        return res;
    }

    /* Optimization: just look in dict ourselves */
    PyObject *value;
    if (PyDict_GetItemRef(ldict, name, &value) != 0) {
        // found or error
        Py_DECREF(ldict);
        return value;
    }

    /* Fall back on generic to get __class__ and __dict__ */
    PyObject *res =
        _PyObject_GenericGetAttrWithDict((PyObject *)self, name, ldict, 0);
    Py_DECREF(ldict);
    return res;
}

/* Called when a dummy is destroyed, indicating that the owning thread is being
 * cleared. */
static PyObject *
clear_locals(PyObject *locals_and_key, PyObject *dummyweakref)
{
    PyObject *localweakref = PyTuple_GetItem(locals_and_key, 0);
    localobject *self = (localobject *)_PyWeakref_GET_REF(localweakref);
    if (self == NULL) {
        Py_RETURN_NONE;
    }

    /* If the thread-local object is still alive and not being cleared,
       remove the corresponding local dict */
    if (self->localdicts != NULL) {
        PyObject *key = PyTuple_GetItem(locals_and_key, 1);
        if (PyDict_Pop(self->localdicts, key, NULL) < 0) {
            PyErr_WriteUnraisable((PyObject*)self);
        }
    }
    if (self->thread_watchdogs != NULL) {
        if (PySet_Discard(self->thread_watchdogs, dummyweakref) < 0) {
            PyErr_WriteUnraisable((PyObject *)self);
        }
    }

    Py_DECREF(self);
    Py_RETURN_NONE;
}

/* Module functions */

static PyObject *
thread_daemon_threads_allowed(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    PyInterpreterState *interp = _PyInterpreterState_GET();
    if (interp->feature_flags & Py_RTFLAGS_DAEMON_THREADS) {
        Py_RETURN_TRUE;
    }
    else {
        Py_RETURN_FALSE;
    }
}

PyDoc_STRVAR(daemon_threads_allowed_doc,
"daemon_threads_allowed($module, /)\n\
--\n\
\n\
Return True if daemon threads are allowed in the current interpreter,\n\
and False otherwise.\n");

static int
do_start_new_thread(thread_module_state *state, PyObject *func, PyObject *args,
                    PyObject *kwargs, PyThread_handle_t *handle)
{
    int wait_at_shutdown = _PyThreadHandle_GetWaitAtShutdown(handle);
    if (wait_at_shutdown) {
        // Add the handle before starting the thread to avoid adding a handle
        // to a thread that has already finished (i.e. if the thread finishes
        // before the call to `ThreadHandle_start()` below returns).
        add_to_shutdown_handles(state, handle);
    }

    int st = _PyThreadHandle_Start(handle, func, args, kwargs);
    if (st < 0) {
        if (wait_at_shutdown) {
            remove_from_shutdown_handles(state, handle);
        }
        return -1;
    }
    return 0;
}

static PyObject *
thread_PyThread_start_new_thread(PyObject *module, PyObject *fargs)
{
    PyObject *func, *args, *kwargs = NULL;
    thread_module_state *state = get_thread_state(module);

    if (!PyArg_UnpackTuple(fargs, "start_new_thread", 2, 3,
                           &func, &args, &kwargs))
        return NULL;
    if (!PyCallable_Check(func)) {
        PyErr_SetString(PyExc_TypeError,
                        "first arg must be callable");
        return NULL;
    }
    if (!PyTuple_Check(args)) {
        PyErr_SetString(PyExc_TypeError,
                        "2nd arg must be a tuple");
        return NULL;
    }
    if (kwargs != NULL && !PyDict_Check(kwargs)) {
        PyErr_SetString(PyExc_TypeError,
                        "optional 3rd arg must be a dictionary");
        return NULL;
    }

    if (PySys_Audit("_thread.start_new_thread", "OOO",
                    func, args, kwargs ? kwargs : Py_None) < 0) {
        return NULL;
    }

    PyThread_handle_t *handle = _PyThreadHandle_New(/*wait_at_shutdown=*/0);
    if (handle == NULL) {
        return NULL;
    }

    if (do_start_new_thread(state, func, args, kwargs, handle) < 0) {
        _PyThreadHandle_Release(handle);
        return NULL;
    }
    PyThread_ident_t ident = _PyThreadHandle_GetIdent(handle);
    _PyThreadHandle_Release(handle);
    return PyLong_FromUnsignedLongLong(ident);
}

PyDoc_STRVAR(start_new_thread_doc,
"start_new_thread($module, function, args, kwargs={}, /)\n\
--\n\
\n\
Start a new thread and return its identifier.\n\
\n\
The thread will call the function with positional arguments from the\n\
tuple args and keyword arguments taken from the optional dictionary\n\
kwargs.  The thread exits when the function returns; the return value\n\
is ignored.  The thread will also exit when the function raises an\n\
unhandled exception; a stack trace will be printed unless the exception\n\
is SystemExit.");

PyDoc_STRVAR(start_new_doc,
"start_new($module, function, args, kwargs={}, /)\n\
--\n\
\n\
An obsolete synonym of start_new_thread().");

static PyObject *
thread_PyThread_start_joinable_thread(PyObject *module, PyObject *fargs,
                                      PyObject *fkwargs)
{
    static char *keywords[] = {"function", "handle", "daemon", NULL};
    PyObject *func = NULL;
    int daemon = -1;
    thread_module_state *state = get_thread_state(module);
    PyObject *hobj_arg = NULL;
    if (!PyArg_ParseTupleAndKeywords(fargs, fkwargs,
                                     "O|Op:start_joinable_thread", keywords,
                                     &func, &hobj_arg, &daemon)) {
        return NULL;
    }

    if (!PyCallable_Check(func)) {
        PyErr_SetString(PyExc_TypeError,
                        "thread function must be callable");
        return NULL;
    }

    int wait_at_shutdown = -1;
    if (daemon >= 0) {
        const char *msg = "\"daemon\" is deprecated, "
                          "set\"wait_at_shutdown\" on the handle instead";
        if (PyErr_WarnEx(PyExc_DeprecationWarning, msg, 1) < 0) {
            return NULL;
        }
        wait_at_shutdown = !daemon;
    }

    if (hobj_arg == NULL) {
        hobj_arg = Py_None;
    }
    else if (hobj_arg != Py_None
             && !Py_IS_TYPE(hobj_arg, state->thread_handle_type))
    {
        PyErr_SetString(PyExc_TypeError, "'handle' must be a _ThreadHandle");
        return NULL;
    }

    if (PySys_Audit("_thread.start_joinable_thread", "OiO", func, daemon,
                    hobj_arg) < 0) {
        return NULL;
    }

    PyThreadHandleObject *hobj = NULL;
    if (hobj_arg != Py_None) {
        hobj = (PyThreadHandleObject*)hobj_arg;
        assert(hobj->handle != NULL);
        if (wait_at_shutdown >= 0) {
            int actual = _PyThreadHandle_GetWaitAtShutdown(hobj->handle);
            if (actual != wait_at_shutdown) {
                 hobj = NULL;
            }
        }
    }

    PyThread_handle_t *handle;
    if (hobj == NULL) {
        if (wait_at_shutdown < 0) {
            wait_at_shutdown = 0;
        }
        handle = _PyThreadHandle_New(wait_at_shutdown);
        if (handle == NULL) {
            return NULL;
        }
        hobj = PyThreadHandleObject_new(state->thread_handle_type, handle);
        _PyThreadHandle_Release(handle);
        if (hobj == NULL) {
            return NULL;
        }
    }
    else {
        Py_INCREF(hobj);
    }

    PyObject* args = PyTuple_New(0);
    if (args == NULL) {
        Py_DECREF(hobj);
        return NULL;
    }
    int st = do_start_new_thread(state, func, args, /*kwargs=*/ NULL, handle);
    Py_DECREF(args);
    if (st < 0) {
        Py_DECREF(hobj);
        return NULL;
    }
    return (PyObject *)hobj;
}

PyDoc_STRVAR(start_joinable_doc,
"start_joinable_thread($module, /, function, handle=None, daemon=True)\n\
--\n\
\n\
*For internal use only*: start a new thread.\n\
\n\
Like start_new_thread(), this starts a new thread calling the given function.\n\
Unlike start_new_thread(), this returns a handle object with methods to join\n\
or detach the given thread.\n\
This function is not for third-party code, please use the\n\
`threading` module instead. During finalization the runtime will not wait for\n\
the thread to exit if daemon is True. If handle is provided it must be a\n\
newly created thread._ThreadHandle instance.");

static PyObject *
thread_PyThread_exit_thread(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    PyErr_SetNone(PyExc_SystemExit);
    return NULL;
}

PyDoc_STRVAR(exit_doc,
"exit($module, /)\n\
--\n\
\n\
This is synonymous to ``raise SystemExit''.  It will cause the current\n\
thread to exit silently unless the exception is caught.");

PyDoc_STRVAR(exit_thread_doc,
"exit_thread($module, /)\n\
--\n\
\n\
An obsolete synonym of exit().");

static PyObject *
thread_PyThread_interrupt_main(PyObject *self, PyObject *args)
{
    int signum = SIGINT;
    if (!PyArg_ParseTuple(args, "|i:signum", &signum)) {
        return NULL;
    }

    if (PyErr_SetInterruptEx(signum)) {
        PyErr_SetString(PyExc_ValueError, "signal number out of range");
        return NULL;
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(interrupt_doc,
"interrupt_main($module, signum=signal.SIGINT, /)\n\
--\n\
\n\
Simulate the arrival of the given signal in the main thread,\n\
where the corresponding signal handler will be executed.\n\
If *signum* is omitted, SIGINT is assumed.\n\
A subthread can use this function to interrupt the main thread.\n\
\n\
Note: the default signal handler for SIGINT raises ``KeyboardInterrupt``."
);

static PyObject *
thread_PyThread_allocate_lock(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return (PyObject *) newlockobject(module);
}

PyDoc_STRVAR(allocate_lock_doc,
"allocate_lock($module, /)\n\
--\n\
\n\
Create a new lock object. See help(type(threading.Lock())) for\n\
information about locks.");

PyDoc_STRVAR(allocate_doc,
"allocate($module, /)\n\
--\n\
\n\
An obsolete synonym of allocate_lock().");

static PyObject *
thread_get_ident(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    PyThread_ident_t ident = PyThread_get_thread_ident_ex();
    if (ident == PYTHREAD_INVALID_THREAD_ID) {
        PyErr_SetString(ThreadError, "no current thread ident");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong(ident);
}

PyDoc_STRVAR(get_ident_doc,
"get_ident($module, /)\n\
--\n\
\n\
Return a non-zero integer that uniquely identifies the current thread\n\
amongst other threads that exist simultaneously.\n\
This may be used to identify per-thread resources.\n\
Even though on some platforms threads identities may appear to be\n\
allocated consecutive numbers starting at 1, this behavior should not\n\
be relied upon, and the number should be seen purely as a magic cookie.\n\
A thread's identity may be reused for another thread after it exits.");

#ifdef PY_HAVE_THREAD_NATIVE_ID
static PyObject *
thread_get_native_id(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    unsigned long native_id = PyThread_get_thread_native_id();
    return PyLong_FromUnsignedLong(native_id);
}

PyDoc_STRVAR(get_native_id_doc,
"get_native_id($module, /)\n\
--\n\
\n\
Return a non-negative integer identifying the thread as reported\n\
by the OS (kernel). This may be used to uniquely identify a\n\
particular thread within a system.");
#endif

static PyObject *
thread__count(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    PyInterpreterState *interp = _PyInterpreterState_GET();
    return PyLong_FromSsize_t(_Py_atomic_load_ssize(&interp->threads.count));
}

PyDoc_STRVAR(_count_doc,
"_count($module, /)\n\
--\n\
\n\
Return the number of currently running Python threads, excluding\n\
the main thread. The returned number comprises all threads created\n\
through `start_new_thread()` as well as `threading.Thread`, and not\n\
yet finished.\n\
\n\
This function is meant for internal and specialized purposes only.\n\
In most applications `threading.enumerate()` should be used instead.");

static PyObject *
thread_stack_size(PyObject *self, PyObject *args)
{
    size_t old_size;
    Py_ssize_t new_size = 0;
    int rc;

    if (!PyArg_ParseTuple(args, "|n:stack_size", &new_size))
        return NULL;

    if (new_size < 0) {
        PyErr_SetString(PyExc_ValueError,
                        "size must be 0 or a positive value");
        return NULL;
    }

    old_size = PyThread_get_stacksize();

    rc = PyThread_set_stacksize((size_t) new_size);
    if (rc == -1) {
        PyErr_Format(PyExc_ValueError,
                     "size not valid: %zd bytes",
                     new_size);
        return NULL;
    }
    if (rc == -2) {
        PyErr_SetString(ThreadError,
                        "setting stack size not supported");
        return NULL;
    }

    return PyLong_FromSsize_t((Py_ssize_t) old_size);
}

PyDoc_STRVAR(stack_size_doc,
"stack_size($module, size=0, /)\n\
--\n\
\n\
Return the thread stack size used when creating new threads.  The\n\
optional size argument specifies the stack size (in bytes) to be used\n\
for subsequently created threads, and must be 0 (use platform or\n\
configured default) or a positive integer value of at least 32,768 (32k).\n\
If changing the thread stack size is unsupported, a ThreadError\n\
exception is raised.  If the specified size is invalid, a ValueError\n\
exception is raised, and the stack size is unmodified.  32k bytes\n\
 currently the minimum supported stack size value to guarantee\n\
sufficient stack space for the interpreter itself.\n\
\n\
Note that some platforms may have particular restrictions on values for\n\
the stack size, such as requiring a minimum stack size larger than 32 KiB or\n\
requiring allocation in multiples of the system memory page size\n\
- platform documentation should be referred to for more information\n\
(4 KiB pages are common; using multiples of 4096 for the stack size is\n\
the suggested approach in the absence of more specific information).");

static int
thread_excepthook_file(PyObject *file, PyObject *exc_type, PyObject *exc_value,
                       PyObject *exc_traceback, PyObject *thread)
{
    /* print(f"Exception in thread {thread.name}:", file=file) */
    if (PyFile_WriteString("Exception in thread ", file) < 0) {
        return -1;
    }

    PyObject *name = NULL;
    if (thread != Py_None) {
        if (PyObject_GetOptionalAttr(thread, &_Py_ID(name), &name) < 0) {
            return -1;
        }
    }
    if (name != NULL) {
        if (PyFile_WriteObject(name, file, Py_PRINT_RAW) < 0) {
            Py_DECREF(name);
            return -1;
        }
        Py_DECREF(name);
    }
    else {
        PyThread_ident_t ident = PyThread_get_thread_ident_ex();
        PyObject *str = PyUnicode_FromFormat("%" PY_FORMAT_THREAD_IDENT_T, ident);
        if (str != NULL) {
            if (PyFile_WriteObject(str, file, Py_PRINT_RAW) < 0) {
                Py_DECREF(str);
                return -1;
            }
            Py_DECREF(str);
        }
        else {
            PyErr_Clear();

            if (PyFile_WriteString("<failed to get thread name>", file) < 0) {
                return -1;
            }
        }
    }

    if (PyFile_WriteString(":\n", file) < 0) {
        return -1;
    }

    /* Display the traceback */
    _PyErr_Display(file, exc_type, exc_value, exc_traceback);

    /* Call file.flush() */
    if (_PyFile_Flush(file) < 0) {
        return -1;
    }

    return 0;
}


PyDoc_STRVAR(ExceptHookArgs__doc__,
"ExceptHookArgs\n\
\n\
Type used to pass arguments to threading.excepthook.");

static PyStructSequence_Field ExceptHookArgs_fields[] = {
    {"exc_type", "Exception type"},
    {"exc_value", "Exception value"},
    {"exc_traceback", "Exception traceback"},
    {"thread", "Thread"},
    {0}
};

static PyStructSequence_Desc ExceptHookArgs_desc = {
    .name = "_thread._ExceptHookArgs",
    .doc = ExceptHookArgs__doc__,
    .fields = ExceptHookArgs_fields,
    .n_in_sequence = 4
};


static PyObject *
thread_excepthook(PyObject *module, PyObject *args)
{
    thread_module_state *state = get_thread_state(module);

    if (!Py_IS_TYPE(args, state->excepthook_type)) {
        PyErr_SetString(PyExc_TypeError,
                        "_thread.excepthook argument type "
                        "must be ExceptHookArgs");
        return NULL;
    }

    /* Borrowed reference */
    PyObject *exc_type = PyStructSequence_GET_ITEM(args, 0);
    if (exc_type == PyExc_SystemExit) {
        /* silently ignore SystemExit */
        Py_RETURN_NONE;
    }

    /* Borrowed references */
    PyObject *exc_value = PyStructSequence_GET_ITEM(args, 1);
    PyObject *exc_tb = PyStructSequence_GET_ITEM(args, 2);
    PyObject *thread = PyStructSequence_GET_ITEM(args, 3);

    PyThreadState *tstate = _PyThreadState_GET();
    PyObject *file = _PySys_GetAttr(tstate, &_Py_ID(stderr));
    if (file == NULL || file == Py_None) {
        if (thread == Py_None) {
            /* do nothing if sys.stderr is None and thread is None */
            Py_RETURN_NONE;
        }

        file = PyObject_GetAttrString(thread, "_stderr");
        if (file == NULL) {
            return NULL;
        }
        if (file == Py_None) {
            Py_DECREF(file);
            /* do nothing if sys.stderr is None and sys.stderr was None
               when the thread was created */
            Py_RETURN_NONE;
        }
    }
    else {
        Py_INCREF(file);
    }

    int res = thread_excepthook_file(file, exc_type, exc_value, exc_tb,
                                     thread);
    Py_DECREF(file);
    if (res < 0) {
        return NULL;
    }

    Py_RETURN_NONE;
}

PyDoc_STRVAR(excepthook_doc,
"_excepthook($module, (exc_type, exc_value, exc_traceback, thread), /)\n\
--\n\
\n\
Handle uncaught Thread.run() exception.");

static PyObject *
thread__is_main_interpreter(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    PyInterpreterState *interp = _PyInterpreterState_GET();
    return PyBool_FromLong(_Py_IsMainInterpreter(interp));
}

PyDoc_STRVAR(thread__is_main_interpreter_doc,
"_is_main_interpreter($module, /)\n\
--\n\
\n\
Return True if the current interpreter is the main Python interpreter.");

static PyObject *
thread_shutdown(PyObject *self, PyObject *args)
{
    thread_module_state *state = get_thread_state(self);
    (void)_PyThread_Shutdown(state->shutdown_handles);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(shutdown_doc,
"_shutdown($module, /)\n\
--\n\
\n\
Wait for all non-daemon threads (other than the calling thread) to stop.");

static PyObject *
thread__make_thread_handle(PyObject *module, PyObject *identobj)
{
    thread_module_state *state = get_thread_state(module);
    if (!PyLong_Check(identobj)) {
        PyErr_SetString(PyExc_TypeError, "ident must be an integer");
        return NULL;
    }
    PyThread_ident_t ident = PyLong_AsUnsignedLongLong(identobj);
    if (PyErr_Occurred()) {
        return NULL;
    }
    PyThread_handle_t *handle = _PyThreadHandle_FromIdent(ident);
    if (handle == NULL) {
        return NULL;
    }
    PyThreadHandleObject *hobj =
        PyThreadHandleObject_new(state->thread_handle_type, handle);
    _PyThreadHandle_Release(handle);
    if (hobj == NULL) {
        return NULL;
    }
    return (PyObject*) hobj;
}

PyDoc_STRVAR(thread__make_thread_handle_doc,
"_make_thread_handle($module, ident, /)\n\
--\n\
\n\
Internal only. Make a thread handle for threads not spawned\n\
by the _thread or threading module.");

static PyObject *
thread__get_main_thread_ident(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return PyLong_FromUnsignedLongLong(_PyRuntime.main_thread);
}

PyDoc_STRVAR(thread__get_main_thread_ident_doc,
"_get_main_thread_ident($module, /)\n\
--\n\
\n\
Internal only. Return a non-zero integer that uniquely identifies the main thread\n\
of the main interpreter.");

static PyMethodDef thread_methods[] = {
    {"start_new_thread",        (PyCFunction)thread_PyThread_start_new_thread,
     METH_VARARGS, start_new_thread_doc},
    {"start_new",               (PyCFunction)thread_PyThread_start_new_thread,
     METH_VARARGS, start_new_doc},
    {"start_joinable_thread",   _PyCFunction_CAST(thread_PyThread_start_joinable_thread),
     METH_VARARGS | METH_KEYWORDS, start_joinable_doc},
    {"daemon_threads_allowed",  (PyCFunction)thread_daemon_threads_allowed,
     METH_NOARGS, daemon_threads_allowed_doc},
    {"allocate_lock",           thread_PyThread_allocate_lock,
     METH_NOARGS, allocate_lock_doc},
    {"allocate",                thread_PyThread_allocate_lock,
     METH_NOARGS, allocate_doc},
    {"exit_thread",             thread_PyThread_exit_thread,
     METH_NOARGS, exit_thread_doc},
    {"exit",                    thread_PyThread_exit_thread,
     METH_NOARGS, exit_doc},
    {"interrupt_main",          (PyCFunction)thread_PyThread_interrupt_main,
     METH_VARARGS, interrupt_doc},
    {"get_ident",               thread_get_ident,
     METH_NOARGS, get_ident_doc},
#ifdef PY_HAVE_THREAD_NATIVE_ID
    {"get_native_id",           thread_get_native_id,
     METH_NOARGS, get_native_id_doc},
#endif
    {"_count",                  thread__count,
     METH_NOARGS, _count_doc},
    {"stack_size",              (PyCFunction)thread_stack_size,
     METH_VARARGS, stack_size_doc},
    {"_excepthook",             thread_excepthook,
     METH_O, excepthook_doc},
    {"_is_main_interpreter",    thread__is_main_interpreter,
     METH_NOARGS, thread__is_main_interpreter_doc},
    {"_shutdown",               thread_shutdown,
     METH_NOARGS, shutdown_doc},
    {"_make_thread_handle", thread__make_thread_handle,
     METH_O, thread__make_thread_handle_doc},
    {"_get_main_thread_ident", thread__get_main_thread_ident,
     METH_NOARGS, thread__get_main_thread_ident_doc},
    {NULL,                      NULL}           /* sentinel */
};


/* Initialization function */

static int
thread_module_exec(PyObject *module)
{
    thread_module_state *state = get_thread_state(module);
    PyObject *d = PyModule_GetDict(module);

    // Initialize the C thread library
    PyThread_init_thread();

    // _ThreadHandle
    state->thread_handle_type = (PyTypeObject *)PyType_FromSpec(&ThreadHandle_Type_spec);
    if (state->thread_handle_type == NULL) {
        return -1;
    }
    if (PyDict_SetItemString(d, "_ThreadHandle", (PyObject *)state->thread_handle_type) < 0) {
        return -1;
    }

    // Lock
    state->lock_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &lock_type_spec, NULL);
    if (state->lock_type == NULL) {
        return -1;
    }
    if (PyModule_AddType(module, state->lock_type) < 0) {
        return -1;
    }
    // Old alias: lock -> LockType
    if (PyDict_SetItemString(d, "LockType", (PyObject *)state->lock_type) < 0) {
        return -1;
    }

    // RLock
    PyTypeObject *rlock_type = (PyTypeObject *)PyType_FromSpec(&rlock_type_spec);
    if (rlock_type == NULL) {
        return -1;
    }
    if (PyModule_AddType(module, rlock_type) < 0) {
        Py_DECREF(rlock_type);
        return -1;
    }
    Py_DECREF(rlock_type);

    // Local dummy
    state->local_dummy_type = (PyTypeObject *)PyType_FromSpec(&local_dummy_type_spec);
    if (state->local_dummy_type == NULL) {
        return -1;
    }

    // Local
    state->local_type = (PyTypeObject *)PyType_FromModuleAndSpec(module, &local_type_spec, NULL);
    if (state->local_type == NULL) {
        return -1;
    }
    if (PyModule_AddType(module, state->local_type) < 0) {
        return -1;
    }

    // Add module attributes
    if (PyDict_SetItemString(d, "error", ThreadError) < 0) {
        return -1;
    }

    // _ExceptHookArgs type
    state->excepthook_type = PyStructSequence_NewType(&ExceptHookArgs_desc);
    if (state->excepthook_type == NULL) {
        return -1;
    }
    if (PyModule_AddType(module, state->excepthook_type) < 0) {
        return -1;
    }

    // TIMEOUT_MAX
    double timeout_max = (double)PY_TIMEOUT_MAX * 1e-6;
    double time_max = PyTime_AsSecondsDouble(PyTime_MAX);
    timeout_max = Py_MIN(timeout_max, time_max);
    // Round towards minus infinity
    timeout_max = floor(timeout_max);

    if (PyModule_Add(module, "TIMEOUT_MAX",
                        PyFloat_FromDouble(timeout_max)) < 0) {
        return -1;
    }

    init_shutdown_handles(state);

    return 0;
}


static int
thread_module_traverse(PyObject *module, visitproc visit, void *arg)
{
    thread_module_state *state = get_thread_state(module);
    Py_VISIT(state->excepthook_type);
    Py_VISIT(state->lock_type);
    Py_VISIT(state->local_type);
    Py_VISIT(state->local_dummy_type);
    Py_VISIT(state->thread_handle_type);
    return 0;
}

static int
thread_module_clear(PyObject *module)
{
    thread_module_state *state = get_thread_state(module);
    Py_CLEAR(state->excepthook_type);
    Py_CLEAR(state->lock_type);
    Py_CLEAR(state->local_type);
    Py_CLEAR(state->local_dummy_type);
    Py_CLEAR(state->thread_handle_type);
    // Remove any remaining handles (e.g. if shutdown exited early due to
    // interrupt) so that attempts to unlink the handle after our module state
    // is destroyed do not crash.
    clear_shutdown_handles(state);
    return 0;
}

static void
thread_module_free(void *module)
{
    thread_module_clear((PyObject *)module);
}



PyDoc_STRVAR(thread_doc,
"This module provides primitive operations to write multi-threaded programs.\n\
The 'threading' module provides a more convenient interface.");

static PyModuleDef_Slot thread_module_slots[] = {
    {Py_mod_exec, thread_module_exec},
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
    {0, NULL}
};

static struct PyModuleDef thread_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_thread",
    .m_doc = thread_doc,
    .m_size = sizeof(thread_module_state),
    .m_methods = thread_methods,
    .m_traverse = thread_module_traverse,
    .m_clear = thread_module_clear,
    .m_free = thread_module_free,
    .m_slots = thread_module_slots,
};

PyMODINIT_FUNC
PyInit__thread(void)
{
    return PyModuleDef_Init(&thread_module);
}
