#include "lodepng_rw.h"

#include "base/minlog.h"

#include <lodepng/lodepng.h>

namespace rad {

std::unique_ptr<Image> rad::LodePngRW::Decode(
    const uint8_t* data, size_t size) {

  std::vector<uint8_t> out;
  unsigned int w, h;
  lodepng::State state;
  int error = lodepng::decode(out, w, h, state, (unsigned char*)data, size);
  if (error) {
    LOG_F(WARNING, "lodepng decode error %d: %s", error, lodepng_error_text(error));
    return {};
  }

  ColorPrimaries color_primaries = ColorPrimaries::Unknown;
  TransferCharacteristics transfer_characteristics = TransferCharacteristics::Unknown;
  if (state.info_png.iccp_name != nullptr) {
    std::string iccp_name = state.info_png.iccp_name;
    if (iccp_name == "Rec.2100 PQ") {
      color_primaries = ColorPrimaries::BT2020;
      transfer_characteristics = TransferCharacteristics::ST2084;
    }
  }

  std::unique_ptr<Image> image(new Image{
      .width = (int)w,
      .height = (int)h,
      .stride = (size_t)w * 4,
      .buffer = ImageBuffer::Alloc(w * h * 4),
      .decoder = DecoderType::lodepng,
      .pixel_format = PixelFormatType::rgba8,
      .color_primaries = color_primaries,
      .transfer_characteristics = transfer_characteristics,
  });
  ::memcpy_s(image->buffer->data, image->buffer->size, out.data(), out.size());

  return image;
}

}  // namespace rad