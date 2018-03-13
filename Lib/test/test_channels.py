import contextlib
import os
from textwrap import dedent
import threading
import unittest

import _xxsubinterpreters as _interpreters
from test.support import interpreters as support
from test.support import _interpreters as interpreters


########################
# helpers

"""
from test.test_interpreters import ...
"""

def use_channel(end):
    if end.end == 'recv':
        try:
            end.recv_nowait()
        except interpreters.ChannelEmptyError:
            pass
    else:
        end.send_nowait(b'spam')
            

class ChannelWrapper(support.ChannelWrapper):

    def __init__(self, end, opposite=None):
        super().__init__(end, opposite)
        self._closed = False

    def _use(self, end):
        use_channel(end)

    def use(self, end=None):
        if end is None:
            self.use_recv()
            self.use_send()
        else:
            end = self.resolve_end(end)
            self._use(end)

    def use_end(self):
        self._use(self.end)

    def use_opposite(self):
        self._use(self.opposite)

    def use_recv(self):
        self._use(self.rchan)

    def use_send(self):
        self._use(self.schan)

    def close_end(self, end, *, force=None):
        if self._closed:
            raise Exception('already closed')
        self._closed = True
        kwargs = {}
        if force is not None:
            kwargs['force'] = force
        end = self.resolve_end(end)
        end.close(**kwargs)

    def close_both(self):
        if self._closed:
            raise Exception('already closed')
        self._closed = True
        self.recv.close()
        self.send.close()

    def preclose(self, end, *, force=None):
        if end == 'both':
            self.close_both()
        else:
            self.close_end(end, force=force)


class ChannelOp(namedtuple('ChannelOp', 'end op interp closed')):

    OPS = ['use', 'close', 'force-close']
    OP = 'use'
    INTERPS = ['local', 'other']
    INTERP = 'local'
    ENDS = ['end', 'opposite', 'send', 'recv', 'both']

    REGEX = re.compile(r"""
        (?:({ops}):)?
        (?:({interps})/)?
        ({ends})
        (->CLOSED)?
        """.format(
            ops='|'.join(OPS),
            interps='|'.join(INTERPS),
            ends='|'.join(ENDS) + '|opp',
            ),
        re.VERBOSE,
        )

    @classmethod
    def from_raw(cls, raw):
        if isinstance(raw, cls):
            return raw
        elif isinstance(raw, str):
            return cls.parse(raw)
        else:
            try:
                return cls(**raw)
            except TypeError:
                try:
                    dict(**raw)
                except TypeError:
                    return cls(*raw)
                raise

    @classmethod
    def parse(cls, raw):
        """Return a ChannelOp corresponding to the given string.

        Format:

          <channel-op> -> <op>?<id><closed>?
          <id>         -> <interp>?<end>
          <interp>     -> local | other
          <end>        -> end | opposite | send | recv
          <closed>     -> "->CLOSED"

        Examples:

          end
          opposite
          other/end
          other/opposite
          use:recv
          close:send
          force-close:local/opposite
          close:end->CLOSED
          use:opposite->CLOSED
        """
        m = cls.REGEX.match(raw)
        if m is None:
            raise ValueError('unsupported channel op string')
        op, interp, end, closed = m.groups()
        return cls(end, op, interp, closed=closed)

    def __new__(cls, end, op=None, interp=None, *, closed=False):
        end = str(end) if end else None
        if end == 'opp':
            end = 'opposite'
        op = str(op) if op else cls.OP
        interp = str(interp) if interp else cls.INTERP
        closed = bool(closed)
        self = super().__new__(cls, end, op, interp, closed)
        return self

    def __init__(self, *args, **kwargs):
        if self.end is None:
            raise TypeError('missing end')
        if self.end not in self.ENDS:
            raise ValueError('unsupported end')

        if self.op not in self.OPS:
            raise ValueError('unsupported op')

        if self.interp not in self.INTERPS:
            raise ValueError('unsupported interp')

    def __str__(self):
        return '{}:{}/{}'.format(self.op, self.interp, self.end)


