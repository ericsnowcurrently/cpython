
/* API for managing interactions between isolated interpreters */

#include "Python.h"
#include "osdefs.h"               // MAXPATHLEN
#include "marshal.h"              // PyMarshal_WriteObjectToString()
#include "pycore_ceval.h"         // _Py_simple_func
#include "pycore_crossinterp.h"   // _PyXIData_t
#include "pycore_initconfig.h"    // _PyStatus_OK()
#include "pycore_namespace.h"     // _PyNamespace_New()
#include "pycore_setobject.h"     // _PySet_NextEntry()
#include "pycore_typeobject.h"    // _PyStaticType_InitBuiltin()


#define XIDWRAPPER_CLASSNAME "CrossInterpreterObjectData"


static Py_ssize_t
_Py_GetMainfile(char *buffer, size_t maxlen)
{
    // We don't expect subinterpreters to have the __main__ module's
    // __name__ set, but proceed just in case.
    PyThreadState *tstate = _PyThreadState_GET();
    PyObject *module = _Py_GetMainModule(tstate);
    if (_Py_CheckMainModule(module) < 0) {
        return -1;
    }
    return _PyModule_GetFilenameUTF8(module, buffer, maxlen);
}


/**************/
/* exceptions */
/**************/

typedef struct xi_exceptions exceptions_t;
static int init_static_exctypes(exceptions_t *, PyInterpreterState *);
static void fini_static_exctypes(exceptions_t *, PyInterpreterState *);
static int init_heap_exctypes(exceptions_t *);
static void fini_heap_exctypes(exceptions_t *);
#include "crossinterp_exceptions.h"


/***************************/
/* cross-interpreter calls */
/***************************/

int
_Py_CallInInterpreter(PyInterpreterState *interp,
                      _Py_simple_func func, void *arg)
{
    if (interp == PyInterpreterState_Get()) {
        return func(arg);
    }
    // XXX Emit a warning if this fails?
    _PyEval_AddPendingCall(interp, (_Py_pending_call_func)func, arg, 0);
    return 0;
}

int
_Py_CallInInterpreterAndRawFree(PyInterpreterState *interp,
                                _Py_simple_func func, void *arg)
{
    if (interp == PyInterpreterState_Get()) {
        int res = func(arg);
        PyMem_RawFree(arg);
        return res;
    }
    // XXX Emit a warning if this fails?
    _PyEval_AddPendingCall(interp, func, arg, _Py_PENDING_RAWFREE);
    return 0;
}


/**************************/
/* cross-interpreter data */
/**************************/

/* registry of {type -> xidatafunc} */

/* For now we use a global registry of shareable classes.  An
   alternative would be to add a tp_* slot for a class's
   xidatafunc. It would be simpler and more efficient. */

static void xid_lookup_init(_PyXIData_lookup_t *);
static void xid_lookup_fini(_PyXIData_lookup_t *);
struct _dlcontext;
static xidatafunc lookup_getdata(struct _dlcontext *, PyObject *);
#include "crossinterp_data_lookup.h"


/* lifecycle */

_PyXIData_t *
_PyXIData_New(void)
{
    _PyXIData_t *xid = PyMem_RawCalloc(1, sizeof(_PyXIData_t));
    if (xid == NULL) {
        PyErr_NoMemory();
    }
    return xid;
}

void
_PyXIData_Free(_PyXIData_t *xid)
{
    PyInterpreterState *interp = PyInterpreterState_Get();
    _PyXIData_Clear(interp, xid);
    PyMem_RawFree(xid);
}


/* defining cross-interpreter data */

static inline void
_xidata_init(_PyXIData_t *xidata)
{
    // If the value is being reused
    // then _xidata_clear() should have been called already.
    assert(xidata->data == NULL);
    assert(xidata->obj == NULL);
    *xidata = (_PyXIData_t){0};
    _PyXIData_INTERPID(xidata) = -1;
}

static inline void
_xidata_clear(_PyXIData_t *xidata)
{
    // _PyXIData_t only has two members that need to be
    // cleaned up, if set: "xidata" must be freed and "obj" must be decref'ed.
    // In both cases the original (owning) interpreter must be used,
    // which is the caller's responsibility to ensure.
    if (xidata->data != NULL) {
        if (xidata->free != NULL) {
            xidata->free(xidata->data);
        }
        xidata->data = NULL;
    }
    Py_CLEAR(xidata->obj);
}

void
_PyXIData_Init(_PyXIData_t *xidata,
               PyInterpreterState *interp,
               void *shared, PyObject *obj,
               xid_newobjfunc new_object)
{
    assert(xidata != NULL);
    assert(new_object != NULL);
    _xidata_init(xidata);
    xidata->data = shared;
    if (obj != NULL) {
        assert(interp != NULL);
        // released in _PyXIData_Clear()
        xidata->obj = Py_NewRef(obj);
    }
    // Ideally every object would know its owning interpreter.
    // Until then, we have to rely on the caller to identify it
    // (but we don't need it in all cases).
    _PyXIData_INTERPID(xidata) = (interp != NULL)
        ? PyInterpreterState_GetID(interp)
        : -1;
    xidata->new_object = new_object;
}

int
_PyXIData_InitWithSize(_PyXIData_t *xidata,
                       PyInterpreterState *interp,
                       const size_t size, PyObject *obj,
                       xid_newobjfunc new_object)
{
    assert(size > 0);
    // For now we always free the shared data in the same interpreter
    // where it was allocated, so the interpreter is required.
    assert(interp != NULL);
    _PyXIData_Init(xidata, interp, NULL, obj, new_object);
    xidata->data = PyMem_RawCalloc(1, size);
    if (xidata->data == NULL) {
        return -1;
    }
    xidata->free = PyMem_RawFree;
    return 0;
}

void
_PyXIData_Clear(PyInterpreterState *interp, _PyXIData_t *xidata)
{
    assert(xidata != NULL);
    // This must be called in the owning interpreter.
    assert(interp == NULL
           || _PyXIData_INTERPID(xidata) == -1
           || _PyXIData_INTERPID(xidata) == PyInterpreterState_GetID(interp));
    _xidata_clear(xidata);
}


/* wrapped data (unwrapper) */

static PyObject *
unwrapper_call(PyThreadState *tstate,
               _PyXIData_unwrapper_t *unwrapper, PyObject *obj)
{
    return unwrapper->func(tstate, unwrapper->data, obj);
}

static void
unwrapper_clear(_PyXIData_unwrapper_t *unwrapper)
{
    if (unwrapper->free != NULL && unwrapper->data != NULL) {
        unwrapper->free(unwrapper->data);
    }
    *unwrapper = (_PyXIData_unwrapper_t){0};
}


static int
verify_xidata_unwrap_object(PyObject *unwrap)
{
    // XXX
    PyErr_SetString(PyExc_NotImplementedError, "TBD");
    return -1;
}

static int
xidata_unwrapper_from_object(PyThreadState *tstate, PyObject *obj,
                            _PyXIData_unwrapper_t *res_unwrapper)
{
    // XXX
    PyErr_SetString(PyExc_NotImplementedError, "TBD");
    return -1;
}


//static int _xidata_pickle(PyObject *obj, _PyXIData_t *data);
//static PyObject * _xidata_unpickle(_PyXIData_t *data);
//
//static PyObject *
//_unwrap_default_func(void *data, PyObject *wrapped)
//{
//    PyObject *unwrapobj = _xidata_unpickle((_PyXIData_t *)data);
//    if (unwrapobj == NULL) {
//        return NULL;
//    }
//    PyObject *unwrapped = PyObject_CallOneArg(unwrapobj, wrapped);
//    Py_DECREF(unwrapobj);
//    return unwrapped;
//}
//
//static void
//_unwrap_default_free(void *data)
//{
//   (void)_PyXIData_ReleaseAndRawFree((_PyXIData_t *)data);
//}
//
//int
//_PyXIData_UnwrapperFromObj(PyThreadState *tstate, PyObject *unwrapobj,
//                           _PyXIData_unwrapper_t *unwrapper)
//{
//    if (!PyCallable_Check(unwrapobj)) {
//        PyErr_Format(PyExc_TypeError,
//                     "expected unwrap obj to be callable, got %R");
//        return -1;
//    }
//    // For now we pickle.
//    _PyXIData_t *pickled = PyMem_RawMalloc(sizeof(_PyXIData_t));
//    if (pickled == NULL) {
//        PyErr_NoMemory();
//        return -1;
//    }
//    if (_xidata_pickle(unwrapobj, pickled) < 0) {
//        PyMem_RawFree(pickled);
//        return -1;
//    }
//    *unwrapper = (_PyXIData_unwrapper_t){
//        .func = _unwrap_default_func,
//        .data = pickled,
//        .free = _unwrap_default_free,
//    };
//    return 0;
//}


/* wrapped data (wrapped orig) */

static void
wrapped_orig_init(_PyXIData_wrapped_orig_t *orig, _PyXIData_t *base)
{
    *orig = (_PyXIData_wrapped_orig_t){
        .data = base->data,
        .new_object = base->new_object,
        .free = base->free,
    };
}

static void
wrapped_orig_clear(_PyXIData_wrapped_orig_t *orig)
{
    if (orig->free != NULL && orig->data != NULL) {
        orig->free(orig->data);
    }
}

static PyObject *
wrapped_orig_apply(_PyXIData_wrapped_orig_t *orig, _PyXIData_t *base)
{
    _PyXIData_t data = *base;
    data.data = orig->data;
    data.new_object = orig->new_object;
    data.free = orig->free;
    return _PyXIData_NewObject(&data);
}


/* wrapped data */

struct _wrapped {
    _PyXIData_unwrapper_t unwrapper;
    _PyXIData_wrapped_orig_t orig;
};

