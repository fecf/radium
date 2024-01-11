#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include <wil/result.h>

namespace rad {

extern const std::string kNativeSeparator;

std::string ConvertToCanonicalPath(const std::string& path, std::error_code& ec) noexcept;
std::string GetFullPath(const std::string& path) noexcept;

class FileStream {
 public:
  FileStream(const std::string& path);
  ~FileStream();

  FileStream(const FileStream&) = delete;
  FileStream& operator=(const FileStream&) = delete;

  size_t Read(uint8_t* dst, size_t size);
  size_t Seek(size_t pos);
  size_t Size();

  bool valid() const { return valid_; }
  std::filesystem::path path() const { return path_; }

 private:
  bool valid_;
  void* handle_;
  std::filesystem::path path_;
};

class MemoryMappedFile {
 public:
  MemoryMappedFile(const std::string& path);
  ~MemoryMappedFile();

  void* data() const;
  size_t size() const;

 private:
  wil::unique_handle handle_file_;
  wil::unique_handle handle_file_mapping_;
  wil::unique_mapview_ptr<void> data_;
  size_t size_;
};

}  // namespace rad

