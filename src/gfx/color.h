#pragma once

#include <string>

namespace rad {

class Color {
 public:
  Color();
  Color(const std::string& rgba_hex);
  Color(uint32_t rgba_hex);
  Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
  Color(float r, float g, float b, float a = 1.0f);

  Color linear_to_srgb() const;
  Color srgb_to_linear() const;
  std::string hex(int channels = 4) const;
  std::string str(int channels = 4) const;
  uint32_t rgba8() const;
  uint32_t abgr8() const;

  float r() const noexcept { return r_; }
  float g() const noexcept { return g_; }
  float b() const noexcept { return b_; }
  float a() const noexcept { return a_; }

  Color scale(float multiply) const;

 private:
  float r_, g_, b_, a_;
};

}  // namespace rad
