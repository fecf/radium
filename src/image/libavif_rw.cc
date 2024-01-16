#include "libavif_rw.h"

#include "base/io.h"
#include "base/minlog.h"

#include <map>
#include <avif/avif.h>

const char* getRangeName(avifRange range) {
  static const std::unordered_map<int, const char*> map{
      {AVIF_RANGE_FULL, "Full"},
      {AVIF_RANGE_LIMITED, "Limited"},
  };
  auto it = map.find(range);
  return it == map.end() ? "Unknown" : it->second;
}

const char* getChromaSamplePositionName(avifChromaSamplePosition chromaSamplePosition) {
  static const std::unordered_map<int, const char*> map{
      {AVIF_CHROMA_SAMPLE_POSITION_UNKNOWN, "Unknown"},
      {AVIF_CHROMA_SAMPLE_POSITION_VERTICAL, "Vertical"},
      {AVIF_CHROMA_SAMPLE_POSITION_COLOCATED, "Colocated"},
  };
  auto it = map.find(chromaSamplePosition);
  return it == map.end() ? "Unknown" : it->second;
}

const char* getColorPrimariesName(avifColorPrimaries colorPrimaries) {
  static const std::unordered_map<int, const char*> map{
      {AVIF_COLOR_PRIMARIES_UNKNOWN, "Unknown"},
      {AVIF_COLOR_PRIMARIES_BT709, "BT709"},
      {AVIF_COLOR_PRIMARIES_IEC61966_2_4, "IEC61966-2-4"},
      {AVIF_COLOR_PRIMARIES_UNSPECIFIED, "Unspecified"},
      {AVIF_COLOR_PRIMARIES_BT470M, "BT470M"},
      {AVIF_COLOR_PRIMARIES_BT470BG, "BT470BG"},
      {AVIF_COLOR_PRIMARIES_BT601, "BT601"},
      {AVIF_COLOR_PRIMARIES_SMPTE240, "SMPTE240"},
      {AVIF_COLOR_PRIMARIES_GENERIC_FILM, "Generic film"},
      {AVIF_COLOR_PRIMARIES_BT2020, "BT2020"},
      {AVIF_COLOR_PRIMARIES_XYZ, "XYZ"},
      {AVIF_COLOR_PRIMARIES_SMPTE431, "SMPTE431"},
      {AVIF_COLOR_PRIMARIES_SMPTE432, "SMPTE432"},
      {AVIF_COLOR_PRIMARIES_EBU3213, "EBU3213"},
  };
  auto it = map.find(colorPrimaries);
  return it == map.end() ? "unknown" : it->second;
}

const char* getTransferCharacteristicsName(
    avifTransferCharacteristics transferCharacteristics) {
  static const std::unordered_map<int, const char*> map{
      {AVIF_TRANSFER_CHARACTERISTICS_UNKNOWN, "Unknown"},
      {AVIF_TRANSFER_CHARACTERISTICS_BT709, "BT709"},
      {AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED, "Unspecified"},
      {AVIF_TRANSFER_CHARACTERISTICS_BT470M, "BT470M"},
      {AVIF_TRANSFER_CHARACTERISTICS_BT470BG, "BT470BG"},
      {AVIF_TRANSFER_CHARACTERISTICS_BT601, "BT601"},
      {AVIF_TRANSFER_CHARACTERISTICS_SMPTE240, "SMPTE240"},
      {AVIF_TRANSFER_CHARACTERISTICS_LINEAR, "Linear"},
      {AVIF_TRANSFER_CHARACTERISTICS_LOG100, "LOG100"},
      {AVIF_TRANSFER_CHARACTERISTICS_LOG100_SQRT10, "LOG100_SQRT10"},
      {AVIF_TRANSFER_CHARACTERISTICS_IEC61966, "IEC61966"},
      {AVIF_TRANSFER_CHARACTERISTICS_BT1361, "BT1361"},
      {AVIF_TRANSFER_CHARACTERISTICS_SRGB, "sRGB"},
      {AVIF_TRANSFER_CHARACTERISTICS_BT2020_10BIT, "BT2020 10-bit"},
      {AVIF_TRANSFER_CHARACTERISTICS_BT2020_12BIT, "BT2020 12-bit"},
      {AVIF_TRANSFER_CHARACTERISTICS_SMPTE2084, "SMPTE2084"},
      {AVIF_TRANSFER_CHARACTERISTICS_SMPTE428, "SMPTE428"},
      {AVIF_TRANSFER_CHARACTERISTICS_HLG, "HLG"},
  };
  auto it = map.find(transferCharacteristics);
  return it == map.end() ? "unknown" : it->second;
}