class ChannelTestBase(support.TestCase):

    def assert_fully_closed(self, rchan, schan):
        with self.assertRaises(interpreters.ChannelClosedError):
            schan.send_nowait(b'spam')
        with self.assertRaises(interpreters.ChannelClosedError):
            rchan.recv_nowait()
        with self.assertRaises(interpreters.ChannelClosedError):
            rchan.close()
        with self.assertRaises(interpreters.ChannelClosedError):
            schan.close()

    def assert_end_closed(self, end, opposite):
        if isinstance(end, interpreters.RecvChannel):
            rchan, schan = end, opposite
            schan.send_nowait(b'spam')  # The send end is still open.
            with self.assertRaises(interpreters.ChannelClosedError):
                rchan.recv_nowait()
        else:
            schan, rchan = end, opposite
            with self.assertRaises(interpreters.ChannelClosedError):
                schan.send_nowait(b'spam')
            with contextlib.suppress(interpreters.ChannelEmptyError):
                rchan.recv_nowait()  # The recv end is still open.
        with self.assertRaises(interpreters.ChannelClosedError):
            end.close()


########################
# general tests

class RecvChannelTests(ChannelTestBase):

    @unittest.skip('broken')  # XXX
    def test_recv(self):
        rchan1, schan1 = interpreters.create_channel()
        rchan2, schan2 = interpreters.create_channel()
        f = self.interpreter_runner("""
            obj = input.recv()
            check.send_nowait(obj)
            output.send_nowait(b'eggs')
            """,
            input=rchan1, output=schan2, check=schan1)
        with self.thread_running(f):
            schan1.send_nowait(b'spam')
        received = rchan2.recv()
        sent = rchan1.recv()

        self.assertEqual(received, b'eggs')
        self.assertEqual(sent, b'spam')

    def test_recv_nowait(self):
        rchan, schan = interpreters.create_channel()
        schan.send_nowait(b'spam')
        obj = rchan.recv()

        self.assertEqual(obj, b'spam')

    def test_recv_nowait_empty(self):
        rchan, _ = interpreters.create_channel()

        with self.assertRaises(interpreters.ChannelEmptyError):
            rchan.recv_nowait()


class SendChannelTests(ChannelTestBase):

    @unittest.skip('not implemented yet')
    def test_send(self):
        interp = interpreters.create()
        rchan, schan = interpreters.create_channel()
        def f():
            interp.run(dedent("""
                obj = rchan.recv()
                schan.send(b'eggs')
                """),
                channels={'schan': schan, 'rchan': rchan})
        t = threading.Thread(target=f)
        t.start()

        schan.send(b'spam')
        obj = rchan.recv()
        self.assertEqual(obj, b'eggs')
        t.join()

    def test_send_nowait(self):
        rchan, schan = interpreters.create_channel()
        schan.send_nowait(b'spam')
        obj = rchan.recv()
        self.assertEqual(obj, b'spam')


########################
# close() tests

def powerset(*values):
    yield from itertools.chain.from_iterable(
        itertools.combinations(values, r) for r in range(len(values)+1))


def _iter_all_ops(op):
    for end in ['end', 'opposite']:
        for interp in ['local', 'other']:
            yield ChannelOp(end, op, interp):


def _iter_use_op_sets(sets=None):
    if sets is None:
        yield from _iter_all_use_op_sets()
        return

    sets = list(sets)
    if not sets:
        return
    if not isinstance(sets[0], list):
        sets =[sets]
    for raw in sets:
        ops = []
        for use in raw:
            op = ChannelOp.from_raw(use)
            if op.op != 'use':
                raise ValueError('bad use {!r}'.format(use))
            if op.end == 'both':
                ops.extend([
                    op._replace(end='end'),
                    op._replace(end='opposite'),
                    ])
            else:
                ops.append(op)
        yield ops


def _iter_all_use_op_sets():
    # recv         _ X       X X X       X X X   X
    # send         _   X     X     X X   X X   X X
    # other recv   _     X     X   X   X X   X X X
    # other send   _       X     X   X X   X X X X
    yield from powerset(
        list(_iter_all_ops('use')))
#    for end in ['end', 'opposite']:
#        for interp in ['local', 'other']:
#            ops.append(
#                ChannelOp(end, 'use', interp))
#    yield from itertools.chain.from_iterable(
#        itertools.combinations(ops, r) for r in range(len(ops)+1))


def _iter_preclosed_op_sets():
    # recv         _ X       X X X       X X X   X F
    # send         _   X     X     X X   X X   X X   F
    # other recv   _     X     X   X   X X   X X X     F
    # other send   _       X     X   X X   X X X X       F
    yield from powerset(
        list(_iter_all_ops('close')))
    for op in _iter_all_ops('force-close'):
        yield [op]


