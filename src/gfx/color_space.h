#pragma once

enum class ColorPrimaries : int {
  Unknown,
  BT601 = 1,
  sRGB = 2,
  BT709 = 2,
  BT2020 = 3,
};

enum class TransferCharacteristics : int {
  Unknown,
  sRGB,
  Linear,
  ST2084,  // PQ
  STDB67,  // HLG
};

enum class MatrixCoefficients : int {};

enum class ColorRange : int {
  Unknown,
  Limited,
  Full,
};
