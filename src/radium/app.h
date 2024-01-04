#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include <base/thread.h>
#include <engine/window.h>

namespace rad {
struct Texture;
}

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

struct ContentContext {
  std::string path;
  std::string latest;
};
struct ContentPrefetchEvent {
  std::string path;
};
struct ContentPrefetchedEvent {
  std::string path;
};
struct ContentLayout {
  float scale = 1.0f;
  float cx = 0.0f;
  float cy = 0.0f;
  float rotate = 0.0f;
};
struct ContentLayoutFitEvent {};
struct ContentLayoutCenterEvent {
  float cx = 0.0f;
  float cy = 0.0f;
};
struct ContentLayoutResizeEvent {
  float scale = 1.0f;
};
struct ContentLayoutZoomInEvent {};
struct ContentLayoutZoomOutEvent {};
struct ContentLayoutZoomResetEvent {};
struct ContentLayoutRotateEvent {
  bool clockwise = true;
};
struct ContentLayoutRotateResetEvent {};

struct ThumbnailContext {};
struct ThumbnailLayout {
  bool show = false;
  int size = 0;
  float alpha = 0.9f;
  float scroll = 0.0f;
};
struct ThumbnailLayoutZoomInEvent {};
struct ThumbnailLayoutZoomOutEvent {};
struct ThumbnailLayoutToggleEvent {};

struct Image {
  std::string path;
};
struct ImageLayout {
  float x;
  float y;
  float width;
  float height;
};
struct ImageLifetime {
  int frame;
};
struct ImageLoadedEvent {
};

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
  void ToggleFullscreen();
  void ToggleDebug();

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
  rad::ThreadPool pool_;
};
