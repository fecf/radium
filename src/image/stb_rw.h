#pragma once

#include <memory>

#include "image.h"

namespace rad {

class StbRW : public ImageDecoderBase {
 public:
  virtual ~StbRW() {}
  virtual std::unique_ptr<Image> Decode(const uint8_t* data, size_t size) override;
};

}  // namespace rad
