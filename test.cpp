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
}

REFLB_MODULE(example, test)
