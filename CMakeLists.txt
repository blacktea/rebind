cmake_minimum_required(VERSION 3.22)
project(rebind LANGUAGES CXX)

# Path to your custom LLVM/Clang install
set(CLANG_INSTALL_PREFIX
    ""
    CACHE PATH "Custom Clang/LLVM install prefix"
)
option(USE_STATIC_LIBCXX "Link libc++/libc++abi/libunwind statically" OFF)
option(BUILD_EXAMPLES "Builds examples" ON)
option(BUILD_TESTS "Builds tests" ON)
option(ENABLE_ADDRESS_SANITIZER "Build with AddressSanitizer" OFF)
option(ENABLE_UNDEFINED_SANITIZER "Build with UndefinedBehaviorSanitizer" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Enable/Disable output of compile commands during generation." FORCE)


set(CMAKE_CXX_COMPILER
    "${CLANG_INSTALL_PREFIX}/bin/clang++"
    CACHE FILEPATH "Clang++ compiler"
)

set(REBIND_SANITIZER_FLAGS "")
set(REBIND_ASAN_RUNTIME "")
if (ENABLE_ADDRESS_SANITIZER)
    list(APPEND REBIND_SANITIZER_FLAGS -fsanitize=address)

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
        set(_rebind_asan_candidates libasan.so libclang_rt.asan-x86_64.so)
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(_rebind_asan_candidates libasan.so libclang_rt.asan-aarch64.so)
    else()
        set(_rebind_asan_candidates libasan.so)
    endif()

    foreach(_rebind_asan_candidate IN LISTS _rebind_asan_candidates)
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=${_rebind_asan_candidate}
            OUTPUT_VARIABLE _rebind_asan_runtime_candidate
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if (_rebind_asan_runtime_candidate AND NOT _rebind_asan_runtime_candidate STREQUAL "${_rebind_asan_candidate}")
            set(REBIND_ASAN_RUNTIME "${_rebind_asan_runtime_candidate}")
            break()
        endif()
    endforeach()
endif()

if (ENABLE_UNDEFINED_SANITIZER)
    list(APPEND REBIND_SANITIZER_FLAGS -fsanitize=undefined)
endif()

if (REBIND_SANITIZER_FLAGS)
    if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "Sanitizers are only supported with Clang and GCC toolchains")
    endif()

    add_library(rebind_sanitizers INTERFACE)
    target_compile_options(rebind_sanitizers INTERFACE ${REBIND_SANITIZER_FLAGS} -fno-omit-frame-pointer)
    target_link_options(rebind_sanitizers INTERFACE ${REBIND_SANITIZER_FLAGS})
endif()

### Build rebind library.

add_library(rebind INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include/rebind/rebind.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/rebind/cast.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/rebind/function_invoker.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/rebind/function_traits.hpp"
)
target_compile_options(rebind INTERFACE 
    -Wall
    -Wextra
    -pedantic
    -std=c++26
    -freflection
)

include(cmake/PythonUtils.cmake)


# libc++ headers (needed for <meta>)
target_include_directories(rebind INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}/include"
    "${CLANG_INSTALL_PREFIX}/include/c++/v1"
    ${Python3_INCLUDE_DIRS}
)

target_link_libraries(rebind INTERFACE
    Python3::Python
)

if (TARGET rebind_sanitizers)
    target_link_libraries(rebind INTERFACE rebind_sanitizers)
endif()

if (USE_STATIC_LIBCXX)
    message(STATUS "Linking libc++ statically")

    target_link_options(rebind INTERFACE
        -Wl,--whole-archive
        "${CLANG_INSTALL_PREFIX}/lib/x86_64-unknown-linux-gnu/libc++.a"
        "${CLANG_INSTALL_PREFIX}/lib/x86_64-unknown-linux-gnu/libc++abi.a"
        "${CLANG_INSTALL_PREFIX}/lib/x86_64-unknown-linux-gnu/libunwind.a"
        -Wl,--no-whole-archive
    )

    target_compile_options(rebind INTERFACE
        -stdlib=libc++
    )
    target_link_libraries(rebind INTERFACE
        dl
        pthread
    )
else()
    message(STATUS "Linking libc++ dynamically")

    target_link_libraries(rebind INTERFACE
        pthread
    )
endif()



if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
