#include "image_provider.h"

#include <base/algorithm.h>
#include <engine/engine.h>
#include <image/image.h>

std::shared_ptr<rad::Texture> ThumbnailImageProvider::Request(const std::string& path, int size) {
  auto cache = get(path);
  if (cache.texture) {
    return cache.texture;
  }

  std::unique_ptr<rad::Image> image = rad::Image::Load(path);
  if (!image) return nullptr;

  float scale = rad::scale_to_fit(image->width, image->height, size, size);
  if (scale != 1.0f) {
    image = image->Resize((int)std::ceil(image->width * scale),
        (int)std::ceil(image->height * scale),
        rad::InterpolationType::Bilinear);
    if (!image) return nullptr;
  }

  std::shared_ptr<rad::Texture> texture = engine().CreateTexture(image.get(), false);
  if (!texture) return nullptr;

  put(path, {size, texture});
  return texture;
}

void ThumbnailImageProvider::Clear() { 
  std::lock_guard lock(mutex_);

  map_.clear();
  cache_.clear(); 
}

void ThumbnailImageProvider::put(const key_t& key, const value_t& value) {
  std::lock_guard lock(mutex_);

  if (map_.contains(key)) {
    cache_.erase(map_[key]);
  }
  cache_.push_front({key, value});
  map_[key] = cache_.begin();

  if (cache_.size() > kCapacity) {
    decltype(cache_)::iterator it = --cache_.end();
    map_.erase(it->first);
    cache_.pop_back();
  }
}

ThumbnailImageProvider::value_t ThumbnailImageProvider::get(const key_t& key) {
  std::lock_guard lock(mutex_);

  if (!map_.contains(key)) {
    return {};
  }
  decltype(cache_)::iterator it = map_.at(key);
  const auto [k, v] = *it;
  cache_.erase(it);
  cache_.push_front({k, v});
  map_[key] = cache_.begin();
  return v;
}

ContentImageProvider::Result ContentImageProvider::Request(const std::string& path) {
  std::unique_ptr<rad::Image> image = rad::Image::Load(path);
  if (!image || image->width == 0 || image->height == 0) return {};

  std::unique_ptr<rad::Texture> texture = engine().CreateTexture(image.get(), true);
  if (!texture) return {};

  return {std::move(image), std::move(texture)};
}
