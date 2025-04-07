import contextlib
import pickle
import sys
import _interpreters
import _interpqueues

from _interpreters import NotShareableError


UNBOUND = 2  # error; this should not happen.

FMT_XID = 0
FMT_PICKLE = 1
FMT_TUPLE = 8
FMT_DICT = 9


#############################
# in the calling interpreter

def _create_call_queues():
    maxsize = 0
    fmt = 0
    callsid = _interpqueues.create(maxsize, fmt, UNBOUND)
    _interpqueues.bind(callsid)
    try:
        resultsid = _interpqueues.create(maxsize, fmt, UNBOUND)
        _interpqueues.bind(resultsid)
    except BaseException:
        try:
            _interpqueues.release(callsid)
        except _interpqueues.QueueNotFoundError:
            pass
    return (callsid, resultsid)


def _release_call_queues(queues):
    callsid, resultsid = queues
    try:
        _interpqueues.release(callsid)
    except _interpqueues.QueueNotFoundError:
        pass
    finally:
        try:
            _interpqueues.release(resultsid)
        except _interpqueues.QueueNotFoundError:
            pass


def _call_in_interpreter(interpid, callsid, resultsid, fn, args, kwargs,
                         reraise=False):
    # Convert the call data to cross-interpreter compatible.
    try:
        callspec, callsid = _callspec_to_xid_literal(fn, args, kwargs, callsid)
    except Exception:
        raise NotShareableError('failed to convert call data')

    # Make the call via a script.
    queues = (callsid, resultsid)
    reraise = bool(reraise)
    # The caller already imported this module into the interpreter's __main__.
    script = f'{__name__}._call_xidata(*{callspec}, *{queues}, {reraise})'
    excinfo = _interpreters.exec(interpid, script, restrict=True)

    # Get the result.
    resdata = _pop_queue(resultsid)
    if resdata is None:
        # This indicates an implementation error somewhere here.
        assert excinfo is not None
        return None, excinfo, True
    (res, excdata), fmt = resdata

    # Return (result, excinfo, exc).
    if excinfo is not None:
        assert res is None, res
        exc = _xid_to_exception(excdata, fmt)
        assert exc is True or reraise, (exc, reraise)
    else:
        assert excdata is None, excdata
        res = _xid_to_result(res, fmt)
        exc = None
    return res, excinfo, exc


def _callspec_to_xid_literal(fn, args, kwargs, callsid):
    # For simplicity, we try pickle first.
    try:
        xid = _pickle_pack((fn, args, kwargs), None)
    except pickle.PicklingError:
        # Fall back to converting each part separately.
        xid, fmt = _callspec_to_xid(fn, args, kwargs)
        assert fmt != FMT_PICKLE
        assert callsid is not None
        _put_queue(callsid, xid, fmt)
        return (None, None), callsid
    else:
        return (xid, FMT_PICKLE), None


def _callspec_to_xid(fn, args, kwargs, mainfile=None):
    def convert_obj(obj, mainfile=None):
        try:
            xid = _pickle_pack(obj, mainfile)
        except pickle.PicklingError:
            # We will try the object as-is.
            fmt = FMT_XID
            xid = obj
        else:
            fmt = FMT_PICKLE
            if not mainfile:
                _, mainfile = xid
        return (xid, fmt), mainfile

    fn_xid, mainfile = convert_obj(fn, mainfile)

    args_xid, mainfile = convert_obj(args, mainfile)
    _, fmt = args_xid
    if fmt == FMT_XID:
        # At least one arg was not pickleable.
        args_xid = []
        for arg in args:
            arg, mainfile = convert_obj(arg, mainfile)
            args_xid.append(arg)
        assert not all(fmt == FMT_PICKLE for _, fmt in args_xid), (args, args_xid)
        args_xid = (tuple(args_xid), FMT_TUPLE)

    kwargs_xid, mainfile = convert_obj(kwargs, mainfile)
    _, fmt = kwargs_xid
    if fmt == FMT_XID:
        # At least one kwarg was not pickleable.
        kwargs_xid = []
        for name, value in kwargs.items():
            value, mainfile = convert_obj(value, mainfile)
            kwargs_xid.append((name, value))
        kwargs_xid = (tuple(kwargs_xid), FMT_DICT)

    return ((fn_xid, args_xid, kwargs_xid, mainfile), FMT_TUPLE)


