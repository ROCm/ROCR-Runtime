//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//
// f32_aql_mec_packets.h
//
//  Trade secret of Advanced Micro Devices, Inc.
//  Copyright 2010, Advanced Micro Devices, Inc., (unpublished)
//
//  All rights reserved.  This notice is intended as a precaution against
//  inadvertent publication and does not imply publication or any waiver
//  of confidentiality.  The year included in the foregoing notice is the
//  year of creation of the work.
//
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#ifndef F32_MEC_AQL_PACKETS_H
#define F32_MEC_AQL_PACKETS_H

namespace pm4_profile {
namespace gfx9 {

//--------------------MEC_AQL_DISPATCH--------------------

#ifndef AQL_MEC_AQL_DISPATCH_DEFINED
#define AQL_MEC_AQL_DISPATCH_DEFINED

typedef struct AQL_MEC_AQL_DISPATCH {
  union {
    struct {
      uint32_t header : 16;
      uint32_t dimensions : 2;
      uint32_t reserved1 : 14;
    } bitfields1;
    uint32_t ordinal1;
  };

  union {
    struct {
      uint32_t workgroupsizex : 16;
      uint32_t workgroupsizey : 16;
    } bitfields2;
    uint32_t ordinal2;
  };

  union {
    struct {
      uint32_t workgroupsizez : 16;
      uint32_t reserved2 : 16;
    } bitfields3;
    uint32_t ordinal3;
  };

  uint32_t gridsizex;

  uint32_t gridsizey;

  uint32_t gridsizez;

  uint32_t privatesegmentsizebytes;

  uint32_t groupsegmentsizebytes;

  uint64_t kernelobjectaddress;

  uint64_t kernargaddress;

  uint64_t reserved3;

  uint64_t completionsignal;

} AQLMEC_AQL_DISPATCH, *PAQLMEC_AQL_DISPATCH;
#endif

//--------------------MEC_AQL_BARRIER--------------------

#ifndef AQL_MEC_AQL_BARRIER_DEFINED
#define AQL_MEC_AQL_BARRIER_DEFINED

typedef struct AQL_MEC_AQL_BARRIER {
  union {
    struct {
      uint32_t header : 16;
      uint32_t type : 16;
    } bitfields1;
    uint32_t ordinal1;
  };

  union {
    struct {
      uint32_t polltime : 16;
      uint32_t reserved1 : 16;
    } bitfields2;
    uint32_t ordinal2;
  };

  uint64_t barrierfield0;

  uint64_t barrierfield1;

  uint64_t barrierfield2;

  uint64_t barrierfield3;

  uint64_t barrierfield4;

  uint64_t reserved2;

  uint64_t completionsignal;

} AQLMEC_AQL_BARRIER, *PAQLMEC_AQL_BARRIER;
#endif

//--------------------MEC_AQL_CALL--------------------

#ifndef AQL_MEC_AQL_CALL_DEFINED
#define AQL_MEC_AQL_CALL_DEFINED

typedef struct AQL_MEC_AQL_CALL {
  union {
    struct {
      uint32_t header : 16;
      uint32_t type : 16;
    } bitfields1;
    uint32_t ordinal1;
  };

  uint32_t reserved1;

  uint64_t returnlocation;

  uint64_t compareaddress;

  uint64_t comparemask;

  uint64_t compareref;

  uint64_t ibbase;

  uint64_t ibsize;

  uint64_t reserved2;

  uint64_t completionsignal;

} AQLMEC_AQL_CALL, *PAQLMEC_AQL_CALL;
#endif

//--------------------MEC_AQL_DMA--------------------

#ifndef AQL_MEC_AQL_DMA_DEFINED
#define AQL_MEC_AQL_DMA_DEFINED

typedef struct AQL_MEC_AQL_DMA {
  union {
    struct {
      uint32_t header : 16;
      uint32_t type : 16;
    } bitfields1;
    uint32_t ordinal1;
  };

  uint32_t reserved1;

  uint64_t returnlocation;

  uint32_t stateobjaddress;

  uint64_t sourceaddress;

  uint64_t destaddress;

  uint64_t size;

  uint64_t reserved2;

  uint64_t completionsignal;

} AQLMEC_AQL_DMA, *PAQLMEC_AQL_DMA;
#endif

//--------------------MEC_AQL_DRAW--------------------

#ifndef AQL_MEC_AQL_DRAW_DEFINED
#define AQL_MEC_AQL_DRAW_DEFINED

typedef struct AQL_MEC_AQL_DRAW {
  union {
    struct {
      uint32_t header : 16;
      uint32_t type : 16;
    } bitfields1;
    uint32_t ordinal1;
  };

  uint32_t maxsize;

  uint32_t indexbase;

  uint32_t indexcount;

  union {
    struct {
      uint32_t indextype : 16;
      uint32_t primtype : 16;
    } bitfields5;
    uint32_t ordinal5;
  };

  uint32_t numinstances;

  uint32_t privatesegmentsizebytes;

  uint32_t groupsegmentsizebytes;

  uint32_t kernelobjectaddress;

  uint32_t kernargaddress;

  uint32_t reserved1;

  uint32_t reserved2;

  uint32_t completionsignal;

} AQLMEC_AQL_DRAW, *PAQLMEC_AQL_DRAW;
#endif

//--------------------MEC_AQL_JUMP--------------------

#ifndef AQL_MEC_AQL_JUMP_DEFINED
#define AQL_MEC_AQL_JUMP_DEFINED
enum MEC_AQL_JUMP_type_enum { type__mec_aql_jump__cond_jump_to_queue_index = 0 };


typedef struct AQL_MEC_AQL_JUMP {
  union {
    struct {
      uint32_t header : 16;
      MEC_AQL_JUMP_type_enum type : 16;
    } bitfields1;
    uint32_t ordinal1;
  };

  uint32_t reserved1;

  uint64_t returnlocation;

  uint64_t compareaddress;

  uint64_t comparemask;

  uint64_t compareref;

  uint64_t ibbase;

  uint64_t ibsize;

  uint64_t reserved2;

  uint64_t completionsignal;

} AQLMEC_AQL_JUMP, *PAQLMEC_AQL_JUMP;
#endif

}  // gfx9
}  // pm4_profile

#endif
