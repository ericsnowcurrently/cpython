/* interpreters module */
/* low-level access to interpreter primitives */
#ifndef Py_BUILD_CORE_BUILTIN
#  define Py_BUILD_CORE_MODULE 1
#endif

#include "Python.h"
#include "interpreteridobject.h"


#define MODULE_NAME "_interpreters"


static const char *
_copy_raw_string(PyObject *strobj)
{
    const char *str = PyUnicode_AsUTF8(strobj);
    if (str == NULL) {
        return NULL;
    }
    char *copied = PyMem_RawMalloc(strlen(str)+1);
    if (copied == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    strcpy(copied, str);
    return copied;
}

static PyInterpreterState *
_get_current_interp(void)
{
    // PyInterpreterState_Get() aborts if lookup fails, so don't need
    // to check the result for NULL.
    return PyInterpreterState_Get();
}

static PyObject *
add_new_exception(PyObject *mod, const char *name, PyObject *base)
{
    assert(!PyObject_HasAttrString(mod, name));
    PyObject *exctype = PyErr_NewException(name, base, NULL);
    if (exctype == NULL) {
        return NULL;
    }
    int res = PyModule_AddType(mod, (PyTypeObject *)exctype);
    if (res < 0) {
        Py_DECREF(exctype);
        return NULL;
    }
    return exctype;
}

#define ADD_NEW_EXCEPTION(MOD, NAME, BASE) \
    add_new_exception(MOD, MODULE_NAME "." Py_STRINGIFY(NAME), BASE)

static PyTypeObject *
add_new_type(PyObject *mod, PyType_Spec *spec)
{
    PyTypeObject *cls = (PyTypeObject *)PyType_FromMetaclass(
                NULL, mod, spec, NULL);
    if (cls == NULL) {
        return NULL;
    }
    if (PyModule_AddType(mod, cls) < 0) {
        Py_DECREF(cls);
        return NULL;
    }
    return cls;
}


/* module state *************************************************************/

typedef struct {
    /* heap types */
    PyTypeObject *RunRequestIDType;

    /* exceptions */
    PyObject *RunFailedError;
} module_state;

static inline module_state *
get_module_state(PyObject *mod)
{
    assert(mod != NULL);
    module_state *state = PyModule_GetState(mod);
    assert(state != NULL);
    return state;
}

static int
traverse_module_state(module_state *state, visitproc visit, void *arg)
{
    /* heap types */
    Py_VISIT(state->RunRequestIDType);

    /* exceptions */
    Py_VISIT(state->RunFailedError);

    return 0;
}

static int
clear_module_state(module_state *state)
{
    /* heap types */
    Py_CLEAR(state->RunRequestIDType);

    /* exceptions */
    Py_CLEAR(state->RunFailedError);

    return 0;
}


/* data-sharing-specific code ***********************************************/

// Ultimately we'd like to preserve enough information about the
// exception and traceback that we could re-constitute (or at least
// simulate, a la traceback.TracebackException), and even chain, a copy
// of the exception in the calling interpreter.

typedef struct _sharedexception {
    const char *name;
    const char *msg;
} _sharedexception;

static const struct _sharedexception no_exception = {
    .name = NULL,
    .msg = NULL,
};

static void
_sharedexception_clear(_sharedexception *exc)
{
    if (exc->name != NULL) {
        PyMem_RawFree((void *)exc->name);
    }
    if (exc->msg != NULL) {
        PyMem_RawFree((void *)exc->msg);
    }
}

static const char *
_sharedexception_bind(PyObject *exc, _sharedexception *sharedexc)
{
    assert(exc != NULL);
    const char *failure = NULL;

    PyObject *nameobj = PyUnicode_FromFormat("%S", Py_TYPE(exc));
    if (nameobj == NULL) {
        failure = "unable to format exception type name";
        goto error;
    }
    sharedexc->name = _copy_raw_string(nameobj);
    Py_DECREF(nameobj);
    if (sharedexc->name == NULL) {
        if (PyErr_ExceptionMatches(PyExc_MemoryError)) {
            failure = "out of memory copying exception type name";
        } else {
            failure = "unable to encode and copy exception type name";
        }
        goto error;
    }

    if (exc != NULL) {
        PyObject *msgobj = PyUnicode_FromFormat("%S", exc);
        if (msgobj == NULL) {
            failure = "unable to format exception message";
            goto error;
        }
        sharedexc->msg = _copy_raw_string(msgobj);
        Py_DECREF(msgobj);
        if (sharedexc->msg == NULL) {
            if (PyErr_ExceptionMatches(PyExc_MemoryError)) {
                failure = "out of memory copying exception message";
            } else {
                failure = "unable to encode and copy exception message";
            }
            goto error;
        }
    }

    return NULL;

error:
    assert(failure != NULL);
    PyErr_Clear();
    _sharedexception_clear(sharedexc);
    *sharedexc = no_exception;
    return failure;
}

static void
_sharedexception_apply(_sharedexception *exc, PyObject *wrapperclass)
{
    if (exc->name != NULL) {
        if (exc->msg != NULL) {
            PyErr_Format(wrapperclass, "%s: %s",  exc->name, exc->msg);
        }
        else {
            PyErr_SetString(wrapperclass, exc->name);
        }
    }
    else if (exc->msg != NULL) {
        PyErr_SetString(wrapperclass, exc->msg);
    }
    else {
        PyErr_SetNone(wrapperclass);
    }
}


/* run-request code *********************************************************/

typedef struct run_request_id_data {
    int64_t global;
    int64_t interp;  // (may be negative)
} _runreqid_data;

/* a single run request */

#define STATUS_RUNNING (0)
#define STATUS_SUCCESS (1)
#define STATUS_FAILURE (-1)

struct run_request;
typedef struct run_request {
    _runreqid_data id;
    PyThreadState *tstate;
    int64_t numrefs;
    struct {
        int code;
        PyObject *exception;
//        int finalizing;
    } status;
    PyThread_type_lock wait_mutex;
    struct run_request *next;
} _runreq;

static _runreq *
_runreq_new(PyThread_type_lock mutex)
{
    _runreq *req = PyMem_NEW(_runreq, 1);
    if (req == NULL) {
        return NULL;
    }
    req->wait_mutex = mutex;
    return req;
}

static void
_runreq_free(_runreq *req)
{
//    req->finalizing = 1;
    if (req->wait_mutex != NULL) {
        PyThread_acquire_lock(req->wait_mutex, WAIT_LOCK);
        PyThread_release_lock(req->wait_mutex);
        PyThread_free_lock(req->wait_mutex);
    }
    PyMem_Free(req);
}

static int
_runreq_wait(_runreq *req)
{
    // XXX
    return 0;
}


/* the set of one interpreter's run requests */

typedef struct interp_run_requests {
    int64_t interpid;
    int finalizing;
    _runreq *head;
} _interp_runreqs;


/* the set of run requests */

typedef struct _run_requests {
    PyThread_type_lock mutex;
    _runreq *head;
    int64_t numopen;
    int64_t next_id;
} _runreqs;

static void
_runreqs_init(_runreqs *reqs, PyThread_type_lock mutex)
{
    reqs->mutex = mutex;
    reqs->head = NULL;
    reqs->numopen = 0;
    reqs->next_id = 1;
}

static void
_runreqs_fini(_runreqs *reqs)
{
    assert(reqs->numopen == 0);
    assert(reqs->head == NULL);
    if (reqs->mutex != NULL) {
        PyThread_free_lock(reqs->mutex);
        reqs->mutex = NULL;
    }
}

static int64_t
_runreqs_next_id(_runreqs *reqs)  // needs lock
{
    int64_t id = reqs->next_id;
    assert(id != 0);
    if (id < 0) {
        /* overflow */
        return -1;
    }
    reqs->next_id += 1;
    return id;
}

static _runreq *
_runreqs_look_up(_runreqs *reqs, int64_t reqid, int64_t interpid)
{
    // XXX
    return NULL;
}

static void
_runreqs_release(_runreq *req)
{
    // XXX
}


/* RunReqestID class */

typedef struct run_request_id {
    PyObject_HEAD
    _runreqid_data id;
    _runreqs *reqs;
} _runreqid;

struct _runreqid_converter_data {
    PyObject *module;
    _runreqid_data id;
};

static int
_runreqid_converter(PyObject *arg, void *ptr)
{
    _runreqid_data reqid = {0};
    struct _runreqid_converter_data *data = ptr;
    module_state *state = get_module_state(data->module);
    assert(state != NULL);
    if (PyObject_TypeCheck(arg, state->RunRequestIDType)) {
        reqid = ((_runreqid *)arg)->id;
    }
    else if (PyIndex_Check(arg)) {
        reqid.global = PyLong_AsLongLong(arg);
        if (reqid.global == -1 && PyErr_Occurred()) {
            return 0;
        }
        if (reqid.global < 0) {
            PyErr_Format(PyExc_ValueError,
                         ("run request ID must be a non-negative int, "
                          "got %R"),
                         arg);
            return 0;
        }
    }
    else {
        PyErr_Format(PyExc_TypeError,
                     "run request ID must be an int, got %.100s",
                     Py_TYPE(arg)->tp_name);
        return 0;
    }
    data->id = reqid;
    return 1;
}

static _runreqid_data
_runreqid_parse_args(const char *func, PyObject *mod,
                     PyObject *args, PyObject *kwds) {
{
    char argspec[100] = "O&|O&:";
    strcpy(argspec[6], func);
    static char *kwlist[] = {"id", "interpid", NULL};
    struct _runreqid_converter_data convdata = {
        .module = mod,
    };
    int64_t interpid = -1;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, argspec, kwlist,
                                     _runreqid_converter, &convdata,
                                     PyInterpreterID_Converter, &interpid,
                                     )) {
        return reqid;
    }

    if (convdata.id.interp == -1) {
        convdata.id.interp = interpid;
    }
    else if (interpid != -1 && interpid != convdata.id.interp) {
        PyErr_Format(PyExc_ValueError, "interpid mismatch (%d != %d)",
                     convdata.id.interp, interpid);
        convdata.id.global = -1;
    }
    assert(convdata.id.interp >= -1);

    return convdata.id;
}

