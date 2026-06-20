#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <algorithm>
#include <array>
#include <functional>
#include <meta>
#include <optional>
#include <print>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "function_invoker.hpp"
#include "function_traits.hpp"

namespace rebind {

// The method counts number of all members methods.
template <auto R>
inline consteval size_t numOfMembers() noexcept {
    static constexpr auto ctx = std::meta::access_context::unprivileged();

    return std::ranges::distance(std::meta::members_of(R, ctx));
}

// ========================
// Free function reflection
// ========================

class CallableBase {
public:
    PyObject* (*invoke)(const CallableBase*, PyObject* args, PyObject* kwargs);
};

template <typename Fn>
class CallableInfo final : public CallableBase {
    using traits = function_traits<Fn>;
    using return_type = typename traits::return_type;
    using args_tuple = typename traits::args_tuple;

public:
    constexpr explicit CallableInfo(std::string_view fnName, Fn f) noexcept : name{fnName}, fn{f} {
        this->invoke = &this->pyWrapperThunk;
    }

    static PyObject* pyWrapperThunk(const CallableBase* base, PyObject* args, PyObject* kwargs) noexcept {
        return static_cast<const CallableInfo<Fn>*>(base)->pyWrapper(args, kwargs);
    }

    [[nodiscard]] constexpr std::string_view getName() const noexcept { return name; }

    [[nodiscard]] std::string_view getDoc() const noexcept { return doc; }

private:
    PyObject* pyWrapper(PyObject* args, PyObject* /*kwargs*/) const noexcept {
        return invokePythonCallable<args_tuple, return_type>(args, [this](auto&&... converted_args) -> decltype(auto) {
            return this->fn(std::forward<decltype(converted_args)>(converted_args)...);
        });
    }

    std::string_view name{};
    std::string_view doc{"doc"};
    Fn fn;
};

template <typename Fn>
CallableInfo(std::string_view, Fn) -> CallableInfo<Fn>;

// The method creates CallableInfo from a member function of a namespace(R) at I index.
template <std::meta::info R, size_t I>
inline consteval auto getFunction() noexcept {
    static constexpr auto ctx = std::meta::access_context::unprivileged();
    constexpr auto members = std::define_static_array(std::meta::members_of(R, ctx));

    if constexpr (std::meta::is_function(members[I])) {
        constexpr std::string_view name = std::meta::identifier_of(members[I]);
        return std::make_tuple(CallableInfo{name, &[:members[I]:]});
    } else {
        return std::tuple<>();
    }
}

template <std::meta::info R, size_t... I>
inline consteval auto collectFunctionsImpl(std::index_sequence<I...>) noexcept {
    return std::tuple_cat(getFunction<R, I>()...);
}

template <std::meta::info R>
[[nodiscard]] inline consteval auto collectFunctions() noexcept {
    return collectFunctionsImpl<R>(std::make_index_sequence<numOfMembers<R>()>{});
}

// =======================
// Struct/Class reflection
// =======================

template <typename T>
struct PyClassWrapper {
    PyObject_HEAD  // new-line
        std::optional<T>
            cpp_class;

    static PyObject* allocate(PyTypeObject* o, PyObject* args, PyObject* kwds) {
        auto* self = reinterpret_cast<PyClassWrapper<T>*>(o->tp_alloc(o, 0));

        if (!self) {
            return nullptr;
        }

        std::construct_at(&self->cpp_class);

        return reinterpret_cast<PyObject*>(self);
    }

