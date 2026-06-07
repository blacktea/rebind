cmake_minimum_required(VERSION 3.22)
project(tests LANGUAGES CXX)


set(test_targets)

function(add_test name src)
    add_library(${name} MODULE ${src}.cpp)
    target_link_libraries(${name} PRIVATE rebind)
    target_compile_options(${name} PRIVATE 
        -Wall
        -Wextra
        -std=c++26
        -freflection
    )

    # Apply Python extension naming
    set_target_properties(${name} PROPERTIES
        PREFIX ""
        SUFFIX "${PYTHON_EXT_SUFFIX}"
        POSITION_INDEPENDENT_CODE ON
    )
    message(STATUS "name " ${name})

    set(test_targets ${test_targets};${name} PARENT_SCOPE)
endfunction()

add_test(tests test_invoke_methods_with_args)
add_test(test_class test_class)


message(STATUS "test_targets" ${test_targets})

set(pytest_env TEST_MODULE_PATH=${CMAKE_CURRENT_BINARY_DIR})
if (ENABLE_ADDRESS_SANITIZER)
    if (NOT REBIND_ASAN_RUNTIME)
        message(FATAL_ERROR "Unable to locate AddressSanitizer runtime for pytest")
    endif()
    list(APPEND pytest_env LD_PRELOAD=${REBIND_ASAN_RUNTIME})
    list(APPEND pytest_env ASAN_OPTIONS=detect_leaks=0)
endif()

# Run tests
add_custom_target(
  pytest
  COMMAND ${CMAKE_COMMAND} -E env ${pytest_env} ${Python3_EXECUTABLE} -m pytest -s test_invoke_methods_with_args.py test_class.py
  DEPENDS ${test_targets}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  USES_TERMINAL)