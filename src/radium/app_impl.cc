#include "app_impl.h"
#include "app.h"

#include <base/io.h>
#include <base/algorithm.h>
#include <base/minlog.h>
#include <base/platform.h>
#include <base/text.h>
#include <engine/engine.h>

#include <fstream>
#include <numeric>
#include <format>

#include <json.hpp>

#include "constants.h"
#include "image_provider.h"
#include "imgui_widgets.h"
#include "material_symbols.h"
#include "service_locator.h"

void Intent::Dispatch(Action action) {
  const auto type_name = typeid(action).name();
  Visitor v{
      [&](const Open& e) { openImpl(e.path); },
      [&](const OpenPrev& e) {
        const std::string& path = m.content_path;
        if (!path.empty()) {
          const auto& entries = m.cwd_entries;
          auto it = std::find_if(entries.begin(), entries.end(),
              [&](const std::string& entry) { return entry == path; });
          if (it != entries.end()) {
            if (it == entries.begin()) {
              it = entries.end();
            }
            it = std::prev(it);
            openImpl(*it);
          }
        }
      },
      [&](const OpenNext& e) {
        const std::string& path = m.content_path;
        if (!path.empty()) {
          const auto& entries = m.cwd_entries;
          auto it = std::find_if(entries.begin(), entries.end(),
              [&](const std::string& entry) { return entry == path; });
          if (it != entries.end()) {
            it = std::next(it);
            if (it == entries.end()) {
              it = entries.begin();
            }
            openImpl(*it);
          }
        }
      },
      [&](const OpenInExplorer& e) { 
        rad::platform::OpenFolder(e.path);
      },
      [&](const Refresh& e) { openImpl(m.content_path); },
      [&](const Fit& e) {
        if (auto content = m.GetPresentContent()) {
          m.content_zoom =
              rad::scale_to_fit(content->source_width, content->source_height,
                  engine().GetWindow()->GetClientRect().width,
                  engine().GetWindow()->GetClientRect().height);
          m.content_cx = 0;
          m.content_cy = 0;
        }
      },
      [&](const Center& e) {
        m.content_cx = e.cx;
        m.content_cy = e.cy;
      },
      [&](const Zoom& e) { m.content_zoom = e.zoom; },
      [&](const ZoomIn& e) {
        float scale = 1.2f;
        m.content_zoom *= scale;
        ImVec2 mouse = ImGui::GetIO().MousePos - ImGui::GetIO().DisplaySize / 2.0f;
        m.content_cx = mouse.x - (mouse.x - m.content_cx) * scale;
        m.content_cy = mouse.y - (mouse.y - m.content_cy) * scale;
      },
      [&](const ZoomOut& e) {
        float scale = 0.8f;
        m.content_zoom *= scale;
        ImVec2 mouse = ImGui::GetIO().MousePos - ImGui::GetIO().DisplaySize / 2.0f;
        m.content_cx = mouse.x - (mouse.x - m.content_cx) * scale;
        m.content_cy = mouse.y - (mouse.y - m.content_cy) * scale;
      },
      [&](const ZoomReset& e) {
        m.content_zoom = 1.0f;
      },
      [&](const Rotate& e) {
        m.content_rotate += e.clockwise ? 90 : -90;
        if (m.content_rotate < 0) {
          m.content_rotate += 360;
        } else if (m.content_rotate >= 360) {
          m.content_rotate -= 360;
        }
      },
      [&](const Reset& e) {
        m.content_zoom = 1.0f;
        m.content_cx = 0;
        m.content_cy = 0;
        m.content_rotate = 0;
        if (auto content = m.GetPresentContent()) {
          if (auto texture = content->texture) {
            m.content_zoom =
                rad::scale_to_fit(content->source_width, content->source_height,
                    engine().GetWindow()->GetClientRect().width,
                    engine().GetWindow()->GetClientRect().height);
          }
        }
      },
      [&](const ToggleThumbnail& e) { m.thumbnail_show = !m.thumbnail_show; },
      [&](const ToggleFullscreen& e) {
        a.PostDeferredTask([this] {
          auto* window = engine().GetWindow();
          if (window->IsBorderlessFullscreen() ||
              window->GetState() == rad::Window::State::Maximize) {
            window->ExitFullscreen();
          } else {
            window->EnterFullscreen(true);
          }
        });
      },
      [&](const ToggleOverlay& e) { m.overlay_show = !m.overlay_show; },
      [&](const ClearRecentlyOpened& e) { m.mru.clear(); },
      [&](const ThumbnailZoomIn& e) {
        m.thumbnail_size = std::min(512, m.thumbnail_size + 16);
      },
      [&](const ThumbnailZoomOut& e) {
        m.thumbnail_size = std::max(16, m.thumbnail_size - 16);
      }
  };
  std::visit(v, action);
}

void Intent::openImpl(const std::string& path) {
  std::string fullpath = rad::GetFullPath(path);
  if (fullpath.empty()) {
    return;
  }
  m.content_path = fullpath;
  m.PushMRU(fullpath);
  
  std::string title = std::format("{} - {}", kAppName, fullpath);
  engine().GetWindow()->SetTitle(title.c_str());

  std::error_code ec;
  std::filesystem::path fspath = rad::to_wstring(rad::ConvertToCanonicalPath(path, ec));
  if (ec) return;
  m.cwd = rad::to_string(fspath.make_preferred().u8string());

  std::filesystem::directory_iterator dir(fspath, ec);
  if (ec) return;
  m.cwd_entries.clear();
  for (const auto& entry : dir) {
    if (entry.is_regular_file()) {
      m.cwd_entries.emplace_back(rad::to_string(entry.path().u8string()));
    }
  }

  PrefetchContent(fullpath);
  std::string prev, next;
  auto it = std::find_if(m.cwd_entries.begin(), m.cwd_entries.end(),
      [&](const std::string& p) { return p == m.content_path; });
  if (it != m.cwd_entries.end()) {
    auto itp = (it == m.cwd_entries.begin()) ? std::prev(m.cwd_entries.end())
                                             : std::prev(it);
    auto itn = (it == std::prev(m.cwd_entries.end())) ? m.cwd_entries.begin()
                                                      : std::next(it);
    prev = *itp;
    next = *itn;
    PrefetchContent(prev);
    PrefetchContent(next);
  }
}

