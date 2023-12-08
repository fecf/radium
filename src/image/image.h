#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "types.h"

namespace rad {

class Image;

class ImageDecoder {
 public:
  virtual ~ImageDecoder();
  virtual std::unique_ptr<Image> Read(const uint8_t* data, size_t size);
};

class Image {
  friend class ImageDecoder;

 public:
  static std::unique_ptr<Image> Load(const uint8_t* data, size_t size, const char* ext);
  static std::unique_ptr<Image> Load(const std::string& path);

  // Create empty image using allocated memory buffer
  Image(int width, int height, size_t stride, ImageFormat format, int channels, ColorSpace cs);
    
  // Create empty image with external memory pointer and default deleter
  Image(int width, int height, size_t stride, ImageFormat format, int channels,
      ColorSpace cs, uint8_t* data);

  // Create empty image with external memory pointer and custom deleter
  Image(int width, int height, size_t stride, ImageFormat format, int channels,
      ColorSpace cs, uint8_t* data, std::function<void(void*)> deleter);

  ~Image();

  int width() const noexcept { return width_; }
  int height() const noexcept { return height_; }
  size_t stride() const noexcept { return stride_; }
  ImageFormat format() const noexcept { return format_; }
  int channels() const noexcept { return channels_; }
  ColorSpace colorspace() const noexcept { return cs_; }
  uint8_t* data() const noexcept { return data_; };
  size_t size() const noexcept { return size_; }

  std::unique_ptr<Image> Resize(int width, int height, ResizeFilter filter) const;

 private:
  int width_;
  int height_;
  size_t stride_;
  ImageFormat format_;
  int channels_;
  ColorSpace cs_;
  uint8_t* data_;
  size_t size_;
  std::function<void(void*)> deleter_;
};

}  // namespace rad

