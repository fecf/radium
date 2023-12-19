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

// natural sort
// based on https://github.com/scopeInfinity/NaturalSort
/*
        The MIT License (MIT)
        Copyright (c) 2016 Gagan Kumar(scopeInfinity)
        Complete License at
   https://raw.githubusercontent.com/scopeInfinity/NaturalSort/master/LICENSE.md
*/
namespace detail {

inline bool natural_less(const wchar_t& lhs, const wchar_t& rhs) {
  return std::towlower(lhs) < std::towlower(rhs);
}

inline bool is_not_digit(const wchar_t& x) { return !std::iswdigit(x); }

template <typename Iterator>
struct comp_over_iterator {
  int operator()(const Iterator& lhs, const Iterator& rhs) const {
    if (natural_less(*lhs, *rhs)) return -1;
    if (natural_less(*rhs, *lhs)) return +1;
    return 0;
  }
};

template <typename Iterator>
struct compare_number {
 private:
  // If Number is Itself fractional Part
  int fractional(
      Iterator lhsBegin, Iterator lhsEnd, Iterator rhsBegin, Iterator rhsEnd) {
    while (lhsBegin < lhsEnd && rhsBegin < rhsEnd) {
      int local_compare = comp_over_iterator<Iterator>()(lhsBegin, rhsBegin);
      if (local_compare != 0) return local_compare;
      lhsBegin++;
      rhsBegin++;
    }
    while (lhsBegin < lhsEnd && *lhsBegin == L'0') lhsBegin++;
    while (rhsBegin < rhsEnd && *rhsBegin == L'0') rhsBegin++;
    if (lhsBegin == lhsEnd && rhsBegin != rhsEnd)
      return -1;
    else if (lhsBegin != lhsEnd && rhsBegin == rhsEnd)
      return +1;
    else  // lhsBegin==lhsEnd && rhsBegin==rhsEnd
      return 0;
  }
  int non_fractional(
      Iterator lhsBegin, Iterator lhsEnd, Iterator rhsBegin, Iterator rhsEnd) {
    // Skip Inital Zero's
    while (lhsBegin < lhsEnd && *lhsBegin == L'0') lhsBegin++;
    while (rhsBegin < rhsEnd && *rhsBegin == L'0') rhsBegin++;

    // Comparing By Length of Both String
    if (lhsEnd - lhsBegin < rhsEnd - rhsBegin) return -1;
    if (lhsEnd - lhsBegin > rhsEnd - rhsBegin) return +1;

    // Equal In length
    while (lhsBegin < lhsEnd) {
      int local_compare = comp_over_iterator<Iterator>()(lhsBegin, rhsBegin);
      if (local_compare != 0) return local_compare;
      lhsBegin++;
      rhsBegin++;
    }
    return 0;
  }

 public:
  int operator()(Iterator lhsBegin, Iterator lhsEnd, bool isFractionalPart1,
      Iterator rhsBegin, Iterator rhsEnd, bool isFractionalPart2) {
    if (isFractionalPart1 && !isFractionalPart2)
      return true;  // 0<num1<1 && num2>=1
    if (!isFractionalPart1 && isFractionalPart2)
      return false;  // 0<num2<1 && num1>=1

    // isFractionPart1 == isFactionalPart2
    if (isFractionalPart1)
      return fractional(lhsBegin, lhsEnd, rhsBegin, rhsEnd);
    else
      return non_fractional(lhsBegin, lhsEnd, rhsBegin, rhsEnd);
  }
};

}  // namespace detail

template <typename Iterator>
int compare3(const Iterator& lhsBegin, const Iterator& lhsEnd,
    const Iterator& rhsBegin, const Iterator& rhsEnd) {
  Iterator current1 = lhsBegin, current2 = rhsBegin;

  // Flag for Space Found Check
  bool flag_found_space1 = false, flag_found_space2 = false;

  while (current1 != lhsEnd && current2 != rhsEnd) {
    // Ignore More than One Continous Space
    /******************************************
    For HandlingComparision Like
            Hello   9
            Hello  10
            Hello 123
    ******************************************/
    while (flag_found_space1 && current1 != lhsEnd && std::iswspace(*current1))
      current1++;
    flag_found_space1 = false;
    if (std::iswspace(*current1)) flag_found_space1 = true;

    while (flag_found_space2 && current2 != rhsEnd && std::iswspace(*current2))
      current2++;
    flag_found_space2 = false;
    if (std::iswspace(*current2)) flag_found_space2 = true;

    if (!std::iswdigit(*current1) || !std::iswdigit(*current2)) {
      // Normal comparision if any of character is non digit character
      if (detail::natural_less(*current1, *current2)) return -1;
      if (detail::natural_less(*current2, *current1)) return 1;
      current1++;
      current2++;
    } else {
      /*********************************
      Capture Numeric Part of Both String
      and then using it to compare Both
      ***********************************/
      Iterator last_nondigit1 =
          std::find_if(current1, lhsEnd, detail::is_not_digit);
      Iterator last_nondigit2 =
          std::find_if(current2, rhsEnd, detail::is_not_digit);

      int result = detail::compare_number<Iterator>()(current1, last_nondigit1,
          (current1 > lhsBegin && *(current1 - 1) == L'.'), current2,
          last_nondigit2, (current2 > rhsBegin && *(current2 - 1) == L'.'));
      if (result != 0) return result;
      current1 = last_nondigit1;
      current2 = last_nondigit2;
    }
  }

  if (current1 == lhsEnd && current2 == rhsEnd) {
    return 0;
  } else {
    return current1 == lhsEnd ? -1 : 1;
  }
}

inline int compare(const std::wstring& first, const std::wstring& second) {
  return compare3(first.begin(), first.end(), second.begin(), second.end()) < 0;
}

}  // namespace rad
