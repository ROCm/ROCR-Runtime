#include "inc/hsa.h"
#include "inc/hsa_ext_amd.h"
#include "inc/hsa_ext_image.h"
#include "image_runtime.h"

#undef HSA_API
#define HSA_API HSA_API_EXPORT

//---------------------------------------------------------------------------//
//  Utilty routines
//---------------------------------------------------------------------------//
static void enforceDefaultPitch(hsa_agent_t agent, const hsa_ext_image_descriptor_t* image_descriptor, size_t& image_data_row_pitch, size_t& image_data_slice_pitch) {
  // Set default pitch
  if (image_data_row_pitch == 0) {
    auto manager = ext_image::ImageRuntime::instance()->image_manager(agent);
    assert((manager != nullptr) && "Image manager should already exit.");
    image_data_row_pitch = image_descriptor->width *
      manager->GetImageProperty(agent, image_descriptor->format, image_descriptor->geometry)
      .element_size;
  }

  // Set default slice pitch
  if ((image_data_slice_pitch == 0) &&
    ((image_descriptor->depth != 0) || (image_descriptor->array_size != 0))) {
      switch (image_descriptor->geometry) {
      case HSA_EXT_IMAGE_GEOMETRY_3D:
      case HSA_EXT_IMAGE_GEOMETRY_2DA:
      case HSA_EXT_IMAGE_GEOMETRY_2DADEPTH: {
        image_data_slice_pitch = image_data_row_pitch * image_descriptor->height;
        break;
                                            }
      case HSA_EXT_IMAGE_GEOMETRY_1DA: {
        image_data_slice_pitch = image_data_row_pitch;
        break;
                                       }
      default:
        fprintf(stderr, "Depth set on single layer image geometry.\n");
        //assert(false && "Depth set on single layer image geometry.");
      }
  }
}