static struct _wrapped *
new_wrapped(_PyXIData_t *data, _PyXIData_unwrapper_t *unwrapper)
{
    struct _wrapped *wrapped = PyMem_RawMalloc(sizeof(struct _wrapped));
    if (wrapped == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    *wrapped = (struct _wrapped){
        .unwrapper = *unwrapper,
    };
    wrapped_orig_init(&wrapped->orig, data);
    return wrapped;
}

static void
wrapped_free(struct _wrapped *wrapped)
{
    wrapped_orig_clear(&wrapped->orig);
    unwrapper_clear(&wrapped->unwrapper);
    PyMem_RawFree(wrapped);
}

static PyObject *
wrapped_apply(PyThreadState *tstate,
              struct _wrapped *wrapped, _PyXIData_t *data)
{
    PyObject *obj = wrapped_orig_apply(&wrapped->orig, data);
    if (obj == NULL) {
        return NULL;
    }
    PyObject *unwrapped = unwrapper_call(tstate, &wrapped->unwrapper, obj);
    Py_DECREF(obj);
    return unwrapped;
}

static PyObject *
_new_object_from_wrapped(_PyXIData_t *data)
{
    PyThreadState *tstate = _PyThreadState_GET();
    return wrapped_apply(tstate, (struct _wrapped *)data->data, data);
}

// This takes "ownership" of the given unwrapper data.
int
_PyXIData_SetUnwrapper(PyThreadState *tstate, _PyXIData_t *data,
                       _PyXIData_unwrapper_t *unwrapper)
{
    assert(data != NULL);
    struct _wrapped *wrapped = new_wrapped(data, unwrapper);
    if (wrapped == NULL) {
        return -1;
    }
    data->data = wrapped;
    data->new_object = _new_object_from_wrapped;
    data->free = (xid_freefunc)wrapped_free;
    return 0;
}

_PyXIData_unwrapper_t *
_PyXIData_GetUnwrapper(_PyXIData_t *data, _PyXIData_wrapped_orig_t *orig)
{
    if (data->new_object != _new_object_from_wrapped) {
        return NULL;
    }
    struct _wrapped *wrapped = (struct _wrapped *)data;
    if (orig != NULL) {
        *orig = wrapped->orig;
    }
    return &wrapped->unwrapper;
}

PyObject *
_PyXIData_NewObjectNotUnwrapped(_PyXIData_t *data)
{
    if (data->new_object == _new_object_from_wrapped) {
        PyThreadState *tstate = _PyThreadState_GET();
        return wrapped_apply(tstate, (struct _wrapped *)data->data, data);
    }
    return _PyXIData_NewObject(data);
}


/* xidata special methods */

static int
unpack_wrap_result(PyThreadState *tstate, PyObject *resobj,
                   PyObject **res_wrapped, PyObject **res_unwrap)
{
    assert(resobj != NULL);
    if (resobj == Py_None) {
        *res_wrapped = NULL;
        *res_unwrap = NULL;
        return 0;
    }

    PyObject *wrapped = NULL;
    PyObject *unwrap = NULL;
    if (PyTuple_Check(resobj)) {
        if (PyTuple_GET_SIZE(resobj) != 2) {
            goto error;
        }
        wrapped = Py_NewRef(PyTuple_GET_ITEM(resobj, 0));
        unwrap = Py_NewRef(PyTuple_GET_ITEM(resobj, 1));
    }
    else {
         if (PySequence_Length(resobj) != 2) {
             goto error;
         }
         wrapped = PySequence_GetItem(resobj, 0);
         if (wrapped == NULL) {
             goto error;
         }
         unwrap = PySequence_GetItem(resobj, 1);
         if (unwrap == NULL) {
             goto error;
         }
    }

    // If the user doesn't want to wrap the object and only wants
    // to unwrap then they will return the object as-is.
    if (_PyObject_CheckXIData(tstate, wrapped) != 0) {
        assert(PyErr_Occurred());
        goto error;
    }

    if (unwrap == Py_None) {
        Py_CLEAR(unwrap);
    }
    else {
        if (verify_xidata_unwrap_object(unwrap) < 0) {
            goto error;
        }
    }

    *res_wrapped = wrapped;
    *res_unwrap = unwrap;
    return 0;

error:
    Py_XDECREF(wrapped);
    Py_XDECREF(unwrap);
    return -1;
}

static PyObject *
try___xidata__(PyThreadState *tstate, PyObject *obj)
{
    PyObject *meth = _PyObject_LookupSpecial(obj, &_Py_ID(__xidata__));
    if (meth == NULL) {
        if (!PyErr_Occurred()) {
            Py_RETURN_NONE;
        }
        return NULL;
    }
    PyObject *wrapper = PyObject_CallOneArg(meth, obj);
    Py_DECREF(meth);
    if (wrapper == NULL) {
        return NULL;
    }
    if (!_PyXIDataWrapper_Check(wrapper)) {
        if (wrapper != Py_None) {
            PyErr_Format(PyExc_TypeError,
                         "expected __xidata__() to return " XIDWRAPPER_CLASSNAME
                         ", got %R", wrapper);
            Py_DECREF(wrapper);
            return NULL;
        }
    }
    return wrapper;
}

static int
try___xidata_wrap__(PyThreadState *tstate, PyObject *obj,
                    PyObject **res_wrapped, PyObject **res_unwrap)
{
    // Return None if the special method is not there.
    PyObject *meth = _PyObject_LookupSpecial(obj, &_Py_ID(__xidata_wrap__));
    if (meth == NULL) {
        if (!PyErr_Occurred()) {
            *res_wrapped = NULL;
            *res_unwrap = NULL;
            return 0;
        }
        return -1;
    }
    PyObject *wrapres = PyObject_CallOneArg(meth, obj);
    Py_DECREF(meth);
    if (wrapres == NULL) {
        return -1;
    }
    if (unpack_wrap_result(tstate, wrapres, res_wrapped, res_unwrap) < 0) {
        const char *msg = "expected __xidata_wrap__() to return a tuple of "
                          "(wrapped obj, unwrap func), got %R";
        _PyXIData_FormatNotShareableError(tstate, msg, wrapres);
        return -1;
    }
    return 0;
}


/* getting cross-interpreter data */

static inline void
_set_xid_lookup_failure(PyThreadState *tstate, PyObject *obj, const char *msg)
{
    if (msg != NULL) {
        assert(obj == NULL);
        set_notshareableerror(tstate, msg);
    }
    else if (obj == NULL) {
        set_notshareableerror(
                tstate, "object does not support cross-interpreter data");
    }
    else {
        _PyXIData_FormatNotShareableError(
                tstate, "%S does not support cross-interpreter data", obj);
    }
}


int
_PyObject_CheckXIData(PyThreadState *tstate, PyObject *obj)
{
    dlcontext_t ctx;
    if (get_lookup_context(tstate, &ctx) < 0) {
        return -1;
    }
    xidatafunc getdata = lookup_getdata(&ctx, obj);
    if (getdata == NULL) {
        if (!PyErr_Occurred()) {
            _set_xid_lookup_failure(tstate, obj, NULL);
        }
        return -1;
    }
    return 0;
}

static int
_check_xidata(PyThreadState *tstate, _PyXIData_t *xidata)
{
    // xidata->data can be anything, including NULL, so we don't check it.

    // xidata->obj may be NULL, so we don't check it.

    if (_PyXIData_INTERPID(xidata) < 0) {
        PyErr_SetString(PyExc_SystemError, "missing interp");
        return -1;
    }

    if (xidata->new_object == NULL) {
        PyErr_SetString(PyExc_SystemError, "missing new_object func");
        return -1;
    }

    // xidata->free may be NULL, so we don't check it.

    return 0;
}

int
_PyObject_GetXIData(PyThreadState *tstate,
                    PyObject *obj, _PyXIData_t *xidata)
{
    PyInterpreterState *interp = tstate->interp;

    assert(xidata->data == NULL);
    assert(xidata->obj == NULL);
    if (xidata->data != NULL || xidata->obj != NULL) {
        _PyErr_SetString(tstate, PyExc_ValueError, "xidata not cleared");
    }

    // Call the "getdata" func for the object.
    dlcontext_t ctx;
    if (get_lookup_context(tstate, &ctx) < 0) {
        return -1;
    }
    Py_INCREF(obj);
    xidatafunc getdata = lookup_getdata(&ctx, obj);
    if (getdata == NULL) {
        if (PyErr_Occurred()) {
            Py_DECREF(obj);
            return -1;
        }
        // Fall back to obj
        Py_DECREF(obj);
        if (!PyErr_Occurred()) {
            _set_xid_lookup_failure(tstate, obj, NULL);
        }
        return -1;
    }
    int res = getdata(tstate, obj, xidata);
    Py_DECREF(obj);
    if (res != 0) {
        return -1;
    }

    // Fill in the blanks and validate the result.
    _PyXIData_INTERPID(xidata) = PyInterpreterState_GetID(interp);
    if (_check_xidata(tstate, xidata) != 0) {
        (void)_PyXIData_Release(xidata);
        return -1;
    }

    return 0;
}

int
_PyObject_GetXIDataWrapped(PyThreadState *tstate,
                           PyObject *obj, xid_wrapfunc wrap,
                           _PyXIData_t *data)
{
    PyObject *wrapped = NULL;
    _PyXIData_unwrapper_t _unwrapper = {0};
    _PyXIData_unwrapper_t *unwrapper = &_unwrapper;
    if (wrap != NULL) {
        if (wrap(tstate, obj, &wrapped, unwrapper) < 0) {
            // We leave the exception in place while we try the fallback,
            // so any exception will have __context__ set.
        }
    }

    if (wrapped == NULL) {
        // Fall back to __xidata__().
        PyObject *wrapper = try___xidata__(tstate, obj);
        if (wrapper == NULL) {
            goto error;
        }
        if (wrapper == Py_None) {
            // Try the next option.
            Py_DECREF(wrapper);
        }
        else {
            // XXX apply directly
            return 0;
        }

        // Fall back to __xidata_wrap__().
        PyObject *unwrap = NULL;
        if (try___xidata_wrap__(tstate, obj, &wrapped, &unwrap) < 0) {
            goto error;
        }
        if (wrapped == NULL) {
            wrapped = Py_NewRef(obj);
        }
        if (unwrap == NULL) {
            unwrapper = NULL;
        }
        else {
            int res = xidata_unwrapper_from_object(tstate, unwrap, unwrapper);
            Py_DECREF(unwrap);
            if (res < 0) {
                goto error;
            }
        }
    }

    // The fallback is done so raise the exception from wrap(), if any.
    if (PyErr_Occurred()) {
        goto error;
    }

    // Apply the wrap() result.
    int res = _PyObject_GetXIData(tstate, wrapped, data);
    Py_DECREF(wrapped);
    if (res < 0) {
        goto error;
    }
    if (unwrapper != NULL) {
        if (_PyXIData_SetUnwrapper(tstate, data, unwrapper) < 0) {
            _xidata_clear(data);
            goto error;
        }
    }
    return 0;

error:
    Py_XDECREF(wrapped);
    if (unwrapper != NULL) {
        unwrapper_clear(unwrapper);
    }
    set_notshareableerror(tstate, "error while wrapping object");
    return -1;
}

int
_PyObject_GetXIDataWrappedWithObj(PyThreadState *tstate,
                                  PyObject *obj, PyObject *wrapobj,
                                  _PyXIData_t *data)
{
    // We do not fall back to special methods.  That is done
    // only in _PyObject_GetXIDataWrapped().
    if (wrapobj == NULL || wrapobj == Py_None) {
        _PyErr_SetString(tstate, PyExc_ValueError, "missing wrap function");
        return -1;
    }
    // Call the wrapper and handle the result.  It must return either
    // a tuple of (C-shareable obj, unwrap func) or an instance of
    // _PyXIDataWrapper_Type.
    PyObject *resobj = PyObject_CallOneArg(wrapobj, obj);
    if (resobj == NULL) {
        goto error;
    }

    // Handle the wrapper case.
    if (_PyXIDataWrapper_Check(resobj)) {
        // XXX apply directly
        return 0;
    }

    // Handle the normal case.
    PyObject *wrapped = NULL;
    PyObject *unwrap = NULL;
    if (unpack_wrap_result(tstate, resobj, &wrapped, &unwrap) < 0) {
        PyErr_Format(PyExc_ValueError,
                     "wrap func returned unexpected value %R", resobj);
        Py_DECREF(resobj);
        goto error;
    }
    if (wrapped == NULL) {
        wrapped = Py_NewRef(obj);
    }
    _PyXIData_unwrapper_t _unwrapper = {0};
    _PyXIData_unwrapper_t *unwrapper = &_unwrapper;
    if (unwrap == NULL) {
        unwrapper = NULL;
    }
    else {
        int res = xidata_unwrapper_from_object(tstate, unwrap, unwrapper);
        Py_DECREF(unwrap);
        if (res < 0) {
            goto error;
        }
    }

    // Apply the wrap() result.
    int res = _PyObject_GetXIData(tstate, wrapped, data);
    Py_DECREF(wrapped);
    if (res < 0) {
        if (unwrapper != NULL) {
            unwrapper_clear(unwrapper);
        }
        goto error;
    }
    if (unwrapper != NULL) {
        // If this succeeds then the xidata takes ownership
        // of the unerapper data, so we don't clear it.
        if (_PyXIData_SetUnwrapper(tstate, data, unwrapper) < 0) {
            unwrapper_clear(unwrapper);
            _xidata_clear(data);
            goto error;
        }
    }
    return 0;

error:
    set_notshareableerror(tstate, "error while wrapping object");
    return -1;
}


/* pickle wrapper */

//struct _pickle_wrapper_data {
//    const char *mainfile;
//    size_t len;
//    char _mainfile[MAXPATHLEN+1];
//};
//
//static struct _pickle_wrapper_data *
//_new_pickle_wrapper_data(void)
//{
//    struct _pickle_wrapper_data *pickled =
//                PyMem_RawMalloc(sizeof(struct _pickle_wrapper_data));
//    if (pickled == NULL) {
//        return NULL;
//    }
//    *pickled = (struct _pickle_wrapper_data){{0}};
//    // Set mainfile if possible.
//    Py_ssize_t len = _Py_GetMainfile(pickled->_mainfile, MAXPATHLEN);
//    if (len < 0) {
//        // For now we ignore any exceptions.
//        PyErr_Clear();
//    }
//    else if (len > 0) {
//        pickled->mainfile = pickled->_mainfile;
//        pickled->len = (size_t)len;
//    }
//    return pickled;
//}
//
//static void
//_pickle_wrapper_data_free(struct _pickle_wrapper_data *data)
//{
//    PyMem_RawFree(data);
//}
//
//static PyObject *
//_xidata_unpickle(_PyXIData_t *data)
//{
////    if (== 0) {
////        // It is missing.
////    }
//    // XXX
//    PyErr_SetString(PyExc_NotImplementedError, "TBD");
//    return NULL;
//}


struct _pickle_context {
    // __main__.__file__
    struct {
        const char *utf8;
        size_t len;
        char _utf8[MAXPATHLEN+1];
    } mainfile;
};

static int
_set_pickle_context(PyThreadState *tstate, struct _pickle_context *ctx)
{
    // Set mainfile if possible.
    Py_ssize_t len = _Py_GetMainfile(ctx->mainfile._utf8, MAXPATHLEN);
    if (len < 0) {
        // For now we ignore any exceptions.
        PyErr_Clear();
    }
    else if (len > 0) {
        ctx->mainfile.utf8 = ctx->mainfile._utf8;
        ctx->mainfile.len = (size_t)len;
    }

    return 0;
}


struct _shared_pickle_data {
    _PyBytes_data_t pickled;  // Must be first if we use _PyBytes_FromXIData().
    struct _pickle_context ctx;
};

PyObject *
_PyPickle_LoadFromXIData(_PyXIData_t *xidata)
{
    struct _shared_pickle_data *shared =
                            (struct _shared_pickle_data *)xidata->data;
    // We avoid copying the pickled data by wrapping it in a memoryview.
    // The alternative is to get a bytes object using _PyBytes_FromXIData().
    PyObject *pickled = PyMemoryView_FromMemory(
                    shared->pickled.bytes, shared->pickled.len, PyBUF_READ);
    if (pickled == NULL) {
        return NULL;
    }

    // Unpickle the object.
    PyObject *loads = PyImport_ImportModuleAttrString("pickle", "loads");
    if (loads == NULL) {
        Py_DECREF(pickled);
        return NULL;
    }
    PyObject *obj = PyObject_CallOneArg(loads, pickled);
    // XXX Handle failure due to __main__.
    Py_DECREF(loads);
    Py_DECREF(pickled);
    return obj;
}

int
_PyPickle_GetXIData(PyThreadState *tstate, PyObject *obj, _PyXIData_t *xidata)
{
    // Pickle the object.
    PyObject *dumps = PyImport_ImportModuleAttrString("pickle", "dumps");
    if (dumps == NULL) {
        return -1;
    }
    PyObject *bytes = PyObject_CallOneArg(dumps, obj);
    Py_DECREF(dumps);
    if (bytes == NULL) {
        // XXX This chains, right?
        _set_xid_lookup_failure(tstate, NULL, "object could not be pickled");
        return -1;
    }

    // If we had an "unwrapper" mechnanism, we could call
    // _PyObject_GetXIData() on the bytes object directly and add
    // a simple unwrapper to call pickle.loads() on the bytes.
    size_t size = sizeof(struct _shared_pickle_data);
    struct _shared_pickle_data *shared =
            (struct _shared_pickle_data *)_PyBytes_GetXIDataWrapped(
                    tstate, bytes, size, _PyPickle_LoadFromXIData, xidata);
    Py_DECREF(bytes);
    if (shared == NULL) {
        return -1;
    }

    if (_set_pickle_context(tstate, &shared->ctx) < 0) {
        _xidata_clear(xidata);
        return -1;
    }

    return 0;
}


/* marshal wrapper */

PyObject *
_PyMarshal_ReadObjectFromXIData(_PyXIData_t *xidata)
{
    _PyBytes_data_t *shared = (_PyBytes_data_t *)xidata->data;
    return PyMarshal_ReadObjectFromString(shared->bytes, shared->len);
}

int
_PyMarshal_GetXIData(PyThreadState *tstate, PyObject *obj, _PyXIData_t *xidata)
{
    PyObject *bytes = PyMarshal_WriteObjectToString(obj, Py_MARSHAL_VERSION);
    if (bytes == NULL) {
        return -1;
    }
    _PyBytes_data_t *shared = _PyBytes_GetXIDataWrapped(
            tstate, bytes, 0, _PyMarshal_ReadObjectFromXIData, xidata);
    Py_DECREF(bytes);
    if (shared == NULL) {
        return -1;
    }
    return 0;
}


/* function wrapper */

static int
verify_stateless_codeobj(PyThreadState *tstate, PyCodeObject *co,
                         int *p_okay, PyObject *globalnames)
{
    if (co->co_flags & CO_GENERATOR) {
        _PyErr_SetString(tstate, PyExc_TypeError, "generators not supported");
        goto failed;
    }
    if (co->co_flags & CO_COROUTINE) {
        _PyErr_SetString(tstate, PyExc_TypeError, "coroutines not supported");
        goto failed;
    }
    if (co->co_flags & CO_ITERABLE_COROUTINE) {
        _PyErr_SetString(tstate, PyExc_TypeError, "coroutines not supported");
        goto failed;
    }
    if (co->co_flags & CO_ASYNC_GENERATOR) {
        _PyErr_SetString(tstate, PyExc_TypeError, "generators not supported");
        goto failed;
    }

   _PyCode_var_counts_t counts = {0};
    _PyCode_GetVarCounts(co, &counts);
    if (_PyCode_SetUnboundVarCounts(co, &counts, globalnames, NULL) < 0) {
        return -1;
    }

    // CO_NESTED is okay as long as there's no closure.
    if (counts.locals.cells.total > 0) {
        _PyErr_SetString(tstate, PyExc_ValueError, "nesting not supported");
        goto failed;
    }
    if (counts.numfree > 0) {  // There's a closure.
        _PyErr_SetString(tstate, PyExc_ValueError, "closures not supported");
        goto failed;
    }
    assert(counts.locals.hidden.total == 0);

    // We don't check counts.unbound.numglobal since we can't
    // distinguish beween globals and builtins here.

    if (p_okay != NULL) {
        *p_okay = 1;
    }
    return 0;

failed:
    if (p_okay != NULL) {
        *p_okay = 0;
    }
    return -1;
}

static int
verify_stateless_funcobj(PyThreadState *tstate, PyObject *func, int *p_okay)
{
    assert(!PyErr_Occurred());
    assert(PyFunction_Check(func));

    PyObject *globalnames = PySet_New(NULL);
    if (globalnames == NULL) {
        return -1;
    }

    // Disallow __defaults__.
    PyObject *defaults = PyFunction_GET_DEFAULTS(func);
    if (defaults != NULL && defaults != Py_None && PyDict_Size(defaults) > 0)
    {
        _PyErr_SetString(tstate, PyExc_ValueError, "defaults not supported");
        goto failed;
    }
    // Disallow __kwdefaults__.
    PyObject *kwdefaults = PyFunction_GET_KW_DEFAULTS(func);
    if (kwdefaults != NULL && kwdefaults != Py_None
            && PyDict_Size(kwdefaults) > 0)
    {
        _PyErr_SetString(tstate, PyExc_ValueError,
                         "keyword defaults not supported");
        goto failed;
    }
    // Disallow __closure__.
    PyObject *closure = PyFunction_GET_CLOSURE(func);
    if (closure != NULL && closure != Py_None && PyTuple_GET_SIZE(closure) > 0)
    {
        _PyErr_SetString(tstate, PyExc_ValueError, "closures not supported");
        goto failed;
    }

    // Check the code.
    PyCodeObject *co = (PyCodeObject *)PyFunction_GET_CODE(func);
    if (verify_stateless_codeobj(tstate, co, p_okay, globalnames) < 0) {
        goto error;
    }

    // Disallow globals.
    PyObject *globals = PyFunction_GET_GLOBALS(func);
    if (globals == NULL) {
        _PyErr_SetString(tstate, PyExc_ValueError, "missing globals");
        goto failed;
    }
    if (!PyDict_Check(globals)) {
        _PyErr_Format(tstate, PyExc_TypeError,
                      "unsupported globals %R", globals);
        goto failed;
    }
    // This is inspired by inspect.getclosurevars().
    Py_ssize_t pos = 0;
    PyObject *name;
    Py_hash_t hash;
    while(_PySet_NextEntry(globalnames, &pos, &name, &hash)) {
        if (PyDict_Contains(globals, name)) {
            if (PyErr_Occurred()) {
                goto error;
            }
            _PyErr_SetString(tstate, PyExc_ValueError,
                             "globals not supported");
            goto failed;
        }
    }

    Py_DECREF(globalnames);
    if (p_okay != NULL) {
        *p_okay = 1;
    }
    return 0;

failed:
    if (p_okay != NULL) {
        *p_okay = 0;
    }

error:
    Py_DECREF(globalnames);
    return -1;
}


PyObject *
_PyFunction_FromXIData(_PyXIData_t *xidata)
{
    // For now "stateless" functions are the only ones we must accommodate.

    PyObject *code = _PyMarshal_ReadObjectFromXIData(xidata);
    if (code == NULL) {
        return NULL;
    }
    // Create a new function.
    assert(PyCode_Check(code));
    PyObject *globals = PyDict_New();
    if (globals == NULL) {
        Py_DECREF(code);
        return NULL;
    }
    PyObject *func = PyFunction_New(code, globals);
    Py_DECREF(code);
    Py_DECREF(globals);
    return func;
}

int
_PyFunction_GetXIDataStateless(PyThreadState *tstate, PyObject *func,
                               _PyXIData_t *xidata)
{
    if (!PyFunction_Check(func)) {
        PyErr_Format(PyExc_TypeError, "expected function, got %R", func);
        return -1;
    }
    if (verify_stateless_funcobj(tstate, func, NULL) < 0) {
        return -1;
    }
    PyObject *code = PyFunction_GET_CODE(func);

    // Ideally code objects would be immortal and directly shareable.
    // In the meantime, we use marshal.
    if (_PyMarshal_GetXIData(tstate, code, xidata) < 0) {
        return -1;
    }
    // Replace _PyMarshal_ReadObjectFromXIData.
    // (_PyFunction_FromXIData() will call it.)
    _PyXIData_SET_NEW_OBJECT(xidata, _PyFunction_FromXIData);
    return 0;
}


/* using cross-interpreter data */

PyObject *
_PyXIData_NewObject(_PyXIData_t *xidata)
{
    return xidata->new_object(xidata);
}

static int
_call_clear_xidata(void *data)
{
    _xidata_clear((_PyXIData_t *)data);
    return 0;
}

static int
_xidata_release(_PyXIData_t *xidata, int rawfree)
{
    if ((xidata->data == NULL || xidata->free == NULL) && xidata->obj == NULL) {
        // Nothing to release!
        if (rawfree) {
            PyMem_RawFree(xidata);
        }
        else {
            xidata->data = NULL;
        }
        return 0;
    }

    // Switch to the original interpreter.
    PyInterpreterState *interp = _PyInterpreterState_LookUpID(
                                        _PyXIData_INTERPID(xidata));
    if (interp == NULL) {
        // The interpreter was already destroyed.
        // This function shouldn't have been called.
        // XXX Someone leaked some memory...
        assert(PyErr_Occurred());
        if (rawfree) {
            PyMem_RawFree(xidata);
        }
        return -1;
    }

    // "Release" the data and/or the object.
    if (rawfree) {
        return _Py_CallInInterpreterAndRawFree(interp, _call_clear_xidata, xidata);
    }
    else {
        return _Py_CallInInterpreter(interp, _call_clear_xidata, xidata);
    }
}

int
_PyXIData_Release(_PyXIData_t *xidata)
{
    return _xidata_release(xidata, 0);
}

int
_PyXIData_ReleaseAndRawFree(_PyXIData_t *xidata)
{
    return _xidata_release(xidata, 1);
}


/* XID wrapper objects */

typedef struct {
    PyObject base;
    PyObject *orig;
    int fallback;
    PyObject *wrapobj;
    xid_wrapfunc wrapfunc;
    _PyXIData_t *data;
    _PyXIData_t *used;
    // pre-allocated
    _PyXIData_t _data;
} xidwrapper;

static int _xidwrapper_bind(PyThreadState *, xidwrapper *);

static xidwrapper *
_new_xidwrapper(PyThreadState *tstate, PyTypeObject *wrappertype,
                _PyXIData_t *data, PyObject *obj, int fallback,
                xid_wrapfunc wrapfunc, PyObject *wrapobj)
{
    xidwrapper *wrapper = PyObject_GC_New(xidwrapper, wrappertype);
    if (wrapper == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    if (obj != NULL) {
        assert(data == NULL);
        assert(wrapfunc == NULL || wrapobj == NULL);
        assert(!fallback || (wrapfunc == NULL && wrapobj == NULL));
        assert(wrapobj == NULL || PyCallable_Check(wrapobj));
        *wrapper = (xidwrapper){
            .base = wrapper->base,
            .orig = Py_NewRef(obj),
            .fallback = fallback,
            .wrapobj = wrapobj,
            .wrapfunc = wrapfunc,
        };
        if (_xidwrapper_bind(tstate, wrapper) < 0) {
            Py_DECREF(obj);
            PyObject_GC_UnTrack(wrapper);
            PyObject_GC_Del((PyObject *)wrapper);
            return NULL;
        }
        assert(wrapper->data != NULL);
    }
    else {
        assert(data != NULL);
        assert(!fallback);
        assert(wrapfunc == NULL);
        assert(wrapobj == NULL);
        if (_PyXIData_INTERPID(data) !=
                            PyInterpreterState_GetID(tstate->interp))
        {
            PyErr_SetString(PyExc_ValueError, "interpreter mismatch");
            PyObject_GC_UnTrack(wrapper);
            PyObject_GC_Del((PyObject *)wrapper);
            return NULL;
        }
        *wrapper = (xidwrapper){
            .base = wrapper->base,
            .data = data,
        };
    }
    return wrapper;
}

static void
_xidwrapper_free(xidwrapper *wrapper)
{
    PyObject_GC_UnTrack(wrapper);
    // Clear the wrapped XI data.
    _PyXIData_t *data = wrapper->data;
    if (data == NULL) {
        data = wrapper->used;
    }
    if (data != NULL) {
        // "Release" the data and/or the object directly,
        // rather than using _PyXIData_Release().
        assert(_PyXIData_INTERPID(data) ==
                PyInterpreterState_GetID(_PyInterpreterState_GET()));
        _xidata_clear(data);
    }
    // Clear the objects.
    Py_CLEAR(wrapper->wrapobj);
    Py_CLEAR(wrapper->orig);
    // Free it.
    PyObject_GC_Del((PyObject *)wrapper);
}

static int
_xidwrapper_handle_used(PyThreadState *tstate, xidwrapper *wrapper)
{
    if (wrapper->orig != NULL) {
        return 0;
    }
    // For now, we don't allow re-using the XI data, and we can't
    // rebuild it without the original object, so we bail out here.
    // Note that we don't fall back to wrapper->data->obj.
    // wrapper->orig is the only signal we recognize.
    assert(wrapper->data != &wrapper->_data);
    if (wrapper->used == NULL) {
        wrapper->used = wrapper->data;
        return 0;
    }
    set_notshareableerror(tstate, "wrapped data already used");
    return -1;
}

/* Populate the _PyXIData_t with data from the wrapper. */
static int
_xidwrapper_apply(PyThreadState *tstate, xidwrapper *wrapper,
                  _PyXIData_t *data)
{
    PyObject *obj = wrapper->orig;
    if (obj == NULL) {
        PyErr_SetString(PyExc_ValueError, "no orig object");
        return -1;
    }
    if (wrapper->wrapfunc != NULL || wrapper->fallback) {
        return _PyObject_GetXIDataWrapped(
                                tstate, obj, wrapper->wrapfunc, data);
    }
    else if (wrapper->wrapobj != NULL) {
        return _PyObject_GetXIDataWrappedWithObj(
                                tstate, obj, wrapper->wrapobj, data);
    }
    else {
        return _PyObject_GetXIData(tstate, obj, data);
    }
}

/* Set the wrapped XI data based on the original object and unwrapper. */
static int
_xidwrapper_bind(PyThreadState *tstate, xidwrapper *wrapper)
{
    assert(wrapper->orig != NULL);
    assert(wrapper->data == NULL);
    if (_xidwrapper_apply(tstate, wrapper, &wrapper->_data) < 0) {
        return -1;
    }
    wrapper->data = &wrapper->_data;
    return 0;
}

/* "Return" the wrapped XI data, applying first if necessary. */
static int
_xidwrapper_get_xidata(PyThreadState *tstate, xidwrapper *wrapper,
                       _PyXIData_t *dest)
{
    assert(wrapper->data != NULL || wrapper->orig != NULL);
    if (wrapper->data != NULL) {
        *dest = *wrapper->data;
        if (wrapper->orig == NULL) {
            assert(wrapper->data != &wrapper->_data);
            assert(wrapper->used == wrapper->data);
            wrapper->data = NULL;
        }
        else {
            assert(wrapper->data == &wrapper->_data);
            assert(wrapper->used == NULL);
            wrapper->data = NULL;
            wrapper->_data = (_PyXIData_t){0};
        }
    }
    else if (_xidwrapper_apply(tstate, wrapper, dest) < 0) {
        return -1;
    }
    return 0;
}

// _PyXIDataWrapper_Type "tp slots"

PyDoc_STRVAR(xidwrapper___doc__,
"Cross-interpreter data that may be safely shared between interpreters.\n\
It may be an object itself (e.g. None), an object's underlying data\n\
(e.g. bytearray), or an efficient copy of an object's data (e.g. tuple).\n\
\n\
Normally, cross-interpreter data is hidden away behind other objects\n\
or APIs.  However, sometimes it can be useful to get an object's\n\
cross-interpreter data ahead of time.  CrossIntpreterDataWrapper\n\
objects are the containers in which that data is stored and exposed\n\
to Python code.\n\
");

static PyObject *
xidwrapper_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyThreadState *tstate = _PyThreadState_GET();
    static char *kwlist[] = {"obj", "wrap", NULL};
    PyObject *obj;
    PyObject *wrapobj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "O|O:__new__", kwlist,
                                     &obj, &wrapobj)) {
        return NULL;
    }
    if (wrapobj == Py_None || wrapobj == Py_True) {
        wrapobj = NULL;
    }

    int fallback = 0;
    if (wrapobj == NULL) {
        // By default, we try the special methods.
        // XXX Otherwise fall back to marshal, pickle?
        fallback = 1;
    }
    else if (wrapobj == Py_False) {
        wrapobj = NULL;
    }

    xidwrapper *wrapper = _new_xidwrapper(
                    tstate, type, NULL, obj, fallback, NULL, wrapobj);
    return (PyObject *)wrapper;
}

