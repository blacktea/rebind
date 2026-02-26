cmake_minimum_required(VERSION 3.22)
project(tests LANGUAGES CXX)


add_library(tests MODULE test.cpp)
target_link_libraries(tests PRIVATE rebind)
target_compile_options(tests PRIVATE 
    -Wall
    -Wextra
    -std=c++26
    -freflection
    -freflection-latest
    -stdlib=libc++
)

# Ask python for extension suffix
execute_process(
    COMMAND ${Python3_EXECUTABLE}-config --extension-suffix
    OUTPUT_VARIABLE PY_EXT_SUFFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Apply Python extension naming
set_target_properties(tests PROPERTIES
    PREFIX ""
    SUFFIX "${PY_EXT_SUFFIX}"
    POSITION_INDEPENDENT_CODE ON
)
set(test_targets tests)

find_package(Python3 REQUIRED)
message(STATUS "Python exe path: ${PYTHON_EXE} ${CMAKE_CURRENT_BINARY_DIR}")


# Run tests
add_custom_target(
  pytest
  COMMAND ${CMAKE_COMMAND} -E env TEST_MODULE_PATH=${CMAKE_CURRENT_BINARY_DIR} ${PYTHON_EXE} -m pytest -s
  DEPENDS ${test_targets}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  USES_TERMINAL)