static PyObject *
new_runreqid(PyTypeObject *cls, _runreqid_data data, _runreqs *runreqs)
{
    if (data.global <= 0) {
        if (!PyErr_Occurred()) {
            PyErr_Format(PyExc_ValueError,
                         ("run request ID must be a positive int, "
                          "got %d"),
                         data.global);
        }
        return NULL;
    }
    _runreqid *self = PyObject_New(_runreqid, cls);
    if (self == NULL) {
        return NULL;
    }
    self->id = data;
    self->owner = runreqs;
    return (PyObject *)self;
}

static void
_runreqid_dealloc(PyObject *self)
{
    _runreqid_data data = ((_runreqid *)self)->id;

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
    /* "Instances of heap-allocated types hold a reference to their type."
     * See: https://docs.python.org/3.11/howto/isolating-extensions.html#garbage-collection-protocol
     * See: https://docs.python.org/3.11/c-api/typeobj.html#c.PyTypeObject.tp_traverse
    */
    // XXX Why don't we implement Py_TPFLAGS_HAVE_GC, e.g. Py_tp_traverse,
    // like we do for _abc._abc_data?
    Py_DECREF(tp);

    _runreq *req = _runreqs_look_up(data);
    if (req == NULL) {
        assert(!PyErr_Occurred());
        return;
    }
    _runreqs_release(req);
}

