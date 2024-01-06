#include "image_rw_factory.h"

#include <cassert>

#include "libavif_rw.h"
#include "libjpegturbo_rw.h"
#include "pnm_rw.h"
#include "stb_rw.h"
#include "wic_rw.h"
#include "wuffs_rw.h"

namespace rad {

ImageRWFactory::ImageRWFactory() {
  defines_.emplace_back(new ImageRWDefine<WuffsRW>("WuffsRW", {".jpeg", ".jpg", ".png"}));
  defines_.emplace_back(new ImageRWDefine<LibJpegTurboRW>("LibJpegTurbo", {".jpeg", ".jpg"}));
  defines_.emplace_back(new ImageRWDefine<LibAvifRW>("LibAvif", {".avif"}));
  defines_.emplace_back(new ImageRWDefine<PnmRW>("PnmRW", {".pnm", ".pgm", ".ppm"}));
  defines_.emplace_back(new ImageRWDefine<PnmRW>("PnmRW", {".pnm", ".pgm", ".ppm"}));
  defines_.emplace_back(new ImageRWDefine<StbRW>("StbRW", {".jpg", ".jpeg", ".tga", ".png", ".bmp", ".psd", ".gif", ".hdr", ".pic", ".pnm"}));
  defines_.emplace_back(new ImageRWDefine<WicRW>("WicRW", {".jpg", ".jpeg", ".tif", ".tiff", ".gif", ".png", ".bmp", ".jxr", ".ico"}));
}

std::unique_ptr<ImageDecoder> ImageRWFactory::CreatePreferredImageRW(
    const std::string& path) {
  std::filesystem::path fspath((const char8_t*)path.c_str());
  std::string extension = fspath.extension().string();
  if (extension.empty()) {
    extension = path;  // just in case if path is only extension (.png, ...)
  }

  std::transform(extension.begin(), extension.end(), extension.begin(),
      [](unsigned char c) { return std::tolower(c); });

  for (const auto& define : defines_) {
    auto it = std::find(
        define->extensions().begin(), define->extensions().end(), extension);
    if (it != define->extensions().end()) {
      return define->create();
    }
  }
  return nullptr;
}

std::vector<std::string> ImageRWFactory::GetSupportedExtensions() {
  std::vector<std::string> exts;

  for (const auto& define : defines_) {
    for (const auto& ext : define->extensions()) {
      exts.push_back(ext);
    }
  }

  std::sort(exts.begin(), exts.end());
  auto it = std::unique(exts.begin(), exts.end());
  exts.erase(it, exts.end());
  return exts;
}

IImageRWDefine::~IImageRWDefine() {}

}  // namespace rad
