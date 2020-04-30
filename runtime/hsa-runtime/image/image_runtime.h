// AMD HSA image extension interface file.

#ifndef HSA_RUNTIME_EXT_IMAGE_IMAGE_RUNTIME_H
#define HSA_RUNTIME_EXT_IMAGE_IMAGE_RUNTIME_H

#include <atomic>
#include <map>
#include <mutex>

#include "inc/hsa.h"

#undef HSA_API
#define HSA_API

#include "inc/hsa_ext_image.h"
#include "inc/hsa_ext_amd.h"
#include "blit_kernel.h"
#include "image_manager.h"
#include "util.h"

namespace ext_image {
class ImageRuntime {
 public:
  /// @brief Getter for the ImageRuntime singleton object.
  static ImageRuntime* instance();

  /// @brief Destroy singleton object.
  static void DestroySingleton();

  /// @brief Retrieve maximum size of width, height, depth, array size in pixels
  /// for a particular geometry on a component.
  hsa_status_t GetImageInfoMaxDimension(hsa_agent_t component,
                                        hsa_agent_info_t attribute,
                                        void* value);

  /// @brief Query image support with particular format and geometry.
  hsa_status_t GetImageCapability(hsa_agent_t component,
                                  const hsa_ext_image_format_t& format,
                                  hsa_ext_image_geometry_t geometry,
                                  uint32_t& capability);

  /// @brief Query the size and address alignment of the backing storage of
  /// the image.
  hsa_status_t GetImageSizeAndAlignment(hsa_agent_t component,
                                        const hsa_ext_image_descriptor_t& desc,
                                        hsa_ext_image_data_layout_t image_data_layout,
                                        size_t image_data_row_pitch,
                                        size_t image_data_slice_pitch,
                                        hsa_ext_image_data_info_t& image_info);

  /// @brief Create device image object and return its handle.
  hsa_status_t CreateImageHandle(
      hsa_agent_t component, const hsa_ext_image_descriptor_t& image_descriptor,
      const void* image_data, const hsa_access_permission_t access_permission,
      hsa_ext_image_data_layout_t image_data_layout,
      size_t image_data_row_pitch,
      size_t image_data_slice_pitch,
      hsa_ext_image_t& image);

  /// @brief Create device image object and return its handle.
  hsa_status_t CreateImageHandleWithLayout(
      hsa_agent_t component, const hsa_ext_image_descriptor_t& image_descriptor,
      const hsa_amd_image_descriptor_t* image_layout,
      const void* image_data, const hsa_access_permission_t access_permission,
      hsa_ext_image_t& image);

  /// @brief Destroy the device image object referenced by the handle.
  hsa_status_t DestroyImageHandle(const hsa_ext_image_t& image);

  /// @brief Copy the content of a linear memory to an image object.
  hsa_status_t CopyBufferToImage(const void* src_memory, size_t src_row_pitch,
                                 size_t src_slice_pitch,
                                 const hsa_ext_image_t& dst_image,
                                 const hsa_ext_image_region_t& image_region);

  /// @brief Copy the content of an image object to a linear memory.
  hsa_status_t CopyImageToBuffer(const hsa_ext_image_t& src_image,
                                 void* dst_memory, size_t dst_row_pitch,
                                 size_t dst_slice_pitch,
                                 const hsa_ext_image_region_t& image_region);

  /// @brief Copy the content of an image object to another image object.
  hsa_status_t CopyImage(const hsa_ext_image_t& src_image,
                         const hsa_ext_image_t& dst_image,
                         const hsa_dim3_t& src_origin,
                         const hsa_dim3_t& dst_origin, const hsa_dim3_t size);

  /// @brief Fill the content of an image object with a pattern.
  hsa_status_t FillImage(const hsa_ext_image_t& image, const void* pattern,
                         const hsa_ext_image_region_t& image_region);

  /// @brief Create device sampler object and return its handle.
  hsa_status_t CreateSamplerHandle(
      hsa_agent_t component,
      const hsa_ext_sampler_descriptor_t& sampler_descriptor,
      hsa_ext_sampler_t& sampler);

  /// @brief Destroy the device sampler object referenced by the handle.
  hsa_status_t DestroySamplerHandle(hsa_ext_sampler_t& sampler);

  amd::ImageManager* image_manager(hsa_agent_t agent) {
    std::map<uint64_t, amd::ImageManager*>::iterator it =
        image_managers_.find(agent.handle);
    return (it != image_managers_.end()) ? it->second : NULL;
  }

  amd::BlitKernel& blit_kernel() { return blit_kernel_; }

  size_t cpu_l2_cache_size() const { return cpu_l2_cache_size_; }

  hsa_amd_memory_pool_t kernarg_pool() const {
    return kernarg_pool_;
  }

 private:
  /// @brief Initialize singleton object, must be called once.
  static ImageRuntime* CreateSingleton();

  static hsa_status_t CreateImageManager(hsa_agent_t agent, void* data);

  ImageRuntime();

  ~ImageRuntime();

  void Cleanup();

  /// Pointer to singleton object.
  static std::atomic<ImageRuntime*> instance_;

  static std::mutex instance_mutex_;

  /// @brief Contains mapping of agent and its corresponding ::ImageManager
  ///        object.
  std::map<uint64_t, amd::ImageManager*> image_managers_;

  /// @brief Manages kernel for accessing images.
  amd::BlitKernel blit_kernel_;

  size_t cpu_l2_cache_size_;

  hsa_amd_memory_pool_t kernarg_pool_;

  DISALLOW_COPY_AND_ASSIGN(ImageRuntime);
};

}  // namespace
#endif  // HSA_RUNTIME_EXT_IMAGE_IMAGE_RUNTIME_H