static PyObject *
_runreqid_repr(PyObject *self)
{
    PyTypeObject *type = Py_TYPE(self);
    const char *name = _PyType_Name(type);

    _int64_t *reqid = ((_runreqid *)self)->id.global;
    return PyUnicode_FromFormat("%s(%" PRId64 ")", name, reqid);
}

static PyObject *
_runreqid_str(PyObject *self)
{
    int64_t *reqid = ((_runreqid *)self)->id.global;
    return PyUnicode_FromFormat("%" PRId64 "", reqid);
}

static PyObject *
_runreqid_int(PyObject *self)
{
    int64_t *reqid = ((_runreqid *)self)->id.global;
    return PyLong_FromLongLong(reqid);
}

static Py_hash_t
_runreqid_hash(PyObject *self)
{
    int64_t *reqid = ((_runreqid *)self)->id.global;
    PyObject *id = PyLong_FromLongLong(reqid);
    if (id == NULL) {
        return -1;
    }
    Py_hash_t hash = PyObject_Hash(id);
    Py_DECREF(id);
    return hash;
}

static PyObject *
_runreqid_richcompare(PyObject *self, PyObject *other, int op)
{
    PyObject *res = NULL;
    if (op != Py_EQ && op != Py_NE) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    PyObject *mod = get_module_from_type(Py_TYPE(self));
    if (mod == NULL) {
        return NULL;
    }
    module_state *state = get_module_state(mod);
    if (state == NULL) {
        goto done;
    }

    if (!PyObject_TypeCheck(self, state->RunRequestIDType)) {
        res = Py_NewRef(Py_NotImplemented);
        goto done;
    }

    int64_t *reqid = ((_runreqid *)self)->id.global;
    int equal;
    if (PyObject_TypeCheck(other, state->RunRequestIDType)) {
        equal = (reqid == _((_runreqid *)other)->id.global);
    }
    else if (PyLong_Check(other)) {
        /* Fast path */
        int overflow;
        long long otherid = PyLong_AsLongLongAndOverflow(other, &overflow);
        if (otherid == -1 && PyErr_Occurred()) {
            goto done;
        }
        equal = !overflow && (otherid >= 0) && (reqid == otherid);
    }
    else if (PyNumber_Check(other)) {
        PyObject *pyid = PyLong_FromLongLong(reqid);
        if (pyid == NULL) {
            goto done;
        }
        res = PyObject_RichCompare(pyid, other, op);
        Py_DECREF(pyid);
        goto done;
    }
    else {
        res = Py_NewRef(Py_NotImplemented);
        goto done;
    }

    if ((op == Py_EQ && equal) || (op == Py_NE && !equal)) {
        res = Py_NewRef(Py_True);
    }
    else {
        res = Py_NewRef(Py_False);
    }

done:
    Py_DECREF(mod);
    return res;
}

