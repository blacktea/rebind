#include "rebind.hpp"

#include <print>

namespace test {

// Test. Extract method from the header.
#include "mylib.h"

int a{};

void foo() { std::println("foo method"); }

void bar() { std::println("bar method"); }

std::string greeting() { return "hello from C++ with Reflection. This's cool feature! C++ ❤Python"; }

float pi() { return 3.14f; }

long long speed_of_light() { return 300'000'000; }

int sum(int a, int b) { return a + b; }

bool is_zero(float f) { return f <= 0.001; }

}  // namespace test

REFLB_MODULE(example, test)
