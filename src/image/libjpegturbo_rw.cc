#include "libjpegturbo_rw.h"

#include <turbojpeg.h>

#include "base/minlog.h"
#include "base/thread.h"

namespace rad {

std::unique_ptr<Image> LibJpegTurboRW::Decode(const uint8_t* data, size_t size) {
  tjhandle handle = ::tjInitDecompress();
  rad::ScopeExit([=] { ::tjDestroy(handle); });

  int width, height, jpeg_subsamp, jpeg_colorspace;
  int ret = ::tjDecompressHeader3(handle, data, (unsigned long)size, &width, &height,
      &jpeg_subsamp, &jpeg_colorspace);
  if (ret != 0) {
    LOG_F(WARNING, "failed to tjDecompressHeader3() %s", tjGetErrorStr2(handle));
    return {};
  }

  std::unique_ptr<Image> image(new Image{
      .width = width,
      .height = height,
      .stride = (size_t)width * 4,
      .buffer = ImageBuffer::Alloc(width * height * 4),
      .pixel_format = PixelFormatType::rgba8,
      .color_space = ColorSpaceType::sRGB,
      .decoder = DecoderType::libjpegturbo,
  });

  ret = ::tjDecompress2(handle, data, (unsigned long)size, image->buffer->data, width,
      width * 4, height, TJPF_RGBA, 0);
  if (ret != 0) {
    LOG_F(WARNING, "failed to tjDecompress2() %s", tjGetErrorStr2(handle));
    return {};
  }

  return image;
}

}  // namespace rad
