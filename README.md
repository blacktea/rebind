# rebind - Python bindings with C++ reflection.

This projects implements Python bindings for C++ using C++26 reflection. The reflection provides meta information about entities(e.g. classes, functions, variables).\
The main advantage of using reflection is that we can automatically build bindings without writing any additional code.

For example, there are C++ functions that we want to expose.

```cpp
namespace test {

    struct Age {
        int age{};

        Age(int age) : age{age} {}

        int get_age(int value) { return age; }

        void set_age(int value) { age = value; }

        int add_to_age(int value) const { return age + value; }
    };
    
    int add(int a, int b) {
        return a + b;
    }
}
```

All we need to do is to provide the `test` namespace:

```cpp
REFLB_MODULE(example, test)
```

By using reflection, the rebind can make entities(classes and functions) available for Python code. Exposing new methods does not require writing binding code.

```python

import example

age = example.Age(10)

print("add to age", age.add_to_age(30))

print("get age", age.set_age(30))

print("sum 1 + 2", example.add(1, 2))

```


## BUILD

### Through Docker

The Dockerfile in the repo compiles trunk gcc and build the project.

1. Build an image: `docker build -t rebind .`
2. Run the image: `docker run -it --rm -v $(pwd):/rebind -t rebind`
3. (Optional) if you want to run test, activate python via `source .venv/bin/activate`
4. Build the project: `cmake -S . --build build` && `cmake --build build`


### Through clang fork[deprecated]

Prerequisites. By now, no mainstream compilers support reflection. [a clang fork](https://github.com/bloomberg/clang-p2996/tree/p2996) is an experimental implementation of the feature. It requires to build a clang compiler.

Build commands:

```
cmake -S llvm -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=./install -DLLVM_ENABLE_PROJECTS='clang;lld' -DLLVM_ENABLE_RUNTIMES="libc;libunwind;libcxxabi;libcxx" && \
cmake --build build && \
cmake --install build
```

First, set up clang compilers for C++ and C:

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


## TEST

Preparing step includes installing venv and install depencies.

```
python3 -m venv .venv
poetry install
```

Then running tests is simple:

```
cmake --build build --target pytest 
```

Optional sanitizer builds:

```
cmake -S . -B build-asan -DENABLE_ADDRESS_SANITIZER=ON
cmake -S . -B build-ubsan -DENABLE_UNDEFINED_SANITIZER=ON
```

## TODO

- [ ] No overload resolution
- [ ] Support =delete and other specifiers
- [ ] Support access to public member variables
- [ ] Handle types mismatch
- [ ] Handle exception translation (C++ → Python)
- [ ] Try replace std::array with std::inplace_vector
- [ ] Try to use concepts
- [ ] Use std::meta::parameters_of instead of function_trats struct.
- [ ] Use ranges as much as possible.
- [ ] And more