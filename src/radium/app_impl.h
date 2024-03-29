#pragma once

#include <string>
#include <memory>
#include <vector>
#include <deque>
#include <variant>

#include <base/thread.h>

class App;

namespace rad {
class Image;
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

  const std::shared_ptr<Content> GetContent() const;
  const std::shared_ptr<Content> GetPresentContent() const;

public:
  std::string content_path;
  std::string present_content_path;

  float content_zoom = 1.0f;
  float content_cx = 0.0f;
  float content_cy = 0.0f;
  float content_rotate = 0.0f;

  bool thumbnail_show = false;
  int thumbnail_size = 128;
  float thumbnail_alpha = 0.9f;
  float thumbnail_scroll = 0.0f;

  bool overlay_show = false;

  std::string cwd;
  std::chrono::file_clock::time_point cwd_last_modified;
  std::vector<std::string> cwd_entries;
  std::deque<std::string> mru;

  struct Content {
    std::string path;
    std::shared_ptr<rad::Image> image;
    std::shared_ptr<rad::Texture> texture;
    std::chrono::system_clock::time_point timestamp;
    int64_t task_id = 0;
    bool completed = false;
    entt::entity e;
  };
  std::vector<std::shared_ptr<Content>> contents;

  struct Thumbnail {
    std::string path;
    std::shared_ptr<rad::Texture> texture;
    int last_shown_frame = 0;
    std::chrono::system_clock::time_point timestamp;
    float target_x = 0;
    float target_y = 0;
    float target_width = 0;
    float target_height = 0;
    int64_t task_id = 0;
    entt::entity e;
  };
  std::unordered_map<std::string, std::shared_ptr<Thumbnail>> thumbnails;
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
  struct ToggleOverlay {};
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
    ToggleOverlay,
    ToggleThumbnail,
    ToggleFullscreen,
    ClearRecentlyOpened,
    ThumbnailZoomIn,
    ThumbnailZoomOut
  >;

  void Dispatch(Action a);
  std::shared_ptr<Model::Content> PrefetchContent(const std::string& path);
  std::shared_ptr<Model::Thumbnail> PrefetchThumbnail(const std::string& path, int size);
  void PresentContent(std::shared_ptr<Model::Content> content);
  void EvictUnusedContent();
  void EvictUnusedThumbnail();

private:
  void openImpl(const std::string& path);

  App& a;
  Model& m;
};

struct View {
  View(App& a, const Model& m, Intent& i) : a(a), i(i), m(m) {}
  void Update();

 private:
  void renderImGui();
  void renderImGuiOverlay();
  void renderContent();
  void renderThumbnail();
  void openDialog();

 private:
  App& a;
  Intent& i;
  const Model& m;
};

