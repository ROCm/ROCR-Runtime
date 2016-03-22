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

#include "small_heap.h"

SmallHeap::memory_t::iterator SmallHeap::merge(
    SmallHeap::memory_t::iterator& keep,
    SmallHeap::memory_t::iterator& destroy) {
  assert((char*)keep->first + keep->second.len == (char*)destroy->first &&
         "Invalid merge");
  assert(keep->second.isfree() && "Merge with allocated block");
  assert(destroy->second.isfree() && "Merge with allocated block");

  keep->second.len += destroy->second.len;
  keep->second.next_free = destroy->second.next_free;
  if (!destroy->second.islastfree())
    memory[destroy->second.next_free].prior_free = keep->first;

  memory.erase(destroy);
  return keep;
}

void SmallHeap::free(void* ptr) {
  if (ptr == NULL) return;

  auto iterator = memory.find(ptr);

  // Check for illegal free
  if (iterator == memory.end()) {
    assert(false && "Illegal free.");
    return;
  }

  const auto start_guard = memory.find(0);
  const auto end_guard = memory.find((void*)0xFFFFFFFFFFFFFFFFull);

  // Return memory to total and link node into free list
  total_free += iterator->second.len;
  if (first_free < iterator->first) {
    auto before = iterator;
    before--;
    while (before != start_guard && !before->second.isfree()) before--;
    assert(before->second.next_free > iterator->first &&
           "Inconsistency in small heap.");
    iterator->second.prior_free = before->first;
    iterator->second.next_free = before->second.next_free;
    before->second.next_free = iterator->first;
    if (!iterator->second.islastfree())
      memory[iterator->second.next_free].prior_free = iterator->first;
  } else {
    iterator->second.setfirstfree();
    iterator->second.next_free = first_free;
    first_free = iterator->first;
    if (!iterator->second.islastfree())
      memory[iterator->second.next_free].prior_free = iterator->first;
  }

  // Attempt compaction
  auto before = iterator;
  before--;
  if (before != start_guard) {
    if (before->second.isfree()) {
      iterator = merge(before, iterator);
    }
  }

  auto after = iterator;
  after++;
  if (after != end_guard) {
    if (after->second.isfree()) {
      iterator = merge(iterator, after);
    }
  }
}

void* SmallHeap::alloc(size_t bytes) {
  // Is enough memory available?
  if ((bytes > total_free) || (bytes == 0)) return NULL;

  memory_t::iterator current;
  memory_t::iterator prior;

  // Walk the free list and allocate at first fitting location
  prior = current = memory.find(first_free);
  while (true) {
    if (bytes <= current->second.len) {
      // Decrement from total
      total_free -= bytes;

      // Is allocation an exact fit?
      if (bytes == current->second.len) {
        if (prior == current) {
          first_free = current->second.next_free;
          if (!current->second.islastfree())
            memory[current->second.next_free].setfirstfree();
        } else {
          prior->second.next_free = current->second.next_free;
          if (!current->second.islastfree())
            memory[current->second.next_free].prior_free = prior->first;
        }
        current->second.next_free = NULL;
        return current->first;
      } else {
        // Split current node
        void* remaining = (char*)current->first + bytes;
        Node& node = memory[remaining];
        node.next_free = current->second.next_free;
        node.prior_free = current->second.prior_free;
        node.len = current->second.len - bytes;
        current->second.len = bytes;

        if (prior == current) {
          first_free = remaining;
          node.setfirstfree();
        } else {
          prior->second.next_free = remaining;
          node.prior_free = prior->first;
        }
        if (!node.islastfree()) memory[node.next_free].prior_free = remaining;

        current->second.next_free = NULL;
        return current->first;
      }
    }

    // End of free list?
    if (current->second.islastfree()) break;

    prior = current;
    current = memory.find(current->second.next_free);
  }

  // Can't service the request due to fragmentation
  return NULL;
}
