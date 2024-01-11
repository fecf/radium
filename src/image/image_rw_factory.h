#pragma once

#include <memory>
#include <string>

namespace rad {

class ImageDecoderBase;

class ImageRWFactory {
 public:
  ImageRWFactory();
  ImageRWFactory(const ImageRWFactory&) = delete;
  ImageRWFactory& operator=(const ImageRWFactory&) = delete;

  std::unique_ptr<ImageDecoderBase> Create(const std::string& path);
};

}  // namespace rad
