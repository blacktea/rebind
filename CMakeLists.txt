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

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Enable/Disable output of compile commands during generation." FORCE)


set(CMAKE_CXX_COMPILER
    "${CLANG_INSTALL_PREFIX}/bin/clang++"
    CACHE FILEPATH "Clang++ compiler"
)

### Build rebind library.

add_library(rebind INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include/rebind/rebind.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/rebind/cast.hpp"
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