#pragma once

#include <tuple>
#include <type_traits>

namespace rebind {
// Helper function to extract return type, arguments and its types of the
// function.
// TODO: use from std::meta. I didn't find any helpful method out there.
template <class T>
struct function_traits;

// 1) Plain function type: R(Args...)
template <class R, class... Args>
struct function_traits<R(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;

    static constexpr std::size_t arity = sizeof...(Args);

    template <std::size_t N>
    using arg = std::tuple_element_t<N, args_tuple>;
};

// 2) Function pointer: R(*)(Args...)
template <class R, class... Args>
struct function_traits<R (*)(Args...)> : function_traits<R(Args...)> {};

// 3) Function reference: R(&)(Args...)
template <class R, class... Args>
struct function_traits<R (&)(Args...)> : function_traits<R(Args...)> {};

}  // namespace rebind