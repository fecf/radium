#include "stb_rw.h"

#include "base/io.h"
#include "base/text.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define __STDC_LIB_EXT1__
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace rad {

std::unique_ptr<Image> StbRW::Decode(const uint8_t* data, size_t size) {
  constexpr size_t kAssumedHeaderSize = 8192;
  static_assert(sizeof(stbi_uc) == sizeof(uint8_t), "sizeof(stbi_uc) == sizeof(uint8_t).");

  // Decode header
  int x, y, comp;
  size_t header_size = std::min(kAssumedHeaderSize, size);
  stbi_info_from_memory(data, (int)header_size, &x, &y, &comp);
  bool is_hdr = stbi_is_hdr_from_memory(data, (int)header_size) > 0;
  bool is_16bit = stbi_is_16_bit_from_memory(data, (int)header_size) > 0;

  // Decode data
  uint8_t* decoded_data = nullptr;
  PixelFormatType pixel_format;
  size_t stride;
  ColorPrimaries color_primaries = ColorPrimaries::sRGB;
  TransferCharacteristics transfer_characteristics = TransferCharacteristics::sRGB;

  if (is_hdr) {
    decoded_data = (uint8_t*)stbi_loadf_from_memory(data, (int)size, &x, &y, &comp, 4);
    stride = x * 4 * 4;
    pixel_format = PixelFormatType::rgba32f;
    transfer_characteristics = TransferCharacteristics::Linear;
  } else {
    if (is_16bit) {
      decoded_data = (uint8_t*)stbi_load_16_from_memory(data, (int)size, &x, &y, &comp, 4);
      stride = x * 4 * 2;
      pixel_format = PixelFormatType::rgba16;
    } else {
      decoded_data = (uint8_t*)stbi_load_from_memory(data, (int)size, &x, &y, &comp, 4);
      stride = x * 4;
      pixel_format = PixelFormatType::rgba8;
    }
  }

  if (!decoded_data) {
    return nullptr;
  }

  return std::unique_ptr<Image>(new Image{
    .width = x,
    .height = y,
    .stride = stride,
    .buffer = ImageBuffer::From(decoded_data, stride * y, stbi_image_free),
    .decoder = DecoderType::stb,
    .pixel_format = pixel_format,
    .color_primaries = color_primaries,
    .transfer_characteristics = transfer_characteristics
  });
}

}  // namespace rad
