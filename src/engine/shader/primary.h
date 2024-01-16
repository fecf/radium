#pragma once

#include "../../gfx/color_space.h"

enum class Filter : int {
  Nearest = 0,
  Bilinear,
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
  
  // color profile of texteure
  ColorPrimaries color_primaries;
  TransferCharacteristics transfer_characteristics;

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