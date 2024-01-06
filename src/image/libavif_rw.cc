#include "libavif_rw.h"

#include "base/io.h"
#include "base/minlog.h"

#include <avif/avif.h>

namespace rad {

std::unique_ptr<Image> LibAvifRW::Read(const uint8_t* data, size_t size) {
  avifDecoder* decoder = nullptr;
  try {
    decoder = avifDecoderCreate();
    if (!decoder) {
      throw std::runtime_error("failed to avifDecoderCreate().");
    }
    decoder->maxThreads = std::thread::hardware_concurrency();
    decoder->codecChoice = AVIF_CODEC_CHOICE_AUTO;
    decoder->imageSizeLimit = AVIF_DEFAULT_IMAGE_SIZE_LIMIT;
    decoder->imageDimensionLimit = AVIF_DEFAULT_IMAGE_DIMENSION_LIMIT;
    decoder->strictFlags = AVIF_STRICT_ENABLED;
    decoder->allowProgressive = false;

    avifResult result = avifDecoderSetIOMemory(decoder, data, size);
    if (result != AVIF_RESULT_OK) {
      throw std::runtime_error("failed to avifDecoderSetIOMemory().");
    }

    result = avifDecoderParse(decoder);
    if (result != AVIF_RESULT_OK) {
      throw std::runtime_error("failed to avifDecoderParse().");
    }

    result = avifDecoderNthImage(decoder, 0);
    if (result != AVIF_RESULT_OK) {
      throw std::runtime_error("failed to avifDecoderParse().");
    }

    avifRGBImage rgb{};
    avifRGBImageSetDefaults(&rgb, decoder->image);
    rgb.chromaUpsampling = AVIF_CHROMA_UPSAMPLING_NEAREST;  // TODO:
    rgb.depth = 8;                                          // TODO:
    rgb.format = AVIF_RGB_FORMAT_RGBA;

    // result = avifRGBImageAllocatePixels(&rgb);
    // if (result != AVIF_RESULT_OK) {
    //   throw std::runtime_error("failed to avifRGBImageAllocatePixels().");
    // }
    int width = decoder->image->width;
    int height = decoder->image->height;
    std::unique_ptr<Image> image(new Image(
        width, height, width * 4, ImageFormat::RGBA8, 4, ColorSpace::sRGB));
    rgb.pixels = image->data();
    rgb.rowBytes = width * 4;

    result = avifImageYUVToRGB(decoder->image, &rgb);
    if (result != AVIF_RESULT_OK) {
      throw std::runtime_error("failed to avifImageYUVToRGB().");
    }

    avifDecoderDestroy(decoder);
    return image;
  } catch (std::exception& ex) {
    if (decoder) {
      avifDecoderDestroy(decoder);
    }
    LOG_F(WARNING, "exception %s", ex.what());
    return {};
  }
}

}  // namespace rad
