import contextlib
import os
from textwrap import dedent
import threading
import unittest

from ._interpreters import *


def clean_up_interpreters():
    for interp in list_all():
        if interp.id == 0:  # main
            continue
        try:
            interp.destroy()
        except RuntimeError:
            pass  # already destroyed


def clean_up_channels():
    import _xxsubinterpreters as _interpreters
    for chan in list_all_channels():
        try:
            _interpreters.channel_destroy(chan.id)
        except ChannelNotFoundError:
            pass  # already destroyed


def interpreter_runner(script, *, interp=None, **channels):
    script = dedent(script)
    if interp is None:
        interp = interpreters.create()
    def func():
        interp.run(script, channels=channels)
    return func


def capture_script(channame, script, *, close=False):
    script = dedent(script)
    indent = """
            """  # lines up with {script} below...
    script = indent[1:] + indent.join(script.splitlines())
    # XXX: Also capture stdout & stderr?
    return dedent(f"""
        try:
            {script}
        except Exception:
            import traceback
            {channame}.send_nowait(
                traceback.format_exc().encode('utf-8'))
            # XXX del traceback?
        else:
            {channame}.send_nowait(None)
        if {close}:
            {channame}.close()
        """)


def captured_runner(script, *, interp=None, **channels):
    script = capture_script('_capture_chan', script, close=True)

    rchan, schan = create_channel()
    channels['_capture_chan'] = schan

    if interp is None:
        interp = interpreters.create()
    def run():
        interp.run(script, channels=channels)

    def resolve():
        err = rchan.recv()
        rchan.close()
        return err.decode('utf-8')

    return run, resolve


def channel_from_end(end, *, create=True):
    if isinstance(end, RecvChannel):
        opposite = SendChannel(end.id)
    elif isinstance(end, SendChannel):
        opposite = RecvChannel(end.id)
    else:
        if not create:
            raise ValueError('unsupported end {!r}'.format(end))
        end, opposite = _channel_from_kind(end)
    return end, opposite


def match_channel(end, opposite=None, *, create=True):
    if opposite is None:
        return = channel_from_end(end)

    if isinstance(end, RecvChannel):
        if not isinstance(opposite, SendChannel):
            raise ValueError(f'mismatch ({opposite})')
    elif isinstance(end, SendChannel):
        if not isinstance(opposite, RecvChannel):
            raise ValueError(f'mismatch ({opposite})')
    else:
        raise ValueError(f'bad end ({end})')
    if opposite.id != end.id:
        raise ValueError(f'mismatch ({opposite.id} != {end.id})')
    return end, opposite


class ChannelWrapper:

    def __init__(self, end, opposite=None):
        end, opposite = match_channel(end, opposite)
        rch, sch = (end, opposite) if end.end == 'recv' else (opposite, end)
        self.end = end
        self.opposite = opposite
        self.rchan = rch
        self.schan = sch

    def resolve_end(self, end):
        if isinstance(end, interpreters.ChannelEnd):
            return end
        elif isinstance(end, str):
            if end == 'recv':
                return self.rchan
            elif end == 'send':
                return self.schan
            elif end == 'end':
                return self.end
            elif end == 'opposite':
                return self.opposite
        raise ValueError('unsupported end {!r}'.format(end))

    def install(self, name, interp):
        channels = dict(
            __ChannelWrapper_end=self.end,
            __ChannelWrapper_opposite=self.opposite,
        )
        interp.run(dedent(f"""
            from test.support.interpreters import ChannelWrapper
            {name} = ChannelWrapper(
                __ChannelWrapper_end,
                __ChannelWrapper_opposite,
            )
            del __ChannelWrapper_end
            del __ChannelWrapper_opposite
            del ChannelWrapper
        """), channels=channels)


class TestCase(unittest.TestCase):

    def tearDown(self):
        clean_up_interpreters()
        clean_up_channels()
        super().tearDown()

    @contextlib.contextmanager
    def thread_running(self, func, **kwargs):
        with _thread_running(func, **kwargs) as t:
            yield t
        self.assertFalse(t.is_alive(), 'function never returned')

    def run_in_thread(self, func):
        with self.thread_running(func):
            pass

    def interpreter_runner(self, script, **channels):
        script = dedent(script)
        interp = interpreters.create()
        def func():
            interp.run(script, channels=channels)
        return func

    def run_captured(self, interp, script, *, threaded=False, **channels):
        run, resolve = captured_runner(script, interp=interp, **channels)
        if threaded:
            with _thread_running(run):
                pass
        else:
            run()
        return resolve()


########################
# internal functions

@contextlib.contextmanager
def _thread_running(func, timeout=1):
    t = threading.Thread(target=func)
    t.start()
    try:
        yield t
    finally:
        t.join(timeout=timeout)


@contextlib.contextmanager
def _running(interp):
    r, w = os.pipe()
    def run():
        interp.run(dedent(f"""
            # wait for "signal"
            with open({r}) as chan:
                chan.read()
            """))
    with _thread_running(run) as t:
        yield
        with open(w, 'w') as chan:
            chan.write('done')


def _channel_from_kind(kind):
    rchan, schan = create_channel()
    if kind == 'recv':
        return rchan, schan
    elif kind == 'send':
        return schan, rchan
    else:
        raise ValueError('unsupported kind {!r}'.format(kind))