std::shared_ptr<Model::Content> Intent::PrefetchContent(
    const std::string& path) {
  auto prefetchFinished = [=](const std::string& path) {
    if (m.content_path == path) {
      auto it = std::find_if(m.contents.begin(), m.contents.end(),
          [=](const std::shared_ptr<Model::Content>& c) {
            return c->path == path;
          });
      if (it != m.contents.end()) {
        m.present_content_path = path;
        Dispatch(Reset{});
      }
    }
  };

  auto it = std::find_if(m.contents.begin(), m.contents.end(),
      [=](const std::shared_ptr<Model::Content>& c) {
        return c->path == path;
      });
  if (it == m.contents.end()) {
    auto content =
        std::shared_ptr<Model::Content>(new Model::Content(), [](auto* ptr) {
          world().destroy(ptr->e);
          delete ptr;
        });
    content->path = path;
    content->e = world().create();
    m.contents.emplace_back(content);

    a.pool_content.Post([=, c = std::weak_ptr<Model::Content>(content)] {
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
      if (auto sp = c.lock()) {
        sp->texture = ServiceLocator::Get<TiledImageProvider>()->Request(sp->path);
        if (sp->texture) {
          sp->source_width = sp->texture->array_src_width();
          sp->source_height = sp->texture->array_src_height();
        }
        sp->mesh = engine().CreateMesh();
        sp->timestamp = std::chrono::system_clock::now();
        sp->completed = true;
        a.PostDeferredTask([=] { prefetchFinished(path); });
      } else {
        LOG_F(INFO, "task (%s) already deleted", path.c_str());
      }
    });
    return content;
  } else {
    if ((*it)->texture) {
      prefetchFinished(path);
    }
    return (*it);
  }
}

std::shared_ptr<Model::Thumbnail> Intent::PrefetchThumbnail(
    const std::string& path, int size) {
  if (!m.thumbnails.contains(path)) {
    auto thumbnail = std::shared_ptr<Model::Thumbnail>(
        new Model::Thumbnail(), [](auto* ptr) {
          world().destroy(ptr->e);
          delete ptr;
        });
    thumbnail->path = path;
    thumbnail->e = world().create();
    m.thumbnails.emplace(path, thumbnail);

    a.pool_thumbnail.Post([=, c = std::weak_ptr<Model::Thumbnail>(thumbnail)] {
      if (auto sp = c.lock()) {
        sp->texture = ServiceLocator::Get<CachedImageProvider>()->Request(sp->path, size);
        if (!sp->texture) {
          return;
        }
        sp->timestamp = std::chrono::system_clock::now();
        sp->mesh = engine().CreateMesh();
      } else {
        LOG_F(INFO, "task (%s) already deleted", path.c_str());
      }
    });
    return thumbnail;
  } else {
    return m.thumbnails[path];
  }
}


void Intent::EvictUnusedContent() {
  std::string prev, next;
  auto it = std::find_if(m.cwd_entries.begin(), m.cwd_entries.end(),
      [&](const std::string& p) { return p == m.content_path; });
  if (it != m.cwd_entries.end()) {
    auto itp = (it == m.cwd_entries.begin()) ? std::prev(m.cwd_entries.end())
                                             : std::prev(it);
    auto itn = (it == std::prev(m.cwd_entries.end())) ? m.cwd_entries.begin()
                                                      : std::next(it);
    prev = *itp;
    next = *itn;
  }

  auto it2 = std::remove_if(
      m.contents.begin(), m.contents.end(), [=](std::shared_ptr<Model::Content> c) {
        return 
          c->path != m.content_path && 
          c->path != m.present_content_path && 
          c->path != next && 
          c->path != prev;
      });
  if (it2 != m.contents.end()) {
    m.contents.erase(it2, m.contents.end());
  }
}

void Intent::EvictUnusedThumbnail() {
  std::erase_if(m.thumbnails, [=](const auto& item) {
    return ImGui::GetFrameCount() - item.second->last_shown_frame > 3;
  });
}

void Model::PushMRU(const std::string& path) {
  auto it = std::remove(mru.begin(), mru.end(), path);
  if (it != mru.end()) {
    mru.erase(it, mru.end());
  }
  mru.push_front(path);
  while (mru.size() > 20) {
    mru.erase(std::prev(mru.end()));
  }
}

const std::shared_ptr<Model::Content> Model::GetContent() const {
  auto it = std::find_if(contents.begin(), contents.end(),
      [this](const std::shared_ptr<Content> a) {
        return a->path == content_path;
      });
  return it != contents.end() ? (*it) : nullptr;
}

const std::shared_ptr<Model::Content> Model::GetPresentContent() const {
  auto it = std::find_if(contents.begin(), contents.end(),
      [this](const std::shared_ptr<Content> a) {
        return a->path == present_content_path;
      });
  return it != contents.end() ? (*it) : nullptr;
}
