import contextlib
import os
import threading
from textwrap import dedent
import unittest

from test import support
from test.support import import_helper
_interpreters = import_helper.import_module('_xxsubinterpreters')

import interpreters


def _captured_script(script):
    r, w = os.pipe()
    indented = script.replace('\n', '\n                ')
    wrapped = dedent(f"""
        import contextlib
        with open({w}, 'w', encoding='utf-8') as spipe:
            with contextlib.redirect_stdout(spipe):
                {indented}
        """)
    return wrapped, open(r, encoding='utf-8')


def clean_up_interpreters():
    for interp in interpreters.list_all():
        if interp.id == 0:  # main
            continue
        try:
            interp.close()
        except RuntimeError:
            pass  # already destroyed


def _run_output(interp, request, channels=None):
    script, rpipe = _captured_script(request)
    with rpipe:
        interp.run(script, channels=channels)
        return rpipe.read()


@contextlib.contextmanager
def _running(interp):
    r, w = os.pipe()
    def run():
        interp.run(dedent(f"""
            # wait for "signal"
            with open({r}) as rpipe:
                rpipe.read()
            """))

    t = threading.Thread(target=run)
    t.start()

    yield

    with open(w, 'w') as spipe:
        spipe.write('done')
    t.join()


class TestBase(unittest.TestCase):

    def tearDown(self):
        clean_up_interpreters()


class CreateTests(TestBase):

    def test_in_main(self):
        interp = interpreters.create()
        self.assertIsInstance(interp, interpreters.Interpreter)
        self.assertIn(interp, interpreters.list_all())

    def test_in_thread(self):
        lock = threading.Lock()
        interp = None
        def f():
            nonlocal interp
            interp = interpreters.create()
            lock.acquire()
            lock.release()
        t = threading.Thread(target=f)
        with lock:
            t.start()
        t.join()
        self.assertIn(interp, interpreters.list_all())

    def test_in_subinterpreter(self):
        main, = interpreters.list_all()
        interp = interpreters.create()
        out = _run_output(interp, dedent("""
            import interpreters
            interp = interpreters.create()
            print(interp.id)
            """))
        interp2 = interpreters.Interpreter(int(out))
        self.assertEqual(interpreters.list_all(), [main, interp, interp2])

    def test_after_destroy_all(self):
        before = set(interpreters.list_all())
        # Create 3 subinterpreters.
        interp_lst = []
        for _ in range(3):
            interps = interpreters.create()
            interp_lst.append(interps)
        # Now destroy them.
        for interp in interp_lst:
            interp.close()
        # Finally, create another.
        interp = interpreters.create()
        self.assertEqual(set(interpreters.list_all()), before | {interp})

    def test_after_destroy_some(self):
        before = set(interpreters.list_all())
        # Create 3 subinterpreters.
        interp1 = interpreters.create()
        interp2 = interpreters.create()
        interp3 = interpreters.create()
        # Now destroy 2 of them.
        interp1.close()
        interp2.close()
        # Finally, create another.
        interp = interpreters.create()
        self.assertEqual(set(interpreters.list_all()), before | {interp3, interp})


class GetCurrentTests(TestBase):

    def test_main(self):
        main = interpreters.get_main()
        current = interpreters.get_current()
        self.assertEqual(current, main)

    def test_subinterpreter(self):
        main = _interpreters.get_main()
        interp = interpreters.create()
        out = _run_output(interp, dedent("""
            import interpreters
            cur = interpreters.get_current()
            print(cur.id)
            """))
        current = interpreters.Interpreter(int(out))
        self.assertNotEqual(current, main)


class ListAllTests(TestBase):

    def test_initial(self):
        interps = interpreters.list_all()
        self.assertEqual(1, len(interps))

    def test_after_creating(self):
        main = interpreters.get_current()
        first = interpreters.create()
        second = interpreters.create()

        ids = []
        for interp in interpreters.list_all():
            ids.append(interp.id)

        self.assertEqual(ids, [main.id, first.id, second.id])

    def test_after_destroying(self):
        main = interpreters.get_current()
        first = interpreters.create()
        second = interpreters.create()
        first.close()

        ids = []
        for interp in interpreters.list_all():
            ids.append(interp.id)

        self.assertEqual(ids, [main.id, second.id])