def _xid_to_exception(xid, fmt):
    if xid is True:
        assert fmt == FMT_XID, fmt
        return True

    assert xid is not None
    if fmt == FMT_XID:
        return xid
    elif fmt == FMT_PICKLE:
        try:
            exc, _ = _pickle_unpack(xid, 'exception')
        except Exception:
            # This isn't critical so we can mostly ignore it.
            return True
        return exc
    else:
        raise NotImplementedError((fmt, excdata))


def _xid_to_result(xid, fmt):
    if fmt == FMT_XID:
        return xid
    elif fmt == FMT_PICKLE:
        res, _ = _pickle_unpack(xid, 'result')
        return res
    else:
        raise NotImplementedError((fmt, res))


#############################
# in the called interpreter

def _call_xidata(xid, fmt, callsid, resultsid, reraise=False):
    sent = False
    try:
        # Convert the cross-interpreter data back into the call data.
        if callsid is not None:
            assert xid is None and fmt is None, (fmt, xid)
            xid, fmt = _pop_queue(callsid)
        try:
            (fn, args, kwargs), mainfile = _xid_to_callspec(xid, fmt)
        except Exception:
            raise NotShareableError('failed to deserialize func or args')

        # Make the call.
        try:
            res = fn(*args or (), **kwargs or {})
        except BaseException as exc:
            if reraise:
                # Send the captured exception out on the results queue,
                # to make it re-raisable and to represent that the call
                # itself failed.
                _put_exception(resultsid, exc, mainfile)
                sent = True
            # Re-raise the exception for the interpreter to handle.
            raise

        # Send the result back.
        _put_result(resultsid, res, mainfile)
        sent = True
    except BaseException:
        # Make sure the results queue gets poked.
        if not sent:
            _put_queue(resultsid, (None, True), FMT_XID)
        raise  # re-raise


def _xid_to_callspec(xid, fmt):
    # Until there's a viable alternative, we always pickle.
    mainfile = None
    if fmt == FMT_PICKLE:
        callspec, mainfile = _pickle_unpack(xid, 'call')
    elif fmt == FMT_TUPLE:
        fn_xid, args_xid, kwargs_xid, mainfile = xid

        xid, fmt = fn_xid
        if fmt == FMT_PICKLE:
            fn, _ = _pickle_unpack(xid, 'func')
        else:
            assert fmt == FMT_XID

        xid, fmt = args_xid
        if fmt == FMT_PICKLE:
            args, _ = _pickle_unpack(xid, 'call args')
        elif fmt == FMT_TUPLE:
            args = []
            for arg, fmt in xid:
                if fmt == FMT_PICKLE:
                    arg, _ = _pickle_unpack(arg, 'call arg')
                else:
                    assert fmt == FMT_XID
                args.append(arg)
            args = tuple(args)
        else:
            assert fmt == FMT_XID

        xid, fmt = kwargs_xid
        if fmt == FMT_PICKLE:
            kwargs, _ = _pickle_unpack(xid, 'call kwargs')
        elif fmt == FMT_DICT:
            kwargs = []
            for name, (value, fmt) in xid:
                if fmt == FMT_PICKLE:
                    value, _ = _pickle_unpack(value, 'call kwarg')
                else:
                    assert fmt == FMT_XID
                kwargs.append((name, value))
            kwargs = dict(kwargs)
        else:
            assert fmt == FMT_XID

        callspec = (fn, args, kwargs)
    else:
        raise NotImplementedError((fmt, xid))
    return callspec, mainfile


