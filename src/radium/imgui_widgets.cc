#include "imgui_widgets.h"

using namespace ImGui;

void FillRect(const ImVec2& size, ImU32 col, float border_thickness,
    ImU32 border_col, float rounding, float margin) {
  ImDrawList* dd = ImGui::GetWindowDrawList();
  const ImVec2 r_min = ImGui::GetItemRectMin() + ImVec2(margin, margin);
  const ImVec2 r_max = ImGui::GetItemRectMin() + size - ImVec2(margin, margin);
  dd->AddRectFilled(r_min, r_max, col, rounding);
  if (border_thickness > 0.0f) {
    dd->AddRect(r_min, r_max, border_col, rounding, 0, border_thickness);
  }
}

void FillItem(ImU32 col, float border_thickness, ImU32 border_col,
    float rounding, float margin) {
  const ImVec2 size = ImGui::GetItemRectSize();
  FillRect(ImGui::GetItemRectSize(), col, border_thickness, border_col,
      rounding, margin);
}

bool RadioButton(const char* label, int* ref, int value) {
  ImVec2 preferred_size = ImGui::CalcTextSize(label) + ImVec2{32.0f, 0.0f};
  const float width =
      std::max(preferred_size.x, ImGui::GetContentRegionAvail().x);
  const float height = ImGui::GetFontSize();

  // InvisibleButton
  ImGui::PushID(label);
  bool clicked = ImGui::InvisibleButton("", {width, height});
  ImGui::PopID();
  bool hovered = ImGui::IsItemHovered();
  if (hovered) {
    FillItem(ImGui::GetColorU32(ImGuiCol_HeaderHovered), 0, 0, 0, -1.0f);
  }

  // Radio
  ImDrawList* dd = ImGui::GetWindowDrawList();
  const ImVec2 start = ImGui::GetItemRectMin();
  const ImVec2 end = start + ImVec2(height, height);
  const ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
  const ImVec2 center = start + (end - start) / 2.0f;
  const float radius = (end.x - start.x) / 2.0f - 2.0f;
  const float radius_check = radius - 2.0f;
  dd->AddCircle(center, radius, col);
  if (*ref == value) {
    dd->AddCircleFilled(center, radius_check, col);
  }

  // Text
  dd->AddText(start + ImVec2{height + 6.0f, 0},
      ImGui::GetColorU32(ImGuiCol_Text), label);

  if (clicked) {
    *ref = value;
  }
  return clicked;
}

bool CheckBox(const char* label, bool* checked) {
  ImVec2 preferred_size = ImGui::CalcTextSize(label) + ImVec2{32.0f, 0.0f};
  const float width =
      std::max(preferred_size.x, ImGui::GetContentRegionAvail().x);
  const float height = ImGui::GetFontSize();

  // InvisibleButton
  ImGui::PushID(label);
  bool clicked = ImGui::InvisibleButton("", {width, height});
  ImGui::PopID();
  bool hovered = ImGui::IsItemHovered();
  if (hovered) {
    FillItem(ImGui::GetColorU32(ImGuiCol_HeaderHovered), 0, 0, 0, -1.0f);
  }

  // Check
  ImDrawList* dd = ImGui::GetWindowDrawList();
  const float margin = 2.0f;
  const ImVec2 start = ImGui::GetItemRectMin() + ImVec2(margin, margin);
  const ImVec2 end =
      ImGui::GetItemRectMin() + ImVec2(height, height) - ImVec2(margin, margin);
  const float w = end.x - start.x;
  const float h = end.y - start.y;
  const ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
  dd->AddRect(start, end, col);  // border
  if (*checked) {
    const float pad_x = 3.0f;
    const float pad_y = 2.5f;
    const ImVec2 s1(start.x + pad_x, end.y - pad_y - h * 0.25f);
    const ImVec2 e1(start.x + pad_x + w * 0.25f, end.y - pad_y);
    const ImVec2 e2(start.x - pad_x + w, start.y + h * 0.1f);
    dd->AddLine(s1, e1, col, 1.5f);
    dd->AddLine(e1, e2, col, 1.5f);
  }

  // Text
  dd->AddText(start + ImVec2{height + 6.0f - margin, -margin},
      ImGui::GetColorU32(ImGuiCol_Text), label);

  if (clicked) {
    *checked = !(*checked);
  }

  return clicked;
}

