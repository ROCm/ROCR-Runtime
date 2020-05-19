#ifndef HSA_RUNTIME_EXT_IMAGE_H
#define HSA_RUNTIME_EXT_IMAGE_H

#include "inc/hsa.h"
#include "inc/hsa_ext_amd.h"
#include "inc/hsa_ext_image.h"
#include "core/inc/hsa_ext_interface.h"

//---------------------------------------------------------------------------//
//  APIs that implement Image functionality
//---------------------------------------------------------------------------//

namespace rocr {
namespace image {

hsa_status_t hsa_amd_image_get_info_max_dim(hsa_agent_t agent, hsa_agent_info_t attribute,
                                            void* value);

hsa_status_t hsa_ext_image_get_capability(hsa_agent_t agent,
                                          hsa_ext_image_geometry_t image_geometry,
                                          const hsa_ext_image_format_t* image_format,
                                          uint32_t* capability_mask);

hsa_status_t hsa_ext_image_data_get_info(hsa_agent_t agent,
                                         const hsa_ext_image_descriptor_t* image_descriptor,
                                         hsa_access_permission_t access_permission,
                                         hsa_ext_image_data_info_t* image_data_info);

hsa_status_t hsa_ext_image_create(hsa_agent_t agent,
                                  const hsa_ext_image_descriptor_t* image_descriptor,
                                  const void* image_data, hsa_access_permission_t access_permission,
                                  hsa_ext_image_t* image);

hsa_status_t hsa_ext_image_destroy(hsa_agent_t agent, hsa_ext_image_t image);

hsa_status_t hsa_ext_image_copy(hsa_agent_t agent, hsa_ext_image_t src_image,
                                const hsa_dim3_t* src_offset, hsa_ext_image_t dst_image,
                                const hsa_dim3_t* dst_offset, const hsa_dim3_t* range);

hsa_status_t hsa_ext_image_import(hsa_agent_t agent, const void* src_memory, size_t src_row_pitch,
                                  size_t src_slice_pitch, hsa_ext_image_t dst_image,
                                  const hsa_ext_image_region_t* image_region);

hsa_status_t hsa_ext_image_export(hsa_agent_t agent, hsa_ext_image_t src_image, void* dst_memory,
                                  size_t dst_row_pitch, size_t dst_slice_pitch,
                                  const hsa_ext_image_region_t* image_region);

hsa_status_t hsa_ext_image_clear(hsa_agent_t agent, hsa_ext_image_t image, const void* data,
                                 const hsa_ext_image_region_t* image_region);

hsa_status_t hsa_ext_sampler_create(hsa_agent_t agent,
                                    const hsa_ext_sampler_descriptor_t* sampler_descriptor,
                                    hsa_ext_sampler_t* sampler);

hsa_status_t hsa_ext_sampler_destroy(hsa_agent_t agent, hsa_ext_sampler_t sampler);

hsa_status_t hsa_ext_image_get_capability_with_layout(hsa_agent_t agent,
                                                      hsa_ext_image_geometry_t image_geometry,
                                                      const hsa_ext_image_format_t* image_format,
                                                      hsa_ext_image_data_layout_t image_data_layout,
                                                      uint32_t* capability_mask);

hsa_status_t hsa_ext_image_data_get_info_with_layout(
    hsa_agent_t agent, const hsa_ext_image_descriptor_t* image_descriptor,
    hsa_access_permission_t access_permission, hsa_ext_image_data_layout_t image_data_layout,
    size_t image_data_row_pitch, size_t image_data_slice_pitch,
    hsa_ext_image_data_info_t* image_data_info);

hsa_status_t hsa_ext_image_create_with_layout(
    hsa_agent_t agent, const hsa_ext_image_descriptor_t* image_descriptor, const void* image_data,
    hsa_access_permission_t access_permission, hsa_ext_image_data_layout_t image_data_layout,
    size_t image_data_row_pitch, size_t image_data_slice_pitch, hsa_ext_image_t* image);

hsa_status_t hsa_amd_image_create(hsa_agent_t agent,
                                  const hsa_ext_image_descriptor_t* image_descriptor,
                                  const hsa_amd_image_descriptor_t* image_layout,
                                  const void* image_data, hsa_access_permission_t access_permission,
                                  hsa_ext_image_t* image);

// Update Api table with func pointers that implement functionality
void LoadImage(core::ImageExtTableInternal* image_api,
               decltype(::hsa_amd_image_create)** interface_api);

// Release resources acquired by Image implementation
void ReleaseImageRsrcs();

}  // namespace image
}  // namespace rocr

#endif  //  HSA_RUNTIME_EXT_IMAGE_H