static PyObject *
_runreqid_get_interp(PyObject *self)
{
    int64_t interpid = ((_runreqid *)self)->id.interp;
    if (interpid == -1) {
        Py_RETURN_NONE;
    }

    PyObject *idobj = PyInterpreterID_New(id);
    if (idobj == NULL) {
        return NULL;
    }
    if (PyInterpreterID_LookUp(id) == NULL) {
        assert(PyErr_Occurred();
        Py_DECREF(idobj);
        return NULL;
    }
    return idobj;
}

static PyGetSetDef _runreqid_getsets[] = {
    {"interpid", (getter)_runreqid_get_interp, NULL,
     PyDoc_STR("the interpreter associated with this request, if known")},
    {NULL}
};

PyDoc_STRVAR(_runreqid_doc,
"An interpreter status ID identifies an Interpreter.run() request.

It may be used as an int.");

static PyType_Slot RunRequestIDType_slots[] = {
    {Py_tp_dealloc, (destructor)_runreqid_dealloc},
    {Py_tp_doc, (void *)_runreqid_doc},
    {Py_tp_repr, (reprfunc)_runreqid_repr},
    {Py_tp_str, (reprfunc)_runreqid_str},
    {Py_tp_hash, _runreqid_hash},
    {Py_tp_richcompare, _runreqid_richcompare},
    {Py_tp_getset, _runreqid_getsets},
    // number slots
    {Py_nb_int, (unaryfunc)_runreqid_int},
    {Py_nb_index,  (unaryfunc)_runreqid_int},
    {0, NULL},
};

static PyType_Spec RunRequestIDType_spec = {
    .name = MODULE_NAME ".RunRequestID",
    .basicsize = sizeof(_runreqid),
    .flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE |
              Py_TPFLAGS_DISALLOW_INSTANTIATION | Py_TPFLAGS_IMMUTABLETYPE),
    .slots = RunRequestIDType_slots,
};


/* interpreter-specific code ************************************************/

static int
exceptions_init(PyObject *mod)
{
    module_state *state = get_module_state(mod);
    if (state == NULL) {
        return -1;
    }

#define ADD(NAME, BASE) \
    do { \
        assert(state->NAME == NULL); \
        state->NAME = ADD_NEW_EXCEPTION(mod, NAME, BASE); \
        if (state->NAME == NULL) { \
            return -1; \
        } \
    } while (0)

    // An uncaught exception came out of interp_run_string().
    ADD(RunFailedError, PyExc_RuntimeError);
#undef ADD

    return 0;
}

static int
_is_running(PyInterpreterState *interp)
{
    PyThreadState *tstate = PyInterpreterState_ThreadHead(interp);
    if (PyThreadState_Next(tstate) != NULL) {
        PyErr_SetString(PyExc_RuntimeError,
                        "interpreter has more than one thread");
        return -1;
    }

    assert(!PyErr_Occurred());
    return PyThreadState_IsRunning(tstate);
}

static int
_ensure_not_running(PyInterpreterState *interp)
{
    int is_running = _is_running(interp);
    if (is_running < 0) {
        return -1;
    }
    if (is_running) {
        PyErr_Format(PyExc_RuntimeError, "interpreter already running");
        return -1;
    }
    return 0;
}

static int
_run_script(PyInterpreterState *interp, const char *codestr,
            _sharedexception *sharedexc)
{
    PyObject *excval = NULL;
    PyObject *main_mod = _PyInterpreterState_GetMainModule(interp);
    if (main_mod == NULL) {
        goto error;
    }
    PyObject *ns = PyModule_GetDict(main_mod);  // borrowed
    Py_DECREF(main_mod);
    if (ns == NULL) {
        goto error;
    }
    Py_INCREF(ns);

    // Run the string (see PyRun_SimpleStringFlags).
    PyObject *result = PyRun_StringFlags(codestr, Py_file_input, ns, ns, NULL);
    Py_DECREF(ns);
    if (result == NULL) {
        goto error;
    }
    else {
        Py_DECREF(result);  // We throw away the result.
    }

    *sharedexc = no_exception;
    return 0;

error:
    excval = PyErr_GetRaisedException();
    const char *failure = _sharedexception_bind(excval, sharedexc);
    if (failure != NULL) {
        fprintf(stderr,
                "RunFailedError: script raised an uncaught exception (%s)",
                failure);
        PyErr_Clear();
    }
    Py_XDECREF(excval);
    assert(!PyErr_Occurred());
    return -1;
}

static int
_run_script_in_interpreter(PyObject *mod, PyInterpreterState *interp,
                           const char *codestr)
{
    if (_ensure_not_running(interp) < 0) {
        return -1;
    }
    module_state *state = get_module_state(mod);

    // Switch to interpreter.
    PyThreadState *save_tstate = NULL;
    if (interp != PyInterpreterState_Get()) {
        // XXX Using the "head" thread isn't strictly correct.
        PyThreadState *tstate = PyInterpreterState_ThreadHead(interp);
        // XXX Possible GILState issues?
        save_tstate = PyThreadState_Swap(tstate);
    }

    // Run the script.
    _sharedexception exc;
    int result = _run_script(interp, codestr, &exc);

    // Switch back.
    if (save_tstate != NULL) {
        PyThreadState_Swap(save_tstate);
    }

    // Propagate any exception out to the caller.
    if (exc.name != NULL) {
        assert(state != NULL);
        _sharedexception_apply(&exc, state->RunFailedError);
    }
    else if (result != 0) {
        // We were unable to allocate a shared exception.
        PyErr_NoMemory();
    }

    return result;
}


/* module level code ********************************************************/

/* globals is the process-global state for the module.  It holds all
   the data that we need to share between interpreters, so it cannot
   hold PyObject values. */
static struct globals {
    int module_count;
    _runreqs runreqs;
} _globals = {0};

static int
_globals_init(void)
{
    // XXX This isn't thread-safe.
    _globals.module_count++;
    int count = _globals.module_count;
    if (count > 1) {
        // Already initialized.
        return 0;
    }

    assert(_globals.runreqs.mutex == NULL);
    PyThread_type_lock mutex = PyThread_allocate_lock();
    if (mutex == NULL) {
        PyErr_SetString(PyExc_RuntimeError,
                        "could not create run-requests lock");
        return -1;
    }
    _runreqs_init(&_globals.runreqs, mutex);
    return 0;
}

static void
_globals_fini(void)
{
    // XXX This isn't thread-safe.
    _globals.module_count--;
    int count = _globals.module_count;
    if (count > 0) {
        return;
    }

    _runreqs_fini(&_globals.runreqs);
}

static _runreqs *
_global_runreqs(void) {
    return &_globals.runreqss;
}


static PyObject *
interp_create(PyObject *self, PyObject *args, PyObject *kwds)
{

    static char *kwlist[] = {"isolated", NULL};
    int isolated = 1;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|$i:create", kwlist,
                                     &isolated)) {
        return NULL;
    }

    // Create and initialize the new interpreter.
    PyThreadState *save_tstate = PyThreadState_GET();
    const _PyInterpreterConfig config = isolated
        ? (_PyInterpreterConfig)_PyInterpreterConfig_INIT
        : (_PyInterpreterConfig)_PyInterpreterConfig_LEGACY_INIT;
    // XXX Possible GILState issues?
    PyThreadState *tstate = _Py_NewInterpreterFromConfig(&config);
    PyThreadState_Swap(save_tstate);
    if (tstate == NULL) {
        /* Since no new thread state was created, there is no exception to
           propagate; raise a fresh one after swapping in the old thread
           state. */
        PyErr_SetString(PyExc_RuntimeError, "interpreter creation failed");
        return NULL;
    }
    PyInterpreterState *interp = PyThreadState_GetInterpreter(tstate);
    PyObject *idobj = PyInterpreterState_GetIDObject(interp);
    if (idobj == NULL) {
        // XXX Possible GILState issues?
        save_tstate = PyThreadState_Swap(tstate);
        Py_EndInterpreter(tstate);
        PyThreadState_Swap(save_tstate);
        return NULL;
    }
    _PyInterpreterState_RequireIDRef(interp, 1);
    return idobj;
}

