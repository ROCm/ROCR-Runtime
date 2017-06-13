#include <string>

#include "aql_profile.h"
#include "pm4_factory.h"
#include "cmdwriter.h" // commandwriter
#include "hsa_perf.h" // perfcounter
#include "thread_trace.h" // threadtrace
#include "gpu_enum.h"
#include "gpu_blockinfo.h"

#define PUBLIC_API __attribute__((visibility("default")))

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
  CommandBufferMgr(const profile_t* profile)
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
    return (buffer.size != 0) ? buffer.ptr + buffer.size : NULL;
  }

  bool setPreSize(const uint32_t& size) {
    bool suc = (size <= buffer.size);
    if (suc) info->precmds_size = size;
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

static inline bool is_event_match(const event_t& event1, const event_t& event2) {
  return (event1.block_name == event2.block_name) && (event1.block_index == event2.block_index) &&
      (event1.counter_id == event2.counter_id);
}

hsa_status_t default_pmcdata_callback(hsa_ext_amd_aql_profile_info_type_t info_type,
                                      hsa_ext_amd_aql_profile_info_data_t* info_data,
                                      void* callback_data) {
  hsa_status_t status = HSA_STATUS_SUCCESS;
  hsa_ext_amd_aql_profile_info_data_t* passed_data =
      reinterpret_cast<hsa_ext_amd_aql_profile_info_data_t*>(callback_data);

  if (info_type == HSA_EXT_AQL_PROFILE_INFO_PMC_DATA) {
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

hsa_status_t default_sqttdata_callback(hsa_ext_amd_aql_profile_info_type_t info_type,
                                       hsa_ext_amd_aql_profile_info_data_t* info_data,
                                       void* callback_data) {
  hsa_status_t status = HSA_STATUS_SUCCESS;
  hsa_ext_amd_aql_profile_info_data_t* passed_data =
      reinterpret_cast<hsa_ext_amd_aql_profile_info_data_t*>(callback_data);

  if (info_type == HSA_EXT_AQL_PROFILE_INFO_SQTT_DATA) {
    if (info_data->sample_id == passed_data->sample_id) {
      passed_data->sqtt_data = info_data->sqtt_data;
      status = HSA_STATUS_INFO_BREAK;
    }
  }

  return status;
}

}  // aql_profile

extern "C" {

// Check if event is valid for the specific GPU
PUBLIC_API hsa_status_t hsa_ext_amd_aql_profile_validate_event(
    hsa_agent_t agent, const hsa_ext_amd_aql_profile_event_t* event, bool* result) {
  return HSA_STATUS_SUCCESS;
}

// Method to populate the provided AQL packet with profiling start commands
PUBLIC_API hsa_status_t hsa_ext_amd_aql_profile_start(
    const hsa_ext_amd_aql_profile_profile_t* profile, aql_profile::packet_t* aql_start_packet) {

  aql_profile::Pm4Factory * pm4_factory = aql_profile::Pm4Factory::Create(profile);
  if (pm4_factory == NULL) return HSA_STATUS_ERROR;

  pm4_profile::CommandWriter* cmdWriter = pm4_factory->getCommandWriter();
  if (cmdWriter == NULL) return HSA_STATUS_ERROR;

  pm4_profile::DefaultCmdBuf commands;
  aql_profile::CommandBufferMgr cmdBufMgr(profile);
  if (cmdBufMgr.getSize() == 0) return HSA_STATUS_ERROR;

  if (profile->type == HSA_EXT_AQL_PROFILE_EVENT_PMC) {
    pm4_profile::Pmu* pmcMgr = pm4_factory->getPmcMgr();
    if (pmcMgr == NULL) return HSA_STATUS_ERROR;

    pmcMgr->setPmcDataBuff((uint8_t*)profile->output_buffer.ptr, profile->output_buffer.size);

    for (const hsa_ext_amd_aql_profile_event_t* p = profile->events;
         p < profile->events + profile->event_count; ++p) {
      pm4_profile::CounterBlock* block =
          pmcMgr->getCounterBlockById(pm4_factory->getBlockId(p));
      if (block == NULL) return HSA_STATUS_ERROR;

      pm4_profile::Counter* counter = block->createCounter();
      if (counter == NULL) return HSA_STATUS_ERROR;

      counter->setParameter(HSA_EXT_TOOLS_COUNTER_PARAMETER_EVENT_INDEX, sizeof(uint32_t),
                            &(p->counter_id));
      counter->setEnable(true);
    }

    // Generate start commands
    pmcMgr->begin(&commands, cmdWriter);
    cmdBufMgr.setPreSize(commands.Size());
    // Generate stop commands
    pmcMgr->end(&commands, cmdWriter);
  } else if (profile->type == HSA_EXT_AQL_PROFILE_EVENT_SQTT) {
    pm4_profile::ThreadTrace* sqttMgr = pm4_factory->getSqttMgr();
    if (sqttMgr == NULL) return HSA_STATUS_ERROR;

    pm4_profile::ThreadTraceConfig sqtt_config;
    sqttMgr->InitThreadTraceConfig(&sqtt_config);
    if (profile->parameters) {
      for (const hsa_ext_amd_aql_profile_parameters_t* p = profile->parameters;
           p < (profile->parameters + profile->parameter_count); ++p) {
        switch (p->parameter_name) {
          case HSA_EXT_AQL_PROFILE_PARAM_COMPUTE_UNIT_TARGET:
            sqtt_config.threadTraceTargetCu = p->value;
            break;
          case HSA_EXT_AQL_PROFILE_PARAM_VM_ID_MASK:
            sqtt_config.threadTraceVmIdMask = p->value;
            break;
          case HSA_EXT_AQL_PROFILE_PARAM_MASK:
            sqtt_config.threadTraceMask = p->value;
            break;
          case HSA_EXT_AQL_PROFILE_PARAM_TOKEN_MASK:
            sqtt_config.threadTraceTokenMask = p->value;
            break;
          case HSA_EXT_AQL_PROFILE_PARAM_TOKEN_MASK2:
            sqtt_config.threadTraceTokenMask2 = p->value;
            break;
          default:
            return HSA_STATUS_ERROR;
        }
      }
    }
    sqttMgr->Init(&sqtt_config);

    sqttMgr->setSqttDataBuff((uint8_t*)profile->output_buffer.ptr, profile->output_buffer.size);

    const uint32_t status_size = sqttMgr->StatusSizeInfo();
    void* status_ptr = cmdBufMgr.setPostfix(status_size);
    if (status_ptr == NULL) return HSA_STATUS_ERROR;
    // Control buffer registering
    sqttMgr->setSqttCtrlBuff((uint32_t*)status_ptr);

    // Generate start commands
    sqttMgr->BeginSession(&commands, cmdWriter);
    cmdBufMgr.setPreSize(commands.Size());
    // Generate stop commands
    sqttMgr->StopSession(&commands, cmdWriter);
  } else
    return HSA_STATUS_ERROR;

  if (!cmdBufMgr.checkTotalSize(commands.Size())) return HSA_STATUS_ERROR;

  const aql_profile::descriptor_t pre_descr = cmdBufMgr.getPreDescr();
  const aql_profile::descriptor_t post_descr = cmdBufMgr.getPostDescr();
  memcpy(pre_descr.ptr, commands.Base(), pre_descr.size);
  memcpy(post_descr.ptr, commands.Base() + pre_descr.size, post_descr.size);
  // Populate start aql packet
  aql_profile::populateAql(pre_descr.ptr, pre_descr.size, cmdWriter, aql_start_packet);

  return HSA_STATUS_SUCCESS;
}

// Method to populate the provided AQL packet with profiling stop commands
PUBLIC_API hsa_status_t hsa_ext_amd_aql_profile_stop(
    const hsa_ext_amd_aql_profile_profile_t* profile, aql_profile::packet_t* aql_stop_packet) {

  aql_profile::Pm4Factory * pm4_factory = aql_profile::Pm4Factory::Create(profile);
  if (pm4_factory == NULL) return HSA_STATUS_ERROR;

  pm4_profile::CommandWriter* cmdWriter = pm4_factory->getCommandWriter();
  if (cmdWriter == NULL) return HSA_STATUS_ERROR;

  aql_profile::CommandBufferMgr cmdBufMgr(profile);
  if (cmdBufMgr.getSize() == 0) return HSA_STATUS_ERROR;

  const aql_profile::descriptor_t post_descr = cmdBufMgr.getPostDescr();
  // Populate stop aql packet
  aql_profile::populateAql(post_descr.ptr, post_descr.size, cmdWriter, aql_stop_packet);

  return HSA_STATUS_SUCCESS;
}

// GFX8 support, converting of the profiling AQL packet to PM4 packet blob
PUBLIC_API hsa_status_t hsa_ext_amd_aql_profile_legacy_get_pm4(
    const aql_profile::packet_t* aql_packet, void* data) {
  // Populate GFX8 pm4 packet blob
  // Adding HSA barrier acquire packet
  data = aql_profile::legacyAqlAcquire(aql_packet, data);
  // Adding PM4 command packet
  data = aql_profile::legacyPm4(aql_packet, data);
  // Adding HSA barrier release packet
  data = aql_profile::legacyAqlRelease(aql_packet, data);
  return HSA_STATUS_SUCCESS;
}

// Method for getting the profile info
PUBLIC_API hsa_status_t hsa_ext_amd_aql_profile_get_info(
    const hsa_ext_amd_aql_profile_profile_t* profile, hsa_ext_amd_aql_profile_info_type_t attribute,
    void* value) {
  hsa_status_t status = HSA_STATUS_SUCCESS;

  switch (attribute) {
    case HSA_EXT_AQL_PROFILE_INFO_COMMAND_BUFFER_SIZE:
      *(uint32_t*)value = 0x1000;  // a current approximation as 4K is big enaugh
      break;
    case HSA_EXT_AQL_PROFILE_INFO_PMC_DATA_SIZE:
      *(uint32_t*)value = 0x1000;  // a current approximation as 4K is big enaugh
      break;
    case HSA_EXT_AQL_PROFILE_INFO_PMC_DATA:
      reinterpret_cast<hsa_ext_amd_aql_profile_info_data_t*>(value)->pmc_data.result = 0;
      status = hsa_ext_amd_aql_profile_iterate_data(profile, aql_profile::default_pmcdata_callback,
                                                    value);
      break;
    case HSA_EXT_AQL_PROFILE_INFO_SQTT_DATA:
      status = hsa_ext_amd_aql_profile_iterate_data(profile, aql_profile::default_sqttdata_callback,
                                                    value);
      break;
    default:
      status = HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return status;
}

// Method for iterating the events output data
PUBLIC_API hsa_status_t hsa_ext_amd_aql_profile_iterate_data(
    const hsa_ext_amd_aql_profile_profile_t* profile,
    hsa_ext_amd_aql_profile_data_callback_t callback, void* data) {

  hsa_status_t status = HSA_STATUS_SUCCESS;
  aql_profile::Pm4Factory * pm4_factory = aql_profile::Pm4Factory::Create(profile);
  if (pm4_factory == NULL) return HSA_STATUS_ERROR;

  if (profile->type == HSA_EXT_AQL_PROFILE_EVENT_PMC) {
    uint32_t info_size = 0;
    void* info_data;
    uint64_t* samples = (uint64_t*)profile->output_buffer.ptr;
    const uint32_t sample_count = profile->output_buffer.size / sizeof(uint64_t);
    uint32_t sample_index = 0;

    pm4_profile::Pmu* pmcMgr = pm4_factory->getPmcMgr();
    if (pmcMgr == NULL) return HSA_STATUS_ERROR;

    for (const hsa_ext_amd_aql_profile_event_t* p = profile->events;
         p < (profile->events + profile->event_count); ++p) {
      pm4_profile::CounterBlock* block =
          pmcMgr->getCounterBlockById(pm4_factory->getBlockId(p));
      if (block == NULL) return HSA_STATUS_ERROR;
      if (!block->getInfo(pm4_profile::GPU_BLK_INFO_CONTROL_METHOD, info_size, &info_data)) {
        return HSA_STATUS_ERROR;
      }
      const pm4_profile::CntlMethod method =
          static_cast<pm4_profile::CntlMethod>(*(static_cast<uint32_t*>(info_data)));
      // A perfcounter data sample per ShaderEngine
      const uint32_t block_samples_count = (method == pm4_profile::CntlMethodBySe ||
                                            method == pm4_profile::CntlMethodBySeAndInstance)
          ? pmcMgr->getNumSe()
          : 1;
      for (uint32_t i = 0; i < block_samples_count; ++i) {
        assert(sample_index < sample_count);
        if (sample_index >= sample_count) return HSA_STATUS_ERROR;

        hsa_ext_amd_aql_profile_info_data_t sample_info;
        sample_info.sample_id = i;
        sample_info.pmc_data.event = *p;
        sample_info.pmc_data.result = samples[sample_index];
        status = callback(HSA_EXT_AQL_PROFILE_INFO_PMC_DATA, &sample_info, data);
        if (status == HSA_STATUS_INFO_BREAK) {
          status = HSA_STATUS_SUCCESS;
          break;
        }
        if (status != HSA_STATUS_SUCCESS) break;
        ++sample_index;
      }
    }
  } else if (profile->type == HSA_EXT_AQL_PROFILE_EVENT_SQTT) {
    pm4_profile::ThreadTrace* sqttMgr = pm4_factory->getSqttMgr();
    if (sqttMgr == NULL) return HSA_STATUS_ERROR;

    aql_profile::CommandBufferMgr cmdBufMgr(profile);
    if (cmdBufMgr.getSize() == 0) return HSA_STATUS_ERROR;

    const uint32_t status_size = sqttMgr->StatusSizeInfo();
    // Control buffer was allocated as the CmdBuffer postfix partition
    void* status_ptr = cmdBufMgr.setPostfix(status_size);
    if (status_ptr == NULL) return HSA_STATUS_ERROR;
    // Control buffer registering
    sqttMgr->setSqttCtrlBuff((uint32_t*)status_ptr);
    // Validate SQTT status and normalize WRPTR
    if (sqttMgr->Validate() == false) return HSA_STATUS_ERROR;

    const uint32_t se_number = sqttMgr->getNumSe();
    // Casting status pointer to SQTT control per ShaderEngine array
    aql_profile::sqtt_ctrl_t* sqtt_ctrl = (aql_profile::sqtt_ctrl_t*)status_ptr;
    assert(status_size == sizeof(aql_profile::sqtt_ctrl_t) * se_number);
    if (status_size != sizeof(aql_profile::sqtt_ctrl_t) * se_number) {
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

      hsa_ext_amd_aql_profile_info_data_t sample_info;
      sample_info.sample_id = i;
      sample_info.sqtt_data.ptr = sample_ptr;
      sample_info.sqtt_data.size = sample_size;
      status = callback(HSA_EXT_AQL_PROFILE_INFO_SQTT_DATA, &sample_info, data);
      if (status == HSA_STATUS_INFO_BREAK) {
        status = HSA_STATUS_SUCCESS;
        break;
      }
      if (status != HSA_STATUS_SUCCESS) break;

      sample_ptr += sample_capacity;
    }
  } else {
    status = HSA_STATUS_ERROR;
  }

  return status;
}
}
