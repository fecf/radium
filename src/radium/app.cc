#include "app.h"

#include <base/io.h>
#include <base/algorithm.h>
#include <base/minlog.h>
#include <base/platform.h>
#include <base/text.h>
#include <engine/engine.h>

#include <fstream>
#include <format>
#include <numeric>

#include <json.hpp>


#include "constants.h"
#include "image_provider.h"
#include "material_symbols.h"
#include "service_locator.h"

#include "embed/ms_regular.h"
#include "resource/resource.h"

namespace {

std::string getSettingsPath() {
  const std::string settings_path = std::format("{}\\{}\\{}.json", rad::platform::getUserDirectory(), kAppName, kAppName);
  return settings_path;
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _DEBUG
  ::AllocConsole();
  FILE* fout = nullptr;
  ::freopen_s(&fout, "CONOUT$", "w", stdout);

  minlog::add_sink(minlog::sink::cout());
  minlog::add_sink(minlog::sink::debug());
#endif

  {
    App app;
    app.Start(argc, argv);
  }

#ifdef _DEBUG
  if (fout != nullptr) {
    ::fclose(fout);
  }
  ::FreeConsole();
#endif
  return 0;
}

App::App()
    : i(Intent(*this, m)),
      v(View(*this, m, i)),
      pool_content(1),
      pool_thumbnail(std::thread::hardware_concurrency()) {}

void App::Start(int argc, char** argv) {
  loadSettings();

  ImGui::CreateContext();
  setupImGui();

  // Parallelising engine initialisation and font builds
  {
    auto initialize_service_locator_task = std::async(std::launch::async, [this] {
      ServiceLocator::Provide(new CachedImageProvider());
      ServiceLocator::Provide(new TiledImageProvider());
    });
    auto build_imgui_fonts_task =
        std::async(std::launch::async, [this] { buildImGuiFonts(); });

    rad::WindowConfig window_config;
    window_config.icon = IDI_ICON;
    window_config.id = kAppName;
    window_config.title = kAppName;
    window_config.x = config_.window_x;
    window_config.y = config_.window_y;
    window_config.width = config_.window_width;
    window_config.height = config_.window_height;
    if (!engine().Initialize(window_config)) {
      throw std::runtime_error("failed to rad::Engine::Initialize().");
    }
    engine().GetWindow()->AddEventListener(
        [=](rad::window_event::window_event_t data) -> bool {
          if (auto resize = std::get_if<rad::window_event::Resize>(&data)) {
            PostDeferredTask([this] { i.Dispatch(Intent::Fit{}); });
          }
          if (auto event_dnd =
                  std::get_if<rad::window_event::DragDrop>(&data)) {
            if (!event_dnd->value.empty()) {
              const std::string& path = event_dnd->value.front();
              PostDeferredTask(
                  [this, path] { i.Dispatch(Intent::Open{path}); });
            }
          }
          return false;
        });

    initialize_service_locator_task.wait();
    build_imgui_fonts_task.wait();

    uploadImGuiFonts();
  }

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
    i.Dispatch(Intent::Open{path});
  } else {
    if (!config_.mru.empty()) {
      i.Dispatch(Intent::Open{config_.mru.front()});
    }
  }

  // main loop
  while (true) {
    if (!engine().BeginFrame()) {
      break;
    }
    processDeferredTasks();
    v.Update();
    engine().Draw();
    engine().EndFrame();
  }

  saveSettings();

  // release all resources
  imgui_font_atlas_.reset();
  ServiceLocator::Clear();
  m.contents.clear();
  m.thumbnails.clear();
  engine().Destroy();
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
    for (const std::string& v : config_.mru) {
      m.mru.push_back(v);
    }
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
  for (const std::string& v : m.mru) {
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

void App::setupImGui() {
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = NULL;  // Do not create imgui.ini
  io.WantSaveIniSettings = false;
}

void App::buildImGuiFonts() {
  ImGuiIO& io = ImGui::GetIO();
  const std::string font_path = rad::platform::getFontDirectory() + "\\yugothr.ttc";
  io.Fonts->Clear();  // Do not use proggy as primary font

  auto icon_ranges = GetIconRanges().data();
  auto character_ranges = io.Fonts->GetGlyphRangesJapanese();

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

  {
    ImFontConfig config;
    config.OversampleH = 3;
    config.FontDataOwnedByAtlas = false;
    config.RasterizerMultiply = 1.0f;
    config.GlyphOffset.y = -1;
    config.FontNo = 1;
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        font_path.c_str(), 32.0f, &config, character_ranges);
    config.MergeMode = true;
    config.GlyphOffset.y = 6;
    config.RasterizerMultiply = 1.2f;
    config.FontNo = 0;
    io.Fonts->AddFontFromMemoryTTF(
        icon_ttf, icon_ttf_size, 38.0f, &config, icon_ranges);
  }

  io.Fonts->AddFontDefault();  // Proggy

  io.Fonts->Build();
}

void App::uploadImGuiFonts() {
  int width = 0, height = 0, bpp = 0;
  unsigned char* pixels = NULL;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bpp);
  assert((pixels != NULL) && (width > 0) && (height > 0) && (bpp == 4));

  auto image = std::make_shared<rad::Image>(width, height, width * bpp,
      rad::ImageFormat::RGBA8, 4, rad::ColorSpace::sRGB, pixels);
  imgui_font_atlas_ = engine().CreateTexture(image);
  ImTextureID texture_id =
      reinterpret_cast<ImTextureID>(imgui_font_atlas_->id());
  ImGui::GetIO().Fonts->SetTexID(texture_id);
}

ImFont* App::GetFont(FontType font) {
  if (font == FontType::Normal) {
    return ImGui::GetIO().Fonts->Fonts[0];
  } else if (font == FontType::Small) {
    return ImGui::GetIO().Fonts->Fonts[1];
  } else if (font == FontType::Large) {
    return ImGui::GetIO().Fonts->Fonts[2];
  } else if (font == FontType::Proggy) {
    return ImGui::GetIO().Fonts->Fonts[3];
  } else {
    return ImGui::GetIO().Fonts->Fonts[0];
  }
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

