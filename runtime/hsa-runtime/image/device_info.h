//  File:           device_info.h
//  Project:        HSA
//
//  Description:    Interface file for Api to query HSA System - Number of
//                  compute nodes, devices, etc.
//
//                  HsaGetAsicFamilyType()
//                  The private Api is provided to query the Id of Asic
//                  family of the device.
//
//  Copyright (c) 2013-2013 Advanced Micro Devices, Inc. (unpublished)
//
//  All rights reserved.  This notice is intended as a precaution against
//  inadvertent publication and does not imply publication or any waiver
//  of confidentiality.  The year included in the foregoing notice is the
//  year of creation of the work.
//

#ifndef HSA_RUNTIME_CORE_INC_DEVICE_INFO_H_
#define HSA_RUNTIME_CORE_INC_DEVICE_INFO_H_

#include "stdint.h"
#include "inc/hsa.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

uint32_t MajorVerFromDevID(uint32_t dev_id);
uint32_t MinorVerFromDevID(uint32_t dev_id);
uint32_t StepFromDevID(uint32_t dev_id);
uint32_t DevIDToAddrLibFamily(uint32_t dev_id);

hsa_status_t GetGPUAsicID(hsa_agent_t agent, uint32_t *chip_id);
#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // HSA_RUNTIME_CORE_INC_DEVICE_INFO_H_
