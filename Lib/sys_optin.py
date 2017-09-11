
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
getdlopenflags() -- returns flags to be used for dlopen() calls
getprofile() -- get the global profiling function
getrefcount() -- return the reference count for an object (plus one :-)
getrecursionlimit() -- return the max recursion depth for the interpreter
getsizeof() -- return the size of an object in bytes
gettrace() -- get the global debug tracing function
setcheckinterval() -- control how often the interpreter checks for events
setdlopenflags() -- set the flags to be used for dlopen() calls
setprofile() -- set the global profiling function
setrecursionlimit() -- set the max recursion depth for the interpreter
settrace() -- set the global debug tracing function
"""

WIN_DOC = ""
if hasattr(_sys, 'dllhandle'):
    WIN_DOC = """
dllhandle -- [Windows only] integer handle of the Python DLL
winver -- [Windows only] version number of the Python DLL
"""


#LEGACY_ATTRS = (
#        # These are handled via read-only properties:
#        "getdefaultencoding",
#        "getfilesystemencoding",
#        "getwindowsversion",
#
#        # These are handled via R/W properties.
#        "getcheckinterval",
#        "setcheckinterval",
#        "getdlopenflags",
#        "setdlopenflags",
#        "getprofile",
#        "setprofile",
#        "getrecursionlimit",
#        "setrecursionlimit",
#        "getswitchinterval",
#        "setswitchinterval",
#        "gettrace",
#        "settrace",
#        "getcoroutine_wrapper",
#        "setcoroutine_wrapper",
#
#        # The import-related attrs are handled separately.
#        )

PRIVATE_ATTRS = (
        "__displayhook__",
        "__excepthook__",
        "__interactivehook__",
        "__stdin__",
        "__stdout__",
        "__stderr__",
        "_xoptions",  # implementation-specific
        )

STATIC_ATTRS = (
        "__displayhook__",
        "__excepthook__",
        "__interactivehook__",
        "__stdin__",
        "__stdout__",
        "__stderr__",
        "abiflags",
        "base_exec_prefix",
        "base_prefix",
        "byteorder",
        "copyright",
        "dllhandle",  # windows-only
        "exec_prefix",
        "executable",
        "flags",
        "float_info",
        "hash_info",
        "hexversion",
        "implementation",
        "int_info",
        "maxsize",
        "maxunicode",
        "platform",
        "prefix",
        "thread_info",
        "tracebacklimit",
        "version",
        "api_version",
        "version_info",
        "warnoptions",
        "winver",  # windows-only
        "_xoptions",  # implementation-specific
        )

RO_ATTRS = (
        "argv",
        "last_type",
        "last_value",
        "last_traceback",
        )

RO_PROPERTIES = (
        "defaultencoding",
        "filesystemencoding",
        "windowsversion",  # windows-only
        )
#if hasattr(_sys, "getwindowsversion"):
#    RO_PROPERTIES += (
#        "windowsversion",
#        )



#OPTIONAL = (
#        # Windows-only
#        "dllhandle",
#        "winver",
#        # implementation-specific
#        "_xoptions",
#        # ifdef Py_TRACE_REFS
#        "getobjects",
#        # ifdef Py_REF_DEBUG
#        "gettotalrefcount",
#        # ifdef COUNT_ALLOCS
#        "getcounts",
#        # ifdef DYNAMIC_EXECUTION_PROFILE
#        "getdxp",
#        # ifdef USE_MALLOPT
#        "mdebug",
#        )

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

RW_PROPERTIES = (
        "checkinterval",
        "profile",
        "recursionlimit",
        "switchinterval",
        "trace",
        "coroutine_wrapper",
        )
if hasattr(_sys, "getdlopenflags"):
    RW_PROPERTIES += (
        "dlopenflags",
        )

IMPORT_ATTRS = (
        "builtin_module_names",
        "dont_write_bytecode",
        "meta_path",
        "modules",
        "path",
        "path_hooks",
        "path_importer_cache",
        )

#SYS_ATTRS = (tuple(a for a in dir(_sys)
#                   if not a.startswith('_') and a not in LEGACY_ATTRS) +
#             tuple(a for a in PRIVATE_ATTRS if hasattr(_sys, a)))
#
#ALL_ATTRS = (SYS_ATTRS +
#             MODULE_ATTRS +
#             tuple(a for a in RO_PROPERTIES if hasattr(_sys, 'get'+a)) +
#             tuple(a for a in RW_PROPERTIES if hasattr(_sys, 'get'+a)))

ALL_ATTRS = (tuple(a for a in dir(_sys) if not a.startswith('_')) +
             PRIVATE_ATTRS +
             tuple(a for a in RO_PROPERTIES if hasattr(_sys, 'get'+a)) +
             tuple(a for a in RW_PROPERTIES if hasattr(_sys, 'get'+a)) +
             tuple(a for a in RW_PROPERTIES if hasattr(_sys, 'set'+a))
             )

#ALL_ATTRS = (tuple(a for a in STATIC_ATTRS if hasattr(_sys, a)) +
#             tuple(a for a in RO_ATTRS if hasattr(_sys, a)) +
#             tuple(a for a in RO_PROPERTIES if hasattr(_sys, 'get'+a)) +
#
#             tuple(a for a in RW_ATTRS if hasattr(_sys, a)) +
#             tuple(a for a in RW_PROPERTIES if hasattr(_sys, 'get'+a)) +
#             tuple(a for a in RW_PROPERTIES if hasattr(_sys, 'set'+a)) +
#             )

LEGACY_ATTRS = (tuple('get' + a for a in RO_PROPERTIES) +
                tuple('get' + a for a in RW_PROPERTIES) +
                tuple('set' + a for a in RW_PROPERTIES))

FUNCS = tuple(name for name, attr in vars(_sys).items()
              if not name.startswith('_') and type(attr) is type(_sys.exit))

PRIVATE_FUNCS = (
        "_clear_type_cache",
        "_current_frames",
        "_debugmallocstats",
        "_getframe",
        )

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

    #__slots__ = ()

    def __dir__(self):
        # XXX Include MODULE_ATTRS?
        # XXX Exclude the import-related attrs?
        (a in STATIC_ATTRS + RO_ATTRS + RW_ATTRS)
        return sorted(STATIC_ATTRS +
                      RO_ATTRS +
                      RO_PROPERTIES +
                      RW_ATTRS + 
                      RW_PROPERTIES +
                      IMPORT_ATTRS +
                      FUNCS +
                      PRIVATE_FUNCS +
                      MODULE_ATTRS)
#                      MODULE_ATTRS +
#                      tuple(k for k in dir(type(self))
#                            if not k.startswith('__') or k[2:-2] in RW_ATTRS))

    def __getattr__(self, name):
        if name in LEGACY_ATTRS:
            alt = name[3:]  # They are all "get*" or "set*".
            warn_deprecated(name+'()', alt)
        return getattr(_sys, name)

    def __setattr__(self, name, value):
        if name in RO_ATTRS:
            raise AttributeError("{} is read-only".format(name))
        if name not in RW_ATTRS:
            warn_deprecated(name, action='change')
        return setattr(_sys, name, value)

    def __delattr__(self, name):
        if name in RO_ATTRS:
            raise AttributeError("{} is read-only".format(name))
        if name not in RW_ATTRS:
            warn_deprecated(name, action='deletion')
        return delattr(_sys, name)

    # R/O properties

#    @property
#    def __displayhook__(self):
#        return _sys.__displayhook__
#
#    @property
#    def __excepthook__(self):
#        return _sys.__excepthook__
#
#    @property
#    def __interactivehook__(self):
#        return _sys.__interactivehook__
#
#    @property
#    def __stdin__(self):
#        return _sys.__stdin__
#
#    @property
#    def __stdout__(self):
#        return _sys.__stdout__
#
#    @property
#    def __stderr__(self):
#        return _sys.__stderr__

#    @property
#    def allocatedblocks(self):
#        return _sys.getallocatedblocks()

#    @property
#    def defaultencoding(self):
#        return _sys.getdefaultencoding()
#
#    @property
#    def filesystemencoding(self):
#        return _sys.getfilesystemencoding()
#
#    if hasattr(_sys, 'getwindowsversion'):
#        @property
#        def windowsversion(self):
#            return _sys.getwindowsversion()
#
#    # R/W properties
#
#    @property
#    def checkinterval(self):
#        return _sys.getcheckinterval()
#
#    @checkinterval.setter
#    def checkinterval(self, value):
#        _sys.setcheckinterval(value)
#
#    if hasattr(_sys, 'getdlopenflags'):
#        @property
#        def dlopenflags(self):
#            return _sys.getdlopenflags()
#
#        @dlopenflags.setter
#        def dlopenflags(self, value):
#            _sys.setdlopenflags(value)
#
#    @property
#    def profile(self):
#        return _sys.getprofile()
#
#    @profile.setter
#    def profile(self, value):
#        _sys.setprofile(value)
#
#    @property
#    def recursionlimit(self):
#        return _sys.getrecursionlimit()
#
#    @recursionlimit.setter
#    def recursionlimit(self, value):
#        _sys.setrecursionlimit(value)
#
#    if hasattr(_sys, 'getswitchinterval'):
#        @property
#        def switchinterval(self):
#            return _sys.getswitchinterval()
#
#        @switchinterval.setter
#        def switchinterval(self, value):
#            _sys.setswitchinterval(value)
#
#    @property
#    def trace(self):
#        return _sys.gettrace()
#
#    @trace.setter
#    def trace(self, value):
#        _sys.settrace(value)
#
#    @property
#    def coroutine_wrapper(self):
#        return _sys.getcoroutine_wrapper()
#
#    @coroutine_wrapper.setter
#    def coroutine_wrapper(self, value):
#        _sys.setcoroutine_wrapper(value)
#
#    # import system
#    
#    @property
#    def builtin_module_names(self):
#        warn_deprecated('builtin_module_names', 'imports')
#        #msg = ('direct use of sys.{} is deprecated; use sys.imports instead'
#        #       ).format(name)
#        #warnings.warn(msg, DeprecationWarning, stacklevel=2)
#        return _sys.builtin_module_names
#
#    @property
#    def dont_write_bytecode(self):
#        warn_deprecated('dont_write_bytecode', 'imports')
#        return _sys.dont_write_bytecode
#
#    @property
#    def meta_path(self):
#        warn_deprecated('meta_path', 'imports')
#        return _sys.meta_path
#
#    @property
#    def modules(self):
#        warn_deprecated('modules', 'imports')
#        return _sys.modules
#
#    @property
#    def path(self):
#        warn_deprecated('path', 'imports')
#        return _sys.path
#
#    @property
#    def path_hooks(self):
#        warn_deprecated('path_hooks', 'imports')
#        return _sys.path_hooks
#
#    @property
#    def path_importer_cache(self):
#        warn_deprecated('path_importer_cache', 'imports')
#        return _sys.path_importer_cache
#
#    # functions
#
#    call_tracing = staticmethod(_sys.call_tracing)
#    callstats = staticmethod(_sys.callstats)
#    _clear_type_cache = staticmethod(_sys._clear_type_cache)
#    _current_frames = staticmethod(_sys._current_frames)
#    _debugmallocstats = staticmethod(_sys._debugmallocstats)
#    exc_info = staticmethod(_sys.exc_info)
#    exit = staticmethod(_sys.exit)
#    getallocatedblocks = staticmethod(_sys.getallocatedblocks)
#    getrefcount = staticmethod(_sys.getrefcount)
#    getsizeof = staticmethod(_sys.getsizeof)
#    _getframe = staticmethod(_sys._getframe)
#    if hasattr(_sys, 'settscdump'):
#        settscdump = staticmethod(_sys.settscdump)
#    intern = staticmethod(_sys.intern)
#    is_finalizing = staticmethod(_sys.is_finalizing)

#func_type = type(_sys.exit)
#for name in SYS_ATTRS:
#    attr = getattr(_sys, name)
##for name, attr in vars(_sys).items():
##    if name.startswith('_') or name not in PRIVATE_ATTRS:
##        continue
#    if type(attr) is func_type:
#        setattr(SysModule, name, staticmethod(attr))
#    elif name in RW_ATTRS:
#        def fget(name):
#            return getattr(_sys, name)
#        def fset(name, value):
#            setattr(_sys, name, value)
#        setattr(SysModule, name, property(fget=fget, fset=fset))
#    else:
#        def fget(name):
#            return getattr(_sys, name)
#        setattr(SysModule, name, property(fget=fget))

for name in RO_PROPERTIES:
    def fget(name):
        return getattr(_sys, 'get' + name)
    setattr(SysModule, name, property(fget=fget))

for name in RW_PROPERTIES:
    def fget(name):
        return getattr(_sys, 'get' + name)()
    def fset(name, value):
        getattr(_sys, 'set' + name)(value)
    setattr(SysModule, name, property(fget=fget, fset=fset))

for name in IMPORT_ATTRS:
    def fget(name):
        warn_deprecated(name, 'imports')
        return getattr(_sys, 'get' + name)
    # XXX Make them read-only.
    #setattr(SysModule, name, property(fget=fget))
    def fset(name, value):
        warn_deprecated(name, 'imports')
        getattr(_sys, 'set'+name)(value)
    setattr(SysModule, name, property(fget=fget, fset=fset))

for name in FUNCS + PRIVATE_FUNCS:
    setattr(SysModule, name, staticmethod(getattr(_sys, name)))


# Create the module object and replace the current one in sys.modules.
sys = SysModule(__name__)
#special = [(k, v)
#           for k, v in vars().items()
#           if k.startswith('__')]
ns = vars()
for name in MODULE_ATTRS:
    setattr(sys, name, ns[name])
#sys.__doc__ = SYS_DOC.format(windows=WIN_DOC)
sys.__doc__ = _sys.__doc__
#sys.__file__ = __file__
#sys.__cached__ = __cached__
#sys.__loader__ = __loader__
#sys.__spec__ = __spec__
#sys.__package__ = __package__
#sys.__builtins__ = __builtins__
_sys.modules[__name__] = sys
print(dir())
