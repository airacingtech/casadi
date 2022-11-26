#pragma once

#include <alpaqa/config/config.hpp>
#include <alpaqa/inner/directions/panoc-direction-update.hpp>
#include <alpaqa/inner/directions/panoc/lbfgs.hpp>
#include <alpaqa/util/type-erasure.hpp>

#include "kwargs-to-struct.hpp"

#include <new>
#include <string>
#include <utility>

#include <pybind11/pytypes.h>
namespace py = pybind11;

namespace alpaqa {

template <Config Conf>
struct PANOCDirectionVTable : util::BasicVTable {
    USING_ALPAQA_CONFIG(Conf);
    using Problem = TypeErasedProblem<config_t>;

    // clang-format off
    required_function_t<void(const Problem &problem, crvec y, crvec Σ, real_t γ_0, crvec x_0, crvec x̂_0, crvec p_0, crvec grad_ψx_0)>
        initialize = nullptr;
    required_function_t<bool(real_t γₖ, real_t γₙₑₓₜ, crvec xₖ, crvec xₙₑₓₜ, crvec pₖ, crvec pₙₑₓₜ, crvec grad_ψxₖ, crvec grad_ψxₙₑₓₜ)>
        update = nullptr;
    required_const_function_t<bool()>
        has_initial_direction = nullptr;
    required_const_function_t<bool(real_t γₖ, crvec xₖ, crvec x̂ₖ, crvec pₖ, crvec grad_ψxₖ, rvec qₖ)>
        apply = nullptr;
    required_function_t<void(real_t γₖ, real_t old_γₖ)>
        changed_γ = nullptr;
    required_function_t<void()>
        reset = nullptr;
    required_const_function_t<py::object()>
        get_params = nullptr;
    required_const_function_t<std::string()>
        get_name = nullptr;
    // clang-format on

    template <class T>
    PANOCDirectionVTable(util::VTableTypeTag<T> t) : util::BasicVTable{t} {
        initialize            = util::type_erased_wrapped<&T::initialize>();
        update                = util::type_erased_wrapped<&T::update>();
        has_initial_direction = util::type_erased_wrapped<&T::has_initial_direction>();
        apply                 = util::type_erased_wrapped<&T::apply>();
        changed_γ             = util::type_erased_wrapped<&T::changed_γ>();
        reset                 = util::type_erased_wrapped<&T::reset>();
        get_params            = util::type_erased_wrapped<&T::get_params>();
        get_name              = util::type_erased_wrapped<&T::get_name>();
    }
    PANOCDirectionVTable() = default;
};

template <Config Conf>
constexpr size_t te_pd_buff_size = util::required_te_buffer_size_for<LBFGSDirection<Conf>>();

template <Config Conf = DefaultConfig, class Allocator = std::allocator<std::byte>>
class TypeErasedPANOCDirection
    : public util::TypeErased<PANOCDirectionVTable<Conf>, Allocator, te_pd_buff_size<Conf>> {
  public:
    USING_ALPAQA_CONFIG(Conf);
    using VTable         = PANOCDirectionVTable<Conf>;
    using allocator_type = Allocator;
    using TypeErased     = util::TypeErased<VTable, allocator_type, te_pd_buff_size<Conf>>;
    using TypeErased::TypeErased;
    using Problem = TypeErasedProblem<config_t>;

  private:
    using TypeErased::call;
    using TypeErased::self;
    using TypeErased::vtable;

  public:
    template <class T, class... Args>
    static TypeErasedPANOCDirection make(Args &&...args) {
        return TypeErased::template make<TypeErasedPANOCDirection, T>(std::forward<Args>(args)...);
    }

    template <class... Args>
    decltype(auto) initialize(Args &&...args) {
        return call(vtable.initialize, std::forward<Args>(args)...);
    }
    template <class... Args>
    decltype(auto) update(Args &&...args) {
        return call(vtable.update, std::forward<Args>(args)...);
    }
    template <class... Args>
    decltype(auto) has_initial_direction(Args &&...args) const {
        return call(vtable.has_initial_direction, std::forward<Args>(args)...);
    }
    template <class... Args>
    decltype(auto) apply(Args &&...args) const {
        return call(vtable.apply, std::forward<Args>(args)...);
    }
    template <class... Args>
    decltype(auto) changed_γ(Args &&...args) {
        return call(vtable.changed_γ, std::forward<Args>(args)...);
    }
    template <class... Args>
    decltype(auto) reset(Args &&...args) {
        return call(vtable.reset, std::forward<Args>(args)...);
    }
    template <class... Args>
    decltype(auto) get_params(Args &&...args) const {
        return call(vtable.get_params, std::forward<Args>(args)...);
    }
    template <class... Args>
    decltype(auto) get_name(Args &&...args) const {
        return call(vtable.get_name, std::forward<Args>(args)...);
    }
};



template <class T, class... Args>
auto erase_direction(Args &&...args) {
    return TypeErasedPANOCDirection<typename T::config_t>::template make<T>(
        std::forward<Args>(args)...);
}

namespace detail {
template <class P>
py::object to_dict_tup(const P &params) {
    return struct_to_dict(params);
}
template <class... Ps>
py::object to_dict_tup(const std::tuple<Ps...> tup) {
    return py::cast([&]<size_t... Is>(std::index_sequence<Is...>) {
        return std::make_tuple(to_dict_tup(std::get<Is>(tup))...);
    }(std::make_index_sequence<sizeof...(Ps)>()));
}
inline py::object to_dict_tup(py::object o) { return std::move(o); }
} // namespace detail

template <class T, class... Args>
auto erase_direction_with_params_dict(Args &&...args) {
    struct S : T {
        using T::T;
        S(const T &d) : T{d} {}
        S(T &&d) : T{std::move(d)} {}
        py::object get_params() const { return detail::to_dict_tup(T::get_params()); }
    };
    return TypeErasedPANOCDirection<typename T::config_t>::template make<S>(
        std::forward<Args>(args)...);
}

} // namespace alpaqa