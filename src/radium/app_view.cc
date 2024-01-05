#include "app_impl.h"
#include "app.h"

#include <engine/engine.h>
#include <base/algorithm.h>
#include <base/platform.h>

void View::Update() {
  i.EvictUnusedContent();
  i.EvictUnusedThumbnail();
  renderImGui();
  renderContent();
  renderThumbnail();
}

void View::renderImGui() {
  ImGuiStyle& style = ImGui::GetStyle();

  style.FrameBorderSize = 0.0f;
  style.ChildBorderSize = 0.0f;
  style.WindowBorderSize = 0.0f;
  style.PopupBorderSize = 0.0f;
  style.ItemInnerSpacing = ImVec2(0.0f, 4.0f);

  ImVec4 kText = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
  ImVec4 kAccent = ImVec4(0.22f, 0.22f, 0.22f, 1.0f);
  ImVec4 kAccentDark = ImVec4(0.15f, 0.4f, 0.2f, 1.0f);
  ImVec4 kHeader = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
  ImVec4 kHeaderHovered = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
  ImVec4 kPopup = ImVec4(0.0f, 0.0f, 0.0f, 0.75f);
  style.Colors[ImGuiCol_Text] = kText;
  style.Colors[ImGuiCol_ChildBg] = ImVec4();
  style.Colors[ImGuiCol_WindowBg] = ImVec4();
  style.Colors[ImGuiCol_TitleBg] = kHeader;
  style.Colors[ImGuiCol_TitleBgCollapsed] = kHeader;
  style.Colors[ImGuiCol_Button] = kHeader;
  style.Colors[ImGuiCol_ButtonHovered] = kAccent;
  style.Colors[ImGuiCol_ButtonActive] = kAccent;
  style.Colors[ImGuiCol_FrameBg] = kHeader;
  style.Colors[ImGuiCol_FrameBgHovered] = kHeader;
  style.Colors[ImGuiCol_FrameBgActive] = kHeader;
  style.Colors[ImGuiCol_PopupBg] = kPopup;
  style.Colors[ImGuiCol_Header] = kHeader;
  style.Colors[ImGuiCol_HeaderHovered] = kHeaderHovered;
  style.Colors[ImGuiCol_HeaderActive] = kHeader;
  style.Colors[ImGuiCol_ScrollbarBg] = ImVec4();
  style.Colors[ImGuiCol_ResizeGrip] = kHeader;
  style.Colors[ImGuiCol_ResizeGripHovered] = kHeader;
  style.Colors[ImGuiCol_SliderGrab] = kAccent;
  style.Colors[ImGuiCol_SliderGrabActive] = kAccent;
  style.Colors[ImGuiCol_ScrollbarGrab] = kHeader;
  style.Colors[ImGuiCol_ScrollbarGrabHovered] = kHeader;
  style.Colors[ImGuiCol_Tab] = kHeader;
  style.Colors[ImGuiCol_TabActive] = kAccentDark;
  style.Colors[ImGuiCol_TabHovered] = kAccentDark;


  // debug
#ifdef _DEBUG
  static bool debug = true;
  if (ImGui::IsKeyPressed(ImGuiKey_D)) {
    debug = !debug;
  }
#else
  static bool debug = false;
#endif
  if (debug) {
    ImGui::SetNextWindowPos({16, 16});
    ImGui::SetNextWindowSize({800, 800});
    if (ImGui::Begin("debug", 0,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs)) {
      if (ImGui::BeginTable("##table", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextColumn();

        ImGui::Text("Framerate");
        ImGui::TableNextColumn();
        ImGui::Text("%f", ImGui::GetIO().Framerate);
        ImGui::TableNextColumn();

        ImGui::Text("Delta");
        ImGui::TableNextColumn();
        ImGui::Text("%f", ImGui::GetIO().DeltaTime);
        ImGui::TableNextColumn();

        ImGui::Text("Path");
        ImGui::TableNextColumn();
        ImGui::Text("%s", m.content_path.c_str());
        ImGui::TableNextColumn();

        ImGui::Text("Current Path");
        ImGui::TableNextColumn();
        ImGui::Text("%s", m.latest_content_path.c_str());
        ImGui::TableNextColumn();

        ImGui::Text("Current Directory");
        ImGui::TableNextColumn();
        ImGui::Text("%s", m.cwd.c_str());
        ImGui::TableNextColumn();

        ImGui::Text("Content Layout");
        ImGui::TableNextColumn();
        ImGui::Text("cx=%.02f cy=%.02f rotate=%.02f scale=%.02f", m.content_cx, m.content_cy, m.content_rotate, m.content_zoom);
        ImGui::TableNextColumn();

        ImGui::Text("Stored Contents");
        ImGui::TableNextColumn();
        for (const auto sp : m.contents) {
          ImGui::Text("%s", sp->path.c_str());
          if (sp->texture) {
            ImGui::SameLine();
            ImGui::Text("%s", "[Loaded]");
          }
        }
        ImGui::TableNextColumn();

        ImGui::Text("Stored Thumbnails");
        ImGui::TableNextColumn();

        for (const auto sp : m.thumbnails) {
          ImGui::Text("%s", sp->path.c_str());
          if (sp->texture) {
            ImGui::SameLine();
            ImGui::Text("%s", "[Loaded]");
          }
        }
        ImGui::TableNextColumn();
        
        ImGui::EndTable();
      }
      ImGui::End();
    }
  }

  // global input event
  if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
    i.Dispatch(Intent::ToggleThumbnail{});
  }

  // content
  const ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos({0, 0});
  ImGui::SetNextWindowSize(io.DisplaySize);
  if (ImGui::Begin("##content", 0, ImGuiWindowFlags_NoDecoration)) {
    if (ImGui::IsWindowFocused()) {
      if (ImGui::GetIO().MouseWheel > 0) {
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
          i.Dispatch(Intent::ZoomIn{});
        } else {
          i.Dispatch(Intent::OpenPrev{});
        }
      } else if (ImGui::GetIO().MouseWheel < 0) {
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
          i.Dispatch(Intent::ZoomOut{});
        } else {
          i.Dispatch(Intent::OpenNext{});
        }
      }

      static ImVec2 drag_start_offset{};
      static ImVec2 drag_start_mouse_pos{};
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
        drag_start_offset = { m.content_cx, m.content_cy, };
        drag_start_mouse_pos = ImGui::GetIO().MousePos;
      }
      if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        const float& dx = ImGui::GetIO().MouseDelta.x;
        const float& dy = ImGui::GetIO().MouseDelta.y;
        i.Dispatch(Intent::Center{
          (drag_start_offset.x + (ImGui::GetIO().MousePos.x - drag_start_mouse_pos.x)),
          (drag_start_offset.y + (ImGui::GetIO().MousePos.y - drag_start_mouse_pos.y)),
        });
      }

      if (ImGui::IsKeyDown(ImGuiKey_LeftAlt)) {
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
          i.Dispatch(Intent::ToggleFullscreen{});
        }
      } else if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
        if (ImGui::IsKeyPressed(ImGuiKey_0, false)) {
          i.Dispatch(Intent::Reset{});
        } else if (ImGui::IsKeyPressed(ImGuiKey_1, false)) {
          i.Dispatch(Intent::Zoom{1.0f});
        } else if (ImGui::IsKeyPressed(ImGuiKey_2, false)) {
          i.Dispatch(Intent::Zoom{2.0f});
        } else if (ImGui::IsKeyPressed(ImGuiKey_3, false)) {
          i.Dispatch(Intent::Zoom{4.0f});
        } else if (ImGui::IsKeyPressed(ImGuiKey_4, false)) {
          i.Dispatch(Intent::Zoom{8.0f});
        } else if (ImGui::IsKeyPressed(ImGuiKey_O, false)) {
          openDialog();
        }
      } else if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
          i.Dispatch(Intent::Rotate{true});
        } else if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
          i.Dispatch(Intent::Rotate{false});
        }
      } else if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        i.Dispatch(Intent::Fit{});
      } else if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        i.Dispatch(Intent::Refresh{});
      } else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        i.Dispatch(Intent::OpenPrev{});
      } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        i.Dispatch(Intent::OpenNext{});
      }
    }

    if (ImGui::BeginPopup("##popup")) {
      if (ImGui::MenuItem("Open File ...")) {
        ImGui::CloseCurrentPopup();
        a.PostDeferredTask([this] { openDialog(); });
      }
      if (ImGui::BeginMenu("Open Recent", !m.mru.empty())) {
        std::optional<std::string> selected;
        for (const std::string& path : m.mru) {
          if (ImGui::MenuItem(path.c_str())) {
            selected = path;
          }
        }
        if (selected) {
          ImGui::CloseCurrentPopup();
          i.Dispatch(Intent::Open{*selected});
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Clear Recently Opened")) {
          i.Dispatch(Intent::ClearRecentlyOpened{});
        }
        ImGui::EndMenu();
      }
      if (ImGui::MenuItem("Open in explorer ...")) {
        i.Dispatch(Intent::OpenInExplorer{ m.latest_content_path });
      }
      ImGui::EndPopup();
    }
    if (ImGui::IsWindowFocused() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Right, false)) {
      ImGui::OpenPopup("##popup");
    }
    ImGui::End();
  }

  // thumbnail
  if (m.thumbnail_show) {
    const auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({io.DisplaySize.x, io.DisplaySize.y});
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1.0f));

    float margin_w = 0.0f;
    float margin_h = 0.0f;
    if (ImGui::Begin("##thumbnail", 0, ImGuiWindowFlags_NoDecoration)) {
      if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        if (ImGui::GetIO().MouseWheel > 0) {
          if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
            i.Dispatch(Intent::ThumbnailZoomIn{});
          }
        } else if (ImGui::GetIO().MouseWheel < 0) {
          if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
            i.Dispatch(Intent::ThumbnailZoomOut{});
          }
        }
      }

      const ImVec2 kChildMargin{0, 0};
      ImGui::SetCursorPos(kChildMargin);
      const ImVec2 kChildSize{
          io.DisplaySize.x - kChildMargin.x * 2.0f,
          io.DisplaySize.y - kChildMargin.y * 2.0f,
      };
      if (ImGui::BeginChild("#thumbnail_scroll", kChildSize, 0)) {
        const float kSpacing = 1.0f;
        const float size = std::max(128.0f, (float)m.thumbnail_size) + kSpacing;

        ImGui::SetCursorPos({0, 0});
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        const int cols = std::min(std::max(1, (int)(avail.x / size)), (int)m.cwd_entries.size());
        const int rows = (cols > 0) ? ((int)m.cwd_entries.size() + (cols - 1)) / cols : 0;
        ImGui::Dummy({cols * size, rows * size});

        margin_w = std::max(0.0f, std::floor((avail.x - (cols * size)) / 2.0f));
        margin_h = std::max(0.0f, std::floor((avail.y - (rows * size)) / 2.0f));

        const float sy = ImGui::GetScrollY();
        const float sh = avail.y;
        const int row_start = (int)(std::floor(sy / (size + kSpacing)));
        const int row_end = (int)(std::ceil(sy / (size + kSpacing)) + std::ceil(sh / (size + kSpacing)));
        const int frame = ImGui::GetFrameCount();

        for (int row = row_start; row < std::min(rows, row_end); ++row) {
          for (int col = 0; col < cols; ++col) {
            const int index = row * cols + col;
            if (index >= m.cwd_entries.size()) {
              break;
            }
            const std::string& path = m.cwd_entries[index];

            ImGui::SetCursorPos({
                col * size + margin_w,
                row * size + margin_h,
            });
            const ImVec2 prev = ImGui::GetCursorPos();
            ImGui::Dummy({size - 1, size - 1});
            const ImVec2 next = ImGui::GetCursorPos();
            const ImVec2 p0 = ImGui::GetItemRectMin();
            const ImVec2 p1 = ImGui::GetItemRectMax();
            constexpr uint32_t border = 0x30ffffff;
            constexpr uint32_t border_selected = 0xc0ffffff;
            constexpr uint32_t border_hovered = 0xa0ffffff;
            if (ImGui::IsItemHovered()) {
              ImGui::GetWindowDrawList()->AddRect({p0.x - 1, p0.y - 1}, {p1.x + 1, p1.y + 1}, border_hovered);
            } else if (path == m.content_path) {
              ImGui::GetWindowDrawList()->AddRect({p0.x - 1, p0.y - 1}, {p1.x + 1, p1.y + 1}, border_selected);
            } else {
              ImGui::GetWindowDrawList()->AddRect({p0.x - 1, p0.y - 1}, {p1.x + 1, p1.y + 1}, border);
            }
            if (ImGui::IsItemHovered()) {
              if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                i.Dispatch(Intent::Open{path});
              }
              if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                i.Dispatch(Intent::ToggleThumbnail{});
              }
            }

            // std::u8string filename =
            // std::filesystem::path(rad::to_wstring(path)).filename().u8string();
            // ImVec2 text_size = ImGui::CalcTextSize((const
            // char*)filename.c_str(), 0, false, p1.x - p0.x);
            // ImGui::PushTextWrapPos(p1.x);
            // ImGui::SetCursorPos({prev.x, prev.y + (p1 - p0).y -
            // text_size.y}); ImGui::TextWrapped("%s", filename.c_str());
            // ImGui::PopTextWrapPos();
            // ImGui::SetCursorPos(next);

            if (auto sp = i.PrefetchThumbnail(path)) {
              sp->target_x = p0.x;
              sp->target_y = p0.y;
              sp->target_width = size;
              sp->target_height = size;
              sp->last_shown_frame = ImGui::GetFrameCount();
            }
          }
        }
        ImGui::EndChild();
      }

      ImVec2 size = ImGui::CalcTextSize(m.cwd.c_str());
      ImGui::SetCursorPos({
          ImGui::GetContentRegionAvail().x / 2.0f - size.x / 2.0f,
          kChildMargin.y + margin_h - 32.0f,
      });
      ImGui::Text("%s", m.cwd.c_str());
      ImGui::End();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
  }
}

