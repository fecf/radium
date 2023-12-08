#include "image_provider.h"

#include <engine/engine.h>
#include <image/image.h>

std::shared_ptr<rad::Texture> ImageProvider::Request(const std::string& path) {
  std::shared_ptr<rad::Image> image = rad::Image::Load(path);
  if (!image) return nullptr;

  std::shared_ptr<rad::Texture> texture = engine().CreateTexture(image, true);
  if (!texture) return nullptr;

  return texture;
}
