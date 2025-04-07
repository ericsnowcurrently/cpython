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
    def _call_xidata(cls, taskdata, fmt, resultsid, needsmain=False):
        with cls._capture_exc(resultsid):
            try:
                (fn, args, kwargs,
                 ) = cls._get_callspec_from_xidata(taskdata, fmt, needsmain)
            except Exception:
                raise ValueError(f'failed to deserialize func or args')
        cls._call(fn, args, kwargs, resultsid)

    @classmethod
    def _get_callspec_as_xidata(cls, fn, args, kwargs):
        # For now, first try pickle and fall back to XID.
        def obj_as_xidata(obj):
            try:
                data = pickle.dumps(obj)
                fmt = 'pickle'
                needsmain = (b'__main__' in data)
            except (TypeError, pickle.PicklingError):
                data = obj
                fmt = 'xid'
                needsmain = None
            return (data, fmt), needsmain

        (data, fmt), needsmain = obj_as_xidata((fn, args, kwargs))
        if fmt == 'pickle':
            return data, fmt, needsmain
        assert fmt == 'xid', (fmt, fn, args, kwargs)

        fn, _needsmain = obj_as_xidata(fn)
        _, _fmt = fn
        if _fmt != 'xid':
            fmt = 'mixed'
        needsmain = needsmain or _needsmain

        _args = []
        for arg in args:
            arg, _needsmain = obj_as_xidata(arg)
            _, _fmt = arg
            _args.append(arg)
            if _fmt != 'xid':
                fmt = 'mixed'
            needsmain = needsmain or _needsmain
        args = tuple(_args)

        _kwargs = []
        for name, value in kwargs.items():
            value, _needsmain = obj_as_xidata(value)
            _, _fmt = value
            _kwargs.append((name, value))
            if _fmt != 'xid':
                fmt = 'mixed'
            needsmain = needsmain or _needsmain
        kwargs = tuple(_kwargs)

        return ((fn, args, kwargs), fmt, needsmain)

    @classmethod
    def _get_callspec_from_xidata(cls, data, fmt, needsmain=False):
        def obj_from_xidata(data, fmt, needsmain):
            if fmt == 'pickle':
                if needsmain:
                    while True:
                        try:
                            return pickle.loads(data)
                        except AttributeError as exc:
                            name = cls._maybe_handle_missing_main_attr(str(exc))
                            if not name:
                                raise  # re-raise
                else:
                    return pickle.loads(data)
            elif fmt == 'xid':
                return data
            else:
                raise NotImplementedError(fmt)

        if fmt == 'pickle':
            return obj_from_xidata(data, fmt, needsmain)
        elif fmt == 'xid':
            fn, args, kwargs = data
            kwargs = dict(kwargs)
            return fn, args, kwargs
        elif fmt == 'mixed':
            fn, args, kwargs = data
            fn = obj_from_xidata(*fn, needsmain)
            args = tuple(obj_from_xidata(*a, needsmain) for a in args)
            kwargs = {k: obj_from_xidata(*v, needsmain) for k, v in kwargs}
            return fn, args, kwargs
        else:
            raise NotImplementedError(fmt)

    @classmethod
    def _maybe_handle_missing_main_attr(cls, errmsg):
        if not errmsg.endswith("'"):
            return None
        msg, _, name = errmsg[:-1].rpartition(" '")
        if msg != "module '__main__' has no attribute":
            return None
        mod = sys.modules.get('__main__')
        if not mod:
            return None
        mainns = vars(mod)
        if name in mainns:
            return None
        parentns = getattr(mod, cls.PARENT_MAIN_NS, None)
        if not parentns:
            return None
        try:
            value = parentns[name]
        except KeyError:
            return None
        if hasattr(value, '__module__'):
            try:
                value.__module__ = '__main__'
            except AttributeError:
                pass
        mainns[name] = value
        return name

    def __init__(self, initdata, shared=None):
        self.initdata = initdata
        self.shared = dict(shared) if shared else None
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

    def run(self, task):
        script, shared = self._get_task_script(task)
        if shared:
            _interpreters.set___main___attrs(
                                self.interpid, shared, restrict=True)
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

    def _get_task_script(self, task):
        fn, args, kwargs = task
        data, fmt, needsmain = self._get_callspec_as_xidata(fn, args, kwargs)
        if needsmain:
            needsmain = self._maybe_prepare_main()
        if fmt == 'pickle':
            args = (data, fmt, self.resultsid, needsmain)
            script = f'WorkerContext._call_xidata(*{args})'
            shared = None
        else:
            args = (fmt, self.resultsid, needsmain)
            script = f'WorkerContext._call_xidata(_taskdata, *{args})'
            shared = dict(_taskdata=data)
        return script, shared

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
