#ifndef Py_LIMITED_API
#ifndef PYIDENTIFIER_H
#define PYIDENTIFIER_H
#ifdef __cplusplus
extern "C" {
#endif


/********************* String Literals ****************************************/
/* This structure helps managing static strings. The basic usage goes like this:
   Instead of doing

       r = PyObject_CallMethod(o, "foo", "args", ...);

   do

       _Py_IDENTIFIER(foo);
       ...
       r = _PyObject_CallMethodId(o, &PyId_foo, "args", ...);

   PyId_foo is a static variable, either on block level or file level. On first
   usage, the string "foo" is interned, and the structures are linked. On interpreter
   shutdown, all strings are released.

   Alternatively, _Py_static_string allows choosing the variable name.
   _PyUnicode_FromId returns a borrowed reference to the interned string.
   _PyObject_{Get,Set,Has}AttrId are __getattr__ versions using _Py_Identifier*.
*/
typedef struct _Py_Identifier {
    PyASCIIObject _ascii;
    const uint8_t string[256];  // an arbitrary size; must be big enough
} _Py_Identifier;
#define _Py_static_string_decl(varname, value) \
    struct _Py_Identifier_##varname { \
        PyASCIIObject _ascii; \
        const uint8_t string[sizeof(value)]; \
    } PyId_##varname

#define _Py_static_string_init(value) \
    { \
        ._ascii = { \
            .ob_base = _PyObject_IMMORTAL_INIT(&PyUnicode_Type), \
            .length = sizeof(value) - 1, \
            .hash = -1, \
            .state = { \
                .kind = 1, \
                .compact = 1, \
                .ascii = 1, \
                .ready = 1, \
            }, \
        }, \
        .string = value, \
    }
#define _Py_static_string(varname, value) \
    static _Py_static_string_decl(varname, value) = _Py_static_string_init(value)
#define _Py_IDENTIFIER(varname) _Py_static_string(varname, #varname)

#define _Py_ID(varname) PyId_##varname


/* unicodeobject.h */

/* Return an interned Unicode object for an Identifier; may fail if there is no memory.*/
PyAPI_FUNC(PyObject*) _PyUnicode_FromId(_Py_Identifier*);

/* Test whether a unicode is equal to ASCII identifier.  Return 1 if true,
   0 otherwise.  The right argument must be ASCII identifier.
   Any error occurs inside will be cleared before return. */
PyAPI_FUNC(int) _PyUnicode_EqualToASCIIId(
    PyObject *left,             /* Left string */
    _Py_Identifier *right       /* Right identifier */
    );

/* object.h */

PyAPI_FUNC(PyObject *) _PyType_LookupId(PyTypeObject *, _Py_Identifier *);
PyAPI_FUNC(PyObject *) _PyObject_LookupSpecial(PyObject *, _Py_Identifier *);

PyAPI_FUNC(int) _PyObject_LookupAttrId(PyObject *, struct _Py_Identifier *, PyObject **);
PyAPI_FUNC(PyObject *) _PyObject_GetAttrId(PyObject *, _Py_Identifier *);
PyAPI_FUNC(int) _PyObject_SetAttrId(PyObject *, _Py_Identifier *, PyObject *);

/* abstract.h */

/* Like PyObject_CallMethod(), but expect a _Py_Identifier*
   as the method name. */
PyAPI_FUNC(PyObject *) _PyObject_CallMethodId(PyObject *obj,
                                              _Py_Identifier *name,
                                              const char *format, ...);

PyAPI_FUNC(PyObject *) _PyObject_CallMethodId_SizeT(PyObject *obj,
                                                    _Py_Identifier *name,
                                                    const char *format,
                                                    ...);

#ifdef PY_SSIZE_T_CLEAN
#  define _PyObject_CallMethodId _PyObject_CallMethodId_SizeT
#endif

PyAPI_FUNC(PyObject *) _PyObject_CallMethodIdObjArgs(
    PyObject *obj,
    struct _Py_Identifier *name,
    ...);

static inline PyObject *
_PyObject_VectorcallMethodId(
    _Py_Identifier *name, PyObject *const *args,
    size_t nargsf, PyObject *kwnames)
{
    PyObject *oname = _PyUnicode_FromId(name); /* borrowed */
    if (!oname) {
        return NULL;
    }
    return PyObject_VectorcallMethod(oname, args, nargsf, kwnames);
}

static inline PyObject *
_PyObject_CallMethodIdNoArgs(PyObject *self, _Py_Identifier *name)
{
    return _PyObject_VectorcallMethodId(name, &self,
           1 | PY_VECTORCALL_ARGUMENTS_OFFSET, NULL);
}

static inline PyObject *
_PyObject_CallMethodIdOneArg(PyObject *self, _Py_Identifier *name, PyObject *arg)
{
    PyObject *args[2] = {self, arg};

    assert(arg != NULL);
    return _PyObject_VectorcallMethodId(name, args,
           2 | PY_VECTORCALL_ARGUMENTS_OFFSET, NULL);
}

/* dictobject.h */

PyAPI_FUNC(PyObject *) _PyDict_GetItemIdWithError(PyObject *dp,
                                                  struct _Py_Identifier *key);
PyAPI_FUNC(int) _PyDict_ContainsId(PyObject *, struct _Py_Identifier *);
PyAPI_FUNC(int) _PyDict_SetItemId(PyObject *dp, struct _Py_Identifier *key, PyObject *item);

PyAPI_FUNC(int) _PyDict_DelItemId(PyObject *mp, struct _Py_Identifier *key);

/* ceval.h */

/* Helper to look up a builtin object */
PyAPI_FUNC(PyObject *) _PyEval_GetBuiltinId(_Py_Identifier *);

/* import.h */

PyAPI_FUNC(PyObject *) _PyImport_GetModuleId(struct _Py_Identifier *name);

/* sysmodule.h */

PyAPI_FUNC(PyObject *) _PySys_GetObjectId(_Py_Identifier *key);
PyAPI_FUNC(int) _PySys_SetObjectId(_Py_Identifier *key, PyObject *);


#ifdef __cplusplus
}
#endif
#endif /* !PYIDENTIFIER*/
#endif /* !Py_LIMITED_API */
