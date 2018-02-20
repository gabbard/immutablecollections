from unittest import TestCase, skip

from collections import Mapping

from flexnlp.utils.immutablecollections.immutablemultidict import ImmutableSetMultiDict, \
    ImmutableListMultiDict


class TestImmutableSetMultiDict(TestCase):
    def test_empty(self):
        empty = ImmutableSetMultiDict.empty()
        self.assertEqual(0, len(empty))
        empty2 = ImmutableSetMultiDict.of(dict())
        self.assertEqual(0, len(empty2))
        self.assertEqual(empty, empty2)
        empty3 = ImmutableSetMultiDict.builder().build()
        self.assertEqual(0, len(empty3))
        self.assertEqual(empty, empty3)

    def test_empty_singleton(self):
        empty1 = ImmutableSetMultiDict.empty()
        empty2 = ImmutableSetMultiDict.empty()
        self.assertIs(empty1, empty2)
        empty3 = ImmutableSetMultiDict.builder().build()
        self.assertIs(empty1, empty3)
        empty4 = ImmutableSetMultiDict.of(dict())
        self.assertIs(empty1, empty4)

    def test_set_repr(self):
        self.assertEqual("i{1: {2, 3}, 4: {5, 6}}",
                         repr(ImmutableSetMultiDict.of(
                             {1: [2, 3], 4: [5, 6]})))

    def test_set_str(self):
        self.assertEqual("{1: {2, 3}, 4: {5, 6}}",
                         str(ImmutableSetMultiDict.of(
                             {1: [2, 3], 4: [5, 6]})))

    def test_unmodified_copy_builder(self):
        ref: ImmutableSetMultiDict[str, int] = (ImmutableSetMultiDict.builder()
                                                .put('foo', 5).put('bar', 6)
                                                .put('foo', 4).build())

        self.assertEqual(ref, ref.modified_copy_builder().build())

    def test_modified_copy_builder(self):
        start: ImmutableSetMultiDict[str, int] = (ImmutableSetMultiDict.builder()
                                                  .put('foo', 5).put('bar', 6)
                                                  .put('foo', 4).build())
        updated = start.modified_copy_builder().put('bar', 1).put('foo', 7).put('meep', -1).build()

        ref: ImmutableSetMultiDict[str, int] = (ImmutableSetMultiDict.builder()
                                                .put('foo', 5).put('bar', 6)
                                                .put('foo', 4).put('foo', 7)
                                                .put('bar', 1).put('meep', -1).build())
        self.assertEqual(ref, updated)


class TestImmutableListMultiDict(TestCase):
    def test_empty(self):
        empty = ImmutableListMultiDict.empty()
        self.assertEqual(0, len(empty))
        empty2 = ImmutableListMultiDict.of(dict())
        self.assertEqual(0, len(empty2))
        self.assertEqual(empty, empty2)
        empty3 = ImmutableListMultiDict.builder().build()
        self.assertEqual(0, len(empty3))
        self.assertEqual(empty, empty3)

    def test_empty_singleton(self):
        empty1 = ImmutableListMultiDict.empty()
        empty2 = ImmutableListMultiDict.empty()
        self.assertIs(empty1, empty2)
        empty3 = ImmutableListMultiDict.builder().build()
        self.assertIs(empty1, empty3)
        empty4 = ImmutableListMultiDict.of(dict())
        self.assertIs(empty1, empty4)

    def test_of(self):
        x = ImmutableListMultiDict.of({1: [2, 2, 3], 4: [5, 6]})
        self.assertEqual([2, 2, 3], list(x[1]))

    def test_repr(self):
        self.assertEqual("i{1: [2, 2, 3]}", repr(ImmutableListMultiDict.of({1: [2, 2, 3]})))

    def test_str(self):
        self.assertEqual("{1: [2, 2, 3]}", str(ImmutableListMultiDict.of({1: [2, 2, 3]})))

    def test_immutable_keys(self):
        x = ImmutableListMultiDict.of({1: [2, 2, 3], 4: [5, 6]})
        # TypeError: 'FrozenDictBackedImmutableListMultiDict' object does not support item
        # assignment
        with self.assertRaises(TypeError):
            # noinspection PyUnresolvedReferences
            x[20] = [1, 2]

    def test_immutable_values(self):
        x = ImmutableListMultiDict.of({1: [2, 2, 3], 4: [5, 6]})
        # AttributeError: '_TupleBackedImmutableList' object has no attribute 'append'
        with self.assertRaises(AttributeError):
            # noinspection PyUnresolvedReferences
            x[1].append(7)
        # TypeError: '_TupleBackedImmutableList' object does not support item assignment
        with self.assertRaises(TypeError):
            # noinspection PyUnresolvedReferences
            x[1][0] = 7

    def test_cannot_init(self):
        with self.assertRaises(TypeError):
            # noinspection PyArgumentList
            ImmutableListMultiDict(dict())

    def test_isinstance(self):
        x = ImmutableListMultiDict.of({1: [2, 2, 3], 4: [5, 6]})
        self.assertTrue(isinstance(x, Mapping))

    def test_slots(self):
        x = ImmutableListMultiDict.of({1: [2, 2, 3], 4: [5, 6]})
        self.assertFalse(hasattr(x, '__dict__'))

    def test_builder(self):
        b: ImmutableListMultiDict.Builder[str, int] = ImmutableListMultiDict.builder()
        b.put('key', 1)
        b.put_all({'key': [3, 2, 1]})
        x = b.build()
        self.assertEqual([1, 3, 2, 1], list(x['key']))

    def test_unmodified_copy_builder(self):
        orig = ImmutableListMultiDict.of({1: [2, 2, 3], 4: [5, 6]})
        self.assertIs(orig, orig.modified_copy_builder().build())

    def test_modified_copy_builder(self):
        orig = ImmutableListMultiDict.of({1: [2, 2, 3], 4: [5, 6]})
        updated = orig.modified_copy_builder().put(4, 5).build()
        expected = ImmutableListMultiDict.of({1: [2, 2, 3], 4: [5, 6, 5]})
        self.assertEqual(expected, updated)

    def test_filter_keys(self):
        orig = ImmutableListMultiDict.of({1: [1], 2: [2], 3: [3], 4: [4]})
        evens = orig.filter_keys(lambda x: x % 2 == 0)
        self.assertEqual(ImmutableListMultiDict.of({2: [2], 4: [4]}), evens)
        all = orig.filter_keys(lambda x: x)
        self.assertEqual(orig, all)
