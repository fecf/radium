#include "app.h"

#include <fstream>
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
      .interval(1.0f / 60.0f)
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

    if (ImGui::IsKeyDown(ImGuiKey_LeftAlt)) {
      if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        ToggleFullscreen();
      }
    } else if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
      if (ImGui::IsKeyPressed(ImGuiKey_0)) {
        content_layout.emit<ecs::ContentLayoutFitEvent>();
        content_layout.emit<ecs::ContentLayoutCenterEvent>();
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
        world().singleton<ecs::ContentLayout>().emit<ecs::ContentLayoutZoomResetEvent>();
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
  content_layout.observe<ecs::ContentLayoutZoomInEvent>([=]{
    world().set([=](ecs::ContentLayout& cl) { cl.scale *= 1.2f; });
  });
  content_layout.observe<ecs::ContentLayoutZoomOutEvent>([=]{
    world().set([=](ecs::ContentLayout& cl) { cl.scale *= 0.8f; });
  });
  content_layout.observe<ecs::ContentLayoutZoomResetEvent>([=]{
    world().set([=](ecs::ContentLayout& cl) { cl.scale = 1.0f; });
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

            float image_x = (float)render->texture->array_src_width();
            float image_y = (float)render->texture->array_src_height();
            float aspect_ratio = image_x / image_y;

            float window_x = (float)engine().GetWindow()->GetClientRect().width;
            float window_y = (float)engine().GetWindow()->GetClientRect().height;
            float window_aspect_ratio = window_x / window_y;

            const auto* content_layout = world().get<ecs::ContentLayout>();
            float scale_x = image_x / window_x * content_layout->scale;
            float scale_y = image_y / window_y * content_layout->scale;

            entity.set(rad::Transform{
                .translate =
                    float3(content_layout->cx, -content_layout->cy, 0.0f),
                .rotate = float3(0, 0, 0),
                .scale =
                    float3(scale_x, scale_y, 1.0f),
            });
          }
        }
      });

  world().system("debug").iter([=](flecs::iter& _) {
    ImGuiStyle& style = ImGui::GetStyle();
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

      ImGui::Text("Content");
      ImGui::Text("%s", world().to_json(c).c_str());

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


/*


  /*
  void App::Open(int offset) {
  if (!ctx_.io.dir.count()) {
    return;
  }

  int target = rad::wrap(ctx_.io.ordinal, offset, 0, (int)(ctx_.io.dir.count() - 1));
  const auto& entry = ctx_.io.dir.entries().at(target);
  OpenFile(entry);
}

  ctx_.io.path = entry;

  const std::string path = entry.path();
  std::filesystem::path fspath(rad::to_wstring(path));
  std::error_code ec;
  if (!std::filesystem::is_regular_file(fspath, ec) || ec) {
    DLOG_F("specified path is a not regular file.");
    return;
  }

  const std::string parent = rad::to_string(fspath.parent_path().u8string());
  if (ctx_.io.dir.path() != parent) {
    ctx_.io.dir = rad::DirectoryList(path, false, true);
    Sort(ctx_.io.sort_type, ctx_.io.sort_desc);
    DLOG_F("changed directory to %s.", parent.c_str());
  }

  UpdateOrdinal();

  // do request
  auto callback = [=](std::shared_ptr<ImageCache> cache) {
    PostDeferredTask([this, cache] {
      if (cache->key() == ctx_.io.path.path()) {
        ctx_.viewer.source = cache;
        if (ctx_.prefs.fitted) {
          ctx_.viewer.fit_once = true;
          ctx_.viewer.always_fit = true;
        } else {
          ctx_.viewer.zoom = 1.0f;
          ctx_.viewer.center_once = true;
          ctx_.viewer.always_fit = false;
        }
      }
    });
  };

  std::shared_ptr<ImageCache> cache = ctx_.source_cache->Request(path, callback);
  if (cache && cache->status() == ImageCache::Status::Completed) {
    ctx_.io.cached = true;
    callback(cache);
  } else {
    ctx_.io.cached = false;
  }
  
  // prefetch images
  const auto& entries = ctx_.io.dir.entries();
  int prev_index = rad::wrap(ctx_.io.ordinal, -1, 0, (int)ctx_.io.dir.count() - 1);
  if ((prev_index != ctx_.io.ordinal) && (prev_index < (int)entries.size())) {
    std::string prev_path = entries[prev_index].path();
    ctx_.source_cache->Request(prev_path, callback);
  }

  int next_index = rad::wrap(ctx_.io.ordinal, +1, 0, (int)ctx_.io.dir.count() - 1);
  if ((next_index != ctx_.io.ordinal) && (next_index < (int)entries.size())) {
    std::string next_path = entries[next_index].path();
    ctx_.source_cache->Request(next_path, callback);
  }
*/

/*

  for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
    const std::shared_ptr<ILayer> layer = *it;
    const auto names = layer->GetNames();
    for (auto it2 = names.rbegin(); it2 != names.rend(); ++it2) {
      ImGuiWindow* window = ImGui::FindWindowByName(*it2);
      if (window != NULL) {
        ImGui::BringWindowToDisplayBack(window);
      }
    }
  }


  static ImVec2 last_pos = ImGui::GetMousePos();
  static double last_time = 0.0;
  static double idle_time = 0.0;
  ImVec2 pos = ImGui::GetMousePos();
  if (ImGui::IsAnyMouseDown() || pos.x != last_pos.x || pos.y != last_pos.y) {
    last_pos = pos;
    last_time = ImGui::GetTime();
  }
  idle_time = ImGui::GetTime() - last_time;

*/

/*

void App::ViewerZoomUpDown(bool up) {
  static std::vector<float> list{0.05f, 0.1f, 0.25f, 0.33f, 0.5f, 0.67f, 0.75f,
      0.8f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f, 1.75f, 2.0f, 2.5f, 3.0f, 4.0f, 5.0f,
      10.0f};

  float zoom = ctx_.viewer.zoom;
  auto it = std::upper_bound(list.begin(), list.end(), zoom);
  if (up) {
    if (it == list.end()) {
      it = std::prev(list.end());
    }
  } else {
    if (it == list.end()) {
      it = std::prev(list.end());
    } else if (it != list.begin() && *it != zoom) {
      it = std::prev(it);
    }
  }
  zoom = *it;

  ctx_.viewer.zoom = zoom;
  ctx_.viewer.always_fit = false;
}

*/

/*

void App::Sort(rad::SortType type) {
  ctx_.io.dir.sort(type, desc);
  UpdateOrdinal();
  ctx_.io.sort_type = type;
  ctx_.io.sort_desc = desc;
  if (ctx_.window.show_explorer) {
    ctx_.explorer.thumbnail_scroll_once_flag = true;
  }
}

*/

/*

void App::ViewerRefresh() {
  ecs::Image* current = world().entity("current_image").get<Image>();
  if (current && !current->path.empty()) {
    world().entity("pending_image").set<ecs::Image>(ecs::Image{current->path});
    world().entity("file_entries").set<ecs::FileEntryList>;
  }
}

*/

/*

bool App::UpdateOrdinal() {
  bool found = false;
  const auto& entries = ctx_.io.dir.entries();
  for (int i = 0; i < (int)entries.size(); ++i) {
    if (entries[i].name() == ctx_.io.path.name()) {
      ctx_.io.ordinal = i;
      found = true;
      break;
    }
  }
  if (found) {
    return true;
  } else {
    ctx_.io.ordinal = 0;
    return false;
  }
}

*/

/*



    auto texture = world().entity("current_image").get<ecs::Image>()->texture;
    if (texture) {
      int width = texture->array_src_width();
      int height = texture->array_src_height();
      if (width && height) {
        PostDeferredTask([this, window, zoom, width, height] {
          ctx_.viewer.zoom = zoom;
          window->Resize((int)(width * zoom), (int)(height * zoom));
        });
      }
    }
*/

/*

    const ImVec2 viewport = ctx.viewer.viewport.GetSize();
    if (!viewport.x || !viewport.y) {
      return;
    }

    const ImVec2 img_size{
        (float)texture->array_src_width(),
        (float)texture->array_src_height(),
    };

    // force fitting
    bool force_fit = ctx.viewer.always_fit || ctx.viewer.fit_once;
    if (force_fit) {
      ctx.viewer.center = {
          img_size.x / 2.0f,
          img_size.y / 2.0f,
      };

      const float sw = viewport.x / img_size.x;
      const float sh = viewport.y / img_size.y;
      if (sh > sw) {
        ctx.viewer.zoom = sw;
      } else {
        ctx.viewer.zoom = sh;
      }
      if (ctx.viewer.fit_once) {
        ctx.viewer.fit_once = false;
      }
    }

    // force centering
    if (ctx.viewer.center_once) {
      ctx.viewer.center_once = false;
      ctx.viewer.center = {
          img_size.x / 2.0f,
          img_size.y / 2.0f,
      };
    }

    // cursor position
    const float zoom = ctx.viewer.zoom;
    const ImVec2 center = ctx.viewer.center;
    const ImVec2 cursor_center = {
        -(img_size.x * zoom * 0.5f) + (viewport.x / 2.0f),
        -(img_size.y * zoom * 0.5f) + (viewport.y / 2.0f),
    };
    const ImVec2 cursor = {
        cursor_center.x - ((center.x * zoom) - (img_size.x * zoom / 2.0f)),
        cursor_center.y - ((center.y * zoom) - (img_size.y * zoom / 2.0f)),
    };

    const ImVec2 mouse = ImGui::GetMousePos();
    ctx.viewer.mouse_pos = mouse;

    const ImVec2 start = ImGui::GetCursorPos();
    ImGui::SetCursorPos(cursor);
    ImGui::Dummy({img_size.x * zoom, img_size.y * zoom});

    const ImVec2 img_min = ImGui::GetItemRectMin() - start;
    const ImVec2 img_max = ImGui::GetItemRectMax() - start;
    ctx.viewer.rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

    const ImVec2& vp_min = ctx.viewer.viewport.Min;
    const ImVec2& vp_max = ctx.viewer.viewport.Max;
    const ImVec2 vp_uv_pos = {
        (std::max(vp_min.x, img_min.x) - img_min.x) / (img_max.x - img_min.x),
        (std::max(vp_min.y, img_min.y) - img_min.y) / (img_max.y - img_min.y),
    };
    const ImVec2 vp_uv_size = {
        ((std::min(vp_max.x, img_max.x)) - std::max(vp_min.x, img_min.x)) /
            (img_max.x - img_min.x),
        ((std::min(vp_max.y, img_max.y)) - std::max(vp_min.y, img_min.y)) /
            (img_max.y - img_min.y),
    };
    ctx.viewer.viewport_uv = ImRect(vp_uv_pos, vp_uv_pos + vp_uv_size);

    const ImVec2 mouse_pos = {
        mouse.x - img_min.x,
        mouse.y - img_min.y,
    };
    const ImVec2 mouse_pos_r = {
        std::max(0.0f, std::min(img_size.x - 1.0f, mouse_pos.x / zoom)),
        std::max(0.0f, std::min(img_size.y - 1.0f, mouse_pos.y / zoom)),
    };
    ctx.viewer.viewport_mouse_pos = mouse_pos;
    ctx.viewer.viewport_mouse_pos_r = mouse_pos_r;

*/