const char* getMatrixCoefficientsName(
    avifMatrixCoefficients matrixCoefficients) {
  static const std::unordered_map<int, const char*> map{
      {AVIF_MATRIX_COEFFICIENTS_IDENTITY, "Identity"},
      {AVIF_MATRIX_COEFFICIENTS_BT709, "BT709"},
      {AVIF_MATRIX_COEFFICIENTS_UNSPECIFIED, "Unspecified"},
      {AVIF_MATRIX_COEFFICIENTS_FCC, "FCC"},
      {AVIF_MATRIX_COEFFICIENTS_BT470BG, "BT470BG"},
      {AVIF_MATRIX_COEFFICIENTS_BT601, "BT601"},
      {AVIF_MATRIX_COEFFICIENTS_SMPTE240, "SMPTE240"},
      {AVIF_MATRIX_COEFFICIENTS_YCGCO, "YCGCO"},
      {AVIF_MATRIX_COEFFICIENTS_BT2020_NCL, "BT2020 NCL"},
      {AVIF_MATRIX_COEFFICIENTS_BT2020_CL, "BT2020 CL"},
      {AVIF_MATRIX_COEFFICIENTS_SMPTE2085, "SMPTE2085"},
      {AVIF_MATRIX_COEFFICIENTS_CHROMA_DERIVED_NCL, "Chroma-derived NCL"},
      {AVIF_MATRIX_COEFFICIENTS_CHROMA_DERIVED_CL, "Chroma-derived CL"},
      {AVIF_MATRIX_COEFFICIENTS_ICTCP, "BT2100 ICtCp"},
      // {AVIF_MATRIX_COEFFICIENTS_YCGCO_RE, "YCGCO_RE"},
      // {AVIF_MATRIX_COEFFICIENTS_YCGCO_RO, "YCGCO_RO"},
  };
  auto it = map.find(matrixCoefficients);
  return it == map.end() ? "unknown" : it->second;
}

