"""Implements InterpreterPoolExecutor."""

import contextlib
import pickle
import sys
import textwrap
from . import thread as _thread
import _interpreters
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

    PARENT_MAIN_NS = '_parent_main_ns'

    @classmethod
    def prepare(cls, initializer, initargs, shared, on_init):
        def resolve_task(fn, args, kwargs):
            #if _interpreters.is_shareable(arg):
            data = pickle.dumps((fn, args, kwargs))
            return data

        if initializer is not None:
            initdata = resolve_task(initializer, initargs, {})
        else:
            initdata = None
        if on_init is not None:
            on_init = (on_init, resolve_task)
        def create_context():
            return cls(initdata, shared, on_init)
        return create_context, resolve_task

    @classmethod
    @contextlib.contextmanager
    def _capture_exc(cls, resultsid):
        try:
            yield
        except BaseException as exc:
            # Send the captured exception out on the results queue,
            # but still leave it unhandled for the interpreter to handle.
            err = pickle.dumps(exc)
            _interpqueues.put(resultsid, (None, err), 1, UNBOUND)
            raise  # re-raise

    @classmethod
    def _call(cls, func, args, kwargs, resultsid):
        with cls._capture_exc(resultsid):
            res = func(*args or (), **kwargs or {})
        # Send the result back.
        try:
            _interpqueues.put(resultsid, (res, None), 0, UNBOUND)
        except _interpreters.NotShareableError:
            res = pickle.dumps(res)
            _interpqueues.put(resultsid, (res, None), 1, UNBOUND)

    @classmethod
    def _call_pickled(cls, pickled, resultsid, main=False):
        with cls._capture_exc(resultsid):
            if main:
                try:
                    fn, args, kwargs = pickle.loads(pickled)
                except AttributeError as exc:
                    if not cls._maybe_handle_missing_main_attr(str(exc)):
                        raise  # re-raise
                    fn, args, kwargs = pickle.loads(pickled)
            else:
                fn, args, kwargs = pickle.loads(pickled)
        cls._call(fn, args, kwargs, resultsid)

    @classmethod
    def _maybe_handle_missing_main_attr(cls, errmsg):
        if not errmsg.endswith("'"):
            return False
        msg, _, name = errmsg[:-1].rpartition(" '")
        if msg != "module '__main__' has no attribute":
            return False
        mod = sys.modules.get('__main__')
        if not mod:
            return False
        mainns = vars(mod)
        if name in mainns:
            return False
        parentns = getattr(mod, cls.PARENT_MAIN_NS, None)
        if not parentns:
            return False
        try:
            value = parentns[name]
        except KeyError:
            return False
        if hasattr(value, '__module__'):
            try:
                value.__module__ = '__main__'
            except AttributeError:
                pass
        mainns[name] = value
        return True

    def __init__(self, initdata, shared=None, on_init=None):
        self.initdata = initdata
        self.shared = dict(shared) if shared else None
        self.on_init = on_init
        self.interpid = None
        self.resultsid = None
        self.mainfile = None

    def __del__(self):
        if self.interpid is not None:
            self.finalize()

    def _exec(self, script):
        assert self.interpid is not None
        excinfo = _interpreters.exec(self.interpid, script, restrict=True)
        if excinfo is not None:
            raise ExecutionFailed(excinfo)

    def initialize(self):
        assert self.interpid is None, self.interpid
        self.interpid = _interpreters.create(reqrefs=True)
        try:
            _interpreters.incref(self.interpid)

            maxsize = 0
            fmt = 0
            self.resultsid = _interpqueues.create(maxsize, fmt, UNBOUND)

            self._exec(f'from {__name__} import WorkerContext')

            if self.shared:
                _interpreters.set___main___attrs(
                                    self.interpid, self.shared, restrict=True)

            if self.initdata:
                self.run(self.initdata)

            if self.on_init is not None:
                on_init, resolve = self.on_init
                allowed = True
                def run_on_interp_during_worker_init(fn, /, *args, **kwargs):
                    if not allowed:
                        raise RuntimeError('worker initization has finished')
                    data = resolve(fn, args, kwargs)
                    return self.run(data)
                on_init(run_on_interp_during_worker_init)
                allowed = False
        except BaseException:
            self.finalize()
            raise  # re-raise

    def finalize(self):
        interpid = self.interpid
        resultsid = self.resultsid
        self.resultsid = None
        self.interpid = None
        if resultsid is not None:
            try:
                _interpqueues.destroy(resultsid)
            except _interpqueues.QueueNotFoundError:
                pass
        if interpid is not None:
            try:
                _interpreters.decref(interpid)
            except _interpreters.InterpreterNotFoundError:
                pass

    def _maybe_prepare_main(self):
        if self.mainfile:
            # XXX Make sure the filename matches?
            return True
        if self.mainfile is False:
            return False

        mod = sys.modules.get('__main__')
        mainfile = getattr(mod, '__file__', None)
        if not mainfile:
            self.mainfile = False
            return False
        # Functions defined in the __main__ module won't
        # automatically be available in the subinterpreter's
        # __main__, so unpickling will fail.  To work around this,
        # we must merge it in first.
        self._exec(f"""if True:
            import runpy
            {self.PARENT_MAIN_NS} = runpy.run_path({mainfile!r})
            """)
        self.mainfile = mainfile
        return True

    def run(self, task):
        data = task
        main = False
        if b'__main__' in data:
            main = self._maybe_prepare_main()
        args = (data, self.resultsid, main)
        script = f'WorkerContext._call_pickled(*{args})'

        try:
            self._exec(script)
        except ExecutionFailed as exc:
            exc_wrapper = exc
        else:
            exc_wrapper = None

        # Return the result, or raise the exception.
        while True:
            try:
                obj = _interpqueues.get(self.resultsid)
            except _interpqueues.QueueNotFoundError:
                raise  # re-raise
            except _interpqueues.QueueError:
                continue
            except ModuleNotFoundError:
                # interpreters.queues doesn't exist, which means
                # QueueEmpty doesn't.  Act as though it does.
                continue
            else:
                break
        (res, excdata), pickled, unboundop = obj
        assert unboundop is None, unboundop
        if excdata is not None:
            assert res is None, res
            assert pickled
            assert exc_wrapper is not None
            exc = pickle.loads(excdata)
            raise exc from exc_wrapper
        return pickle.loads(res) if pickled else res


