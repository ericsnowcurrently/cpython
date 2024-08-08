
static crossinterpdatafunc _lookup_getdata_from_registry(
                                            PyInterpreterState *, PyObject *);

static crossinterpdatafunc
lookup_getdata(PyInterpreterState *interp, PyObject *obj)
{
   /* Cross-interpreter objects are looked up by exact match on the class.
      We can reassess this policy when we move from a global registry to a
      tp_* slot. */
    return _lookup_getdata_from_registry(interp, obj);
}

crossinterpdatafunc
_PyCrossInterpreterData_Lookup(PyObject *obj)
{
    PyInterpreterState *interp = PyInterpreterState_Get();
    return lookup_getdata(interp, obj);
}


/**************************************/
/* Cross-interpreter-supporting types */
/**************************************/

static int
_xidtype_check(const struct _xidtype_def *def)
{
    if (def->dflt.getdata == NULL) {
        assert(def->dflt.filter == NULL);
        PyErr_Format(PyExc_ValueError, "missing default getdata");
        return -1;
    }
    /* The default def->filter may be NULL. */

    return 0;
}

static int
_xidtype_init(struct _xidtype_def *def,
              const struct _xidtype_spec *dflt)
{
    if (def->dflt.getdata != NULL) {
        PyErr_Format(PyExc_ValueError, "XID type def already initialized");
        return -1;
    }

    *def = (struct _xidtype_def){
        .dflt = *dflt,
    };
    if (_xidtype_check(def) < 0) {
        return -1;
    }
    return 0;
}

static int
_xidtype_match(const struct _xidtype_def *def, const struct _xidtype_spec *spec)
{
    assert(spec->getdata != NULL);

    if (spec->filter == NULL) {
        if (def->dflt.filter != NULL) {
            assert(def->dflt.getdata != spec->getdata);
            if (def->dflt.getdata == spec->getdata) {
                PyErr_Format(PyExc_ValueError, "spec missing filter");
            }
            return 0;
        }
        if (def->dflt.getdata != spec->getdata) {
            return 0;
        }
        return 1;
    }

    if (def->dflt.filter == spec->filter) {
        assert(def->dflt.getdata == spec->getdata);
        if (def->dflt.getdata != spec->getdata) {
            PyErr_Format(PyExc_ValueError, "mismatch on getdata");
            return 0;
        }
        return 1;
    }
    assert(def->dflt.getdata != spec->getdata);
    if (def->dflt.getdata == spec->getdata) {
        PyErr_Format(PyExc_ValueError, "mismatch on filter");
    }
    return 0;
}

static crossinterpdatafunc
_xidtype_resolve_getdata(struct _xidtype_def *def, PyObject *obj)
{
    PyThreadState *tstate = _PyThreadState_GET();
    if (def->dflt.filter == NULL || def->dflt.filter(tstate, obj)) {
        return def->dflt.getdata;
    }
    return NULL;
}


/***********************************************/
/* a registry of {type -> crossinterpdatafunc} */
/***********************************************/

/* For now we use a global registry of shareable classes.  An
   alternative would be to add a tp_* slot for a class's
   crossinterpdatafunc. It would be simpler and more efficient.  */


/* registry lifecycle */

static void _register_builtins_for_crossinterpreter_data(struct _xidregistry *);

static void
_xidregistry_init(struct _xidregistry *registry)
{
    if (registry->initialized) {
        return;
    }
    registry->initialized = 1;

    if (registry->global) {
        // Registering the builtins is cheap so we don't bother doing it lazily.
        assert(registry->head == NULL);
        _register_builtins_for_crossinterpreter_data(registry);
    }
}

static void _xidregistry_clear(struct _xidregistry *);

static void
_xidregistry_fini(struct _xidregistry *registry)
{
    if (!registry->initialized) {
        return;
    }
    registry->initialized = 0;

    _xidregistry_clear(registry);
}

static inline struct _xidregistry * _get_global_xidregistry(_PyRuntimeState *);
static inline struct _xidregistry * _get_xidregistry(PyInterpreterState *);

static void
xid_lookup_init(PyInterpreterState *interp)
{
    if (_Py_IsMainInterpreter(interp)) {
        _xidregistry_init(_get_global_xidregistry(interp->runtime));
    }
    _xidregistry_init(_get_xidregistry(interp));
}