bool BeginCombo(const char* label, const char* preview_value) {
  using namespace ImGui;

  ImGuiComboFlags flags = ImGuiComboFlags_NoArrowButton;
  ImGuiContext& g = *GImGui;
  ImGuiWindow* window = GetCurrentWindow();

  ImGuiNextWindowDataFlags backup_next_window_data_flags =
      g.NextWindowData.Flags;
  g.NextWindowData
      .ClearFlags();  // We behave like Begin() and need to consume those values
  if (window->SkipItems) return false;

  const ImGuiStyle& style = g.Style;
  const ImGuiID id = window->GetID(label);
  IM_ASSERT(
      (flags & (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)) !=
      (ImGuiComboFlags_NoArrowButton |
          ImGuiComboFlags_NoPreview));  // Can't use both flags together

  const float arrow_size =
      (flags & ImGuiComboFlags_NoArrowButton) ? 0.0f : GetFrameHeight();
  const ImVec2 label_size = CalcTextSize(label, NULL, true);
  const float w =
      (flags & ImGuiComboFlags_NoPreview) ? arrow_size : CalcItemWidth();
  const ImRect bb(window->DC.CursorPos,
      window->DC.CursorPos +
          ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
  const ImRect total_bb(
      bb.Min, bb.Max + ImVec2(label_size.x > 0.0f
                                  ? style.ItemInnerSpacing.x + label_size.x
                                  : 0.0f,
                           0.0f));
  ImVec2 base = GetCursorPos();
  ItemSize(total_bb, style.FramePadding.y);
  if (!ItemAdd(total_bb, id, &bb)) return false;

  // Open on click
  bool hovered, held;
  bool pressed = ButtonBehavior(bb, id, &hovered, &held);
  const ImGuiID popup_id = ImHashStr("##ComboPopup", 0, id);
  bool popup_open = IsPopupOpen(popup_id, ImGuiPopupFlags_None);
  pressed =
      (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsItemHovered());
  if (pressed && !popup_open) {
    ImVec2 curr = GetCursorPos();
    OpenPopupEx(popup_id, ImGuiPopupFlags_None);
    popup_open = true;
  }

  // Render shape
  const ImU32 frame_col =
      GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
  const float value_x2 = ImMax(bb.Min.x, bb.Max.x - arrow_size);
  RenderNavHighlight(bb, id);
  if (!(flags & ImGuiComboFlags_NoPreview))
    window->DrawList->AddRectFilled(bb.Min, ImVec2(value_x2, bb.Max.y),
        frame_col, style.FrameRounding,
        (flags & ImGuiComboFlags_NoArrowButton) ? ImDrawFlags_RoundCornersAll
                                                : ImDrawFlags_RoundCornersLeft);
  if (!(flags & ImGuiComboFlags_NoArrowButton)) {
    ImU32 bg_col = GetColorU32(
        (popup_open || hovered) ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    ImU32 text_col = GetColorU32(ImGuiCol_Text);
    window->DrawList->AddRectFilled(ImVec2(value_x2, bb.Min.y), bb.Max, bg_col,
        style.FrameRounding,
        (w <= arrow_size) ? ImDrawFlags_RoundCornersAll
                          : ImDrawFlags_RoundCornersRight);
    if (value_x2 + arrow_size - style.FramePadding.x <= bb.Max.x)
      RenderArrow(window->DrawList,
          ImVec2(
              value_x2 + style.FramePadding.y, bb.Min.y + style.FramePadding.y),
          text_col, ImGuiDir_Down, 1.0f);
  }
  RenderFrameBorder(bb.Min, bb.Max, style.FrameRounding);

  // Custom preview
  if (flags & ImGuiComboFlags_CustomPreview) {
    g.ComboPreviewData.PreviewRect =
        ImRect(bb.Min.x, bb.Min.y, value_x2, bb.Max.y);
    IM_ASSERT(preview_value == NULL || preview_value[0] == 0);
    preview_value = NULL;
  }

  // Render preview and label
  if (preview_value != NULL && !(flags & ImGuiComboFlags_NoPreview)) {
    if (g.LogEnabled) LogSetNextTextDecoration("{", "}");
    RenderTextClipped(bb.Min + style.FramePadding, ImVec2(value_x2, bb.Max.y),
        preview_value, NULL, NULL);
  }
  if (label_size.x > 0)
    RenderText(ImVec2(bb.Max.x + style.ItemInnerSpacing.x,
                   bb.Min.y + style.FramePadding.y),
        label);

  if (!popup_open) return false;

  g.NextWindowData.Flags = backup_next_window_data_flags;

  SetNextWindowPos({bb.Min.x, bb.Min.y});
  SetNextWindowSize({bb.GetWidth(), -1});

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 8.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 8.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));

  bool ret = BeginPopupEx(popup_id, ImGuiWindowFlags_NoDecoration);
  if (!ret) {
    ImGui::PopStyleVar(4);
  }
  return ret;
}

