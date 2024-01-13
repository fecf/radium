#include "io.h"

#include <filesystem>

#include "base/text.h"
#include "base/minlog.h"
#include "base/platform.h"
#include "base/algorithm.h"

inline std::chrono::system_clock::time_point filetime_to_system_clock(
    LPFILETIME ft) {
  ULARGE_INTEGER ul;
  ul.HighPart = ft->dwHighDateTime;
  ul.LowPart = ft->dwLowDateTime;
  time_t secs = ul.QuadPart / 10000000ULL - 11644473600ULL;
  auto tp = std::chrono::system_clock::from_time_t(secs);
  return tp;
};

namespace rad {

FileStream::FileStream(const std::string& path) : handle_(), path_() {
  DWORD desired_access = GENERIC_READ;
  DWORD share_mode = FILE_SHARE_READ;
  DWORD flags = FILE_ATTRIBUTE_NORMAL;
  handle_ = ::CreateFileW(to_wstring(path).c_str(), desired_access,
      share_mode, NULL, OPEN_EXISTING, flags, NULL);
  path_ = std::filesystem::path((const char8_t*)path.c_str());
  if (handle_ == INVALID_HANDLE_VALUE) {
    handle_ = NULL;
    valid_ = false;
    return;
  }
  valid_ = true;
}

FileStream::~FileStream() {
  if ((handle_ != NULL) && (handle_ != INVALID_HANDLE_VALUE)) {
    ::CloseHandle(handle_);
  }
}

size_t FileStream::Read(uint8_t* dst, size_t size) {
  if (!valid_) return 0;

  DWORD read_bytes{};
  BOOL ret = ::ReadFile(handle_, dst, (DWORD)size, &read_bytes, NULL);
  if (ret == FALSE) {
    return 0;
  }

  return read_bytes;
}

size_t FileStream::Seek(size_t pos) {
  if (!valid_) return 0;

  LARGE_INTEGER dist{}, after{};
  dist.QuadPart = pos;
  BOOL ret = ::SetFilePointerEx(handle_, dist, &after, FILE_BEGIN);
  if (ret == FALSE) {
    return 0;
  }
  return after.QuadPart;
}

size_t FileStream::Size() {
  if (!valid_) return 0;

  LARGE_INTEGER size{};
  BOOL ret = ::GetFileSizeEx(handle_, &size);
  if (ret == FALSE) {
    return 0;
  }
  return size.QuadPart;
}

std::string ConvertToCanonicalPath(const std::string& path, std::error_code& ec) noexcept {
  std::string fullpath = GetFullPath(path);
  if (fullpath.empty()) {
    return {};
  }

  std::filesystem::path fspath =
      std::filesystem::weakly_canonical(rad::to_wstring(fullpath), ec);
  if (ec) {
    return {};
  }
  if (!fspath.is_absolute()) {
    fspath = std::filesystem::absolute(fspath, ec);
    if (ec) {
      return {};
    }
  }
  return rad::to_string(fspath.wstring());
}

std::string GetFullPath(const std::string& path) noexcept {
  /*
  HANDLE handle = ::CreateFileW(to_wstring(path).c_str(), GENERIC_READ,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
      OPEN_EXISTING, FILE_ATTRIBUTE_READONLY | FILE_FLAG_NO_BUFFERING, NULL);
  if (handle == INVALID_HANDLE_VALUE) {
    return {};
  }

  std::vector<wchar_t> buf(32768);
  DWORD size = ::GetFinalPathNameByHandleW(handle, buf.data(), (DWORD)buf.size(), FILE_NAME_NORMALIZED);
  ::CloseHandle(handle);

  if (size >= 4) {
    if (buf[0] == '\\' && buf[1] == '\\' && buf[2] == '?' && buf[3] == '\\') {
      return to_string(buf.data() + 4);
    } else {
      return to_string(buf.data());
    }
  }
  return "";
  */

  std::vector<wchar_t> buf(32768);
  DWORD size = ::GetFullPathNameW(to_wstring(path).c_str(), buf.size(), buf.data(), NULL);
  return to_string(buf.data());
}

MemoryMappedFile::MemoryMappedFile(const std::string& path) {
  wil::unique_handle handle_file(::CreateFileW(to_wstring(path).c_str(), GENERIC_READ,
      FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL, 0));

  LARGE_INTEGER size;
  BOOL ret = ::GetFileSizeEx(handle_file.get(), &size);
  size_ = ret ? size.QuadPart : 0;

  DWORD high = size_ >> 32;
  DWORD low = size_ & 0xffffffff;
  wil::unique_handle handle_file_mapping(
      ::CreateFileMappingW(handle_file.get(), 0, PAGE_READONLY, high, low, 0));

  SYSTEM_INFO system_info{};
  ::GetSystemInfo(&system_info);
  size_t page_size = system_info.dwAllocationGranularity;

  wil::unique_mapview_ptr<void> data(
      ::MapViewOfFile(handle_file_mapping.get(), FILE_MAP_READ, 0, 0, size_));
  if (data != NULL) {
    handle_file_ = std::move(handle_file);
    handle_file_mapping_ = std::move(handle_file_mapping);
    data_ = std::move(data);
  }
}

MemoryMappedFile::~MemoryMappedFile() {}

void* MemoryMappedFile::data() const { return data_.get(); }

size_t MemoryMappedFile::size() const { return size_; }

}  // namespace rad
