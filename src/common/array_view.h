/*
 * @Author: DI JUNKUN
 * @Date: 2024-12-18
 * Copyright 2018 The WebRTC project authors. All Rights Reserved.
 */

#ifndef _ARRAY_VIEW_H_
#define _ARRAY_VIEW_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <type_traits>

#include "type_traits.h"

namespace array_view_internal {

// Magic constant for indicating that the size of an ArrayView is variable
// instead of fixed.
enum : std::ptrdiff_t { kArrayViewVarSize = -4711 };

// Base class for ArrayViews of fixed nonzero size.
template <typename T, std::ptrdiff_t Size>
class ArrayViewBase {
  static_assert(Size > 0, "ArrayView size must be variable or non-negative");

 public:
  ArrayViewBase(T* data, size_t /* size */) : data_(data) {}

  static constexpr size_t size() { return Size; }
  static constexpr bool empty() { return false; }
  T* data() const { return data_; }

 protected:
  static constexpr bool fixed_size() { return true; }

 private:
  T* data_;
};

// Specialized base class for ArrayViews of fixed zero size.
template <typename T>
class ArrayViewBase<T, 0> {
 public:
  explicit ArrayViewBase(T* /* data */, size_t /* size */) {}

  static constexpr size_t size() { return 0; }
  static constexpr bool empty() { return true; }
  T* data() const { return nullptr; }

 protected:
  static constexpr bool fixed_size() { return true; }
};

// Specialized base class for ArrayViews of variable size.
template <typename T>
class ArrayViewBase<T, array_view_internal::kArrayViewVarSize> {
 public:
  ArrayViewBase(T* data, size_t size)
      : data_(size == 0 ? nullptr : data), size_(size) {}

  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  T* data() const { return data_; }

 protected:
  static constexpr bool fixed_size() { return false; }

 private:
  T* data_;
  size_t size_;
};

}  // namespace array_view_internal

template <typename T,
          std::ptrdiff_t Size = array_view_internal::kArrayViewVarSize>
class ArrayView final : public array_view_internal::ArrayViewBase<T, Size> {
 public:
  using value_type = T;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using const_iterator = const T*;

  // Construct an ArrayView from a pointer and a length.
  template <typename U>
  ArrayView(U* data, size_t size)
      : array_view_internal::ArrayViewBase<T, Size>::ArrayViewBase(data, size) {
    RTC_DCHECK_EQ(size == 0 ? nullptr : data, this->data());
    RTC_DCHECK_EQ(size, this->size());
    RTC_DCHECK_EQ(!this->data(),
                  this->size() == 0);  // data is null iff size == 0.
  }

  // Construct an empty ArrayView. Note that fixed-size ArrayViews of size > 0
  // cannot be empty.
  ArrayView() : ArrayView(nullptr, 0) {}
  ArrayView(std::nullptr_t)  // NOLINT
      : ArrayView() {}
  ArrayView(std::nullptr_t, size_t size)
      : ArrayView(static_cast<T*>(nullptr), size) {
    static_assert(Size == 0 || Size == array_view_internal::kArrayViewVarSize,
                  "");
    RTC_DCHECK_EQ(0, size);
  }

  // Construct an ArrayView from a C-style array.
  template <typename U, size_t N>
  ArrayView(U (&array)[N])  // NOLINT
      : ArrayView(array, N) {
    static_assert(Size == N || Size == array_view_internal::kArrayViewVarSize,
                  "Array size must match ArrayView size");
  }

  // (Only if size is fixed.) Construct a fixed size ArrayView<T, N> from a
  // non-const std::array instance. For an ArrayView with variable size, the
  // used ctor is ArrayView(U& u) instead.
  template <typename U, size_t N,
            typename std::enable_if<
                Size == static_cast<std::ptrdiff_t>(N)>::type* = nullptr>
  ArrayView(std::array<U, N>& u)  // NOLINT
      : ArrayView(u.data(), u.size()) {}

  // (Only if size is fixed.) Construct a fixed size ArrayView<T, N> where T is
  // const from a const(expr) std::array instance. For an ArrayView with
  // variable size, the used ctor is ArrayView(U& u) instead.
  template <typename U, size_t N,
            typename std::enable_if<
                Size == static_cast<std::ptrdiff_t>(N)>::type* = nullptr>
  ArrayView(const std::array<U, N>& u)  // NOLINT
      : ArrayView(u.data(), u.size()) {}

  // (Only if size is fixed.) Construct an ArrayView from any type U that has a
  // static constexpr size() method whose return value is equal to Size, and a
  // data() method whose return value converts implicitly to T*. In particular,
  // this means we allow conversion from ArrayView<T, N> to ArrayView<const T,
  // N>, but not the other way around. We also don't allow conversion from
  // ArrayView<T> to ArrayView<T, N>, or from ArrayView<T, M> to ArrayView<T,
  // N> when M != N.
  template <typename U, typename std::enable_if<
                            Size != array_view_internal::kArrayViewVarSize &&
                            HasDataAndSize<U, T>::value>::type* = nullptr>
  ArrayView(U& u)  // NOLINT
      : ArrayView(u.data(), u.size()) {
    static_assert(U::size() == Size, "Sizes must match exactly");
  }
  template <typename U, typename std::enable_if<
                            Size != array_view_internal::kArrayViewVarSize &&
                            HasDataAndSize<U, T>::value>::type* = nullptr>
  ArrayView(const U& u)  // NOLINT(runtime/explicit)
      : ArrayView(u.data(), u.size()) {
    static_assert(U::size() == Size, "Sizes must match exactly");
  }

