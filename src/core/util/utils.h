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

// Generally useful utility functions

#ifndef HSA_RUNTIME_CORE_UTIL_UTILS_H_
#define HSA_RUNTIME_CORE_UTIL_UTILS_H_

#include "stdint.h"
#include "stddef.h"
#include "stdlib.h"
#include <assert.h>

typedef unsigned int uint;
typedef uint64_t uint64;

#if defined(__GNUC__)
#include "mm_malloc.h"
#if defined(__i386__) || defined(__x86_64__)
#include <x86intrin.h>
#else
#error \
    "Processor or compiler not identified.  " \
    "Need to provide a lightweight approximate clock interface via function uint64_t __rdtsc() or adapt timer.h to your platform."
#endif

#define __forceinline __inline__ __attribute__((always_inline))
#if !defined (__clang__) // __debugbreak is provided by clang when using -fms-extensions
static __forceinline void __debugbreak() { __builtin_trap(); }
#endif
#define __declspec(x) __attribute__((x))
#undef __stdcall
#define __stdcall  // __attribute__((__stdcall__))
#define __ALIGNED__(x) __attribute__((aligned(x)))

static __forceinline void* _aligned_malloc(size_t size, size_t alignment) {
  return _mm_malloc(size, alignment);
}
static __forceinline void _aligned_free(void* ptr) { return _mm_free(ptr); }
#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include "intrin.h"
#define __ALIGNED__(x) __declspec(align(x))
#if (_MSC_VER < 1800)
static __forceinline unsigned long long int strtoull(const char* str,
                                                     char** endptr, int base) {
  return static_cast<unsigned long long>(_strtoui64(str, endptr, base));
}
#endif
#else
#error "Compiler and/or processor not identified."
#endif

#define STRING2(x) #x
#define STRING(x) STRING2(x)

#define PASTE2(x, y) x##y
#define PASTE(x, y) PASTE2(x, y)

// A macro to disallow the copy and move constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  TypeName(TypeName&&);                    \
  void operator=(const TypeName&);         \
  void operator=(TypeName&&);

template <typename lambda>
class ScopeGuard {
 public:
  explicit __forceinline ScopeGuard(const lambda& release)
      : release_(release), dismiss_(false) {}

  ScopeGuard(ScopeGuard& rhs) { *this = rhs; }

  __forceinline ~ScopeGuard() {
    if (!dismiss_) release_();
  }
  __forceinline ScopeGuard& operator=(ScopeGuard& rhs) {
    dismiss_ = rhs.dismiss_;
    release_ = rhs.release_;
    rhs.dismiss_ = true;
  }
  __forceinline void Dismiss() { dismiss_ = true; }

 private:
  lambda release_;
  bool dismiss_;
};

template <typename lambda>
static __forceinline ScopeGuard<lambda> MakeScopeGuard(lambda rel) {
  return ScopeGuard<lambda>(rel);
}

#define MAKE_SCOPE_GUARD_HELPER(lname, sname, ...) \
  auto lname = __VA_ARGS__;                        \
  ScopeGuard<decltype(lname)> sname(lname);
#define MAKE_SCOPE_GUARD(...)                                   \
  MAKE_SCOPE_GUARD_HELPER(PASTE(scopeGuardLambda, __COUNTER__), \
                          PASTE(scopeGuard, __COUNTER__), __VA_ARGS__)
#define MAKE_NAMED_SCOPE_GUARD(name, ...)                             \
  MAKE_SCOPE_GUARD_HELPER(PASTE(scopeGuardLambda, __COUNTER__), name, \
                          __VA_ARGS__)

/// @brief: Finds out the min one of two inputs, input must support ">"
/// operator.
/// @param: a(Input), a reference to type T.
/// @param: b(Input), a reference to type T.
/// @return: T.
template <class T>
static __forceinline T Min(const T& a, const T& b) {
  return (a > b) ? b : a;
}

/// @brief: Find out the max one of two inputs, input must support ">" operator.
/// @param: a(Input), a reference to type T.
/// @param: b(Input), a reference to type T.
/// @return: T.
template <class T>
static __forceinline T Max(const T& a, const T& b) {
  return (b > a) ? b : a;
}

/// @brief: Free the memory space which is newed previously.
/// @param: ptr(Input), a pointer to memory space. Can't be NULL.
/// @return: void.
struct DeleteObject {
  template <typename T>
  void operator()(const T* ptr) const {
    delete ptr;
  }
};

/// @brief: Checks if a value is power of two, if it is, return true. Be careful
/// when passing 0.
/// @param: val(Input), the data to be checked.
/// @return: bool.
template <typename T>
static __forceinline bool IsPowerOfTwo(T val) {
  return (val & (val - 1)) == 0;
}

/// @brief: Calculates the floor value aligned based on parameter of alignment.
/// If value is at the boundary of alignment, it is unchanged.
/// @param: value(Input), value to be calculated.
/// @param: alignment(Input), alignment value.
/// @return: T.
template <typename T>
static __forceinline T AlignDown(T value, size_t alignment) {
  assert(IsPowerOfTwo(alignment));
  return (T)(value & ~(alignment - 1));
}

/// @brief: Same as previous one, but first parameter becomes pointer, for more
/// info, see the previous desciption.
/// @param: value(Input), pointer to type T.
/// @param: alignment(Input), alignment value.
/// @return: T*, pointer to type T.
template <typename T>
static __forceinline T* AlignDown(T* value, size_t alignment) {
  return (T*)AlignDown((intptr_t)value, alignment);
}

/// @brief: Calculates the ceiling value aligned based on parameter of
/// alignment.
/// If value is at the boundary of alignment, it is unchanged.
/// @param: value(Input), value to be calculated.
/// @param: alignment(Input), alignment value.
/// @param: T.
template <typename T>
static __forceinline T AlignUp(T value, size_t alignment) {
  return AlignDown((T)(value + alignment - 1), alignment);
}

/// @brief: Same as previous one, but first parameter becomes pointer, for more
/// info, see the previous desciption.
/// @param: value(Input), pointer to type T.
/// @param: alignment(Input), alignment value.
/// @return: T*, pointer to type T.
template <typename T>
static __forceinline T* AlignUp(T* value, size_t alignment) {
  return (T*)AlignDown((intptr_t)((uint8_t*)value + alignment - 1), alignment);
}

/// @brief: Checks if the input value is at the boundary of alignment, if it is,
/// @return true.
/// @param: value(Input), value to be checked.
/// @param: alignment(Input), alignment value.
/// @return: bool.
template <typename T>
static __forceinline bool IsMultipleOf(T value, size_t alignment) {
  return (AlignUp(value, alignment) == value);
}

/// @brief: Same as previous one, but first parameter becomes pointer, for more
/// info, see the previous desciption.
/// @param: value(Input), pointer to type T.
/// @param: alignment(Input), alignment value.
/// @return: bool.
template <typename T>
static __forceinline bool IsMultipleOf(T* value, size_t alignment) {
  return (AlignUp(value, alignment) == value);
}

static __forceinline uint32_t NextPow2(uint32_t value) {
  if (value == 0) return 1;
  uint32_t v = value - 1;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return v + 1;
}

static __forceinline uint64_t NextPow2(uint64_t value) {
  if (value == 0) return 1;
  uint64_t v = value - 1;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v |= v >> 32;
  return v + 1;
}

#include "atomic_helpers.h"

#endif  // HSA_RUNTIME_CORE_UTIL_UTIIS_H_