static void
xidwrapper_dealloc(xidwrapper *wrapper)
{
    _xidwrapper_free(wrapper);
}

static int
xidwrapper_traverse(PyObject *self, visitproc visit, void *arg)
{
    xidwrapper *wrapper = (xidwrapper *)self;
    Py_VISIT(wrapper->wrapobj);
    Py_VISIT(wrapper->orig);
    return 0;
}

static int
xidwrapper_clear(PyObject *self)
{
    xidwrapper *wrapper = (xidwrapper *)self;
    Py_CLEAR(wrapper->wrapobj);
    Py_CLEAR(wrapper->orig);
    return 0;
}

static PyObject *
xidwrapper_repr(PyObject *self)
{
    const char *typename = Py_TYPE(self)->tp_name;
    xidwrapper *wrapper = (xidwrapper *)self;
    PyObject *obj = wrapper->orig;
    if (obj != NULL) {
        PyObject *res = PyUnicode_FromFormat("<%s object (wrapping %R) at %p>",
                                             typename, obj, self);
        if (res != NULL) {
            return res;
        }
    }
    return PyUnicode_FromFormat("<%s object at %p>", typename, self);
}

/* The impl of the XI data protocol for _PyXIDataWrapper_Type. */
static int
xidwrapper_shared(PyThreadState *tstate, PyObject *wrapperobj,
                  _PyXIData_t *data)
{
    assert(Py_IS_TYPE(wrapperobj, &_PyXIDataWrapper_Type));
    xidwrapper *wrapper = (xidwrapper *)wrapperobj;
    if (_xidwrapper_handle_used(tstate, wrapper) < 0) {
        return -1;
    }
    if (_xidwrapper_get_xidata(tstate, wrapper, data) < 0) {
        return -1;
    }
    return 0;
}

