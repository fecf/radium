#pragma once

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <cctype>
#include <cwctype>
#include <functional>
#include <vector>

namespace rad {

template <typename T>
float scale_to_fit(T image_w, T image_h, T viewport_w, T viewport_h) {
  float aspect_ratio = (float)image_w / image_h;
  float viewport_aspect_ratio = (float)viewport_w / viewport_h;
  if (aspect_ratio > viewport_aspect_ratio) {
    return (float)viewport_w / image_w;
  } else {
    return (float)viewport_h / image_h;
  }
}

template <typename T, typename V>
T::const_iterator find_next_element_by_value(const T& container, V value) {
  assert(!container.empty());

  auto it = std::upper_bound(container.begin(), container.end(), value);
  if (it == container.end()) {
    return std::prev(it);
  }
  return it;
}

template <typename T, typename V>
T::const_iterator find_prev_element_by_value(const T& container, V value) {
  assert(!container.empty());

  auto it = std::upper_bound(
      container.rbegin(), container.rend(), value, std::greater<V>())
                .base();
  if (it == container.begin()) {
    return it;
  }
  return std::prev(it);
}

template <typename T, typename V>
T::const_iterator find_nearest_element_by_value(const T& container, V value) {
  assert(!container.empty());

  auto it = std::find(container.begin(), container.end(), value);
  if (it == container.end()) {
    return it;
  }

  auto next = find_next_element_by_value(container, value);
  V next_diff = {};
  if (next != container.end()) {
    next_diff = std::abs(value - *next);
  }

  auto prev = find_prev_element_by_value(container, value);
  V prev_diff = {};
  if (prev != container.end()) {
    prev_diff = std::abs(value - *prev);
  }

  if (next_diff < prev_diff) {
    return next;
  } else {
    return prev;
  }

  throw std::domain_error("container is empty.");
}

template<typename T> T wrap(T v, T delta, T min, T max) {
  int mod = max + 1 - min;
  v += delta - min;
  v += (1 - v / mod) * mod;
  return v % mod + min;
}

template <typename T, typename U>
constexpr size_t offset_of(U T::*member) {
  return (char*)&((T*)nullptr->*member) - (char*)nullptr;
}
template <class T>
struct remove_member_pointer {
  typedef T type;
};
template <class C, class T>
struct remove_member_pointer<T C::*> {
  typedef T type;
};

namespace natural_sort {

bool sort(const std::wstring& a, const std::wstring& b);

}

}  // namespace rad