bool Selectable(const char* label, bool selected, ImGuiSelectableFlags flags,
    const ImVec2& size_arg) {
  using namespace ImGui;

  ImGuiWindow* window = GetCurrentWindow();
  if (window->SkipItems) return false;

  ImGuiContext& g = *GImGui;
  const ImGuiStyle& style = g.Style;

  // Submit label or explicit size to ItemSize(), whereas ItemAdd() will submit
  // a larger/spanning rectangle.
  ImGuiID id = window->GetID(label);
  ImVec2 label_size = CalcTextSize(label, NULL, true);
  ImVec2 size(size_arg.x != 0.0f ? size_arg.x : label_size.x,
      size_arg.y != 0.0f ? size_arg.y : label_size.y);
  ImVec2 pos = window->DC.CursorPos;
  pos.y += window->DC.CurrLineTextBaseOffset;

  // Text stays at the submission position, but bounding box may be extended on
  // both sides
  ImVec2 text_min = pos;
  text_min.x += 4.0f;
  text_min.y += 3.0f;

  const bool span_all_columns =
      (flags & ImGuiSelectableFlags_SpanAllColumns) != 0;
  const float min_x = span_all_columns ? window->ParentWorkRect.Min.x : pos.x;
  const float max_x =
      span_all_columns ? window->ParentWorkRect.Max.x : window->WorkRect.Max.x;
  if (size_arg.x == 0.0f || (flags & ImGuiSelectableFlags_SpanAvailWidth))
    size.x = ImMax(label_size.x, max_x - min_x);
  size.y += 2.0f;

  const ImVec2 text_max(min_x + size.x, pos.y + size.y);

  ItemSize(size, 0.0f);

  // Selectables are meant to be tightly packed together with no click-gap, so
  // we extend their box to cover spacing between selectable.
  ImRect bb(min_x, pos.y, text_max.x, text_max.y);
  if ((flags & ImGuiSelectableFlags_NoPadWithHalfSpacing) == 0) {
    const float spacing_x = span_all_columns ? 0.0f : style.ItemSpacing.x;
    const float spacing_y = style.ItemSpacing.y;
    const float spacing_L = IM_FLOOR(spacing_x * 0.50f);
    const float spacing_U = IM_FLOOR(spacing_y * 0.50f);
    bb.Min.x -= spacing_L;
    bb.Min.y -= spacing_U;
    bb.Max.x += (spacing_x - spacing_L);
    bb.Max.y += (spacing_y - spacing_U);
  }
  // if (g.IO.KeyCtrl) { GetForegroundDrawList()->AddRect(bb.Min, bb.Max,
  // IM_COL32(0, 255, 0, 255)); }

  // Modify ClipRect for the ItemAdd(), faster than doing a
  // PushColumnsBackground/PushTableBackground for every Selectable..
  const float backup_clip_rect_min_x = window->ClipRect.Min.x;
  const float backup_clip_rect_max_x = window->ClipRect.Max.x;
  if (span_all_columns) {
    window->ClipRect.Min.x = window->ParentWorkRect.Min.x;
    window->ClipRect.Max.x = window->ParentWorkRect.Max.x;
  }

  const bool disabled_item = (flags & ImGuiSelectableFlags_Disabled) != 0;
  const bool item_add = ItemAdd(bb, id, NULL,
      disabled_item ? ImGuiItemFlags_Disabled : ImGuiItemFlags_None);
  if (span_all_columns) {
    window->ClipRect.Min.x = backup_clip_rect_min_x;
    window->ClipRect.Max.x = backup_clip_rect_max_x;
  }

  if (!item_add) return false;

  const bool disabled_global =
      (g.CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
  if (disabled_item &&
      !disabled_global)  // Only testing this as an optimization
    BeginDisabled();

  // FIXME: We can standardize the behavior of those two, we could also keep the
  // fast path of override ClipRect + full push on render only, which would be
  // advantageous since most selectable are not selected.
  if (span_all_columns && window->DC.CurrentColumns)
    PushColumnsBackground();
  else if (span_all_columns && g.CurrentTable)
    TablePushBackgroundChannel();

  // We use NoHoldingActiveID on menus so user can click and _hold_ on a menu
  // then drag to browse child entries
  ImGuiButtonFlags button_flags = 0;
  if (flags & ImGuiSelectableFlags_NoHoldingActiveID) {
    button_flags |= ImGuiButtonFlags_NoHoldingActiveId;
  }
  if (flags & ImGuiSelectableFlags_NoSetKeyOwner) {
    button_flags |= ImGuiButtonFlags_NoSetKeyOwner;
  }
  if (flags & ImGuiSelectableFlags_SelectOnClick) {
    button_flags |= ImGuiButtonFlags_PressedOnClick;
  }
  if (flags & ImGuiSelectableFlags_SelectOnRelease) {
    button_flags |= ImGuiButtonFlags_PressedOnRelease;
  }
  if (flags & ImGuiSelectableFlags_AllowDoubleClick) {
    button_flags |= ImGuiButtonFlags_PressedOnClickRelease |
                    ImGuiButtonFlags_PressedOnDoubleClick;
  }

  const bool was_selected = selected;
  bool hovered, held;
  bool pressed = ButtonBehavior(bb, id, &hovered, &held, button_flags);

  // Auto-select when moved into
  // - This will be more fully fleshed in the range-select branch
  // - This is not exposed as it won't nicely work with some user side handling
  // of shift/control
  // - We cannot do 'if (g.NavJustMovedToId != id) { selected = false; pressed =
  // was_selected; }' for two reasons
  //   - (1) it would require focus scope to be set, need exposing
  //   PushFocusScope() or equivalent (e.g. BeginSelection() calling
  //   PushFocusScope())
  //   - (2) usage will fail with clipped items
  //   The multi-select API aim to fix those issues, e.g. may be replaced with a
  //   BeginSelection() API.
  if ((flags & ImGuiSelectableFlags_SelectOnNav) && g.NavJustMovedToId != 0 &&
      g.NavJustMovedToFocusScopeId == g.CurrentFocusScopeId)
    if (g.NavJustMovedToId == id) selected = pressed = true;

  // Update NavId when clicking or when Hovering (this doesn't happen on most
  // widgets), so navigation can be resumed with gamepad/keyboard
  if (pressed || (hovered && (flags & ImGuiSelectableFlags_SetNavIdOnHover))) {
    if (!g.NavDisableMouseHover && g.NavWindow == window &&
        g.NavLayer == window->DC.NavLayerCurrent) {
      SetNavID(id, window->DC.NavLayerCurrent, g.CurrentFocusScopeId,
          WindowRectAbsToRel(window, bb));  // (bb == NavRect)
      g.NavDisableHighlight = true;
    }
  }
  if (pressed) MarkItemEdited(id);

  if (flags & ImGuiSelectableFlags_AllowItemOverlap) SetItemAllowOverlap();

  // In this branch, Selectable() cannot toggle the selection so this will never
  // trigger.
  if (selected != was_selected)  //-V547
    g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledSelection;

  // Render
  if (hovered || selected) {
    const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_HeaderActive
                                  : hovered         ? ImGuiCol_HeaderHovered
                                                    : ImGuiCol_Header);
    RenderFrame(bb.Min, bb.Max, col, false, 0.0f);
  }
  RenderNavHighlight(bb, id,
      ImGuiNavHighlightFlags_TypeThin | ImGuiNavHighlightFlags_NoRounding);

  if (span_all_columns && window->DC.CurrentColumns)
    PopColumnsBackground();
  else if (span_all_columns && g.CurrentTable)
    TablePopBackgroundChannel();

  RenderTextClipped(text_min, text_max, label, NULL, &label_size,
      style.SelectableTextAlign, &bb);

  // Automatically close popups
  if (pressed && (window->Flags & ImGuiWindowFlags_Popup) &&
      !(flags & ImGuiSelectableFlags_DontClosePopups) &&
      !(g.LastItemData.InFlags & ImGuiItemFlags_SelectableDontClosePopup))
    CloseCurrentPopup();

  if (disabled_item && !disabled_global) EndDisabled();

  IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
  return pressed;  //-V1020
}