class TestInterpreterAttrs(TestBase):

    def test_id_type(self):
        main = interpreters.get_main()
        current = interpreters.get_current()
        interp = interpreters.create()
        self.assertIsInstance(main.id, _interpreters.InterpreterID)
        self.assertIsInstance(current.id, _interpreters.InterpreterID)
        self.assertIsInstance(interp.id, _interpreters.InterpreterID)

    def test_main_id(self):
        main = interpreters.get_main()
        self.assertEqual(main.id, 0)

    def test_custom_id(self):
        interp = interpreters.Interpreter(1)
        self.assertEqual(interp.id, 1)

        with self.assertRaises(TypeError):
            interpreters.Interpreter('1')

    def test_id_readonly(self):
        interp = interpreters.Interpreter(1)
        with self.assertRaises(AttributeError):
            interp.id = 2

    @unittest.skip('not ready yet (see bpo-32604)')
    def test_main_isolated(self):
        main = interpreters.get_main()
        self.assertFalse(main.isolated)

    @unittest.skip('not ready yet (see bpo-32604)')
    def test_subinterpreter_isolated_default(self):
        interp = interpreters.create()
        self.assertFalse(interp.isolated)

    def test_subinterpreter_isolated_explicit(self):
        interp1 = interpreters.create(isolated=True)
        interp2 = interpreters.create(isolated=False)
        self.assertTrue(interp1.isolated)
        self.assertFalse(interp2.isolated)

    @unittest.skip('not ready yet (see bpo-32604)')
    def test_custom_isolated_default(self):
        interp = interpreters.Interpreter(1)
        self.assertFalse(interp.isolated)

    def test_custom_isolated_explicit(self):
        interp1 = interpreters.Interpreter(1, isolated=True)
        interp2 = interpreters.Interpreter(1, isolated=False)
        self.assertTrue(interp1.isolated)
        self.assertFalse(interp2.isolated)

    def test_isolated_readonly(self):
        interp = interpreters.Interpreter(1)
        with self.assertRaises(AttributeError):
            interp.isolated = True

    def test_equality(self):
        interp1 = interpreters.create()
        interp2 = interpreters.create()
        self.assertEqual(interp1, interp1)
        self.assertNotEqual(interp1, interp2)


class TestInterpreterIsRunning(TestBase):

    def test_main(self):
        main = interpreters.get_main()
        self.assertTrue(main.is_running())

    @unittest.skip('Fails on FreeBSD')
    def test_subinterpreter(self):
        interp = interpreters.create()
        self.assertFalse(interp.is_running())

        with _running(interp):
            self.assertTrue(interp.is_running())
        self.assertFalse(interp.is_running())

    def test_from_subinterpreter(self):
        interp = interpreters.create()
        out = _run_output(interp, dedent(f"""
            import _xxsubinterpreters as _interpreters
            if _interpreters.is_running({interp.id}):
                print(True)
            else:
                print(False)
            """))
        self.assertEqual(out.strip(), 'True')

    def test_already_destroyed(self):
        interp = interpreters.create()
        interp.close()
        with self.assertRaises(RuntimeError):
            interp.is_running()

    def test_does_not_exist(self):
        interp = interpreters.Interpreter(1_000_000)
        with self.assertRaises(RuntimeError):
            interp.is_running()

    def test_bad_id(self):
        interp = interpreters.Interpreter(-1)
        with self.assertRaises(ValueError):
            interp.is_running()


