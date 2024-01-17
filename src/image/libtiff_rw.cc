#include "libtiff_rw.h"

#include "image.h"
#include "base/io.h"
#include "base/minlog.h"

#include <tiffio.h>

namespace {

const char* getSampleFormatName(uint32_t sample_format) {
  static const std::unordered_map<int, const char*> map{
      {SAMPLEFORMAT_INT, "Int"},
      {SAMPLEFORMAT_UINT, "Uint"},
      {SAMPLEFORMAT_IEEEFP, "Float"},
  };
  auto it = map.find(sample_format);
  return it == map.end() ? "Unknown" : it->second;
}

}  // namespace

namespace rad {

std::unique_ptr<Image> LibTiffRW::Decode(const std::string& path) {
  std::unique_ptr<TIFF, decltype(::TIFFClose)*> tiff(
      ::TIFFOpen(path.c_str(), "r"), ::TIFFClose);
  if (!tiff) {
    return {};
  }

  uint32_t width = 0, height = 0, bits_per_sample = 0, rows_per_strip = 0,
           sample_format = 0;
  ::TIFFGetField(tiff.get(), TIFFTAG_IMAGEWIDTH, &width);
  ::TIFFGetField(tiff.get(), TIFFTAG_IMAGELENGTH, &height);
  ::TIFFGetField(tiff.get(), TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
  ::TIFFGetField(tiff.get(), TIFFTAG_ROWSPERSTRIP, &rows_per_strip);
  ::TIFFGetField(tiff.get(), TIFFTAG_SAMPLEFORMAT, &sample_format);

  std::vector<std::pair<std::string, std::string>> metadata{
      {"bits_per_sample", std::to_string(bits_per_sample)},
      {"rows_per_strip", std::to_string(rows_per_strip)},
      {"sample_format", getSampleFormatName(sample_format)},
  };

  PixelFormatType pixel_format = PixelFormatType::rgba8;
  size_t stride = width * 4;
  if (sample_format == SAMPLEFORMAT_IEEEFP) {
    stride = width * 16;
    pixel_format = PixelFormatType::rgba32f;
  } else if (sample_format == SAMPLEFORMAT_UINT ||
             sample_format == SAMPLEFORMAT_INT) {
    if (bits_per_sample == 32) {
      stride = width * 4;
      pixel_format = PixelFormatType::rgba8;
    } else if (bits_per_sample == 64) {
      stride = width * 8;
      pixel_format = PixelFormatType::rgba16;
    }
  }

  std::unique_ptr<Image> image(new Image{
      .width = (int)width,
      .height = (int)height,
      .stride = stride,
      .buffer = ImageBuffer::Alloc(stride * height),
      .decoder = DecoderType::libtiff,
      .pixel_format = pixel_format,
      .metadata = metadata,
  });

  if (sample_format == SAMPLEFORMAT_IEEEFP) {
    for (int y = 0; y < (int)height; y += rows_per_strip) {
      int rows = (y + rows_per_strip > height) ? height - y : rows_per_strip;
      size_t strip = ::TIFFVStripSize(tiff.get(), rows);
      size_t ret = ::TIFFReadEncodedStrip(tiff.get(),
          ::TIFFComputeStrip(tiff.get(), y, 0), image->buffer->data, strip);
      if (ret == -1) {
        break;
      }
    }
  } else {
    int ret = ::TIFFReadRGBAImageOriented(tiff.get(), width, height, (uint32_t*)image->buffer->data, ORIENTATION_LEFTTOP);
    if (!ret) {
      LOG_F(WARNING, "failed to TIFFReadRGBAImage %d", ret);
    }
  }

  return image;
}

}  // namespace rad
