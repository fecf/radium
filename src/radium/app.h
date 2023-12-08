#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <queue>

#include "service_locator.h"

#include <base/io.h>
#include <base/text.h>
#include <engine/engine.h>
#include <engine/window.h>
#include <flecs.h>

struct Config {
  int window_x = rad::Window::kDefault;
  int window_y = rad::Window::kDefault;
  int window_width = rad::Window::kDefault;
  int window_height = rad::Window::kDefault;
  rad::Window::State window_state = rad::Window::State::Normal;
  std::vector<std::string> mru;
  bool nav = true;
  bool debug = false;
};

namespace nlohmann {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Config, window_x, window_y,
    window_width, window_height, window_state, mru, nav, debug);
}

namespace ecs {

struct Content {
  std::string path;
};

struct ContentLayout {
  float scale = 1.0f;
  float cx = 0.0f;
  float cy = 0.0f;
};
struct ContentLayoutFitEvent {};
struct ContentLayoutCenterEvent {};
struct ContentLayoutResizeEvent { float scale = 1.0f; };
struct ContentLayoutZoomInEvent {};
struct ContentLayoutZoomOutEvent {};
struct ContentLayoutZoomResetEvent {};

struct Explorer {
  int zoom = 0;
};

struct ImageSource {
  std::string path;
};
struct ImageSourceRefreshEvent {};

struct FileEntryList {
  std::string path;
  std::vector<std::string> entries;
};
struct FileEntryListRefreshEvent {
  std::string path;
};

}  // namespace ecs

class App {
 public:
  friend App& app();

  void Start(int argc, char** argv);
  void PostDeferredTask(std::function<void()> func);

  void Open(const std::string& path);
  void OpenDialog();
  void OpenPrev();
  void OpenNext();
  void OpenDirectory(const std::string& path);
  void Refresh();
  void ToggleExplorer();
  void ToggleFullscreen();

 private:
  App() = default;

  void initImGui();
  void initECS();

  void processDeferredTasks();

  bool loadSettings();
  bool saveSettings();

  void pushMRU(const std::string& path);

  struct Font {
    ImFont* small = nullptr;
    ImFont* normal = nullptr;
    ImFont* proggy = nullptr;
  } font;

 private:
  Config config_;
  std::mutex mutex_;
  std::queue<std::function<void()>> deferred_tasks_;
  std::unique_ptr<rad::Texture> imgui_font_atlas_{};
  std::deque<std::string> mru_;
};

/*
struct ContentWindow {
  float alpha = 1.0f;
  float zoom = 1.0f;
  ImRect viewport;
  ImVec2 center;  // center coords of image (before zoom)
  bool always_fit = true;

  // temporary flags
  bool once_fit = false;
  bool once_center = false;
  float once_zoom_based_on_image = 0.0f;

  // temporary
  ImVec2 mouse_pos;
  ImVec2 viewport_mouse_pos;
  ImVec2 viewport_mouse_pos_r;
  ImRect viewport_uv;

  ImRect rect;

  bool nav = true;
  int nav_size = 200;
  ImRect nav_rect;
};
*/

/*
struct ExplorerWindow {
  int thumbnail_size = 256;
  bool thumbnail_scroll_flag = false;
};

*/