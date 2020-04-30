#ifndef AMD_HSA_EXT_IMAGE_IMAGE_LUT_H
#define AMD_HSA_EXT_IMAGE_IMAGE_LUT_H

#include <stdint.h>

#include "inc/hsa_ext_image.h"
#include "resource.h"
#include "util.h"

namespace amd {
class ImageLut {
 public:
  ImageLut() {}

  virtual ~ImageLut() {}

  virtual uint32_t MapGeometry(hsa_ext_image_geometry_t geometry) const = 0;

  virtual ImageProperty MapFormat(const hsa_ext_image_format_t& format,
                                  hsa_ext_image_geometry_t geometry) const = 0;

  virtual Swizzle MapSwizzle(hsa_ext_image_channel_order32_t order) const = 0;

  virtual uint32_t GetMaxWidth(hsa_ext_image_geometry_t geometry) const = 0;

  virtual uint32_t GetMaxHeight(hsa_ext_image_geometry_t geometry) const = 0;

  virtual uint32_t GetMaxDepth(hsa_ext_image_geometry_t geometry) const = 0;

  virtual uint32_t GetMaxArraySize(hsa_ext_image_geometry_t geometry) const = 0;

 private:
   DISALLOW_COPY_AND_ASSIGN(ImageLut);
};

}  // namespace
#endif  // AMD_HSA_EXT_IMAGE_IMAGE_LUT_H