void View::renderContent() {
  for (auto content : m.contents) {
    if (!content->texture || !content->mesh) {
      continue;
    }

    rad::Render& render = world().get_or_emplace<rad::Render>(content->e);
    if (content->path != m.latest_content_path) {
      render.bypass = true;
      continue;
    }
    render.bypass = false;
    render.priority = 0;
    render.alpha = m.thumbnail_show ? 0.1f : 1.0f;
    render.mesh = content->mesh;
    render.texture = content->texture;

    float scaled_w = content->source_width * m.content_zoom;
    float scaled_h = content->source_height * m.content_zoom;
    float theta = (float)(m.content_rotate * M_PI / 180.0f);
    float scaled_rw = abs(cos(theta) * scaled_w - sin(theta) * scaled_h);
    float scaled_rh = abs(sin(theta) * scaled_w + cos(theta) * scaled_h);

    float viewport_w = (float)engine().GetWindow()->GetClientRect().width;
    float viewport_h = (float)engine().GetWindow()->GetClientRect().height;
    float translate_x = m.content_cx;
    float translate_y = m.content_cy;
    if (scaled_rw <= viewport_w && scaled_rh <= viewport_h) {
      translate_x = 0;
      translate_y = 0;
    } else {
      translate_x = std::min(translate_x, std::max(0.0f, (scaled_rw - viewport_w) / 2.0f));
      translate_y = std::min(translate_y, std::max(0.0f, (scaled_rh - viewport_h) / 2.0f));
      translate_x = std::max(translate_x, std::min(0.0f, -(scaled_rw - viewport_w) / 2.0f));
      translate_y = std::max(translate_y, std::min(0.0f, -(scaled_rh - viewport_h) / 2.0f));
    }
    if (m.content_cx != translate_x || m.content_cy != translate_y) {
      i.Dispatch(Intent::Center{
          translate_x,
          translate_y,
      });
    }
    rad::Transform& tf = world().get_or_emplace<rad::Transform>(content->e);
    tf.translate = float3(translate_x, -translate_y, 0.0f);
    tf.rotate = float3(0.0f, 0.0f, m.content_rotate);
    tf.scale = float3(scaled_w, scaled_h, 1.0f);
  };
}