PyDoc_STRVAR(create_doc,
"create() -> ID\n\
\n\
Create a new interpreter and return a unique generated ID.");


static PyObject *
interp_destroy(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"id", NULL};
    PyObject *id;
    // XXX Use "L" for id?
    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "O:destroy", kwlist, &id)) {
        return NULL;
    }

    // Look up the interpreter.
    PyInterpreterState *interp = PyInterpreterID_LookUp(id);
    if (interp == NULL) {
        return NULL;
    }

    // Ensure we don't try to destroy the current interpreter.
    PyInterpreterState *current = _get_current_interp();
    if (current == NULL) {
        return NULL;
    }
    if (interp == current) {
        PyErr_SetString(PyExc_RuntimeError,
                        "cannot destroy the current interpreter");
        return NULL;
    }

    // Ensure the interpreter isn't running.
    /* XXX We *could* support destroying a running interpreter but
       aren't going to worry about it for now. */
    if (_ensure_not_running(interp) < 0) {
        return NULL;
    }

    // Destroy the interpreter.
    PyThreadState *tstate = PyInterpreterState_ThreadHead(interp);
    // XXX Possible GILState issues?
    PyThreadState *save_tstate = PyThreadState_Swap(tstate);
    Py_EndInterpreter(tstate);
    PyThreadState_Swap(save_tstate);

    Py_RETURN_NONE;
}

