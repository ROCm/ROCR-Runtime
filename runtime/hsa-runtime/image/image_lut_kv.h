#ifndef AMD_HSA_EXT_IMAGE_IMAGE_LUT_KV_H
#define AMD_HSA_EXT_IMAGE_IMAGE_LUT_KV_H

#include "image_lut.h"

namespace rocr {
namespace image {

class ImageLutKv : public ImageLut {
 public:
  ImageLutKv() {}

  virtual ~ImageLutKv() {}

  virtual uint32_t MapGeometry(hsa_ext_image_geometry_t geometry) const;

  virtual ImageProperty MapFormat(const hsa_ext_image_format_t& format,
                                  hsa_ext_image_geometry_t geometry) const;

  virtual Swizzle MapSwizzle(hsa_ext_image_channel_order32_t order) const;

  virtual uint32_t GetMaxWidth(hsa_ext_image_geometry_t geometry) const;

  virtual uint32_t GetMaxHeight(hsa_ext_image_geometry_t geometry) const;

  virtual uint32_t GetMaxDepth(hsa_ext_image_geometry_t geometry) const;

  virtual uint32_t GetMaxArraySize(hsa_ext_image_geometry_t geometry) const;

  uint32_t GetPixelSize(uint8_t data_format, uint8_t data_type) const;

 private:
  // Lookup table of image geometry to device geometry enum.
  static const uint32_t kGeometryLut_[GEOMETRY_COUNT];

  // Lookup table of channel format property. Based on HSA Programmer's
  // Reference Manual 1.0P Table 9-4 Channel Order, Channel type and Image
  // Geometry Combinations.
  static const ImageProperty kPropLut_[ORDER_COUNT][TYPE_COUNT];

  // Lookup table of channel order swizzle.
  static const Swizzle kSwizzleLut_[ORDER_COUNT];

  // Lookup table of image geometry to max dimension.
  // Each record contains four values: widht, height, depth, array_size.
  static const uint32_t kMaxDimensionLut_[GEOMETRY_COUNT][4];

  DISALLOW_COPY_AND_ASSIGN(ImageLutKv);
};

}  // namespace image
}  // namespace rocr
#endif  // AMD_HSA_EXT_IMAGE_IMAGE_LUT_KV_H
