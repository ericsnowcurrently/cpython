/* InterpreterID object */

#include "Python.h"
#include "pycore_interp.h"     // _PyInterpreterState_LookUpID()
#include "pycore_simpleid.h"   // _PySimpleID_NewSubclass()
#include "interpreteridobject.h"


PyDoc_STRVAR(interpid_doc,
"A interpreter ID identifies a interpreter and may be used as an int.");

// We preserve the static type for public C-API compatibility.
PyTypeObject PyInterpreterID_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "InterpreterID",
    .tp_doc = interpid_doc,
    .tp_base = &PySimpleID_Type,
    .tp_new = _PySimpleID_tp_new,
    .tp_dealloc = (destructor)_PySimpleID_tp_dealloc,
};


static PyInterpreterState *
get_interp(simpleid_t id, void **p_value)
{
    if (p_value != NULL && *p_value != NULL) {
#ifndef NDEBUG
        PyInterpreterState *interp = _PyInterpreterState_LookUpID((int64_t)id);
        assert(interp != NULL && (void *)interp == *p_value);
        return interp;
#else
        return *p_value;
#endif
    }
    return _PyInterpreterState_LookUpID(id);
}

static int
lifetime_init(void *ctx, simpleid_t id, void **p_value)
{
    assert(sizeof(simpleid_t) <= sizeof(int64_t));
    PyInterpreterState *interp = get_interp(id, p_value);
    if (interp == NULL) {
        assert(PyErr_Occurred());
        return -1;
    }
    if (_PyInterpreterState_IDInitref(interp) < 0) {
        return -1;
    }
    *p_value = (void *)interp;
    return 0;
}

static void
lifetime_incref(void *ctx, simpleid_t id, void **p_value)
{
    assert(sizeof(simpleid_t) <= sizeof(int64_t));
    PyInterpreterState *interp = get_interp(id, p_value);
    if (interp != NULL) {
        _PyInterpreterState_IDIncref(interp);
    }
    else {
        // This should have been caught by lifetime_init().
        // There is an ever-so-slight race here, thoough.
        // It might have been deleted already too.
        PyErr_Clear();
    }
    if (p_value != NULL) {
        *p_value = interp;
    }
}

static void
lifetime_decref(void *ctx, simpleid_t id, void **p_value)
{
    assert(sizeof(simpleid_t) <= sizeof(int64_t));
    PyInterpreterState *interp = get_interp(id, p_value);
    if (interp != NULL) {
        _PyInterpreterState_IDDecref(interp);
    }
    else {
        // already deleted
        PyErr_Clear();
    }
    if (p_value != NULL) {
        *p_value = interp;
    }
}

struct simpleid_lifetime_t idlifetimes = {
    // ctx is NULL since we use global state.
    .init = lifetime_init,
    .incref = lifetime_incref,
    .decref = lifetime_decref,
};

static int
ensure_initialized(void)
{
    PyTypeObject *cls = &PyInterpreterID_Type;
    if (_PySimpleID_SubclassInitialized(cls)) {
        return 0;
    }

    if (_PySimpleID_InitSubclass(cls, &idlifetimes) < 0) {
        return -1;
    }
    return 0;
}

PyObject *
PyInterpreterID_New(int64_t id)
{
    if (ensure_initialized() < 0) {
        return NULL;
    }
    assert(id <= Py_SIMPLEID_MAX);
    return PySimpleID_New((simpleid_t)id, &PyInterpreterID_Type);
}

PyObject *
PyInterpreterState_GetIDObject(PyInterpreterState *interp)
{
    if (ensure_initialized() < 0) {
        return NULL;
    }
    int64_t id = PyInterpreterState_GetID(interp);
    if (id < 0) {
        return NULL;
    }
    if (_PyInterpreterState_IDInitref(interp) < 0) {
        return NULL;
    }
    assert(id <= Py_SIMPLEID_MAX);
    return PySimpleID_New((simpleid_t)id, &PyInterpreterID_Type);
}

PyInterpreterState *
PyInterpreterID_LookUp(PyObject *requested_id)
{
    int64_t id;
    if (!_PySimpleID_converter(requested_id, &id)) {
        return NULL;
    }
    return _PyInterpreterState_LookUpID(id);
}
