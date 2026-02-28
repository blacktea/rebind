#include "rebind/rebind.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace test {

float return_float() { return 3.14f; }

double return_double() { return 2.718281828; }

bool return_bool_true() { return true; }

bool return_bool_false() { return false; }

long long return_long_long() { return -123'456'789'012LL; }

unsigned long long return_unsigned_long_long() { return 123'456'789'012ULL; }

int return_int() { return -42; }

std::string return_string() { return "hello from std::string"; }

std::string_view return_string_view() { return "hello from std::string_view"; }

const char* return_c_string() { return "hello from const char*"; }

int add_int(int a, int b) { return a + b; }

int add_long(long a, long b) { return static_cast<int>(a + b); }

int add_unsigned_long(unsigned long a, unsigned long b) { return static_cast<int>(a + b); }

int add_long_long(long long a, long long b) { return static_cast<int>(a + b); }

int add_unsigned_long_long(unsigned long long a, unsigned long long b) { return static_cast<int>(a + b); }

int add_uint32(std::uint32_t a, std::uint32_t b) { return static_cast<int>(a + b); }

int add_int64(std::int64_t a, std::int64_t b) { return static_cast<int>(a + b); }

int add_uint64(std::uint64_t a, std::uint64_t b) { return static_cast<int>(a + b); }

int add_size_t(size_t a, size_t b) { return static_cast<int>(a + b); }

bool bool_identity(bool value) { return value; }

float add_float(float a, float b) { return a + b; }

double add_double(double a, double b) { return a + b; }

}  // namespace test

REFLB_MODULE(tests, test)
