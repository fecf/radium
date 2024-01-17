#pragma once

#include <memory>

#include "image.h"

namespace rad {

class LibTiffRW : public ImageDecoderBase {
 public:
  virtual ~LibTiffRW() {}
  virtual std::unique_ptr<Image> Decode(const std::string& path) override;
};

}  // namespace rad