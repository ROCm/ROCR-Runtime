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

#ifndef HSA_RUNTIME_CORE_INC_AMD_GPU_PM4_H_
#define HSA_RUNTIME_CORE_INC_AMD_GPU_PM4_H_

#define PM4_HDR_IT_OPCODE_NOP                             0x10
#define PM4_HDR_IT_OPCODE_INDIRECT_BUFFER                 0x3F
#define PM4_HDR_IT_OPCODE_RELEASE_MEM                     0x49
#define PM4_HDR_IT_OPCODE_ACQUIRE_MEM                     0x58

#define PM4_HDR_SHADER_TYPE(x)                            (((x) & 0x1) << 1)
#define PM4_HDR_IT_OPCODE(x)                              (((x) & 0xFF) << 8)
#define PM4_HDR_COUNT(x)                                  (((x) & 0x3FFF) << 16)
#define PM4_HDR_TYPE(x)                                   (((x) & 0x3) << 30)

#define PM4_HDR(it_opcode, pkt_size_dw, gfxip_ver) (  \
  PM4_HDR_SHADER_TYPE((gfxip_ver) == 7 ? 1 : 0)    |  \
  PM4_HDR_IT_OPCODE(it_opcode)                     |  \
  PM4_HDR_COUNT(pkt_size_dw - 2)                   |  \
  PM4_HDR_TYPE(3)                                     \
)

#define PM4_INDIRECT_BUFFER_DW1_IB_BASE_LO(x)              (((x) & 0x3FFFFFFF) << 2)
#define PM4_INDIRECT_BUFFER_DW2_IB_BASE_HI(x)              (((x) & 0xFFFF) << 0)
#define PM4_INDIRECT_BUFFER_DW3_IB_SIZE(x)                 (((x) & 0xFFFFF) << 0)
#define PM4_INDIRECT_BUFFER_DW3_IB_VALID(x)                (((x) & 0x1) << 23)

#define PM4_ACQUIRE_MEM_DW1_COHER_CNTL(x)                  (((x) & 0x7FFFFFFF) << 0)
#  define PM4_ACQUIRE_MEM_COHER_CNTL_TC_WB_ACTION_ENA      (1 << 18)
#  define PM4_ACQUIRE_MEM_COHER_CNTL_TC_ACTION_ENA         (1 << 23)
#  define PM4_ACQUIRE_MEM_COHER_CNTL_SH_KCACHE_ACTION_ENA  (1 << 27)
#  define PM4_ACQUIRE_MEM_COHER_CNTL_SH_ICACHE_ACTION_ENA  (1 << 29)
#define PM4_ACQUIRE_MEM_DW2_COHER_SIZE(x)                  (((x) & 0xFFFFFFFF) << 0)
#define PM4_ACQUIRE_MEM_DW3_COHER_SIZE_HI(x)               (((x) & 0xFF) << 0)

#define PM4_RELEASE_MEM_DW1_EVENT_INDEX(x)                 (((x) & 0xF) << 8)
#  define PM4_RELEASE_MEM_EVENT_INDEX_AQL                  0x7

#endif  // header guard
