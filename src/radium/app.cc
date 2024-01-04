#include "app.h"

#include <base/io.h>
#include <base/algorithm.h>
#include <base/minlog.h>
#include <base/platform.h>
#include <base/text.h>
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
  const std::string settings_path = std::format("{}\\{}\\{}.json", rad::platform::getUserDirectory(), kAppName, kAppName);
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
  world().set<ecs::ContentContext>({fullpath});
  pushMRU(fullpath);
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
  auto character_ranges = io.Fonts->GetGlyphRangesJapanese();

  const std::string font_path = rad::platform::getFontDirectory() + "\\yugothr.ttc";
  io.Fonts->Clear();  // Do not use proggy as primary font

  void* icon_ttf = (void*)___src_radium_embed_ms_regular_ttf;
  int icon_ttf_size = (int)___src_radium_embed_ms_regular_ttf_len;
  {
    ImFontConfig config;
    config.OversampleH = 3;
    config.FontDataOwnedByAtlas = false;
    config.RasterizerMultiply = 1.0f;
    config.GlyphOffset.y = -1;
    config.FontNo = 1;
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        font_path.c_str(), 21.0f, &config, character_ranges);
    config.MergeMode = true;
    config.GlyphOffset.y = 4;
    config.RasterizerMultiply = 1.2f;
    config.FontNo = 0;
    io.Fonts->AddFontFromMemoryTTF(
        icon_ttf, icon_ttf_size, 27.0f, &config, icon_ranges);
  }

  {
    ImFontConfig config;
    config.OversampleH = 3;
    config.FontDataOwnedByAtlas = false;
    config.RasterizerMultiply = 1.0f;
    config.GlyphOffset.y = -1;
    config.FontNo = 1;
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        font_path.c_str(), 18.0f, &config, character_ranges);
    config.MergeMode = true;
    config.GlyphOffset.y = 4;
    config.RasterizerMultiply = 1.2f;
    config.FontNo = 0;
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
