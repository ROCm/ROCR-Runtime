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

#ifndef HSA_RUNTME_CORE_INC_SHARED_H_
#define HSA_RUNTME_CORE_INC_SHARED_H_

#include "core/util/utils.h"
#include <assert.h>

#include <cstring>
#include <functional>

namespace core {
/// @brief Base class encapsulating the allocator and deallocator for
/// shared shared object.
class BaseShared {
 public:
  static void SetAllocateAndFree(
      const std::function<void*(size_t, size_t)>& allocate,
      const std::function<void(void*)>& free) {
    allocate_ = allocate;
    free_ = free;
  }

 protected:
  static std::function<void*(size_t, size_t)> allocate_;
  static std::function<void(void*)> free_;
};

/// @brief Base class for classes that encapsulates object shared between
/// host and agents.  Alignment defaults to __alignof(T) but may be increased.
template <typename T, size_t Align=0>
class Shared : public BaseShared {
 public:
  Shared() {
    assert(allocate_ != nullptr && free_ != nullptr &&
           "Shared object allocator is not set");
    static_assert((__alignof(T) <= Align) || (Align == 0),
                  "Align is less than alignof(T)");

    shared_object_ =
        reinterpret_cast<T*>(allocate_(sizeof(T), Max(__alignof(T), Align)));

    assert(shared_object_ != NULL && "Failed on allocating shared_object_");

    memset(shared_object_, 0, sizeof(T));

    if (shared_object_ != NULL) new (shared_object_) T;
  }

  virtual ~Shared() {
    assert(allocate_ != nullptr && free_ != nullptr &&
           "Shared object allocator is not set");

    if (IsSharedObjectAllocationValid()) {
      shared_object_->~T();
      free_(shared_object_);
    }
  }

  T* shared_object() const { return shared_object_; }

  bool IsSharedObjectAllocationValid() const {
    return (shared_object_ != NULL);
  }

 private:
  T* shared_object_;
};

}  // namespace core
#endif  // header guard
