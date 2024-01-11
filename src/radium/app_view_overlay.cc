#include "app_impl.h"

#include <image/image.h>

#include "material_symbols.h"

void View::renderImGuiOverlay() {
  if (!m.overlay_show || m.thumbnail_show) {
    return;
  }

  ImGui::SetNextWindowPos({0, 0});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 16.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 8.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 2.0f));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.9f));
  if (ImGui::Begin("##overlay", 0,
          ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration |
              ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing)) {

    ImGui::Text("%s %s", ICON_MD_PHOTO, m.present_content_path.c_str());
    if (auto content = m.GetPresentContent()) {
      if (content->image) {
        ImGui::Text("%s %dx%d", ICON_MD_STRAIGHTEN, content->image->width, content->image->height);
        ImGui::SameLine(0.0, 8.0f);
        ImGui::Text("%s %.2fx", ICON_MD_ZOOM_IN, m.content_zoom);
        ImGui::SameLine(0.0, 8.0f);
        ImGui::Text("%s %.0f", ICON_MD_ROTATE_RIGHT, m.content_rotate);

        if (content->image->metadata.size()) {
          ImGui::Spacing();

          if (ImGui::BeginTable("overlay_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableHeader("Name");
            ImGui::TableHeader("Value");
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Value");

              for (const auto& [key, value] : content->image->metadata) {
                ImGui::TableNextColumn();
                ImGui::Text("%s", key.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s", value.c_str());
              }
            ImGui::EndTable();
          }
        }
      }
    }

    ImGui::End();
  }
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
  ImGui::PopStyleVar();
  ImGui::PopStyleVar();
}