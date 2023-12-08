#pragma once

#include <cassert>
#include <chrono>
#include <functional>
#include <string>
#include <vector>

#define LOG(severity)                                                  \
  minlog::Dispatcher(minlog::Severity::##severity, __FILE__, __LINE__, \
                     __FUNCTION__)
#define DLOG() \
  minlog::Dispatcher(minlog::Severity::DEBUG, __FILE__, __LINE__, __FUNCTION__)
#define LOG_F(severity, format, ...) \
  LOG(severity) << minlog::ssprintf(format, __VA_ARGS__)
#define DLOG_F(format, ...) DLOG() << minlog::ssprintf(format, __VA_ARGS__)

namespace minlog {

extern bool g_timestamp;
extern bool g_elapsed;
extern bool g_thread;
extern bool g_severity;
extern bool g_function;

using Sink = std::function<void(const char*)>;
namespace sink {
Sink cout();
Sink cerr();
Sink debug();
Sink file(const std::string& path);
}  // namespace sink

enum Severity { FATAL = 0, WARNING, INFO, DEBUG, MAX_SEVERITY };
void add_sink(Severity severity, Sink sink);
void add_sink(Sink sink);

template <typename T>
auto converter(const T& v) {
  return v;
}
inline const char* converter(const std::string& v) { return v.c_str(); }

template <typename... Args>
std::string ssprintf(const std::string& format, Args... args) {
  int size_s =
      std::snprintf(nullptr, 0, format.c_str(), converter(args)...) + 1;
  if (size_s <= 0) {
    assert(false && "empty string");
    return {};
  }

  thread_local std::vector<char> buf;
  if (buf.size() < (size_t)size_s) buf.resize(size_s * 2, '\0');

  std::snprintf(buf.data(), size_s, format.c_str(), args...);
  return std::string(buf.data(), buf.data() + size_s - 1);
}

struct Entry {
  std::chrono::system_clock::time_point timestamp;
  Severity severity;
  const char* file;
  int line;
  const char* function;
  std::string message;
};

struct Dispatcher {
  Dispatcher() = delete;
  Dispatcher(Severity severity, const char* file, int line,
             const char* function);
  ~Dispatcher();

  template <typename T>
  Dispatcher& operator<<(const T& v) {
    ss_ << v;
    return *this;
  }

 private:
  std::stringstream ss_;
  Entry entry_;
};

}  // namespace minlog
