#include "pnm_rw.h"

#include <bitset>
#include <cassert>
#include <fstream>
#include <iosfwd>
#include <pnm.hpp>

#include "base/io.h"

namespace rad {

struct membuf : std::streambuf {
  membuf(char* base, std::ptrdiff_t n) : begin(base), end(base + n) {
    this->setg(base, base, base + n);
  }

  virtual pos_type seekoff(off_type off, std::ios_base::seekdir dir,
      std::ios_base::openmode which = std::ios_base::in) override {
    if (dir == std::ios_base::cur)
      gbump((int)off);
    else if (dir == std::ios_base::end)
      setg(begin, end + off, end);
    else if (dir == std::ios_base::beg)
      setg(begin, begin + off, end);

    return gptr() - eback();
  }

  virtual pos_type seekpos(
      std::streampos pos, std::ios_base::openmode mode) override {
    return seekoff(pos - pos_type(off_type(0)), std::ios_base::beg, mode);
  }

  char *begin, *end;
};

std::unique_ptr<Image> PnmRW::Decode(const uint8_t* data, size_t size) {
  membuf membuf((char*)data, size);
  std::istream ifs(&membuf);

  PNM::Info info;
  std::vector<std::uint8_t> buf;
  ifs >> PNM::load(buf, info);

  int w = static_cast<int>(info.width());
  int h = static_cast<int>(info.height());
  int ch = static_cast<int>(info.channel());
  int depth = static_cast<int>(info.depth());
  int value_max = static_cast<int>(info.max());

  if (ch != 1 && ch != 3) {
    throw std::runtime_error("unexpected channel.");
  }
  if (depth != 1 && depth != 8) {
    throw std::runtime_error("unexpected bit depth.");
  }

  const uint8_t* src = buf.data();
  std::unique_ptr<Image> image(new Image{
      .width = w,
      .height = h,
      .stride = (size_t)w * 4,
      .buffer = ImageBuffer::Alloc(w * h * 4),
      .pixel_format = PixelFormatType::rgba8,
      .color_space = ColorSpaceType::sRGB,
      .decoder = DecoderType::pnm,
  });

  if (depth == 8) {
    uint8_t* dst = image->buffer->data;
    if (ch == 1) {
      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          uint8_t v = src[y * w + x];
          dst[y * w + x] = 255 | v << 16 | v << 8 | v;
        }
      }
    } else {
      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          uint8_t r = src[y * w * 3 + x * 3 + 0];
          uint8_t g = src[y * w * 3 + x * 3 + 1];
          uint8_t b = src[y * w * 3 + x * 3 + 2];
          dst[y * w + x] = 255 | b << 16 | g << 8 | r;
        }
      }
    }
  } else if (depth == 1) {
    const uint8_t* src = buf.data();
    uint32_t* dst = (uint32_t*)image->buffer->data;
    if (ch == 1) {
      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          uint8_t v = src[y * w + x] * 255 / value_max;
          dst[y * w + x] = 255 << 24 | v << 16 | v << 8 | v;
        }
      }
    } else {
      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          uint8_t r = src[y * w * 3 + x * 3 + 0] * 255 / value_max;
          uint8_t g = src[y * w * 3 + x * 3 + 1] * 255 / value_max;
          uint8_t b = src[y * w * 3 + x * 3 + 2] * 255 / value_max;
          dst[y * w + x] = 255 << 24 | b << 16 | g << 8 | r;
        }
      }
    }
  } else {
    assert(false && "unexpected bit depth.");
    return nullptr;
  }

  return image;
}

}  // namespace rad