def _iter_close_ops():
    # recv         X       F
    # send           X       F
    # other recv       X       F
    # other send         X       F
    yield from _iter_all_ops('close')
    yield from _iter_all_ops('force-close')


def _update_close_op_status(op, closed):
    if closed is True:
        op = op._replace(closed=True)
    else:
        if (op.end, op.interp) in closed:
            op = op._replace(closed=True)
        else:
            closed.add(
                (op.end, op.interp))
        if op.op == 'force-close':
            closed = True
    return op, closed


def iter_prep_ops(uses=None):
    combos = powerset(
        list(_iter_use_op_sets(uses)),
        list(_iter_preclosed_op_sets()),
        )
    for uses, preclosed in combos:
        ops = []

        for op in uses:
            ops.append(op)

        closed = set()
        for op in preclosed:
            op, closed = _update_close_op_status(op, closed)
            ops.append(op)

        yield ops, closed


class ChannelCloseFixture(ChannelWrapper):

    # XXX threaded vs. not
    # XXX pending objects

    def __init__(self, end, opposite=None, interp=None, prepops=None
                 *, local=True, checkfresh=True):
        super().__init__(end, opposite)
        self.interp = interp
        if prepops is None:
            prepops = list(iter_prep_ops())
        else:
            prepops = [ChannelOp.from_raw(op) for op in prepops]
        self.prepops = prepops
        self.local = local
        if local:
            self.other = interp
        else:
            raise NotImplementedError
        #self.install(interp)
        self.checkfresh = checkfresh

    #@property
    #def interp(self):
    #    if self._interp is None:
    #        self._interp = interpreters.create()
    #    return self._interp

    def prep(self):
        closed = set()
        for op, closed in self.prepops:
            self._run_op(op)
        return closed

    def run_all(self):
        for closeop in _iter_close_ops():
            self._run(closeop)

    def _run(self, closeop):
        closed = self.prep()
        closeop, closed = _update_close_op_status(closeop, closed)
        self._run_op(closeop)

        # result:
        #   fail
        #   recv
        #   send
        #   other recv
        #   other send
        #   fresh recv
        #   fresh send
        if closed is True:
            self.assert_fully_closed()
        else:
            self.assert_closed(closed)

    def _run_op(self, op):
        if op.interp == 'other':
            self._run_other_op(op)
            return
        end, op, _ = op
        with self._maybe_closed(op):
            if op.op == 'use':
                self.use(op.end)
            elif op.op == 'close':
                self.close(op.end)
            elif op.op == 'force-close':
                self.close(op.end, force=True)
            else:
                raise ValueError('unsupported op {!r}'.format(op.op))

    @contextlib.contextmanager
    def _maybe_closed(self, op):
        if op.closed:
            with self.assertRaises(interpreters.ChannelClosedError):
                yield
        else:
            yield

    def _run_other_op(self, op):
        raise NotImplementedError
        #end = self.resolve_end(end)
        #interp.run(dedent(f"""
        #    fix.preclose('{end.end}', force={force})
        #"""))


#class InterpreterChannelWrapper:
#
#    def __init__(self, end, opposite=None):












class ChannelCloseTest(namedtuple('ChannelCloseTest',
                                  'local end uses preclose preother fail checkfresh')):
    ...


