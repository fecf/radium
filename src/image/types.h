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
  RGB8 = 0,
  Unknown = -1,
};

enum class ColorSpace {
  sRGB = 0,
  Linear,
};

}  // namespace rad
