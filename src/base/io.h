#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace rad {

extern const std::string kNativeSeparator;

std::string ConvertToCanonicalPath(const std::string& path, std::error_code& ec) noexcept;

enum class SortType {
  NameAsc,
  NameDesc,
  SizeAsc,
  SizeDesc,
  CreatedAsc,
  CreatedDesc,
  ModifiedAsc,
  ModifiedDesc,
};

class FileEntry {
  friend class DirectoryList;

 public:
  FileEntry();
  FileEntry(const std::string& path);

  enum Flags {
    None = 0, Directory, System, Hidden, 
  };

  bool valid() const { return valid_; }
  const std::string& path() const { return path_; }
  const std::string& name() const { return filename_; }
  size_t size() const { return size_; }
  std::chrono::system_clock::time_point created() const { return created_; }
  std::chrono::system_clock::time_point modified() const { return modified_; }
  int flags() const;

private:
  bool valid_;
  std::string path_;
  std::string filename_;
  size_t size_;
  std::chrono::system_clock::time_point created_;
  std::chrono::system_clock::time_point modified_;
  int flags_;
};

class DirectoryList {
 public:
  DirectoryList();
  DirectoryList(const std::string& path, bool include_subdirectories = false,
      bool include_files = true);
  ~DirectoryList();

  const std::vector<FileEntry>& entries() const { return entries_; }
  size_t count() const { return entries_.size(); }
  const FileEntry& operator[](size_t pos) const { return entries_[pos]; }
  const std::string& path() const { return path_; }

  void sort(SortType type, bool desc);
  SortType sortType() const { return sort_type_; }
  bool sortDesc() const { return sort_desc_; }

 private:
  std::string path_;
  std::vector<FileEntry> entries_;
  SortType sort_type_;
  bool sort_desc_;
};

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

class FileReader {
 public:
  FileReader(const std::string& path, size_t prefetch_size = 1024 * 1024);
  ~FileReader();

  const uint8_t* Read(size_t pos, size_t size);
  size_t GetSize() const { return size_; }

 private:
  size_t pos_;
  size_t size_;
  std::vector<uint8_t> prefetched_;
  std::unique_ptr<FileStream> filestream_;
};

}  // namespace rad

