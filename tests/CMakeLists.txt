cmake_minimum_required(VERSION 3.22)
project(tests LANGUAGES CXX)

add_library(tests MODULE args_and_returns.cpp)
target_link_libraries(tests PRIVATE rebind)
target_compile_options(tests PRIVATE 
    -Wall
    -Wextra
    -std=c++26
    -freflection
    -freflection-latest
    -stdlib=libc++
)

# Apply Python extension naming
set_target_properties(tests PROPERTIES
    PREFIX ""
    SUFFIX "${PYTHON_EXT_SUFFIX}"
    POSITION_INDEPENDENT_CODE ON
)
set(test_targets tests)

# Run tests
add_custom_target(
  pytest
  COMMAND ${CMAKE_COMMAND} -E env TEST_MODULE_PATH=${CMAKE_CURRENT_BINARY_DIR} ${Python3_EXECUTABLE} -m pytest -s args_and_returns.py
  DEPENDS ${test_targets}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  USES_TERMINAL)
  