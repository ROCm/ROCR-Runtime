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

// A simple first fit memory allocator with eager compaction.  For use with few
// items (where list iteration is faster than trees).
// Not thread safe!

#ifndef HSA_RUNTME_CORE_UTIL_SMALL_HEAP_H_
#define HSA_RUNTME_CORE_UTIL_SMALL_HEAP_H_

#include "utils.h"

#include <map>

class SmallHeap {
 public:
  class Node {
   public:
    size_t len;
    void* next_free;
    void* prior_free;
    static const intptr_t END = -1;

    __forceinline bool isfree() const { return next_free != NULL; }
    __forceinline bool islastfree() const { return intptr_t(next_free) == END; }
    __forceinline bool isfirstfree() const {
      return intptr_t(prior_free) == END;
    }
    __forceinline void setlastfree() {
      *reinterpret_cast<intptr_t*>(&next_free) = END;
    }
    __forceinline void setfirstfree() {
      *reinterpret_cast<intptr_t*>(&prior_free) = END;
    }
  };

 private:
  SmallHeap(const SmallHeap& rhs);
  SmallHeap& operator=(const SmallHeap& rhs);

  void* const pool;
  const size_t length;

  size_t total_free;
  void* first_free;
  std::map<void*, Node> memory;

  typedef decltype(memory) memory_t;
  memory_t::iterator merge(memory_t::iterator& keep,
                           memory_t::iterator& destroy);

 public:
  SmallHeap() : pool(NULL), length(0), total_free(0) {}
  SmallHeap(void* base, size_t length)
      : pool(base), length(length), total_free(length) {
    first_free = pool;

    Node& node = memory[first_free];
    node.len = length;
    node.setlastfree();
    node.setfirstfree();

    memory[0].len = 0;
    memory[(void*)0xFFFFFFFFFFFFFFFFull].len = 0;
  }

  void* alloc(size_t bytes);
  void free(void* ptr);

  void* base() const { return pool; }
  size_t size() const { return length; }
  size_t remaining() const { return total_free; }
};

#endif