void EndCombo() {
  ImGui::PopStyleVar(4);
  ImGui::EndCombo();
}

void DrawChevron(bool opened, float size) {
  size += 0.5f;
  ImVec2 center = ImGui::GetCursorScreenPos();
  center.x += size / 2.0f;
  center.y += size / 2.0f;

  ImGui::Dummy({size, size});
  ImVec2 next = ImGui::GetCursorPos();

  ImGui::SetCursorPos(center);
  ImDrawList* dd = ImGui::GetCurrentWindow()->DrawList;
  const ImU32 color =
      ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Text));
  constexpr float thickness = 1.25;

  const float w = size / 2.0f;
  const float h = size / 4.0f;
  if (opened) {
    const ImVec2 points[3]{
        {center.x - w, center.y - h},
        {center.x, center.y + h},
        {center.x + w, center.y - h},
    };
    dd->AddPolyline(
        points, IM_ARRAYSIZE(points), ImGui::GetColorU32(color), 0, thickness);
  } else {
    const ImVec2 points[3]{
        {center.x - h, center.y - w},
        {center.x + h, center.y},
        {center.x - h, center.y + w},
    };
    dd->AddPolyline(points, IM_ARRAYSIZE(points), color, 0, thickness);
  }

  ImGui::SetCursorPos(next);
}

