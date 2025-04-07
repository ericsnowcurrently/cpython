"""Implements InterpreterPoolExecutor."""

import contextlib
import pickle
import sys
import textwrap
from . import thread as _thread
import _interpreters
import _interpreters_call as _ic
import _interpqueues


class ExecutionFailed(_interpreters.InterpreterError):
    """An unhandled exception happened during execution."""

    def __init__(self, excinfo):
        msg = excinfo.formatted
        if not msg:
            if excinfo.type and excinfo.msg:
                msg = f'{excinfo.type.__name__}: {excinfo.msg}'
            else:
                msg = excinfo.type.__name__ or excinfo.msg
        super().__init__(msg)
        self.excinfo = excinfo

    def __str__(self):
        try:
            formatted = self.excinfo.errdisplay
        except Exception:
            return super().__str__()
        else:
            return textwrap.dedent(f"""
{super().__str__()}

Uncaught in the interpreter:

{formatted}
                """.strip())


UNBOUND = 2  # error; this should not happen.


class WorkerContext(_thread.WorkerContext):

    # Switching to PEP 734's Interpreter object would simplify things.

    @classmethod
    def prepare(cls, initializer, initargs, shared):
        def resolve_task(fn, args, kwargs):
            # XXX Optionally serialize here?
            return (fn, args, kwargs)

        if initializer is not None:
            initdata = resolve_task(initializer, initargs, {})
        else:
            initdata = None
        def create_context():
            return cls(initdata, shared)

        return create_context, resolve_task

    def __init__(self, initdata, shared=None):
        self.initdata = initdata
        self.shared = dict(shared) if shared else None
        self.interpid = None
        self.callqueues = None
        self.mainfile = None

    def __del__(self):
        self.finalize()

    def initialize(self):
        # Create the interpreter
        assert self.interpid is None, self.interpid
        interpid = self.interpid = _interpreters.create(reqrefs=True)
        try:
            _interpreters.incref(interpid)
        except BaseException:
            _interpreters.destroy(interpid)
            raise  # re-raise

        try:
            # Create the queues.
            assert self.callqueues is None, self.callqueues
            self.callqueues = _ic._create_call_queues()

            # Set up needed stuff in the interpreter's __main__ module.
            script = f'import {_ic.__name__}'
            excinfo = _interpreters.exec(self.interpid, script, restrict=True)
            if excinfo is not None:
                raise ExecutionFailed(excinfo)

            # Add any provided objects to the interpreter's __main__ module.
            if self.shared:
                _interpreters.set___main___attrs(
                                    self.interpid, self.shared, restrict=True)

            # Run any provided initializer.
            if self.initdata:
                self.run(self.initdata)
        except BaseException:
            self.finalize()
            raise  # re-raise

    def finalize(self):
        try:
            # Clear the queues.
            callqueues = self.callqueues
            self.callqueues = None
            if callqueues is not None:
                _ic._release_call_queues(callqueues)
        finally:
            # Clear the interpreter.
            interpid = self.interpid
            self.interpid = None
            if interpid is not None:
                try:
                    _interpreters.decref(interpid)
                except _interpreters.InterpreterNotFoundError:
                    pass

    def run(self, task):
        fn, args, kwargs = task
        callsid, resultsid = self.callqueues

        res, excinfo, exc = _ic._call_in_interpreter(
                self.interpid, callsid, resultsid,
                fn, args, kwargs,
                reraise=True)
        if excinfo is not None:
            assert res is None, res
            if exc is not None and exc is not True:
                try:
                    raise ExecutionFailed(excinfo)
                except ExecutionFailed:
                    raise exc
            else:
                raise ExecutionFailed(excinfo)
        else:
            assert exc is None, exc
            return res


class BrokenInterpreterPool(_thread.BrokenThreadPool):
    """
    Raised when a worker thread in an InterpreterPoolExecutor failed initializing.
    """


class InterpreterPoolExecutor(_thread.ThreadPoolExecutor):

    BROKEN = BrokenInterpreterPool

    @classmethod
    def prepare_context(cls, initializer, initargs, shared):
        return WorkerContext.prepare(initializer, initargs, shared)

    def __init__(self, max_workers=None, thread_name_prefix='',
                 initializer=None, initargs=(), shared=None):
        """Initializes a new InterpreterPoolExecutor instance.

        Args:
            max_workers: The maximum number of interpreters that can be used to
                execute the given calls.
            thread_name_prefix: An optional name prefix to give our threads.
            initializer: A callable used to initialize each worker interpreter.
            initargs: A tuple of arguments to pass to the initializer.
            shared: A mapping of shareable objects to be inserted into
                each worker interpreter's __main__ module.
        """
        super().__init__(max_workers, thread_name_prefix,
                         initializer, initargs, shared=shared)