void View::renderThumbnail() {
  for (auto thumbnail : m.thumbnails) {
    if (!thumbnail->texture || !thumbnail->mesh) {
      continue;
    }

    rad::Render& render = world().get_or_emplace<rad::Render>(thumbnail->e);
    render.alpha = 1.0f;
    render.priority = 1;
    render.mesh = thumbnail->mesh;
    render.texture = thumbnail->texture;

    float scale = rad::scale_to_fit(thumbnail->texture->width(),
        thumbnail->texture->height(), (int)thumbnail->target_width,
        (int)thumbnail->target_height);
    float scaled_w = thumbnail->texture->width() * scale;
    float scaled_h = thumbnail->texture->height() * scale;

    float viewport_w = (float)engine().GetWindow()->GetClientRect().width;
    float viewport_h = (float)engine().GetWindow()->GetClientRect().height;
    float translate_x = -viewport_w / 2.0f + thumbnail->target_width / 2.0f +
                        thumbnail->target_x;
    float translate_y = -viewport_h / 2.0f + thumbnail->target_height / 2.0f +
                        thumbnail->target_y;

    rad::Transform& tf = world().get_or_emplace<rad::Transform>(thumbnail->e);
    tf.translate = float3(translate_x, -translate_y, 0.0f);
    tf.scale = float3(scaled_w, scaled_h, 1.0f);
  }
}

void View::openDialog() {
  std::string path = rad::platform::ShowOpenFileDialog(engine().GetWindow()->GetHandle(), "Open File ...");
  i.Dispatch(Intent::Open{path});
}
