import pytest
import os
import sys

sys.path.insert(0, os.environ["TEST_MODULE_PATH"])

import tests


def test_foo_and_bar_do_not_raise() -> None:
    tests.foo()
    tests.bar()


def test_greeting() -> None:
    assert (
        tests.greeting()
        == "hello from C++ with Reflection. This's cool feature! C++ ❤Python"
    )


def test_pi() -> None:
    assert tests.pi() == pytest.approx(3.14)


def test_speed_of_light() -> None:
    assert tests.speed_of_light() == 300_000_000


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        (0.0, True),
        (0.0001, True),
        (12.23, False),
    ],
)
def test_is_zero(value: float, expected: bool) -> None:
    assert tests.is_zero(value) is expected


def test_sum() -> None:
    assert tests.sum(3, 124) == 127
