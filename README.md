# rebind
Python bindings with C++ reflection.

This projects implements Python bindings for C++ using C++26 reflection. The reflection provides meta information about entities(e.g. classes, functions, variables).\
The main advantage of using reflection is that we can automatically build bindings without writing any additional code.

For example, there are C++ functions that we want to expose.

```cpp
namespace test {
    int a{};
    void foo() {
        std::println("foo method");
    }

    void bar() {
        std::println("bar method");
    }
}
```

All we need to do is to provide the `test` namespace:

```cpp
REFLB_MODULE(example, test)
```

By using reflection, the rebind project can get `foo` and `bar` methods and makes them available for Python code. Exposing new methods does not require writing binding code; recompiling the project is sufficient.

## Limitations

Current limitations include:
- Only free functions in namespaces are supported
- No function arguments are parsed yet
- Return values other than `void` are not handled
- No overload resolution
- No classes, member functions, or variables
- No exception translation (C++ → Python)
- Requires an experimental Clang fork (not standard C++)

These limitations are intentional to keep the project focused on
demonstrating C++ reflection.


## BUILD

Prerequisites. By now, no mainstream compilers support reflection. [a clang fork](https://github.com/bloomberg/clang-p2996/tree/p2996) is an experimental implementation of the feature. It requires to build a clang compiler.

Build commands:

```
cmake -S llvm -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=./install -DLLVM_ENABLE_PROJECTS='clang;lld' -DLLVM_ENABLE_RUNTIMES="libc;libunwind;libcxxabi;libcxx" && \
cmake --build build && \
cmake --install build
```

First of all, set up clang compilers for C++ and C:

```
export CC=/path_to_clang/install/bin/clang
export CXX=/path_to_clang/install/bin/clang++
```

Then, configure and build the project.

```
cmake -S . -B build -DCLANG_INSTALL_PREFIX=/path_to_clang/install -DUSE_STATIC_LIBCXX=ON && \
cmake --build build
```
These commands produce a shared library. To run a Python code, that library and python file should be in the same folder. Now we can run Python code, which invokes C++ functions.

```
python test.py
```