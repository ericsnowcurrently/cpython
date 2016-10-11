from . import util as test_util

machinery = test_util.import_importlib('importlib.machinery')

import sys


class ImportStateTests:

    def setUp(self):
        super().setUp()

        self.modules = {'sys': sys}
        self.path_finder = self.machinery.PathFinder()
        self.finders = [self.path_finder]
        self.search_locations = ['x', 'y']

    def test_default(self):
        state = self.machinery.ImportState()

        self.assertEqual(state.modules, {})
        self.assertEqual(state.finders, [])
        self.assertEqual(state.search_locations, [])
        self.assertIsNone(state.path_finder)

    def test_full(self):
        state = self.machinery.ImportState(self.modules, self.finders,
                                           self.search_locations,
                                           self.path_finder)

        self.assertEqual(state.modules, self.modules)
        self.assertEqual(state.finders, self.finders)
        self.assertEqual(state.search_locations, self.search_locations)
        self.assertEqual(state.path_finder, self.path_finder)

    def test_repr(self):
        state = self.machinery.ImportState(self.modules, self.finders,
                                           self.search_locations,
                                           self.path_finder)
        staterepr = repr(state)

        self.assertEqual(repr(state),
                         ('ImportState(modules={}, finders={}, '
                          'search_locations={}, path_finder={})'
                          ).format(self.modules, self.finders,
                                   self.search_locations, self.path_finder))
        self.assertEqual(repr(self.machinery.ImportState()),
                         ('ImportState(modules={}, finders=[], '
                          'search_locations=[], path_finder=None)'))


(Frozen_ImportStateTests,
 Source_ImportStateTests
 ) = test_util.test_both(ImportStateTests, machinery=machinery)


class ImportStateEqualityTests:

    def setUp(self):
        super().setUp()

        self.modules = {'sys': sys}
        self.path_finder = self.machinery.PathFinder()
        self.finders = [self.path_finder]
        self.search_locations = ['x', 'y']
        self.state = self.machinery.ImportState(self.modules, self.finders,
                                                self.search_locations,
                                                self.path_finder)

    def test_equal_identity(self):
        self.assertTrue(self.state == self.state)

    def test_equal_empty_identity(self):
        state = self.machinery.ImportState()

        self.assertTrue(state == state)

    def test_equal_same(self):
        state = self.machinery.ImportState(self.state.modules,
                                           self.state.finders,
                                           self.state.search_locations,
                                           self.state.path_finder)

        self.assertTrue(self.state == state)

    def test_equal_empty_same(self):
        state1 = self.machinery.ImportState()
        state2 = self.machinery.ImportState()

        self.assertTrue(state1 == state2)

    def test_not_equal(self):
        self.assertFalse(self.state == self.machinery.ImportState())


(Frozen_ImportStateEqualityTests,
 Source_ImportStateEqualityTests
 ) = test_util.test_both(ImportStateEqualityTests, machinery=machinery)


if __name__ == '__main__':
    unittest.main()
