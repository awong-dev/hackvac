#ifndef ESPCXX_CXX17HACK_H_
#define ESPCXX_CXX17HACK_H_

// This Esp-idf toolchain uses the crosstoll gcc-5.2 for xtensa which has
// partial C++14/17 support. This support is good enough for most uses,
// but annoying, some of the standard library features are still in
// std::experimental. As the toolchain is just about to be upgraded to
// gcc-8.2 which will have nerarly complete C++14/17 support, this header
// provides a hack to move some of the more useful standard library upgrades
// into namespace std. While technically undefined behavior, it works fine
// for this specific case. Yay for having too many C++ porting scars. Thanks
// Chrome.

#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 8

#include <experimental/optional>
#include <experimental/string_view>

namespace std {
template <typename T> using optional = experimental::optional<T>;
using string_view = experimental::string_view;
}  // namespace std

#else

#include <optional>
#include <string_view>

#endif

#endif  // ESPCXX_CXX17HACK_H_
