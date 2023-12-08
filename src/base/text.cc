#include "text.h"

#include <algorithm>
#include <string>
#include <winrt/base.h>

namespace rad {

std::string readable_byte_count(size_t bytes, bool si) {
  int unit = si ? 1000 : 1024;
  if (bytes < unit) return std::to_string(bytes) + " B";
  unsigned int exp = (int)(std::log(bytes) / std::log(unit));
  std::string units = si ? "kMGTPE" : "KMGTPE";
  std::string pre = units[exp - 1] + (si ? "" : "i");

  char buf[1024];
  sprintf_s(buf, "%.1f %sB", bytes / std::pow(unit, exp), pre.c_str());
  return buf;
}

std::wstring to_wstring(const std::string& str) {
  return static_cast<std::wstring>(winrt::to_hstring(str));
}

std::wstring to_wstring(const std::u8string& str) {
  return to_wstring(std::string(str.begin(), str.end()));
}

std::string to_string(const std::wstring& str) {
  return winrt::to_string(str);
}

std::string to_string(const std::u8string& str) {
  return std::string(str.begin(), str.end());
}

}  // namespace rad
