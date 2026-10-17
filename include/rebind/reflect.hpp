#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <algorithm>
#include <array>
#include <functional>
#include <inplace_vector>
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
// Function reflection
// ========================

template <typename F, size_t ...I>
constexpr auto for_each_index_sequence(F&& f, std::index_sequence<I...>) {
    constexpr bool allVoid = (std::is_void_v<decltype(std::forward<F>(f).template operator()<I>())> && ...);

    if constexpr (allVoid) {
        (std::forward<F>(f).template operator()<I>(), ...);
    } else {
        auto wrap = [&f]<size_t Index>() {
            using RetType = decltype(f.template operator()<Index>());
            if constexpr (std::is_void_v<RetType>) {
                f.template operator()<Index>();
                return std::tuple<>{};
            } else {
                return std::make_tuple(f.template operator()<Index>());
            }
        };
        return std::tuple_cat(wrap.template operator()<I>()...);
    }
}

// TODO: rename class name. add comments.
template<typename FunctionInvokeTuple>
class CallableInfoCollection {
public:
    constexpr explicit CallableInfoCollection(std::string_view name) 
    : m_name{name} {}

    static PyObject* invoke(PyObject* self, PyObject* args) noexcept {
        try {
            PyObject *result{nullptr};

            for_each_index_sequence([&result, self, args]<size_t I>() {
                try {
                    if (!result) {
                        result = std::tuple_element_t<I, FunctionInvokeTuple>::invoke(self, args);
                    }
                // TODO: catch specialized exception denoting argument incompatibility.
                } catch (...) {}
            }, std::make_index_sequence<std::tuple_size_v<FunctionInvokeTuple>>{});
            
            return result != nullptr ? result : Py_None;
        }
        catch(...) {
            return Py_None;
        }
    }

    [[nodiscard]] constexpr std::string_view getName() const noexcept { return m_name; }

    [[nodiscard]] constexpr std::string_view getDoc() const noexcept { return m_doc; }

private:
    std::string_view m_name{};
    std::string_view m_doc{"doc"};
};

// Helper function used to determine whether the function overloads others.
template<std::meta::info R, std::size_t I>
inline consteval bool isFirstFunctionWithThisName()
{
    constexpr auto ctx =
        std::meta::access_context::unprivileged();

    constexpr auto members =
        std::define_static_array(std::meta::members_of(R, ctx));

    constexpr auto current = members[I];

    if constexpr (!std::meta::is_function(current)) {
        return false;
    } else {
        constexpr std::string_view currentName =
            std::meta::identifier_of(current);

        for (std::size_t j = 0; j < I; ++j) {
            const auto previous = members[j];

            if (std::meta::is_function(previous) &&
                std::meta::identifier_of(previous) == currentName)
            {
                return false;
            }
        }

        return true;
    }
}

template<
    std::meta::info R,
    std::meta::info Representative,
    std::size_t I>
inline consteval auto selectFunction()
{
    constexpr auto ctx =
        std::meta::access_context::unprivileged();

    constexpr auto members =
        std::define_static_array(std::meta::members_of(R, ctx));

    constexpr auto candidate = members[I];

    if constexpr (
        std::meta::is_function(candidate) &&
        std::meta::identifier_of(candidate) ==
            std::meta::identifier_of(Representative)
    ) {
        constexpr std::string_view name =
            std::meta::identifier_of(candidate);

        return FunctionInvoker<&[:candidate:]>{};
    }
}

template<
    std::meta::info R,
    std::meta::info Representative,
    size_t NMEMBERS
>
inline consteval auto makeFunctionCollection()
{
    auto functions = for_each_index_sequence([]<size_t I>() {
        return selectFunction<R, Representative, I>();
    }, std::make_index_sequence<NMEMBERS>{});

    return CallableInfoCollection<decltype(functions)>{
        std::meta::identifier_of(Representative)
    };
}

// Discovers functions and creates callable objects.
template<std::meta::info R>
inline consteval auto collectFunctions()
{
    constexpr auto ctx =
        std::meta::access_context::unprivileged();
    constexpr auto members =
        std::define_static_array(std::meta::members_of(R, ctx));

    constexpr size_t membersCount = members.size();

    std::inplace_vector<PyMethodDef, membersCount> functions;

    for_each_index_sequence([&functions, &members]<size_t I>(){
        if constexpr (isFirstFunctionWithThisName<R, I>()) {
            constexpr auto representative = members[I];

            using FunctionCollection = decltype(makeFunctionCollection<R, representative, membersCount>());
            functions.push_back(PyMethodDef{
                .ml_name = std::meta::identifier_of(representative).data(),
                .ml_meth = FunctionCollection::invoke,
                .ml_flags = METH_VARARGS,
                .ml_doc = "doc",  //< TODO: meaningful doc.
            });
        }
    }, std::make_index_sequence<membersCount>{});
    
    return functions;
}

// =======================
// Struct/Class reflection
// =======================

template <typename T>
struct PyClassWrapper {
    PyObject_HEAD;
    std::optional<T> cpp_class;

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

template <size_t Capacity>
inline consteval auto makeClassDescriptor(const std::inplace_vector<PyMethodDef, Capacity>& methodDefs) {
    ClassDescriptor<Capacity> desc{};

    std::ranges::copy(methodDefs, desc.methods.begin());

    // Fill sentinel method
    desc.methods[methodDefs.size()] =
        PyMethodDef{.ml_name = nullptr, .ml_meth = nullptr, .ml_flags = 0, .ml_doc = nullptr};

    return desc;
}

template <std::meta::info R, typename Wrapper, size_t I, size_t Capacity>
inline consteval void appendMethodDef(std::inplace_vector<PyMethodDef, Capacity>& methodDefs) noexcept {
    static constexpr auto ctx = std::meta::access_context::unprivileged();
    constexpr auto members = std::define_static_array(std::meta::members_of(R, ctx));

    if constexpr (std::meta::is_function(members[I]) && std::meta::is_public(members[I]) &&
                  !std::meta::is_static_member(members[I]) && !std::meta::is_special_member_function(members[I]) &&
                  !std::meta::is_constructor(members[I]))
    {
        methodDefs.push_back(
            PyMethodDef{
                .ml_name = std::meta::identifier_of(members[I]).data(),
                .ml_meth = MethodInvoker<&[:members[I]:], Wrapper>::invoke,
                .ml_flags = METH_VARARGS,
                .ml_doc = "doc",  //< TODO: meaningful doc.
            }
        );
    }
}

template <std::meta::info R, typename Wrapper, size_t... I>
inline consteval auto collectMethodDefsImpl(std::index_sequence<I...>) noexcept {
    std::inplace_vector<PyMethodDef, sizeof...(I)> methodDefs;
    (appendMethodDef<R, Wrapper, I>(methodDefs), ...);
    return methodDefs;
}

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
