import io
import _sys
import sys
import unittest


MODULE_ATTRS = {
        "__name__",
        "__doc__",
        "__loader__",
        "__spec__",
        "__package__",
        "__file__",
        "__cached__",
        "__builtins__",
        }


class SysModuleTest(unittest.TestCase):

    def setUp(self):
        self.orig_stdout = _sys.stdout
        self.orig_displayhook = _sys.displayhook
        self.orig_excepthook = _sys.excepthook

    def tearDown(self):
        _sys.stdout = self.orig_stdout
        _sys.displayhook = self.orig_displayhook
        _sys.excepthook = self.orig_excepthook

    def test_get_existing_attr_backward_compatible(self):
        for name, expected in vars(_sys).items():
            if name in MODULE_ATTRS:
                continue
            with self.subTest(name):
                value = getattr(sys, name)
                self.assertIs(value, expected)

    def test_get_missing_attr_backward_compatible(self):
        with self.assertRaises(AttributeError):
            sys.spamspamspam

    def test_set_new_attr_backward_compatible(self):
        value = 'eggs'
        sys.spamspamspam = value
        self.assertIs(sys.spamspamspam, value)
        self.assertIs(_sys.spamspamspam, value)

    def test_set_existing_attr_backward_compatible(self):
        out = io.StringIO()
        def hook(obj):
            raise RuntimeError
        sys.displayhook = hook
        sys.excepthook = hook
        sys.stdout = out

        self.assertIs(_sys.displayhook, hook)
        self.assertIs(_sys.excepthook, hook)
        self.assertIs(_sys.stdout, out)

    def test_del_new_attr_backward_compatible(self):
        _sys.spamspamspam = 'eggs'
        del sys.spamspamspam
        with self.assertRaises(AttributeError):
            sys.spamspamspam
        with self.assertRaises(AttributeError):
            _sys.spamspamspam
