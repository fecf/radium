#pragma once

#include <memory>

#include "image.h"

namespace rad {

class PnmRW : public ImageDecoder {
 public:
  virtual ~PnmRW() {}
  virtual std::unique_ptr<Image> Read(const uint8_t* data, size_t size) override;
};

}  // namespace rad
