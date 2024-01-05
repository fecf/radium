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


std::shared_ptr<rad::Texture> CachedImageProvider::Request(
  const std::string& path, std::optional<int> max_size) {
  auto texture = get(path);
  if (texture) {
    return texture;
  }

  texture = ImageProvider::Request(path, max_size);
  if (texture) {
    put(path, texture);
  }
  return texture;
}

void CachedImageProvider::Clear() { 
  std::lock_guard lock(mutex_);

  map_.clear();
  cache_.clear(); 
}

void CachedImageProvider::put(const key_t& key, const value_t& value) {
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

CachedImageProvider::value_t CachedImageProvider::get(const key_t& key) {
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

std::shared_ptr<rad::Texture> TiledImageProvider::Request(const std::string& path) {
  std::shared_ptr<rad::Image> image = rad::Image::Load(path);
  if (!image) return nullptr;

  std::shared_ptr<rad::Texture> texture = engine().CreateTexture(image, true);
  if (!texture) return nullptr;

  return texture;
}