class ChannelCloseTests(unittest.TestCase):

    TESTS = [
        #(end, other, use, pre, preother, fail, checkfresh),
        ('recv', None, ['recv'], 'end', None, False, False),
        ]

    def setUp(self):
        super().setUp()

    def assert_closed(self, fix, *, checkfresh=True):
        ...

    def assert_fully_closed(self, fix, *, checkfresh=True):
        ...

        rchan, schan = interpreters.create_channel()
        fix = ChannelFixture(rchan, schan)
        force = False
        interp = None

    def test_close(self):
        for local, end, uses, preclose, preother, fail, checkfresh in self.TESTS:

    def run_test(self, fix, prep, preclose=None, preother=None,
                 *, force=False):
        # Prep the channel.
        for end, other in prep:
            if other:
                raise NotImplementedError
            else:
                fix.use(end)
        
        # Pre-close.
        if preclose is not None:
            fix.preclose(preclose, force=???)
        if preother is not None:
            raise NotImplementedError

        # Close.
        fix.end.close(force=force)

        # Check.
        if force:
            self.assert_fully_closed(fix, interp=interp)
        else:
            self.assert_closed(fix)

    def run_test(self, prep, preclose, close, after):
        ...

    def test_simple(self):
        rchan, schan = interpreters.create_channel()
        fix = ChannelFixture(rchan, schan)

        fix.end.close()

        self.assert_end_closed(fix)

    def test_close_end(self):
        rchan, schan = interpreters.create_channel()
        fix = ChannelFixture(rchan, schan)

        fix.use_recv()
        fix.use_other_end()

        fix.preclose()
        fix.preclose_other()

        fix.end.close()

        self.assert_end_closed(fix)

    def test_close_both(self):
        rchan, schan = interpreters.create_channel()
        fix = ChannelFixture(rchan, schan)

        fix.use_recv()
        fix.use_other_end()

        fix.preclose()
        fix.preclose_other()

        fix.end.close()
        fix.opposite.close()

        self.assert_both_closed(fix)

    def test_close_fully(self):
        rchan, schan = interpreters.create_channel()
        fix = ChannelFixture(rchan, schan)

        fix.use_recv()
        fix.use_other_end()

        fix.preclose()
        fix.preclose_other()

        fix.end.close(force=True)

        self.assert_fully_closed(fix)


'''

    @classmethod
    def _prep_interp(cls, interp, end, opposite=None, **altnames):
        end, opposite = cls._channel_from_ends(end, opposite)
        channels = cls._compose_channels(end, opposite, altnames)
        interp.run(dedent("""
            #from test.support import interpreters
            from test.test_interpreters import ChannelFixture
            fix = ChannelFixture(test_end, test_opposite)
        """),
        channels=channels)

    @classmethod
    def _compose_channels(cls, end, opposite, altnames):
        rch, sch = (end, opposite) if end.end == 'recv' else (opposite, end)
        channels = dict(
            test_end=end,
            test_opposite=opposite,
            test_rchan=rch,
            test_schan=sch,
            **altnames
            )
        for name, chan in altnames.items():
            if chan is rch or chan is sch:
                continue
            elif not isinstance(chan, str):
                raise ValueError('unsupported channel {!r}'.format(chan))
            elif chan == 'end':
                channels[name] = end
            elif chan == 'opposite':
                channels[name] = opposite
            elif chan == 'recv':
                channels[name] = rch
            elif chan == 'send':
                channels[name] = sch
        return channels

    @property
    def interp(self):
        try:
            return self._interp
        except AttributeError:
            self._interp = interpreters.create()
            return self._interp

    def prep(self, *ops):
        interp = None
        for end, nowait, other in ops:
            if other:
                if interp is None:
                    interp = self.interp
                    interp.run('from test.support import interpreters',
                               channels=dict(schan=self.schan,
                                             rchan=self.rchan))
                self._run_other(interp, end, nowait)
            else:
                self._run_local(end, nowait)

    def _run_local(self, end, nowait):
        if end == 'recv':
            if nowait:
                try:
                    self.rchan.recv_nowait()
                except interpreters.ChannelEmptyError:
                    pass
            else:
                self.rchan.recv()
        else:
            if nowait:
                self.schan.send_nowait(b'spam')
            else:
                self.schan.send(b'spam')

    def _run_other(self, interp, end, nowait):
        if end == 'recv':
            if nowait:
                interp.run(dedent("""
                    try:
                        rchan.recv_nowait()
                    except interpreters.ChannelEmptyError:
                        pass
                    """))
            else:
                interp.run('rchan.recv()')
        else:
            if nowait:
                interp.run('schan.send_nowait(b"spam")')
            else:
                interp.run('schan.send(b"spam")')

    def preclose(self, which, *, force=False):
        ...

    def preclose_other(self, which, *, force=False):
        ...

    def close(self, *, closed=None):
        ...

    def after(self, *ops, assertRaises=None):
        ...

    def check(self, *, endonly=False):
        ...

    def test_already_closed(self):
        ...

    #def test_already_closed(self):
    #    fix = ChannelFixture('recv')
    #    fix.prep(
    #        (...,),
    #        (...,),
    #        (...,),
    #        )
    #    fix.preclose('recv')
    #    fix.preclose_other('send')
    #    fix.close()

    def test_already_closed(self):
        rchan, schan = interpreters.create_channel()
        interp = interpreters.create()
        fix = ChannelFixture(rchan, schan, interp)
        fix.prep([
            (...,),
            (...,),
            (...,),
            ])
        fix.preclose(rchan)
        fix.preclose_other(schan)
        with self.assertRaises(interpreters.ChannelClosedError):
            rchan.close()
        fix.after([
            (...,),
        ])
        fix.check_closed()


    for ends in [[], [rch], [sch], [rch, sch]]:
        for end in ends:
            if end.end == 'recv':
                try:
                    end.recv_nowait()
                except ChannelEmptyError:
                    pass
            else:
                end.send_nowait(b'spam')



    def _run_interp(self, interp, script):
        interp.run(dedent(script))

    def use(self, end, interp=None):
        if interp is None:
            if end.end == 'recv':
                try:
                    end.recv_nowait()
                except ChannelEmptyError:
                    pass
            else:
                end.send_nowait(b'spam')
        else:
            interp.run(dedent(script))


def _get_func_body(func):
    import ast, inspect
    src = inspect.getsource(func)
    root = ast.parse(src)
    node = root.body[0]
    body = src.split('\n', (node.body[0].lineno - node.lineno))[-1]
    return dedent(body)


def run_func(interp, func=None):
    if func is None:
        # Used as a decorator...
        return lambda f: run_func(interp, f)

    # XXX check signature, non-locals, globals?
    src = _get_func_body(func)
    interp.run(src)


class ChannelCloseTests(ChannelTestBase):

    interp vs. other
    close end vs. close fully
    use end vs. other end

    (end, opposite)
    (other end, other_opposite)

    use end / use opposite
    other: use end / use opposite

    close when already closed

    # before (x N)
    #   use end
    #   use opposite
    #   use other end
    #   use other opposite
    # pre-close
    #   close end
    #   close opposite
    #   force-close end
    #   force-close opposite
    #   close other end
    #   close opposite
    #   force-close other end
    #   force-close opposite
    # close
    #   (maybe catch ChannelClosedError)
    #   close end
    #   force-close end
    # after (x N)
    #   use end
    #   use opposite
    #   use other end
    #   use other opposite
    # check end/fully closed


    def test_local_channels(self):
        rchan, schan = interpreters.create_channel()

        ops = [
            ('send', b'spam'),
            ]
        for op, obj in ops:
            # close local after only end used
            # close local after only opposite used
            if op == 'recv':
                end, opposite = rchan, schan
                ...
            else:
                end, opposite = schan, rchan
                ...
'''






    def run_test(self, ops, end, force):
        for ops, closed in iter_test_ops():
        for op in ops:
            op = CloseOp.from_raw(op)



