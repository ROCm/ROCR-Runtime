#ifndef _PM4_FACTORY_H_
#define _PM4_FACTORY_H_

#include <string.h>
#include <assert.h>

#include "aql_profile.h"

namespace pm4_profile {
class CommandWriter;
class Pmu;
class ThreadTrace;
}

namespace aql_profile {

class Pm4Factory {
 public:
  static Pm4Factory* Create(const hsa_ext_amd_aql_profile_profile_t* profile);
  virtual pm4_profile::CommandWriter* getCommandWriter() = 0;
  virtual pm4_profile::Pmu* getPmcMgr() = 0;
  virtual pm4_profile::ThreadTrace* getSqttMgr() = 0;
  virtual uint32_t getBlockId(const event_t* event) = 0;
};

class Gfx8Factory : public Pm4Factory {
 public:
  pm4_profile::CommandWriter* getCommandWriter();
  pm4_profile::Pmu* getPmcMgr();
  pm4_profile::ThreadTrace* getSqttMgr();
  uint32_t getBlockId(const event_t* event);

 private:
  static uint32_t block_id_table[HSA_EXT_AQL_PROFILE_BLOCKS_NUMBER];
};

class Gfx9Factory : public Pm4Factory {
 public:
  pm4_profile::CommandWriter* getCommandWriter();
  pm4_profile::Pmu* getPmcMgr();
  pm4_profile::ThreadTrace* getSqttMgr();
  uint32_t getBlockId(const event_t* event);

 private:
  static uint32_t block_id_table[HSA_EXT_AQL_PROFILE_BLOCKS_NUMBER];
};

inline Pm4Factory* Pm4Factory::Create(const hsa_ext_amd_aql_profile_profile_t* profile) {
  Pm4Factory* instance = NULL;
  char agent_name[64];
  hsa_agent_get_info(profile->agent, HSA_AGENT_INFO_NAME, agent_name);
  if (strncmp(agent_name, "gfx8", 4) == 0) {
    instance = new Gfx8Factory();
  } else if (strncmp(agent_name, "gfx9", 4) == 0) {
    instance = new Gfx9Factory();
  }
  return instance;
}

}  // aql_profile

#endif  // _PM4_FACTORY_H_
