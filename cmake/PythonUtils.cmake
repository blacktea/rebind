
# Find and link Python
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

set(PYTHON_EXT_SUFFIX ".cpython-312-x86_64-linux-gnu.so")
message(STATUS "Python exe path: Python3_EXECUTABLE: ${Python3_EXECUTABLE}")

# Ask python for extension suffix
execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import sys, importlib; s = importlib.import_module('distutils.sysconfig' if sys.version_info < (3, 10) else 'sysconfig'); print(s.get_config_var('EXT_SUFFIX') or s.get_config_var('SO'))"
        OUTPUT_VARIABLE PYTHON_EXT_SUFFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

message(STATUS "Python extension suffix ${PYTHON_EXT_SUFFIX}")
