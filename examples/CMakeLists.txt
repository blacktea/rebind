cmake_minimum_required(VERSION 3.22)
project(example LANGUAGES CXX)


add_library(example MODULE test.cpp mylib.h)
target_link_libraries(example PRIVATE rebind)
target_compile_options(example PRIVATE 
    -Wall
    -Wextra
    -std=c++26
    -freflection
)

# Apply Python extension naming
set_target_properties(example PROPERTIES
    PREFIX ""
    SUFFIX "${PYTHON_EXT_SUFFIX}"
    POSITION_INDEPENDENT_CODE ON
)