class TestInterpreterClose(TestBase):

    def test_basic(self):
        main = interpreters.get_main()
        interp1 = interpreters.create()
        interp2 = interpreters.create()
        interp3 = interpreters.create()
        self.assertEqual(set(interpreters.list_all()),
                         {main, interp1, interp2, interp3})
        interp2.close()
        self.assertEqual(set(interpreters.list_all()),
                         {main, interp1, interp3})

    def test_all(self):
        before = set(interpreters.list_all())
        interps = set()
        for _ in range(3):
            interp = interpreters.create()
            interps.add(interp)
        self.assertEqual(set(interpreters.list_all()), before | interps)
        for interp in interps:
            interp.close()
        self.assertEqual(set(interpreters.list_all()), before)

    def test_main(self):
        main, = interpreters.list_all()
        with self.assertRaises(RuntimeError):
            main.close()

        def f():
            with self.assertRaises(RuntimeError):
                main.close()

        t = threading.Thread(target=f)
        t.start()
        t.join()

    def test_already_destroyed(self):
        interp = interpreters.create()
        interp.close()
        with self.assertRaises(RuntimeError):
            interp.close()

    def test_does_not_exist(self):
        interp = interpreters.Interpreter(1_000_000)
        with self.assertRaises(RuntimeError):
            interp.close()

    def test_bad_id(self):
        interp = interpreters.Interpreter(-1)
        with self.assertRaises(ValueError):
            interp.close()

    def test_from_current(self):
        main, = interpreters.list_all()
        interp = interpreters.create()
        out = _run_output(interp, dedent(f"""
            import interpreters
            interp = interpreters.Interpreter({int(interp.id)})
            try:
                interp.close()
            except RuntimeError:
                print('failed')
            """))
        self.assertEqual(out.strip(), 'failed')
        self.assertEqual(set(interpreters.list_all()), {main, interp})

    def test_from_sibling(self):
        main, = interpreters.list_all()
        interp1 = interpreters.create()
        interp2 = interpreters.create()
        self.assertEqual(set(interpreters.list_all()),
                         {main, interp1, interp2})
        interp1.run(dedent(f"""
            import interpreters
            interp2 = interpreters.Interpreter(int({interp2.id}))
            interp2.close()
            interp3 = interpreters.create()
            interp3.close()
            """))
        self.assertEqual(set(interpreters.list_all()), {main, interp1})

    def test_from_other_thread(self):
        interp = interpreters.create()
        def f():
            interp.close()

        t = threading.Thread(target=f)
        t.start()
        t.join()

    @unittest.skip('Fails on FreeBSD')
    def test_still_running(self):
        main, = interpreters.list_all()
        interp = interpreters.create()
        with _running(interp):
            with self.assertRaises(RuntimeError):
                interp.close()
            self.assertTrue(interp.is_running())


class TestInterpreterRun(TestBase):

    def test_success(self):
        interp = interpreters.create()
        script, file = _captured_script('print("it worked!", end="")')
        with file:
            interp.run(script)
            out = file.read()

        self.assertEqual(out, 'it worked!')

    def test_in_thread(self):
        interp = interpreters.create()
        script, file = _captured_script('print("it worked!", end="")')
        with file:
            def f():
                interp.run(script)

            t = threading.Thread(target=f)
            t.start()
            t.join()
            out = file.read()

        self.assertEqual(out, 'it worked!')

    @support.requires_fork()
    def test_fork(self):
        interp = interpreters.create()
        import tempfile
        with tempfile.NamedTemporaryFile('w+', encoding='utf-8') as file:
            file.write('')
            file.flush()

            expected = 'spam spam spam spam spam'
            script = dedent(f"""
                import os
                try:
                    os.fork()
                except RuntimeError:
                    with open('{file.name}', 'w', encoding='utf-8') as out:
                        out.write('{expected}')
                """)
            interp.run(script)

            file.seek(0)
            content = file.read()
            self.assertEqual(content, expected)

    @unittest.skip('Fails on FreeBSD')
    def test_already_running(self):
        interp = interpreters.create()
        with _running(interp):
            with self.assertRaises(RuntimeError):
                interp.run('print("spam")')

    def test_does_not_exist(self):
        interp = interpreters.Interpreter(1_000_000)
        with self.assertRaises(RuntimeError):
            interp.run('print("spam")')

    def test_bad_id(self):
        interp = interpreters.Interpreter(-1)
        with self.assertRaises(ValueError):
            interp.run('print("spam")')

    def test_bad_script(self):
        interp = interpreters.create()
        with self.assertRaises(TypeError):
            interp.run(10)

    def test_bytes_for_script(self):
        interp = interpreters.create()
        with self.assertRaises(TypeError):
            interp.run(b'print("spam")')

    # test_xxsubinterpreters covers the remaining Interpreter.run() behavior.


class TestIsShareable(TestBase):

    def test_default_shareables(self):
        shareables = [
                # singletons
                None,
                # builtin objects
                b'spam',
                'spam',
                10,
                -10,
                ]
        for obj in shareables:
            with self.subTest(obj):
                shareable = interpreters.is_shareable(obj)
                self.assertTrue(shareable)

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
                100.0,
                # user-defined types and objects
                Cheese,
                Cheese('Wensleydale'),
                SubBytes(b'spam'),
                ]
        for obj in not_shareables:
            with self.subTest(repr(obj)):
                self.assertFalse(
                    interpreters.is_shareable(obj))
