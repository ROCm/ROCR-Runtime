#include "aql_profile.h"

#include <string>
#include <map>
#include <vector>

#include "pm4_factory.h"
#include "cmdwriter.h"     // commandwriter
#include "perf_counter.h"  // perfcounter
#include "thread_trace.h"  // threadtrace
#include "gpu_block_info.h"
#include "logger.h"

#define PUBLIC_API __attribute__((visibility("default")))
#define DESTRUCTOR_API __attribute__((destructor))
#define ERR_CHECK(cond, err, msg)                                                                  \
  {                                                                                                \
    if (cond) {                                                                                    \
      ERR_LOGGING << msg;                                                                          \
      return err;                                                                                  \
    }                                                                                              \
  }

namespace aql_profile {

// Command buffer partitioning manager
// Supports Pre/Post commands partitioning
// and postfix control partition
class CommandBufferMgr {
  const static uint32_t align_size = 0x100;
  const static uint32_t align_mask = align_size - 1;

  struct info_t {
    uint32_t precmds_size;
    uint32_t postcmds_size;
  };

  descriptor_t buffer;
  uint32_t postfix_size;
  info_t* info;

  uint32_t align(const uint32_t& size) { return (size + align_mask) & ~align_mask; }

 public:
  explicit CommandBufferMgr(const profile_t* profile)
      : buffer(profile->command_buffer), postfix_size(0), info(NULL) {
    info = (info_t*)setPostfix(sizeof(info_t));
  }

  uint32_t getSize() { return buffer.size; }

  void* setPostfix(const uint32_t& size) {
    if (size > postfix_size) {
      const uint32_t delta = size - postfix_size;
      postfix_size = size;
      buffer.size -= (delta < buffer.size) ? delta : buffer.size;
    }
    if (buffer.size == 0)
      throw aql_profile_exc_msg("CommandBufferMgr::setPostfix(): buffer size set to zero");
    return (buffer.size != 0) ? buffer.ptr + buffer.size : NULL;
  }

  bool setPreSize(const uint32_t& size) {
    bool suc = (size <= buffer.size);
    if (suc) info->precmds_size = size;
    if (!suc)
      throw aql_profile_exc_msg("CommandBufferMgr::setPreSize(): size set out of the buffer");
    return suc;
  }

  uint32_t getPostOffset() { return align(info->precmds_size); }

  bool checkTotalSize(const uint32_t& size) {
    bool suc = (size <= buffer.size);
    if (suc) suc = (size >= info->precmds_size);
    if (suc) {
      info->postcmds_size = size - info->precmds_size;
      suc = ((getPostOffset() + info->postcmds_size) <= buffer.size);
    }
    if (!suc)
      throw aql_profile_exc_msg("CommandBufferMgr::checkTotalSize(): size set out of the buffer");
    return suc;
  }

  descriptor_t getPreDescr() {
    descriptor_t descr;
    descr.ptr = buffer.ptr;
    descr.size = info->precmds_size;
    return descr;
  }