bool BeginCombo2(const char* id, const char* preview_value) {
  if (PushButton(id)) {
    if (ImGui::BeginPopup("##combo2")) {
      return true;
    }
  }
  return false;
}

bool Selectable2(const char* label, bool selected) { return false; }

void EndCombo2() { ImGui::EndPopup(); }

void Spinner(
    float radius, float thickness, int num_segments, float speed, ImU32 color) {
  ImDrawList* dd = ImGui::GetWindowDrawList();
  ImVec2 pos = ImGui::GetCursorScreenPos();
  ImVec2 size{radius * 2, radius * 2};
  ImRect bb{pos, {pos.x + size.x, pos.y + size.y}};
  ImGui::ItemSize(bb);
  if (!ImGui::ItemAdd(bb, 0)) return;

  float time = static_cast<float>(ImGui::GetCurrentContext()->Time) * speed;
  dd->PathClear();
  int start = static_cast<int>(abs(ImSin(time) * (num_segments - 5)));
  float a_min = IM_PI * 2.0f * ((float)start) / (float)num_segments;
  float a_max = IM_PI * 2.0f * ((float)num_segments - 3) / (float)num_segments;
  ImVec2 centre = {pos.x + radius, pos.y + radius};
  for (int i = 0; i < num_segments; ++i) {
    float a = a_min + ((float)i / (float)num_segments) * (a_max - a_min);
    dd->PathLineTo({centre.x + ImCos(a + time * 8) * radius,
        centre.y + ImSin(a + time * 8) * radius});
  }
  dd->PathStroke(color, false, thickness);
}

void Frame(const ImVec2& size, ImU32 col, float border_thickness,
    ImU32 border_col, float rounding) {
  ImGui::PushID("##frame");
  ImGui::InvisibleButton("", size);
  ImGui::PopID();
  FillItem(col, 1.0f, 0x70707070, 3.0f, 1.0f);
}

bool TextButton(
    const char* label, bool toggled, const ImVec2& preferred_size, ImU32 bg) {
  ImGuiStyle& style = ImGui::GetStyle();
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  ImDrawList* dd = ImGui::GetWindowDrawList();
  const ImVec2 text_size = ImGui::CalcTextSize(label, NULL, true);
  const ImVec2 actual_size = {std::max(preferred_size.x, text_size.x),
      std::max(preferred_size.y, text_size.y)};
  {
    const ImGuiID id = window->GetID(label);
    ImVec2 size = ImGui::CalcItemSize(
        actual_size + style.FramePadding * 2.0f, 0.0f, 0.0f);
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    ImGui::ItemSize(size, style.FramePadding.y);
    if (ImGui::ItemAdd(bb, id)) {
      bool hovered, held;
      ImGui::ButtonBehavior(bb, id, &hovered, &held);
    }
  }

  bool clicked = ImGui::IsItemClicked();
  FillItem(bg, 0, 0, 5.0f, 0);

  bool hovered = ImGui::IsItemHovered();
  ImU32 fg = hovered   ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
             : toggled ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
                       : ImGui::GetColorU32(ImGuiCol_Text);
  if (hovered) {
    ImU32 col = ImGui::GetColorU32(ImGuiCol_FrameBgHovered);
    FillItem(0x24ffffff, 0, 0, 5.0f, 0);
  }

  ImVec2 rmin = ImGui::GetItemRectMin();
  ImVec2 rmax = ImGui::GetItemRectMax();
  ImVec2 pos = {
      rmin.x + style.FramePadding.x,
      rmin.y + (rmax.y - rmin.y) / 2.0f - ImGui::GetFontSize() / 2.0f,
  };

  dd->AddText(pos, fg, label);
  return clicked;
}

