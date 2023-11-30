/* SimpleID object */

#include "Python.h"
#include "pycore_abstract.h"   // _PyIndex_Check()
#include "pycore_simpleid.h"
#include "pycore_typeobject.h"  // _PyType_IsReady()


typedef struct simpleid {
    PyObject_HEAD
    simpleid_t id;
} PySimpleIDObject;


static int
validate_simpleid(simpleid_t id)
{
    if (id < 0) {
        PyErr_Format(PyExc_ValueError,
                     "ID must be a non-negative int, got %lld", id);
        return -1;
    }
    return 0;
}

static int
simpleid_converter(PyObject *arg, void *ptr)
{
    simpleid_t id;
    if (PyObject_TypeCheck(arg, &PySimpleID_Type)) {
        id = ((PySimpleIDObject *)arg)->id;
    }
    else if (_PyIndex_Check(arg)) {
        assert(PY_LLONG_MAX <= Py_SIMPLEID_MAX);
        id = (simpleid_t)PyLong_AsLongLong(arg);
        if (id == -1 && PyErr_Occurred()) {
            return 0;
        }
        if (validate_simpleid(id) < 0) {
            return 0;
        }
    }
    else {
        PyErr_Format(PyExc_TypeError,
                     "ID must be an int, got %.100s",
                     Py_TYPE(arg)->tp_name);
        return 0;
    }
    *(simpleid_t *)ptr = id;
    return 1;
}


/*********************/
/* simple ID objects */
/*********************/

static PySimpleIDObject *
newsimpleid(PyTypeObject *cls, simpleid_t id)
{
    PySimpleIDObject *self = PyObject_New(PySimpleIDObject, cls);
    if (self == NULL) {
        return NULL;
    }
    self->id = id;

    return self;
}

static PyObject *
simpleid_new(PyTypeObject *cls, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"id", NULL};
    simpleid_t id;
    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "O&:SimpleID.__init__", kwlist,
                                     simpleid_converter, &id)) {
        return NULL;
    }

    return (PyObject *)newsimpleid(cls, id);
}

static void
simpleid_dealloc(PyObject *v)
{
    Py_TYPE(v)->tp_free(v);
}

static PyObject *
simpleid_repr(PyObject *self)
{
    PyTypeObject *type = Py_TYPE(self);
    const char *name = _PyType_Name(type);
    PySimpleIDObject *id = (PySimpleIDObject *)self;
    return PyUnicode_FromFormat("%s(%" PRId64 ")", name, id->id);
}

static PyObject *
simpleid_str(PyObject *self)
{
    PySimpleIDObject *id = (PySimpleIDObject *)self;
    return PyUnicode_FromFormat("%" PRId64 "", id->id);
}

static PyObject *
simpleid_int(PyObject *self)
{
    PySimpleIDObject *id = (PySimpleIDObject *)self;
    return PyLong_FromLongLong(id->id);
}

static PyNumberMethods simpleid_as_number = {
     0,                       /* nb_add */
     0,                       /* nb_subtract */
     0,                       /* nb_multiply */
     0,                       /* nb_remainder */
     0,                       /* nb_divmod */
     0,                       /* nb_power */
     0,                       /* nb_negative */
     0,                       /* nb_positive */
     0,                       /* nb_absolute */
     0,                       /* nb_bool */
     0,                       /* nb_invert */
     0,                       /* nb_lshift */
     0,                       /* nb_rshift */
     0,                       /* nb_and */
     0,                       /* nb_xor */
     0,                       /* nb_or */
     (unaryfunc)simpleid_int, /* nb_int */
     0,                       /* nb_reserved */
     0,                       /* nb_float */

     0,                       /* nb_inplace_add */
     0,                       /* nb_inplace_subtract */
     0,                       /* nb_inplace_multiply */
     0,                       /* nb_inplace_remainder */
     0,                       /* nb_inplace_power */
     0,                       /* nb_inplace_lshift */
     0,                       /* nb_inplace_rshift */
     0,                       /* nb_inplace_and */
     0,                       /* nb_inplace_xor */
     0,                       /* nb_inplace_or */

     0,                       /* nb_floor_divide */
     0,                       /* nb_true_divide */
     0,                       /* nb_inplace_floor_divide */
     0,                       /* nb_inplace_true_divide */

     (unaryfunc)simpleid_int, /* nb_index */
};

