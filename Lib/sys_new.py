
import _sys


SYS_DOC = """
This module provides access to some objects used or maintained by the
interpreter and to functions that interact strongly with the interpreter.

Dynamic objects:

argv -- command line arguments; argv[0] is the script pathname if known
path -- module search path; path[0] is the script directory, else ''
modules -- dictionary of loaded modules

displayhook -- called to show results in an interactive session
excepthook -- called to handle any uncaught exception other than SystemExit
  To customize printing in an interactive session or to install a custom
  top-level exception handler, assign other functions to replace these.

stdin -- standard input file object; used by input()
stdout -- standard output file object; used by print()
stderr -- standard error object; used for error messages
  By assigning other file objects (or objects that behave like files)
  to these, it is possible to redirect all of the interpreter's I/O.

last_type -- type of last uncaught exception
last_value -- value of last uncaught exception
last_traceback -- traceback of last uncaught exception
  These three are only available in an interactive session after a
  traceback has been printed.

checkinterval -- how often the interpreter checks for events
profile -- the global profiling function
recursionlimit -- the max recursion depth for the interpreter
trace -- the global debug tracing function
{dlopen}
Static objects:

builtin_module_names -- tuple of module names built into this interpreter
copyright -- copyright notice pertaining to this interpreter
exec_prefix -- prefix used to find the machine-specific Python library
executable -- absolute path of the executable binary of the Python interpreter
float_info -- a struct sequence with information about the float implementation.
float_repr_style -- string indicating the style of repr() output for floats
hash_info -- a struct sequence with information about the hash algorithm.
hexversion -- version information encoded as a single integer
implementation -- Python implementation information.
int_info -- a struct sequence with information about the int implementation.
maxsize -- the largest supported length of containers.
maxunicode -- the value of the largest Unicode code point
platform -- platform identifier
prefix -- prefix used to find the Python library
thread_info -- a struct sequence with information about the thread implementation.
version -- the version of this interpreter as a string
version_info -- version information as a named tuple
{windows}
__stdin__ -- the original stdin; don't touch!
__stdout__ -- the original stdout; don't touch!
__stderr__ -- the original stderr; don't touch!
__displayhook__ -- the original displayhook; don't touch!
__excepthook__ -- the original excepthook; don't touch!

Functions:

displayhook() -- print an object to the screen, and save it in builtins._
excepthook() -- print an exception and its traceback to sys.stderr
exc_info() -- return thread-safe information about the current exception
exit() -- exit the interpreter by raising SystemExit
getrefcount() -- return the reference count for an object (plus one :-)
getsizeof() -- return the size of an object in bytes
"""

DLOPEN_DOC = ""
if hasattr(_sys, 'getdlopenflags'):
    DLOPEN_DOC = """\
dlopenflags -- the flags to be used for dlopen() calls
"""

WIN_DOC = ""
if hasattr(_sys, 'dllhandle'):
    WIN_DOC = """
dllhandle -- [Windows only] integer handle of the Python DLL
winver -- [Windows only] version number of the Python DLL
"""

__doc__ = SYS_DOC.format(dlopen=DLOPEN_DOC, windows=WIN_DOC)
#__doc__ = _sys.__doc__


PRIVATE_ATTRS = (
        "__displayhook__",
        "__excepthook__",
        "__interactivehook__",
        "__stdin__",
        "__stdout__",
        "__stderr__",
        "_xoptions",  # implementation-specific

        # functions
        "_clear_type_cache",
        "_current_frames",
        "_debugmallocstats",
        "_getframe",
        )

RO_PROPERTIES = {
        "getdefaultencoding": "defaultencoding",
        "getfilesystemencoding": "filesystemencoding",
        "getwindowsversion": "windowsversion",  # windows-only
        }

RW_ATTRS = (
        "displayhook",
        "excepthook",
        "float_repr_style",
        "ps1",
        "ps2",
        "stdin",
        "stdout",
        "stderr",
        )

RW_PROPERTIES = {
        "getcheckinterval": "checkinterval",
        "setcheckinterval": "checkinterval",
        "getprofile": "profile",
        "setprofile": "profile",
        "getrecursionlimit": "recursionlimit",
        "setrecursionlimit": "recursionlimit",
        "getswitchinterval": "switchinterval",
        "setswitchinterval": "switchinterval",
        "gettrace": "trace",
        "settrace": "trace",
        "get_coroutine_wrapper": "coroutine_wrapper",
        "set_coroutine_wrapper": "coroutine_wrapper",
        "getdlopenflags": "dlopenflags",  # implementation-specific
        "setdlopenflags": "dlopenflags",  # implementation-specific
        }

LEGACY_ATTRS = tuple(a for a in tuple(RO_PROPERTIES) + tuple(RW_PROPERTIES)
                     if a in dir(_sys))

MODULE_ATTRS = (
        "__name__",
        "__doc__",
        "__file__",
        "__cached__",
        "__loader__",
        "__spec__",
        "__package__",
        "__builtins__",
        )


def warn_deprecated(name, alt=None, action=None):
    import warnings
    if not action:
        action = 'use'
    msg = 'direct {} of sys.{} is deprecated'.format(action, name)
    if alt:
        msg += '; use sys.{} instead'.format(alt)
    warnings.warn(msg, DeprecationWarning, stacklevel=3)


class SysModule(type(_sys)):

    #__slots__ = MODULE_ATTRS

    def __init__(self, name, *, ns):
        super().__init__(name)

        for name in MODULE_ATTRS:
            object.__setattr__(self, name, ns[name])

    def __dir__(self):
        return sorted(tuple(a for a in dir(_sys)
                            if not a.startswith('_') and a not in LEGACY_ATTRS) +
                      tuple(a for a in PRIVATE_ATTRS if hasattr(_sys, a)) +
                      MODULE_ATTRS +  # XXX Exclude?
                      tuple(a for a in RO_PROPERTIES if hasattr(_sys, 'get'+a)) +
                      tuple(a for a in RW_PROPERTIES if hasattr(_sys, 'get'+a)))

    def __getattr__(self, name):
        if name in LEGACY_ATTRS:
            alt = name[3:]  # They are all "get*" or "set*".
            warn_deprecated(name+'()', alt)
        return getattr(_sys, name)

    def __setattr__(self, name, value):
        if name not in RW_ATTRS:
            # XXX Fail?
            warn_deprecated(name, action='change')
        return setattr(_sys, name, value)

    def __delattr__(self, name):
        if name not in RW_ATTRS:
            # XXX Fail?
            warn_deprecated(name, action='deletion')
        return delattr(_sys, name)


# Add the properties.
for sysname, name in dict(RO_PROPERTIES, **RW_PROPERTIES).items():
    prop = getattr(SysModule, name, property())
    if sysname.startswith('set'):
        prop = prop.setter(lambda v: getattr(_sys, sysname)(v))
    else:
        prop = prop.getter(lambda: getattr(_sys, sysname)())
    setattr(SysModule, name, prop)


# Create the module object and replace the current one in sys.modules.
sys = SysModule(__name__, ns=vars())
_sys.modules[__name__] = sys
