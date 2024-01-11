#pragma once

#include <memory>

#include "image.h"

namespace rad {

class WicRW : public ImageDecoderBase {
 public:
  virtual ~WicRW() {}
  virtual std::unique_ptr<Image> Decode(const uint8_t* data, size_t size) override;
};

}  // namespace rad
