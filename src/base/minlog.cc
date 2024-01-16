#include "minlog.h"

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>

#include <windows.h>

#include "text.h"

namespace minlog {

bool g_timestamp = false;
bool g_elapsed = true;
bool g_thread = false;
bool g_severity = true;
bool g_function = false;

namespace impl {

constexpr const char* severity_to_string[(int)Severity::MAX_SEVERITY]{
    "FATAL", "WARNING", "INFO", "DEBUG"};

class Logger {
 public:
  static Logger& GetInstance() {
    static Logger logger;
    return logger;
  }

  Logger()
      : start_(std::chrono::high_resolution_clock::now()),
        thread_(std::thread(&Logger::thread, this)) {}
  ~Logger() {
    if (thread_.joinable()) {
      exit_ = true;
      cv_.notify_one();
      thread_.join();
    }
  }
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  void addSink(Severity severity, Sink sink) {
    sinks_[(int)severity].push_back(sink);
  }
  void dispatch(const Entry& entry) {
    {
      std::unique_lock lock(mutex_);
      queue_.push(entry);
    }
    cv_.notify_one();
  }

 private:
  std::string system_clock_to_string(
      const std::chrono::system_clock::time_point& t) const {
    time_t tt = std::chrono::system_clock::to_time_t(t);
    tm local_tm{};
    errno_t err = localtime_s(&local_tm, &tt);
    if (err != 0) {
      throw std::runtime_error("failed localtime_s().");
    }

    constexpr const char* format = "%04d-%02d-%02dT%02d:%02d:%02d";
    return ssprintf(format, local_tm.tm_year + 1900, local_tm.tm_mon + 1,
        local_tm.tm_mday, local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec);
  }
  std::string generate(const Entry& entry) {
    const std::string timestamp = system_clock_to_string(entry.timestamp);
    const char* severity = severity_to_string[(int)entry.severity];

    uint32_t tid = 0;
    std::stringstream ss;
    ss << std::this_thread::get_id();
    tid = std::stoul(ss.str());

    std::chrono::high_resolution_clock::time_point now =
        std::chrono::high_resolution_clock::now();
    double elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(now - start_)
            .count() /
        1000000.0;

    std::string msg;
    if (g_timestamp) {
      msg += timestamp + " ";
    }
    if (g_elapsed) {
      msg += ssprintf("[%.04f] ", elapsed);
    }
    if (g_thread) {
      msg += ssprintf("[%u] ", tid);
    }
    if (g_severity) {
      msg += ssprintf("[%s] ", severity);
    }
    if (g_function) {
      msg += ssprintf("%s() ", entry.function);
    }
    msg += entry.message;
    return msg;
  }

  void thread() {
    Entry entry;
    while (!exit_ || queue_.size()) {
      {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [&] { return exit_ || queue_.size(); });
        if (exit_) break;

        entry = std::move(queue_.front());
        queue_.pop();
      }

      const std::string msg = generate(entry);
      for (Sink& writer : sinks_[entry.severity]) {
        writer(msg.c_str());
      }
    }
  }

 private:
  std::chrono::high_resolution_clock::time_point start_;

  std::mutex mutex_;
  std::atomic<bool> exit_;
  std::condition_variable cv_;

  std::vector<Sink> sinks_[Severity::MAX_SEVERITY];
  std::thread thread_;
  std::queue<Entry> queue_;
};

}  // namespace impl

void add_sink(Severity severity, Sink sink) {
  impl::Logger::GetInstance().addSink(severity, sink);
}

void add_sink(Sink sink) {
  for (int i = 0; i < Severity::MAX_SEVERITY; ++i) {
    impl::Logger::GetInstance().addSink((Severity)i, sink);
  }
}

Dispatcher::Dispatcher(
    Severity severity, const char* file, int line, const char* function)
    : entry_{std::chrono::system_clock::now(), severity, file, line, function} {
}

Dispatcher::~Dispatcher() {
  entry_.message = ss_.str();
  impl::Logger::GetInstance().dispatch(entry_);
}

namespace sink {

Sink cout() {
  return [](const char* msg) { std::cout << msg << std::endl; };
}

Sink cerr() {
  return [](const char* msg) { std::cerr << msg << std::endl; };
}

Sink debug() {
  return [](const char* msg) {
    std::wstring utf16 = rad::to_wstring(msg) + L"\n";
    ::OutputDebugStringW(utf16.c_str());
  };
}

Sink file(const std::string& path) {
  return [=](const char* msg) {
    std::ofstream ofs(path, std::ios::app);
    ofs << msg << std::endl;
    ofs.flush();
    ofs.close();
  };
}

}  // namespace sink

}  // namespace minlog
