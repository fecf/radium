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

namespace comparer {

bool name(const FileEntry& lhs, const FileEntry& rhs, bool desc) {
  return desc ? compare(to_wstring(rhs.name()), to_wstring(lhs.name()))
              : compare(to_wstring(lhs.name()), to_wstring(rhs.name()));
}

bool size(const FileEntry& lhs, const FileEntry& rhs, bool desc) {
  return desc ? (lhs.size() > rhs.size()) : (lhs.size() < rhs.size());
}

bool created(const FileEntry& lhs, const FileEntry& rhs, bool desc) {
  return desc ? (lhs.created() > rhs.created())
              : (lhs.created() < rhs.created());
}

bool modified(const FileEntry& lhs, const FileEntry& rhs, bool desc) {
  return desc ? (lhs.modified() > rhs.modified())
              : (lhs.modified() < rhs.modified());
}

}  // namespace comparer

const std::string kNativeSeparator =
    to_string(std::wstring(1, std::filesystem::path::preferred_separator));

FileEntry::FileEntry() : valid_(false), size_(), flags_() {}

FileEntry::FileEntry(const std::string& path)
    : valid_(false),
      path_(),
      filename_(),
      size_(),
      created_(),
      modified_(),
      flags_() {
  std::error_code ec;
  std::filesystem::path fspath(to_wstring(path));

  // resolve dot-dot elements
  fspath = std::filesystem::weakly_canonical(fspath, ec);
  if (ec) {
    DLOG_F("failed std::filesystem::canonical(%s).", path.c_str());
    return;
  }

  // convert to absolute
  if (!fspath.is_absolute()) {
    fspath = std::filesystem::absolute(fspath, ec);
    if (ec) {
      DLOG_F("failed std::filesystem::absolute(%s).", path.c_str());
      return;
    }
  }

  // convert all separators
  fspath = fspath.make_preferred();
  path_ = to_string(fspath.u8string());
  filename_ = to_string(fspath.filename().u8string());

  WIN32_FIND_DATA data{};
  HANDLE handle = ::FindFirstFileW(to_wstring(path_).c_str(), &data);
  if (handle == INVALID_HANDLE_VALUE) {
    assert(false && "invalid handle value.");
    valid_ = false;
    return;
  }

  valid_ = true;
  flags_ = None;
  size_ = 0;
  if (::wcscmp(data.cFileName, L".") == 0 ||
      ::wcscmp(data.cFileName, L"..") == 0) {
    return;
  } else if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    flags_ |= Directory;
  } else {
    if (data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) {
      flags_ |= System;
    } else if (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
      flags_ |= Hidden;
    } else {
      ULARGE_INTEGER ul;
      ul.HighPart = data.nFileSizeHigh;
      ul.LowPart = data.nFileSizeLow;
      size_ = ul.QuadPart;
    }
  }
  created_ = filetime_to_system_clock(&data.ftCreationTime);
  modified_ = filetime_to_system_clock(&data.ftLastWriteTime);
}

int FileEntry::flags() const { return flags_; }

DirectoryList::DirectoryList() : sort_type_(SortType::NameAsc) {}