namespace rad {

std::unique_ptr<Image> LibAvifRW::Decode(const uint8_t* data, size_t size) {
  try {
    std::unique_ptr<avifDecoder, decltype(avifDecoderDestroy)*> decoder(
        avifDecoderCreate(), avifDecoderDestroy);
    if (!decoder) {
      throw std::runtime_error("failed to avifDecoderCreate().");
    }
    decoder->maxThreads = std::thread::hardware_concurrency();
    decoder->codecChoice = AVIF_CODEC_CHOICE_AUTO;
    decoder->imageSizeLimit = AVIF_DEFAULT_IMAGE_SIZE_LIMIT;
    decoder->imageDimensionLimit = AVIF_DEFAULT_IMAGE_DIMENSION_LIMIT;
    decoder->strictFlags = AVIF_STRICT_ENABLED;
    decoder->allowProgressive = false;

    avifResult result = avifDecoderSetIOMemory(decoder.get(), data, size);
    if (result != AVIF_RESULT_OK) {
      throw std::runtime_error("failed to avifDecoderSetIOMemory().");
    }

    result = avifDecoderParse(decoder.get());
    if (result != AVIF_RESULT_OK) {
      throw std::runtime_error("failed to avifDecoderParse().");
    }

    result = avifDecoderNthImage(decoder.get(), 0);
    if (result != AVIF_RESULT_OK) {
      throw std::runtime_error("failed to avifDecoderParse().");
    }

    avifRGBImage rgb{};
    avifRGBImageSetDefaults(&rgb, decoder->image);
    rgb.chromaUpsampling = AVIF_CHROMA_UPSAMPLING_AUTOMATIC;
    rgb.depth = decoder->image->depth > 8 ? 16 : 8;
    rgb.format = AVIF_RGB_FORMAT_RGBA;

    PixelFormatType pixel_format = PixelFormatType::rgba8;
    ColorPrimaries color_primaries = ColorPrimaries::BT709;
    TransferCharacteristics transfer_characteristics =
        TransferCharacteristics::sRGB;
    size_t stride = decoder->image->width * 4;
    size_t size = decoder->image->width * 4 * decoder->image->height;
    if (rgb.depth == 16) {
      pixel_format = PixelFormatType::rgba16;
      stride = decoder->image->width * 8;
      size = decoder->image->width * 8 * decoder->image->height;
    }
    if (decoder->image->colorPrimaries == AVIF_COLOR_PRIMARIES_BT2020) {
      color_primaries = ColorPrimaries::BT2020;
    } else if (decoder->image->colorPrimaries == AVIF_COLOR_PRIMARIES_BT601) {
      color_primaries = ColorPrimaries::BT601;
    } else if (decoder->image->colorPrimaries == AVIF_COLOR_PRIMARIES_BT709) {
      color_primaries = ColorPrimaries::BT709;
    } else {
      LOG_F(INFO, "not supported avifColorPrimaries(%d)",
          decoder->image->colorPrimaries);
    }
    if (decoder->image->transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_SMPTE2084) {
      transfer_characteristics = TransferCharacteristics::ST2084;
    } else if (decoder->image->transferCharacteristics ==
               AVIF_TRANSFER_CHARACTERISTICS_HLG) {
      transfer_characteristics = TransferCharacteristics::STDB67;
    } else if (decoder->image->transferCharacteristics ==
               AVIF_TRANSFER_CHARACTERISTICS_SRGB) {
      transfer_characteristics = TransferCharacteristics::sRGB;
    } else if (decoder->image->transferCharacteristics ==
               AVIF_TRANSFER_CHARACTERISTICS_LINEAR) {
      transfer_characteristics = TransferCharacteristics::Linear;
    } else {
      LOG_F(INFO, "not supported avifTransferCharacteristics(%d)",
          decoder->image->transferCharacteristics);
    }

    std::unique_ptr<Image> image(new Image{
        .width = (int)decoder->image->width,
        .height = (int)decoder->image->height,
        .stride = stride,
        .buffer = ImageBuffer::Alloc(size),
        .decoder = DecoderType::libavif,
        .pixel_format = pixel_format,
        .color_primaries = color_primaries,
        .transfer_characteristics = transfer_characteristics,
        .metadata =
            {
                {"depth", std::to_string(decoder->image->depth)},
                {"yuvFormat", avifPixelFormatToString(decoder->image->yuvFormat)},
                {"yuvRange", getRangeName(decoder->image->yuvRange)},
                {"yuvChromaSamplePosition", getChromaSamplePositionName(decoder->image->yuvChromaSamplePosition)},
                {"ownsAlphaPlane", decoder->image->imageOwnsAlphaPlane ? "True" : "False"},
                {"alphaPremultiplied", decoder->image->alphaPremultiplied ? "True" : "False"},
                {"colorPrimaries",
                    std::format("{}({})",
                        getColorPrimariesName(decoder->image->colorPrimaries),
                        decoder->image->colorPrimaries)},
                {"transferCharacteristics",
                    std::format("{}({})",
                        getTransferCharacteristicsName(
                            decoder->image->transferCharacteristics),
                        decoder->image->transferCharacteristics)},
                {"matrixCoefficients",
                    std::format("{}({})",
                        getMatrixCoefficientsName(
                            decoder->image->matrixCoefficients),
                        decoder->image->matrixCoefficients)},
                {"maxCLL", std::to_string(decoder->image->clli.maxCLL)},
                {"maxPALL", std::to_string(decoder->image->clli.maxPALL)},
            },
    });
    rgb.pixels = image->buffer->data;
    rgb.rowBytes = (uint32_t)stride;

    result = avifImageYUVToRGB(decoder->image, &rgb);
    if (result != AVIF_RESULT_OK) {
      throw std::runtime_error("failed to avifImageYUVToRGB().");
    }

    return image;
  } catch (std::exception& ex) {
    LOG_F(WARNING, "exception %s", ex.what());
    return {};
  }
}

}  // namespace rad