PyDoc_STRVAR(destroy_doc,
"destroy(id)\n\
\n\
Destroy the identified interpreter.\n\
\n\
Attempting to destroy the current interpreter results in a RuntimeError.\n\
So does an unrecognized ID.");


static PyObject *
interp_list_all(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    PyObject *ids, *id;
    PyInterpreterState *interp;

    ids = PyList_New(0);
    if (ids == NULL) {
        return NULL;
    }

    interp = PyInterpreterState_Head();
    while (interp != NULL) {
        id = PyInterpreterState_GetIDObject(interp);
        if (id == NULL) {
            Py_DECREF(ids);
            return NULL;
        }
        // insert at front of list
        int res = PyList_Insert(ids, 0, id);
        Py_DECREF(id);
        if (res < 0) {
            Py_DECREF(ids);
            return NULL;
        }

        interp = PyInterpreterState_Next(interp);
    }

    return ids;
}

PyDoc_STRVAR(list_all_doc,
"list_all() -> [ID]\n\
\n\
Return a list containing the ID of every existing interpreter.");


static PyObject *
interp_get_current(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    PyInterpreterState *interp =_get_current_interp();
    if (interp == NULL) {
        return NULL;
    }
    return PyInterpreterState_GetIDObject(interp);
}

PyDoc_STRVAR(get_current_doc,
"get_current() -> ID\n\
\n\
Return the ID of current interpreter.");


static PyObject *
interp_get_main(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    // Currently, 0 is always the main interpreter.
    int64_t id = 0;
    return PyInterpreterID_New(id);
}

PyDoc_STRVAR(get_main_doc,
"get_main() -> ID\n\
\n\
Return the ID of main interpreter.");


static PyObject *
interp_run_string(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"id", "script", NULL};
    PyObject *id, *code;
    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "OU:run_string", kwlist,
                                     &id, &code)) {
        return NULL;
    }

    // Look up the interpreter.
    PyInterpreterState *interp = PyInterpreterID_LookUp(id);
    if (interp == NULL) {
        return NULL;
    }

    // Extract code.
    Py_ssize_t size;
    const char *codestr = PyUnicode_AsUTF8AndSize(code, &size);
    if (codestr == NULL) {
        return NULL;
    }
    if (strlen(codestr) != (size_t)size) {
        PyErr_SetString(PyExc_ValueError,
                        "source code string cannot contain null bytes");
        return NULL;
    }

    // Run the code in the interpreter.
    if (_run_script_in_interpreter(self, interp, codestr) != 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(run_string_doc,
"run_string(id, script)\n\
\n\
Execute the provided string in the identified interpreter.\n\
(See PyRun_SimpleStrings.)\n\
\n\
It runs in the current OS thread.  The Python thread of the calling\n\
interpreter is completely blocked until this finishes.");

static PyObject *
interp_is_running(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"id", NULL};
    PyObject *id;
    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "O:is_running", kwlist, &id)) {
        return NULL;
    }

    PyInterpreterState *interp = PyInterpreterID_LookUp(id);
    if (interp == NULL) {
        return NULL;
    }
    int is_running = _is_running(interp);
    if (is_running < 0) {
        return NULL;
    }
    if (is_running) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

