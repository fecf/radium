#include "app.h"

#include <fstream>
#include <numeric>

#include <base/algorithm.h>
#include <base/minlog.h>
#include <base/platform.h>
#include <base/text.h>
#include <engine/engine.h>
#include <debug/live++.h>

#include "constants.h"
#include "image_provider.h"
#include "imgui_widgets.h"
#include "material_symbols.h"

#include "embed/ms_regular.h"
#include "resource/resource.h"

#include <json.hpp>

namespace {

std::string getSettingsPath() {
  const std::string settings_path = std::format(
      "{}\\{}\\{}.json", rad::platform::getUserDirectory(), kAppName, kAppName);
  return settings_path;
}

bool isEquivalentPath(const std::string& a, const std::string& b) {
  return std::filesystem::equivalent(rad::to_wstring(a), rad::to_wstring(b));
}

}  // namespace

App& app() {
  static App instance;
  return instance;
}

int main(int argc, char** argv) {
  app().Start(argc, argv);
  engine().Destroy();
  return 0;
}

void App::Start(int argc, char** argv) {
#ifdef _DEBUG
  minlog::add_sink(minlog::sink::cout());
  minlog::add_sink(minlog::sink::debug());
#endif

  ServiceLocator::provide(new ImageProvider());

  loadSettings();

  rad::WindowConfig window_config;
  window_config.icon = IDI_ICON;
  window_config.id = kAppName;
  window_config.title = kAppName;
  window_config.x = config_.window_x;
  window_config.y = config_.window_y;
  window_config.width = config_.window_width;
  window_config.height = config_.window_height;
  if (!engine().Initialize(window_config)) {
    throw std::runtime_error("failed to init graphics engine.");
  }
  engine().GetWindow()->AddEventListener(
      [=](rad::window_event::window_event_t data) -> bool {
        auto event_resized = std::get_if<rad::window_event::Resize>(&data);
        if (event_resized) {
          // auto* camera = world().get<rad::Camera>();
          // camera->viewport_width = (float)engine->GetWindow()->GetClientRect().width;
          // camera->viewport_height = (float)engine->GetWindow()->GetClientRect().height;
        }
        auto event_dnd = std::get_if<rad::window_event::DragDrop>(&data);
        if (event_dnd) {
          if (!event_dnd->value.empty()) {
            const std::string& path = event_dnd->value.front();
            PostDeferredTask([this, path] { Open(path); });
          }
        }
        return false;
      });

  initImGui();
  initECS();

  if (config_.window_state == rad::Window::State::Maximize) {
    engine().GetWindow()->Show(rad::Window::State::Maximize);
  } else {
    engine().GetWindow()->Show(rad::Window::State::Normal);
  }

  // start
  if (!config_.mru.empty()) {
    Open(config_.mru.front());
  }

  // main loop
  while (true) {
    livepp::sync();
    if (engine().BeginFrame()) {
      flecs::log::set_level(1);
      world().progress();
      flecs::log::set_level(-1);
      engine().Draw(world());
      engine().EndFrame();
    } else {
      break;
    }
  }

  saveSettings();

  imgui_font_atlas_ = {};
  world().reset();
}

void App::Open(const std::string& path) {
  world().set<ecs::Content>({path});
  pushMRU(path);
}

void App::OpenDialog() {
  std::string path = rad::platform::ShowOpenFileDialog(
      engine().GetWindow()->GetHandle(), "Open File ...");
  if (!path.empty()) {
    Open(path);
  }
}

void App::OpenPrev() {
  const std::string& path = world().get<ecs::Content>()->path;
  if (!path.empty()) {
    const auto& entries = world().get<ecs::FileEntryList>()->entries;
    auto it = std::find_if(
        entries.begin(), entries.end(), [&](const std::string& entry) {
          return isEquivalentPath(entry, path);
        });
    if (it != entries.end()) {
      if (it == entries.begin()) {
        it = entries.end();
      }
      it = std::prev(it);
      Open(*it);
    }
  }
}

void App::OpenNext() {
  const std::string& path = world().get<ecs::Content>()->path;
  if (!path.empty()) {
    const auto& entries = world().get<ecs::FileEntryList>()->entries;
    auto it = std::find_if(
        entries.begin(), entries.end(), [&](const std::string& entry) {
          return isEquivalentPath(entry, path);
        });
    if (it != entries.end()) {
      it = std::next(it);
      if (it == entries.end()) {
        it = entries.begin();
      }
      Open(*it);
    }
  }
}

