#include "rebind/rebind.hpp"

#include <format>
#include <string>
#include <string_view>

namespace test_class {

struct TestClass {
    int age{};

    int get_age() const { return age; }

    void set_age(int value) { age = value; }

    void birthday() { ++age; }

    int add_to_age(int value) const { return age + value; }

    bool is_adult() const { return age >= 18; }

    std::string describe(std::string_view name) const { return std::format("{} is {}", name, age); }
};

struct TestConstructor {
    TestConstructor(int age) : m_age{age} {}

    int add_to_age(int value) const { return m_age + value; }

    int m_age{};
};

/*
// TODO: Support =delete(std::meta::is_deleted). Now it doesn't compile
struct TestConstructorDeleted {
    TestConstructorDeleted(int age) = delete;

    int add_to_age(int value) const { return m_age + value; }

    int m_age{};
};

*/

}  // namespace test_class

REFLB_MODULE(test_class, test_class)
