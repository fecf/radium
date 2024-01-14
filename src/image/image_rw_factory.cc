#include "image_rw_factory.h"

#include <cassert>

#include "libavif_rw.h"
#include "libjpegturbo_rw.h"
#include "lodepng_rw.h"
#include "pnm_rw.h"
#include "stb_rw.h"
#include "wic_rw.h"
#include "wuffs_rw.h"

namespace rad {

ImageRWFactory::ImageRWFactory() {}

std::unique_ptr<ImageDecoderBase> ImageRWFactory::Create(const std::string& path) {
  std::filesystem::path fspath((const char8_t*)path.c_str());
  std::string extension = fspath.extension().string();
  if (extension.empty()) {
    extension = path;  // just in case if path is only extension (.png, ...)
  }

  std::transform(extension.begin(), extension.end(), extension.begin(),
      [](unsigned char c) { return std::tolower(c); });

  if (extension == ".png") {
    return std::make_unique<LodePngRW>();
  }
  if (extension == ".bmp" || extension == ".gif" || extension == ".jpeg" ||
      extension == ".jpg" || extension == ".tga") {
    return std::make_unique<WuffsRW>();
  }
  if (extension == ".avif") {
    return std::make_unique<LibAvifRW>();
  }
  if (extension == ".psd" || extension == ".hdr" || extension == ".pic") {
    return std::make_unique<StbRW>();
  }
  // if (extension == ".tif" || extension == ".tiff" || extension == ".ico" ||
  //     extension == ".jxr") {
  //   return std::make_unique<WicRW>();
  // }

  return nullptr;
}

}  // namespace rad
