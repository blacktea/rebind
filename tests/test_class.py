import pytest
import os
import sys

sys.path.insert(0, os.environ["TEST_MODULE_PATH"])

import custom


def test_init_custom() -> None:
    c = custom.Custom()
    c.first = "first name"
    print(f"name {c.name()}")
    assert False
