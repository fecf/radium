#pragma once

#include <cassert>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#pragma once

namespace rad {

enum class DecoderType {
  unknown,
  libavif,
  libjpegturbo,
  stb,
  pnm,
  wic,
  wuffs,
};

enum class FormatType {
  unknown,
  bmp,
  jpg,
};

enum class PixelFormatType {
  unknown,
  rgba8 = 0,
  rgba16,
  rgba32f,
  bgra8,
};

enum class ColorSpaceType {
  unknown,
  srgb,
  linear,
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
  FormatType format = FormatType::unknown;
  PixelFormatType pixel_format = PixelFormatType::unknown;
  ColorSpaceType color_space = ColorSpaceType::unknown;
  DecoderType decoder = DecoderType::unknown;
  std::vector<std::pair<std::string, std::string>> metadata;
};

}  // namespace rad