used, other_used, preclosed,
    ends = [None, 'end', 'opposite', 'both']
    for local in (True, False):
        for end in ends:

            for use in ends:
                for other_use in ends:

                    for preclose in ends:
                        for preclose_force in (True, False):
                            for preclose_other in ends:
                                for preclose_other_force in (True, False):

                                    if fail:
                                        with self.assertRaises(ChannelClosedError):
                                            close(force=force)
                                    else:
                                        end.close(force=force)

                                    assert end closed
                                    if both:
                                        assert opposite closed
                                        assert other no closed
                                    elif preclose_force or preclose_other_force:
                                        assert opposite closed
                                        assert other closed
                                    else:
                                        assert opposite not closed

                                    assert fresh interp closed




class RecvChannelCloseTests(ChannelTestBase):

    def test_close_local_only(self):
        rchan, schan = interpreters.create_channel()

        ...

    def test_close_only_recv_used(self):
        rchan, schan = interpreters.create_channel()
        with contextlib.suppress(interpreters.ChannelEmptyError):
            rchan.recv_nowait()
        rchan.close()

        self.assert_fully_closed(rchan, schan)

    def test_close_only_send_used(self):
        rchan, schan = interpreters.create_channel()
        schan.send_nowait(b'spam')
        rchan.close()

        self.assert_end_closed(rchan, schan)

    def test_close_both_used(self):
        rchan, schan = interpreters.create_channel()
        schan.send_nowait(b'spam')
        rchan.recv_nowait()
        rchan.close()

        self.assert_end_closed(rchan, schan)

    def test_close_never_used(self):
        rchan, schan = interpreters.create_channel()
        rchan.close()

        self.assert_fully_closed(rchan, schan)

    def test_close_already_closed(self):
        rchan, schan = interpreters.create_channel()
        rchan.close()

        with self.assertRaises(interpreters.ChannelClosedError):
            rchan.close()

    def test_close_recv_used_by_other(self):
        rchan, schan = interpreters.create_channel()
        self.run_in_thread(
            self.interpreter_runner("""
                try:
                    chan.recv_nowait()
                except Exception:
                    pass
                """,
                chan=rchan))
        rchan.close()

        self.assert_end_closed(rchan, schan)

    @unittest.skip('broken')  # XXX
    def test_close_send_used_by_other(self):
        rchan, schan = interpreters.create_channel()
        self.run_in_thread(
            self.interpreter_runner('ch.send_nowait(b"spam")', ch=schan))
        rchan.close()

        self.assert_end_closed(rchan, schan)

    def test_close_other_recv_closed(self):
        rchan, schan = interpreters.create_channel()
        with contextlib.suppress(interpreters.ChannelEmptyError):
            rchan.recv_nowait()
        self.run_in_thread(
            self.interpreter_runner('rch.close()', rch=rchan))
        rchan.close()

        self.assert_end_closed(rchan, schan)

    def test_close_other_send_closed(self):
        rchan, schan = interpreters.create_channel()
        with contextlib.suppress(interpreters.ChannelEmptyError):
            rchan.recv_nowait()
        self.run_in_thread(
            self.interpreter_runner('sch.close()', sch=schan))
        rchan.close()

        self.assert_end_closed(rchan, schan)

    def test_close_other_both_closed(self):
        rchan, schan = interpreters.create_channel()
        with contextlib.suppress(interpreters.ChannelEmptyError):
            rchan.recv_nowait()
        self.run_in_thread(
            self.interpreter_runner("""
                rch.close()
                sch.close()
                """,
                rch=rchan, sch=schan))
        rchan.close()

        self.assert_end_closed(rchan, schan)

    def test_close_never_used_other_closed(self):
        rchan, schan = interpreters.create_channel()
        self.run_in_thread(
            self.interpreter_runner('ch.close()', ch=rchan))

        self.assert_fully_closed(rchan, schan)

    def test_close_forced(self):
        rchan, schan = interpreters.create_channel()
        schan.send_nowait(b'spam')
        rchan.close(force=True)

        self.assert_fully_closed(rchan, schan)

    def test_close_other_forced(self):
        rchan, schan = interpreters.create_channel()
        with contextlib.suppress(interpreters.ChannelEmptyError):
            rchan.recv_nowait()
        self.run_in_thread(
            self.interpreter_runner('ch.close(force=True)', ch=rchan))

        self.assert_fully_closed(rchan, schan)

    def test_close_recv_in_other_after_closing(self):
        rchan, schan = interpreters.create_channel()
        schan.send_nowait(b'spam')
        rchan.close()
        self.run_in_thread(
            self.interpreter_runner('ch.recv()', ch=rchan))

        self.assert_end_closed(rchan, schan)

    @unittest.skip('broken')  # XXX
    def test_close_send_in_other_after_closing(self):
        rchan, schan = interpreters.create_channel()
        schan.send_nowait(b'spam')
        rchan.close()
        self.run_in_thread(
            self.interpreter_runner('ch.send_nowait(b"spam")', ch=schan))

        self.assert_end_closed(rchan, schan)

    def test_close_use_in_other_after_fully_closing(self):
        rchan, schan = interpreters.create_channel()
        rchan.close()
        self.run_in_thread(
            self.interpreter_runner("""
                from test.support import interpreters
                try:
                    ch.recv()
                except interpreters.ChannelClosedError:
                    pass
                else:
                    assert(False, 'channel unexpectedly still open')
                """,
                ch=rchan))

        self.assert_fully_closed(rchan, schan)