static void
xid_lookup_fini(PyInterpreterState *interp)
{
    _xidregistry_fini(_get_xidregistry(interp));
    if (_Py_IsMainInterpreter(interp)) {
        _xidregistry_fini(_get_global_xidregistry(interp->runtime));
    }
}


/* registry thread safety */

static void
_xidregistry_lock(struct _xidregistry *registry)
{
    if (registry->global) {
        PyMutex_Lock(&registry->mutex);
    }
    // else: Within an interpreter we rely on the GIL instead of a separate lock.
}

static void
_xidregistry_unlock(struct _xidregistry *registry)
{
    if (registry->global) {
        PyMutex_Unlock(&registry->mutex);
    }
}


/* accessing the registry */

static inline struct _xidregistry *
_get_global_xidregistry(_PyRuntimeState *runtime)
{
    return &runtime->xi.registry;
}

static inline struct _xidregistry *
_get_xidregistry(PyInterpreterState *interp)
{
    return &interp->xi.registry;
}

static inline struct _xidregistry *
_get_xidregistry_for_type(PyInterpreterState *interp, PyTypeObject *cls)
{
    struct _xidregistry *registry = _get_global_xidregistry(interp->runtime);
    if (cls->tp_flags & Py_TPFLAGS_HEAPTYPE) {
        registry = _get_xidregistry(interp);
    }
    return registry;
}

static struct _xidregitem * _xidregistry_remove_entry(
        struct _xidregistry *, struct _xidregitem *);

