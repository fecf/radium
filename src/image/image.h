#pragma once

#include <cassert>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "gfx/color_space.h"

namespace rad {

enum class DecoderType {
  Unknown,
  libavif,
  libjpegturbo,
  stb,
  pnm,
  wic,
  wuffs,
  lodepng,
};

enum class FormatType {
  Unknown,
};

enum class PixelFormatType {
  Unknown,
  rgba8,
  rgba16,
  rgba16f,
  rgba32f,
  bgra8,
};

enum class InterpolationType {
  Nearest,
  Bilinear,
};

class Image;

class ImageDecoderBase {
 public:
  virtual ~ImageDecoderBase();
  virtual std::unique_ptr<Image> Decode(const std::string& path);
  virtual std::unique_ptr<Image> Decode(const uint8_t* data, size_t size);
};

class ImageBuffer {
 public:
  ~ImageBuffer() {
    if (deleter) {
      deleter(data);
    }
  }
  static std::shared_ptr<ImageBuffer> Alloc(size_t size) {
    return std::shared_ptr<ImageBuffer>(new ImageBuffer{
        .data = (uint8_t*)::malloc(size),
        .size = size,
        .deleter = ::free,
    });
  }
  static std::shared_ptr<ImageBuffer> From(
      uint8_t* data, size_t size, std::function<void(void*)> deleter) {
    return std::shared_ptr<ImageBuffer>(new ImageBuffer{
        .data = data,
        .size = size,
        .deleter = std::move(deleter),
    });
  }
  uint8_t* data = nullptr;
  size_t size = 0;
  std::function<void(void*)> deleter = nullptr;
};

class Image {
  friend class ImageDecoderBase;

 public:
  static std::unique_ptr<Image> Load(const uint8_t* data, size_t size, const char* ext);
  static std::unique_ptr<Image> Load(const std::string& path);

  ~Image();
  std::unique_ptr<Image> Resize(int width, int height, InterpolationType filter) const;

 public:
  int width = 0;
  int height = 0;
  size_t stride = 0;
  std::shared_ptr<ImageBuffer> buffer;

  DecoderType decoder = DecoderType::Unknown;
  PixelFormatType pixel_format = PixelFormatType::Unknown;
  ColorPrimaries color_primaries = ColorPrimaries::Unknown;
  TransferCharacteristics transfer_characteristics = TransferCharacteristics::Unknown;

  std::vector<std::pair<std::string, std::string>> metadata;
};

}  // namespace rad

