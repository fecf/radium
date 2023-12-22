#pragma once

#include <memory>

#include "image.h"

namespace rad {

class nvJPEGRW : public ImageDecoder {
 public:
  virtual ~nvJPEGRW() {}
  virtual std::unique_ptr<Image> Read(
      const uint8_t* data, size_t size) override;
};

}  // namespace rad