static struct _xidregitem *
_xidregistry_find_type(struct _xidregistry *xidregistry, PyTypeObject *cls)
{
    struct _xidregitem *cur = xidregistry->head;
    while (cur != NULL) {
        if (cur->weakref != NULL) {
            // cur is/was a heap type.
            PyObject *registered = _PyWeakref_GET_REF(cur->weakref);
            if (registered == NULL) {
                // The weakly ref'ed object was freed.
                cur = _xidregistry_remove_entry(xidregistry, cur);
                continue;
            }
            assert(PyType_Check(registered));
            assert(cur->cls == (PyTypeObject *)registered);
            assert(cur->cls->tp_flags & Py_TPFLAGS_HEAPTYPE);
            Py_DECREF(registered);
        }
        if (cur->cls == cls) {
            assert(cur->tracking.dflt.refcount > 0);
            assert(cur->def.dflt.getdata != NULL);
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

static crossinterpdatafunc
_lookup_getdata_from_registry(PyInterpreterState *interp, PyObject *obj)
{
    crossinterpdatafunc func = NULL;
    PyTypeObject *cls = Py_TYPE(obj);

    struct _xidregistry *xidregistry = _get_xidregistry_for_type(interp, cls);
    _xidregistry_lock(xidregistry);

    struct _xidregitem *matched = _xidregistry_find_type(xidregistry, cls);
    if (matched != NULL) {
        func = _xidtype_resolve_getdata(&matched->def, obj);
    }

    _xidregistry_unlock(xidregistry);
    return func;
}


/* updating the registry */

static int
_xidregitem_init(struct _xidregitem *entry,
                 PyTypeObject *cls, const struct _xidtype_spec *spec)
{
    PyObject *ref = NULL;
    if (cls->tp_flags & Py_TPFLAGS_HEAPTYPE) {
        // XXX Assign a callback to clear the entry from the registry?
        ref = PyWeakref_NewRef((PyObject *)cls, NULL);
        if (ref == NULL) {
            return -1;
        }
    }
    *entry = (struct _xidregitem){
        // We do not keep a reference, to avoid keeping the class alive.
        .cls = cls,
        .weakref = ref,
        .tracking = {
            .dflt = {
                .refcount = 1,
            },
            .next_id = 1,
        },
    };
    if (_xidtype_init(&entry->def, spec) < 0) {
        Py_XDECREF(ref);
        return -1;
    }
    return 0;
}

static void
_xidregitem_clear(struct _xidregitem *entry)
{
    Py_XDECREF(entry->weakref);
}

static struct _xidregitem *
_xidregistry_add_entry(struct _xidregistry *xidregistry,
                       PyTypeObject *cls, const struct _xidtype_spec *spec)
{
    struct _xidregitem *newhead = PyMem_RawMalloc(sizeof(struct _xidregitem));
    if (newhead == NULL) {
        return NULL;
    }
    if (_xidregitem_init(newhead, cls, spec) < 0) {
        PyMem_RawFree(newhead);
        return NULL;
    }
    newhead->next = xidregistry->head;
    if (newhead->next != NULL) {
        newhead->next->prev = newhead;
    }
    xidregistry->head = newhead;
    return newhead;
}

static struct _xidregitem *
_xidregistry_remove_entry(struct _xidregistry *xidregistry,
                          struct _xidregitem *entry)
{
    struct _xidregitem *next = entry->next;
    if (entry->prev != NULL) {
        assert(entry->prev->next == entry);
        entry->prev->next = next;
    }
    else {
        assert(xidregistry->head == entry);
        xidregistry->head = next;
    }
    if (next != NULL) {
        next->prev = entry->prev;
    }
    _xidregitem_clear(entry);
    PyMem_RawFree(entry);
    return next;
}

static void
_xidregistry_clear(struct _xidregistry *xidregistry)
{
    struct _xidregitem *cur = xidregistry->head;
    xidregistry->head = NULL;
    while (cur != NULL) {
        struct _xidregitem *next = cur->next;
        _xidregitem_clear(cur);
        PyMem_RawFree(cur);
        cur = next;
    }
}

int
_PyCrossInterpreterData_RegisterClass(PyTypeObject *cls,
                                      const struct _xidtype_spec *spec,
                                      uint64_t *p_id)
{
    if (!PyType_Check(cls)) {
        PyErr_Format(PyExc_ValueError, "only classes may be registered");
        return -1;
    }
    if (spec == NULL) {
        PyErr_Format(PyExc_ValueError, "missing registry spec");
        return -1;
    }

    int res = -1;
    uint64_t id = 0;
    PyInterpreterState *interp = _PyInterpreterState_GET();
    struct _xidregistry *xidregistry = _get_xidregistry_for_type(interp, cls);
    _xidregistry_lock(xidregistry);

    struct _xidregitem *entry = _xidregistry_find_type(xidregistry, cls);
    if (entry == NULL) {
        entry = _xidregistry_add_entry(xidregistry, cls, spec);
        if (entry == NULL) {
            goto finally;
        }
    }
    else {
        struct _xidregtype_tracking *tracking = &entry->tracking;
        assert(tracking->dflt.refcount > 0);
        if (_xidtype_match(&entry->def, spec)) {
            struct _xidregtype *rt = &tracking->dflt;
            assert(rt->refcount > 0);
            rt->refcount += 1;
        }
        else {
            PyErr_Format(PyExc_RuntimeError,
                         "type %s already registered", cls->tp_name);
            goto finally;
        }
    }

    res = 0;

finally:
    _xidregistry_unlock(xidregistry);
    if (p_id != NULL && id > 0) {
        *p_id = id;
    }
    return res;
}

int
_PyCrossInterpreterData_UnregisterClass(PyTypeObject *cls, uint64_t id)
{
    int res = -1;
    PyInterpreterState *interp = _PyInterpreterState_GET();
    struct _xidregistry *xidregistry = _get_xidregistry_for_type(interp, cls);
    _xidregistry_lock(xidregistry);

    struct _xidregitem *matched = _xidregistry_find_type(xidregistry, cls);
    if (matched == NULL) {
        goto finally;
    }
    struct _xidregtype_tracking *tracking = &matched->tracking;
    struct _xidtype_def *def = &matched->def;
    assert(tracking->dflt.refcount > 0);

    if (id == 0) {
        assert(tracking->dflt.id == 0);
        assert(def->dflt.getdata != NULL);
        struct _xidregtype *rt = &tracking->dflt;
        assert(rt->refcount > 0);
        rt->refcount -= 1;
        if (rt->refcount == 0) {
            (void)_xidregistry_remove_entry(xidregistry, matched);
        }
    }
    else {
        PyErr_Format(PyExc_ValueError, "unrecognized ID %d", id);
        goto finally;
    }

    res = 0;

finally:
    _xidregistry_unlock(xidregistry);
    return res;
}


/********************************************/
/* cross-interpreter data for builtin types */
/********************************************/

// bytes

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
_bytes_shared(PyThreadState *tstate, PyObject *obj,
              _PyCrossInterpreterData *data)
{
    if (_PyCrossInterpreterData_InitWithSize(
            data, tstate->interp, sizeof(struct _shared_bytes_data), obj,
            _new_bytes_object
            ) < 0)
    {
        return -1;
    }
    struct _shared_bytes_data *shared = (struct _shared_bytes_data *)data->data;
    if (PyBytes_AsStringAndSize(obj, &shared->bytes, &shared->len) < 0) {
        _PyCrossInterpreterData_Clear(tstate->interp, data);
        return -1;
    }
    return 0;
}

// str

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
_str_shared(PyThreadState *tstate, PyObject *obj,
            _PyCrossInterpreterData *data)
{
    if (_PyCrossInterpreterData_InitWithSize(
            data, tstate->interp, sizeof(struct _shared_str_data), obj,
            _new_str_object
            ) < 0)
    {
        return -1;
    }
    struct _shared_str_data *shared = (struct _shared_str_data *)data->data;
    shared->kind = PyUnicode_KIND(obj);
    shared->buffer = PyUnicode_DATA(obj);
    shared->len = PyUnicode_GET_LENGTH(obj);
    return 0;
}

// int

static PyObject *
_new_long_object(_PyCrossInterpreterData *data)
{
    return PyLong_FromSsize_t((Py_ssize_t)(data->data));
}

static int
_long_shared(PyThreadState *tstate, PyObject *obj,
             _PyCrossInterpreterData *data)
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
    _PyCrossInterpreterData_Init(data, tstate->interp, (void *)value, NULL,
            _new_long_object);
    // data->obj and data->free remain NULL
    return 0;
}

// float

static PyObject *
_new_float_object(_PyCrossInterpreterData *data)
{
    double * value_ptr = data->data;
    return PyFloat_FromDouble(*value_ptr);
}

static int
_float_shared(PyThreadState *tstate, PyObject *obj,
             _PyCrossInterpreterData *data)
{
    if (_PyCrossInterpreterData_InitWithSize(
            data, tstate->interp, sizeof(double), NULL,
            _new_float_object
            ) < 0)
    {
        return -1;
    }
    double *shared = (double *)data->data;
    *shared = PyFloat_AsDouble(obj);
    return 0;
}

// None

static PyObject *
_new_none_object(_PyCrossInterpreterData *data)
{
    // XXX Singleton refcounts are problematic across interpreters...
    return Py_NewRef(Py_None);
}

static int
_none_shared(PyThreadState *tstate, PyObject *obj,
             _PyCrossInterpreterData *data)
{
    _PyCrossInterpreterData_Init(data, tstate->interp, NULL, NULL,
            _new_none_object);
    // data->data, data->obj and data->free remain NULL
    return 0;
}

// bool

static PyObject *
_new_bool_object(_PyCrossInterpreterData *data)
{
    if (data->data){
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static int
_bool_shared(PyThreadState *tstate, PyObject *obj,
             _PyCrossInterpreterData *data)
{
    _PyCrossInterpreterData_Init(data, tstate->interp,
            (void *) (Py_IsTrue(obj) ? (uintptr_t) 1 : (uintptr_t) 0), NULL,
            _new_bool_object);
    // data->obj and data->free remain NULL
    return 0;
}

// tuple

struct _shared_tuple_data {
    Py_ssize_t len;
    _PyCrossInterpreterData **data;
};

static PyObject *
_new_tuple_object(_PyCrossInterpreterData *data)
{
    struct _shared_tuple_data *shared = (struct _shared_tuple_data *)(data->data);
    PyObject *tuple = PyTuple_New(shared->len);
    if (tuple == NULL) {
        return NULL;
    }

    for (Py_ssize_t i = 0; i < shared->len; i++) {
        PyObject *item = _PyCrossInterpreterData_NewObject(shared->data[i]);
        if (item == NULL){
            Py_DECREF(tuple);
            return NULL;
        }
        PyTuple_SET_ITEM(tuple, i, item);
    }
    return tuple;
}

static void
_tuple_shared_free(void* data)
{
    struct _shared_tuple_data *shared = (struct _shared_tuple_data *)(data);
#ifndef NDEBUG
    int64_t interpid = PyInterpreterState_GetID(_PyInterpreterState_GET());
#endif
    for (Py_ssize_t i = 0; i < shared->len; i++) {
        if (shared->data[i] != NULL) {
            assert(_PyCrossInterpreterData_INTERPID(shared->data[i]) == interpid);
            _PyCrossInterpreterData_Release(shared->data[i]);
            PyMem_RawFree(shared->data[i]);
            shared->data[i] = NULL;
        }
    }
    PyMem_Free(shared->data);
    PyMem_RawFree(shared);
}

static int
_tuple_shared(PyThreadState *tstate, PyObject *obj,
             _PyCrossInterpreterData *data)
{
    Py_ssize_t len = PyTuple_GET_SIZE(obj);
    if (len < 0) {
        return -1;
    }
    struct _shared_tuple_data *shared = PyMem_RawMalloc(sizeof(struct _shared_tuple_data));
    if (shared == NULL){
        PyErr_NoMemory();
        return -1;
    }

    shared->len = len;
    shared->data = (_PyCrossInterpreterData **) PyMem_Calloc(shared->len, sizeof(_PyCrossInterpreterData *));
    if (shared->data == NULL) {
        PyErr_NoMemory();
        return -1;
    }

    for (Py_ssize_t i = 0; i < shared->len; i++) {
        _PyCrossInterpreterData *data = _PyCrossInterpreterData_New();
        if (data == NULL) {
            goto error;  // PyErr_NoMemory already set
        }
        PyObject *item = PyTuple_GET_ITEM(obj, i);

        int res = -1;
        if (!_Py_EnterRecursiveCallTstate(tstate, " while sharing a tuple")) {
            res = _PyObject_GetCrossInterpreterData(item, data);
            _Py_LeaveRecursiveCallTstate(tstate);
        }
        if (res < 0) {
            PyMem_RawFree(data);
            goto error;
        }
        shared->data[i] = data;
    }
    _PyCrossInterpreterData_Init(
            data, tstate->interp, shared, obj, _new_tuple_object);
    data->free = _tuple_shared_free;
    return 0;

error:
    _tuple_shared_free(shared);
    return -1;
}

// type

static int
_type_filter(PyThreadState *tstate, PyObject *obj)
{
    assert(PyType_Check(obj));
    PyTypeObject *type = (PyTypeObject *)obj;

    if (!_PyType_HasFeature(type, _Py_TPFLAGS_STATIC_BUILTIN)) {
        return 0;
    }
    return 1;
}

static PyObject *
_new_type_object(_PyCrossInterpreterData *data)
{
    PyObject *type = (PyObject *)data->data;
    assert(PyType_Check(type));
    assert(_PyType_HasFeature(
                (PyTypeObject *)type, _Py_TPFLAGS_STATIC_BUILTIN));
    return type;
}

static int
_type_shared(PyThreadState *tstate, PyObject *obj,
             _PyCrossInterpreterData *data)
{
    assert(_type_filter(tstate, obj));
    PyTypeObject *type = (PyTypeObject *)obj;

    // Since we're only dealing with static builtin types,
    // we don't need to tie the shared data to an object (or interpreter).
    PyInterpreterState *interp = NULL;
    obj = NULL;
    _PyCrossInterpreterData_Init(data, interp, type, obj, _new_type_object);
    return 0;
}

// registration

static void
_register_builtins_for_crossinterpreter_data(struct _xidregistry *xidregistry)
{
    static const struct regspec {
        const char *name;
        PyTypeObject *type;
        struct _xidtype_spec xid;
    } builtins[] = {
        {"None", &_PyNone_Type, {NULL, _none_shared}},
        {"int", &PyLong_Type, {NULL, _long_shared}},
        {"bytes", &PyBytes_Type, {NULL, _bytes_shared}},
        {"str", &PyUnicode_Type, {NULL, _str_shared}},
        {"bool", &PyBool_Type, {NULL, _bool_shared}},
        {"float", &PyFloat_Type, {NULL, _float_shared}},
        {"tuple", &PyTuple_Type, {NULL, _tuple_shared}},
        {"type", &PyType_Type, {_type_filter, _type_shared}},
        {NULL},
    };

    for (const struct regspec *spec = builtins; spec->name != NULL; spec += 1) {
        if (_xidregistry_add_entry(xidregistry, spec->type, &spec->xid) < 0) {
            PyErr_PrintEx(0);
            _Py_FatalErrorFormat(
                    __func__,
                    "could not register %s for cross-interpreter sharing",
                    spec->name);
        }
    }
}
