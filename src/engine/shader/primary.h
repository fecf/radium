#pragma once

enum class Filter : int {
  Nearest = 0,
  Bilinear,
};

enum class ColorSpace : int {
  sRGB = 0,
  Linear,
};

enum class ToneMapping : int {
  None = 0,
  Standard,
  StandardInverse,
  Reinhard,
  ReinhardInverse,
  ACES,
  ACESInverse,
};

enum class Colorblind : int {
  None = 0,
  Protanopia,
  Deuteranopia,
  Tritanopia,
};

#pragma pack(push, 1)
struct Constants {
  float alpha;       // 0.0f ~ 1.0f
  ColorSpace cs_src;
  ColorSpace cs_dst;
  Filter filter;
  float brightness;  // -1.0f ~ 1.0f
  float contrast;    // -1.0f ~ 1.0f
  float exposure;    // -10.0f ~ 10.0f
  float luminance;   // -1.0f ~ 1.0f
  float chroma;      // -1.0f ~ 1.0f
  float hue;         // -180.0f ~ 180.0f
  Colorblind cb_mode;
  float cb_intensity;
  ToneMapping tone_mapping;
};
#pragma pack(pop)