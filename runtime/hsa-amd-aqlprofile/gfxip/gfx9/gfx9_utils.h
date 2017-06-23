/*
 ***********************************************************************************************************************
 *
 *  Trade secret of Advanced Micro Devices, Inc.
 *  Copyright (c) 2016, Advanced Micro Devices, Inc., (unpublished)
 *
 *  All rights reserved. This notice is intended as a precaution against inadvertent publication and
 *does not imply
 *  publication or any waiver of confidentiality. The year included in the foregoing notice is the
 *year of creation of
 *  the work.
 *
 **********************************************************************************************************************/

#ifndef _GFX9_UTILS_H_
#define _GFX9_UTILS_H_

namespace pm4_profile {
namespace gfx9 {

/*
 * PM4 packet helper constants and macros.
 * Constructed from header file:
 *    core/hw/gfxip/gfx9/chip/gfx9_f32_pfp_pm4_packets_gr.h
 */

// Shift amounts for each field of a type-3 PM4 header:
#define PM4_PREDICATE_SHIFT 0
#define PM4_SHADERTYPE_SHIFT 1
#define PM4_TYPE_SHIFT 30
#define PM4_COUNT_SHIFT 16
#define PM4_OPCODE_SHIFT 8

/*
 * Constructs a PM4 type-3 header and packs it into a uint.
 */
#define PM4_TYPE3_HDR(_opc_, _count_)                                                              \
  (uint32_t)((3) << PM4_TYPE_SHIFT | ((_count_)-2) << PM4_COUNT_SHIFT | (_opc_) << PM4_OPCODE_SHIFT)

// Packet shader types:
#define PM4_SHADER_GRAPHICS 0
#define PM4_SHADER_COMPUTE 1

// Indices into VGT event type table
#define EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP 0
#define EVENT_WRITE_INDEX_ZPASS_DONE 1
#define EVENT_WRITE_INDEX_SAMPLE_PIPELINESTAT 2
#define EVENT_WRITE_INDEX_SAMPLE_STREAMOUTSTATS 3
#define EVENT_WRITE_INDEX_VS_PS_PARTIAL_FLUSH 4
#define EVENT_WRITE_INDEX_ANY_EOP_TIMESTAMP 5
#define EVENT_WRITE_INDEX_ANY_EOS_TIMESTAMP 6
#define EVENT_WRITE_EOS_INDEX_CSDONE_PSDONE 6
#define EVENT_WRITE_INDEX_CACHE_FLUSH_EVENT 7
#define EVENT_WRITE_INDEX_INVALID 0xffffffff

static const uint8_t EventTypeToIndexTable[] = {
    0,                                        // Reserved_0x00 0x00000000
    EVENT_WRITE_INDEX_SAMPLE_STREAMOUTSTATS,  // SAMPLE_STREAMOUTSTATS1
                                              // 0x00000001
    EVENT_WRITE_INDEX_SAMPLE_STREAMOUTSTATS,  // SAMPLE_STREAMOUTSTATS2
                                              // 0x00000002
    EVENT_WRITE_INDEX_SAMPLE_STREAMOUTSTATS,  // SAMPLE_STREAMOUTSTATS3
                                              // 0x00000003
    EVENT_WRITE_INDEX_ANY_EOP_TIMESTAMP,      // CACHE_FLUSH_TS 0x00000004
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // CONTEXT_DONE 0x00000005
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // CACHE_FLUSH 0x00000006
    EVENT_WRITE_INDEX_VS_PS_PARTIAL_FLUSH,    // CS_PARTIAL_FLUSH 0x00000007
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // VGT_STREAMOUT_SYNC 0x00000008
    0,                                        // Reserved_0x09    0x00000009
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // VGT_STREAMOUT_RESET 0x0000000a
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // END_OF_PIPE_INCR_DE 0x0000000b
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // END_OF_PIPE_IB_END 0x0000000c
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // RST_PIX_CNT 0x0000000d
    0,                                        // Reserved_0x0E  0x0000000e
    EVENT_WRITE_INDEX_VS_PS_PARTIAL_FLUSH,    // VS_PARTIAL_FLUSH 0x0000000f
    EVENT_WRITE_INDEX_VS_PS_PARTIAL_FLUSH,    // PS_PARTIAL_FLUSH 0x00000010
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // FLUSH_HS_OUTPUT 0x00000011
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // FLUSH_LS_OUTPUT 0x00000012
    0,                                        // Reserved_0x13 0x00000013
    EVENT_WRITE_INDEX_ANY_EOP_TIMESTAMP,      // CACHE_FLUSH_AND_INV_TS_EVENT
                                              // 0x00000014
    EVENT_WRITE_INDEX_ZPASS_DONE,             // ZPASS_DONE  0x00000015
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // CACHE_FLUSH_AND_INV_EVENT
                                              // 0x00000016
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // PERFCOUNTER_START 0x00000017
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // PERFCOUNTER_STOP 0x00000018
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // PIPELINESTAT_START 0x00000019
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // PIPELINESTAT_STOP 0x0000001a
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // PERFCOUNTER_SAMPLE 0x0000001b
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // FLUSH_ES_OUTPUT 0x0000001c
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // FLUSH_GS_OUTPUT 0x0000001d
    EVENT_WRITE_INDEX_SAMPLE_PIPELINESTAT,    // SAMPLE_PIPELINESTAT 0x0000001e
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // SO_VGTSTREAMOUT_FLUSH 0x0000001f
    EVENT_WRITE_INDEX_SAMPLE_STREAMOUTSTATS,  // SAMPLE_STREAMOUTSTATS
                                              // 0x00000020
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // RESET_VTX_CNT 0x00000021
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // BLOCK_CONTEXT_DONE 0x00000022
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // CS_CONTEXT_DONE 0x00000023
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // VGT_FLUSH 0x00000024
    0,                                        // Reserved_0x25  0x00000025
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // SQ_NON_EVENT 0x00000026
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // SC_SEND_DB_VPZ 0x00000027
    EVENT_WRITE_INDEX_ANY_EOP_TIMESTAMP,      // BOTTOM_OF_PIPE_TS 0x00000028
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // FLUSH_SX_TS 0x00000029
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // DB_CACHE_FLUSH_AND_INV 0x0000002a
    EVENT_WRITE_INDEX_ANY_EOP_TIMESTAMP,      // FLUSH_AND_INV_DB_DATA_TS 0x0000002b
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // FLUSH_AND_INV_DB_META 0x0000002c
    EVENT_WRITE_INDEX_ANY_EOP_TIMESTAMP,      // FLUSH_AND_INV_CB_DATA_TS 0x0000002d
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // FLUSH_AND_INV_CB_META 0x0000002e
    EVENT_WRITE_EOS_INDEX_CSDONE_PSDONE,      // CS_DONE 0x0000002f
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // PS_DONE 0x00000030
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // FLUSH_AND_INV_CB_PIXEL_DATA
                                              // 0x00000031
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // SX_CB_RAT_ACK_REQUEST 0x00000032
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // THREAD_TRACE_START 0x00000033
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // THREAD_TRACE_STOP 0x00000034
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // THREAD_TRACE_MARKER 0x00000035
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // THREAD_TRACE_FLUSH 0x00000036
    EVENT_WRITE_INDEX_ANY_NON_TIMESTAMP,      // THREAD_TRACE_FINISH 0x00000037
};

/// @brief Enum specifying the size of elements of a buffer
enum BufElementSize {
  kBufElementSize2 = 0,
  kBufElementSize4 = 1,
  kBufElementSize8 = 2,
  kBufElementSize16 = 3
};

/// @brief Enum specifying the striding of a buffer
enum BufIndexStride {
  kBufIndexStride8 = 0,
  kBufIndexStride16 = 1,
  kBufIndexStride32 = 2,
  kBufIndexStride64 = 3
};

}  // gfx9
}  // pm4_profile

#endif  // _GFX9_UTILS_H_
