"""This module provides access to some objects used or maintained by the
interpreter and to functions that interact strongly with the interpreter.

This is a wrapper around the low-level _sys module.
"""

import _sys


MODULE_ATTRS = [
        "__name__",
        "__doc__",
        "__loader__",
        "__spec__",
        "__package__",
        # not in _sys:
        "__file__",
        "__cached__",
        "__builtins__",
        ]


# For now we strictly proxy _sys.

class SysModule(type(_sys)):

    # XXX What about cases of "moduletype = type(sys)"?

    def __init__(self, name, *, _ns=None):
        super().__init__(name)

        if _ns is not None:
            for attr in MODULE_ATTRS:
                object.__setattr__(self, name, _ns[attr])

    def __dir__(self):
        return sorted(dir(_sys) + MODULE_ATTRS)

    def __getattr__(self, name):
        return getattr(_sys, name)

    def __setattr__(self, name, value):
        # XXX Only wrap existing attrs?
        setattr(_sys, name, value)

    def __delattr__(self, name):
        # XXX Only wrap existing attrs?
        delattr(_sys, name)


_sys.modules[__name__] = SysModule(__name__, _ns=vars())
