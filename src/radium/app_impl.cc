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

constexpr int kPrefetchStartDelayMs = 16;

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
      [&](const Refresh& e) { 
        auto it = std::remove_if(m.contents.begin(), m.contents.end(),
            [&](std::shared_ptr<Model::Content> sp) {
              return sp->path == m.content_path;
            });
        m.contents.erase(it, m.contents.end());
        openImpl(m.content_path); 
      },
      [&](const Fit& e) {
        if (auto content = m.GetPresentContent()) {
          if (content->image) {
            m.content_zoom =
                rad::scale_to_fit(content->image->width, content->image->height,
                    engine().GetWindow()->GetClientRect().width,
                    engine().GetWindow()->GetClientRect().height);
          }
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
          if (auto image = content->image) {
            m.content_zoom =
                rad::scale_to_fit(content->image->width, content->image->height,
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
  if (path.empty()) {
    return;
  }

  std::string fullpath = rad::GetFullPath(path);
  std::error_code ec;
  std::filesystem::path fspath(rad::to_wstring(fullpath));
  if (!std::filesystem::is_regular_file(fspath, ec) || ec) {
    return;
  }

  // set content path
  m.content_path = fullpath;

  // add to mru list
  auto it = std::remove(m.mru.begin(), m.mru.end(), path);
  if (it != m.mru.end()) {
    m.mru.erase(it, m.mru.end());
  }
  m.mru.push_front(path);
  while (m.mru.size() > 20) {
    m.mru.erase(std::prev(m.mru.end()));
  }

  // set window title
  std::string title = std::format("{} - {}", kAppName, fullpath);
  engine().GetWindow()->SetTitle(title.c_str());

  // change cwd
  bool changed = false;
  std::filesystem::path fsdir = fspath.parent_path();
  std::string dir = rad::to_string(fsdir.wstring());
  if (m.cwd != dir) {
    m.cwd = dir;
    changed = true;
  }
  std::filesystem::file_time_type modified =
      std::filesystem::last_write_time(fsdir, ec);
  if (m.cwd_last_modified != modified && !ec) {
    m.cwd_last_modified = modified;
    changed = true;
  }
  if (changed) {
    m.cwd_entries.clear();
    std::filesystem::directory_iterator dir(fsdir, ec);
    for (const auto& entry : dir) {
      if (entry.is_regular_file()) {
        m.cwd_entries.emplace_back(rad::to_string(entry.path().wstring()));
      }
    }

    std::sort(m.cwd_entries.begin(), m.cwd_entries.end(),
        [](const std::string& a, const std::string& b) {
          if (rad::natural_sort::strnatcasecmp(rad::to_wstring(a).c_str(),
                  rad::to_wstring(b).c_str()) == -1)
            return true;
          else
            return false;
        });
  }

  // prefetch content
  std::shared_ptr<Model::Content> content = PrefetchContent(fullpath);
  if (content && content->completed) {
    PresentContent(content);
  }
  
  // prefetch adjacent content
  auto it2 = std::find_if(m.cwd_entries.begin(), m.cwd_entries.end(),
      [&](const std::string& p) { return p == fullpath; });
  if (it2 != m.cwd_entries.end()) {
    auto prev = (it2 == m.cwd_entries.begin()) ? std::prev(m.cwd_entries.end())
                                               : std::prev(it2);
    auto next = (it2 == std::prev(m.cwd_entries.end())) ? m.cwd_entries.begin()
                                                        : std::next(it2);
    PrefetchContent(*prev);
    PrefetchContent(*next);
  }
}

std::shared_ptr<Model::Content> Intent::PrefetchContent(
    const std::string& path) {
  auto it = std::find_if(m.contents.begin(), m.contents.end(),
      [=](const std::shared_ptr<Model::Content>& c) {
        return c->path == path;
      });
  if (it != m.contents.end()) {
    return (*it);
  }

  auto content = std::shared_ptr<Model::Content>(
      new Model::Content(), [=](Model::Content* ptr) {
        auto task_id = ptr->task_id;
        auto entity = ptr->e;
        a.PostDeferredTask([=] {
          a.pool_content.TryCancel(task_id);
          world().destroy(entity);
          delete ptr;
        });
      });
  content->path = path;
  content->e = world().create();
  m.contents.emplace_back(content);

  auto wp = std::weak_ptr<Model::Content>(content);
  a.pool_content.Post([=] {
    if (wp.expired()) {
      LOG_F(INFO, "task (%s) already deleted (1)", path.c_str());
      return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(kPrefetchStartDelayMs));
    auto result = ServiceLocator::Get<ContentImageProvider>()->Request(path);

    if (auto sp = wp.lock()) {
      sp->image = std::move(result.image);
      if (sp->image) {
        sp->image->buffer.reset();  // no need to use
      }
      sp->texture = std::move(result.texture);
      sp->timestamp = std::chrono::system_clock::now();
      sp->completed = true;
      sp->task_id = rad::ThreadPool::CurrentTaskId;

      a.PostDeferredTask([=] { 
        if (sp->path == m.content_path) {
          PresentContent(sp);
        }
      });
    } else {
      LOG_F(INFO, "task (%s) already deleted (2)", path.c_str());
    }
  });
  return content;
}

std::shared_ptr<Model::Thumbnail> Intent::PrefetchThumbnail(
    const std::string& path, int size) {
  if (!m.thumbnails.contains(path)) {
    auto thumbnail = std::shared_ptr<Model::Thumbnail>(
        new Model::Thumbnail(), [=](Model::Thumbnail* ptr) {
          auto task_id = ptr->task_id;
          auto entity = ptr->e;
          a.PostDeferredTask([=] {
            a.pool_thumbnail.TryCancel(task_id);
            world().destroy(entity);
            delete ptr;
          });
        });
    thumbnail->path = path;
    thumbnail->e = world().create();
    m.thumbnails.emplace(path, thumbnail);

    auto wp = std::weak_ptr<Model::Thumbnail>(thumbnail);
    a.pool_thumbnail.Post([=] {
      if (wp.expired()) {
        LOG_F(INFO, "task (%s) already deleted (1)", path.c_str());
        return;
      }

      if (auto sp = wp.lock()) {
        sp->texture = ServiceLocator::Get<ThumbnailImageProvider>()->Request(sp->path, size);
        if (!sp->texture) {
          return;
        }
        sp->timestamp = std::chrono::system_clock::now();
        sp->task_id = rad::ThreadPool::CurrentTaskId;
      } else {
        LOG_F(INFO, "task (%s) already deleted", path.c_str());
      }
    });
    return thumbnail;
  } else {
    return m.thumbnails[path];
  }
}

void Intent::PresentContent(std::shared_ptr<Model::Content> content) {
  m.present_content_path = content->path;
  Dispatch(Intent::Reset{});
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