void App::OpenDirectory(const std::string& path) {
  world().set<ecs::FileEntryList>({path});
}

void App::pushMRU(const std::string& path) {
  auto it = std::remove(mru_.begin(), mru_.end(), path);
  if (it != mru_.end()) {
    mru_.erase(it, mru_.end());
  }
  mru_.push_front(path);
  while (mru_.size() > 20) {
    mru_.erase(std::prev(mru_.end()));
  }
}

bool App::loadSettings() {
  std::string path = getSettingsPath();
  try {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
      std::filesystem::path fspath(rad::to_wstring(path));
      std::filesystem::create_directories(fspath.parent_path(), ec);

      std::ofstream ofs(path);
      ofs.close();
    }

    std::ifstream ifs(path);
    config_ = nlohmann::json::parse(ifs);
    return true;
  } catch (std::exception& ex) {
    LOG_F(WARNING, "failed to parse json. (%s)", ex.what());
    config_ = Config();
    return false;
  }
}

bool App::saveSettings() {
  std::string path = getSettingsPath();
  config_.window_x = engine().GetWindow()->GetWindowRect().x;
  config_.window_y = engine().GetWindow()->GetWindowRect().y;
  config_.window_width = engine().GetWindow()->GetClientRect().width;
  config_.window_height = engine().GetWindow()->GetClientRect().height;
  config_.window_state = engine().GetWindow()->GetState();
  config_.mru.clear();
  for (const std::string& v : mru_) {
    config_.mru.push_back(v);
  }
  try {
    std::filesystem::path fspath(path);
    if (!std::filesystem::exists(fspath.parent_path())) {
      std::filesystem::create_directory(fspath.parent_path());
    }
    std::ofstream ofs(path);
    std::string str = nlohmann::json(config_).dump(4);
    ofs << str;
    return true;
  } catch (std::exception& ex) {
    LOG_F(FATAL, "failed to write json. (%s)", ex.what());
    return false;
  }
}

void App::initImGui() {
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = NULL;  // Do not create imgui.ini
  io.WantSaveIniSettings = false;

  ImGuiStyle& style = ImGui::GetStyle();
  style = ImGuiStyle();

  auto icon_ranges = GetIconRanges().data();
  // auto character_ranges = GetCharacterRanges().data();
  auto character_ranges = io.Fonts->GetGlyphRangesJapanese();

  const std::string font_path = rad::platform::getFontDirectory() + "\\seguivar.ttf";
  io.Fonts->Clear();  // Do not use default as primary font

  void* icon_ttf = (void*)___src_radium_embed_ms_regular_ttf;
  int icon_ttf_size = (int)___src_radium_embed_ms_regular_ttf_len;
  {
    ImFontConfig config;
    config.OversampleH = 3;
    config.FontDataOwnedByAtlas = false;
    config.RasterizerMultiply = 1.0f;
    config.GlyphOffset.y = -1;
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        font_path.c_str(), 21.0f, &config, character_ranges);
    config.MergeMode = true;
    config.GlyphOffset.y = 4;
    config.RasterizerMultiply = 1.2f;
    io.Fonts->AddFontFromMemoryTTF(
        icon_ttf, icon_ttf_size, 27.0f, &config, icon_ranges);
  }
  {
    ImFontConfig config;
    config.OversampleH = 3;
    config.FontDataOwnedByAtlas = false;
    config.RasterizerMultiply = 1.0f;
    config.GlyphOffset.y = -1;
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        font_path.c_str(), 18.0f, &config, character_ranges);
    config.MergeMode = true;
    config.GlyphOffset.y = 4;
    config.RasterizerMultiply = 1.2f;
    io.Fonts->AddFontFromMemoryTTF(
        icon_ttf, icon_ttf_size, 24.0f, &config, icon_ranges);
  }
  io.Fonts->AddFontDefault();  // 2 = Proggy
  io.Fonts->Build();
  font.normal = io.Fonts->Fonts[0];
  font.small = io.Fonts->Fonts[1];
  font.proggy = io.Fonts->Fonts[2];

  // upload font
  int width = 0, height = 0, bpp = 0;
  unsigned char* pixels = NULL;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bpp);
  assert((pixels != NULL) && (width > 0) && (height > 0) && (bpp == 4));

  auto image = std::make_shared<rad::Image>(width, height, width * bpp,
      rad::ImageFormat::RGBA8, 4, rad::ColorSpace::sRGB, pixels);
  imgui_font_atlas_ = engine().CreateTexture(image);
  ImTextureID texture_id = reinterpret_cast<ImTextureID>(imgui_font_atlas_->id());
  io.Fonts->SetTexID(texture_id);
}