DirectoryList::DirectoryList(
    const std::string& path, bool include_subdirectories, bool include_files)
    : sort_type_(SortType::NameAsc) {
  std::filesystem::path fspath(to_wstring(path));
  std::error_code ec;
  fspath = std::filesystem::weakly_canonical(fspath, ec);
  if (ec) {
    DLOG_F("failed std::filesystem::canonical(%s).", path.c_str());
    return;
  }
  if (!fspath.is_absolute()) {
    fspath = std::filesystem::absolute(fspath, ec);
    if (ec) {
      DLOG_F("failed std::filesystem::absolute(%s).", path.c_str());
      return;
    }
  }

  fspath = fspath.make_preferred();
  bool is_dir = std::filesystem::is_directory(fspath, ec);
  if (!is_dir || ec) {
    fspath = fspath.parent_path();
  }
  path_ = to_string(fspath.u8string());

  std::wstring pattern = fspath.wstring() + to_wstring(kNativeSeparator) + L"*.*";
  WIN32_FIND_DATA data{};
  HANDLE handle = ::FindFirstFileW(pattern.c_str(), &data);
  if (handle == INVALID_HANDLE_VALUE) {
    DLOG_F("failed to ::FindFirstFile().");
    return;
  }

  std::vector<FileEntry> children;
  do {
    int flags = FileEntry::Flags::None;
    size_t size = 0;
    if (::wcscmp(data.cFileName, L".") == 0 ||
        ::wcscmp(data.cFileName, L"..") == 0) {
      continue;
    } else if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (!include_subdirectories) {
        continue;
      }
      flags |= FileEntry::Flags::Directory;
    } else {
      if (!include_files) {
        continue;
      }
      if (data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) {
        flags |= FileEntry::Flags::System;
      } else if (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
        flags |= FileEntry::Flags::Hidden;
      } else {
        ULARGE_INTEGER ul;
        ul.HighPart = data.nFileSizeHigh;
        ul.LowPart = data.nFileSizeLow;
        size = ul.QuadPart;
      }
    }

    FileEntry entry;
    entry.valid_ = true;
    entry.filename_ = to_string(data.cFileName);
    entry.path_ = to_string(
        fspath.wstring() + to_wstring(kNativeSeparator) + data.cFileName);
    entry.size_ = size;
    entry.created_ = filetime_to_system_clock(&data.ftCreationTime);
    entry.modified_ = filetime_to_system_clock(&data.ftLastWriteTime);
    entry.flags_ = flags;
    entries_.push_back(entry);
  } while (::FindNextFile(handle, &data) != 0);

  BOOL ret = ::FindClose(handle);
  assert(ret);

  // TODO: instead of doing sort when emitted, prepare sorted indexes or whatever in this time.
}

DirectoryList::~DirectoryList() {}

void DirectoryList::sort(SortType type, bool desc) {
  std::function<bool(const FileEntry&, const FileEntry&, bool)> comparer = comparer::name;
  if (type == SortType::NameAsc) {
    comparer = comparer::name;
  } else if (type == SortType::SizeAsc) {
    comparer = comparer::size;
  } else if (type == SortType::CreatedAsc) {
    comparer = comparer::created;
  } else if (type == SortType::ModifiedAsc) {
    comparer = comparer::modified;
  } else {
    assert(false && "unexpected sort type.");
    LOG_F(WARNING, "unexpected sort type (%d).", type);
  }

  using namespace std::placeholders;
  std::stable_sort(
      entries_.begin(), entries_.end(), std::bind(comparer, _1, _2, desc));

  sort_type_ = type;
  sort_desc_ = desc;
}

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

FileReader::FileReader(const std::string& path, size_t prefetch_size)
    : pos_(0) {
  filestream_ = std::unique_ptr<FileStream>(new FileStream(path));
  size_ = filestream_->Size();
  pos_ = -1;
  prefetched_.resize(prefetch_size, 0);

  // Prefetch
  Read(0, prefetch_size);
}

FileReader::~FileReader() {}

const uint8_t* FileReader::Read(size_t pos, size_t size) {
  if (pos > size_) {
    return nullptr;
  }

  if (pos < pos_ || pos >= (pos_ + prefetched_.size())) {
    size_t cache_size = prefetched_.size();
    size_t aligned = pos / cache_size * cache_size;
    if (pos_ != aligned) {
      size_t after = filestream_->Seek(aligned);
      if (after != aligned) {
        throw std::runtime_error("failed Seek().");
      }
    }
    pos_ = aligned;

    size_t avail = size_ - pos;
    size_t read_bytes = filestream_->Read(prefetched_.data(), std::min(avail, cache_size));
  }

  return prefetched_.data() + (pos - pos_);
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
  if (!std::filesystem::is_directory(fspath, ec) && !ec) {
    fspath = fspath.parent_path();
  }
  if (!fspath.is_absolute()) {
    fspath = std::filesystem::absolute(fspath, ec);
    if (ec) {
      return {};
    }
  }
  return rad::to_string(fspath.u8string());
}

std::string GetFullPath(const std::string& path) noexcept {
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
}

}  // namespace rad
