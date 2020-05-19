#ifndef HSA_RUNTIME_EXT_IMAGE_IMAGE_MANAGER_AI_H
#define HSA_RUNTIME_EXT_IMAGE_IMAGE_MANAGER_AI_H

#include "addrlib/inc/addrinterface.h"
#include "image_manager_kv.h"

namespace rocr {
namespace image {

class ImageManagerAi : public ImageManagerKv {
 public:
  explicit ImageManagerAi();
  virtual ~ImageManagerAi();

  /// @brief Calculate the size and alignment of the backing storage of an
  /// image.
  virtual hsa_status_t CalculateImageSizeAndAlignment(
      hsa_agent_t component, const hsa_ext_image_descriptor_t& desc,
      hsa_ext_image_data_layout_t image_data_layout,
      size_t image_data_row_pitch, size_t image_data_slice_pitch,
      hsa_ext_image_data_info_t& image_info) const;

  /// @brief Fill image structure with device specific image object.
  virtual hsa_status_t PopulateImageSrd(Image& image) const;

  /// @brief Fill image structure with device specific image object using the given format.
  virtual hsa_status_t PopulateImageSrd(Image& image, const metadata_amd_t* desc) const;

  /// @brief Modify device specific image object according to the specified
  /// new format.
  virtual hsa_status_t ModifyImageSrd(Image& image,
                                      hsa_ext_image_format_t& new_format) const;

  /// @brief Fill sampler structure with device specific sampler object.
  virtual hsa_status_t PopulateSamplerSrd(Sampler& sampler) const;

 protected:
  uint32_t GetAddrlibSurfaceInfoAi(hsa_agent_t component,
                             const hsa_ext_image_descriptor_t& desc,
                             Image::TileMode tileMode,
                             size_t image_data_row_pitch,
                             size_t image_data_slice_pitch,
                             ADDR2_COMPUTE_SURFACE_INFO_OUTPUT& out) const;

  bool IsLocalMemory(const void* address) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageManagerAi);
};

}  // namespace image
}  // namespace rocr
#endif  // HSA_RUNTIME_EXT_IMAGE_IMAGE_MANAGER_AI_H
