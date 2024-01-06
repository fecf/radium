#pragma once

#include "app_impl.h"

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

class App {
 public:
  App();

  void Start(int argc, char** argv);
  void PostDeferredTask(std::function<void()> func);

  enum FontType { Normal, Small, Large, Proggy };
  ImFont* GetFont(FontType font);

  rad::ThreadPool pool_content;
  rad::ThreadPool pool_thumbnail;

 private:
  void setupImGui();
  void buildImGuiFonts();
  void uploadImGuiFonts();
  void processDeferredTasks();
  bool loadSettings();
  bool saveSettings();

 private:
  Config config_;
  std::recursive_mutex mutex_;
  std::queue<std::function<void()>> deferred_tasks_;
  std::unique_ptr<rad::Texture> imgui_font_atlas_;

  Model m;
  View v;
  Intent i;
};
