////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2020-2020, Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef HSA_RUNTIME_CORE_INC_SCRATCH_CACHE_H_
#define HSA_RUNTIME_CORE_INC_SCRATCH_CACHE_H_

#include "core/inc/amd_gpu_agent.h"
#include "core/util/locks.h"
#include "core/util/utils.h"

#include <map>
#include <functional>

namespace rocr {
namespace AMD {

class ScratchCache {
 public:
  struct node {
    enum STATE { FREE = 0, ALLOC = 1, TRIM = 2, STEAL = 4 };
    void* base;
    bool large;
    uint32_t state;

    node() : base(nullptr), state(FREE) {}

    bool isFree() const { return state == FREE; }
    bool trimPending() const { return state == (ALLOC | TRIM); }

    void trim() {
      assert(!isFree() && "Trim of free scratch node.");
      state |= TRIM;
    }
    void free() {
      assert(!isFree() && "Free of free scratch node.");
      state = FREE;
    }
    void alloc() {
      assert(isFree() && "Alloc of non-free scratch node.");
      state = ALLOC;
    }
  };

  typedef ::std::multimap<size_t, node> map_t;
  typedef map_t::iterator ref_t;
  typedef ::std::function<void(void*, size_t, bool)> deallocator_t;

  // @brief Contains scratch memory information.
  struct ScratchInfo {
    void* queue_base;
    // Size to fill the machine with size_per_thread
    size_t size;
    // Size to satisfy the present dispatch without throttling.
    size_t dispatch_size;
    size_t size_per_thread;
    uint32_t lanes_per_wave;
    uint32_t waves_per_group;
    ptrdiff_t queue_process_offset;
    bool large;
    bool retry;
    hsa_signal_t queue_retry;
    uint64_t wanted_slots;
    ScratchCache::ref_t scratch_node;
  };

  ScratchCache(const ScratchCache& rhs) = delete;
  ScratchCache(ScratchCache&& rhs) = delete;
  ScratchCache& operator=(const ScratchCache& rhs) = delete;
  ScratchCache& operator=(ScratchCache&& rhs) = delete;

  ScratchCache(deallocator_t deallocator) : dealloc(deallocator) {}

  ~ScratchCache() { assert(map.empty() && "ScratchCache not empty at shutdown."); }

  bool alloc(ScratchInfo& info) {
    ref_t it = map.upper_bound(info.size - 1);
    if (it == map.end()) return false;

    // Small requests must have an exact size match and be small.
    if (!info.large) {
      while ((it != map.end()) && (it->first == info.size)) {
        if (it->second.isFree() && (!it->second.large)) {
          it->second.alloc();
          info.queue_base = it->second.base;
          info.scratch_node = it;
          return true;
        }
        it++;
      }
      return false;
    }

    // Large requests may use a small allocation and do not require an exact size match.
    while (it != map.end()) {
      if (it->second.isFree()) {
        it->second.alloc();
        info.queue_base = it->second.base;
        info.size = it->first;
        info.scratch_node = it;
        return true;
      }
      it++;
    }
    return false;
  }

  void free(ScratchInfo& info) {
    assert(!info.scratch_node->second.isFree() && "free called on free scratch node.");
    auto it = info.scratch_node;
    if (it->second.trimPending()) {
      dealloc(it->second.base, it->first, it->second.large);
      map.erase(it);
      return;
    }
    it->second.free();
  }

  bool trim(bool trim_nodes_in_use) {
    bool ret = !map.empty();
    auto it = map.begin();
    while (it != map.end()) {
      if (it->second.isFree()) {
        dealloc(it->second.base, it->first, it->second.large);
        auto temp = it;
        it++;
        map.erase(temp);
      } else {
        if (trim_nodes_in_use) it->second.trim();
        it++;
      }
    }
    return ret;
  }

  void insert(ScratchInfo& info) {
    node n;
    n.base = info.queue_base;
    n.large = info.large;
    n.alloc();

    auto it = map.insert(std::make_pair(info.size, n));
    info.scratch_node = it;
  }

 private:
  map_t map;
  deallocator_t dealloc;
};

}  // namespace AMD
}  // namespace rocr

#endif  // header guard
