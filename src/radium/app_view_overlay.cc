#include "app.h"
#include "app_impl.h"
#include "imgui_widgets.h"

#include <base/minlog.h>
#include <base/text.h>
#include <image/image.h>
#include <engine/engine.h>
#include <magic_enum.hpp>

#include "material_symbols.h"

void createTable(const std::string& name, const nlohmann::json& json) {
  int count = 0;
  for (const auto& [key, value] : json.items()) {
    if (value.is_object() || value.is_array()) {
      ImGui::Text("%s:", key.c_str());
      ImGui::Indent();
      createTable(key, value);
      ImGui::Unindent();
    } else {
      ImGui::Text("%s: %s", key.c_str(), value.dump().c_str());
    }
    count++;
  }
}

void View::renderImGuiOverlay() {
  if (!m.overlay_show) {
    return;
  }

  auto content = m.GetPresentContent();
  if (!content || !content->image || !content->texture) {
    return;
  }

  ImGui::SetNextWindowPos({0, 0});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushFont(a.GetFont(App::Small));
  if (ImGui::Begin("##overlay", 0,
          ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration |
              ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground |
              ImGuiWindowFlags_NoFocusOnAppearing)) {
    const uint32_t col = 0xf0000000;
    const float spacing = 16.0f;

  #ifdef _DEBUG
    static bool debug = true;
    if (ImGui::IsKeyPressed(ImGuiKey_D)) {
      debug = !debug;
    }
#else
    static bool debug = false;
#endif
    if (debug) {
      ImGui::BeginGroup();
      std::string imgui = std::format("fps:{:.04f} | delta:{:.04f}", ImGui::GetIO().Framerate, ImGui::GetIO().DeltaTime);
      ImGui::Text(imgui.c_str());

      std::string layout =
          std::format("center:{:.2f}, {:.2f} | rotate:{:.2f} | scale:{:.2f}",
              m.content_cx, m.content_cy, m.content_rotate, m.content_zoom);
      ImGui::Text(layout.c_str());

      auto ss = magic_enum::enum_name(content->image->pixel_format);
      std::string detail = std::format("{} | {} | {}",
        magic_enum::enum_name(content->image->decoder).data(),
        magic_enum::enum_name(content->image->pixel_format).data(),
        magic_enum::enum_name(content->image->color_space).data()
      );
      ImGui::Text("%s", detail.c_str());

      ImGui::Text("%s", m.cwd.c_str());
      for (const auto sp : m.contents) {
        ImGui::Text("%s", sp->path.c_str());
        if (sp->texture) {
          ImGui::SameLine();
          ImGui::Text("%s", "[Loaded]");
        }
      }

      // for (const auto& [path, sp] : m.thumbnails) {
      //   ImGui::Text("%s", path.c_str());
      //   if (sp->texture) {
      //     ImGui::SameLine();
      //     ImGui::Text("%s", "[Loaded]");
      //   }
      // }

      auto json = engine().GetStats();
      createTable("debug2", json);

      ImGui::EndGroup();
      ImGui::GetBackgroundDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), col);
      ImGui::Dummy({1.0f, spacing});
    }

    // std::string filename = m.content_path;
    std::filesystem::path fspath(rad::to_wstring(m.present_content_path));
    std::string filename = rad::to_string(fspath.filename().u8string());

    std::string str = filename;
    str += std::format(" | {}x{}", content->image->width, content->image->height);
    str += std::format(" | {:.2f}x", m.content_zoom);
    // str += std::format(" | {}", magic_enum::enum_name(content->image->pixel_format).data());
    // str += std::format(" | {}", magic_enum::enum_name(content->image->color_space).data());

    ImGui::BeginGroup();
    if (auto content = m.GetContent()) {
      auto base = ImGui::GetCursorPos();
      if (!content->completed) {
        ImGui::SetCursorPos(base + ImVec2{2.0f, 2.0f});
        Spinner(ImGui::GetFontSize() / 2.0f - 2.0f, 1.5f, 32, 1.0f, 0xc0ffffff);
        ImGui::SetCursorPos(base + ImVec2{ImGui::GetFontSize() + 2.0f, 0.0f});
      }
    }
    ImGui::Text("%s", str.c_str());
    ImGui::EndGroup();
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), col);
    ImGui::Dummy({1.0, spacing});

    if (content->image->metadata.size()) {
      for (const auto& [key, value] : content->image->metadata) {
        std::string str = std::format("{}: {}", key.c_str(), value.c_str());
        ImGui::Text("%s", str.c_str());
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), col);
      }
    }
    ImGui::End();
  }
  ImGui::PopFont();
  ImGui::PopStyleVar();
  ImGui::PopStyleVar();
  ImGui::PopStyleVar();
}
