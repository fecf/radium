#pragma once

#include <numeric>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

#include <base/io.h>
#include <base/thread.h>
#include <engine/engine.h>
#include <image/image.h>

class ImageStoreConfig {
 public:
  int width = 0;
  int height = 0;
  bool tiled_texture = false;
};

class ImageCache {
  friend class ImageStore;

 public:
  ImageCache();
  virtual ~ImageCache();

  const std::string& key() const { return key_; }

  enum class Status { Loading, Completed, Failed };
  const Status status() const { return status_; }

  const std::shared_ptr<rad::Texture> texture() const { return texture_; }

  struct Stats {
    double load_time;
    double save_time;
    double resize_time;
    double upload_time;
  };
  const Stats& stats() const { return stats_; }

 private:
  std::string key_;
  ImageStoreConfig config_;
  std::atomic<Status> status_;
  std::shared_ptr<rad::Texture> texture_;
  Stats stats_;
  bool valid_;

  uint64_t created_sequence_;
  uint64_t updated_sequence_;
};

class ImageStore {
 public:
  ImageStore();
  ~ImageStore();

  using Callback = std::function<void(std::shared_ptr<ImageCache>)>;

  size_t count() const { return map_.size(); }
  rad::ThreadPool* thread_pool() const { return pool_.get(); }

  void SetConfig(const ImageStoreConfig& config);
  void SetCapacity(int capacity);
  void SetSequence(uint64_t sequence);
  std::shared_ptr<ImageCache> Request(const std::string& key, Callback cb = {});
  void Evict();
  void Cancel();
  void Wait();
  void Delete(const std::string& key);
  void DeleteAll();
  void Enumerate(std::function<void(const std::string&)> cb);
  void Invalidate(const std::string& key);

protected:
  virtual std::shared_ptr<rad::Image> loadImage(const std::string& key);
  virtual void saveImage(const std::string& key, std::shared_ptr<rad::Image> image);

private:
  void requestTask(std::shared_ptr<ImageCache> sp, Callback cb);

  std::mutex mutex_;
  std::unique_ptr<rad::ThreadPool> pool_;
  ImageStoreConfig config_;

  uint64_t item_sequence_;
  std::atomic<uint64_t> store_sequence_;
  std::map<std::string, std::pair<uint64_t, std::shared_ptr<ImageCache>>> map_;
  int capacity_;
};

