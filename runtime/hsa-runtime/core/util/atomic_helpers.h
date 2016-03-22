////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
// 
// Copyright (c) 2014-2015, Advanced Micro Devices, Inc. All rights reserved.
// 
// Developed by:
// 
//                 AMD Research and AMD HSA Software Development
// 
//                 Advanced Micro Devices, Inc.
// 
//                 www.amd.com
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal with the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
// 
//  - Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimers.
//  - Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimers in
//    the documentation and/or other materials provided with the distribution.
//  - Neither the names of Advanced Micro Devices, Inc,
//    nor the names of its contributors may be used to endorse or promote
//    products derived from this Software without specific prior written
//    permission.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS WITH THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////

// Helpers to use non-atomic types with C++11 atomic operations.

#ifndef HSA_RUNTIME_CORE_UTIL_ATOMIC_HELPERS_H_
#define HSA_RUNTIME_CORE_UTIL_ATOMIC_HELPERS_H_

#include <atomic>
#include "utils.h"

/// @brief: Special assert used here to check each atomic variable for lock free
/// implementation.
/// ANY locked atomics are very likely incompatable with out-of-library
/// concurrent access (HW access for instance)
#define lockless_check(exp) assert(exp)

namespace atomic {
/// @brief: Checks if type T is compatible with its atomic representation.
/// @param: ptr(Input), a pointer to type T for check.
/// @return: void.
template <class T>
static __forceinline void BasicCheck(const T* ptr) {
  static_assert(sizeof(T) == sizeof(std::atomic<T>),
                "Type is size incompatible with its atomic representation!");
  lockless_check(
      reinterpret_cast<const std::atomic<T>*>(ptr)->is_lock_free() &&
      "Atomic operation is not lock free!  Use may conflict with peripheral HW "
      "atomics!");
};

/// @brief: function overloading, for more info, see previous one.
/// @param: ptr(Input), a pointer to a volatile type.
/// @return: void.
template <class T>
static __forceinline void BasicCheck(const volatile T* ptr) {
  static_assert(sizeof(T) == sizeof(std::atomic<T>),
                "Type is size incompatible with its atomic representation!");
  lockless_check(
      reinterpret_cast<const volatile std::atomic<T>*>(ptr)->is_lock_free() &&
      "Atomic operation is not lock free!  Use may conflict with peripheral HW "
      "atomics!");
};

/// @brief: Load value of type T atomically with specified memory order.
/// @param: ptr(Input), a pointer to type T.
/// @param: order(Input), memory order with atomic load, relaxed by default.
/// @return: T, loaded value.
template <class T>
static __forceinline T
    Load(const T* ptr, std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  const std::atomic<T>* aptr = reinterpret_cast<const std::atomic<T>*>(ptr);
  return aptr->load(order);
}

/// @brief: function overloading, for more info, see previous one.
/// @param: ptr(Input), a pointer to volatile type T.
/// @param: order(Input), memory order with atomic load, relaxed by default.
/// @return: T, loaded value.
template <class T>
static __forceinline T
    Load(const volatile T* ptr,
         std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  volatile const std::atomic<T>* aptr =
      reinterpret_cast<volatile const std::atomic<T>*>(ptr);
  return aptr->load(order);
}

/// @brief: Store value of type T with specified memory order.
/// @param: ptr(Input), a pointer to instance which will be stored.
/// @param: val(Input), value to be stored.
/// @param: order(Input), memory order with atomic store, relaxed by default.
/// @return: void.
template <class T>
static __forceinline void Store(
    T* ptr, T val, std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  std::atomic<T>* aptr = reinterpret_cast<std::atomic<T>*>(ptr);
  aptr->store(val, order);
}

/// @brief: Function overloading, for more info, see previous one.
/// @param: ptr(Input), a pointer to volatile instance which will be stored.
/// @param: val(Input), value to be stored.
/// @param: order(Input), memory order with atomic store, relaxed by default.
/// @return: void.
template <class T>
static __forceinline void Store(
    volatile T* ptr, T val,
    std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  volatile std::atomic<T>* aptr =
      reinterpret_cast<volatile std::atomic<T>*>(ptr);
  aptr->store(val, order);
}

/// @brief: Compare and swap value atomically with specified memory order.
/// @param: ptr(Input), a pointer to variable which is operated on.
/// @param: val(Input), value to be stored if condition is satisfied.
/// @param: expected(Input), value which is expected.
/// @param: order(Input), memory order with atomic operation.
/// @return: T, observed value of type T.
template <class T>
static __forceinline T
    Cas(T* ptr, T val, T expected,
        std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  std::atomic<T>* aptr = reinterpret_cast<std::atomic<T>*>(ptr);
  aptr->compare_exchange_strong(expected, val, order);
  return expected;
}

/// @brief: Function overloading, for more info, see previous one.
/// @param: ptr(Input), a pointer to volatile variable which is operated on.
/// @param: val(Input), value to be stored if condition is satisfied.
/// @param: expected(Input), value which is expected.
/// @param: order(Input), memory order which is relaxed by default.
/// @return: T, observed value of type T.
template <class T>
static __forceinline T
    Cas(volatile T* ptr, T val, T expected,
        std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  volatile std::atomic<T>* aptr =
      reinterpret_cast<volatile std::atomic<T>*>(ptr);
  aptr->compare_exchange_strong(expected, val, order);
  return expected;
}

/// @brief: Exchange the value atomically with specified memory order.
/// @param: ptr(Input), a pointer to variable which is operated on.
/// @param: val(Input), value to be stored.
/// @param: order(Input), memory order which is relaxed by default.
/// @return: T, the value prior to the exchange.
template <class T>
static __forceinline T
    Exchange(T* ptr, T val,
             std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  std::atomic<T>* aptr = reinterpret_cast<std::atomic<T>*>(ptr);
  return aptr->exchange(val, order);
}

/// @brief: Function overloading, for more info, see previous one.
/// @param: ptr(Input), a pointer to variable which is operated on.
/// @param: val(Input), value to be stored.
/// @param: order(Input), memory order which is relaxed by default.
/// @return: T, the value prior to the exchange.
template <class T>
static __forceinline T
    Exchange(volatile T* ptr, T val,
             std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  volatile std::atomic<T>* aptr =
      reinterpret_cast<volatile std::atomic<T>*>(ptr);
  return aptr->exchange(val, order);
}

/// @brief: Add value to variable atomically with specified memory order.
/// @param: ptr(Input), a pointer to variable which is operated on.
/// @param: val(Input), value to be added.
/// @param: order(Input), memory order which is relaxed by default.
/// @return: T, the value of the variable prior to the addition.
template <class T>
static __forceinline T
    Add(T* ptr, T val, std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  std::atomic<T>* aptr = reinterpret_cast<std::atomic<T>*>(ptr);
  return aptr->fetch_add(val, order);
}

/// @brief: Subtract value from the variable atomically with specified memory
/// order.
/// @param: ptr(Input), a pointer to variable which is operated on.
/// @param: val(Input), value to be subtraced.
/// @param: order(Input), memory order which is relaxed by default.
/// @return: T, value of the variable prior to the subtraction.
template <class T>
static __forceinline T
    Sub(T* ptr, T val, std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  std::atomic<T>* aptr = reinterpret_cast<std::atomic<T>*>(ptr);
  return aptr->fetch_sub(val, order);
}

/// @brief: Bit And operation on variable atomically with specified memory
/// order.
/// @param: ptr(Input), a pointer to variable which is operated on.
/// @param: val(Input), value which is ANDed with variable.
/// @param: order(Input), memory order which is relaxed by default.
/// @return: T, value of variable prior to the operation.
template <class T>
static __forceinline T
    And(T* ptr, T val, std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  std::atomic<T>* aptr = reinterpret_cast<std::atomic<T>*>(ptr);
  return aptr->fetch_and(val, order);
}

/// @brief: Bit Or operation on variable atomically with specified memory order.
/// @param: ptr(Input), a pointer to variable which is operated on.
/// @param: val(Input), value which is ORed with variable.
/// @param: order(Input), memory order which is relaxed by default.
/// @return: T, value of variable prior to the operation.
template <class T>
static __forceinline T
    Or(T* ptr, T val, std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  std::atomic<T>* aptr = reinterpret_cast<std::atomic<T>*>(ptr);
  return aptr->fetch_or(val, order);
}

/// @brief: Bit Xor operation on variable atomically with specified memory
/// order.
/// @param: ptr(Input), a pointer to variable which is operated on.
/// @param: val(Input), value which is XORed with variable.
/// @order: order(Input), memory order which is relaxed by default.
/// @return: T, valud of variable prior to the opertaion.
template <class T>
static __forceinline T
    Xor(T* ptr, T val, std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  std::atomic<T>* aptr = reinterpret_cast<std::atomic<T>*>(ptr);
  return aptr->fetch_xor(val, order);
}

/// @brief: Increase the value of variable atomically with specified memory
/// order.
/// @param: ptr(Input), a pointer to variable which is operated on.
/// @param: order(Input), memory order which is relaxed by default.
/// @return: T, value of variable prior to the operation.
template <class T>
static __forceinline T
    Increment(T* ptr, std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  std::atomic<T>* aptr = reinterpret_cast<std::atomic<T>*>(ptr);
  return aptr->fetch_add(1, order);
}

/// @brief: Decrease the value of the variable atomically with specified memory
/// order.
/// @param: ptr(Input), a pointer to variable which is operated on.
/// @param: order(Input), memory order which is relaxed by default.
/// @return: T, value of variable prior to the operation.
template <class T>
static __forceinline T
    Decrement(T* ptr, std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  std::atomic<T>* aptr = reinterpret_cast<std::atomic<T>*>(ptr);
  return aptr->fetch_sub(1, order);
}

/// @brief: Add value to variable atomically with specified memory order.
/// @param: ptr(Input), a pointer to volatile variable which is operated on.
/// @param: val(Input), value to be added.
/// @param: order(Input), memory order which is relaxed by default.
/// @return: T, the value of the variable prior to the addition.
template <class T>
static __forceinline T
    Add(volatile T* ptr, T val,
        std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  volatile std::atomic<T>* aptr =
      reinterpret_cast<volatile std::atomic<T>*>(ptr);
  return aptr->fetch_add(val, order);
}

/// @brief: Subtract value from the variable atomically with specified memory
/// order.
/// @param: ptr(Input), a pointer to volatile variable which is operated on.
/// @param: val(Input), value to be subtraced.
/// @param: order(Input), memory order which is relaxed by default.
/// @return: T, value of the variable prior to the subtraction.
template <class T>
static __forceinline T
    Sub(volatile T* ptr, T val,
        std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  volatile std::atomic<T>* aptr =
      reinterpret_cast<volatile std::atomic<T>*>(ptr);
  return aptr->fetch_sub(val, order);
}

/// @brief: Bit And operation on variable atomically with specified memory
/// order.
/// @param: ptr(Input), a pointer to volatile variable which is operated on.
/// @param: val(Input), value which is ANDed with variable.
/// @param: order(Input), memory order which is relaxed by default.
/// @return: T, value of variable prior to the operation.
template <class T>
static __forceinline T
    And(volatile T* ptr, T val,
        std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  volatile std::atomic<T>* aptr =
      reinterpret_cast<volatile std::atomic<T>*>(ptr);
  return aptr->fetch_and(val, order);
}

/// @brief: Bit Or operation on variable atomically with specified memory order.
/// @param: ptr(Input), a pointer to volatile variable which is operated on.
/// @param: val(Input), value which is ORed with variable.
/// @param: order(Input), memory order which is relaxed by default.
/// @return: T, value of variable prior to the operation.
template <class T>
static __forceinline T Or(volatile T* ptr, T val,
                          std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  volatile std::atomic<T>* aptr =
      reinterpret_cast<volatile std::atomic<T>*>(ptr);
  return aptr->fetch_or(val, order);
}

/// @brief: Bit Xor operation on variable atomically with specified memory
/// order.
/// @param: ptr(Input), a pointer to volatile variable which is operated on.
/// @param: val(Input), value which is XORed with variable.
/// @order: order(Input), memory order which is relaxed by default.
/// @return: T, valud of variable prior to the opertaion.
template <class T>
static __forceinline T
    Xor(volatile T* ptr, T val,
        std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  volatile std::atomic<T>* aptr =
      reinterpret_cast<volatile std::atomic<T>*>(ptr);
  return aptr->fetch_xor(val, order);
}

/// @brief: Increase the value of variable atomically with specified memory
/// order.
/// @param: ptr(Input), a pointer to volatile variable which is operated on.
/// @param: order(Input), memory order which is relaxed by default.
/// @return: T, value of variable prior to the operation.
template <class T>
static __forceinline T
    Increment(volatile T* ptr,
              std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  volatile std::atomic<T>* aptr =
      reinterpret_cast<volatile std::atomic<T>*>(ptr);
  return aptr->fetch_add(1, order);
}

/// @brief: Decrease the value of the variable atomically with specified memory
/// order.
/// @param: ptr(Input), a pointer to volatile variable which is operated on.
/// @param: order(Input), memory order which is relaxed by default.
/// @return: T, value of variable prior to the operation.
template <class T>
static __forceinline T
    Decrement(volatile T* ptr,
              std::memory_order order = std::memory_order_relaxed) {
  BasicCheck<T>(ptr);
  volatile std::atomic<T>* aptr =
      reinterpret_cast<volatile std::atomic<T>*>(ptr);
  return aptr->fetch_sub(1, order);
}
}

// Remove special assert to avoid name polution
#undef lockless_check

#endif  // HSA_RUNTIME_CORE_UTIL_ATOMIC_HELPERS_H_