  // (Only if size is variable.) Construct an ArrayView from any type U that
  // has a size() method whose return value converts implicitly to size_t, and
  // a data() method whose return value converts implicitly to T*. In
  // particular, this means we allow conversion from ArrayView<T> to
  // ArrayView<const T>, but not the other way around. Other allowed
  // conversions include
  // ArrayView<T, N> to ArrayView<T> or ArrayView<const T>,
  // std::vector<T> to ArrayView<T> or ArrayView<const T>,
  // const std::vector<T> to ArrayView<const T>,
  // rtc::Buffer to ArrayView<uint8_t> or ArrayView<const uint8_t>, and
  // const rtc::Buffer to ArrayView<const uint8_t>.
  template <typename U, typename std::enable_if<
                            Size == array_view_internal::kArrayViewVarSize &&
                            HasDataAndSize<U, T>::value>::type* = nullptr>
  ArrayView(U& u)  // NOLINT
      : ArrayView(u.data(), u.size()) {}
  template <typename U, typename std::enable_if<
                            Size == array_view_internal::kArrayViewVarSize &&
                            HasDataAndSize<U, T>::value>::type* = nullptr>
  ArrayView(const U& u)  // NOLINT(runtime/explicit)
      : ArrayView(u.data(), u.size()) {}

  // Indexing and iteration. These allow mutation even if the ArrayView is
  // const, because the ArrayView doesn't own the array. (To prevent mutation,
  // use a const element type.)
  T& operator[](size_t idx) const {
    RTC_DCHECK_LT(idx, this->size());
    RTC_DCHECK(this->data());
    return this->data()[idx];
  }
  T* begin() const { return this->data(); }
  T* end() const { return this->data() + this->size(); }
  const T* cbegin() const { return this->data(); }
  const T* cend() const { return this->data() + this->size(); }
  std::reverse_iterator<T*> rbegin() const {
    return std::make_reverse_iterator(end());
  }
  std::reverse_iterator<T*> rend() const {
    return std::make_reverse_iterator(begin());
  }
  std::reverse_iterator<const T*> crbegin() const {
    return std::make_reverse_iterator(cend());
  }
  std::reverse_iterator<const T*> crend() const {
    return std::make_reverse_iterator(cbegin());
  }

  ArrayView<T> subview(size_t offset, size_t size) const {
    return offset < this->size()
               ? ArrayView<T>(this->data() + offset,
                              std::min(size, this->size() - offset))
               : ArrayView<T>();
  }
  ArrayView<T> subview(size_t offset) const {
    return subview(offset, this->size());
  }
};

// Comparing two ArrayViews compares their (pointer,size) pairs; it does *not*
// dereference the pointers.
template <typename T, std::ptrdiff_t Size1, std::ptrdiff_t Size2>
bool operator==(const ArrayView<T, Size1>& a, const ArrayView<T, Size2>& b) {
  return a.data() == b.data() && a.size() == b.size();
}
template <typename T, std::ptrdiff_t Size1, std::ptrdiff_t Size2>
bool operator!=(const ArrayView<T, Size1>& a, const ArrayView<T, Size2>& b) {
  return !(a == b);
}

// Variable-size ArrayViews are the size of two pointers; fixed-size ArrayViews
// are the size of one pointer. (And as a special case, fixed-size ArrayViews
// of size 0 require no storage.)
static_assert(sizeof(ArrayView<int>) == 2 * sizeof(int*), "");
static_assert(sizeof(ArrayView<int, 17>) == sizeof(int*), "");
static_assert(std::is_empty<ArrayView<int, 0>>::value, "");

template <typename T>
inline ArrayView<T> MakeArrayView(T* data, size_t size) {
  return ArrayView<T>(data, size);
}

// Only for primitive types that have the same size and aligment.
// Allow reinterpret cast of the array view to another primitive type of the
// same size.
// Template arguments order is (U, T, Size) to allow deduction of the template
// arguments in client calls: reinterpret_array_view<target_type>(array_view).
template <typename U, typename T, std::ptrdiff_t Size>
inline ArrayView<U, Size> reinterpret_array_view(ArrayView<T, Size> view) {
  static_assert(sizeof(U) == sizeof(T) && alignof(U) == alignof(T),
                "ArrayView reinterpret_cast is only supported for casting "
                "between views that represent the same chunk of memory.");
  static_assert(
      std::is_fundamental<T>::value && std::is_fundamental<U>::value,
      "ArrayView reinterpret_cast is only supported for casting between "
      "fundamental types.");
  return ArrayView<U, Size>(reinterpret_cast<U*>(view.data()), view.size());
}

#endif