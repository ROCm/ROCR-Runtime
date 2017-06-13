#ifndef _AQL_PROFILE_H_
#define _AQL_PROFILE_H_

#include "hsa_ext_amd_aql_profile.h"

namespace pm4_profile {
class CommandWriter;
}

namespace aql_profile {

typedef hsa_ext_amd_aql_profile_descriptor_t descriptor_t;
typedef hsa_ext_amd_aql_profile_profile_t profile_t;
typedef hsa_ext_amd_aql_profile_info_type_t info_type_t;
typedef hsa_ext_amd_aql_profile_data_callback_t data_callback_t;
typedef hsa_ext_amd_aql_pm4_packet_t packet_t;
typedef hsa_ext_amd_aql_profile_event_t event_t;

void populateAql(void* cmdBuffer, uint32_t cmdSz, pm4_profile::CommandWriter* cmdWriter,
                 packet_t* aql_packet);
void* legacyAqlAcquire(const packet_t* aql_packet, void* data);
void* legacyAqlRelease(const packet_t* aql_packet, void* data);
void* legacyPm4(const packet_t* aql_packet, void* data);
}

#endif  // _AQL_PROFILE_H_
