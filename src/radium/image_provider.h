#pragma once

#include <functional>
#include <string>

#include <engine/engine.h>

class ImageProvider {
 public:
  std::shared_ptr<rad::Texture> Request(const std::string& path);
};