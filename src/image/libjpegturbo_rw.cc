#include "libjpegturbo_rw.h"

#include <turbojpeg.h>

#include "base/minlog.h"

namespace rad {

std::unique_ptr<Image> LibJpegTurboRW::Read(const uint8_t* data, size_t size) {
  tjhandle handle = ::tjInitDecompress();

  int width, height, jpeg_subsamp, jpeg_colorspace;
  int ret;
  ret = ::tjDecompressHeader3(handle, data, (unsigned long)size, &width, &height,
      &jpeg_subsamp, &jpeg_colorspace);
  if (ret != 0) {
    LOG_F(WARNING, "failed to tjDecompressHeader3() %s", tjGetErrorStr2(handle));
    return {};
  }

  std::unique_ptr<Image> image(new Image(width, height, width * 4,
      ImageFormat::RGBA8, 4, ColorSpace::sRGB));

  ret = ::tjDecompress2(handle, data, (unsigned long)size, image->data(), width,
      width * 4, height, TJPF_RGBA, 0);
  if (ret != 0) {
    LOG_F(WARNING, "failed to tjDecompress2() %s", tjGetErrorStr2(handle));
    return {};
  }

  return image;
}

}  // namespace rad
