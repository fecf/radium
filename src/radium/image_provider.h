#pragma once

#include <functional>
#include <string>
#include <optional>

#include <engine/engine.h>

class ImageProvider {
 public:
  std::shared_ptr<rad::Texture> Request(
      const std::string& path, std::optional<int> max_size = {});
};

class TiledImageProvider {
 public:
  std::shared_ptr<rad::Texture> Request(const std::string& path);
};
