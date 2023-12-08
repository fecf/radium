#include "stb_rw.h"

#include "base/io.h"
#include "base/text.h"
#include "image/types.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define __STDC_LIB_EXT1__
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace rad {

std::unique_ptr<Image> StbRW::Read(const uint8_t* data, size_t size) {
  // Decode header
  int x, y, comp;
  {
    constexpr size_t kAssumedHeaderSize = 8192;
    size_t header_size = std::min(kAssumedHeaderSize, size);
    stbi_info_from_memory(data, (int)header_size, &x, &y, &comp);
  }

  // Decode data
  uint8_t* decoded = nullptr;
  ImageFormat format;
  ColorSpace cs;
  size_t stride;

  int is_16bit = stbi_is_16_bit_from_memory(data, (int)size);
  int is_hdr = stbi_is_hdr_from_memory(data, (int)size);
  if (is_16bit) {
    decoded = (uint8_t*)stbi_load_16_from_memory(data, (int)size, &x, &y, &comp, 4);
    stride = x * 4 * 2;
    format = ImageFormat::RGBA16;
    cs = ColorSpace::sRGB;
  } else {
    static_assert(sizeof(stbi_uc) == sizeof(uint8_t),
        "sizeof(stbi_uc) == sizeof(uint8_t).");
    decoded = (uint8_t*)stbi_load_from_memory(data, (int)size, &x, &y, &comp, 4);
    format = ImageFormat::RGBA8;
    cs = ColorSpace::sRGB;
    stride = x * 4;
  }
  if (!decoded) {
    return nullptr;
  }

  std::unique_ptr<Image> image(
      new Image(x, y, stride, format, 3, cs, decoded, ::stbi_image_free));
  return image;
}

}  // namespace rad
