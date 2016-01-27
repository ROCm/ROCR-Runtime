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

// Memory_Datebase.cpp
// Implementation of class Memory_Database.

#include "core/inc/memory_database.h"

#include "core/inc/runtime.h"

namespace core {

// Check if the given address is in the page range or registered. If it is,
// return ture.
bool MemoryDatabase::FindContainingBlock(
    uintptr_t address, std::map<uintptr_t, PageRange>::iterator& near_hint) {
  // Block is prior to near_hint
  if (address < near_hint->first) {
    while (address < near_hint->first) near_hint--;
    if (address < near_hint->first + near_hint->second.size_) return true;
    return false;
  }
  // Block is at or after near_hint
  while (near_hint->first + near_hint->second.size_ <= address) near_hint++;
  if (near_hint->first <= address) return true;
  return false;
}

bool MemoryDatabase::RegisterImpl(void* ptr, size_t size,
                                  bool RegisterWithDrivers) {
  // Check for zero length, NULL pointer, and pointer overflow.
  if(ptr == NULL)
    return true;

  if ((size == 0) || ((uintptr_t)ptr + size < (uintptr_t)ptr))
    return false;

  const uintptr_t base = (uintptr_t)ptr;

  // variable: start_page is the address of the page which base belongs to
  const uintptr_t start_page = GetPage(base);
  // variable: end_page is the address of the page which is imediately after the
  // requested block.
  const uintptr_t end_page = GetNextPage(base, size);

  // Provisionally insert the range
  // std::map::insert will return pair<iterator, bool> data type, if the key is
  // already existing, bool is false.
  // variable: provisional_iterator is an iterator to the element whose "key" is
  // equal to the value of base.
  auto provisional_iterator =
      requested_ranges_.insert(std::pair<uintptr_t, Range>(base, Range()))
          .first;
  // variable: temp_iterator is used for checking requested region overlap.
  auto temp_iterator = provisional_iterator;

  // variable: range is a reference to the value of map
  Range& range = temp_iterator->second;

  // Checks for basic validity of the requested range at all exit points
  // Will assert on new range insertion failure (even though spec defines this
  // as a recoverable error)
  // since this is always a program error.
  MAKE_SCOPE_GUARD([&]() {
    assert(range.start_page != 0);
    assert(range.size != 0);
  })

  // If requested region was already registered - only valid case is a second
  // registration on an hsa allocation region
  if (range.size != 0) {
    // Requested region is an existing memory allocation registration
    if ((range.size == size) && (!range.toDriver) && (range.ref_count_ == 1)) {
      range.Retain();
      return true;
    }
    return false;
  }

  // Requested range is new - check for overlaps
  auto before = temp_iterator;
  auto after = temp_iterator;
  // variable: before will be the element before currently inserted element in
  // requested_ranges_, and after will be the element after currently inserted
  // element in requested_ranges_.
  after++;
  before--;
  // If previous requested regions will overlap new region, erase the new entry,
  // and return false
  if (before->first + before->second.size > base) {
    requested_ranges_.erase(provisional_iterator);
    return false;
  }
  // If new requested region overlaps the following requested region, erase the
  // new entry, and return false.
  if (base + size > after->first) {
    requested_ranges_.erase(provisional_iterator);
    return false;
  }
  // Fill out new range
  range.size = size;
  range.ref_count_ = 1;
  range.toDriver = RegisterWithDrivers;

  // Get last page block of prior region
  // Start at first block of next region and work back
  auto near_block = registered_ranges_.find(after->second.start_page);
  assert(near_block != registered_ranges_.end() &&
         "Inconsistency in memory database.");

  // Adjust start of registered page region for overlaps
  uintptr_t new_start_page = 0;
  auto start_block = near_block;
  if (FindContainingBlock(start_page, start_block)) {
    range.start_page = start_block->first;
    start_block->second.Retain();
    new_start_page = start_block->first + start_block->second.size_;
  } else {
    new_start_page = start_page;
    range.start_page = start_page;
  }

  // Adjust end of registered page region for overlaps
  uintptr_t new_end_page;
  auto end_block = near_block;
  if (FindContainingBlock(end_page - 1, end_block)) {
    new_end_page = end_block->first;
    // Don't double count a block if the start and end blocks are identical
    if (start_block != end_block) end_block->second.Retain();
  } else {
    new_end_page = end_page;
  }

  // Remaining pages
  // Register new space whose size is equal to new_length with KFD driver and
  // establish the corresponding mapping in registered_ranges_.
  if (new_start_page < new_end_page) {
    size_t new_length = new_end_page - new_start_page;
    if (RegisterWithDrivers) {
      bool ret = Runtime::runtime_singleton_->RegisterWithDrivers(
          (void*)new_start_page, new_length);
      assert(ret && "KFD registration failure!");
    }
    registered_ranges_[new_start_page] =
        PageRange(new_length, RegisterWithDrivers);
    if (range.start_page == 0)
      range.start_page = new_start_page;  // When start_page of base is 0, this
                                          // will override the guard element.
  }

  return true;
}

bool MemoryDatabase::DeregisterImpl(void* ptr) {
  if (ptr == NULL) return true;

  uintptr_t base = (uintptr_t)ptr;

  // Find out if value of base if an existing key of requested_ranges_, if it
  // is, stores the corresponding iterator to variable of
  // requested_range_iterator.
  auto requested_range_iterator = requested_ranges_.find(base);
  // If ptr is not in requested_ranges_, return.
  if (requested_range_iterator == requested_ranges_.end()) return false;

  // Check for last release of a hsa memory allocator region
  if (!requested_range_iterator->second.Release()) return true;

  // Calculate the ending address of the being deleted block and stores it to
  // new variable of end_of_range.
  const uintptr_t end_of_range =
      requested_range_iterator->first + requested_range_iterator->second.size;

  // Find out the corresponding entry in registered_ranges_, and stores the
  // iterator to new variable of registered_range_iterator.
  auto registered_range_iterator =
      registered_ranges_.find(requested_range_iterator->second.start_page);
  assert(registered_range_iterator != registered_ranges_.end() &&
         "Inconsistency in memory database.");

  // Reduce reference counts and release ranges
  while (registered_range_iterator->first < end_of_range) {
    bool release_from_devices = registered_range_iterator->second.Release();
    auto temp = registered_range_iterator;
    temp++;
    if (release_from_devices) {
      if (registered_range_iterator->second.toDriver) {
        Runtime::runtime_singleton_->DeregisterWithDrivers(
            (void*)registered_range_iterator->first);
      }
      registered_ranges_.erase(registered_range_iterator);
    }
    registered_range_iterator = temp;
    assert(registered_range_iterator != registered_ranges_.end() &&
           "Inconsistency in memory database.");
  }

  // Removes the corresponding entry from the requested_ranges_.
  requested_ranges_.erase(requested_range_iterator);
  return true;
}
}  // namespace core
