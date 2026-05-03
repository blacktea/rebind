import pytest
import os
import sys

sys.path.insert(0, os.environ["TEST_MODULE_PATH"])

import class_test


def test_init_class() -> None:
    obj = class_test.TestClass()
    assert obj.get_age() == 0
    assert obj.is_adult() is False


def test_mutating_methods_update_instance_state() -> None:
    obj = class_test.TestClass()

    obj.set_age(17)
    assert obj.get_age() == 17
    assert obj.add_to_age(5) == 22
    assert obj.is_adult() is False

    obj.birthday()
    assert obj.get_age() == 18
    assert obj.is_adult() is True


def test_methods_with_arguments_and_return_values() -> None:
    obj = class_test.TestClass()
    obj.set_age(21)

    assert obj.describe("Alice") == "Alice is 21"


def test_method_argument_count_mismatch_raises_system_error() -> None:
    obj = class_test.TestClass()

    with pytest.raises(SystemError, match="returned a result with an exception set"):
        obj.set_age()

    with pytest.raises(SystemError, match="returned a result with an exception set"):
        obj.describe("Alice", "extra")
