#pragma once

#include <string>
#include <memory>
#include <vector>
#include <deque>
#include <variant>

#include <base/thread.h>

class App;

namespace rad {
struct Texture;
struct Mesh;
}

template <typename... Base>
struct Visitor : Base... {
  using Base::operator()...;
};

template <typename... T>
Visitor(T...) -> Visitor<T...>;

struct Model {
public:
  struct Content;

  void PushMRU(const std::string& path);
  std::shared_ptr<Content> GetLatestContent();

public:
  std::string content_path;
  std::string latest_content_path;

  float content_zoom = 1.0f;
  float content_cx = 0.0f;
  float content_cy = 0.0f;
  float content_rotate = 0.0f;

  bool thumbnail_show = false;
  int thumbnail_size = 0;
  float thumbnail_alpha = 0.9f;
  float thumbnail_scroll = 0.0f;

  std::string cwd;
  std::vector<std::string> cwd_entries;
  std::deque<std::string> mru;

  struct Content {
    std::string path;
    std::shared_ptr<rad::Texture> texture;
    std::shared_ptr<rad::Mesh> mesh;
    std::chrono::system_clock::time_point timestamp;

    int source_width = 0;
    int source_height = 0;

    entt::entity e;
  };
  std::vector<std::shared_ptr<Content>> contents;

  struct Thumbnail {
    std::string path;
    std::shared_ptr<rad::Texture> texture;
    std::shared_ptr<rad::Mesh> mesh;
    int last_shown_frame = 0;
    std::chrono::system_clock::time_point timestamp;

    float target_x = 0;
    float target_y = 0;
    float target_width = 0;
    float target_height = 0;

    entt::entity e;
  };
  std::vector<std::shared_ptr<Thumbnail>> thumbnails;
};

struct Intent {
  Intent(App& a, Model& m) : a(a), m(m) {}

  struct Open { std::string path; };
  struct OpenPrev {};
  struct OpenNext {};
  struct OpenInExplorer {
    std::string path;
  };
  struct Refresh {};
  struct Fit {};
  struct Center {
    float cx, cy;
  };
  struct Zoom {
    float zoom;
  };
  struct ZoomIn {};
  struct ZoomOut {};
  struct ZoomReset {};
  struct Rotate {
    bool clockwise;
  };
  struct Reset {};
  struct ToggleThumbnail {};
  struct ToggleFullscreen {};
  struct ClearRecentlyOpened {};
  struct ThumbnailZoomIn {};
  struct ThumbnailZoomOut {};

  using Action = std::variant<
    Open,
    OpenPrev,
    OpenNext,
    OpenInExplorer,
    Refresh,
    Fit,
    Center,
    Zoom,
    ZoomIn,
    ZoomOut,
    ZoomReset,
    Rotate,
    Reset,
    ToggleThumbnail,
    ToggleFullscreen,
    ClearRecentlyOpened,
    ThumbnailZoomIn,
    ThumbnailZoomOut
  >;

  void Dispatch(Action a);
  std::shared_ptr<Model::Content> PrefetchContent(const std::string& path);
  std::shared_ptr<Model::Thumbnail> PrefetchThumbnail(const std::string& path);
  void EvictUnusedContent();
  void EvictUnusedThumbnail();

private:
  void openImpl(const std::string& path);

  App& a;
  Model& m;
  rad::ThreadPool pool;
};

struct View {
  View(App& a, const Model& m, Intent& i) : a(a), i(i), m(m) {}
  void Update();

 private:
  void renderImGui();
  void renderContent();
  void renderThumbnail();
  void openDialog();

 private:
  App& a;
  Intent& i;
  const Model& m;
};

