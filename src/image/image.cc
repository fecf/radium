#include "image.h"

#include <cassert>

#include "base/minlog.h"
#include "base/io.h"

#include "types.h"
#include "image_rw_factory.h"

namespace rad {

inline size_t getPitch(ImageFormat format, int width) noexcept {
  if (format == ImageFormat::RGBA8) {
    return width * 4;
  } else if (format == ImageFormat::BGRA8) {
    return width * 4;
  } else if (format == ImageFormat::RGBA16) {
    return width * 2 * 4;
  } else if (format == ImageFormat::RGBA32F) {
    return width * 4 * 4;
  }
  assert(false && "not implemented.");
  return width * 4 * 4;
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

std::unique_ptr<Image> Image::Load(const uint8_t* data, size_t size, const char* format) {
  ImageRWFactory factory;
  auto rw = factory.CreatePreferredImageRW(format);
  if (!rw) {
    LOG_F(WARNING, "failed to create decoder for %s.", format);
    return nullptr;
  }
  return rw->Read(data, size);
}

std::unique_ptr<Image> Image::Load(const std::string& path) {
  ImageRWFactory factory;
  auto rw = factory.CreatePreferredImageRW(path);
  if (!rw) {
    LOG_F(WARNING, "failed to create decoder for %s.", path.c_str());
    return nullptr;
  }

  FileStream fs(path);
  if (!fs.valid()) {
    LOG_F(WARNING, "failed to open file %s.", path.c_str());
    return nullptr;
  }

  std::vector<uint8_t> buf(fs.Size());
  fs.Read(buf.data(), fs.Size());

  return rw->Read(buf.data(), buf.size());
}

Image::Image(int width, int height, size_t stride, ImageFormat format,
    int channels, ColorSpace cs)
    : width_(width),
      height_(height),
      stride_(stride),
      format_(format),
      channels_(channels),
      cs_(cs),
      size_(stride * height)
{
  data_ = new uint8_t[stride * height];
  deleter_ = [](void* ptr) { delete[] ptr; };
}

Image::Image(int width, int height, size_t stride, ImageFormat format,
    int channels, ColorSpace cs, uint8_t* data)
    : width_(width),
      height_(height),
      stride_(stride),
      format_(format),
      channels_(channels),
      cs_(cs),
      data_(data),
      size_(stride * height) {
  deleter_ = [](void* ptr) { delete[] ptr; };
}

Image::Image(int width, int height, size_t stride, ImageFormat format,
    int channels, ColorSpace cs, uint8_t* data,
    std::function<void(void*)> deleter)
    : width_(width),
      height_(height),
      stride_(stride),
      format_(format),
      channels_(channels),
      cs_(cs),
      data_(data),
      size_(stride * height),
      deleter_(deleter) {}

Image::~Image() {
  assert(deleter_);
  if (deleter_) {
    deleter_(data_);
  }
}

std::unique_ptr<Image> Image::Resize(
    int dst_width, int dst_height, ResizeFilter filter) const {

  size_t size = getPitch(format_, dst_width) * dst_height;
  uint8_t* dst = new uint8_t[size];
  if (dst == nullptr) {
    assert(false && "failed to allocate memory.");
    return nullptr;
  }

  size_t dst_stride = 0;
  if (format_ == ImageFormat::RGBA8 || format_ == ImageFormat::BGRA8) {
    resizeNN<uchar4>(reinterpret_cast<const uchar4*>(data_),
        reinterpret_cast<uchar4*>(dst), width_, height_, dst_width, dst_height);
    dst_stride = dst_width * sizeof(uchar4);
  } else if (format_ == ImageFormat::RGBA16) {
    resizeNN<ushort4>(reinterpret_cast<const ushort4*>(data_),
        reinterpret_cast<ushort4*>(dst), width_, height_, dst_width,
        dst_height);
    dst_stride = dst_width * sizeof(ushort4);
  } else if (format_ == ImageFormat::RGBA32F) {
    resizeNN<float4>(reinterpret_cast<const float4*>(data_),
        reinterpret_cast<float4*>(dst), width_, height_, dst_width, dst_height);
    dst_stride = dst_width * sizeof(float4);
  }

  std::unique_ptr<Image> image(new Image(
      dst_width, dst_height, dst_stride, format_, channels_, cs_, dst));
  return image;
}

ImageDecoder::~ImageDecoder() {}

std::unique_ptr<Image> ImageDecoder::Read(const uint8_t* data, size_t size) {
  throw std::domain_error("not implemented.");
}

}  // namespace rad
