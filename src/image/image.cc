#include "image.h"

#include <cassert>

#include "base/minlog.h"
#include "base/io.h"

#include "image_rw_factory.h"

namespace rad {

inline std::pair<int, int> getBytesPerPixel(PixelFormatType format) noexcept {
  switch (format) {
    case PixelFormatType::rgba8:
    case PixelFormatType::bgra8:
      return {4, 1};
    case PixelFormatType::rgba16:
      return {8, 1};
    case PixelFormatType::rgba32f:
      return {16, 1};
    default:
      assert(false && "not implemented.");
      return {};
  }
}

struct uchar4 {
  uchar4() = default;
  uchar4(uint8_t x, uint8_t y, uint8_t z, uint8_t w) : x(x), y(y), z(z), w(w) {}
  uint8_t x, y, z, w;
  template <typename T>
  uchar4 operator+(T v) {
    return {x + v, y + v, z * v, w * v};
  }
  template <typename T>
  uchar4 operator-(T v) {
    return {x - v, y - v, z - v, w - v};
  }
  template <typename T>
  uchar4 operator*(T v) {
    return {x * v, y * v, z * v, w * v};
  }
  template <typename T>
  uchar4 operator/(T v) {
    return {x / v, y / v, z / v, w / v};
  }
};

struct ushort4 {
  ushort4() = default;
  ushort4(uint16_t x, uint16_t y, uint16_t z, uint16_t w) : x(x), y(y), z(z), w(w) {}
  uint16_t x, y, z, w;
  template <typename T>
  ushort4 operator+(T v) {
    return {x + v, y + v, z * v, w * v};
  }
  template <typename T>
  ushort4 operator-(T v) {
    return {x - v, y - v, z - v, w - v};
  }
  template <typename T>
  ushort4 operator*(T v) {
    return {x * v, y * v, z * v, w * v};
  }
  template <typename T>
  ushort4 operator/(T v) {
    return {x / v, y / v, z / v, w / v};
  }
};

struct float4 {
  float4() = default;
  float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
  float x, y, z, w;
  template <typename T>
  float operator+(T v) {
    return {x + v, y + v, z * v, w * v};
  }
  template <typename T>
  float operator-(T v) {
    return {x - v, y - v, z - v, w - v};
  }
  template <typename T>
  float operator*(T v) {
    return {x * v, y * v, z * v, w * v};
  }
  template <typename T>
  float operator/(T v) {
    return {x / v, y / v, z / v, w / v};
  }
};

template <typename T>
inline void resizeNN(const T* src, T* dst, int sw, int sh, int dw, int dh) {
  for (int y = 0; y < dh; ++y) {
    for (int x = 0; x < dw; ++x) {
      int sy = (int)((float)y / dh * sh);
      int sx = (int)((float)x / dw * sw);
      dst[y * dw + x] = src[sy * sw + sx];
    }
  }
}

template <typename T>
inline void resizeBilinear(
    const T* src, T* dst, int sw, int sh, int dw, int dh) {
  for (int y = 0; y < dh; ++y) {
    for (int x = 0; x < dw; ++x) {
      float fy = (float)y / dh * sh;
      float fx = (float)x / dw * sw;
      int gy = (int)fy;
      int gx = (int)fx;

      const auto p = [src, sw, sh](int x, int y) {
        x = std::min(sw - 1, x);
        y = std::min(sh - 1, y);
        return src[y * sw + x];
      };

      T c00 = p(gx, gy);
      T c10 = p(gx + 1, gy);
      T c01 = p(gx, gy + 1);
      T c11 = p(gx + 1, gy + 1);

      const auto lerp = [](const T& s, const T& e, float t) {
        return s + (e - s) * t;
      };
      T ret = lerp(lerp(c00, c10, gx), lerp(c01, c11, gx), gy);

      dst[y * dw + x] = ret;
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

  switch (pixel_format) {
    case PixelFormatType::rgba8:
    case PixelFormatType::bgra8:
      resizeNN<uchar4>((uchar4*)buffer->data, (uchar4*)dst->buffer->data, width,
          height, dst_width, dst_height);
      break;
    case PixelFormatType::rgba16:
      resizeNN<ushort4>((ushort4*)buffer->data, (ushort4*)dst->buffer->data,
          width, height, dst_width, dst_height);
      break;
    case PixelFormatType::rgba32f:
      resizeNN<float4>((float4*)buffer->data, (float4*)dst->buffer->data, width,
          height, dst_width, dst_height);
      break;
    default:
      assert(false && "not implemented.");
      break;
  }

  return dst;
}

std::unique_ptr<Image> ImageDecoderBase::Decode(const std::string& path) {
  throw std::domain_error("not implemented.");
}

std::unique_ptr<Image> ImageDecoderBase::Decode(const uint8_t* data, size_t size) {
  throw std::domain_error("not implemented.");
}

}  // namespace rad