class SendChannelCloseTests(ChannelTestBase):

    def test_close_only_recv_used(self):
        rchan, schan = interpreters.create_channel()
        with contextlib.suppress(interpreters.ChannelEmptyError):
            rchan.recv_nowait()
        schan.close()

        self.assert_fully_closed(rchan, schan)

    def test_close_only_send_used(self):
        rchan, schan = interpreters.create_channel()
        schan.send_nowait(b'spam')
        schan.close()

        self.assert_end_closed(schan, rchan)

    def test_close_both_used(self):
        rchan, schan = interpreters.create_channel()
        schan.send_nowait(b'spam')
        rchan.recv_nowait()
        schan.close()

        self.assert_end_closed(schan, rchan)

    def test_close_never_used(self):
        rchan, schan = interpreters.create_channel()
        schan.close()

        self.assert_fully_closed(rchan, schan)

    def test_close_already_closed(self):
        rchan, schan = interpreters.create_channel()
        schan.close()

        with self.assertRaises(interpreters.ChannelClosedError):
            rchan.close()

    def test_close_recv_used_by_other(self):
        rchan, schan = interpreters.create_channel()
        self.run_in_thread(
            self.interpreter_runner("""
                try:
                    chan.recv_nowait()
                except Exception:
                    pass
                """,
                chan=rchan))
        schan.close()

        self.assert_end_closed(schan, rchan)

    @unittest.skip('broken')  # XXX
    def test_close_send_used_by_other(self):
        rchan, schan = interpreters.create_channel()
        self.run_in_thread(
            self.interpreter_runner('ch.send_nowait(b"spam")', ch=schan))
        schan.close()

        self.assert_end_closed(schan, rchan)

    def test_close_other_recv_closed(self):
        rchan, schan = interpreters.create_channel()
        schan.send_nowait(b'spam')
        self.run_in_thread(
            self.interpreter_runner('rch.close()', rch=rchan))
        schan.close()

        self.assert_end_closed(schan, rchan)

    def test_close_other_send_closed(self):
        rchan, schan = interpreters.create_channel()
        schan.send_nowait(b'spam')
        self.run_in_thread(
            self.interpreter_runner('sch.close()', sch=schan))
        schan.close()

        self.assert_end_closed(schan, rchan)

    def test_close_other_both_closed(self):
        rchan, schan = interpreters.create_channel()
        schan.send_nowait(b'spam')
        self.run_in_thread(
            self.interpreter_runner("""
                rch.close()
                sch.close()
                """,
                rch=rchan, sch=schan))
        schan.close()

        self.assert_end_closed(schan, rchan)

    def test_close_never_used_other_closed(self):
        rchan, schan = interpreters.create_channel()
        self.run_in_thread(
            self.interpreter_runner('ch.close()', ch=rchan))

        self.assert_fully_closed(rchan, schan)

    def test_close_forced(self):
        rchan, schan = interpreters.create_channel()
        with contextlib.suppress(interpreters.ChannelEmptyError):
            rchan.recv_nowait()
        schan.close(force=True)

        self.assert_fully_closed(rchan, schan)

    def test_close_other_forced(self):
        rchan, schan = interpreters.create_channel()
        schan.send_nowait(b'spam')
        self.run_in_thread(
            self.interpreter_runner('ch.close(force=True)', ch=rchan))

        self.assert_fully_closed(rchan, schan)

    def test_close_recv_in_other_after_closing(self):
        rchan, schan = interpreters.create_channel()
        with contextlib.suppress(interpreters.ChannelEmptyError):
            rchan.recv_nowait()
        schan.close()
        self.run_in_thread(
            self.interpreter_runner('ch.recv()', ch=rchan))

        self.assert_end_closed(schan, rchan)

    @unittest.skip('broken')  # XXX
    def test_close_send_in_other_after_closing(self):
        rchan, schan = interpreters.create_channel()
        with contextlib.suppress(interpreters.ChannelEmptyError):
            rchan.recv_nowait()
        schan.close()
        self.run_in_thread(
            self.interpreter_runner('ch.send_nowait(b"spam")', ch=schan))

        self.assert_end_closed(schan, rchan)

    def test_close_use_in_other_after_fully_closing(self):
        rchan, schan = interpreters.create_channel()
        schan.close()
        self.run_in_thread(
            self.interpreter_runner("""
                from test.support import interpreters
                try:
                    ch.recv()
                except interpreters.ChannelClosedError:
                    pass
                else:
                    assert(False, 'channel unexpectedly still open')
                """,
                ch=rchan))

        self.assert_fully_closed(rchan, schan)


if __name__ == '__main__':
    unittest.main()
