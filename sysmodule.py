# grep "import \(.*, \)\?sys\(,\|$\)\|from sys import \|'sys'" **/*.py

"""
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

"""
__displayhook__
__doc__
__excepthook__
__interactivehook__
__loader__
__name__
__package__
__spec__
__stderr__
__stdin__
__stdout__
_clear_type_cache
_current_frames
_debugmallocstats
_getframe
_home
_mercurial
_xoptions
abiflags
api_version
argv
base_exec_prefix
base_prefix
builtin_module_names
byteorder
call_tracing
callstats
copyright
displayhook
dont_write_bytecode
exc_info
excepthook
exec_prefix
executable
exit
flags
float_info
float_repr_style
get_coroutine_wrapper
getallocatedblocks
getcheckinterval
getdefaultencoding
getdlopenflags
getfilesystemencoding
getprofile
getrecursionlimit
getrefcount
getsizeof
getswitchinterval
gettrace
hash_info
hexversion
implementation
int_info
intern
is_finalizing
maxsize
maxunicode
meta_path
modules
path
path_hooks
path_importer_cache
platform
prefix
set_coroutine_wrapper
setcheckinterval
setdlopenflags
setprofile
setrecursionlimit
setswitchinterval
settrace
stderr
stdin
stdout
thread_info
version
version_info
warnoptions
"""

def _install():
    _sys.__name__ = '_sys'
    _sys.modules['_sys'] = _sys
    _sys.modules['sys'] = sys


try:
    import _sys
except ImportError:
    import sys as _sys
    install = _install
else:
    def install(*, force=False):
        if force:
            _install()
        else:
            raise RuntimeError('sysmodule already installed')


ModuleType = type(_sys)


SYS_DOC = _sys.__doc__
#SYS_DOC = """\
#"""


class SysModuleMeta(type(type(_sys))):
    """Support for backward-compatibility.
    
    Using type(sys) to get the normal module type is a common idiom.
    Thus SysModule must accommodate such existing usage.  Note that
    "type(some_module) is type(sys)" checks will fail.
    """

    def __call__(cls, name, *args, **kwargs):
        if name != 'sys':
            return type(ModuleType).__call__(ModuleType, name, *args, **kwargs)
        return super().__call__(name, *args, **kwargs)

    def __instancecheck__(cls, instance):
        if type(ModuleType).__instancecheck__(ModuleType, instance):
            return True
        return super().__instancecheck__(instance)

    def __subclasscheck__(cls, subclass):
        if type(ModuleType).__subclasscheck__(ModuleType, subclass):
            return True
        return super().__subclasscheck__(subclass)

    def __eq__(cls, other):
        if type(ModuleType).__eq__(ModuleType, other):
            return True
        return super().__eq__(other)


class SysModule(ModuleType, metaclass=SysModuleMeta):
    """A wrapper around the _sys module."""

    # XXX handle reload

    # XXX singleton?

#    def __new__(cls, name):
#        if name != 'sys':  # for backward-compatibility
#            return type(_sys)(name)
#        return type(_sys).__new__(cls, name)

    def __init__(self, name):
        super().__init__(name)

        # Set a custom loader?
        loader = __loader__
        #origin = __spec__.origin
        origin = '{}.{}'.format(__name__, type(self).__name__)
        loader_state = __spec__.loader_state
        spec = type(__spec__)(name, loader,
                              origin=origin,
                              loader_state=loader_state,
                              is_package=False,
                              )

        self.__dict__['__doc__'] = SYS_DOC
        self.__dict__['__loader__'] = loader
        self.__dict__['__package__'] = ''
        self.__dict__['__spec__'] = spec

        #self.__dict__['__builtins__'] = __builtins__
        #self.__dict__['__cached__'] = __cached__
        #self.__dict__['__file__'] = __file__

    def __getattr__(self, name):
        return getattr(_sys, name)

    def __setattr__(self, name, value):
        if hasattr(_sys, name):
            setattr(_sys, name, value)
            return
        raise AttributeError('the sys module is a read-only proxy around _sys')

    def __delattr__(self, name, value):
        if hasattr(_sys, name):
            delattr(_sys, name)
            return
        raise AttributeError('the sys module is a read-only proxy around _sys')


sys = SysModule('sys')


#########################
# tests

print('-----------')
spam = SysModule('spam')
print(spam)
print(type(spam))
print('instance check:', isinstance(spam, SysModule))
print('subclass check:', issubclass(type(spam), SysModule))
print('type == check:', (type(spam) == SysModule))
print('class == check:', (spam.__class__ == SysModule))

print('-----------')
print(sys)
print(type(sys))
print('instance check:', isinstance(sys, SysModule))
print('subclass check:', issubclass(type(sys), SysModule))
print('instance check:', isinstance(sys, type(_sys)))
print('subclass check:', issubclass(type(sys), type(_sys)))
print('type == check:', (type(sys) == SysModule))
print('class == check:', (sys.__class__ == SysModule))
print('type == check:', (type(sys) == type(_sys)))
print('class == check:', (sys.__class__ == _sys.__class__))

print('-----------')
