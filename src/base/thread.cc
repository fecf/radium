#include "thread.h"

#include "minlog.h"
using namespace std::chrono_literals;

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

ThreadPool::ThreadPool() : ThreadPool(std::thread::hardware_concurrency()) {}

ThreadPool::ThreadPool(int concurrency) : running_() {
  if (concurrency <= 0) {
    concurrency = std::thread::hardware_concurrency();
  }
  futures_.resize(concurrency);
}

ThreadPool::~ThreadPool() { Wait(); }

void ThreadPool::Post(Func func, uint64_t tag) {
  std::lock_guard lock(mutex_);
  tasks_.emplace_back(Task{std::move(func), tag});

  for (std::future<void>& future : futures_) {
    if (future.valid() &&
        (future.wait_for(0s) == std::future_status::timeout)) {
      continue;
    }

    future = std::async(std::launch::async, [this] {
      while (!tasks_.empty()) {
        mutex_.lock();
        if (tasks_.empty()) {
          mutex_.unlock();
          continue;
        }
        Task task = std::move(tasks_.front());
        tasks_.erase(tasks_.begin());
        mutex_.unlock();

        running_++;
        try {
          if (!task.func) {
            throw std::domain_error("task.func is empty.");
          }
          task.func();
        } catch (std::exception& ex) {
          DLOG_F("unhandled exception (%s).", ex.what());
          assert(false && "unhandled exception.");
        }
        running_--;
        cv_.notify_all();
      }
    });
  }
}

void ThreadPool::Cancel() {
  std::lock_guard lock(mutex_);
  tasks_.clear();
  cv_.notify_all();
}

void ThreadPool::Cancel(uint64_t tag) {
  std::lock_guard lock(mutex_);
  auto it = std::remove_if(tasks_.begin(), tasks_.end(),
      [tag](const auto& task) { return task.tag == tag; });
  if (it != tasks_.end()) {
    tasks_.erase(it, tasks_.end());
    cv_.notify_all();
  }
}

void ThreadPool::Wait() {
  {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [&] { return tasks_.empty() && running_ == 0; });
  }
}

}  // namespace rad