// _PyXIDataWrapper_Type methods

static PyObject *
xidwrapper___xidata__(PyObject *self)
{
    return Py_NewRef(self);
}

static PyObject *
xidwrapper_as_object(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"unwrap", NULL};
    int unwrap = 1;
    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "|p:as_object", kwlist,
                                     &unwrap)) {
        return NULL;
    }

    PyThreadState *tstate = _PyThreadState_GET();
    xidwrapper *wrapper = (xidwrapper *)self;

    // See the note in _xidwrapper_handle_used().
    if (wrapper->orig == NULL
            && (wrapper->data != NULL || wrapper->used != NULL))
    {
        PyErr_SetString(PyExc_TypeError, "not allowed");
        return NULL;
    }

    if (wrapper->data == NULL) {
        if (wrapper->orig == NULL) {
            PyErr_SetString(PyExc_ValueError, "no orig object");
            return NULL;
        }
        if (_xidwrapper_bind(tstate, wrapper) < 0) {
            return NULL;
        }
    }
    return unwrap
        ? _PyXIData_NewObject(wrapper->data)
        : _PyXIData_NewObjectNotUnwrapped(wrapper->data);
}

PyDoc_STRVAR(xidwrapper_as_object___doc__,
"as_object($self, *, unwrap=True) -> object\n\
\n\
Return an object corresponding to the original shareable object.\n\
");

static PyMethodDef xidwrapper_methods[] = {
    {"__xidata__",                _PyCFunction_CAST(xidwrapper___xidata__),
     METH_NOARGS, NULL},
    {"as_object",                 _PyCFunction_CAST(xidwrapper_as_object),
     METH_VARARGS | METH_KEYWORDS, xidwrapper_as_object___doc__},
    {NULL,              NULL}           /* sentinel */
};

static PyObject *
xidwrapper_get_orig(PyObject *self, void *Py_UNUSED(closure))
{
    xidwrapper *wrapper = (xidwrapper *)self;
    if (wrapper->orig == NULL) {
        PyErr_SetString(PyExc_ValueError, "original object not available");
        return NULL;
    }
    return Py_NewRef(wrapper->orig);
}

