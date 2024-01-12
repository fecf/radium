#include "image.h"

#include <cassert>

#include "base/minlog.h"
#include "base/io.h"

#include "image_rw_factory.h"

namespace rad {

std::pair<int, int> getBytesPerPixel(PixelFormatType format) noexcept {
  switch (format) {
    case PixelFormatType::rgba8:
    case PixelFormatType::bgra8:
      return {4, 1};
    case PixelFormatType::rgba16:
    case PixelFormatType::rgba16f:
      return {8, 1};
    case PixelFormatType::rgba32f:
      return {16, 1};
    default:
      assert(false && "not implemented.");
      return {};
  }
}

void resizeNearest(const void* src, void* dst, size_t channels,
    size_t channel_size, int sw, int sh, int dw, int dh) {
  for (int y = 0; y < dh; ++y) {
    int sy = (int)((float)y / dh * sh);
    for (int x = 0; x < dw; ++x) {
      int sx = (int)((float)x / dw * sw);
      size_t src_idx = sy * sw * channel_size * channels + sx * channel_size * channels;
      size_t dst_idx = y * dw * channel_size * channels + x * channel_size * channels;
      ::memcpy((uint8_t*)dst + dst_idx, (uint8_t*)src + src_idx, channels * channel_size);
    }
  }
}

ImageDecoderBase::~ImageDecoderBase() {}

std::unique_ptr<Image> Image::Load(const uint8_t* data, size_t size, const char* format) {
  ImageRWFactory factory;
  auto rw = factory.Create(format);
  if (!rw) {
    LOG_F(WARNING, "failed to create decoder for %s.", format);
    return nullptr;
  }

  std::unique_ptr<rad::Image> image = rw->Decode(data, size);
  return image;
}

std::unique_ptr<Image> Image::Load(const std::string& path) {
  ImageRWFactory factory;
  auto rw = factory.Create(path);
  if (!rw) {
    LOG_F(WARNING, "failed to create decoder for %s.", path.c_str());
    return nullptr;
  }

  MemoryMappedFile mmap(path);
  return rw->Decode((const uint8_t*)mmap.data(), mmap.size());
}

Image::~Image() {}

std::unique_ptr<Image> Image::Resize(
    int dst_width, int dst_height, InterpolationType filter) const {
  std::pair<int, int> bpp = getBytesPerPixel(pixel_format);
  size_t stride = dst_width * (bpp.first + bpp.second - 1) / bpp.second;
  size_t size = stride * dst_height;

  std::unique_ptr<Image> dst(new Image(*this));
  dst->buffer = ImageBuffer::Alloc(size);
  dst->width = dst_width;
  dst->height = dst_height;
  dst->stride = stride;

  size_t channels = 4;
  size_t channel_size;
  switch (pixel_format) {
    case PixelFormatType::rgba8:
    case PixelFormatType::bgra8:
      channel_size = 1;
      break;
    case PixelFormatType::rgba16:
      channel_size = 2;
      break;
    case PixelFormatType::rgba16f:
      channel_size = 2;
      break;
    case PixelFormatType::rgba32f:
      channel_size = 4;
      break;
    default:
      assert(false && "not implemented.");
      break;
  }

  resizeNearest(buffer->data, dst->buffer->data, channels, channel_size, width, height, dst_width, dst_height);
  return dst;
}

std::unique_ptr<Image> ImageDecoderBase::Decode(const std::string& path) {
  throw std::domain_error("not implemented.");
}

std::unique_ptr<Image> ImageDecoderBase::Decode(const uint8_t* data, size_t size) {
  throw std::domain_error("not implemented.");
}

}  // namespace rad
