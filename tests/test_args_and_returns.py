import pytest
import os
import sys

sys.path.insert(0, os.environ["TEST_MODULE_PATH"])

import tests


def test_return_types() -> None:
    assert tests.return_float() == pytest.approx(3.14)
    assert tests.return_double() == pytest.approx(2.718281828)
    assert tests.return_bool_true() is True
    assert tests.return_bool_false() is False
    assert tests.return_long_long() == -123_456_789_012
    assert tests.return_unsigned_long_long() == 123_456_789_012
    assert tests.return_int() == -42
    assert tests.return_string() == "hello from std::string"
    assert tests.return_string_view() == "hello from std::string_view"
    assert tests.return_c_string() == "hello from const char*"
    assert (
        tests.accept_and_return_c_string("hello, accept_and_return_c_string")
        == "hello, accept_and_return_c_string"
    )
    assert tests.accept_and_return_std_string("std::string") == "return std::string"
    assert (
        tests.accept_and_return_std_string_view("std::string_view")
        == "std::string_view"
    )


def test_argument_types() -> None:
    assert tests.add_int(1, 2) == 3
    assert tests.add_long(1, 2) == 3
    assert tests.add_unsigned_long(1, 2) == 3
    assert tests.add_long_long(1, 2) == 3
    assert tests.add_unsigned_long_long(1, 2) == 3
    assert tests.add_uint32(1, 2) == 3
    assert tests.add_int64(1, 2) == 3
    assert tests.add_uint64(1, 2) == 3
    assert tests.add_size_t(1, 2) == 3
    assert tests.bool_identity(True) is True
    assert tests.bool_identity(False) is False
    assert tests.add_float(1.25, 2.75) == pytest.approx(4.0)
    assert tests.add_double(1.5, 2.25) == pytest.approx(3.75)


def test_argument_count_mismatch_raises_system_error() -> None:
    with pytest.raises(SystemError, match="returned a result with an exception set"):
        tests.add_int(1)