bool PushButton(const char* label, bool toggled, const ImVec2& preferred_size) {
  bool ret = TextButton(
      label, toggled, preferred_size, ImGui::GetColorU32(ImGuiCol_Button));
  return ret;
}

bool SliderImpl(ImGuiDataType data_type, const char* label, void* p_data,
    void* p_min, void* p_max, float perc, ImGuiSliderFlags flags) {
  using namespace ImGui;
  ImGuiWindow* window = GetCurrentWindow();
  if (window->SkipItems) return false;

  ImGuiContext& g = *GImGui;
  const ImGuiStyle& style = g.Style;
  const ImGuiID id = window->GetID(label);
  const float w = CalcItemWidth();

  float font_size = ImGui::GetFontSize();
  float band = 5.0f;
  float radius = 6.0f;
  float min_x = window->DC.CursorPos.x;
  float min_y = window->DC.CursorPos.y;
  float max_x = window->DC.CursorPos.x + w;
  float max_y =
      window->DC.CursorPos.y + font_size + style.FramePadding.y * 2.0f;
  const ImRect frame_bb({min_x, min_y}, {max_x, max_y});
  const ImRect total_bb(
      {frame_bb.Min.x, frame_bb.Min.y}, {frame_bb.Max.x, frame_bb.Max.y});
  const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
  ItemSize(total_bb, style.FramePadding.y);
  if (!ItemAdd(total_bb, id, &frame_bb,
          temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
    return false;

  // Default format string when passing NULL
  const char* format = DataTypeGetInfo(data_type)->PrintFmt;

  const bool hovered = ItemHoverable(frame_bb, id, 0);
  bool temp_input_is_active = temp_input_allowed && TempInputIsActive(id);
  if (!temp_input_is_active) {
    // Tabbing or CTRL-clicking on Slider turns it into an input box
    const bool input_requested_by_tabbing =
        temp_input_allowed && (g.LastItemData.StatusFlags &
                                  ImGuiItemStatusFlags_FocusedByTabbing) != 0;
    const bool clicked = hovered && IsMouseClicked(0, id);
    const bool make_active = (input_requested_by_tabbing || clicked ||
                              g.NavActivateId == id || g.NavActivateId == id);
    if (make_active && clicked) SetKeyOwner(ImGuiKey_MouseLeft, id);
    if (make_active && temp_input_allowed)
      if (input_requested_by_tabbing || (clicked && g.IO.KeyCtrl) ||
          g.NavActivateId == id)
        temp_input_is_active = true;

    if (make_active && !temp_input_is_active) {
      SetActiveID(id, window);
      SetFocusID(id, window);
      FocusWindow(window);
      g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
    }
  }

  if (temp_input_is_active) {
    // Only clamp CTRL+Click input when ImGuiSliderFlags_AlwaysClamp is set
    const bool is_clamp_input = (flags & ImGuiSliderFlags_AlwaysClamp) != 0;
    return TempInputScalar(frame_bb, id, label, data_type, p_data, format,
        is_clamp_input ? p_min : NULL, is_clamp_input ? p_max : NULL);
  }

  // Draw frame
  const ImU32 frame_col = GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive
                                      : hovered        ? ImGuiCol_FrameBgHovered
                                                       : ImGuiCol_FrameBg);
  RenderNavHighlight(frame_bb, id);

  // Slider behavior
  ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 1.0f);
  ImRect grab_bb;
  bool changed = false;
  auto bb = frame_bb;
  if (SliderBehavior(
          bb, id, data_type, p_data, p_min, p_max, format, flags, &grab_bb)) {
    changed = true;
    MarkItemEdited(id);
  }
  ImGui::PopStyleVar();

  // Render grab
  if (grab_bb.Max.x > grab_bb.Min.x) {
    float mid_w = grab_bb.Min.x + grab_bb.GetWidth() * perc;
    float mid_h = bb.GetHeight() / 2.0f + 1.0f;
    float y0 = mid_h - band / 2.0f;
    float y1 = mid_h + band / 2.0f;
    float rounding = band;
    ImVec2 min = {bb.Min.x, bb.Min.y + y0};
    ImVec2 max = {bb.Max.x, bb.Min.y + y1};
    window->DrawList->AddRectFilled(min - ImVec2{0.5f, 0.5f},
        max + ImVec2{0.5f, 0.5f}, 0xaf000000, rounding, 0);
    window->DrawList->AddRectFilled(min, {mid_w, max.y},
        ImGui::GetColorU32(ImGuiCol_SliderGrabActive), rounding, 0);
    window->DrawList->AddRectFilled(
        {mid_w, min.y}, max, frame_col, rounding, 0);

    ImVec2 center{mid_w, bb.Min.y + mid_h};
    window->DrawList->PushClipRect(
        {center - ImVec2{radius, radius}}, {center + ImVec2{radius, radius}});
    window->DrawList->AddCircleFilled(center, radius + 1.0f, 0xaf000000);
    window->DrawList->AddCircleFilled(center, radius,
        GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive
                                     : ImGuiCol_SliderGrab));
    window->DrawList->PopClipRect();
  }

  ImGui::SetItemUsingMouseWheel();
  if (ImGui::IsItemHovered()) {
    float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f) {
      if (ImGui::IsItemActive()) {
        ImGui::ClearActiveID();
      } else {
        if (data_type == ImGuiDataType_Float) {
          const float min = *(float*)p_min;
          const float max = *(float*)p_max;
          *(float*)p_data += std::clamp((-wheel) / 100.0f, min, max);
          changed = true;
        }
      }
    }
  }

  IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
  return changed;
}

