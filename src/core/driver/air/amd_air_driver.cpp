////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2023, Advanced Micro Devices, Inc. All rights reserved.
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

#include "core/inc/amd_air_driver.h"

#include <sys/mman.h>
#include <sys/ioctl.h>

#include "amdair_ioctl.h"
#include "core/inc/amd_aie_aql_queue.h"
#include "inc/hsa.h"

namespace rocr {
namespace AMD {

AirDriver::AirDriver(const std::string name,
                     core::Agent::DeviceType agent_device_type)
    : core::Driver(name, agent_device_type) {}

AirDriver::~AirDriver() {
  munmap(reinterpret_cast<void*>(process_doorbells_), air_page_size_);
}

hsa_status_t AirDriver::GetMemoryProperties(uint32_t node_id,
                                            core::MemProperties &mprops) const {
  mprops.flags_ = MemFlagsHeapTypeDRAM;
  mprops.size_bytes_ = device_dram_heap_size_;
  mprops.virtual_base_addr_ = 0;
  return HSA_STATUS_SUCCESS;
}

hsa_status_t AirDriver::AllocateMemory(void** mem, size_t size,
                                       uint32_t node_id, core::MemFlags flags) {
  amdair_alloc_device_memory_args args{0, 0, node_id, size, 0};

  if (flags & MemFlagsHeapTypeDRAM) {
    args.flags = AMDAIR_IOC_ALLOC_MEM_HEAP_TYPE_DRAM;
  } else if (flags & MemFlagsHeapTypeBRAM) {
    args.flags = AMDAIR_IOC_ALLOC_MEM_HEAP_TYPE_BRAM;
  } else {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (ioctl(fd_, AMDAIR_IOC_ALLOC_DEVICE_MEMORY, &args) == -1) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  *mem = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_,
              args.mmap_offset);

  mem_allocations_[*mem] = args.handle;

  return HSA_STATUS_SUCCESS;
}

hsa_status_t AirDriver::FreeMemory(void *mem, uint32_t node_id) {
  amdair_free_device_memory_args args{0, node_id};

  if (mem_allocations_.find(mem) == mem_allocations_.end()) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  args.handle = mem_allocations_[mem];

  if (ioctl(fd_, AMDAIR_IOC_FREE_DEVICE_MEMORY, &args) == -1) {
    return HSA_STATUS_ERROR_RESOURCE_FREE;
  }

  mem_allocations_.erase(mem);
  munmap(mem, device_dram_heap_size_);

  return HSA_STATUS_SUCCESS;
}

hsa_status_t AirDriver::CreateQueue(core::Queue& queue) {
  AieAqlQueue& aie_queue(static_cast<AieAqlQueue&>(queue));

  amdair_create_queue_args args{0, 0, 0, aie_queue.dram_heap_vaddr_,
                                aie_queue.queue_size_bytes_,
                                aie_queue.node_id_, AMDAIR_QUEUE_DEVICE, 0 , 0};

  if (ioctl(fd_, AMDAIR_IOC_CREATE_QUEUE, &args) == -1) {
    return HSA_STATUS_ERROR_INVALID_QUEUE_CREATION;
  }

  off_t db_offset(args.doorbell_offset);
  off_t queue_offset(args.queue_offset);
  off_t queue_buf_offset(args.queue_buf_offset);

  if (!process_doorbells_) {
    process_doorbells_
        = reinterpret_cast<uint64_t*>(mmap(nullptr, air_page_size_,
                                           PROT_READ | PROT_WRITE, MAP_SHARED,
                                           fd_, db_offset));
  }
  aie_queue.shared_queue_ = reinterpret_cast<core::SharedQueue*>(
      mmap(nullptr, air_page_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_,
           queue_offset));
  aie_queue.shared_queue_->amd_queue.hsa_queue.base_address
      = mmap(nullptr, aie_queue.queue_size_bytes_,
             PROT_READ | PROT_WRITE, MAP_SHARED, fd_, queue_buf_offset);

  aie_queue.hardware_doorbell_ptr_ = process_doorbells_ + args.doorbell_id;
  aie_queue.queue_id_ = args.queue_id;
  aie_queue.doorbell_id_ = args.doorbell_id;

  return HSA_STATUS_SUCCESS;
}

hsa_status_t AirDriver::DestroyQueue(core::Queue& queue) const {
  AieAqlQueue& aie_queue(static_cast<AieAqlQueue&>(queue));
  amdair_destroy_queue_args args{ aie_queue.node_id_, aie_queue.queue_id_,
                                  aie_queue.doorbell_id_ };

  if (ioctl(fd_, AMDAIR_IOC_DESTROY_QUEUE, &args) == -1) {
    return HSA_STATUS_ERROR_INVALID_QUEUE;
  }

  return HSA_STATUS_SUCCESS;
}

} // namespace AMD
} // namespace rocr
