#include "material_symbols.h"

const std::vector<unsigned short>& GetIconRanges() {
  static std::vector<unsigned short> ranges{ICON_MIN_MD, ICON_MAX_16_MD, 0};
  return ranges;
}

