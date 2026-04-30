import os
import sys

sys.path.insert(0, os.environ["TEST_MODULE_PATH"])

import class_test


def test_init_class() -> None:
    class_test.TestClass()