static Py_hash_t
simpleid_hash(PyObject *self)
{
    PySimpleIDObject *id = (PySimpleIDObject *)self;
    PyObject *obj = PyLong_FromLongLong(id->id);
    if (obj == NULL) {
        return -1;
    }
    Py_hash_t hash = PyObject_Hash(obj);
    Py_DECREF(obj);
    return hash;
}

static PyObject *
simpleid_richcompare(PyObject *self, PyObject *other, int op)
{
    if (op != Py_EQ && op != Py_NE) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    if (!PyObject_TypeCheck(self, &PySimpleID_Type)) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    PySimpleIDObject *id = (PySimpleIDObject *)self;
    int equal;
    if (PyObject_TypeCheck(other, &PySimpleID_Type)) {
        PySimpleIDObject *otherid = (PySimpleIDObject *)other;
        equal = (id->id == otherid->id);
    }
    else if (PyLong_CheckExact(other)) {
        /* Fast path */
        int overflow;
        long long otherid = PyLong_AsLongLongAndOverflow(other, &overflow);
        if (otherid == -1 && PyErr_Occurred()) {
            return NULL;
        }
        equal = !overflow && (otherid >= 0) && (id->id == otherid);
    }
    else if (PyNumber_Check(other)) {
        PyObject *pyid = PyLong_FromLongLong(id->id);
        if (pyid == NULL) {
            return NULL;
        }
        PyObject *res = PyObject_RichCompare(pyid, other, op);
        Py_DECREF(pyid);
        return res;
    }
    else {
        Py_RETURN_NOTIMPLEMENTED;
    }

    if ((op == Py_EQ && equal) || (op == Py_NE && !equal)) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

PyDoc_STRVAR(simpleid_doc,
"A simple ID uniquely identifies some resource and may be used as an int.");

PyTypeObject PySimpleID_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "SimpleID",                     /* tp_name */
    sizeof(PySimpleIDObject),       /* tp_basicsize */
    0,                              /* tp_itemsize */
    (destructor)simpleid_dealloc,   /* tp_dealloc */
    0,                              /* tp_vectorcall_offset */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_as_async */
    (reprfunc)simpleid_repr,        /* tp_repr */
    &simpleid_as_number,            /* tp_as_number */
    0,                              /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    simpleid_hash,                  /* tp_hash */
    0,                              /* tp_call */
    (reprfunc)simpleid_str,         /* tp_str */
    0,                              /* tp_getattro */
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    simpleid_doc,                   /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    simpleid_richcompare,           /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    0,                              /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    simpleid_new,                   /* tp_new */
};


/***********************/
/* simple ID lifetimes */
/***********************/

// We stash extra info in tp_as_buffer:
struct fake_buffer {
    PyBufferProcs ignored;
    struct simpleid_lifetime_t lifetime;
};