  descriptor_t getPostDescr() {
    descriptor_t descr;
    descr.ptr = buffer.ptr + getPostOffset();
    descr.size = info->postcmds_size;
    return descr;
  }
};

static inline pm4_profile::CountersMap CountersMapCreate(const profile_t* profile,
                                                         const Pm4Factory* pm4_factory) {
  pm4_profile::CountersMap countersMap;
  for (const hsa_ven_amd_aqlprofile_event_t* p = profile->events;
       p < profile->events + profile->event_count; ++p) {
    countersMap[pm4_factory->getBlockId(p)].push_back(p->counter_id);
  }
  return countersMap;
}

typedef std::vector<const event_t*> EventsVec;
static inline EventsVec EventsVecCreate(const profile_t* profile, const Pm4Factory* pm4_factory) {
  pm4_profile::CountersMap countersMap = CountersMapCreate(profile, pm4_factory);

  std::map<uint32_t, const event_t*> id_map;
  for (const hsa_ven_amd_aqlprofile_event_t* p = profile->events;
       p < profile->events + profile->event_count; ++p) {
    id_map.insert(decltype(id_map)::value_type(pm4_factory->getBlockId(p), p));
  }

  // Iterate through the list of blocks/counters to generate correct order events vector
  EventsVec eventsVec;
  for (pm4_profile::CountersMap::const_iterator block_it = countersMap.begin();
       block_it != countersMap.end(); ++block_it) {
    const uint32_t block_id = block_it->first;
    const pm4_profile::CountersVec& counters = block_it->second;
    const uint32_t counter_count = counters.size();

    for (uint32_t ind = 0; ind < counter_count; ++ind) {
      eventsVec.push_back(id_map[block_id] + ind);
    }
  }

  return eventsVec;
}

static inline bool is_event_match(const event_t& event1, const event_t& event2) {
  return (event1.block_name == event2.block_name) && (event1.block_index == event2.block_index) &&
      (event1.counter_id == event2.counter_id);
}

hsa_status_t default_pmcdata_callback(hsa_ven_amd_aqlprofile_info_type_t info_type,
                                      hsa_ven_amd_aqlprofile_info_data_t* info_data,
                                      void* callback_data) {
  hsa_status_t status = HSA_STATUS_SUCCESS;
  hsa_ven_amd_aqlprofile_info_data_t* passed_data =
      reinterpret_cast<hsa_ven_amd_aqlprofile_info_data_t*>(callback_data);

  if (info_type == HSA_VEN_AMD_AQLPROFILE_INFO_PMC_DATA) {
    if (is_event_match(info_data->pmc_data.event, passed_data->pmc_data.event)) {
      if (passed_data->sample_id == UINT32_MAX) {
        passed_data->pmc_data.result += info_data->pmc_data.result;
      } else if (passed_data->sample_id == info_data->sample_id) {
        passed_data->pmc_data.result = info_data->pmc_data.result;
        status = HSA_STATUS_INFO_BREAK;
      }
    }
  }

  return status;
}

struct sqtt_ctrl_t {
  uint32_t status;
  uint32_t counter;
  uint32_t writePtr;
};

hsa_status_t default_sqttdata_callback(hsa_ven_amd_aqlprofile_info_type_t info_type,
                                       hsa_ven_amd_aqlprofile_info_data_t* info_data,
                                       void* callback_data) {
  hsa_status_t status = HSA_STATUS_SUCCESS;
  hsa_ven_amd_aqlprofile_info_data_t* passed_data =
      reinterpret_cast<hsa_ven_amd_aqlprofile_info_data_t*>(callback_data);

  if (info_type == HSA_VEN_AMD_AQLPROFILE_INFO_SQTT_DATA) {
    if (info_data->sample_id == passed_data->sample_id) {
      passed_data->sqtt_data = info_data->sqtt_data;
      status = HSA_STATUS_INFO_BREAK;
    }
  }

  return status;
}

std::mutex Logger::mutex;
Logger* Logger::instance = NULL;
std::mutex Pm4Factory::mutex;
Pm4Factory::instances_t Pm4Factory::instances;

DESTRUCTOR_API void destructor() {
  Logger::Destroy();
  Pm4Factory::Destroy();
}

}  // aql_profile

