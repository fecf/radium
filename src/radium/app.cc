#include "app.h"

#include <base/algorithm.h>
#include <base/minlog.h>
#include <base/platform.h>
#include <base/text.h>
#include <debug/live++.h>
#include <engine/engine.h>

#include <fstream>
#include <json.hpp>
#include <numeric>

#include "constants.h"
#include "embed/ms_regular.h"
#include "image_provider.h"
#include "imgui_widgets.h"
#include "material_symbols.h"
#include "resource/resource.h"
#include "service_locator.h"

namespace {

std::string getSettingsPath() {
  const std::string settings_path = std::format(
      "{}\\{}\\{}.json", rad::platform::getUserDirectory(), kAppName, kAppName);
  return settings_path;
}

}  // namespace

App& app() {
  static App instance;
  return instance;
}

int main(int argc, char** argv) {
#ifdef _DEBUG
  flecs::log::enable_colors(false);
  ::AllocConsole();
  FILE* fout = nullptr;
  ::freopen_s(&fout, "CONOUT$", "w", stdout);
#endif

  app().Start(argc, argv);
  engine().Destroy();

#ifdef _DEBUG
  ::fclose(fout);
  ::FreeConsole();
#endif
  return 0;
}

void App::Start(int argc, char** argv) {
#ifdef _DEBUG
  minlog::add_sink(minlog::sink::cout());
  minlog::add_sink(minlog::sink::debug());
#endif

  ServiceLocator::provide(new ImageProvider());
  ServiceLocator::provide(new TiledImageProvider());

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
          // camera->viewport_width =
          // (float)engine->GetWindow()->GetClientRect().width;
          // camera->viewport_height =
          // (float)engine->GetWindow()->GetClientRect().height;
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

  // Draw the first frame before show window
  if (engine().BeginFrame()) {
    engine().Draw();
    engine().EndFrame();
  }
  if (config_.window_state == rad::Window::State::Maximize) {
    engine().GetWindow()->Show(rad::Window::State::Maximize);
  } else {
    engine().GetWindow()->Show(rad::Window::State::Normal);
  }

  // start
  if (argc >= 2) {
    std::string path = argv[1];
    Open(path);
  } else {
    if (!config_.mru.empty()) {
      Open(config_.mru.front());
    }
  }

  // main loop
  while (true) {
    livepp::sync();
    if (engine().BeginFrame()) {
      processDeferredTasks();

#ifdef _DEBUG
      flecs::log::set_level(2);
      world().progress();
      flecs::log::set_level(-1);
#else
      world().progress();
#endif

      engine().Draw();
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
  // normalize
  std::string fullpath = rad::GetFullPath(path);
  if (fullpath.empty()) {
    return;
  }
  world().set<ecs::ContentContext>({path});
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
  const std::string& path = world().get<ecs::ContentContext>()->path;
  if (!path.empty()) {
    const auto& entries = world().get<ecs::FileEntryList>()->entries;
    auto it = std::find_if(entries.begin(), entries.end(),
        [&](const std::string& entry) { return entry == path; });
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
  const std::string& path = world().get<ecs::ContentContext>()->path;
  if (!path.empty()) {
    const auto& entries = world().get<ecs::FileEntryList>()->entries;
    auto it = std::find_if(entries.begin(), entries.end(),
        [&](const std::string& entry) { return entry == path; });
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

  const std::string font_path =
      rad::platform::getFontDirectory() + "\\seguivar.ttf";
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
  ImTextureID texture_id =
      reinterpret_cast<ImTextureID>(imgui_font_atlas_->id());
  io.Fonts->SetTexID(texture_id);
}

void App::initECS() {
  world().set_target_fps(60);

  // components
  world()
      .component<std::string>()
      .opaque(flecs::String)  // Opaque type that maps to string
      .serialize([](const flecs::serializer* s, const std::string* data) {
        const char* str = data->c_str();
        return s->value(flecs::String, &str);  // Forward to serializer
      })
      .assign_string([](std::string* data, const char* value) {
        *data = value;  // Assign new value to std::string
      });
  world().component<ecs::ContentContext>().member<std::string>("path");
  world()
      .component<ecs::ContentLayout>()
      .member<float>("scale")
      .member<float>("cx")
      .member<float>("cy")
      .member<float>("rotate")
      .member<bool>("fit");
  world().component<ecs::Image>().member<std::string>("path");
  world().component<ecs::FileEntryList>();
  world()
      .component<ecs::ThumbnailLayout>()
      .member<bool>("show")
      .member<int>("size")
      .member<float>("alpha");
  world().set<ecs::ContentContext>({});
  world().set<ecs::ContentLayout>({});
  world().set<ecs::ThumbnailLayout>({});
  world().set<ecs::FileEntryList>({});

  // tags
  auto Keep = world().entity();
  auto Thumbnail = world().entity();
  auto Pending = world().entity();
  auto Latest = world().entity();

  // queries
  auto query_content_images =
      world().query_builder<ecs::Image>().without(Thumbnail).build();
  auto query_thumbnail_images =
      world().query_builder<ecs::Image>().with(Thumbnail).build();

  world()
      .observer<ecs::ContentContext>("content")
      .singleton()
      .event(flecs::OnSet)
      .each([=](const ecs::ContentContext& el) {
        std::string title = std::format("{} - {}", kAppName, el.path);
        engine().GetWindow()->SetTitle(title.c_str());
        world().singleton<ecs::FileEntryList>().emit(
            ecs::FileEntryListRefreshEvent{el.path});
        world().singleton<ecs::ContentContext>().emit(
            ecs::ContentPrefetchEvent{el.path});
      });

  world().singleton<ecs::FileEntryList>().observe(
      [=](flecs::entity e, ecs::FileEntryListRefreshEvent& ev) {
        ecs::FileEntryList* el = e.get_mut<ecs::FileEntryList>();
        std::error_code ec;
        std::filesystem::path fspath = rad::ConvertToCanonicalPath(ev.path, ec);
        if (ec) return;
        std::filesystem::directory_iterator it(fspath, ec);
        if (ec) return;

        el->entries.clear();
        for (const auto& entry : it) {
          if (entry.is_regular_file()) {
            el->entries.emplace_back(rad::to_string(entry.path().u8string()));
          }
        }
        el->path = rad::to_string(fspath.make_preferred().u8string());
      });

  world().singleton<ecs::ContentContext>().observe(
      [=](ecs::ContentPrefetchEvent ev) {
        world().defer([=] {
          query_content_images.each([=](flecs::entity e, ecs::Image&) {
            if (e.has(Keep) && !e.has(Latest)) {
              e.remove(Keep);
            }
          });
        });

        auto entity = query_content_images.find(
            [&](const ecs::Image& source) { return source.path == ev.path; });
        if (entity) {
          entity.add(Keep);
          world().singleton<ecs::ContentContext>().emit(
              ecs::ContentPrefetchedEvent{ev.path});
        } else {
          world().entity().set(ecs::Image{ev.path}).add(Keep);
        }

        const ecs::FileEntryList* fel = world().get<ecs::FileEntryList>();
        auto it = std::find_if(fel->entries.begin(), fel->entries.end(),
            [&](const std::string& path) { return ev.path == path; });
        if (it != fel->entries.end()) {
          auto itp = (it == fel->entries.begin())
                         ? std::prev(fel->entries.end())
                         : std::prev(it);
          entity = query_content_images.find(
              [=](const ecs::Image& source) { return source.path == *itp; });
          if (entity) {
            entity.add(Keep);
          } else {
            world().entity().set(ecs::Image{*itp}).add(Keep);
          }

          auto itn = (it == std::prev(fel->entries.end()))
                         ? fel->entries.begin()
                         : std::next(it);
          entity = query_content_images.find(
              [&](const ecs::Image& source) { return source.path == *itn; });
          if (entity) {
            entity.add(Keep);
          } else {
            world().entity().set(ecs::Image{*itn}).add(Keep);
          }
        }
      });

  world().singleton<ecs::ContentContext>().observe(
      [=](ecs::ContentPrefetchedEvent ev) {
        const auto* content = world().get<ecs::ContentContext>();
        if (content->path != ev.path) {
          return;
        }

        world().defer_begin();
        auto e = query_content_images.find([=](const ecs::Image& image) {
          return content->path == image.path;
        });
        if (e) {
          if (const rad::Render* render = e.get<rad::Render>()) {
            query_content_images.each(
                [=](flecs::entity e, const ecs::Image& image) {
                  e.remove(Latest);
                });
            e.add(Latest);
            world()
                .singleton<ecs::ContentLayout>()
                .enqueue<ecs::ContentLayoutRotateResetEvent>();
            world()
                .singleton<ecs::ContentLayout>()
                .enqueue<ecs::ContentLayoutCenterEvent>({});
            world()
                .singleton<ecs::ContentLayout>()
                .enqueue<ecs::ContentLayoutFitEvent>();
          }
        }
        world().defer_end();
      });

  world().system<ecs::Image>("evict").without(Keep).each(
      [=](flecs::entity e, ecs::Image& el) {
        if (const ecs::ImageLifetime* lt = e.get<ecs::ImageLifetime>()) {
          if (ImGui::GetFrameCount() - lt->frame > 3) {
            e.destruct();
          }
        } else {
          e.destruct();
        }
      });

  flecs::entity content_layout = world().singleton<ecs::ContentLayout>();
  content_layout.observe([=](const ecs::ContentLayoutCenterEvent& ev) {
    world().set([=](ecs::ContentLayout& cl) {
      cl.cx = ev.cx;
      cl.cy = ev.cy;
    });
  });
  content_layout.observe<ecs::ContentLayoutFitEvent>([=] {
    world().each<ecs::Image>([=](flecs::entity e, ecs::Image& img) {
      if (e.has(Latest)) {
        if (const auto* render = e.get<rad::Render>()) {
          auto* cl = world().get_mut<ecs::ContentLayout>();
          cl->scale = rad::scale_to_fit(render->texture->array_src_width(),
              render->texture->array_src_height(),
              engine().GetWindow()->GetClientRect().width,
              engine().GetWindow()->GetClientRect().height);
          cl->cx = 0;
          cl->cy = 0;
        }
      }
    });
  });
  content_layout.observe<ecs::ContentLayoutZoomInEvent>([=] {
    world().set([=](ecs::ContentLayout& cl) {
      float scale = 1.2f;
      cl.scale *= scale;
      ImVec2 mouse =
          ImGui::GetIO().MousePos - ImGui::GetIO().DisplaySize / 2.0f;
      cl.cx = mouse.x - (mouse.x - cl.cx) * scale;
      cl.cy = mouse.y - (mouse.y - cl.cy) * scale;
    });
  });
  content_layout.observe<ecs::ContentLayoutZoomOutEvent>([=] {
    world().set([=](ecs::ContentLayout& cl) {
      float scale = 0.8f;
      cl.scale *= scale;
      ImVec2 mouse =
          ImGui::GetIO().MousePos - ImGui::GetIO().DisplaySize / 2.0f;
      cl.cx = mouse.x - (mouse.x - cl.cx) * scale;
      cl.cy = mouse.y - (mouse.y - cl.cy) * scale;
    });
  });
  content_layout.observe<ecs::ContentLayoutZoomResetEvent>(
      [=] { world().set([=](ecs::ContentLayout& cl) { cl.scale = 1.0f; }); });
  content_layout.observe([=](const ecs::ContentLayoutResizeEvent& ev) {
    world().set([=](ecs::ContentLayout& cl) { cl.scale = ev.scale; });
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
  content_layout.observe<ecs::ContentLayoutRotateResetEvent>(
      [=] { world().set([=](ecs::ContentLayout& cl) { cl.rotate = 0; }); });

  flecs::entity thumbnail_layout = world().singleton<ecs::ThumbnailLayout>();
  thumbnail_layout.observe<ecs::ThumbnailLayoutZoomInEvent>([=] {
    world().set([=](ecs::ThumbnailLayout& tl) {
      tl.size = std::min(512, tl.size + 16);
    });
  });
  thumbnail_layout.observe<ecs::ThumbnailLayoutZoomOutEvent>([=] {
    world().set([=](ecs::ThumbnailLayout& tl) {
      tl.size = std::max(16, tl.size - 16);
    });
  });
  thumbnail_layout.observe<ecs::ThumbnailLayoutToggleEvent>([=] {
    world().set([=](ecs::ThumbnailLayout& tl) { tl.show = !tl.show; });
  });

  world().observer<ecs::Image>().event<ecs::ImageLoadedEvent>().each(
      [](flecs::entity e, ecs::Image& i) {});

  world()
      .system<ecs::Image>("resource")
      .without<rad::Render>()
      .without(Pending)
      .each([=](flecs::entity e, ecs::Image& img) {
        e.add(Pending);

        bool is_thumbnail = e.has(Thumbnail);
        pool_.Post([=]() mutable {
          std::shared_ptr<rad::Texture> tex;
          if (is_thumbnail) {
            tex = ServiceLocator::get<ImageProvider>()->Request(img.path, 512);
          } else {
            tex = ServiceLocator::get<TiledImageProvider>()->Request(img.path);
          }
          if (!tex) return;

          PostDeferredTask([=]() mutable {
            if (!e.is_alive()) return;

            world().defer_begin();
            e.set(rad::Render{
                .mesh = engine().CreateMesh(),
                .alpha = 0.0,
                .texture = tex,
            });
            e.remove(Pending);
            world().defer_end();

            world().defer_begin();
            world().singleton<ecs::ContentContext>().enqueue(
                ecs::ContentPrefetchedEvent{img.path});
            world().defer_end();
          });
        });
      });

  world().system("imgui").iter([=](flecs::iter& _) {
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
        const ecs::ContentContext* c = world().get<ecs::ContentContext>();

        if (ImGui::BeginTable(
                "##table", 2, ImGuiTableFlags_SizingStretchProp)) {
          ImGui::TableNextColumn();

          ImGui::Text("Framerate");
          ImGui::TableNextColumn();
          ImGui::Text("%f", ImGui::GetIO().Framerate);
          ImGui::TableNextColumn();

          ImGui::Text("Delta");
          ImGui::TableNextColumn();
          ImGui::Text("%f", ImGui::GetIO().DeltaTime);
          ImGui::TableNextColumn();

          ImGui::Text("Content");
          ImGui::TableNextColumn();
          ImGui::Text("%s", world().to_json(c).c_str());
          ImGui::TableNextColumn();

          ImGui::Text("ContentLayout");
          ImGui::TableNextColumn();
          ImGui::Text(
              "%s", world().to_json(world().get<ecs::ContentLayout>()).c_str());
          ImGui::TableNextColumn();

          ImGui::Text("ImageSource (Content)");
          ImGui::TableNextColumn();
          query_content_images.iter(
              [=](flecs::iter& it, const ecs::Image* source) {
                for (size_t i : it) {
                  ImGui::Text(
                      "%s", world().to_json<ecs::Image>(&source[i]).c_str());
                  if (it.entity(i).has(Keep)) {
                    ImGui::SameLine();
                    ImGui::Text("%s", "[Keep]");
                  }
                  if (it.entity(i).has<rad::Render>()) {
                    ImGui::SameLine();
                    ImGui::Text("%s", "[Render]");
                  }
                }
              });
          ImGui::TableNextColumn();

          ImGui::Text("ImageSource (Thumbnail)");
          ImGui::TableNextColumn();
          query_thumbnail_images.iter(
              [=](flecs::iter& it, const ecs::Image* source) {
                for (size_t i : it) {
                  ImGui::Text(
                      "%s", world().to_json<ecs::Image>(&source[i]).c_str());
                  if (it.entity(i).has(Keep)) {
                    ImGui::SameLine();
                    ImGui::Text("%s", "[Keep]");
                  }
                  if (it.entity(i).has<rad::Render>()) {
                    ImGui::SameLine();
                    ImGui::Text("%s", "[Render]");
                  }
                }
              });
          ImGui::TableNextColumn();

          ImGui::EndTable();
        }
        ImGui::End();
      }
    }

    flecs::entity content_layout = world().singleton<ecs::ContentLayout>();
    flecs::entity thumbnail_layout = world().singleton<ecs::ThumbnailLayout>();
    flecs::entity file_entry_list = world().singleton<ecs::FileEntryList>();

    // global input event
    if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
      thumbnail_layout.enqueue<ecs::ThumbnailLayoutToggleEvent>();
    }

    // content
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    if (ImGui::Begin("##content", 0, ImGuiWindowFlags_NoDecoration)) {
      if (ImGui::IsWindowFocused()) {
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
          if (ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
            ToggleFullscreen();
          }
        } else if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
          if (ImGui::IsKeyPressed(ImGuiKey_0, false)) {
            content_layout.emit<ecs::ContentLayoutRotateResetEvent>();
            content_layout.emit<ecs::ContentLayoutFitEvent>();
            content_layout.emit(ecs::ContentLayoutCenterEvent{});
          } else if (ImGui::IsKeyPressed(ImGuiKey_1, false)) {
            content_layout.emit<ecs::ContentLayoutResizeEvent>({1.0f});
          } else if (ImGui::IsKeyPressed(ImGuiKey_2, false)) {
            content_layout.emit<ecs::ContentLayoutResizeEvent>({2.0f});
          } else if (ImGui::IsKeyPressed(ImGuiKey_3, false)) {
            content_layout.emit<ecs::ContentLayoutResizeEvent>({4.0f});
          } else if (ImGui::IsKeyPressed(ImGuiKey_4, false)) {
            content_layout.emit<ecs::ContentLayoutResizeEvent>({8.0f});
          } else if (ImGui::IsKeyPressed(ImGuiKey_O, false)) {
            OpenDialog();
          }
        } else if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
          if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
            content_layout.emit(ecs::ContentLayoutRotateEvent{true});
          } else if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
            content_layout.emit(ecs::ContentLayoutRotateEvent{false});
          }
        } else if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
          content_layout.emit<ecs::ContentLayoutFitEvent>();
        } else if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
          Refresh();
        } else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
          OpenPrev();
        } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
          OpenNext();
        }
      }

      if (ImGui::BeginPopup("##popup")) {
        if (ImGui::MenuItem("Open File ...")) {
          ImGui::CloseCurrentPopup();
          PostDeferredTask([=] { OpenDialog(); });
        }
        if (ImGui::BeginMenu("Open Recent", !mru_.empty())) {
          bool selected = false;
          for (const std::string& path : mru_) {
            if (ImGui::MenuItem(path.c_str())) {
              selected = true;
              PostDeferredTask([=] { Open(path.c_str()); });
            }
          }
          if (selected) {
            ImGui::CloseCurrentPopup();
          }
          ImGui::Separator();
          if (ImGui::MenuItem("Clear Recently Opened")) {
            mru_.clear();
          }
          ImGui::EndMenu();
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
    const ecs::ThumbnailLayout* tl =
        thumbnail_layout.get<ecs::ThumbnailLayout>();
    const ecs::FileEntryList* fel = file_entry_list.get<ecs::FileEntryList>();

    if (tl->show) {
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
              thumbnail_layout.emit<ecs::ThumbnailLayoutZoomInEvent>();
            }
          } else if (ImGui::GetIO().MouseWheel < 0) {
            if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
              thumbnail_layout.emit<ecs::ThumbnailLayoutZoomOutEvent>();
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
          const float size = std::max(16.0f, (float)tl->size) + kSpacing;

          ImGui::SetCursorPos({0, 0});
          const ImVec2 avail = ImGui::GetContentRegionAvail();

          const int cols = std::min(
              std::max(1, (int)(avail.x / size)), (int)fel->entries.size());
          assert(cols > 0);
          const int rows = ((int)fel->entries.size() + (cols - 1)) / cols;
          ImGui::Dummy({cols * size, rows * size});

          margin_w =
              std::max(0.0f, std::floor((avail.x - (cols * size)) / 2.0f));
          margin_h =
              std::max(0.0f, std::floor((avail.y - (rows * size)) / 2.0f));

          const float sy = ImGui::GetScrollY();
          const float sh = avail.y;
          const int row_start = (int)(std::floor(sy / (size + kSpacing)));
          const int row_end = (int)(std::ceil(sy / (size + kSpacing)) +
                                    std::ceil(sh / (size + kSpacing)));
          const int frame = ImGui::GetFrameCount();
          for (int row = row_start; row < std::min(rows, row_end); ++row) {
            for (int col = 0; col < cols; ++col) {
              const int index = row * cols + col;
              if (index >= fel->entries.size()) {
                break;
              }

              const std::string& path = fel->entries[index];

              const ImVec2 start = {
                  col * size + margin_w, row * size + margin_h};
              ImGui::SetCursorPos(start);

              const ImVec2 prev = ImGui::GetCursorPos();
              ImGui::Dummy({size - 1, size - 1});
              const ImVec2 p0 = ImGui::GetItemRectMin();
              const ImVec2 p1 = ImGui::GetItemRectMax();
              const ImVec2 next = ImGui::GetCursorPos();

              if (ImGui::IsItemHovered()) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                  Open(path);
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                  world()
                      .entity<ecs::ThumbnailLayout>()
                      .emit<ecs::ThumbnailLayoutToggleEvent>();
                }
              }

              const uint32_t border = 0x30ffffff;
              const uint32_t border_selected = 0xc0ffffff;
              ImGui::GetWindowDrawList()->AddRect(
                  {p0.x - 1, p0.y - 1}, {p1.x + 1, p1.y + 1}, border);

              if (path == world().get<ecs::ContentContext>()->path) {
                ImGui::GetWindowDrawList()->AddRect({p0.x - 1, p0.y - 1},
                    {p1.x + 1, p1.y + 1}, border_selected);
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

              flecs::entity e = query_thumbnail_images.find(
                  [&](const ecs::Image& t) { return path == t.path; });
              if (!e) {
                e = world().entity().add(Thumbnail);
              }
              e.set(ecs::Image{.path = path});
              e.set(ecs::ImageLifetime{.frame = frame});
              e.set(ecs::ImageLayout{
                  .x = p0.x,
                  .y = p0.y,
                  .width = size,
                  .height = size,
              });
            }
          }
          ImGui::EndChild();
        }

        ImVec2 size = ImGui::CalcTextSize(
            world().get<ecs::FileEntryList>()->path.c_str());
        ImGui::SetCursorPos({
            ImGui::GetContentRegionAvail().x / 2.0f - size.x / 2.0f,
            kChildMargin.y + margin_h - 32.0f,
        });
        ImGui::Text("%s", world().get<ecs::FileEntryList>()->path.c_str());

        ImGui::End();
      }
      ImGui::PopStyleColor();
      ImGui::PopStyleVar();
    }
  });

  world()
      .system<ecs::Image>("render_content")
      .with<rad::Render>()
      .without(Thumbnail)
      .each([=](flecs::entity e, const ecs::Image& img) {
        auto* render = e.get_mut<rad::Render>();
        if (!e.has(Latest)) {
          render->alpha = 0.0f;
          return;
        }

        const auto* thumbnail = world().get<ecs::ThumbnailLayout>();
        if (thumbnail->show) {
          render->alpha = 0.25f;
        } else {
          render->alpha = 1.0f;
        }

        const auto* content = world().get<ecs::ContentContext>();
        float image_w = (float)render->texture->array_src_width();
        float image_h = (float)render->texture->array_src_height();
        float aspect_ratio = image_w / image_h;

        float viewport_w = (float)engine().GetWindow()->GetClientRect().width;
        float viewport_h = (float)engine().GetWindow()->GetClientRect().height;
        float viewport_aspect_ratio = viewport_w / viewport_h;

        auto* content_layout = world().get_mut<ecs::ContentLayout>();
        float scaled_w = image_w * content_layout->scale;
        float scaled_h = image_h * content_layout->scale;

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

        e.set(rad::Transform{
            .translate = float3(translate_x, -translate_y, 0.0f),
            .rotate = float3(0.0f, 0.0f, content_layout->rotate),
            .scale = float3(scaled_w, scaled_h, 1.0f),
        });
      });

  world()
      .system<ecs::ImageLayout>("render_thumbnail")
      .with<ecs::Image>()
      .with<rad::Render>()
      .with(Thumbnail)
      .each([=](flecs::entity e, ecs::ImageLayout& img) {
        auto render = e.get_mut<rad::Render>();
        if (!render) return;
        render->alpha = 1.0f;
        render->priority = 1;

        float viewport_w = (float)engine().GetWindow()->GetClientRect().width;
        float viewport_h = (float)engine().GetWindow()->GetClientRect().height;
        float translate_x = -viewport_w / 2.0f + img.width / 2.0f + img.x;
        float translate_y = -viewport_h / 2.0f + img.height / 2.0f + img.y;

        float scale = rad::scale_to_fit(render->texture->width(),
            render->texture->height(), (int)img.width, (int)img.height);
        float scaled_w = render->texture->width() * scale;
        float scaled_h = render->texture->height() * scale;
        e.set(rad::Transform{
            .translate = float3(translate_x, -translate_y, 0.0f),
            .scale = float3(scaled_w, scaled_h, 1.0f),
        });
      });
}

void App::Refresh() { Open(world().get<ecs::ContentContext>()->path); }

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
  std::lock_guard lock(mutex_);
  while (deferred_tasks_.size()) {
    auto func = deferred_tasks_.front();
    func();
    deferred_tasks_.pop();
  }
}