    static void deallocate(PyObject* o) {
        auto* self = reinterpret_cast<PyClassWrapper<T>*>(o);
        std::destroy_at(&self->cpp_class);
        Py_TYPE(o)->tp_free(o);
    }
};

template <size_t NMethods>
struct ClassDescriptor {
    PyTypeObject type{};
    std::array<PyMethodDef, NMethods + 1> methods{};  //< +1 for sentinel object.
};

template <typename T>
inline consteval auto makeClassDescriptor(T&& methodsTuple) {
    constexpr size_t methodsCount = std::tuple_size_v<std::remove_cvref_t<T>>;
    ClassDescriptor<methodsCount> desc{};

    // Assign methods from the tuple.
    std::apply(
        [&desc](auto... method_def) {
            size_t method_id{};
            ((desc.methods[method_id++] = method_def), ...);
        },
        methodsTuple
    );

    // Fill sentinel method
    desc.methods[methodsCount] = PyMethodDef{.ml_name = nullptr, .ml_meth = nullptr, .ml_flags = 0, .ml_doc = nullptr};

    return desc;
}

template <std::meta::info R, typename Wrapper, size_t I>
inline consteval auto getMethodDef() noexcept {
    static constexpr auto ctx = std::meta::access_context::unprivileged();
    constexpr auto members = std::define_static_array(std::meta::members_of(R, ctx));

    if constexpr (std::meta::is_function(members[I]) && std::meta::is_public(members[I]) &&
                  !std::meta::is_static_member(members[I]) && !std::meta::is_special_member_function(members[I]) &&
                  !std::meta::is_constructor(members[I]))
    {
        return std::make_tuple(
            PyMethodDef{
                .ml_name = std::meta::identifier_of(members[I]).data(),
                .ml_meth = MethodInvoker<&[:members[I]:], Wrapper>::invoke,
                .ml_flags = METH_VARARGS,
                .ml_doc = "doc",  //< TODO: meaningful doc.
            }
        );
    } else {
        return std::tuple<>();
    }
}

template <std::meta::info R, typename Wrapper, size_t... I>
inline consteval auto collectMethodDefsImpl(std::index_sequence<I...>) noexcept {
    return std::tuple_cat(getMethodDef<R, Wrapper, I>()...);
}

// Returns tuple of PyMethodDef. TODO: replace with std::array/std::inplace_vector?
template <std::meta::info C, typename Wrapper>
inline consteval auto collectMethodDefs() noexcept {
    return collectMethodDefsImpl<C, Wrapper>(std::make_index_sequence<numOfMembers<C>()>{});
}

template <std::meta::info R>
consteval auto getTypesOfParameters() {
    constexpr auto params = std::define_static_array(std::meta::parameters_of(R));

    return [params]<size_t... I>(std::index_sequence<I...>) {
        return std::tuple<typename[:std::meta::type_of(params[I]):]...>{};
    }(std::make_index_sequence<params.size()>());
}

template <std::meta::info C, typename Wrapper>
inline consteval auto getConstructorInvoker() {
    static constexpr auto ctx = std::meta::access_context::unprivileged();

    // TODO: use ranges
    template for (constexpr auto m : std::define_static_array(std::meta::members_of(C, ctx))) {
        if constexpr (std::meta::is_public(m) && std::meta::is_constructor(m) && !std::meta::is_copy_constructor(m) &&
                      !std::meta::is_move_constructor(m))
        {
            auto argTypes = getTypesOfParameters<m>();
            return &ConstructorInvoker<decltype(argTypes), Wrapper>::invoke;
        }
    }
}

template <std::meta::info C>
inline consteval auto reflect_class() noexcept {
    using pywrapper_t = PyClassWrapper<typename[:C:]>;

    const std::string_view class_name = std::meta::identifier_of(C);

    constexpr auto methodsDefs = collectMethodDefs<C, pywrapper_t>();
    auto desc = makeClassDescriptor(methodsDefs);
    // TODO: init ob_base via Python macros?
    desc.type.ob_base.ob_base.ob_refcnt = 1;
    desc.type.ob_base.ob_base.ob_type = nullptr;
    desc.type.ob_base.ob_size = 0;

    desc.type.tp_name = class_name.data();  //< TODO: include module name.
    desc.type.tp_basicsize = sizeof(pywrapper_t);
    desc.type.tp_itemsize = 0;
    desc.type.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    desc.type.tp_doc = PyDoc_STR(class_name.data());

    desc.type.tp_new = pywrapper_t::allocate;                     //< Note: allocates a new Python wrapper object/
    desc.type.tp_init = getConstructorInvoker<C, pywrapper_t>();  //< Note: can be invoked multiple times.
    desc.type.tp_dealloc = pywrapper_t::deallocate;
    desc.type.tp_methods = nullptr;  //< Note: assigned at runtime due to consteval rules.

    return desc;
}

// =======================
// Reflection
// =======================

template <auto R, size_t I>
inline consteval auto getClass() noexcept {
    static constexpr auto ctx = std::meta::access_context::unprivileged();
    constexpr auto members = std::define_static_array(std::meta::members_of(R, ctx));

    if constexpr (!std::meta::is_variable(members[I]) && !std::meta::is_function(members[I]) &&
                  std::meta::is_class_type(members[I]))
    {
        return std::make_tuple(reflect_class<members[I]>());
    } else {
        return std::tuple<>();
    }
}

template <std::meta::info R, size_t... I>
inline consteval auto collectClassesImpl(std::index_sequence<I...>) noexcept {
    return std::tuple_cat(getClass<R, I>()...);
}

template <std::meta::info R>
inline consteval auto reflectClasses() noexcept {
    return collectClassesImpl<R>(std::make_index_sequence<numOfMembers<R>()>{});
}

template <typename FunctionTuple, typename ClassTuple>
struct Entities {
    FunctionTuple functions;
    ClassTuple classes;
};

template <std::meta::info N>
inline consteval auto reflect() {
    auto functions = collectFunctions<N>();
    auto classes = reflectClasses<N>();
    Entities<std::remove_cvref_t<decltype(functions)>, std::remove_cvref_t<decltype(classes)>> entities{
        .functions = functions,
        .classes = classes,
    };
    return entities;
}

}  // namespace rebind
