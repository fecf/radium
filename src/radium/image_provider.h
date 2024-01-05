#pragma once

#include <functional>
#include <string>
#include <memory>
#include <unordered_map>
#include <optional>

#include <engine/engine.h>

class ImageProvider {
 public:
  std::shared_ptr<rad::Texture> Request(
      const std::string& path, std::optional<int> max_size = {});
};

class CachedImageProvider : public ImageProvider {
 public:
  std::shared_ptr<rad::Texture> Request(
      const std::string& path, std::optional<int> max_size = {});
  void Clear();

 private:
  std::mutex mutex_;
  using key_t = std::string;
  using value_t = std::shared_ptr<rad::Texture>;
  std::list<std::pair<key_t, value_t>> cache_;
  std::unordered_map<std::string, decltype(cache_)::iterator> map_;
  static const int kCapacity = 256;

  void put(const key_t& key, const value_t& value);
  value_t get(const key_t& key);
};

class TiledImageProvider {
 public:
  std::shared_ptr<rad::Texture> Request(const std::string& path);
};
