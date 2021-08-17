#ifndef Py_INTERNAL_THREAD_H
#define Py_INTERNAL_THREAD_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#ifndef _POSIX_THREADS
/* This means pthreads are not implemented in libc headers, hence the macro
   not present in unistd.h. But they still can be implemented as an external
   library (e.g. gnu pth in pthread emulation) */
# ifdef HAVE_PTHREAD_H
#  include <pthread.h> /* _POSIX_THREADS */
# endif
/* Check if we're running on HP-UX and _SC_THREADS is defined. If so, then
   enough of the Posix threads package is implemented to support python
   threads.

   This is valid for HP-UX 11.23 running on an ia64 system. If needed, add
   a check of __ia64 to verify that we're running on an ia64 system instead
   of a pa-risc system.
*/
# ifndef _POSIX_THREADS
#  ifdef __hpux
#   ifdef _SC_THREADS
#    define _POSIX_THREADS
#   endif
#  endif
# endif
#endif


/** poxix threads ********************/
#ifdef _POSIX_THREADS

# if defined(__APPLE__) || defined(HAVE_PTHREAD_DESTRUCTOR)
#  define destructor xxdestructor
# endif
# include <pthread.h>
# if defined(__APPLE__) || defined(HAVE_PTHREAD_DESTRUCTOR)
#  undef destructor
# endif

/* The POSIX spec says that implementations supporting the sem_*
   family of functions must indicate this by defining
   _POSIX_SEMAPHORES. */
# ifdef _POSIX_SEMAPHORES
/* On FreeBSD 4.x, _POSIX_SEMAPHORES is defined empty, so
   we need to add 0 to make it work there as well. */
#  if (_POSIX_SEMAPHORES+0) == -1
#   define HAVE_BROKEN_POSIX_SEMAPHORES
#  else
#   include <semaphore.h>
#   include <errno.h>
#  endif
# endif

/* Whether or not to use semaphores directly rather than emulating them with
 * mutexes and condition variables:
 */
# if (defined(_POSIX_SEMAPHORES) && !defined(HAVE_BROKEN_POSIX_SEMAPHORES) && \
      defined(HAVE_SEM_TIMEDWAIT))
#  define USE_SEMAPHORES
# else
#  undef USE_SEMAPHORES
# endif

/* locks */
# ifdef USE_SEMAPHORES
typedef sem_t _PyThread_type_lock;

# else
/* A pthread mutex isn't sufficient to model the Python lock type
 * because, according to Draft 5 of the docs (P1003.4a/D5), both of the
 * following are undefined:
 *  -> a thread tries to lock a mutex it already has locked
 *  -> a thread tries to unlock a mutex locked by a different thread
 * pthread mutexes are designed for serializing threads over short pieces
 * of code anyway, so wouldn't be an appropriate implementation of
 * Python's locks regardless.
 *
 * The pthread_lock struct implements a Python lock as a "locked?" bit
 * and a <condition, mutex> pair.  In general, if the bit can be acquired
 * instantly, it is, else the pair is used to block the thread until the
 * bit is cleared.     9 May 1994 tim@ksr.com
 */
typedef struct {
    char             locked; /* 0=unlocked, 1=locked */
    /* a <cond, mutex> pair to handle an acquire of a locked lock */
    pthread_cond_t   lock_released;
    pthread_mutex_t  mut;
} pthread_lock;
typedef pthread_lock _PyThread_type_lock;

# endif  // !USE_SEMAPHORES


/** nt threads ********************/
#elif defined(NT_THREADS)

/* options */
# ifndef _PY_USE_CV_LOCKS
#  define _PY_USE_CV_LOCKS 1     /* use locks based on cond vars */
# endif

# if _PY_USE_CV_LOCKS
#  include "condvar.h"
struct _NRMUTEX {
    PyMUTEX_T cs;
    PyCOND_T cv;
    int locked;
};
typedef struct _NRMUTEX _PyThread_type_lock;

# else
struct _NRMUTEX {
    HANDLE handle;
};
typedef struct _NRMUTEX _PyThread_type_lock;

# endif


/** other threads ********************/
#else
# error "Require native threads. See https://bugs.python.org/issue31370"
#endif /* ! _POSIX_THREADS && ! NT_THREADS */


PyAPI_FUNC(int) _PyThread_init_lock(PyThread_type_lock *);
PyAPI_FUNC(void) _PyThread_clear_lock(PyThread_type_lock);

#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_THREAD_H */
