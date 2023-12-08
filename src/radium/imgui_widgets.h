#pragma once

#include <imgui.h>

#include <string>
#include <vector>

const ImVec2 kIconPadding = {8.0f, 8.0f};

void FillRect(const ImVec2& size, ImU32 col, float border_thickness = 0.0f,
    ImU32 border_col = 0x0, float rounding = 0.0f, float margin = 0.0f);
void FillItem(ImU32 col, float border_thickness = 0.0f, ImU32 border_col = 0x0,
    float rounding = 0.0f, float margin = 0.0f);

bool RadioButton(const char* label, int* ref, int value);
bool CheckBox(const char* label, bool* ref);

template <typename T>
bool RadioButton(const char* label, T* ref, T value) {
  static_assert(sizeof(T) <= 4, "unexpected sizeof(T)");

  if constexpr (sizeof(T) == 4) {
    return RadioButton(label, (int*)ref, (int)value);
  } else {
    int temp = (int)(*ref);
    if (RadioButton(label, (int*)&temp, (int)value)) {
      *ref = (T)temp;
      return true;
    }
    return false;
  }
}

bool BeginCombo(const char* id, const char* preview_value);
bool Selectable(const char* label, bool selected,
    ImGuiSelectableFlags flags = 0, const ImVec2& size_arg = {});
void EndCombo();

// TODO:
void DrawChevron(bool opened, float size);
bool BeginCombo2(const char* id, const char* preview_value);
bool Selectable2(const char* label, bool selected);
void EndCombo2();

void Spinner(
    float radius, float thickness, int num_segments, float speed, ImU32 color);
void Frame(const ImVec2& size, ImU32 col, float border_thickness,
    ImU32 border_col, float rounding);
bool TextButton(const char* label, bool toggled = false,
    const ImVec2& preferred_size = ImVec2(), ImU32 bg = 0x00000000);
bool PushButton(const char* label, bool toggled = false,
    const ImVec2& preferred_size = ImVec2());
bool Slider(
    const char* label, int* v, int min, int max, ImGuiSliderFlags flags = 0);
bool Slider(const char* label, float* v, float min, float max,
    ImGuiSliderFlags flags = 0);

void HyperLink(const char* fmt, ...);
bool TreeHeader(const char* label, bool default_open = false);
