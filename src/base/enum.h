#pragma once

#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <string_view>

// -- enum to string based on
// https://gist.github.com/zhmu/9ac375706ffbafa5d24693f8475abd79

// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Rink Springer <rink@rink.nu>
//
// Based on
// https://github.com/Neargye/magic_enum/blob/master/include/magic_enum.hpp
// Copyright (c) 2019 - 2022 Daniil Goncharov <neargye@gmail.com>.
//
// Permission is hereby  granted, free of charge, to any  person obtaining a
// copy of this software and associated  documentation files (the "Software"),
// to deal in the Software  without restriction, including without  limitation
// the rights to  use, copy,  modify, merge,  publish, distribute,  sublicense,
// and/or  sell copies  of  the Software,  and  to  permit persons  to  whom the
// Software  is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS
// OR IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN
// NO EVENT  SHALL THE AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY
// CLAIM,  DAMAGES OR  OTHER LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT
// OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

namespace magic_enum {

constexpr auto ENUM_MIN_VALUE = -128;
constexpr auto ENUM_MAX_VALUE = 128;

template <std::size_t N>
struct static_string {
  constexpr static_string(std::string_view sv) noexcept {
    // std::copy() is not constexpr in C++17, hence...
    for (std::size_t n = 0; n < N; ++n) content[n] = sv[n];
  }
  constexpr operator std::string_view() const noexcept {
    return {content.data(), N};
  }

 private:
  std::array<char, N + 1> content{};
};

constexpr auto is_pretty(char ch) noexcept {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

constexpr auto pretty_name(std::string_view sv) noexcept {
  for (std::size_t n = sv.size() - 1; n > 0; --n) {
    if (!is_pretty(sv[n])) {
      sv.remove_prefix(n + 1);
      break;
    }
  }
  return sv;
}

template <typename E, E V>
constexpr auto n() noexcept {
#if defined(__GNUC__) || defined(__clang__)
  return pretty_name({__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__) - 2});
#elif defined(_MSC_VER)
  return pretty_name({__FUNCSIG__, sizeof(__FUNCSIG__) - 17});
#endif
}

template <typename E, E V>
constexpr auto is_valid() {
  constexpr E v = static_cast<E>(V);
  return !n<E, V>().empty();
}

template <typename E>
constexpr auto value(std::size_t v) {
  return static_cast<E>(ENUM_MIN_VALUE + v);
}

template <std::size_t N>
constexpr auto count_values(const bool (&valid)[N]) {
  std::size_t count = 0;
  for (std::size_t n = 0; n < N; ++n)
    if (valid[n]) ++count;
  return count;
}

template <typename E, std::size_t... I>
constexpr auto values(std::index_sequence<I...>) noexcept {
  constexpr bool valid[sizeof...(I)] = {is_valid<E, value<E>(I)>()...};
  constexpr auto num_valid = count_values(valid);
  static_assert(num_valid > 0, "no support for empty enums");

  std::array<E, num_valid> values = {};
  for (std::size_t offset = 0, n = 0; n < num_valid; ++offset) {
    if (valid[offset]) {
      values[n] = value<E>(offset);
      ++n;
    }
  }

  return values;
}

template <typename E>
constexpr auto values() noexcept {
  constexpr auto enum_size = ENUM_MAX_VALUE - ENUM_MIN_VALUE + 1;
  return values<E>(std::make_index_sequence<enum_size>({}));
}

template <typename E>
inline constexpr auto values_v = values<E>();

template <typename E, E V>
constexpr auto enum_name() {
  constexpr auto name = n<E, V>();
  return static_string<name.size()>(name);
}

template <typename E, E V>
inline constexpr auto enum_name_v = enum_name<E, V>();

template <typename E, std::size_t... I>
constexpr auto entries(std::index_sequence<I...>) noexcept {
  return std::array<std::pair<E, std::string_view>, sizeof...(I)>{
      {{values_v<E>[I], enum_name_v<E, values_v<E>[I]>}...}};
}

template <typename E>
inline constexpr auto entries_v =
    entries<E>(std::make_index_sequence<values_v<E>.size()>());

template <typename E>
constexpr std::string_view enum_to_string(E value) {
  for (const auto& [key, name] : entries_v<E>) {
    if (value == key) return name;
  }
  return {};
}

}  // namespace magic_enum