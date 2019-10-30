////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2020, Advanced Micro Devices, Inc. All rights reserved.
//
// Developed by:
//
//                 AMD Research and AMD HSA Software Development
//
//                 Advanced Micro Devices, Inc.
//
//                 www.amd.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal with the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
//  - Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimers.
//  - Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimers in
//    the documentation and/or other materials provided with the distribution.
//  - Neither the names of Advanced Micro Devices, Inc,
//    nor the names of its contributors may be used to endorse or promote
//    products derived from this Software without specific prior written
//    permission.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS WITH THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////

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
          case 3:
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
