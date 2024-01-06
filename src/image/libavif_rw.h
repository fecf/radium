#pragma once

#include <memory>

#include "image.h"

namespace rad {

class LibAvifRW : public ImageDecoder {
 public:
  virtual ~LibAvifRW() {}
  virtual std::unique_ptr<Image> Read(const uint8_t* data, size_t size) override;
};

}  // namespace rad