#ifndef _AQL_PROFILE_H_
#define _AQL_PROFILE_H_

#include <iostream>
#include <string>

#include "hsa_ven_amd_aqlprofile.h"
#include "aql_profile_exception.h"

namespace pm4_profile {
class CommandWriter;
}

namespace aql_profile {
typedef hsa_ven_amd_aqlprofile_descriptor_t descriptor_t;
typedef hsa_ven_amd_aqlprofile_profile_t profile_t;
typedef hsa_ven_amd_aqlprofile_info_type_t info_type_t;
typedef hsa_ven_amd_aqlprofile_data_callback_t data_callback_t;
typedef hsa_ext_amd_aql_pm4_packet_t packet_t;
typedef hsa_ven_amd_aqlprofile_event_t event_t;

void populateAql(const void* cmd_buffer, uint32_t cmd_size, pm4_profile::CommandWriter* cmd_writer,
                 packet_t* aql_packet);
void* legacyAqlAcquire(const packet_t* aql_packet, void* data);
void* legacyAqlRelease(const packet_t* aql_packet, void* data);
void* legacyPm4(const packet_t* aql_packet, void* data);

class event_exception : public aql_profile_exc_val<event_t> {
 public:
  event_exception(const std::string& m, const event_t& ev) : aql_profile_exc_val(m, ev) {}
};

static std::ostream& operator<<(std::ostream& os, const event_t& ev) {
  os << "event( block(" << ev.block_name << "." << ev.block_index << "), Id(" << ev.counter_id
     << "))";
  return os;
}
}  // namespace aql_profile

#endif  // _AQL_PROFILE_H_
