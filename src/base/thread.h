#pragma once

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <vector>

namespace rad {

class ScopeExit {
 public:
  ScopeExit() = delete;
  ScopeExit(std::function<void()> callback) : callback_(callback) {
    assert(callback_);
  }
  ~ScopeExit() { callback_(); }
  ScopeExit(const ScopeExit&) = delete;
  ScopeExit& operator=(const ScopeExit&) = delete;
 private:
  std::function<void()> callback_;
};

struct Timer {
  Timer();
  Timer(const std::string& name);
  ~Timer();
  double elapsed();

 private:
  std::string name_;
  std::chrono::high_resolution_clock::time_point start_;
};

class ThreadPool {
 public:
  using Func = std::function<void()>;
  struct Task {
    Func func;
    uint64_t tag;
  };

  ThreadPool();
  ThreadPool(int concurrency);
  ~ThreadPool();

  void Post(Func func, uint64_t tag = 0);
  void Cancel();
  void Cancel(uint64_t tag);
  void Wait();

  int runnings() const { return running_; }
  int remainings() const { return (int)tasks_.size(); }

 private:
  std::mutex mutex_;
  std::vector<Task> tasks_;
  std::vector<std::future<void>> futures_;
  std::atomic<int> running_;
  std::condition_variable cv_;
};

}  // namespace rad
