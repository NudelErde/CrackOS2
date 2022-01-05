#pragma once

namespace std {

template<typename T>
struct remove_reference { typedef T type; };
template<typename T>
struct remove_reference<T&> { typedef T type; };
template<typename T>
struct remove_reference<T&&> { typedef T type; };

template<typename T>
constexpr typename remove_reference<T>::type&& move(T&& arg) noexcept {
    return static_cast<typename remove_reference<T>::type&&>(arg);
}


template<typename T>
struct is_lvalue_reference { static constexpr bool value = false; };
template<typename T>
struct is_lvalue_reference<T&> { static constexpr bool value = true; };

template<typename T>
constexpr T&& forward(typename remove_reference<T>::type& arg) noexcept {
    return static_cast<T&&>(arg);
}

template<typename T>
constexpr T&& forward(typename remove_reference<T>::type&& arg) noexcept {
    static_assert(!is_lvalue_reference<T>::value, "invalid rvalue to lvalue conversion");
    return static_cast<T&&>(arg);
}

}// namespace std


template<typename T, T v>
struct integral_constant {
    static constexpr T value = v;
    constexpr operator T() const noexcept { return value; }
};

template<typename T, T v>
constexpr T integral_constant<T, v>::value;

/// The type used as a compile-time boolean with true value.
typedef integral_constant<bool, true> true_type;

/// The type used as a compile-time boolean with false value.
typedef integral_constant<bool, false> false_type;
