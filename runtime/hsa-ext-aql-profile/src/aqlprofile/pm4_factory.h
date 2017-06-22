#ifndef _PM4_FACTORY_H_
#define _PM4_FACTORY_H_

#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <climits>

#include "aql_profile.h"
#include "gpu_block_info.h"
#include "aql_profile_exception.h"

namespace pm4_profile {
class CommandWriter;
class Pmu;
class ThreadTrace;
extern GpuBlockInfo Gfx9HwBlocks[];
extern const uint32_t Gfx9HwBlockCount;
extern GpuBlockInfo Gfx8HwBlocks[];
extern const uint32_t Gfx8HwBlockCount;
}

namespace aql_profile {

class Pm4Factory {
 public:
  enum { kBadBlockId = UINT_MAX };

  static Pm4Factory* Create(const hsa_ext_amd_aql_profile_profile_t* profile);
  virtual pm4_profile::CommandWriter* getCommandWriter() = 0;
  virtual pm4_profile::Pmu* getPmcMgr() = 0;
  virtual pm4_profile::ThreadTrace* getSqttMgr() = 0;

  uint32_t getBlockId(const event_t* event) {
    const hsa_ext_amd_aql_profile_block_name_t& block_name = event->block_name;
    if (block_name >= tables.get_block_id_count())
      throw aql_profile_exception<uint32_t>(std::string("Invalid block name, block_name"),
                                            block_name);
    return (block_name < tables.get_block_id_count())
        ? tables.get_block_id_ptr()[block_name] + event->block_index
        : kBadBlockId;
  }
  const pm4_profile::GpuBlockInfo* getBlockInfo(const uint32_t& block_id) {
    const pm4_profile::GpuBlockInfo* info = NULL;
    if (block_id < tables.get_block_info_count()) {
      info = tables.get_block_info_ptr() + block_id;
      if (info->counterGroupId != block_id)
        throw aql_profile_exception<uint32_t>(std::string("Bad block id table, block_id"),
                                              block_id);
    } else
      throw aql_profile_exception<uint32_t>(std::string("Invalid block id, block_id"), block_id);
    return info;
  }
  const pm4_profile::GpuBlockInfo* getBlockInfo(const event_t* event) {
    const uint32_t block_id = getBlockId(event);
    return getBlockInfo(block_id);
  }

 protected:
  class tables_t {
   public:
    tables_t(uint32_t* dp, uint32_t dc, pm4_profile::GpuBlockInfo* ip, uint32_t ic)
        : block_id_ptr(dp), block_id_count(dc), block_info_ptr(ip), block_info_count(ic) {}
    tables_t(const tables_t& t)
        : block_id_ptr(t.block_id_ptr),
          block_id_count(t.block_id_count),
          block_info_ptr(t.block_info_ptr),
          block_info_count(t.block_info_count) {}
    tables_t() : block_id_ptr(0), block_id_count(0), block_info_ptr(0), block_info_count(0) {}

    uint32_t* get_block_id_ptr() { return block_id_ptr; }
    uint32_t get_block_id_count() { return block_id_count; }
    pm4_profile::GpuBlockInfo* get_block_info_ptr() { return block_info_ptr; }
    uint32_t get_block_info_count() { return block_info_count; }

   private:
    uint32_t* block_id_ptr;
    uint32_t block_id_count;
    pm4_profile::GpuBlockInfo* block_info_ptr;
    uint32_t block_info_count;
  };

  Pm4Factory(const tables_t& t) { tables = t; }

  static tables_t tables;
};

class Gfx8Factory : public Pm4Factory {
 public:
  Gfx8Factory()
      : Pm4Factory(tables_t(block_id_table, HSA_EXT_AQL_PROFILE_BLOCKS_NUMBER,
                            pm4_profile::Gfx8HwBlocks, pm4_profile::Gfx8HwBlockCount)) {}
  pm4_profile::CommandWriter* getCommandWriter();
  pm4_profile::Pmu* getPmcMgr();
  pm4_profile::ThreadTrace* getSqttMgr();

 private:
  static uint32_t block_id_table[HSA_EXT_AQL_PROFILE_BLOCKS_NUMBER];
};

class Gfx9Factory : public Pm4Factory {
 public:
  Gfx9Factory()
      : Pm4Factory(tables_t(block_id_table, HSA_EXT_AQL_PROFILE_BLOCKS_NUMBER,
                            pm4_profile::Gfx9HwBlocks, pm4_profile::Gfx9HwBlockCount)) {}
  pm4_profile::CommandWriter* getCommandWriter();
  pm4_profile::Pmu* getPmcMgr();
  pm4_profile::ThreadTrace* getSqttMgr();

 private:
  static uint32_t block_id_table[HSA_EXT_AQL_PROFILE_BLOCKS_NUMBER];
};

inline Pm4Factory* Pm4Factory::Create(const hsa_ext_amd_aql_profile_profile_t* profile) {
  Pm4Factory* instance = NULL;
  char agent_name[64];
  hsa_agent_get_info(profile->agent, HSA_AGENT_INFO_NAME, agent_name);

  if (strncmp(agent_name, "gfx801", 6) == 0) {
    throw aql_profile_exception<std::string>(std::string("GFX8 Carrizo is not supported "),
                                             agent_name);
  } else if (strncmp(agent_name, "gfx8", 4) == 0) {
    instance = new Gfx8Factory();
  } else if (strncmp(agent_name, "gfx9", 4) == 0) {
    instance = new Gfx9Factory();
  }

  return instance;
}

}  // aql_profile

#endif  // _PM4_FACTORY_H_
