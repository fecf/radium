#pragma once

#include <cassert>
#include <stdexcept>

namespace rad {

enum class ResizeFilter { Nearest = 0, Bilinear = 1 };

enum class ImageFormat {
  RGBA8 = 0,
  RGBA16,
  RGBA32F,
  BGRA8,
  Unknown = -1,
};

enum class ColorSpace {
  sRGB = 0,
  Linear,
};

inline int getImageFormatChannels(ImageFormat format) {
  switch (format) {
    case ImageFormat::RGBA8:
    case ImageFormat::RGBA32F:
    case ImageFormat::BGRA8:
      return 4;
    default:
      assert(false && "unknown format.");
      throw std::runtime_error("unknown format.");
  }
}

}  // namespace rad