bool Slider(const char* label, float* p_data, float min, float max,
    ImGuiSliderFlags flags) {
  float perc = (*p_data - min) / (max - min);
  return SliderImpl(
      ImGuiDataType_Float, label, p_data, &min, &max, perc, flags);
}

bool Slider(
    const char* label, int* p_data, int min, int max, ImGuiSliderFlags flags) {
  float perc = ((float)*p_data - min) / (max - min);
  return SliderImpl(ImGuiDataType_S32, label, p_data, &min, &max, perc, flags);
}

void HyperLink(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  TextV(fmt, args);
  va_end(args);

  if (ImGui::GetHoveredID() == ImGui::GetItemID()) {
    if (ImGui::IsItemHovered()) {
      ImDrawList* dd = ImGui::GetCurrentWindow()->DrawList;
      ImVec2 ir0 = ImGui::GetItemRectMin();
      ImVec2 ir1 = ImGui::GetItemRectMax();
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      dd->AddLine({ir0.x, ir1.y - 1.0f}, {ir1.x, ir1.y - 1.0f}, 0x70f0f0f0);
      dd->AddLine({ir0.x, ir1.y - 0.0f}, {ir1.x, ir1.y - 0.0f}, 0x70101010);
    }
  }
}

bool TreeHeader(const char* label, bool default_open) {
  ImVec2 base = ImGui::GetCursorPos();
  ImGui::BeginGroup();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4());
  int flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Framed;
  if (default_open) {
    flags |= ImGuiTreeNodeFlags_DefaultOpen;
  }
  bool opened = ImGui::TreeNodeBehavior(ImGui::GetID(label), flags, label);
  ImGui::PopStyleColor();

  if (ImGui::IsItemVisible()) {
    const ImRect rect{ImGui::GetItemRectMin(), ImGui::GetItemRectMax()};
    ImGui::SetCursorPosX(base.x);
    ImGui::SetCursorPosY(rect.GetCenter().y - 12.0f / 2.0f);
    DrawChevron(opened, 12.0f);

    ImGui::SameLine();
    ImGui::SetCursorPosY(rect.GetCenter().y - GetFontSize() / 2.0f);
    ImGui::Text(label);

    if (opened) {
      ImGui::Spacing();
    }
  }
  ImGui::EndGroup();
  ImGui::Indent();
  return opened;
}
