#pragma once

#include <string>
#include <cstdint>

namespace rad {

std::wstring to_wstring(const std::string& str);
std::wstring to_wstring(const std::u8string& str);
std::string to_string(const std::wstring& str);
std::string to_string(const std::u8string& str);

std::string readable_byte_count(size_t bytes, bool si);

}  // namespace rad
