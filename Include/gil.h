#ifndef Py_GIL_H
#define Py_GIL_H

#ifdef Py_BUILD_CORE

#include "Python/condvar.h"
#ifndef Py_HAVE_CONDVAR
#error You need either a POSIX-compatible or a Windows system!
#endif


/* Enable if you want to force the switching of threads at least every `interval` */
#undef FORCE_SWITCHING
#define FORCE_SWITCHING


struct _gil_globals {
    /* microseconds (the Python API uses seconds, though) */
    unsigned long interval;
    /* Last PyThreadState holding / having held the GIL. This helps us know
       whether anyone else was scheduled after we dropped the GIL. */
    _Py_atomic_address last_holder;
    /* Whether the GIL is already taken (-1 if uninitialized). This is atomic
       because it can be read without any lock taken in ceval.c. */
    _Py_atomic_int locked;
    /* Number of GIL switches since the beginning. */
    unsigned long switch_number;
#ifdef WITH_THREAD
    /* This condition variable allows one or several threads to wait until
       the GIL is released. In addition, the mutex also protects the above
       variables. */
    PyCOND_T cond;
    PyMUTEX_T mutex;
#ifdef FORCE_SWITCHING
    /* This condition variable helps the GIL-releasing thread wait for
       a GIL-awaiting thread to be scheduled and take the GIL. */
    PyCOND_T switch_cond;
    PyMUTEX_T switch_mutex;
#endif
#endif /* WITH_THREAD */
};

#endif /* Py_BUILD_CORE */

#endif /* !Py_GIL_H */
