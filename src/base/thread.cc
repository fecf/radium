#include "thread.h"

#include "minlog.h"
using namespace std::chrono_literals;

#include <shared_mutex>

namespace rad {

Timer::Timer() : start_(std::chrono::high_resolution_clock::now()) {}

Timer::Timer(const std::string& name)
    : start_(std::chrono::high_resolution_clock::now()), name_(name) {}

Timer::~Timer() {
  if (!name_.empty()) {
    DLOG_F("timer [%s] elapsed %.04f ms", name_.c_str(), elapsed() * 1000.0f);
  }
}

double Timer::elapsed() {
  std::chrono::time_point end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start_;
  double value = elapsed.count();
  start_ = std::chrono::high_resolution_clock::now();
  return value;
}

thread_local ThreadPool::TaskId ThreadPool::CurrentTaskId;

ThreadPool::ThreadPool(size_t concurrency)
    : next_task_id_(1), running_count_(0) {
  if (concurrency == 0) {
    concurrency = std::thread::hardware_concurrency();
  }

  workers_.reserve(concurrency);
  for (auto i = 0; i < concurrency; ++i) {
    workers_.emplace_back(&ThreadPool::worker, this);
  }
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard lock(mutex_);
    exit_ = true;
  }
  cv_.notify_all();
  for (auto& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

ThreadPool::TaskId ThreadPool::Post(TaskFunc func) {
  std::lock_guard lock(mutex_);
  TaskId id = next_task_id_++;
  task_map_.emplace(id, std::move(func));
  cv_.notify_all();
  return id;
}

void ThreadPool::worker() {
  TaskId id;
  TaskFunc func;

  while (!exit_ || !task_map_.empty()) {
    {
      std::unique_lock lock(mutex_);
      cv_.wait(lock, [&] { return exit_ || !task_map_.empty(); });
      if (exit_) break;
      if (task_map_.empty()) continue;

      std::tie(id, func) = *task_map_.begin();
      assert(id >= 1);
      assert(func);
      task_map_.erase(task_map_.begin());
    }

    running_count_++;
    try {
      ThreadPool::CurrentTaskId = id;
      func();
      cv_.notify_one();
    } catch (std::exception& ex) {
      DLOG_F("unhandled exception at id=%d reason=%s", id, ex.what());
      assert(false && "unhandled exception.");
    }
    running_count_--;
  }
}

bool ThreadPool::TryCancel(TaskId id) {
  std::lock_guard lock(mutex_);
  auto it = task_map_.find(id);
  if (it == task_map_.end()) {
    return false;
  }
  task_map_.erase(it);
  return true;
}

bool ThreadPool::TryCancelAll() {
  std::lock_guard lock(mutex_);
  if (task_map_.empty()) {
    return false;
  }
  task_map_.clear();
  return true;
}

void ThreadPool::Wait(TaskId id) {
  std::unique_lock lock(mutex_);
  auto it = task_map_.find(id);
  if (it != task_map_.end()) {
    cv_.wait(lock, [&] { return !task_map_.contains(id); });
  }
}

void ThreadPool::WaitAll() {
  std::unique_lock lock(mutex_);
  cv_.wait(lock, [&] { return task_map_.empty(); });
}

}  // namespace rad
