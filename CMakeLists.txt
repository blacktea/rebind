cmake_minimum_required(VERSION 3.22)
project(example LANGUAGES CXX)

# Path to your custom LLVM/Clang install
set(CLANG_INSTALL_PREFIX
    ""
    CACHE PATH "Custom Clang/LLVM install prefix"
)

option(USE_STATIC_LIBCXX "Link libc++/libc++abi/libunwind statically" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Enable/Disable output of compile commands during generation." FORCE)


set(CMAKE_CXX_COMPILER
    "${CLANG_INSTALL_PREFIX}/bin/clang++"
    CACHE FILEPATH "Clang++ compiler"
)

### Build rebind library.

add_library(rebind INTERFACE)


target_compile_options(rebind INTERFACE 
    -Wall
    -Wextra
    -std=c++26
    -freflection
    -freflection-latest
    -stdlib=libc++
)

# libc++ headers (needed for <meta>)
target_include_directories(rebind INTERFACE
    "${CLANG_INSTALL_PREFIX}/include/c++/v1"
    ${Python3_INCLUDE_DIRS}
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

    target_link_libraries(rebind INTERFACE
        dl
        pthread
    )
else()
    message(STATUS "Linking libc++ dynamically")

    target_link_options(rebind INTERFACE
        -stdlib=libc++
    )

    target_link_libraries(rebind INTERFACE
        c++
        c++abi
        unwind
        dl
        pthread
    )
endif()


### Build example library.

add_library(example MODULE test.cpp)
target_link_libraries(example PRIVATE rebind)


# Find and link Python
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
target_link_libraries(example PRIVATE
Python3::Python
)

# Ask python for extension suffix
execute_process(
    COMMAND ${Python3_EXECUTABLE}-config --extension-suffix
    OUTPUT_VARIABLE PY_EXT_SUFFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Apply Python extension naming
set_target_properties(example PROPERTIES
    PREFIX ""
    SUFFIX "${PY_EXT_SUFFIX}"
    POSITION_INDEPENDENT_CODE ON
)