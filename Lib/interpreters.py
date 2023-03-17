"""Multiple interpreters High Level Module."""

import _xxsubinterpreters as _interpreters

# aliases:
from _xxsubinterpreters import is_shareable


__all__ = [
    'Interpreter', 'get_current', 'get_main', 'create', 'list_all',
    'is_shareable',
    ]


def create(*, isolated=True):
    """Return a new (idle) Python interpreter."""
    id = _interpreters.create(isolated=isolated)
    return Interpreter(id, isolated=isolated)


def list_all():
    """Return all existing interpreters."""
    return [Interpreter(id) for id in _interpreters.list_all()]


def get_current():
    """Return the currently running interpreter."""
    id = _interpreters.get_current()
    return Interpreter(id)


def get_main():
    """Return the main interpreter."""
    id = _interpreters.get_main()
    return Interpreter(id)


class Interpreter:
    """A single Python interpreter."""

    def __init__(self, id, *, isolated=None):
        if not isinstance(id, (int, _interpreters.InterpreterID)):
            raise TypeError(f'id must be an int, got {id!r}')
        self._id = id
        self._isolated = isolated

    def __repr__(self):
        data = dict(id=int(self._id), isolated=self._isolated)
        kwargs = (f'{k}={v!r}' for k, v in data.items())
        return f'{type(self).__name__}({", ".join(kwargs)})'

    def __hash__(self):
        return hash(self._id)

    def __eq__(self, other):
        if not isinstance(other, Interpreter):
            return NotImplemented
        else:
            return other._id == self._id

    @property
    def id(self):
        return self._id

    @property
    def isolated(self):
        if self._isolated is None:
            # XXX The low-level function has not been added yet.
            # See bpo-....
            self._isolated = _interpreters.is_isolated(self._id)
        return self._isolated

    def is_running(self):
        """Return whether or not the identified interpreter is running."""
        return _interpreters.is_running(self._id)

    def close(self):
        """Finalize and destroy the interpreter.

        Attempting to destroy the current interpreter results
        in a RuntimeError.
        """
        return _interpreters.destroy(self._id)

    def run(self, src_str, /, *, channels=None):
        """Run the given source code in the interpreter.

        This blocks the current Python thread until done.
        """
        _interpreters.run_string(self._id, src_str, channels)
