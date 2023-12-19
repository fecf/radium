#include "image_provider.h"

#include <base/algorithm.h>
#include <engine/engine.h>
#include <image/image.h>

std::shared_ptr<rad::Texture> ImageProvider::Request(
    const std::string& path, std::optional<int> max_size) {
  std::shared_ptr<rad::Image> image = rad::Image::Load(path);
  if (!image) return nullptr;

  if (max_size) {
    float scale = rad::scale_to_fit(
        image->width(), image->height(), *max_size, *max_size);
    if (scale != 1.0f) {
      image = image->Resize((int)std::ceil(image->width() * scale),
        (int)std::ceil(image->height() * scale), rad::ResizeFilter::Bilinear);
      if (!image) return nullptr;
    }
  }

  std::shared_ptr<rad::Texture> texture = engine().CreateTexture(image, false);
  if (!texture) return nullptr;

  return texture;
}

std::shared_ptr<rad::Texture> TiledImageProvider::Request(const std::string& path) {
  std::shared_ptr<rad::Image> image = rad::Image::Load(path);
  if (!image) return nullptr;

  std::shared_ptr<rad::Texture> texture = engine().CreateTexture(image, true);
  if (!texture) return nullptr;

  return texture;
}
