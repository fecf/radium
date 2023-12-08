#include "resource_cache.h"

#include <base/minlog.h>
#include <base/platform.h>
#include <base/thread.h>
#include <image/image.h>
#include <image/types.h>

#pragma warning(push)
#pragma warning(disable : 4267)
#include <qoi/qoixx.hpp>
#pragma warning(pop)

#include "constants.h"

ImageCache::ImageCache()
    : status_(Status::Loading),
      valid_(true),
      stats_(),
      created_sequence_(),
      updated_sequence_() {}

ImageCache::~ImageCache() {}

ImageStore::ImageStore() : item_sequence_(), store_sequence_(), capacity_(0) {
  pool_ = std::unique_ptr<rad::ThreadPool>(new rad::ThreadPool());
}

ImageStore::~ImageStore() {
  pool_->Cancel();
  pool_->Wait();
}

void ImageStore::SetConfig(const ImageStoreConfig& config) { config_ = config; }

void ImageStore::SetCapacity(int capacity) { capacity_ = capacity; }

void ImageStore::SetSequence(uint64_t sequence) { store_sequence_ = sequence; }

void ImageStore::requestTask(std::shared_ptr<ImageCache> sp, ImageStore::Callback cb) {
  try {
    rad::Timer timer;

    std::shared_ptr<rad::Image> image = loadImage(sp->key_);
    if (!image || image->width() == 0 || image->height() == 0) {
      throw std::domain_error("failed to load image.");
    }
    sp->stats_.load_time = timer.elapsed();

    if (sp->config_.width > 0 || sp->config_.height > 0) {
      int w = sp->config_.width;
      int h = sp->config_.height;
      double aspect_ratio = (double)image->width() / image->height();
      if (w <= 0) {
        w = (int)(h * aspect_ratio);
      } else if (h <= 0) {
        h = (int)(w / aspect_ratio);
      }
      if (w != image->width() && h != image->height()) {
        image = image->Resize(w, h, rad::ResizeFilter::Bilinear);
        if (!image) {
          throw std::domain_error("failed to resize image.");
        }
      }
      sp->stats_.resize_time = timer.elapsed();
    }

    std::shared_ptr<rad::Texture> texture =
        engine().CreateTexture(image, sp->config_.tiled_texture);
    if (!texture) {
      throw std::domain_error("failed to create texture.");
    }
    sp->stats_.upload_time = timer.elapsed();

    saveImage(sp->key(), image);

    {
      std::lock_guard lock(mutex_);
      sp->stats_.save_time = timer.elapsed();
      sp->texture_ = texture;
      sp->status_ = ImageCache::Status::Completed;
    }

    if (cb) {
      cb(sp);
    }
  } catch (std::exception& ex) {
    {
      std::lock_guard lock(mutex_);
      sp->status_ = ImageCache::Status::Failed;
    }
    DLOG_F(
        "failed to load image. key=%s reason=%s", sp->key().c_str(), ex.what());
  }
}

std::shared_ptr<ImageCache> ImageStore::Request(
    const std::string& key, ImageStore::Callback cb) {

  std::shared_ptr<ImageCache> sp;
  std::function<void()> request;

  {
    std::lock_guard lock(mutex_);
    auto it = map_.find(key);
    if (it == map_.end()) {
      sp = std::shared_ptr<ImageCache>(new ImageCache());
      sp->key_ = key;
      sp->config_ = config_;
      sp->status_ = ImageCache::Status::Loading;
      sp->stats_ = {};
      sp->valid_ = true;
      sp->created_sequence_ = item_sequence_; 
      sp->updated_sequence_ = item_sequence_; 
      map_[key] = {store_sequence_, sp};
      ++item_sequence_;
      request = std::bind(&ImageStore::requestTask, this, sp, cb);
    } else {
      sp = it->second.second;
      assert(sp);

      it->second.first = store_sequence_;
      sp->updated_sequence_ = item_sequence_;
      ++item_sequence_;
      if (!sp->valid_) {
        sp->config_ = config_;
        sp->status_ = ImageCache::Status::Loading;
        sp->stats_ = {};
        sp->valid_ = true;
        request = std::bind(&ImageStore::requestTask, this, sp, cb);
      }
    }
  }

  assert(sp);
  if (request) {
    pool_->Post(std::move(request), sp->created_sequence_);
  }

  assert(sp);
  return sp;
}

void ImageStore::Evict() {
  std::lock_guard lock(mutex_);

  // evict by store sequence
  std::erase_if(map_, [=](const auto& el) {
    if ((el.second.first > 0) && (el.second.first < store_sequence_)) {
      pool_->Cancel(el.second.second->created_sequence_);
      return true;
    } else {
      return false;
    }
  });

  // evict by item sequence
  while ((capacity_ > 0) && (map_.size() > capacity_)) {
    auto it = std::min_element(
        map_.begin(), map_.end(), [](const auto& a, const auto& b) {
          return a.second.second->updated_sequence_ <
                 b.second.second->updated_sequence_;
        });
    if (it != map_.end()) {
      map_.erase(it);
    } else {
      break;
    }
  }
}

void ImageStore::Cancel() {
  std::lock_guard lock(mutex_);
  std::erase_if(map_, [=](const auto& el) {
    if (el.second.second->status() == ImageCache::Status::Loading) {
      pool_->Cancel(el.second.second->created_sequence_);
      return true;
    }
    return false;
  });
}

void ImageStore::Wait() { 
  pool_->Wait(); 
}

void ImageStore::Delete(const std::string& key) {
  std::lock_guard lock(mutex_);
  std::erase_if(map_, [=](const auto& el) { return el.first == key; });
}

void ImageStore::DeleteAll() { 
  std::lock_guard lock(mutex_);
  map_.clear(); 
}

void ImageStore::Invalidate(const std::string& key) {
  std::lock_guard lock(mutex_);
  auto it = map_.find(key);
  if (it != map_.end()) {
    it->second.second->valid_ = false;
  }
}

void ImageStore::Enumerate(std::function<void(const std::string&)> cb) {
  std::lock_guard lock(mutex_);
  for (const auto& kv : map_) {
    cb(kv.first);
  }
}

std::shared_ptr<rad::Image> ImageStore::loadImage(const std::string& key) {
  return rad::Image::Load(key);
}

void ImageStore::saveImage(const std::string& key, std::shared_ptr<rad::Image> image) {
  return;
}

