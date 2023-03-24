#ifndef Py_INTERNAL_GLOBAL_OBJECTS_H
#define Py_INTERNAL_GLOBAL_OBJECTS_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include "pycore_gc.h"              // PyGC_Head
#include "pycore_global_strings.h"  // struct _Py_global_strings
#include "pycore_hamt.h"            // PyHamtNode_Bitmap
#include "pycore_context.h"         // _PyContextTokenMissing
#include "pycore_typeobject.h"      // pytype_slotdef


// These would be in pycore_long.h if it weren't for an include cycle.
#define _PY_NSMALLPOSINTS           257
#define _PY_NSMALLNEGINTS           5


// Only immutable objects should be considered runtime-global.
// All others must be per-interpreter.

struct _Py_cached_objects {
    /* A thread state tied to the main interpreter,
       used exclusively for when a global object (e.g. interned strings)
       is resized (i.e. deallocated + allocated) from an arbitrary thread. */
    PyThreadState main_tstate;

    /* Sharing an object between interpreters is currently messy and
       there are few cases that justify the trouble.  The objects for
       those few cases are all stored here. */

    PyObject *interned_strings;
};

typedef enum {
    _Py_INTERNED_STRINGS_DICT,
} _Py_protected_global_object;

static inline PyObject **
_Py_find_protected_global_object(struct _Py_cached_objects *state,
                                 _Py_protected_global_object which)
{
    switch (which) {
    case _Py_INTERNED_STRINGS_DICT:
        return &state->interned_strings;
    }
    _Py_FatalErrorFunc(__func__, "unsupported global object");
}

static inline PyObject *
_Py_get_protected_global_object(struct _Py_cached_objects *state,
                                _Py_protected_global_object which)
{
    PyObject **ptr = _Py_find_protected_global_object(state, which);
    return *ptr;
}
#define _Py_get_protected_global_object(which) \
    _Py_get_protected_global_object(&_PyRuntime.cached_objects, which)

static inline void
_Py_set_protected_global_object(struct _Py_cached_objects *state,
                               _Py_protected_global_object which,
                               PyObject *obj)
{
    PyObject **ptr = _Py_find_protected_global_object(state, which);
    assert(*ptr == NULL);
    *ptr = obj;
}
#define _Py_set_protected_global_object(which, obj) \
    _Py_set_protected_global_object(&_PyRuntime.cached_objects, which, obj)

static inline void
_Py_clear_protected_global_object(struct _Py_cached_objects *state,
                                  _Py_protected_global_object which)
{
    PyObject **ptr = _Py_find_protected_global_object(state, which);
    assert(*ptr != NULL);
    *ptr = NULL;
}
#define _Py_clear_protected_global_object(which) \
    _Py_clear_protected_global_object(&_PyRuntime.cached_objects, which)


#define _Py_GLOBAL_OBJECT(NAME) \
    _PyRuntime.static_objects.NAME
#define _Py_SINGLETON(NAME) \
    _Py_GLOBAL_OBJECT(singletons.NAME)

struct _Py_static_objects {
    struct {
        /* Small integers are preallocated in this array so that they
         * can be shared.
         * The integers that are preallocated are those in the range
         * -_PY_NSMALLNEGINTS (inclusive) to _PY_NSMALLPOSINTS (exclusive).
         */
        PyLongObject small_ints[_PY_NSMALLNEGINTS + _PY_NSMALLPOSINTS];

        PyBytesObject bytes_empty;
        struct {
            PyBytesObject ob;
            char eos;
        } bytes_characters[256];

        struct _Py_global_strings strings;

        _PyGC_Head_UNUSED _tuple_empty_gc_not_used;
        PyTupleObject tuple_empty;

        _PyGC_Head_UNUSED _hamt_bitmap_node_empty_gc_not_used;
        PyHamtNode_Bitmap hamt_bitmap_node_empty;
        _PyContextTokenMissing context_token_missing;
    } singletons;
};

#define _Py_INTERP_CACHED_OBJECT(interp, NAME) \
    (interp)->cached_objects.NAME

struct _Py_interp_cached_objects {
    /* AST */
    PyObject *str_replace_inf;

    /* object.__reduce__ */
    PyObject *objreduce;
    PyObject *type_slots_pname;
    pytype_slotdef *type_slots_ptrs[MAX_EQUIV];

};

#define _Py_INTERP_STATIC_OBJECT(interp, NAME) \
    (interp)->static_objects.NAME
#define _Py_INTERP_SINGLETON(interp, NAME) \
    _Py_INTERP_STATIC_OBJECT(interp, singletons.NAME)

struct _Py_interp_static_objects {
    struct {
        int _not_used;
        // hamt_empty is here instead of global because of its weakreflist.
        _PyGC_Head_UNUSED _hamt_empty_gc_not_used;
        PyHamtObject hamt_empty;
        PyBaseExceptionObject last_resort_memory_error;
    } singletons;
};


#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_GLOBAL_OBJECTS_H */