static int
set_lifetime(PyTypeObject *cls, struct simpleid_lifetime_t *lifetime)
{
    assert(PyType_IsSubtype(cls, &PySimpleID_Type));
    assert(_PyType_IsReady(cls));
    assert(cls->tp_as_buffer == NULL);
    assert(lifetime != NULL);
    assert(lifetime->incref != NULL);
    assert(lifetime->decref != NULL);

    struct fake_buffer *fake = PyMem_RawMalloc(sizeof(struct fake_buffer));
    if (fake == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    *fake = (struct fake_buffer){
        .lifetime = *lifetime,
    };

    cls->tp_as_buffer = (PyBufferProcs *)fake;
    return 0;
}

static struct simpleid_lifetime_t *
get_lifetime(PyTypeObject *cls)
{
    assert(PyType_IsSubtype(cls, &PySimpleID_Type));
    assert(cls->tp_as_buffer != NULL);
    return &((struct fake_buffer *)cls->tp_as_buffer)->lifetime;
}

static PySimpleIDObject *
newsimpleid_subclass(PyTypeObject *cls, simpleid_t id, int force)
{
    PySimpleIDObject *self = newsimpleid(cls, id);
    if (self == NULL) {
        return NULL;
    }

    struct simpleid_lifetime_t *lifetime = get_lifetime(cls);
    void *value = NULL;
    if (lifetime->init != NULL) {
        if (lifetime->init(lifetime->ctx, id, &value) < 0) {
            Py_DECREF(self);
            return NULL;
        }
    }
    lifetime->incref(lifetime->ctx, id, &value);
    if (value == NULL) {
        // No matching item was found.
        Py_DECREF(self);
        if (!force) {
            PyErr_Format(PyExc_RuntimeError,
                         "ID %lld not found", id);
            return NULL;
        }
    }

    return self;
}

static PyObject *
simpleid_subclass_new(PyTypeObject *cls, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"id", "force", NULL};
    int force = 0;
    simpleid_t id;
    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "O&|$p:SimpleID.__init__", kwlist,
                                     simpleid_converter, &id, &force)) {
        return NULL;
    }

    return (PyObject *)newsimpleid_subclass(cls, id, force);
}

static void
simpleid_subclass_dealloc(PyObject *v)
{
    struct simpleid_lifetime_t *lifetime = get_lifetime(Py_TYPE(v));
    simpleid_t id = ((PySimpleIDObject *)v)->id;
    lifetime->decref(lifetime->ctx, id, NULL);

    simpleid_dealloc(v);
}

PyTypeObject *
_PySimpleID_NewSubclass(const char *name, PyObject *module, const char *doc,
                        struct simpleid_lifetime_t *lifetime)
{
    PyType_Slot slots[] = {
        {Py_tp_new, simpleid_subclass_new},
        {Py_tp_dealloc, simpleid_subclass_dealloc},
        {Py_tp_doc, (void *)doc},
        {0, NULL},
    };
    PyType_Spec spec = {
        .name = name,
        .slots = slots,
    };
    PyObject *bases = (PyObject *)&PySimpleID_Type;
    PyObject *cls = PyType_FromMetaclass(NULL, module, &spec, bases);
    if (cls == NULL) {
        return NULL;
    }

    if (set_lifetime((PyTypeObject *)cls, lifetime) < 0) {
        Py_DECREF(cls);
        return NULL;
    }

    return (PyTypeObject *)cls;
}
int
_PySimpleID_SubclassInitialized(PyTypeObject *cls)
{
    assert(cls->tp_flags & _Py_TPFLAGS_STATIC_BUILTIN);
    return cls->tp_as_buffer != NULL;
}

int
_PySimpleID_InitSubclass(PyTypeObject *cls, struct simpleid_lifetime_t *lifetime)
{
    assert(cls->tp_flags & _Py_TPFLAGS_STATIC_BUILTIN);
    assert(_PyType_IsReady(cls));
    assert(cls->tp_as_buffer == NULL);
    return set_lifetime(cls, lifetime);
}

PyObject *
_PySimpleID_tp_new(PyTypeObject *cls, PyObject *args, PyObject *kwds)
{
    return simpleid_subclass_new(cls, args, kwds);
}

void
_PySimpleID_tp_dealloc(PyObject *v)
{
    simpleid_subclass_dealloc(v);
}


/***********************/
/* other API functions */
/***********************/

int
_PySimpleID_converter(PyObject *arg, void *ptr)
{
    return simpleid_converter(arg, ptr);
}

PyObject *
PySimpleID_New(simpleid_t id, PyTypeObject *subclass)
{
    PyTypeObject *cls = &PySimpleID_Type;
    if (subclass == NULL) {
        return (PyObject *)newsimpleid(cls, id);
    }

    if (!PyType_IsSubtype(subclass, &PySimpleID_Type)) {
        PyErr_Format(PyExc_TypeError,
                     "expected a SimpleID subclass, got %R", subclass);
        return NULL;
    }
    return (PyObject *)newsimpleid_subclass(subclass, id, 0);
}