class BrokenInterpreterPool(_thread.BrokenThreadPool):
    """
    Raised when a worker thread in an InterpreterPoolExecutor failed initializing.
    """


class InterpreterPoolExecutor(_thread.ThreadPoolExecutor):

    BROKEN = BrokenInterpreterPool

    @classmethod
    def prepare_context(cls, initializer, initargs, shared, on_init):
        return WorkerContext.prepare(initializer, initargs, shared, on_init)

    def __init__(self, max_workers=None, thread_name_prefix='',
                 initializer=None, initargs=(), shared=None, on_init=None):
        """Initializes a new InterpreterPoolExecutor instance.

        Args:
            max_workers: The maximum number of interpreters that can be used to
                execute the given calls.
            thread_name_prefix: An optional name prefix to give our threads.
            initializer: A callable used to initialize each worker interpreter.
            initargs: A tuple of arguments to pass to the initializer.
            shared: A mapping of shareable objects to be inserted into
                each worker interpreter's __main__ module.
            on_init: A function that gets called in the worker thread
                at the end of initialization.  The function is passed
                one argument: a temporary function that works similarly
                to executor.submit(), but runs on the worker's interpreter
                immediately and returns the result.
        """
        super().__init__(max_workers, thread_name_prefix,
                         initializer, initargs, shared=shared, on_init=on_init)