PyDoc_STRVAR(is_running_doc,
"is_running(id) -> bool\n\
\n\
Return whether or not the identified interpreter is running.\n\
\n\
Specifically, this checks if a run_string() call is still running.\n\
For threads started via run_string_in_thread(), use the request ID\n\
that was returned or call get_run_requests().");


static PyObject *
interp_run_string_background(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"id", "script", NULL};
    PyObject *id, *code;
    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "OU:run_string_background", kwlist,
                                     &id, &code)) {
        return NULL;
    }

    // Look up the interpreter.
    PyInterpreterState *interp = PyInterpreterID_LookUp(id);
    if (interp == NULL) {
        return NULL;
    }

    // Extract code.
    Py_ssize_t size;
    const char *codestr = PyUnicode_AsUTF8AndSize(code, &size);
    if (codestr == NULL) {
        return NULL;
    }
    if (strlen(codestr) != (size_t)size) {
        PyErr_SetString(PyExc_ValueError,
                        "source code string cannot contain null bytes");
        return NULL;
    }

    // XXX
    ...

    // Run the code in the interpreter.
    if (_run_script_in_interpreter(self, interp, codestr) != 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(run_string_background_doc,
"run_string_background(id, script) -> RunRequestID\n\
\n\
Execute the provided string in the identified interpreter.\n\
(See PyRun_SimpleStrings.)\n\
\n\
The code is run in a new thread.  The returned request ID may be used\n\
to check on the status of the execution.");

static PyObject *
interp_get_run_requests(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"id", NULL};
    PyObject *id;
    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "O:is_running", kwlist, &id)) {
        return NULL;
    }

    // XXX
    ...
}

PyDoc_STRVAR(get_run_requests_doc,
"get_run_requests(id) -> [RunRequestID]\n\
\n\
Return the request ID for every known call to run_string_background().");

static PyObject *
interp_get_status(PyObject *self, PyObject *args, PyObject *kwds)
{
    _runreqid_data data = _runreqid_parse_args("get_status",
                                               self, args, kwds);
    if (data.global < 0) {
        return NULL;
    }
    _runreqs *reqs = _global_runreqs();
    _runreq *req = _runreqs_look_up(reqs, interpid, reqid);
    if (req == NULL) {
        return NULL;
    }

    return PyLong_FromLong(req->status);
}

PyDoc_STRVAR(get_run_request_status_doc,
"get_status(reqid, [interpid]) -> int\n\
\n\
Return the current status of the identified interp.run() request.\n\
(0: running, 1: succeeded, -1: failed)");

static PyObject *
interp_get_failure(PyObject *self, PyObject *args, PyObject *kwds)
{
    _runreqid_data data = _runreqid_parse_args("get_failure",
                                               self, args, kwds);
    if (data.global < 0) {
        return NULL;
    }
    _runreqs *reqs = _global_runreqs();
    _runreq *req = _runreqs_look_up(reqs, interpid, reqid);
    if (req == NULL) {
        return NULL;
    }

    // XXX
    ...
}

PyDoc_STRVAR(get_failure_doc,
"get_failure(reqid, [interpid]) -> (name, msg, tb)\n\
\n\
Return the information about a failed interp.run() request.\n\
\n\
This raises RuntimeError if the request is still running or was successful.");

static PyObject *
interp_wait(PyObject *self, PyObject *args, PyObject *kwds)
{
    _runreqid_data data = _runreqid_parse_args("wait",
                                               self, args, kwds);
    if (data.global < 0) {
        return NULL;
    }
    _runreqs *reqs = _global_runreqs();
    _runreq *req = _runreqs_look_up(reqs, interpid, reqid);
    if (req == NULL) {
        return NULL;
    }
    if (_runreq_wait(req) < 0) {
        return NULL;
    }
    return PyLong_FromLong(req->status);
}