void App::initECS() {
  world().set_target_fps(60);

  // components
  world().component<std::string>()
        .opaque(flecs::String) // Opaque type that maps to string
            .serialize([](const flecs::serializer *s, const std::string *data) {
                const char *str = data->c_str();
                return s->value(flecs::String, &str); // Forward to serializer
            })
            .assign_string([](std::string* data, const char *value) {
                *data = value; // Assign new value to std::string
            });
  world().component<ecs::Content>().member<std::string>("path");
  world()
      .component<ecs::ContentLayout>()
      .member<float>("scale")
      .member<float>("cx")
      .member<float>("cy")
      .member<float>("rotate")
      .member<bool>("fit");
  world().component<ecs::ImageSource>().member<std::string>("path");
  world().component<ecs::Explorer>();
  world().component<ecs::FileEntryList>();
  world().set<ecs::Content>({});
  world().set<ecs::ContentLayout>({});
  world().set<ecs::Explorer>({});
  world().set<ecs::FileEntryList>({});

  // tags
  auto Active = world().entity().add(flecs::Exclusive);
  auto Keep = world().entity();

  world()
      .system("input")
      .kind(flecs::PreUpdate)
      .iter([=](flecs::iter&) { 
    flecs::entity content_layout = world().singleton<ecs::ContentLayout>();
    if (ImGui::GetIO().MouseWheel > 0) {
      if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
        content_layout.emit<ecs::ContentLayoutZoomInEvent>();
      } else {
        OpenPrev();
      }
    } else if (ImGui::GetIO().MouseWheel < 0) {
      if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
        content_layout.emit<ecs::ContentLayoutZoomOutEvent>();
      } else {
        OpenNext();
      }
    } else if (ImGui::GetIO().MouseDoubleClicked[0]) {
      OpenDialog();
    }

    static ImVec2 drag_start_offset{};
    static ImVec2 drag_start_mouse_pos{};
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
      auto* cl = world().get<ecs::ContentLayout>();
      drag_start_offset = {
          cl->cx,
          cl->cy,
      };
      drag_start_mouse_pos = ImGui::GetIO().MousePos;
    }
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
      const float& dx = ImGui::GetIO().MouseDelta.x;
      const float& dy = ImGui::GetIO().MouseDelta.y;
      content_layout.emit(ecs::ContentLayoutCenterEvent{
          (drag_start_offset.x +
              (ImGui::GetIO().MousePos.x - drag_start_mouse_pos.x)),
          (drag_start_offset.y +
              (ImGui::GetIO().MousePos.y - drag_start_mouse_pos.y))});
    }

    if (ImGui::IsKeyDown(ImGuiKey_LeftAlt)) {
      if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        ToggleFullscreen();
      }
    } else if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
      if (ImGui::IsKeyPressed(ImGuiKey_0)) {
        content_layout.emit<ecs::ContentLayoutRotateResetEvent>();
        content_layout.emit<ecs::ContentLayoutFitEvent>();
        content_layout.emit(ecs::ContentLayoutCenterEvent{});
      } else if (ImGui::IsKeyPressed(ImGuiKey_1)) {
        content_layout.emit<ecs::ContentLayoutResizeEvent>({1.0f});
      } else if (ImGui::IsKeyPressed(ImGuiKey_2)) {
        content_layout.emit<ecs::ContentLayoutResizeEvent>({2.0f});
      } else if (ImGui::IsKeyPressed(ImGuiKey_3)) {
        content_layout.emit<ecs::ContentLayoutResizeEvent>({4.0f});
      } else if (ImGui::IsKeyPressed(ImGuiKey_4)) {
        content_layout.emit<ecs::ContentLayoutResizeEvent>({8.0f});
      } else if (ImGui::IsKeyPressed(ImGuiKey_O)) {
        OpenDialog();
      }
    } else if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
      if (ImGui::IsKeyPressed(ImGuiKey_R)) {
        content_layout.emit(ecs::ContentLayoutRotateEvent{true});
      } else if (ImGui::IsKeyPressed(ImGuiKey_W)) {
        content_layout.emit(ecs::ContentLayoutRotateEvent{false});
      }
    } else if (ImGui::IsKeyPressed(ImGuiKey_F)) {
      content_layout.emit<ecs::ContentLayoutFitEvent>();
    } else if (ImGui::IsKeyPressed(ImGuiKey_F5)) {
      Refresh();
    } else if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
      ToggleExplorer();
    } else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
      OpenPrev();
    } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
      OpenNext();
    }
  });

  world()
      .observer<ecs::Content>("content")
      .singleton()
      .event(flecs::OnSet)
      .each([=](const ecs::Content& el) {
        world().singleton<ecs::FileEntryList>().emit(ecs::FileEntryListRefreshEvent{el.path});
        world().singleton<ecs::Content>().emit<ecs::ImageSourceRefreshEvent>();
        world().singleton<ecs::ContentLayout>().emit<ecs::ContentLayoutRotateResetEvent>();
        world().singleton<ecs::ContentLayout>().emit<ecs::ContentLayoutFitEvent>();
        world().singleton<ecs::ContentLayout>().emit<ecs::ContentLayoutCenterEvent>({});
      });

  world()
      .singleton<ecs::FileEntryList>()
      .observe([=](flecs::entity e, ecs::FileEntryListRefreshEvent& ev) {
        ecs::FileEntryList* el = e.get_mut<ecs::FileEntryList>();
        std::error_code ec;
        std::filesystem::path fspath = rad::ConvertToCanonicalPath(ev.path, ec);
        if (ec) return;
        std::filesystem::directory_iterator it(fspath, ec);
        if (ec) return;
        for (const auto& entry : it) {
          if (entry.is_regular_file()) {
            el->entries.emplace_back(rad::to_string(entry.path().u8string()));
          }
        }
        el->path = rad::to_string(fspath.make_preferred().u8string());
      });

  world().singleton<ecs::Content>().observe<ecs::ImageSourceRefreshEvent>(
      [=](flecs::entity src) {
    const ecs::Content* el = src.get<ecs::Content>();

    std::string title = std::format("{} - {}", kAppName, el->path);
    engine().GetWindow()->SetTitle(title.c_str());

    world().query<ecs::ImageSource>().iter([=](flecs::iter& it) {
      for (size_t i : it) {
        if (it.entity(i).has(Keep)) {
          it.entity(i).remove(Keep);
        }
      }
    });

    auto upsert = [=](const std::string& path) {
      auto entity = world().filter<ecs::ImageSource>().find(
          [=](const ecs::ImageSource& source) {
            return isEquivalentPath(source.path, path);
          });
      if (entity) {
        return entity.add(Keep);
      } else {
        return world().entity().set(ecs::ImageSource{path}).add(Keep);
      }
    };
    auto entity = upsert(el->path);
    world().singleton<ecs::Content>().add(Active, entity);

    const ecs::FileEntryList* fel = world().get<ecs::FileEntryList>();
    auto it = std::find_if(fel->entries.begin(), fel->entries.end(),
        [=](const std::string& path) {
          return isEquivalentPath(el->path, path);
        });
    if (it != fel->entries.end()) {
      auto itp = (it == fel->entries.begin())
                     ? std::prev(fel->entries.end())
                     : std::prev(it);
      if (itp != it) {
        upsert(*itp);
      }

      auto itn = (it == std::prev(fel->entries.end()))
                     ? fel->entries.begin()
                     : std::next(it);
      if (itn != it && itn != itp) {
        upsert(*itn);
      }
    }
  });

  world()
      .observer<ecs::ImageSource>("evict")
      .without(Keep)
      .event(flecs::OnSet)
      .iter([=](flecs::iter& e, const ecs::ImageSource* el) {
        for (size_t i : e) {
          e.entity(i).destruct();
        }
      });

  flecs::entity content_layout = world().singleton<ecs::ContentLayout>();
  content_layout.observe([=](const ecs::ContentLayoutCenterEvent& ev) {
    world().set([=](ecs::ContentLayout& cl) {
      cl.cx = ev.cx;
      cl.cy = ev.cy;
    });
  });
  content_layout.observe<ecs::ContentLayoutFitEvent>([=]{
    world().set([=](ecs::ContentLayout& cl) {
      cl.fit_flag = true;
    });
  });
  content_layout.observe<ecs::ContentLayoutZoomInEvent>([=]{
    world().set([=](ecs::ContentLayout& cl) { 
      float scale = 1.2f;
      cl.scale *= scale;
      ImVec2 mouse = ImGui::GetIO().MousePos - ImGui::GetIO().DisplaySize / 2.0f;
      cl.cx = mouse.x - (mouse.x - cl.cx) * scale;
      cl.cy = mouse.y - (mouse.y - cl.cy) * scale;
    });
  });
  content_layout.observe<ecs::ContentLayoutZoomOutEvent>([=]{
    world().set([=](ecs::ContentLayout& cl) { 
      float scale = 0.8f;
      cl.scale *= scale;
      ImVec2 mouse = ImGui::GetIO().MousePos - ImGui::GetIO().DisplaySize / 2.0f;
      cl.cx = mouse.x - (mouse.x - cl.cx) * scale;
      cl.cy = mouse.y - (mouse.y - cl.cy) * scale;
    });
  });
  content_layout.observe<ecs::ContentLayoutZoomResetEvent>([=]{
    world().set([=](ecs::ContentLayout& cl) { 
      cl.scale = 1.0f; 
    });
  });
  content_layout.observe([=](const ecs::ContentLayoutResizeEvent& ev) {
    world().set([=](ecs::ContentLayout& cl) {
      cl.scale = ev.scale;
    });
  });
  content_layout.observe([=](const ecs::ContentLayoutRotateEvent& ev) {
    world().set([=](ecs::ContentLayout& cl) {
      cl.rotate += ev.clockwise ? -90 : 90;
      if (cl.rotate < 0) {
        cl.rotate += 360;
      } else if (cl.rotate >= 360) {
        cl.rotate -= 360;
      }
    });
  });
  content_layout.observe<ecs::ContentLayoutRotateResetEvent>([=] {
    world().set([=](ecs::ContentLayout& cl) { cl.rotate = 0; });
  });

  world()
      .system<ecs::ImageSource>("render")
      .kind(flecs::PostUpdate)
      .iter([=](flecs::iter& it, ecs::ImageSource* el) {
        for (size_t i : it) {
          auto entity = it.entity(i).mut(it);
          if (!entity.has<rad::Render>()) {
            if (auto ip = ServiceLocator::get<ImageProvider>()) {
              if (auto texture = ip->Request(el[i].path)) {
                entity.set(rad::Render{
                    .mesh = engine().CreateMesh(),
                    .alpha = 0.0,
                    .color = {},
                    .texture = texture,
                });
              }
            }
          } else {
            float alpha = world().singleton<ecs::Content>().has(Active, entity) ? 1.0f : 0.0f;
            rad::Render* render = entity.get_mut<rad::Render>();
            render->alpha = alpha;

            float image_w = (float)render->texture->array_src_width();
            float image_h = (float)render->texture->array_src_height();
            float aspect_ratio = image_w / image_h;

            float viewport_w = (float)engine().GetWindow()->GetClientRect().width;
            float viewport_h = (float)engine().GetWindow()->GetClientRect().height;
            float viewport_aspect_ratio = viewport_w / viewport_h;

            ecs::ContentLayout* content_layout = world().get_mut<ecs::ContentLayout>();
            float scale = content_layout->scale;
            float scaled_w = image_w * content_layout->scale;
            float scaled_h = image_h * content_layout->scale;
            if (content_layout->fit_flag) {
              content_layout->fit_flag = false;
              if (aspect_ratio > viewport_aspect_ratio) {
                content_layout->scale = viewport_w / image_w;
              } else {
                content_layout->scale = viewport_h / image_h;
              }
              content_layout->cx = 0;
              content_layout->cy = 0;
            }

            float theta = (float)(-content_layout->rotate * M_PI / 180.0f);
            float translate_x = content_layout->cx;
            float translate_y = content_layout->cy;
            float scaled_rw = abs(cos(theta) * scaled_w - sin(theta) * scaled_h);
            float scaled_rh = abs(sin(theta) * scaled_w + cos(theta) * scaled_h);
            if (scaled_rw <= viewport_w && scaled_rh <= viewport_h) {
              translate_x = 0;
              translate_y = 0;
            } else {
              translate_x = std::min(translate_x, std::max(0.0f, (scaled_rw - viewport_w) / 2.0f));
              translate_y = std::min(translate_y, std::max(0.0f, (scaled_rh - viewport_h) / 2.0f));
              translate_x = std::max(translate_x, std::min(0.0f, -(scaled_rw - viewport_w) / 2.0f));
              translate_y = std::max(translate_y, std::min(0.0f, -(scaled_rh - viewport_h) / 2.0f));
            }
            content_layout->cx = translate_x;
            content_layout->cy = translate_y;

            entity.set(rad::Transform{
                .translate = float3(translate_x, -translate_y, 0.0f),
                .rotate = float3(0.0f, 0.0f, content_layout->rotate),
                .scale = float3(scaled_w, scaled_h, 1.0f),
            });
          }
        }
      });

  world().system("debug").iter([=](flecs::iter& _) {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::GetIO().MouseDragThreshold = 6.0;
    style.FrameBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.PopupBorderSize = 0.0f;
    style.CellPadding = ImVec2(4.0f, 1.0f);
    style.ItemSpacing = ImVec2();
    style.FrameRounding = 0.0f;
    style.FramePadding = ImVec2(4.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2();
    style.WindowPadding = ImVec2(8, 8);
    style.WindowRounding = 0.0f;
    style.DisabledAlpha = 0.5f;

    ImVec4 kText = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    ImVec4 kAccent = ImVec4(0.75f, 1.0f, 0.44f, 1.0f);
    ImVec4 kAccentDark = ImVec4(0.15f, 0.4f, 0.2f, 1.0f);
    ImVec4 kHeader = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    ImVec4 kHeaderHovered = ImVec4(0.85f, 1.0f, 0.54f, 1.0f);
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

    // reflection
    ImGui::SetNextWindowPos({16, 16});
    ImGui::SetNextWindowSize({800, 800});
    if (ImGui::Begin("debug", 0, ImGuiWindowFlags_NoDecoration)) {
      const ecs::Content* c = world().get<ecs::Content>();

      ImGui::Text("FPS %f", ImGui::GetIO().Framerate);
      ImGui::Text("MouseWheel %f", ImGui::GetIO().MouseWheel);

      ImGui::Text("Content\n%s", world().to_json(c).c_str());
      ImGui::Text("ContentLayout\n%s",
          world().to_json(world().get<ecs::ContentLayout>()).c_str());

      ImGui::Text("ImageSource");
      world().filter<ecs::ImageSource>().iter(
          [=](flecs::iter& it, const ecs::ImageSource* source) {
            for (size_t i : it) {
              ImGui::Text(
                  "%s", world().to_json<ecs::ImageSource>(&source[i]).c_str());
              if (it.entity(i).has(Keep)) {
                ImGui::SameLine();
                ImGui::Text("%s", "[Keep]");
              }
              if (it.entity(i).has<rad::Render>()) {
                ImGui::SameLine();
                ImGui::Text("%s", "[Render]");
              }
              if (world().singleton<ecs::Content>().has(Active, it.entity(i))) {
                ImGui::SameLine();
                ImGui::Text("%s", "[Active]");
              }
            }
          });

      ImGui::End();
    }
  });
}

void App::Refresh() {
}

void App::ToggleExplorer() {
}

void App::ToggleFullscreen() {
  PostDeferredTask([this] {
    auto* window = engine().GetWindow();
    if (window->IsBorderlessFullscreen() ||
        window->GetState() == rad::Window::State::Maximize) {
      window->ExitFullscreen();
    } else {
      window->EnterFullscreen(true);
    }
  });
}

void App::PostDeferredTask(std::function<void()> func) {
  std::lock_guard lock(mutex_);
  deferred_tasks_.push(std::move(func));
}

void App::processDeferredTasks() {
  while (deferred_tasks_.size()) {
    auto func = deferred_tasks_.front();
    func();
    deferred_tasks_.pop();
  }
}