def _put_exception(resultsid, exc, mainfile):
    assert exc is not None
    try:
        _put_queue(resultsid, (None, exc), FMT_XID)
    except NotShareableError:
        pass
    else:
        return
    # Fall back to pickle.
    try:
        exc = _pickle_pack(exc, mainfile)
        fmt = FMT_PICKLE
    except Exception:
        raise NotShareableError('failed to serialize exception')
    _put_queue(resultsid, (None, exc), fmt)


def _put_result(resultsid, res, mainfile):
    try:
        _put_queue(resultsid, (res, None), FMT_XID)
    except NotShareableError:
        pass
    else:
        return

    # Fall back to pickle.
    try:
        res = _pickle_pack(res, mainfile)
        fmt = FMT_PICKLE
    except Exception:
        raise NotShareableError('failed to serialize result')
    _put_queue(resultsid, (res, exc), fmt)


#############################
# in both

def _put_queue(queueid, obj, fmt):
    return _interpqueues.put(queueid, obj, fmt, UNBOUND)


def _pop_queue(queueid, wait=False, default=None):
    while True:
        try:
            try:
                obj, fmt, unboundop = _interpqueues.get(queueid)
                break
            except ModuleNotFoundError:
                # interpreters.queues doesn't exist, which means
                # QueueEmpty doesn't.  Act as though it does.
                raise _interpqueues.QueueError('queue empty')
        except _interpqueues.QueueNotFoundError:
            raise  # re-raise
        except _interpqueues.QueueError:
            # The queue is empty.
            if wait:
                continue
            return default
    assert unboundop is None, unboundop
    return obj, fmt


def _pickle_pack(obj, mainfile=None):
    try:
        data = pickle.dumps(obj)
    except TypeError as exc:
        raise pickle.PicklingError(str(exc))
    if b'__main__' in data:
        if mainfile is None:
            mainfile = _interpreters.get_mainfile()
    else:
        mainfile = None
    return data, mainfile


def _pickle_unpack(xid, kind=None):
#    try:
#        data, mainfile = xid
#    except ValueError as exc:
#        raise ValueError((str(exc), xid))
    data, mainfile = xid
    if not mainfile:
        return pickle.loads(data), None
    assert b'__main__' in data, (mainfile, data)

    mainns = parentns = None
    while True:
        try:
            data = pickle.loads(data)
            break
        except AttributeError as exc:
            errmsg = str(exc)
            (name, mainns, parentns,
             ) = _handle_missing_main_attr(errmsg, mainfile, mainns, parentns)
            if not name:
                # It wasn't fixed, so re-raise the AttributeError.
                raise
    return data, mainfile


def _handle_missing_main_attr(errmsg, mainfile, mainns, parentns):
    # Functions (and other objects) defined in the main
    # interpreter's __main__ module won't automatically
    # be available in the subinterpreter's __main__,
    # so unpickling will fail.  To work around this,
    # we must merge the object in first.
    if not errmsg.endswith("'"):
        return None, mainns, parentns
    msg, _, name = errmsg[:-1].rpartition(" '")
    if msg != "module '__main__' has no attribute":
        return None, mainns, parentns

    if mainns is None:
        mod = sys.modules.get('__main__')
        if not mod:
            return None, mainns, parentns
        mainns = vars(mod)
        if name in mainns:
            return None, mainns, parentns

    if parentns is None:
        assert mainfile
        parentns = _interpreters.get_parent_mainns(mainfile)
        if not parentns:
            return None, mainns, parentns

    try:
        value = parentns[name]
    except KeyError:
        return None
    if hasattr(value, '__module__'):
        try:
            value.__module__ = '__main__'
        except AttributeError:
            pass
    # XXX Fix func.__globals__.
    mainns[name] = value

    return name, mainns, parentns