extern "C" {

PUBLIC_API hsa_status_t hsa_ven_amd_aqlprofile_error_string(const char** str) {
  *str = aql_profile::Logger::LastMessage().c_str();
  return HSA_STATUS_SUCCESS;
}

// Check if event is valid for the specific GPU
PUBLIC_API hsa_status_t hsa_ven_amd_aqlprofile_validate_event(
    hsa_agent_t agent, const hsa_ven_amd_aqlprofile_event_t* event, bool* result) {
  hsa_status_t status = HSA_STATUS_SUCCESS;
  *result = false;

  try {
    aql_profile::Pm4Factory* pm4_factory = aql_profile::Pm4Factory::Create(agent);
    if (pm4_factory->getBlockInfo(event) != NULL) *result = true;
  } catch (aql_profile::event_exception& e) {
    INFO_LOGGING << e.what();
  } catch (std::exception& e) {
    ERR_LOGGING << e.what();
    status = HSA_STATUS_ERROR;
  }

  return status;
}

// Method to populate the provided AQL packet with profiling start commands
PUBLIC_API hsa_status_t hsa_ven_amd_aqlprofile_start(
    const hsa_ven_amd_aqlprofile_profile_t* profile, aql_profile::packet_t* aql_start_packet) {
  try {
    aql_profile::Pm4Factory* pm4_factory = aql_profile::Pm4Factory::Create(profile);
    pm4_profile::CommandWriter* cmdWriter = pm4_factory->getCommandWriter();
    pm4_profile::DefaultCmdBuf commands;
    aql_profile::CommandBufferMgr cmdBufMgr(profile);

    if (profile->type == HSA_VEN_AMD_AQLPROFILE_EVENT_TYPE_PMC) {
      pm4_profile::PerfCounter* pmcMgr = pm4_factory->getPmcMgr();

      // Generate start commands
      const pm4_profile::CountersMap countersMap = CountersMapCreate(profile, pm4_factory);
      pmcMgr->begin(&commands, cmdWriter, countersMap);
      cmdBufMgr.setPreSize(commands.Size());

      // Generate stop commands
      const uint32_t data_size =
          pmcMgr->end(&commands, cmdWriter, countersMap, profile->output_buffer.ptr);
      ERR_CHECK(data_size == 0, HSA_STATUS_ERROR, "PMC mgr end(): data size set to zero");
      assert(data_size <= profile->output_buffer.size);
      if (data_size > profile->output_buffer.size) {
        ERR_LOGGING << "data size assertion failed, data_size(" << data_size << "), buffer size("
                    << profile->output_buffer.size << ")";
        return HSA_STATUS_ERROR;
      }
    } else if (profile->type == HSA_VEN_AMD_AQLPROFILE_EVENT_TYPE_SQTT) {
      pm4_profile::ThreadTrace* sqttMgr = pm4_factory->getSqttMgr();

      pm4_profile::ThreadTraceConfig sqtt_config;
      sqttMgr->InitThreadTraceConfig(&sqtt_config);
      if (profile->parameters) {
        for (const hsa_ven_amd_aqlprofile_parameter_t* p = profile->parameters;
             p < (profile->parameters + profile->parameter_count); ++p) {
          switch (p->parameter_name) {
            case HSA_VEN_AMD_AQLPROFILE_PARAMETER_NAME_COMPUTE_UNIT_TARGET:
              sqtt_config.threadTraceTargetCu = p->value;
              break;
            case HSA_VEN_AMD_AQLPROFILE_PARAMETER_NAME_VM_ID_MASK:
              sqtt_config.threadTraceVmIdMask = p->value;
              break;
            case HSA_VEN_AMD_AQLPROFILE_PARAMETER_NAME_MASK:
              sqtt_config.threadTraceMask = p->value;
              break;
            case HSA_VEN_AMD_AQLPROFILE_PARAMETER_NAME_TOKEN_MASK:
              sqtt_config.threadTraceTokenMask = p->value;
              break;
            case HSA_VEN_AMD_AQLPROFILE_PARAMETER_NAME_TOKEN_MASK2:
              sqtt_config.threadTraceTokenMask2 = p->value;
              break;
            default:
              ERR_LOGGING << "Bad SQTT parameter name (" << p->parameter_name << ")";
              return HSA_STATUS_ERROR_INVALID_ARGUMENT;
          }
        }
      }
      sqttMgr->Init(&sqtt_config);

      sqttMgr->setSqttDataBuff((uint8_t*)profile->output_buffer.ptr, profile->output_buffer.size);

      // Control buffer registering
      const uint32_t status_size = sqttMgr->StatusSizeInfo();
      void* status_ptr = cmdBufMgr.setPostfix(status_size);
      sqttMgr->setSqttCtrlBuff((uint32_t*)status_ptr);

      // Generate start commands
      sqttMgr->BeginSession(&commands, cmdWriter);
      cmdBufMgr.setPreSize(commands.Size());
      // Generate stop commands
      sqttMgr->StopSession(&commands, cmdWriter);
    } else {
      ERR_LOGGING << "Bad profile type (" << profile->type << ")";
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
    }

    cmdBufMgr.checkTotalSize(commands.Size());

    const aql_profile::descriptor_t pre_descr = cmdBufMgr.getPreDescr();
    const aql_profile::descriptor_t post_descr = cmdBufMgr.getPostDescr();
    memcpy(pre_descr.ptr, commands.Base(), pre_descr.size);
    memcpy(post_descr.ptr, commands.Base() + pre_descr.size, post_descr.size);
    // Populate start aql packet
    aql_profile::populateAql(pre_descr.ptr, pre_descr.size, cmdWriter, aql_start_packet);
  } catch (std::exception& e) {
    ERR_LOGGING << e.what();
    return HSA_STATUS_ERROR;
  }

  return HSA_STATUS_SUCCESS;
}

// Method to populate the provided AQL packet with profiling stop commands
PUBLIC_API hsa_status_t hsa_ven_amd_aqlprofile_stop(const hsa_ven_amd_aqlprofile_profile_t* profile,
                                                    aql_profile::packet_t* aql_stop_packet) {
  try {
    aql_profile::Pm4Factory* pm4_factory = aql_profile::Pm4Factory::Create(profile);
    pm4_profile::CommandWriter* cmdWriter = pm4_factory->getCommandWriter();
    aql_profile::CommandBufferMgr cmdBufMgr(profile);

    // Populate stop aql packet
    const aql_profile::descriptor_t post_descr = cmdBufMgr.getPostDescr();
    aql_profile::populateAql(post_descr.ptr, post_descr.size, cmdWriter, aql_stop_packet);
  } catch (std::exception& e) {
    ERR_LOGGING << e.what();
    return HSA_STATUS_ERROR;
  }

  return HSA_STATUS_SUCCESS;
}

// Legacy devices, converting of the profiling AQL packet to PM4 packet blob
PUBLIC_API hsa_status_t
hsa_ven_amd_aqlprofile_legacy_get_pm4(const aql_profile::packet_t* aql_packet, void* data) {
  try {
    // Populate GFX8 pm4 packet blob
    // Adding HSA barrier acquire packet
    data = aql_profile::legacyAqlAcquire(aql_packet, data);
    // Adding PM4 command packet
    data = aql_profile::legacyPm4(aql_packet, data);
    // Adding HSA barrier release packet
    data = aql_profile::legacyAqlRelease(aql_packet, data);
  } catch (std::exception& e) {
    ERR_LOGGING << e.what();
    return HSA_STATUS_ERROR;
  }

  return HSA_STATUS_SUCCESS;
}

// Method for getting the profile info
PUBLIC_API hsa_status_t
hsa_ven_amd_aqlprofile_get_info(const hsa_ven_amd_aqlprofile_profile_t* profile,
                                hsa_ven_amd_aqlprofile_info_type_t attribute, void* value) {
  hsa_status_t status = HSA_STATUS_SUCCESS;

  try {
    switch (attribute) {
      case HSA_VEN_AMD_AQLPROFILE_INFO_COMMAND_BUFFER_SIZE:
        *(uint32_t*)value = 0x1000;  // a current approximation as 4K is big enaugh
        break;
      case HSA_VEN_AMD_AQLPROFILE_INFO_PMC_DATA_SIZE:
        *(uint32_t*)value = 0x1000;  // a current approximation as 4K is big enaugh
        break;
      case HSA_VEN_AMD_AQLPROFILE_INFO_PMC_DATA:
        reinterpret_cast<hsa_ven_amd_aqlprofile_info_data_t*>(value)->pmc_data.result = 0;
        status = hsa_ven_amd_aqlprofile_iterate_data(profile, aql_profile::default_pmcdata_callback,
                                                     value);
        break;
      case HSA_VEN_AMD_AQLPROFILE_INFO_SQTT_DATA:
        status = hsa_ven_amd_aqlprofile_iterate_data(profile,
                                                     aql_profile::default_sqttdata_callback, value);
        break;
      default:
        status = HSA_STATUS_ERROR_INVALID_ARGUMENT;
        ERR_LOGGING << "Invalid attribute (" << attribute << ")";
    }
  } catch (std::exception& e) {
    ERR_LOGGING << e.what();
    return HSA_STATUS_ERROR;
  }

  return status;
}

// Method for iterating the events output data
PUBLIC_API hsa_status_t
hsa_ven_amd_aqlprofile_iterate_data(const hsa_ven_amd_aqlprofile_profile_t* profile,
                                    hsa_ven_amd_aqlprofile_data_callback_t callback, void* data) {
  hsa_status_t status = HSA_STATUS_SUCCESS;

  try {
    aql_profile::Pm4Factory* pm4_factory = aql_profile::Pm4Factory::Create(profile);

    if (profile->type == HSA_VEN_AMD_AQLPROFILE_EVENT_TYPE_PMC) {
      uint32_t info_size = 0;
      void* info_data;
      uint64_t* samples = (uint64_t*)profile->output_buffer.ptr;
      const uint32_t sample_count = profile->output_buffer.size / sizeof(uint64_t);
      uint32_t sample_index = 0;

      pm4_profile::PerfCounter* pmcMgr = pm4_factory->getPmcMgr();

      aql_profile::EventsVec eventsVec = EventsVecCreate(profile, pm4_factory);
      for (aql_profile::EventsVec::const_iterator it = eventsVec.begin(); it != eventsVec.end();
           ++it) {
        const hsa_ven_amd_aqlprofile_event_t* p = *it;
        const pm4_profile::CntlMethod method = pm4_factory->getBlockInfo(p)->method;
        // A perfcounter data sample per ShaderEngine
        const uint32_t block_samples_count = (method == pm4_profile::CntlMethodBySe ||
                                              method == pm4_profile::CntlMethodBySeAndInstance)
            ? pmcMgr->getNumSe()
            : 1;
        for (uint32_t i = 0; i < block_samples_count; ++i) {
          assert(sample_index < sample_count);
          if (sample_index >= sample_count) {
            ERR_LOGGING << "Bad sample index (" << sample_index << "/" << sample_count << ")";
            return HSA_STATUS_ERROR;
          }

          hsa_ven_amd_aqlprofile_info_data_t sample_info;
          sample_info.sample_id = i;
          sample_info.pmc_data.event = *p;
          sample_info.pmc_data.result = samples[sample_index];
          status = callback(HSA_VEN_AMD_AQLPROFILE_INFO_PMC_DATA, &sample_info, data);
          if (status == HSA_STATUS_INFO_BREAK) {
            status = HSA_STATUS_SUCCESS;
            break;
          }
          if (status != HSA_STATUS_SUCCESS) {
            ERR_LOGGING << "PMC data callback error, sample_id(" << i << ") status(" << status
                        << ")";
            break;
          }
          ++sample_index;
        }
      }
    } else if (profile->type == HSA_VEN_AMD_AQLPROFILE_EVENT_TYPE_SQTT) {
      pm4_profile::ThreadTrace* sqttMgr = pm4_factory->getSqttMgr();
      aql_profile::CommandBufferMgr cmdBufMgr(profile);

      // Control buffer was allocated as the CmdBuffer postfix partition
      const uint32_t status_size = sqttMgr->StatusSizeInfo();
      void* status_ptr = cmdBufMgr.setPostfix(status_size);
      // Control buffer registering
      sqttMgr->setSqttCtrlBuff((uint32_t*)status_ptr);
      // Validate SQTT status and normalize WRPTR
      if (sqttMgr->Validate() == false) {
        ERR_LOGGING << "SQTT data corrupted";
        return HSA_STATUS_ERROR;
      }

      const uint32_t se_number = sqttMgr->getNumSe();
      // Casting status pointer to SQTT control per ShaderEngine array
      aql_profile::sqtt_ctrl_t* sqtt_ctrl = (aql_profile::sqtt_ctrl_t*)status_ptr;
      const uint32_t status_size_exp = sizeof(aql_profile::sqtt_ctrl_t) * se_number;
      assert(status_size == status_size_exp);
      if (status_size != status_size_exp) {
        ERR_LOGGING << "Bad SQTT controll data structure"
                    << ", status_size(" << status_size << "), status_size_exp(" << status_size_exp
                    << "), se_number(" << se_number << ")";
        return HSA_STATUS_ERROR;
      }
      // SQTT output buffer and capacity per ShaderEngine
      void* sample_ptr = profile->output_buffer.ptr;
      const uint32_t sample_capacity = profile->output_buffer.size / se_number;
      // The samples sizes are returned in the control buffer
      for (int i = 0; i < se_number; ++i) {
        // WPTR specifies the index in thread trace buffer where next token will be
        // written by hardware. The index is incremented by size of 32 bytes.
        uint32_t sample_size = sqtt_ctrl[i].writePtr * TT_WRITE_PTR_BLK;

        hsa_ven_amd_aqlprofile_info_data_t sample_info;
        sample_info.sample_id = i;
        sample_info.sqtt_data.ptr = sample_ptr;
        sample_info.sqtt_data.size = sample_size;
        status = callback(HSA_VEN_AMD_AQLPROFILE_INFO_SQTT_DATA, &sample_info, data);
        if (status == HSA_STATUS_INFO_BREAK) {
          status = HSA_STATUS_SUCCESS;
          break;
        }
        if (status != HSA_STATUS_SUCCESS) {
          ERR_LOGGING << "SQTT data callback error, sample_id(" << i << ") status(" << status
                      << ")";
          break;
        }

        sample_ptr += sample_capacity;
      }
    } else {
      ERR_LOGGING << "Bad profile type (" << profile->type << ")";
      status = HSA_STATUS_ERROR_INVALID_ARGUMENT;
    }
  } catch (std::exception& e) {
    ERR_LOGGING << e.what();
    return HSA_STATUS_ERROR;
  }

  return status;
}
}