//---------------------------------------------------------------------------//
//  Image APIs
//---------------------------------------------------------------------------//
extern "C" {
	
hsa_status_t HSA_API hsa_amd_image_get_info_max_dim_impl(hsa_agent_t agent,
                                                    hsa_agent_info_t attribute,
                                                    void* value) {
  if (agent.handle == 0) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  if (value == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return ext_image::ImageRuntime::instance()->GetImageInfoMaxDimension(
      agent, attribute, value);
}

hsa_status_t HSA_API
    hsa_ext_image_get_capability_impl(hsa_agent_t agent,
                                 hsa_ext_image_geometry_t image_geometry,
                                 const hsa_ext_image_format_t* image_format,
                                 uint32_t* capability_mask) {
  if (agent.handle == 0) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  if ((image_format == NULL) || (capability_mask == NULL) ||
      (image_geometry < HSA_EXT_IMAGE_GEOMETRY_1D) ||
      (image_geometry > HSA_EXT_IMAGE_GEOMETRY_2DADEPTH)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return ext_image::ImageRuntime::instance()->GetImageCapability(
      agent, *image_format, image_geometry, *capability_mask);
}

hsa_status_t HSA_API hsa_ext_image_data_get_info_impl(
    hsa_agent_t agent, const hsa_ext_image_descriptor_t* image_descriptor,
    hsa_access_permission_t access_permission,
    hsa_ext_image_data_info_t* image_data_info) {
  if (agent.handle == 0) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  if ((image_descriptor == NULL) || (image_data_info == NULL) ||
      (access_permission < HSA_ACCESS_PERMISSION_RO) ||
      (access_permission > HSA_ACCESS_PERMISSION_RW)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return ext_image::ImageRuntime::instance()->GetImageSizeAndAlignment(
      agent, *image_descriptor, HSA_EXT_IMAGE_DATA_LAYOUT_OPAQUE, 0, 0, *image_data_info);
}

hsa_status_t HSA_API
    hsa_ext_image_create_impl(hsa_agent_t agent,
                         const hsa_ext_image_descriptor_t* image_descriptor,
                         const void* image_data,
                         hsa_access_permission_t access_permission,
                         hsa_ext_image_t* image) {
  if (agent.handle == 0) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  if (image_descriptor == NULL || image_data == NULL || image == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return ext_image::ImageRuntime::instance()->CreateImageHandle(
      agent, *image_descriptor, image_data, access_permission,
      HSA_EXT_IMAGE_DATA_LAYOUT_OPAQUE, 0, 0, *image);
}

hsa_status_t HSA_API
    hsa_ext_image_destroy_impl(hsa_agent_t agent, hsa_ext_image_t image) {
  if (agent.handle == 0) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  return ext_image::ImageRuntime::instance()->DestroyImageHandle(image);
}

hsa_status_t HSA_API
    hsa_ext_image_copy_impl(hsa_agent_t agent, hsa_ext_image_t src_image,
                       const hsa_dim3_t* src_offset, hsa_ext_image_t dst_image,
                       const hsa_dim3_t* dst_offset, const hsa_dim3_t* range) {
  if (agent.handle == 0) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  if (src_image.handle == 0 || dst_image.handle == 0 || src_offset == NULL ||
      dst_offset == NULL || range == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return ext_image::ImageRuntime::instance()->CopyImage(
      src_image, dst_image, *src_offset, *dst_offset, *range);
}

hsa_status_t HSA_API
    hsa_ext_image_import_impl(hsa_agent_t agent, const void* src_memory,
                         size_t src_row_pitch, size_t src_slice_pitch,
                         hsa_ext_image_t dst_image,
                         const hsa_ext_image_region_t* image_region) {
  if (agent.handle == 0) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  if (src_memory == NULL || dst_image.handle == 0 || image_region == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return ext_image::ImageRuntime::instance()->CopyBufferToImage(
      src_memory, src_row_pitch, src_slice_pitch, dst_image, *image_region);
}

hsa_status_t HSA_API
    hsa_ext_image_export_impl(hsa_agent_t agent, hsa_ext_image_t src_image,
                         void* dst_memory, size_t dst_row_pitch,
                         size_t dst_slice_pitch,
                         const hsa_ext_image_region_t* image_region) {
  if (agent.handle == 0) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  if (dst_memory == NULL || src_image.handle == 0 || image_region == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return ext_image::ImageRuntime::instance()->CopyImageToBuffer(
      src_image, dst_memory, dst_row_pitch, dst_slice_pitch, *image_region);
}

hsa_status_t HSA_API
    hsa_ext_image_clear_impl(hsa_agent_t agent, hsa_ext_image_t image,
                        const void* data,
                        const hsa_ext_image_region_t* image_region) {
  if (agent.handle == 0) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  if (image.handle == 0 || image_region == NULL || data == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return ext_image::ImageRuntime::instance()->FillImage(image, data,
                                                        *image_region);
};

hsa_status_t HSA_API hsa_ext_sampler_create_impl(
    hsa_agent_t agent, const hsa_ext_sampler_descriptor_t* sampler_descriptor,
    hsa_ext_sampler_t* sampler) {
  if (agent.handle == 0) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  if (sampler_descriptor == NULL || sampler == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return ext_image::ImageRuntime::instance()->CreateSamplerHandle(
      agent, *sampler_descriptor, *sampler);
}

hsa_status_t HSA_API
    hsa_ext_sampler_destroy_impl(hsa_agent_t agent, hsa_ext_sampler_t sampler) {
  if (agent.handle == 0) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  return ext_image::ImageRuntime::instance()->DestroySamplerHandle(sampler);
}

hsa_status_t HSA_API
    hsa_ext_image_get_capability_with_layout_impl(hsa_agent_t agent,
                                 hsa_ext_image_geometry_t image_geometry,
                                 const hsa_ext_image_format_t* image_format,
                                 hsa_ext_image_data_layout_t image_data_layout,
                                 uint32_t* capability_mask) {
  if (agent.handle == 0) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  if ((image_format == NULL) || (capability_mask == NULL) ||
      (image_geometry < HSA_EXT_IMAGE_GEOMETRY_1D) ||
      (image_geometry > HSA_EXT_IMAGE_GEOMETRY_2DADEPTH) ||
      (image_data_layout != HSA_EXT_IMAGE_DATA_LAYOUT_LINEAR)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return ext_image::ImageRuntime::instance()->GetImageCapability(
      agent, *image_format, image_geometry, *capability_mask);
}

hsa_status_t HSA_API hsa_ext_image_data_get_info_with_layout_impl(
    hsa_agent_t agent, const hsa_ext_image_descriptor_t* image_descriptor,
    hsa_access_permission_t access_permission,
    hsa_ext_image_data_layout_t image_data_layout,
    size_t image_data_row_pitch,
    size_t image_data_slice_pitch,
    hsa_ext_image_data_info_t* image_data_info) {
  if (agent.handle == 0) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  if ((image_descriptor == NULL) || (image_data_info == NULL) ||
      (access_permission < HSA_ACCESS_PERMISSION_RO) ||
      (access_permission > HSA_ACCESS_PERMISSION_RW) ||
      (image_data_layout != HSA_EXT_IMAGE_DATA_LAYOUT_LINEAR)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  enforceDefaultPitch(agent, image_descriptor, image_data_row_pitch, image_data_slice_pitch);

  return ext_image::ImageRuntime::instance()->GetImageSizeAndAlignment(
      agent, *image_descriptor, image_data_layout, image_data_row_pitch,
      image_data_slice_pitch, *image_data_info);
}

hsa_status_t HSA_API
    hsa_ext_image_create_with_layout_impl(hsa_agent_t agent,
                         const hsa_ext_image_descriptor_t* image_descriptor,
                         const void* image_data,
                         hsa_access_permission_t access_permission,
                         hsa_ext_image_data_layout_t image_data_layout,
                         size_t image_data_row_pitch,
                         size_t image_data_slice_pitch,
                         hsa_ext_image_t* image) {
  if (agent.handle == 0) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  if (image_descriptor == NULL || image_data == NULL || image == NULL ||
      image_data_layout != HSA_EXT_IMAGE_DATA_LAYOUT_LINEAR) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  enforceDefaultPitch(agent, image_descriptor, image_data_row_pitch, image_data_slice_pitch);

  return ext_image::ImageRuntime::instance()->CreateImageHandle(
      agent, *image_descriptor, image_data, access_permission, image_data_layout,
      image_data_row_pitch, image_data_slice_pitch, *image);
}

hsa_status_t HSA_API hsa_amd_image_create_impl(
    hsa_agent_t agent,
    const hsa_ext_image_descriptor_t *image_descriptor,
	const hsa_amd_image_descriptor_t *image_layout,
    const void *image_data,
    hsa_access_permission_t access_permission,
    hsa_ext_image_t *image)
{
  if (agent.handle == 0) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  if (image_descriptor == NULL || image_data == NULL || image == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return ext_image::ImageRuntime::instance()->CreateImageHandleWithLayout(
      agent, *image_descriptor, image_layout, image_data, access_permission, *image);
}

}
