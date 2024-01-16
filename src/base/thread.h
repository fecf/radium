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
  using TaskId = int64_t;
  using TaskFunc = std::function<void()>;

  ThreadPool(size_t concurrency);
  ~ThreadPool();

  TaskId Post(TaskFunc func);
  bool TryCancel(TaskId id);
  bool TryCancelAll();
  void Wait(TaskId id);
  void WaitAll();

  size_t running_count() const { return running_count_; }
  size_t remaining_count() const { return task_map_.size(); }

  static thread_local TaskId CurrentTaskId;

 private:
  void worker();

 private:
  std::vector<std::thread> workers_;
  std::atomic<bool> exit_;

  std::mutex mutex_;
  std::condition_variable cv_;
  std::map<TaskId, TaskFunc> task_map_;
  std::atomic<TaskId> next_task_id_;

  std::atomic<size_t> running_count_;
};

}  // namespace rad
