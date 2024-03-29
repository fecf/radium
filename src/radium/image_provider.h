#pragma once

#include <functional>
#include <string>
#include <memory>
#include <unordered_map>
#include <optional>

#include <engine/engine.h>

class ThumbnailImageProvider {
 public:
  std::shared_ptr<rad::Texture> Request(const std::string& path, int size);
  void Clear();

 private:
  struct Cache {
    int requested_size;
    std::shared_ptr<rad::Texture> texture;
  };

  std::mutex mutex_;
  using key_t = std::string;
  using value_t = Cache;
  std::list<std::pair<key_t, value_t>> cache_;
  std::unordered_map<std::string, decltype(cache_)::iterator> map_;
  static const int kCapacity = 4096;

  void put(const key_t& key, const value_t& value);
  value_t get(const key_t& key);
};

class ContentImageProvider {
 public:
  struct Result {
    std::unique_ptr<rad::Image> image;
    std::unique_ptr<rad::Texture> texture;
  };
  Result Request(const std::string& path);
};
