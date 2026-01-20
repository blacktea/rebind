#include "rebind.hpp"

#include <print>

namespace test {
    int a{};

    void foo() {
        std::println("foo method");
    }

    void bar() {
        std::println("bar method");
    }

    std::string greeting() {
        return "hello from C++ with Reflection. This's cool feature! C++ ❤ Python";
    }

    float pi() {
        return 3.14f;
    }

    long long speed_of_light() {
        return 300'000'000;
    }
}

REFLB_MODULE(example, test)