static PyObject *
xidwrapper_get_iswrapped(PyObject *self, void *Py_UNUSED(closure))
{
    int iswrapped;
    xidwrapper *wrapper = (xidwrapper *)self;
    if (wrapper->data != NULL) {
        iswrapped = (_PyXIData_GetUnwrapper(wrapper->data, NULL) != NULL);
    }
    else if (wrapper->used != NULL) {
        iswrapped = (_PyXIData_GetUnwrapper(wrapper->used, NULL) != NULL);
    }
    else {
        PyThreadState *tstate = PyThreadState_Get();
        if (_xidwrapper_bind(tstate, wrapper) < 0) {
            return NULL;
        }
        assert(wrapper->data != NULL);
        iswrapped = (_PyXIData_GetUnwrapper(wrapper->data, NULL) != NULL);
    }
    if (iswrapped) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *
xidwrapper_get_reusable(PyObject *self, void *Py_UNUSED(closure))
{
    xidwrapper *wrapper = (xidwrapper *)self;
    if (wrapper->orig != NULL) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyGetSetDef xidwrapper_getsets[] = {
    {"orig",         xidwrapper_get_orig, NULL, NULL},
    {"iswrapped",    xidwrapper_get_iswrapped, NULL, NULL},
    {"reusable",     xidwrapper_get_reusable, NULL, NULL},
    {0}
};

PyTypeObject _PyXIDataWrapper_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = XIDWRAPPER_CLASSNAME,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_basicsize = sizeof(xidwrapper),
    .tp_dealloc = (destructor)xidwrapper_dealloc,
    .tp_repr = xidwrapper_repr,
    .tp_doc = xidwrapper___doc__,
    .tp_traverse = xidwrapper_traverse,
    .tp_clear = xidwrapper_clear,
    .tp_methods = xidwrapper_methods,
    .tp_getset = xidwrapper_getsets,
    .tp_new = xidwrapper_new,
    //.tp_xidata = xidwrapper_share,
};


static int
_xidwrapper_init_type(PyInterpreterState *interp)
{
    if (_PyStaticType_InitBuiltin(interp, &_PyXIDataWrapper_Type) < 0) {
        return -1;
    }
    return 0;
}

static void
_xidwrapper_fini_type(PyInterpreterState *interp)
{
    _PyStaticType_FiniBuiltin(interp, &_PyXIDataWrapper_Type);
}

static int
_xidwrapper_register_type(PyInterpreterState *interp)
{
    /* Register_PyXIDataWrapper_Type as shareable. */
    // This is necessary only as long as we don't have a tp_ slot for it.
    PyThreadState *tstate = _PyThreadState_GET();
    assert(tstate->interp == interp);
    if (_PyXIData_RegisterClass(
                tstate, &_PyXIDataWrapper_Type, xidwrapper_shared) < 0)
    {
        _PyStaticType_FiniBuiltin(interp, &_PyXIDataWrapper_Type);
        return -1;
    }
    return 0;
}


int
_PyXIDataWrapper_CheckExact(PyObject *obj)
{
    return Py_IS_TYPE(obj, &_PyXIDataWrapper_Type);
}

int
_PyXIDataWrapper_Check(PyObject *obj)
{
    return PyObject_IsInstance(obj, (PyObject *)&_PyXIDataWrapper_Type);
}


PyObject *
_PyXIDataWrapper_New(PyThreadState *tstate, PyObject *obj)
{
    if (obj == NULL) {
        PyErr_SetString(PyExc_ValueError, "missing obj");
        return NULL;
    }
    xidwrapper *wrapper = _new_xidwrapper(
                tstate, &_PyXIDataWrapper_Type, NULL, obj, 1, NULL, NULL);
    if (wrapper == NULL) {
        return NULL;
    }
    return (PyObject *)wrapper;
}

PyObject *
_PyXIDataWrapper_NewWithoutFallback(PyThreadState *tstate, PyObject *obj)
{
    if (obj == NULL) {
        PyErr_SetString(PyExc_ValueError, "missing obj");
        return NULL;
    }
    xidwrapper *wrapper = _new_xidwrapper(
                tstate, &_PyXIDataWrapper_Type, NULL, obj, 0, NULL, NULL);
    if (wrapper == NULL) {
        return NULL;
    }
    return (PyObject *)wrapper;
}

PyObject *
_PyXIDataWrapper_NewWithWrapFunc(PyThreadState *tstate,
                                 PyObject *obj, xid_wrapfunc wrap)
{
    if (obj == NULL) {
        PyErr_SetString(PyExc_ValueError, "missing obj");
        return NULL;
    }
    if (wrap == NULL) {
        PyErr_SetString(PyExc_ValueError, "missing wrap func");
        return NULL;
    }
    xidwrapper *wrapper = _new_xidwrapper(
                tstate, &_PyXIDataWrapper_Type, NULL, obj, 0, wrap, NULL);
    if (wrapper == NULL) {
        return NULL;
    }
    return (PyObject *)wrapper;
}

PyObject *
_PyXIDataWrapper_NewWithWrapObj(PyThreadState *tstate,
                                PyObject *obj, PyObject *wrap)
{
    if (obj == NULL) {
        PyErr_SetString(PyExc_ValueError, "missing obj");
        return NULL;
    }
    if (wrap == NULL) {
        PyErr_SetString(PyExc_ValueError, "missing wrap func");
        return NULL;
    }
    if (!PyCallable_Check(wrap)) {
        PyErr_SetString(PyExc_TypeError, "wrap arg is not callable");
        return NULL;
    }
    xidwrapper *wrapper = _new_xidwrapper(
                tstate, &_PyXIDataWrapper_Type, NULL, obj, 0, NULL, wrap);
    if (wrapper == NULL) {
        return NULL;
    }
    return (PyObject *)wrapper;
}

PyObject *
_PyXIDataWrapper_FromData(PyThreadState *tstate, _PyXIData_t *data)
{
    if (data == NULL) {
        PyErr_SetString(PyExc_ValueError, "missing data");
        return NULL;
    }
    xidwrapper *wrapper = _new_xidwrapper(
                tstate, &_PyXIDataWrapper_Type, data, NULL, 0, NULL, NULL);
    if (wrapper == NULL) {
        return NULL;
    }
    return (PyObject *)wrapper;
}


/*************************/
/* convenience utilities */
/*************************/

static const char *
_copy_string_obj_raw(PyObject *strobj, Py_ssize_t *p_size)
{
    Py_ssize_t size = -1;
    const char *str = PyUnicode_AsUTF8AndSize(strobj, &size);
    if (str == NULL) {
        return NULL;
    }

    if (size != (Py_ssize_t)strlen(str)) {
        PyErr_SetString(PyExc_ValueError, "found embedded NULL character");
        return NULL;
    }

    char *copied = PyMem_RawMalloc(size+1);
    if (copied == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    strcpy(copied, str);
    if (p_size != NULL) {
        *p_size = size;
    }
    return copied;
}


static int
_convert_exc_to_TracebackException(PyObject *exc, PyObject **p_tbexc)
{
    PyObject *args = NULL;
    PyObject *kwargs = NULL;
    PyObject *create = NULL;

    // This is inspired by _PyErr_Display().
    PyObject *tbexc_type = PyImport_ImportModuleAttrString(
        "traceback",
        "TracebackException");
    if (tbexc_type == NULL) {
        return -1;
    }
    create = PyObject_GetAttrString(tbexc_type, "from_exception");
    Py_DECREF(tbexc_type);
    if (create == NULL) {
        return -1;
    }

    args = PyTuple_Pack(1, exc);
    if (args == NULL) {
        goto error;
    }

    kwargs = PyDict_New();
    if (kwargs == NULL) {
        goto error;
    }
    if (PyDict_SetItemString(kwargs, "save_exc_type", Py_False) < 0) {
        goto error;
    }
    if (PyDict_SetItemString(kwargs, "lookup_lines", Py_False) < 0) {
        goto error;
    }

    PyObject *tbexc = PyObject_Call(create, args, kwargs);
    Py_DECREF(args);
    Py_DECREF(kwargs);
    Py_DECREF(create);
    if (tbexc == NULL) {
        goto error;
    }

    *p_tbexc = tbexc;
    return 0;

error:
    Py_XDECREF(args);
    Py_XDECREF(kwargs);
    Py_XDECREF(create);
    return -1;
}

// We accommodate backports here.
#ifndef _Py_EMPTY_STR
# define _Py_EMPTY_STR &_Py_STR(empty)
#endif

static const char *
_format_TracebackException(PyObject *tbexc)
{
    PyObject *lines = PyObject_CallMethod(tbexc, "format", NULL);
    if (lines == NULL) {
        return NULL;
    }
    assert(_Py_EMPTY_STR != NULL);
    PyObject *formatted_obj = PyUnicode_Join(_Py_EMPTY_STR, lines);
    Py_DECREF(lines);
    if (formatted_obj == NULL) {
        return NULL;
    }

    Py_ssize_t size = -1;
    const char *formatted = _copy_string_obj_raw(formatted_obj, &size);
    Py_DECREF(formatted_obj);
    // We remove trailing the newline added by TracebackException.format().
    assert(formatted[size-1] == '\n');
    ((char *)formatted)[size-1] = '\0';
    return formatted;
}


static int
_release_xid_data(_PyXIData_t *xidata, int rawfree)
{
    PyObject *exc = PyErr_GetRaisedException();
    int res = rawfree
        ? _PyXIData_Release(xidata)
        : _PyXIData_ReleaseAndRawFree(xidata);
    if (res < 0) {
        /* The owning interpreter is already destroyed. */
        _PyXIData_Clear(NULL, xidata);
        // XXX Emit a warning?
        PyErr_Clear();
    }
    PyErr_SetRaisedException(exc);
    return res;
}


/***********************/
/* exception snapshots */
/***********************/

static int
_excinfo_init_type_from_exception(struct _excinfo_type *info, PyObject *exc)
{
    /* Note that this copies directly rather than into an intermediate
       struct and does not clear on error.  If we need that then we
       should have a separate function to wrap this one
       and do all that there. */
    PyObject *strobj = NULL;

    PyTypeObject *type = Py_TYPE(exc);
    if (type->tp_flags & _Py_TPFLAGS_STATIC_BUILTIN) {
        assert(_Py_IsImmortal((PyObject *)type));
        info->builtin = type;
    }
    else {
        // Only builtin types are preserved.
        info->builtin = NULL;
    }

    // __name__
    strobj = PyType_GetName(type);
    if (strobj == NULL) {
        return -1;
    }
    info->name = _copy_string_obj_raw(strobj, NULL);
    Py_DECREF(strobj);
    if (info->name == NULL) {
        return -1;
    }

    // __qualname__
    strobj = PyType_GetQualName(type);
    if (strobj == NULL) {
        return -1;
    }
    info->qualname = _copy_string_obj_raw(strobj, NULL);
    Py_DECREF(strobj);
    if (info->qualname == NULL) {
        return -1;
    }

    // __module__
    strobj = PyType_GetModuleName(type);
    if (strobj == NULL) {
        return -1;
    }
    info->module = _copy_string_obj_raw(strobj, NULL);
    Py_DECREF(strobj);
    if (info->module == NULL) {
        return -1;
    }

    return 0;
}

static int
_excinfo_init_type_from_object(struct _excinfo_type *info, PyObject *exctype)
{
    PyObject *strobj = NULL;

    // __name__
    strobj = PyObject_GetAttrString(exctype, "__name__");
    if (strobj == NULL) {
        return -1;
    }
    info->name = _copy_string_obj_raw(strobj, NULL);
    Py_DECREF(strobj);
    if (info->name == NULL) {
        return -1;
    }

    // __qualname__
    strobj = PyObject_GetAttrString(exctype, "__qualname__");
    if (strobj == NULL) {
        return -1;
    }
    info->qualname = _copy_string_obj_raw(strobj, NULL);
    Py_DECREF(strobj);
    if (info->qualname == NULL) {
        return -1;
    }

    // __module__
    strobj = PyObject_GetAttrString(exctype, "__module__");
    if (strobj == NULL) {
        return -1;
    }
    info->module = _copy_string_obj_raw(strobj, NULL);
    Py_DECREF(strobj);
    if (info->module == NULL) {
        return -1;
    }

    return 0;
}

static void
_excinfo_clear_type(struct _excinfo_type *info)
{
    if (info->builtin != NULL) {
        assert(info->builtin->tp_flags & _Py_TPFLAGS_STATIC_BUILTIN);
        assert(_Py_IsImmortal((PyObject *)info->builtin));
    }
    if (info->name != NULL) {
        PyMem_RawFree((void *)info->name);
    }
    if (info->qualname != NULL) {
        PyMem_RawFree((void *)info->qualname);
    }
    if (info->module != NULL) {
        PyMem_RawFree((void *)info->module);
    }
    *info = (struct _excinfo_type){NULL};
}

static void
_excinfo_normalize_type(struct _excinfo_type *info,
                        const char **p_module, const char **p_qualname)
{
    if (info->name == NULL) {
        assert(info->builtin == NULL);
        assert(info->qualname == NULL);
        assert(info->module == NULL);
        // This is inspired by TracebackException.format_exception_only().
        *p_module = NULL;
        *p_qualname = NULL;
        return;
    }

    const char *module = info->module;
    const char *qualname = info->qualname;
    if (qualname == NULL) {
        qualname = info->name;
    }
    assert(module != NULL);
    if (strcmp(module, "builtins") == 0) {
        module = NULL;
    }
    else if (strcmp(module, "__main__") == 0) {
        module = NULL;
    }
    *p_qualname = qualname;
    *p_module = module;
}

static void
_PyXI_excinfo_Clear(_PyXI_excinfo *info)
{
    _excinfo_clear_type(&info->type);
    if (info->msg != NULL) {
        PyMem_RawFree((void *)info->msg);
    }
    if (info->errdisplay != NULL) {
        PyMem_RawFree((void *)info->errdisplay);
    }
    *info = (_PyXI_excinfo){{NULL}};
}

PyObject *
_PyXI_excinfo_format(_PyXI_excinfo *info)
{
    const char *module, *qualname;
    _excinfo_normalize_type(&info->type, &module, &qualname);
    if (qualname != NULL) {
        if (module != NULL) {
            if (info->msg != NULL) {
                return PyUnicode_FromFormat("%s.%s: %s",
                                            module, qualname, info->msg);
            }
            else {
                return PyUnicode_FromFormat("%s.%s", module, qualname);
            }
        }
        else {
            if (info->msg != NULL) {
                return PyUnicode_FromFormat("%s: %s", qualname, info->msg);
            }
            else {
                return PyUnicode_FromString(qualname);
            }
        }
    }
    else if (info->msg != NULL) {
        return PyUnicode_FromString(info->msg);
    }
    else {
        Py_RETURN_NONE;
    }
}

static const char *
_PyXI_excinfo_InitFromException(_PyXI_excinfo *info, PyObject *exc)
{
    assert(exc != NULL);

    if (PyErr_GivenExceptionMatches(exc, PyExc_MemoryError)) {
        _PyXI_excinfo_Clear(info);
        return NULL;
    }
    const char *failure = NULL;

    if (_excinfo_init_type_from_exception(&info->type, exc) < 0) {
        failure = "error while initializing exception type snapshot";
        goto error;
    }

    // Extract the exception message.
    PyObject *msgobj = PyObject_Str(exc);
    if (msgobj == NULL) {
        failure = "error while formatting exception";
        goto error;
    }
    info->msg = _copy_string_obj_raw(msgobj, NULL);
    Py_DECREF(msgobj);
    if (info->msg == NULL) {
        failure = "error while copying exception message";
        goto error;
    }

    // Pickle a traceback.TracebackException.
    PyObject *tbexc = NULL;
    if (_convert_exc_to_TracebackException(exc, &tbexc) < 0) {
#ifdef Py_DEBUG
        PyErr_FormatUnraisable("Exception ignored while creating TracebackException");
#endif
        PyErr_Clear();
    }
    else {
        info->errdisplay = _format_TracebackException(tbexc);
        Py_DECREF(tbexc);
        if (info->errdisplay == NULL) {
#ifdef Py_DEBUG
            PyErr_FormatUnraisable("Exception ignored while formatting TracebackException");
#endif
            PyErr_Clear();
        }
    }

    return NULL;

error:
    assert(failure != NULL);
    _PyXI_excinfo_Clear(info);
    return failure;
}

static const char *
_PyXI_excinfo_InitFromObject(_PyXI_excinfo *info, PyObject *obj)
{
    const char *failure = NULL;

    PyObject *exctype = PyObject_GetAttrString(obj, "type");
    if (exctype == NULL) {
        failure = "exception snapshot missing 'type' attribute";
        goto error;
    }
    int res = _excinfo_init_type_from_object(&info->type, exctype);
    Py_DECREF(exctype);
    if (res < 0) {
        failure = "error while initializing exception type snapshot";
        goto error;
    }

    // Extract the exception message.
    PyObject *msgobj = PyObject_GetAttrString(obj, "msg");
    if (msgobj == NULL) {
        failure = "exception snapshot missing 'msg' attribute";
        goto error;
    }
    info->msg = _copy_string_obj_raw(msgobj, NULL);
    Py_DECREF(msgobj);
    if (info->msg == NULL) {
        failure = "error while copying exception message";
        goto error;
    }

    // Pickle a traceback.TracebackException.
    PyObject *errdisplay = PyObject_GetAttrString(obj, "errdisplay");
    if (errdisplay == NULL) {
        failure = "exception snapshot missing 'errdisplay' attribute";
        goto error;
    }
    info->errdisplay = _copy_string_obj_raw(errdisplay, NULL);
    Py_DECREF(errdisplay);
    if (info->errdisplay == NULL) {
        failure = "error while copying exception error display";
        goto error;
    }

    return NULL;

error:
    assert(failure != NULL);
    _PyXI_excinfo_Clear(info);
    return failure;
}

static void
_PyXI_excinfo_Apply(_PyXI_excinfo *info, PyObject *exctype)
{
    PyObject *tbexc = NULL;
    if (info->errdisplay != NULL) {
        tbexc = PyUnicode_FromString(info->errdisplay);
        if (tbexc == NULL) {
            PyErr_Clear();
        }
    }

    PyObject *formatted = _PyXI_excinfo_format(info);
    PyErr_SetObject(exctype, formatted);
    Py_DECREF(formatted);

    if (tbexc != NULL) {
        PyObject *exc = PyErr_GetRaisedException();
        if (PyObject_SetAttrString(exc, "_errdisplay", tbexc) < 0) {
#ifdef Py_DEBUG
            PyErr_FormatUnraisable("Exception ignored while "
                                   "setting _errdisplay");
#endif
            PyErr_Clear();
        }
        Py_DECREF(tbexc);
        PyErr_SetRaisedException(exc);
    }
}

static PyObject *
_PyXI_excinfo_TypeAsObject(_PyXI_excinfo *info)
{
    PyObject *ns = _PyNamespace_New(NULL);
    if (ns == NULL) {
        return NULL;
    }
    int empty = 1;

    if (info->type.name != NULL) {
        PyObject *name = PyUnicode_FromString(info->type.name);
        if (name == NULL) {
            goto error;
        }
        int res = PyObject_SetAttrString(ns, "__name__", name);
        Py_DECREF(name);
        if (res < 0) {
            goto error;
        }
        empty = 0;
    }

    if (info->type.qualname != NULL) {
        PyObject *qualname = PyUnicode_FromString(info->type.qualname);
        if (qualname == NULL) {
            goto error;
        }
        int res = PyObject_SetAttrString(ns, "__qualname__", qualname);
        Py_DECREF(qualname);
        if (res < 0) {
            goto error;
        }
        empty = 0;
    }

    if (info->type.module != NULL) {
        PyObject *module = PyUnicode_FromString(info->type.module);
        if (module == NULL) {
            goto error;
        }
        int res = PyObject_SetAttrString(ns, "__module__", module);
        Py_DECREF(module);
        if (res < 0) {
            goto error;
        }
        empty = 0;
    }

    if (empty) {
        Py_CLEAR(ns);
    }

    return ns;

error:
    Py_DECREF(ns);
    return NULL;
}

static PyObject *
_PyXI_excinfo_AsObject(_PyXI_excinfo *info)
{
    PyObject *ns = _PyNamespace_New(NULL);
    if (ns == NULL) {
        return NULL;
    }
    int res;

    PyObject *type = _PyXI_excinfo_TypeAsObject(info);
    if (type == NULL) {
        if (PyErr_Occurred()) {
            goto error;
        }
        type = Py_NewRef(Py_None);
    }
    res = PyObject_SetAttrString(ns, "type", type);
    Py_DECREF(type);
    if (res < 0) {
        goto error;
    }

    PyObject *msg = info->msg != NULL
        ? PyUnicode_FromString(info->msg)
        : Py_NewRef(Py_None);
    if (msg == NULL) {
        goto error;
    }
    res = PyObject_SetAttrString(ns, "msg", msg);
    Py_DECREF(msg);
    if (res < 0) {
        goto error;
    }

    PyObject *formatted = _PyXI_excinfo_format(info);
    if (formatted == NULL) {
        goto error;
    }
    res = PyObject_SetAttrString(ns, "formatted", formatted);
    Py_DECREF(formatted);
    if (res < 0) {
        goto error;
    }

    if (info->errdisplay != NULL) {
        PyObject *tbexc = PyUnicode_FromString(info->errdisplay);
        if (tbexc == NULL) {
            PyErr_Clear();
        }
        else {
            res = PyObject_SetAttrString(ns, "errdisplay", tbexc);
            Py_DECREF(tbexc);
            if (res < 0) {
                goto error;
            }
        }
    }

    return ns;

error:
    Py_DECREF(ns);
    return NULL;
}


int
_PyXI_InitExcInfo(_PyXI_excinfo *info, PyObject *exc)
{
    assert(!PyErr_Occurred());
    if (exc == NULL || exc == Py_None) {
        PyErr_SetString(PyExc_ValueError, "missing exc");
        return -1;
    }
    const char *failure;
    if (PyExceptionInstance_Check(exc) || PyExceptionClass_Check(exc)) {
        failure = _PyXI_excinfo_InitFromException(info, exc);
    }
    else {
        failure = _PyXI_excinfo_InitFromObject(info, exc);
    }
    if (failure != NULL) {
        PyErr_SetString(PyExc_Exception, failure);
        return -1;
    }
    return 0;
}

PyObject *
_PyXI_FormatExcInfo(_PyXI_excinfo *info)
{
    return _PyXI_excinfo_format(info);
}

PyObject *
_PyXI_ExcInfoAsObject(_PyXI_excinfo *info)
{
    return _PyXI_excinfo_AsObject(info);
}

void
_PyXI_ClearExcInfo(_PyXI_excinfo *info)
{
    _PyXI_excinfo_Clear(info);
}


/***************************/
/* short-term data sharing */
/***************************/

/* error codes */

static int
_PyXI_ApplyErrorCode(_PyXI_errcode code, PyInterpreterState *interp)
{
    PyThreadState *tstate = _PyThreadState_GET();

    assert(!PyErr_Occurred());
    switch (code) {
    case _PyXI_ERR_NO_ERROR: _Py_FALLTHROUGH;
    case _PyXI_ERR_UNCAUGHT_EXCEPTION:
        // There is nothing to apply.
#ifdef Py_DEBUG
        Py_UNREACHABLE();
#endif
        return 0;
    case _PyXI_ERR_OTHER:
        // XXX msg?
        PyErr_SetNone(PyExc_InterpreterError);
        break;
    case _PyXI_ERR_NO_MEMORY:
        PyErr_NoMemory();
        break;
    case _PyXI_ERR_ALREADY_RUNNING:
        assert(interp != NULL);
        _PyErr_SetInterpreterAlreadyRunning();
        break;
    case _PyXI_ERR_MAIN_NS_FAILURE:
        PyErr_SetString(PyExc_InterpreterError,
                        "failed to get __main__ namespace");
        break;
    case _PyXI_ERR_APPLY_NS_FAILURE:
        PyErr_SetString(PyExc_InterpreterError,
                        "failed to apply namespace to __main__");
        break;
    case _PyXI_ERR_NOT_SHAREABLE:
        _set_xid_lookup_failure(tstate, NULL, NULL);
        break;
    default:
#ifdef Py_DEBUG
        Py_UNREACHABLE();
#else
        PyErr_Format(PyExc_RuntimeError, "unsupported error code %d", code);
#endif
    }
    assert(PyErr_Occurred());
    return -1;
}

/* shared exceptions */

static const char *
_PyXI_InitError(_PyXI_error *error, PyObject *excobj, _PyXI_errcode code)
{
    if (error->interp == NULL) {
        error->interp = PyInterpreterState_Get();
    }

    const char *failure = NULL;
    if (code == _PyXI_ERR_UNCAUGHT_EXCEPTION) {
        // There is an unhandled exception we need to propagate.
        failure = _PyXI_excinfo_InitFromException(&error->uncaught, excobj);
        if (failure != NULL) {
            // We failed to initialize error->uncaught.
            // XXX Print the excobj/traceback?  Emit a warning?
            // XXX Print the current exception/traceback?
            if (PyErr_ExceptionMatches(PyExc_MemoryError)) {
                error->code = _PyXI_ERR_NO_MEMORY;
            }
            else {
                error->code = _PyXI_ERR_OTHER;
            }
            PyErr_Clear();
        }
        else {
            error->code = code;
        }
        assert(error->code != _PyXI_ERR_NO_ERROR);
    }
    else {
        // There is an error code we need to propagate.
        assert(excobj == NULL);
        assert(code != _PyXI_ERR_NO_ERROR);
        error->code = code;
        _PyXI_excinfo_Clear(&error->uncaught);
    }
    return failure;
}

PyObject *
_PyXI_ApplyError(_PyXI_error *error)
{
    PyThreadState *tstate = PyThreadState_Get();
    if (error->code == _PyXI_ERR_UNCAUGHT_EXCEPTION) {
        // Raise an exception that proxies the propagated exception.
       return _PyXI_excinfo_AsObject(&error->uncaught);
    }
    else if (error->code == _PyXI_ERR_NOT_SHAREABLE) {
        // Propagate the exception directly.
        _set_xid_lookup_failure(tstate, NULL, error->uncaught.msg);
    }
    else {
        // Raise an exception corresponding to the code.
        assert(error->code != _PyXI_ERR_NO_ERROR);
        (void)_PyXI_ApplyErrorCode(error->code, error->interp);
        if (error->uncaught.type.name != NULL || error->uncaught.msg != NULL) {
            // __context__ will be set to a proxy of the propagated exception.
            PyObject *exc = PyErr_GetRaisedException();
            _PyXI_excinfo_Apply(&error->uncaught, PyExc_InterpreterError);
            PyObject *exc2 = PyErr_GetRaisedException();
            PyException_SetContext(exc, exc2);
            PyErr_SetRaisedException(exc);
        }
    }
    assert(PyErr_Occurred());
    return NULL;
}

/* shared namespaces */

/* Shared namespaces are expected to have relatively short lifetimes.
   This means dealloc of a shared namespace will normally happen "soon".
   Namespace items hold cross-interpreter data, which must get released.
   If the namespace/items are cleared in a different interpreter than
   where the items' cross-interpreter data was set then that will cause
   pending calls to be used to release the cross-interpreter data.
   The tricky bit is that the pending calls can happen sufficiently
   later that the namespace/items might already be deallocated.  This is
   a problem if the cross-interpreter data is allocated as part of a
   namespace item.  If that's the case then we must ensure the shared
   namespace is only cleared/freed *after* that data has been released. */

typedef struct _sharednsitem {
    const char *name;
    _PyXIData_t *xidata;
    // We could have a "PyXIData _data" field, so it would
    // be allocated as part of the item and avoid an extra allocation.
    // However, doing so adds a bunch of complexity because we must
    // ensure the item isn't freed before a pending call might happen
    // in a different interpreter to release the XI data.
} _PyXI_namespace_item;

static int
_sharednsitem_is_initialized(_PyXI_namespace_item *item)
{
    if (item->name != NULL) {
        return 1;
    }
    return 0;
}

static int
_sharednsitem_init(_PyXI_namespace_item *item, PyObject *key)
{
    item->name = _copy_string_obj_raw(key, NULL);
    if (item->name == NULL) {
        assert(!_sharednsitem_is_initialized(item));
        return -1;
    }
    item->xidata = NULL;
    assert(_sharednsitem_is_initialized(item));
    return 0;
}

static int
_sharednsitem_has_value(_PyXI_namespace_item *item, int64_t *p_interpid)
{
    if (item->xidata == NULL) {
        return 0;
    }
    if (p_interpid != NULL) {
        *p_interpid = _PyXIData_INTERPID(item->xidata);
    }
    return 1;
}

static int
_sharednsitem_set_value(_PyXI_namespace_item *item, PyObject *value)
{
    assert(_sharednsitem_is_initialized(item));
    assert(item->xidata == NULL);
    item->xidata = PyMem_RawMalloc(sizeof(_PyXIData_t));
    if (item->xidata == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    PyThreadState *tstate = PyThreadState_Get();
    if (_PyObject_GetXIData(tstate, value, item->xidata) != 0) {
        PyMem_RawFree(item->xidata);
        item->xidata = NULL;
        // The caller may want to propagate PyExc_NotShareableError
        // if currently switched between interpreters.
        return -1;
    }
    return 0;
}

static void
_sharednsitem_clear_value(_PyXI_namespace_item *item)
{
    _PyXIData_t *xidata = item->xidata;
    if (xidata != NULL) {
        item->xidata = NULL;
        int rawfree = 1;
        (void)_release_xid_data(xidata, rawfree);
    }
}

static void
_sharednsitem_clear(_PyXI_namespace_item *item)
{
    if (item->name != NULL) {
        PyMem_RawFree((void *)item->name);
        item->name = NULL;
    }
    _sharednsitem_clear_value(item);
}

static int
_sharednsitem_copy_from_ns(struct _sharednsitem *item, PyObject *ns)
{
    assert(item->name != NULL);
    assert(item->xidata == NULL);
    PyObject *value = PyDict_GetItemString(ns, item->name);  // borrowed
    if (value == NULL) {
        if (PyErr_Occurred()) {
            return -1;
        }
        // When applied, this item will be set to the default (or fail).
        return 0;
    }
    if (_sharednsitem_set_value(item, value) < 0) {
        return -1;
    }
    return 0;
}

static int
_sharednsitem_apply(_PyXI_namespace_item *item, PyObject *ns, PyObject *dflt)
{
    PyObject *name = PyUnicode_FromString(item->name);
    if (name == NULL) {
        return -1;
    }
    PyObject *value;
    if (item->xidata != NULL) {
        value = _PyXIData_NewObject(item->xidata);
        if (value == NULL) {
            Py_DECREF(name);
            return -1;
        }
    }
    else {
        value = Py_NewRef(dflt);
    }
    int res = PyDict_SetItem(ns, name, value);
    Py_DECREF(name);
    Py_DECREF(value);
    return res;
}

struct _sharedns {
    Py_ssize_t len;
    _PyXI_namespace_item *items;
};

static _PyXI_namespace *
_sharedns_new(void)
{
    _PyXI_namespace *ns = PyMem_RawCalloc(sizeof(_PyXI_namespace), 1);
    if (ns == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    *ns = (_PyXI_namespace){ 0 };
    return ns;
}

static int
_sharedns_is_initialized(_PyXI_namespace *ns)
{
    if (ns->len == 0) {
        assert(ns->items == NULL);
        return 0;
    }

    assert(ns->len > 0);
    assert(ns->items != NULL);
    assert(_sharednsitem_is_initialized(&ns->items[0]));
    assert(ns->len == 1
           || _sharednsitem_is_initialized(&ns->items[ns->len - 1]));
    return 1;
}

#define HAS_COMPLETE_DATA 1
#define HAS_PARTIAL_DATA 2

static int
_sharedns_has_xidata(_PyXI_namespace *ns, int64_t *p_interpid)
{
    // We expect _PyXI_namespace to always be initialized.
    assert(_sharedns_is_initialized(ns));
    int res = 0;
    _PyXI_namespace_item *item0 = &ns->items[0];
    if (!_sharednsitem_is_initialized(item0)) {
        return 0;
    }
    int64_t interpid0 = -1;
    if (!_sharednsitem_has_value(item0, &interpid0)) {
        return 0;
    }
    if (ns->len > 1) {
        // At this point we know it is has at least partial data.
        _PyXI_namespace_item *itemN = &ns->items[ns->len-1];
        if (!_sharednsitem_is_initialized(itemN)) {
            res = HAS_PARTIAL_DATA;
            goto finally;
        }
        int64_t interpidN = -1;
        if (!_sharednsitem_has_value(itemN, &interpidN)) {
            res = HAS_PARTIAL_DATA;
            goto finally;
        }
        assert(interpidN == interpid0);
    }
    res = HAS_COMPLETE_DATA;
    *p_interpid = interpid0;

finally:
    return res;
}

static void
_sharedns_clear(_PyXI_namespace *ns)
{
    if (!_sharedns_is_initialized(ns)) {
        return;
    }

    // If the cross-interpreter data were allocated as part of
    // _PyXI_namespace_item (instead of dynamically), this is where
    // we would need verify that we are clearing the items in the
    // correct interpreter, to avoid a race with releasing the XI data
    // via a pending call.  See _sharedns_has_xidata().
    for (Py_ssize_t i=0; i < ns->len; i++) {
        _sharednsitem_clear(&ns->items[i]);
    }
    PyMem_RawFree(ns->items);
    ns->items = NULL;
    ns->len = 0;
}

static void
_sharedns_free(_PyXI_namespace *ns)
{
    _sharedns_clear(ns);
    PyMem_RawFree(ns);
}

static int
_sharedns_init(_PyXI_namespace *ns, PyObject *names)
{
    assert(!_sharedns_is_initialized(ns));
    assert(names != NULL);
    Py_ssize_t len = PyDict_CheckExact(names)
        ? PyDict_Size(names)
        : PySequence_Size(names);
    if (len < 0) {
        return -1;
    }
    if (len == 0) {
        PyErr_SetString(PyExc_ValueError, "empty namespaces not allowed");
        return -1;
    }
    assert(len > 0);

    // Allocate the items.
    _PyXI_namespace_item *items =
            PyMem_RawCalloc(sizeof(struct _sharednsitem), len);
    if (items == NULL) {
        PyErr_NoMemory();
        return -1;
    }

    // Fill in the names.
    Py_ssize_t i = -1;
    if (PyDict_CheckExact(names)) {
        Py_ssize_t pos = 0;
        for (i=0; i < len; i++) {
            PyObject *key;
            if (!PyDict_Next(names, &pos, &key, NULL)) {
                // This should not be possible.
                assert(0);
                goto error;
            }
            if (_sharednsitem_init(&items[i], key) < 0) {
                goto error;
            }
        }
    }
    else if (PySequence_Check(names)) {
        for (i=0; i < len; i++) {
            PyObject *key = PySequence_GetItem(names, i);
            if (key == NULL) {
                goto error;
            }
            int res = _sharednsitem_init(&items[i], key);
            Py_DECREF(key);
            if (res < 0) {
                goto error;
            }
        }
    }
    else {
        PyErr_SetString(PyExc_NotImplementedError,
                        "non-sequence namespace not supported");
        goto error;
    }

    ns->items = items;
    ns->len = len;
    assert(_sharedns_is_initialized(ns));
    return 0;

error:
    for (Py_ssize_t j=0; j < i; j++) {
        _sharednsitem_clear(&items[j]);
    }
    PyMem_RawFree(items);
    assert(!_sharedns_is_initialized(ns));
    return -1;
}

void
_PyXI_FreeNamespace(_PyXI_namespace *ns)
{
    if (!_sharedns_is_initialized(ns)) {
        return;
    }

    int64_t interpid = -1;
    if (!_sharedns_has_xidata(ns, &interpid)) {
        _sharedns_free(ns);
        return;
    }

    if (interpid == PyInterpreterState_GetID(PyInterpreterState_Get())) {
        _sharedns_free(ns);
    }
    else {
        // If we weren't always dynamically allocating the cross-interpreter
        // data in each item then we would need to using a pending call
        // to call _sharedns_free(), to avoid the race between freeing
        // the shared namespace and releasing the XI data.
        _sharedns_free(ns);
    }
}

_PyXI_namespace *
_PyXI_NamespaceFromNames(PyObject *names)
{
    if (names == NULL || names == Py_None) {
        return NULL;
    }

    _PyXI_namespace *ns = _sharedns_new();
    if (ns == NULL) {
        return NULL;
    }

    if (_sharedns_init(ns, names) < 0) {
        PyMem_RawFree(ns);
        if (PySequence_Size(names) == 0) {
            PyErr_Clear();
        }
        return NULL;
    }

    return ns;
}

#ifndef NDEBUG
static int _session_is_active(_PyXI_session *);
#endif
static void _propagate_not_shareable_error(_PyXI_session *);

int
_PyXI_FillNamespaceFromDict(_PyXI_namespace *ns, PyObject *nsobj,
                            _PyXI_session *session)
{
    // session must be entered already, if provided.
    assert(session == NULL || _session_is_active(session));
    assert(_sharedns_is_initialized(ns));
    for (Py_ssize_t i=0; i < ns->len; i++) {
        _PyXI_namespace_item *item = &ns->items[i];
        if (_sharednsitem_copy_from_ns(item, nsobj) < 0) {
            _propagate_not_shareable_error(session);
            // Clear out the ones we set so far.
            for (Py_ssize_t j=0; j < i; j++) {
                _sharednsitem_clear_value(&ns->items[j]);
            }
            return -1;
        }
    }
    return 0;
}

// All items are expected to be shareable.
static _PyXI_namespace *
_PyXI_NamespaceFromDict(PyObject *nsobj, _PyXI_session *session)
{
    // session must be entered already, if provided.
    assert(session == NULL || _session_is_active(session));
    if (nsobj == NULL || nsobj == Py_None) {
        return NULL;
    }
    if (!PyDict_CheckExact(nsobj)) {
        PyErr_SetString(PyExc_TypeError, "expected a dict");
        return NULL;
    }

    _PyXI_namespace *ns = _sharedns_new();
    if (ns == NULL) {
        return NULL;
    }

    if (_sharedns_init(ns, nsobj) < 0) {
        if (PyDict_Size(nsobj) == 0) {
            PyMem_RawFree(ns);
            PyErr_Clear();
            return NULL;
        }
        goto error;
    }

    if (_PyXI_FillNamespaceFromDict(ns, nsobj, session) < 0) {
        goto error;
    }

    return ns;

error:
    assert(PyErr_Occurred()
           || (session != NULL && session->error_override != NULL));
    _sharedns_free(ns);
    return NULL;
}

int
_PyXI_ApplyNamespace(_PyXI_namespace *ns, PyObject *nsobj, PyObject *dflt)
{
    for (Py_ssize_t i=0; i < ns->len; i++) {
        if (_sharednsitem_apply(&ns->items[i], nsobj, dflt) != 0) {
            return -1;
        }
    }
    return 0;
}


/**********************/
/* high-level helpers */
/**********************/

/* enter/exit a cross-interpreter session */

static void
_enter_session(_PyXI_session *session, PyInterpreterState *interp)
{
    // Set here and cleared in _exit_session().
    assert(!session->own_init_tstate);
    assert(session->init_tstate == NULL);
    assert(session->prev_tstate == NULL);
    // Set elsewhere and cleared in _exit_session().
    assert(!session->running);
    assert(session->main_ns == NULL);
    // Set elsewhere and cleared in _capture_current_exception().
    assert(session->error_override == NULL);
    // Set elsewhere and cleared in _PyXI_ApplyCapturedException().
    assert(session->error == NULL);

    // Switch to interpreter.
    PyThreadState *tstate = PyThreadState_Get();
    PyThreadState *prev = tstate;
    if (interp != tstate->interp) {
        tstate = _PyThreadState_NewBound(interp, _PyThreadState_WHENCE_EXEC);
        // XXX Possible GILState issues?
        session->prev_tstate = PyThreadState_Swap(tstate);
        assert(session->prev_tstate == prev);
        session->own_init_tstate = 1;
    }
    session->init_tstate = tstate;
    session->prev_tstate = prev;
}

static void
_exit_session(_PyXI_session *session)
{
    PyThreadState *tstate = session->init_tstate;
    assert(tstate != NULL);
    assert(PyThreadState_Get() == tstate);

    // Release any of the entered interpreters resources.
    if (session->main_ns != NULL) {
        Py_CLEAR(session->main_ns);
    }

    // Ensure this thread no longer owns __main__.
    if (session->running) {
        _PyInterpreterState_SetNotRunningMain(tstate->interp);
        assert(!PyErr_Occurred());
        session->running = 0;
    }

    // Switch back.
    assert(session->prev_tstate != NULL);
    if (session->prev_tstate != session->init_tstate) {
        assert(session->own_init_tstate);
        session->own_init_tstate = 0;
        PyThreadState_Clear(tstate);
        PyThreadState_Swap(session->prev_tstate);
        PyThreadState_Delete(tstate);
    }
    else {
        assert(!session->own_init_tstate);
    }
    session->prev_tstate = NULL;
    session->init_tstate = NULL;
}

#ifndef NDEBUG
static int
_session_is_active(_PyXI_session *session)
{
    return (session->init_tstate != NULL);
}
#endif

static void
_propagate_not_shareable_error(_PyXI_session *session)
{
    if (session == NULL) {
        return;
    }
    PyThreadState *tstate = PyThreadState_Get();
    PyObject *exctype = get_notshareableerror_type(tstate);
    if (exctype == NULL) {
        PyErr_FormatUnraisable(
                "Exception ignored while propagating not shareable error");
        return;
    }
    if (PyErr_ExceptionMatches(exctype)) {
        // We want to propagate the exception directly.
        session->_error_override = _PyXI_ERR_NOT_SHAREABLE;
        session->error_override = &session->_error_override;
    }
}

static void
_capture_current_exception(_PyXI_session *session)
{
    assert(session->error == NULL);
    if (!PyErr_Occurred()) {
        assert(session->error_override == NULL);
        return;
    }

    // Handle the exception override.
    _PyXI_errcode *override = session->error_override;
    session->error_override = NULL;
    _PyXI_errcode errcode = override != NULL
        ? *override
        : _PyXI_ERR_UNCAUGHT_EXCEPTION;

    // Pop the exception object.
    PyObject *excval = NULL;
    if (errcode == _PyXI_ERR_UNCAUGHT_EXCEPTION) {
        // We want to actually capture the current exception.
        excval = PyErr_GetRaisedException();
    }
    else if (errcode == _PyXI_ERR_ALREADY_RUNNING) {
        // We don't need the exception info.
        PyErr_Clear();
    }
    else {
        // We could do a variety of things here, depending on errcode.
        // However, for now we simply capture the exception and save
        // the errcode.
        excval = PyErr_GetRaisedException();
    }

    // Capture the exception.
    _PyXI_error *err = &session->_error;
    *err = (_PyXI_error){
        .interp = session->init_tstate->interp,
    };
    const char *failure;
    if (excval == NULL) {
        failure = _PyXI_InitError(err, NULL, errcode);
    }
    else {
        failure = _PyXI_InitError(err, excval, _PyXI_ERR_UNCAUGHT_EXCEPTION);
        Py_DECREF(excval);
        if (failure == NULL && override != NULL) {
            err->code = errcode;
        }
    }

    // Handle capture failure.
    if (failure != NULL) {
        // XXX Make this error message more generic.
        fprintf(stderr,
                "RunFailedError: script raised an uncaught exception (%s)",
                failure);
        err = NULL;
    }

    // Finished!
    assert(!PyErr_Occurred());
    session->error  = err;
}

PyObject *
_PyXI_ApplyCapturedException(_PyXI_session *session)
{
    assert(!PyErr_Occurred());
    assert(session->error != NULL);
    PyObject *res = _PyXI_ApplyError(session->error);
    assert((res == NULL) != (PyErr_Occurred() == NULL));
    session->error = NULL;
    return res;
}

int
_PyXI_HasCapturedException(_PyXI_session *session)
{
    return session->error != NULL;
}

int
_PyXI_Enter(_PyXI_session *session,
            PyInterpreterState *interp, PyObject *nsupdates)
{
    // Convert the attrs for cross-interpreter use.
    _PyXI_namespace *sharedns = NULL;
    if (nsupdates != NULL) {
        sharedns = _PyXI_NamespaceFromDict(nsupdates, NULL);
        if (sharedns == NULL && PyErr_Occurred()) {
            assert(session->error == NULL);
            return -1;
        }
    }

    // Switch to the requested interpreter (if necessary).
    _enter_session(session, interp);
    PyThreadState *session_tstate = session->init_tstate;
    _PyXI_errcode errcode = _PyXI_ERR_UNCAUGHT_EXCEPTION;

    // Ensure this thread owns __main__.
    if (_PyInterpreterState_SetRunningMain(interp) < 0) {
        // In the case where we didn't switch interpreters, it would
        // be more efficient to leave the exception in place and return
        // immediately.  However, life is simpler if we don't.
        errcode = _PyXI_ERR_ALREADY_RUNNING;
        goto error;
    }
    session->running = 1;

    // Cache __main__.__dict__.
    PyObject *main_mod = _Py_GetMainModule(session_tstate);
    if (_Py_CheckMainModule(main_mod) < 0) {
        errcode = _PyXI_ERR_MAIN_NS_FAILURE;
        goto error;
    }
    PyObject *ns = PyModule_GetDict(main_mod);  // borrowed
    Py_DECREF(main_mod);
    if (ns == NULL) {
        errcode = _PyXI_ERR_MAIN_NS_FAILURE;
        goto error;
    }
    session->main_ns = Py_NewRef(ns);

    // Apply the cross-interpreter data.
    if (sharedns != NULL) {
        if (_PyXI_ApplyNamespace(sharedns, ns, NULL) < 0) {
            errcode = _PyXI_ERR_APPLY_NS_FAILURE;
            goto error;
        }
        _PyXI_FreeNamespace(sharedns);
    }

    errcode = _PyXI_ERR_NO_ERROR;
    assert(!PyErr_Occurred());
    return 0;

error:
    assert(PyErr_Occurred());
    // We want to propagate all exceptions here directly (best effort).
    assert(errcode != _PyXI_ERR_UNCAUGHT_EXCEPTION);
    session->error_override = &errcode;
    _capture_current_exception(session);
    _exit_session(session);
    if (sharedns != NULL) {
        _PyXI_FreeNamespace(sharedns);
    }
    return -1;
}

void
_PyXI_Exit(_PyXI_session *session)
{
    _capture_current_exception(session);
    _exit_session(session);
}


/*********************/
/* runtime lifecycle */
/*********************/

int
_Py_xi_global_state_init(_PyXI_global_state_t *state)
{
    assert(state != NULL);
    xid_lookup_init(&state->data_lookup);
    return 0;
}

void
_Py_xi_global_state_fini(_PyXI_global_state_t *state)
{
    assert(state != NULL);
    xid_lookup_fini(&state->data_lookup);
}

int
_Py_xi_state_init(_PyXI_state_t *state, PyInterpreterState *interp)
{
    assert(state != NULL);
    assert(interp == NULL || state == _PyXI_GET_STATE(interp));

    xid_lookup_init(&state->data_lookup);

    // Initialize exceptions.
    if (interp != NULL) {
        if (init_static_exctypes(&state->exceptions, interp) < 0) {
            fini_heap_exctypes(&state->exceptions);
            return -1;
        }
    }
    if (init_heap_exctypes(&state->exceptions) < 0) {
        return -1;
    }

    return 0;
}

void
_Py_xi_state_fini(_PyXI_state_t *state, PyInterpreterState *interp)
{
    assert(state != NULL);
    assert(interp == NULL || state == _PyXI_GET_STATE(interp));

    fini_heap_exctypes(&state->exceptions);
    if (interp != NULL) {
        fini_static_exctypes(&state->exceptions, interp);
    }

    xid_lookup_fini(&state->data_lookup);
}


PyStatus
_PyXI_Init(PyInterpreterState *interp)
{
    if (_Py_IsMainInterpreter(interp)) {
        _PyXI_global_state_t *global_state = _PyXI_GET_GLOBAL_STATE(interp);
        if (global_state == NULL) {
            PyErr_PrintEx(0);
            return _PyStatus_ERR(
                    "failed to get global cross-interpreter state");
        }
        if (_Py_xi_global_state_init(global_state) < 0) {
            PyErr_PrintEx(0);
            return _PyStatus_ERR(
                    "failed to initialize  global cross-interpreter state");
        }
    }

    _PyXI_state_t *state = _PyXI_GET_STATE(interp);
    if (state == NULL) {
        PyErr_PrintEx(0);
        return _PyStatus_ERR(
                "failed to get interpreter's cross-interpreter state");
    }
    // The static types were already initialized in _PyXI_InitTypes(),
    // so we pass in NULL here to avoid initializing them again.
    if (_Py_xi_state_init(state, NULL) < 0) {
        PyErr_PrintEx(0);
        return _PyStatus_ERR(
                "failed to initialize interpreter's cross-interpreter state");
    }

    if (_xidwrapper_register_type(interp) < 0) {
        return _PyStatus_ERR(
                "failed to register the cross-interpreter wrapper type");
    }

    return _PyStatus_OK();
}

// _PyXI_Fini() must be called before the interpreter is cleared,
// since we must clear some heap objects.

void
_PyXI_Fini(PyInterpreterState *interp)
{
    _PyXI_state_t *state = _PyXI_GET_STATE(interp);
#ifndef NDEBUG
    if (state == NULL) {
        PyErr_PrintEx(0);
        return;
    }
#endif
    // The static types will be finalized soon in _PyXI_FiniTypes(),
    // so we pass in NULL here to avoid finalizing them right now.
    _Py_xi_state_fini(state, NULL);

    if (_Py_IsMainInterpreter(interp)) {
        _PyXI_global_state_t *global_state = _PyXI_GET_GLOBAL_STATE(interp);
        _Py_xi_global_state_fini(global_state);
    }
}

PyStatus
_PyXI_InitTypes(PyInterpreterState *interp)
{
    if (init_static_exctypes(&_PyXI_GET_STATE(interp)->exceptions, interp) < 0) {
        PyErr_PrintEx(0);
        return _PyStatus_ERR(
                "failed to initialize the cross-interpreter exception types");
    }
    if (_xidwrapper_init_type(interp) < 0) {
        return _PyStatus_ERR(
                "failed to initialize the cross-interpreter wrapper type");
    }
    // We would initialize heap types here too but that leads to ref leaks.
    // Instead, we intialize them in _PyXI_Init().
    return _PyStatus_OK();
}

void
_PyXI_FiniTypes(PyInterpreterState *interp)
{
    // We would finalize heap types here too but that leads to ref leaks.
    // Instead, we finalize them in _PyXI_Fini().
    _xidwrapper_fini_type(interp);
    fini_static_exctypes(&_PyXI_GET_STATE(interp)->exceptions, interp);
}


/*************/
/* other API */
/*************/

PyInterpreterState *
_PyXI_NewInterpreter(PyInterpreterConfig *config, long *maybe_whence,
                     PyThreadState **p_tstate, PyThreadState **p_save_tstate)
{
    PyThreadState *save_tstate = PyThreadState_Swap(NULL);
    assert(save_tstate != NULL);

    PyThreadState *tstate;
    PyStatus status = Py_NewInterpreterFromConfig(&tstate, config);
    if (PyStatus_Exception(status)) {
        // Since no new thread state was created, there is no exception
        // to propagate; raise a fresh one after swapping back in the
        // old thread state.
        PyThreadState_Swap(save_tstate);
        _PyErr_SetFromPyStatus(status);
        PyObject *exc = PyErr_GetRaisedException();
        PyErr_SetString(PyExc_InterpreterError,
                        "sub-interpreter creation failed");
        _PyErr_ChainExceptions1(exc);
        return NULL;
    }
    assert(tstate != NULL);
    PyInterpreterState *interp = PyThreadState_GetInterpreter(tstate);

    long whence = _PyInterpreterState_WHENCE_XI;
    if (maybe_whence != NULL) {
        whence = *maybe_whence;
    }
    _PyInterpreterState_SetWhence(interp, whence);

    if (p_tstate != NULL) {
        // We leave the new thread state as the current one.
        *p_tstate = tstate;
    }
    else {
        // Throw away the initial tstate.
        PyThreadState_Clear(tstate);
        PyThreadState_Swap(save_tstate);
        PyThreadState_Delete(tstate);
        save_tstate = NULL;
    }
    if (p_save_tstate != NULL) {
        *p_save_tstate = save_tstate;
    }
    return interp;
}

void
_PyXI_EndInterpreter(PyInterpreterState *interp,
                     PyThreadState *tstate, PyThreadState **p_save_tstate)
{
#ifndef NDEBUG
    long whence = _PyInterpreterState_GetWhence(interp);
#endif
    assert(whence != _PyInterpreterState_WHENCE_RUNTIME);

    if (!_PyInterpreterState_IsReady(interp)) {
        assert(whence == _PyInterpreterState_WHENCE_UNKNOWN);
        // PyInterpreterState_Clear() requires the GIL,
        // which a not-ready does not have, so we don't clear it.
        // That means there may be leaks here until clearing the
        // interpreter is fixed.
        PyInterpreterState_Delete(interp);
        return;
    }
    assert(whence != _PyInterpreterState_WHENCE_UNKNOWN);

    PyThreadState *save_tstate = NULL;
    PyThreadState *cur_tstate = PyThreadState_GET();
    if (tstate == NULL) {
        if (PyThreadState_GetInterpreter(cur_tstate) == interp) {
            tstate = cur_tstate;
        }
        else {
            tstate = _PyThreadState_NewBound(interp, _PyThreadState_WHENCE_FINI);
            assert(tstate != NULL);
            save_tstate = PyThreadState_Swap(tstate);
        }
    }
    else {
        assert(PyThreadState_GetInterpreter(tstate) == interp);
        if (tstate != cur_tstate) {
            assert(PyThreadState_GetInterpreter(cur_tstate) != interp);
            save_tstate = PyThreadState_Swap(tstate);
        }
    }

    Py_EndInterpreter(tstate);

    if (p_save_tstate != NULL) {
        save_tstate = *p_save_tstate;
    }
    PyThreadState_Swap(save_tstate);
}
