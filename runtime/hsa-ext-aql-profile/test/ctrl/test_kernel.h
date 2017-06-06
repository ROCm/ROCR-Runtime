/******************************************************************************

Copyright ©2013 Advanced Micro Devices, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#ifndef _TEST_KERNEL_H_
#define _TEST_KERNEL_H_

#include <map>
#include <stdint.h>

// Class implements Kernel test
class TestKernel {
 public:
  // Memory descriptors IDs
  enum { INPUT_DES_ID, OUTPUT_DES_ID, LOCAL_DES_ID, MASK_DES_ID, KERNARG_DES_ID, REFOUT_DES_ID };

  // Memory descriptors vector declaration
  struct mem_descr_t {
    void* ptr;
    uint32_t size;
    bool local;
  };

  // Memory map declaration
  typedef std::map<uint32_t, mem_descr_t> mem_map_t;
  typedef mem_map_t::iterator mem_it_t;
  typedef mem_map_t::const_iterator mem_const_it_t;

  // Initialize method
  virtual void init() = 0;

  // Return kernel memory map
  mem_map_t& get_mem_map() { return mem_map_; }

  // Return NULL descriptor
  static mem_descr_t null_descriptor() { return {0, 0, 0}; }

  // Methods to get the kernel attributes
  void* get_kernarg_ptr() const { return get_descr(KERNARG_DES_ID).ptr; }
  uint32_t get_kernarg_size() const { return get_descr(KERNARG_DES_ID).size; }
  void* get_output_ptr() const { return get_descr(OUTPUT_DES_ID).ptr; }
  uint32_t get_output_size() const { return get_descr(OUTPUT_DES_ID).size; }
  void* get_local_ptr() const { return get_descr(LOCAL_DES_ID).ptr; }
  void* get_refout_ptr() const { return get_descr(REFOUT_DES_ID).ptr; }
  virtual uint32_t get_elements_count() const = 0;

  // Print output
  virtual void print_output() const = 0;

  // Return name
  virtual std::string Name() const = 0;

 protected:
  // Set system memory descriptor
  bool set_sys_descr(const uint32_t& id, const uint32_t& size) {
    return set_mem_descr(id, size, false);
  }

  // Set local memory descriptor
  bool set_local_descr(const uint32_t& id, const uint32_t& size) {
    return set_mem_descr(id, size, true);
  }

  // Get memory descriptor
  mem_descr_t get_descr(const uint32_t& id) const {
    mem_const_it_t it = mem_map_.find(id);
    return (it != mem_map_.end()) ? it->second : null_descriptor();
  }

 private:
  // Set memory descriptor
  bool set_mem_descr(const uint32_t& id, const uint32_t& size, const bool& local) {
    const mem_descr_t des = {NULL, size, local};
    auto ret = mem_map_.insert(mem_map_t::value_type(id, des));
    return ret.second;
  }

  // Kernel memory map object
  mem_map_t mem_map_;
};

#endif  // _TEST_KERNEL_H_
