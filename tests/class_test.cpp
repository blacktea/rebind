#include "rebind/rebind.hpp"

namespace class_test {

struct TestClass {
    int age{};
};

}  // namespace class_test

REFLB_MODULE(class_test, class_test)
