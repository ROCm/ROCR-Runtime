// These lines should be changed later after we get formal Copyright.
//------------------------------------------------------------------------------
//
//  File:           device_info.cpp
//  Project:        HSA
//
//  Description:    Implementation file for Api to query HSA System - Number of
//                  compute nodes, devices, queue properties, etc.
//
//                  HsaGetQueueProperties()
//                  The public Api provided for users to query the properties
//                  of a Queue object.
//
//  Copyright (c) 2013-2013 Advanced Micro Devices, Inc. (unpublished)
//
//  All rights reserved.  This notice is intended as a precaution against
//  inadvertent publication and does not imply publication or any waiver
//  of confidentiality.  The year included in the foregoing notice is the
//  year of creation of the work.

#include <assert.h>
#include <string>

#include "core/inc/hsa_internal.h"
#include "device_info.h"
#include "addrlib/src/amdgpu_asic_addr.h"

namespace rocr {
namespace image {

uint32_t MajorVerFromDevID(uint32_t dev_id) {
  return dev_id/100;
}

uint32_t MinorVerFromDevID(uint32_t dev_id) {
  return (dev_id % 100)/10;
}

uint32_t StepFromDevID(uint32_t dev_id) {
  return (dev_id%100)%10;
}

hsa_status_t GetGPUAsicID(hsa_agent_t agent, uint32_t *chip_id) {
  char asic_name[64];
  assert(chip_id != nullptr);

  hsa_status_t status = HSA::hsa_agent_get_info(
      agent, static_cast<hsa_agent_info_t>(HSA_AGENT_INFO_NAME), &asic_name);
  assert(status == HSA_STATUS_SUCCESS);

  if (status != HSA_STATUS_SUCCESS) {
    return status;
  }
  std::string a_str(asic_name);

  assert(a_str.compare(0, 3, "gfx", 3) == 0);

  a_str.erase(0,3);
  *chip_id = std::stoi(a_str);
  return HSA_STATUS_SUCCESS;
}

uint32_t DevIDToAddrLibFamily(uint32_t dev_id) {
  uint32_t major_ver = MajorVerFromDevID(dev_id);
  uint32_t minor_ver = MinorVerFromDevID(dev_id);
  uint32_t step = StepFromDevID(dev_id);

  // FAMILY_UNKNOWN 0xFF
  // FAMILY_SI     - Southern Islands: Tahiti (P), Pitcairn (PM), Cape Verde (M), Bali (V)
  // FAMILY_TN     - Fusion Trinity: Devastator - DVST (M), Scrapper (V)
  // FAMILY_CI     - Sea Islands: Hawaii (P), Maui (P), Bonaire (M)
  // FAMILY_KV     - Fusion Kaveri: Spectre, Spooky; Fusion Kabini: Kalindi
  // FAMILY_VI     - Volcanic Islands: Iceland (V), Tonga (M)
  // FAMILY_CZ     - Carrizo, Nolan, Amur
  // FAMILY_PI     - Pirate Islands
  // FAMILY_AI     - Arctic Islands
  // FAMILY_RV     - Raven
  // FAMILY_NV     - Navi
  switch (major_ver) {
    case 6:
      switch (minor_ver) {
        case 0:
          switch (step) {
            case 0:
            case 1:
              return FAMILY_SI;

            default:
              return FAMILY_UNKNOWN;
          }
        default:
          return FAMILY_UNKNOWN;
      }

    case 7:
      switch (minor_ver) {
        case 0:
          switch (step) {
            case 0:
            case 1:
            case 2:
              return FAMILY_CI;

            case 3:
              return FAMILY_KV;

            default:
              return FAMILY_UNKNOWN;
          }

        default:
          return FAMILY_UNKNOWN;
      }

    case 8:
      switch (minor_ver) {
        case 0:
          switch (step) {
            case 0:
            case 2:
            case 3:
            case 4:
              return FAMILY_VI;

            case 1:
              return FAMILY_CZ;

            default:
              return FAMILY_UNKNOWN;
          }
        default:
          return FAMILY_UNKNOWN;
      }

    case 9:
      switch (minor_ver) {
        case 0:
          switch (step) {
            case 0:
            case 1:
            case 4:   // Vega12
            case 6:   // Vega20
            case 8:   // Arcturus
              return FAMILY_AI;

            case 2:
            case 3:
              return FAMILY_RV;

            default:
              return FAMILY_UNKNOWN;
          }

        default:
          return FAMILY_UNKNOWN;
      }

      case 10:
        switch (minor_ver) {
          case 0:
          case 1:  // Navi
            switch (step) {
              case 0:
              case 1:
              case 2:
              case 3:
                return FAMILY_NV;

              default:
                return FAMILY_UNKNOWN;
            }

          default:
            return FAMILY_UNKNOWN;
        }

      default:
       return FAMILY_UNKNOWN;
  }

  assert(0);  // We should have already returned
}

}  // namespace image
}  // namespace rocr
