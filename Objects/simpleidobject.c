/* SimpleID object */

#include "Python.h"
#include "pycore_abstract.h"   // _PyIndex_Check()
#include "pycore_simpleid.h"


/***********************/
/* simple ID metaclass */
/***********************/

typedef struct {
    PyTypeObject base;
} simpleidtype;

PyTypeObject _PySimpleID_Type_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "_SimpleIDType",                /* tp_name */
    sizeof(simpleidtype),           /* tp_basicsize */
    0,                              /* tp_itemsize */
    0,                              /* tp_dealloc */
    0,                              /* tp_vectorcall_offset */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_as_async */
    0,                              /* tp_repr */
    0,                              /* tp_as_number */
    0,                              /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,                              /* tp_hash */
    0,                              /* tp_call */
    0,                              /* tp_str */
    0,                              /* tp_getattro */
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_TYPE_SUBCLASS,  /* tp_flags */
    0,                              /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    0,                              /* tp_members */
    0,                              /* tp_getset */
    &PyType_Type,                   /* tp_base */
};


/**************/
/* simple IDs */
/**************/

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

typedef struct simpleid {
    PyObject_HEAD
    simpleid_t id;
} PySimpleIDObject;

static PySimpleIDObject *
newsimpleid(PyTypeObject *cls, simpleid_t id)
{
    if (!PyType_IsSubtype(cls, &PySimpleID_Type)) {
        PyErr_Format(PyExc_TypeError,
                     "ID must be a SimpleIDType subclass, got %R", cls);
        return NULL;
    }

    PySimpleIDObject *self = PyObject_New(PySimpleIDObject, cls);
    if (self == NULL) {
        return NULL;
    }
    self->id = id;

    return self;
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
    PyVarObject_HEAD_INIT(&_PySimpleID_Type_Type, 0)
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

PyObject *
PySimpleID_New(simpleid_t id)
{
    return (PyObject *)newsimpleid(&PySimpleID_Type, id);
}
