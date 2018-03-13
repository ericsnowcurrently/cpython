import os
from textwrap import dedent
import threading
import unittest

from test.support import interpreters as support
from test.support import _interpreters as interpreters


class IsShareableTests(unittest.TestCase):

    def test_default_shareables(self):
        shareables = [
                # singletons
                None,
                # builtin objects
                b'spam',
                ]
        for obj in shareables:
            with self.subTest(obj):
                self.assertTrue(
                    interpreters.is_shareable(obj))

    def test_not_shareable(self):
        class Cheese:
            def __init__(self, name):
                self.name = name
            def __str__(self):
                return self.name

        class SubBytes(bytes):
            """A subclass of a shareable type."""

        not_shareables = [
                # singletons
                True,
                False,
                NotImplemented,
                ...,
                # builtin types and objects
                type,
                object,
                object(),
                Exception(),
                42,
                100.0,
                'spam',
                # user-defined types and objects
                Cheese,
                Cheese('Wensleydale'),
                SubBytes(b'spam'),
                ]
        for obj in not_shareables:
            with self.subTest(obj):
                self.assertFalse(
                    interpreters.is_shareable(obj))


class ListAllTests(support.TestCase):

    def test_initial(self):
        main = interpreters.get_main()
        interps = interpreters.list_all()
        self.assertEqual(interps, [main])

    def test_after_creating(self):
        main = interpreters.get_main()
        first = interpreters.create()
        second = interpreters.create()
        interps = interpreters.list_all()
        self.assertEqual(interps, [main, first, second])

    def test_after_destroying(self):
        main = interpreters.get_main()
        first = interpreters.create()
        second = interpreters.create()
        first.destroy()
        interps = interpreters.list_all()
        self.assertEqual(interps, [main, second])


class GetCurrentTests(support.TestCase):

    def test_main(self):
        main = interpreters.get_main()
        cur = interpreters.get_current()
        self.assertEqual(cur, main)

    def test_subinterpreter(self):
        return
        main = interpreters.get_main()
        interp = interpreters.create()
        out = _run_output(interp, dedent("""
            import _xxsubinterpreters as _interpreters
            print(_interpreters.get_current())
            """))
        cur = int(out.strip())
        _, expected = interpreters.list_all()
        self.assertEqual(cur, expected)
        self.assertNotEqual(cur, main)


class GetMainTests(support.TestCase):

    def test_from_main(self):
        [expected] = interpreters.list_all()
        main = interpreters.get_main()
        self.assertEqual(main, expected)

    def test_from_subinterpreter(self):
        return
        [expected] = interpreters.list_all()
        interp = interpreters.create()
        out = _run_output(interp, dedent("""
            import _xxsubinterpreters as _interpreters
            print(_interpreters.get_main())
            """))
        main = int(out.strip())
        self.assertEqual(main, expected)


class InterpreterTests(support.TestCase):

    def test_create(self):
        interp = interpreters.create()
        self.assertIs(type(interp), interpreters.Interpreter)
        self.assertIn(interp, interpreters.list_all())

    def test_is_running(self):
        main = interpreters.get_main()
        self.assertTrue(main.is_running())

        interp = interpreters.create()
        self.assertFalse(interp.is_running())
        with _running(interp):
            self.assertTrue(interp.is_running())
        self.assertFalse(interp.is_running())

    def test_destroy(self):
        interp1 = interpreters.create()
        interp2 = interpreters.create()
        interp3 = interpreters.create()
        self.assertIn(interp2, interpreters.list_all())
        interp2.destroy()
        self.assertNotIn(interp2, interpreters.list_all())
        self.assertIn(interp1, interpreters.list_all())
        self.assertIn(interp3, interpreters.list_all())

    def run_string_in_current_thread(self):
        interp = interpreters.create()
        rchan, schan = interpreters.create_channel()
        interp.run('chan.send_nowait(b"it worked!)',
                   channels={'chan': schan})
        obj = rchan.recv()
        self.assertEqual(obj, b'it worked!')

    def run_string_in_own_thread(self):
        interp = interpreters.create()
        rchan, schan = interpreters.create_channel()

        def f():
            interp.run('chan.send_nowait(b"it worked!)',
                       channels={'chan': schan})
        with self._thread_running(f):
            pass
        obj = rchan.recv()
        self.assertEqual(obj, b'it worked!')


if __name__ == '__main__':
    unittest.main()
