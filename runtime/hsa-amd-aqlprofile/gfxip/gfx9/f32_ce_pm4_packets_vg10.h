//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//
//  Trade secret of Advanced Micro Devices, Inc.
//  Copyright 2014, Advanced Micro Devices, Inc., (unpublished)
//
//  All rights reserved.  This notice is intended as a precaution against
//  inadvertent publication and does not imply publication or any waiver
//  of confidentiality.  The year included in the foregoing notice is the
//  year of creation of the work.
//
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#ifndef F32_CE_PM4_PACKETS_H
#define F32_CE_PM4_PACKETS_H

namespace pm4_profile {
namespace gfx9 {


#ifndef PM4_CE_HEADER_DEFINED
#define PM4_CE_HEADER_DEFINED
typedef union PM4_CE_TYPE_3_HEADER {
  struct {
    uint32_t reserved1 : 8;  ///< reserved
    uint32_t opcode : 8;     ///< IT opcode
    uint32_t count : 14;     ///< number of DWORDs - 1 in the information body.
    uint32_t type : 2;       ///< packet identifier. It should be 3 for type 3 packets
  };
  uint32_t u32All;
} PM4_CE_TYPE_3_HEADER;
#endif  // PM4_CE_HEADER_DEFINED

//--------------------CE_COND_EXEC--------------------

#ifndef PM4_CE_COND_EXEC_DEFINED
#define PM4_CE_COND_EXEC_DEFINED

typedef struct PM4_CE_COND_EXEC {
  union {
    PM4_CE_TYPE_3_HEADER header;  /// header
    uint32_t ordinal1;
  };

  union {
    struct {
      uint32_t reserved1 : 2;
      uint32_t addr_lo : 30;
    } bitfields2;
    uint32_t ordinal2;
  };

  uint32_t addr_hi;

  uint32_t reserved2;

  union {
    struct {
      uint32_t exec_count : 14;
      uint32_t reserved3 : 18;
    } bitfields5;
    uint32_t ordinal5;
  };

} PM4CE_COND_EXEC, *PPM4CE_COND_EXEC;
#endif

//--------------------CE_CONTEXT_CONTROL--------------------

#ifndef PM4_CE_CONTEXT_CONTROL_DEFINED
#define PM4_CE_CONTEXT_CONTROL_DEFINED

typedef struct PM4_CE_CONTEXT_CONTROL {
  union {
    PM4_CE_TYPE_3_HEADER header;  /// header
    uint32_t ordinal1;
  };

  union {
    struct {
      uint32_t reserved1 : 28;
      uint32_t load_ce_ram : 1;
      uint32_t reserved2 : 2;
      uint32_t load_enable : 1;
    } bitfields2;
    uint32_t ordinal2;
  };

  uint32_t reserved3;

} PM4CE_CONTEXT_CONTROL, *PPM4CE_CONTEXT_CONTROL;
#endif

//--------------------CE_COPY_DATA--------------------

#ifndef PM4_CE_COPY_DATA_DEFINED
#define PM4_CE_COPY_DATA_DEFINED
enum CE_COPY_DATA_src_sel_enum {
  src_sel__ce_copy_data__mem_mapped_register = 0,
  src_sel__ce_copy_data__memory = 1,
  src_sel__ce_copy_data__tc_l2 = 2,
  src_sel__ce_copy_data__immediate_data = 5
};

enum CE_COPY_DATA_dst_sel_enum {
  dst_sel__ce_copy_data__mem_mapped_register = 0,
  dst_sel__ce_copy_data__tc_l2 = 2,
  dst_sel__ce_copy_data__memory = 5
};

enum CE_COPY_DATA_src_cache_policy_enum {
  src_cache_policy__ce_copy_data__lru = 0,
  src_cache_policy__ce_copy_data__stream = 1
};

enum CE_COPY_DATA_count_sel_enum {
  count_sel__ce_copy_data__32_bits_of_data = 0,
  count_sel__ce_copy_data__64_bits_of_data = 1
};

enum CE_COPY_DATA_wr_confirm_enum {
  wr_confirm__ce_copy_data__do_not_wait_for_confirmation = 0,
  wr_confirm__ce_copy_data__wait_for_confirmation = 1
};

enum CE_COPY_DATA_dst_cache_policy_enum {
  dst_cache_policy__ce_copy_data__lru = 0,
  dst_cache_policy__ce_copy_data__stream = 1
};

enum CE_COPY_DATA_engine_sel_enum { engine_sel__ce_copy_data__constant_engine = 2 };


typedef struct PM4_CE_COPY_DATA {
  union {
    PM4_CE_TYPE_3_HEADER header;  /// header
    uint32_t ordinal1;
  };

  union {
    struct {
      CE_COPY_DATA_src_sel_enum src_sel : 4;
      uint32_t reserved1 : 4;
      CE_COPY_DATA_dst_sel_enum dst_sel : 4;
      uint32_t reserved2 : 1;
      CE_COPY_DATA_src_cache_policy_enum src_cache_policy : 2;
      uint32_t reserved3 : 1;
      CE_COPY_DATA_count_sel_enum count_sel : 1;
      uint32_t reserved4 : 3;
      CE_COPY_DATA_wr_confirm_enum wr_confirm : 1;
      uint32_t reserved5 : 4;
      CE_COPY_DATA_dst_cache_policy_enum dst_cache_policy : 2;
      uint32_t reserved6 : 3;
      CE_COPY_DATA_engine_sel_enum engine_sel : 2;
    } bitfields2;
    uint32_t ordinal2;
  };

  union {
    struct {
      uint32_t src_reg_offset : 18;
      uint32_t reserved7 : 14;
    } bitfields3a;
    struct {
      uint32_t reserved8 : 2;
      uint32_t src_32b_addr_lo : 30;
    } bitfields3b;
    struct {
      uint32_t reserved9 : 3;
      uint32_t src_64b_addr_lo : 29;
    } bitfields3c;
    uint32_t imm_data;

    uint32_t ordinal3;
  };

  union {
    uint32_t src_memtc_addr_hi;

    uint32_t src_imm_data;

    uint32_t ordinal4;
  };

  union {
    struct {
      uint32_t dst_reg_offset : 18;
      uint32_t reserved10 : 14;
    } bitfields5a;
    struct {
      uint32_t reserved11 : 2;
      uint32_t dst_32b_addr_lo : 30;
    } bitfields5b;
    struct {
      uint32_t reserved12 : 3;
      uint32_t dst_64b_addr_lo : 29;
    } bitfields5c;
    uint32_t ordinal5;
  };

  uint32_t dst_addr_hi;

} PM4CE_COPY_DATA, *PPM4CE_COPY_DATA;
#endif

//--------------------CE_DUMP_CONST_RAM--------------------

#ifndef PM4_CE_DUMP_CONST_RAM_DEFINED
#define PM4_CE_DUMP_CONST_RAM_DEFINED
enum CE_DUMP_CONST_RAM_cache_policy_enum {
  cache_policy__ce_dump_const_ram__lru = 0,
  cache_policy__ce_dump_const_ram__stream = 1,
  cache_policy__ce_dump_const_ram__bypass = 2
};


typedef struct PM4_CE_DUMP_CONST_RAM {
  union {
    PM4_CE_TYPE_3_HEADER header;  /// header
    uint32_t ordinal1;
  };

  union {
    struct {
      uint32_t offset : 16;
      uint32_t reserved1 : 9;
      CE_DUMP_CONST_RAM_cache_policy_enum cache_policy : 2;
      uint32_t reserved2 : 3;
      uint32_t increment_cs : 1;
      uint32_t increment_ce : 1;
    } bitfields2;
    uint32_t ordinal2;
  };

  union {
    struct {
      uint32_t num_dw : 15;
      uint32_t reserved3 : 17;
    } bitfields3;
    uint32_t ordinal3;
  };

  uint32_t addr_lo;

  uint32_t addr_hi;

} PM4CE_DUMP_CONST_RAM, *PPM4CE_DUMP_CONST_RAM;
#endif

//--------------------CE_DUMP_CONST_RAM_OFFSET--------------------

#ifndef PM4_CE_DUMP_CONST_RAM_OFFSET_DEFINED
#define PM4_CE_DUMP_CONST_RAM_OFFSET_DEFINED
enum CE_DUMP_CONST_RAM_OFFSET_cache_policy_enum {
  cache_policy__ce_dump_const_ram_offset__lru = 0,
  cache_policy__ce_dump_const_ram_offset__stream = 1,
  cache_policy__ce_dump_const_ram_offset__bypass = 2
};


typedef struct PM4_CE_DUMP_CONST_RAM_OFFSET {
  union {
    PM4_CE_TYPE_3_HEADER header;  /// header
    uint32_t ordinal1;
  };

  union {
    struct {
      uint32_t offset : 16;
      uint32_t reserved1 : 9;
      CE_DUMP_CONST_RAM_OFFSET_cache_policy_enum cache_policy : 2;
      uint32_t reserved2 : 3;
      uint32_t increment_cs : 1;
      uint32_t increment_ce : 1;
    } bitfields2;
    uint32_t ordinal2;
  };

  union {
    struct {
      uint32_t num_dw : 15;
      uint32_t reserved3 : 17;
    } bitfields3;
    uint32_t ordinal3;
  };

  uint32_t addr_offset;

} PM4CE_DUMP_CONST_RAM_OFFSET, *PPM4CE_DUMP_CONST_RAM_OFFSET;
#endif

//--------------------CE_FRAME_CONTROL--------------------

#ifndef PM4_CE_FRAME_CONTROL_DEFINED
#define PM4_CE_FRAME_CONTROL_DEFINED
enum CE_FRAME_CONTROL_command_enum {
  command__ce_frame_control__tmz_begin = 0,
  command__ce_frame_control__tmz_end = 1
};


typedef struct PM4_CE_FRAME_CONTROL {
  union {
    PM4_CE_TYPE_3_HEADER header;  /// header
    uint32_t ordinal1;
  };

  union {
    struct {
      uint32_t tmz : 1;
      uint32_t reserved1 : 27;
      CE_FRAME_CONTROL_command_enum command : 4;
    } bitfields2;
    uint32_t ordinal2;
  };

} PM4CE_FRAME_CONTROL, *PPM4CE_FRAME_CONTROL;
#endif

//--------------------CE_INCREMENT_CE_COUNTER--------------------

#ifndef PM4_CE_INCREMENT_CE_COUNTER_DEFINED
#define PM4_CE_INCREMENT_CE_COUNTER_DEFINED
enum CE_INCREMENT_CE_COUNTER_cntrsel_enum {
  cntrsel__ce_increment_ce_counter__invalid = 0,
  cntrsel__ce_increment_ce_counter__increment_ce_counter = 1,
  cntrsel__ce_increment_ce_counter__increment_cs_counter = 2,
  cntrsel__ce_increment_ce_counter__increment_ce_and_cs_counters = 3
};


typedef struct PM4_CE_INCREMENT_CE_COUNTER {
  union {
    PM4_CE_TYPE_3_HEADER header;  /// header
    uint32_t ordinal1;
  };

  union {
    struct {
      CE_INCREMENT_CE_COUNTER_cntrsel_enum cntrsel : 2;
      uint32_t reserved1 : 30;
    } bitfields2;
    uint32_t ordinal2;
  };

} PM4CE_INCREMENT_CE_COUNTER, *PPM4CE_INCREMENT_CE_COUNTER;
#endif

//--------------------CE_INDIRECT_BUFFER_CONST--------------------

#ifndef PM4_CE_INDIRECT_BUFFER_CONST_DEFINED
#define PM4_CE_INDIRECT_BUFFER_CONST_DEFINED

typedef struct PM4_CE_INDIRECT_BUFFER_CONST {
  union {
    PM4_CE_TYPE_3_HEADER header;  /// header
    uint32_t ordinal1;
  };

  union {
    struct {
      uint32_t reserved1 : 2;
      uint32_t ib_base_lo : 30;
    } bitfields2;
    uint32_t ordinal2;
  };

  uint32_t ib_base_hi;

  union {
    struct {
      uint32_t ib_size : 20;
      uint32_t chain : 1;
      uint32_t pre_ena : 1;
      uint32_t reserved2 : 2;
      uint32_t vmid : 4;
      uint32_t cache_policy : 2;
      uint32_t pre_resume : 1;
      uint32_t priv : 1;
    } bitfields4;
    uint32_t ordinal4;
  };

} PM4CE_INDIRECT_BUFFER_CONST, *PPM4CE_INDIRECT_BUFFER_CONST;
#endif

//--------------------CE_LOAD_CONST_RAM--------------------

#ifndef PM4_CE_LOAD_CONST_RAM_DEFINED
#define PM4_CE_LOAD_CONST_RAM_DEFINED
enum CE_LOAD_CONST_RAM_cache_policy_enum {
  cache_policy__ce_load_const_ram__lru = 0,
  cache_policy__ce_load_const_ram__stream = 1
};


typedef struct PM4_CE_LOAD_CONST_RAM {
  union {
    PM4_CE_TYPE_3_HEADER header;  /// header
    uint32_t ordinal1;
  };

  uint32_t addr_lo;

  uint32_t addr_hi;

  union {
    struct {
      uint32_t num_dw : 15;
      uint32_t reserved1 : 17;
    } bitfields4;
    uint32_t ordinal4;
  };

  union {
    struct {
      uint32_t start_addr : 16;
      uint32_t reserved2 : 9;
      CE_LOAD_CONST_RAM_cache_policy_enum cache_policy : 2;
      uint32_t reserved3 : 5;
    } bitfields5;
    uint32_t ordinal5;
  };

} PM4CE_LOAD_CONST_RAM, *PPM4CE_LOAD_CONST_RAM;
#endif

//--------------------CE_NOP--------------------

#ifndef PM4_CE_NOP_DEFINED
#define PM4_CE_NOP_DEFINED

typedef struct PM4_CE_NOP {
  union {
    PM4_CE_TYPE_3_HEADER header;  /// header
    uint32_t ordinal1;
  };

  //  uint32_t data_block[];  // N-DWords

} PM4CE_NOP, *PPM4CE_NOP;
#endif

//--------------------CE_PRIME_UTCL2--------------------

#ifndef PM4_CE_PRIME_UTCL2_DEFINED
#define PM4_CE_PRIME_UTCL2_DEFINED
enum CE_PRIME_UTCL2_cache_perm_enum {
  cache_perm__ce_prime_utcl2__read = 0,
  cache_perm__ce_prime_utcl2__write = 1,
  cache_perm__ce_prime_utcl2__execute = 2
};

enum CE_PRIME_UTCL2_prime_mode_enum {
  prime_mode__ce_prime_utcl2__dont_wait_for_xack = 0,
  prime_mode__ce_prime_utcl2__wait_for_xack = 1
};

enum CE_PRIME_UTCL2_engine_sel_enum { engine_sel__ce_prime_utcl2__constant_engine = 2 };


typedef struct PM4_CE_PRIME_UTCL2 {
  union {
    PM4_CE_TYPE_3_HEADER header;  /// header
    uint32_t ordinal1;
  };

  union {
    struct {
      CE_PRIME_UTCL2_cache_perm_enum cache_perm : 3;
      CE_PRIME_UTCL2_prime_mode_enum prime_mode : 1;
      uint32_t reserved1 : 26;
      CE_PRIME_UTCL2_engine_sel_enum engine_sel : 2;
    } bitfields2;
    uint32_t ordinal2;
  };

  uint32_t addr_lo;

  uint32_t addr_hi;

  union {
    struct {
      uint32_t requested_pages : 14;
      uint32_t reserved2 : 18;
    } bitfields5;
    uint32_t ordinal5;
  };

} PM4CE_PRIME_UTCL2, *PPM4CE_PRIME_UTCL2;
#endif

//--------------------CE_SET_BASE--------------------

#ifndef PM4_CE_SET_BASE_DEFINED
#define PM4_CE_SET_BASE_DEFINED
enum CE_SET_BASE_base_index_enum {
  base_index__ce_set_base__ce_dst_base_addr = 2,
  base_index__ce_set_base__ce_partition_bases = 3
};


typedef struct PM4_CE_SET_BASE {
  union {
    PM4_CE_TYPE_3_HEADER header;  /// header
    uint32_t ordinal1;
  };

  union {
    struct {
      CE_SET_BASE_base_index_enum base_index : 4;
      uint32_t reserved1 : 28;
    } bitfields2;
    uint32_t ordinal2;
  };

  union {
    struct {
      uint32_t reserved2 : 3;
      uint32_t address_lo : 29;
    } bitfields3a;
    struct {
      uint32_t cs1_index : 16;
      uint32_t reserved3 : 16;
    } bitfields3b;
    uint32_t ordinal3;
  };

  union {
    uint32_t address_hi;

    struct {
      uint32_t cs2_index : 16;
      uint32_t reserved4 : 16;
    } bitfields4b;
    uint32_t ordinal4;
  };

} PM4CE_SET_BASE, *PPM4CE_SET_BASE;
#endif

//--------------------CE_SWITCH_BUFFER--------------------

#ifndef PM4_CE_SWITCH_BUFFER_DEFINED
#define PM4_CE_SWITCH_BUFFER_DEFINED

typedef struct PM4_CE_SWITCH_BUFFER {
  union {
    PM4_CE_TYPE_3_HEADER header;  /// header
    uint32_t ordinal1;
  };

  union {
    struct {
      uint32_t tmz : 1;
      uint32_t reserved1 : 31;
    } bitfields2;
    uint32_t ordinal2;
  };

} PM4CE_SWITCH_BUFFER, *PPM4CE_SWITCH_BUFFER;
#endif

//--------------------CE_WAIT_ON_DE_COUNTER_DIFF--------------------

#ifndef PM4_CE_WAIT_ON_DE_COUNTER_DIFF_DEFINED
#define PM4_CE_WAIT_ON_DE_COUNTER_DIFF_DEFINED

typedef struct PM4_CE_WAIT_ON_DE_COUNTER_DIFF {
  union {
    PM4_CE_TYPE_3_HEADER header;  /// header
    uint32_t ordinal1;
  };

  uint32_t diff;

} PM4CE_WAIT_ON_DE_COUNTER_DIFF, *PPM4CE_WAIT_ON_DE_COUNTER_DIFF;
#endif

//--------------------CE_WRITE_CONST_RAM--------------------

#ifndef PM4_CE_WRITE_CONST_RAM_DEFINED
#define PM4_CE_WRITE_CONST_RAM_DEFINED

typedef struct PM4_CE_WRITE_CONST_RAM {
  union {
    PM4_CE_TYPE_3_HEADER header;  /// header
    uint32_t ordinal1;
  };

  union {
    struct {
      uint32_t offset : 16;
      uint32_t reserved1 : 16;
    } bitfields2;
    uint32_t ordinal2;
  };

  //  uint32_t data[];  // N-DWords

} PM4CE_WRITE_CONST_RAM, *PPM4CE_WRITE_CONST_RAM;
#endif

//--------------------CE_WRITE_DATA--------------------

#ifndef PM4_CE_WRITE_DATA_DEFINED
#define PM4_CE_WRITE_DATA_DEFINED
enum CE_WRITE_DATA_dst_sel_enum {
  dst_sel__ce_write_data__mem_mapped_register = 0,
  dst_sel__ce_write_data__memory = 5,
  dst_sel__ce_write_data__preemption_meta_memory = 8
};

enum CE_WRITE_DATA_addr_incr_enum {
  addr_incr__ce_write_data__increment_address = 0,
  addr_incr__ce_write_data__do_not_increment_address = 1
};

enum CE_WRITE_DATA_wr_confirm_enum {
  wr_confirm__ce_write_data__do_not_wait_for_write_confirmation = 0,
  wr_confirm__ce_write_data__wait_for_write_confirmation = 1
};

enum CE_WRITE_DATA_cache_policy_enum {
  cache_policy__ce_write_data__lru = 0,
  cache_policy__ce_write_data__stream = 1
};

enum CE_WRITE_DATA_engine_sel_enum { engine_sel__ce_write_data__constant_engine = 2 };


typedef struct PM4_CE_WRITE_DATA {
  union {
    PM4_CE_TYPE_3_HEADER header;  /// header
    uint32_t ordinal1;
  };

  union {
    struct {
      uint32_t reserved1 : 8;
      CE_WRITE_DATA_dst_sel_enum dst_sel : 4;
      uint32_t reserved2 : 4;
      CE_WRITE_DATA_addr_incr_enum addr_incr : 1;
      uint32_t reserved3 : 2;
      uint32_t resume_vf : 1;
      CE_WRITE_DATA_wr_confirm_enum wr_confirm : 1;
      uint32_t reserved4 : 4;
      CE_WRITE_DATA_cache_policy_enum cache_policy : 2;
      uint32_t reserved5 : 3;
      CE_WRITE_DATA_engine_sel_enum engine_sel : 2;
    } bitfields2;
    uint32_t ordinal2;
  };

  union {
    struct {
      uint32_t dst_mmreg_addr : 18;
      uint32_t reserved6 : 14;
    } bitfields3a;
    struct {
      uint32_t reserved7 : 2;
      uint32_t dst_mem_addr_lo : 30;
    } bitfields3b;
    uint32_t ordinal3;
  };

  uint32_t dst_mem_addr_hi;

  //  uint32_t data[];  // N-DWords

} PM4CE_WRITE_DATA, *PPM4CE_WRITE_DATA;
#endif

}  // gfx9
}  // pm4_profile

#endif
