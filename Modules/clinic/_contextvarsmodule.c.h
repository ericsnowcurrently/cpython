/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"            // PyGC_Head
#  include "pycore_runtime.h"       // _Py_ID()
#endif  // Py_BUILD_CORE && !Py_BUILD_CORE_MODULE


PyDoc_STRVAR(_contextvars_copy_context__doc__,
"copy_context($module, /)\n"
"--\n"
"\n");

#define _CONTEXTVARS_COPY_CONTEXT_METHODDEF    \
    {"copy_context", (PyCFunction)_contextvars_copy_context, METH_NOARGS, _contextvars_copy_context__doc__},

static PyObject *
_contextvars_copy_context_impl(PyObject *module);

static PyObject *
_contextvars_copy_context(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return _contextvars_copy_context_impl(module);
}
/*[clinic end generated code: output=f77f142740f24e52 input=a9049054013a1b77]*/