PyDoc_STRVAR(wait_doc,
"wait(reqid, [interpid]) -> int\n\
\n\
Return the status of the identified interp.run() request,
after waiting for it to finish.

(See get_status() about the return value.)");


static PyObject *
_runreqid_from_int_for_testing(PyObject *self, PyObject *args, PyObject *kwds)
{
    _runreqid_data data = _runreqid_parse_args("_runreqid_for_testing",
                                               self, args, kwds);
    if (data.global < 0) {
        return NULL;
    }
    module_state *state = get_module_state(self);
    if (state == NULL) {
        return NULL;
    }
    PyTypeObject *cls = state->RunRequestIDType;
    return new_runreqid(cls, data, _globals.runreqs);
}

static PyMethodDef module_functions[] = {
    {"create",                    _PyCFunction_CAST(interp_create),
     METH_VARARGS | METH_KEYWORDS, create_doc},
    {"destroy",                   _PyCFunction_CAST(interp_destroy),
     METH_VARARGS | METH_KEYWORDS, destroy_doc},
    {"list_all",                  interp_list_all,
     METH_NOARGS, list_all_doc},
    {"get_current",               interp_get_current,
     METH_NOARGS, get_current_doc},
    {"get_main",                  interp_get_main,
     METH_NOARGS, get_main_doc},

    {"is_running",                _PyCFunction_CAST(interp_is_running),
     METH_VARARGS | METH_KEYWORDS, is_running_doc},
    {"run_string_foreground",
     _PyCFunction_CAST(interp_run_string_foreground),
     METH_VARARGS | METH_KEYWORDS, run_string_foreground_doc},
    {"run_string_background",
     _PyCFunction_CAST(interp_run_string_background),
     METH_VARARGS | METH_KEYWORDS, run_string_background_doc},
    {"get_status",                _PyCFunction_CAST(interp_get_status),
     METH_VARARGS | METH_KEYWORDS, get_status_doc},
    {"get_failure",               _PyCFunction_CAST(interp_get_failure),
     METH_VARARGS | METH_KEYWORDS, get_failure_doc},
    {"wait",                      _PyCFunction_CAST(interp_wait),
     METH_VARARGS | METH_KEYWORDS, wait_doc},
    {"_runreqid_for_testing",
     _PyCFunction_CAST(_runreqid_from_int_for_testing),
     METH_VARARGS | METH_KEYWORDS, NULL},

    {NULL,                        NULL}           /* sentinel */
};


/* initialization function */

PyDoc_STRVAR(module_doc,
"This module provides primitive operations to manage Python interpreters.\n\
The 'interpreters' module provides a more convenient interface.");

static int
module_exec(PyObject *mod)
{
    if (_globals_init() != 0) {
        return -1;
    }

    /* Add exception types */
    if (exceptions_init(mod) != 0) {
        goto error;
    }

    // PyInterpreterID
    if (PyModule_AddType(mod, &PyInterpreterID_Type) < 0) {
        goto error;
    }

    // RunRequestID
    state->RunRequestIDType = add_new_type(mod, &RunRequestIDType_spec);
    if (state->RunRequestIDType == NULL) {
        goto error;
    }

    return 0;

error:
    _globals_fini();
    return -1;
}

static struct PyModuleDef_Slot module_slots[] = {
    {Py_mod_exec, module_exec},
    {0, NULL},
};

static int
module_traverse(PyObject *mod, visitproc visit, void *arg)
{
    module_state *state = get_module_state(mod);
    assert(state != NULL);
    traverse_module_state(state, visit, arg);
    return 0;
}

static int
module_clear(PyObject *mod)
{
    module_state *state = get_module_state(mod);
    assert(state != NULL);
    clear_module_state(state);
    return 0;
}

static void
module_free(void *mod)
{
    module_state *state = get_module_state(mod);
    assert(state != NULL);
    clear_module_state(state);
    _globals_fini();
}

static struct PyModuleDef moduledef = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = MODULE_NAME,
    .m_doc = module_doc,
    .m_size = sizeof(module_state),
    .m_methods = module_functions,
    .m_slots = module_slots,
    .m_traverse = module_traverse,
    .m_clear = module_clear,
    .m_free = (freefunc)module_free,
};

PyMODINIT_FUNC
PyInit__interpreters(void)
{
    return PyModuleDef_Init(&moduledef);
}
