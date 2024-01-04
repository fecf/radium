#pragma once

#include <functional>
#include <string>
#include <memory>
#include <mutex>
#include <queue>
#include <variant>
#include <vector>

namespace rad {

namespace window_event {
struct Move {
  int x;
  int y;
};
struct Resize {
  int width;
  int height;
};
struct State {
  int state;
};
struct Fullscreen {
  bool enabled;
};
struct DragDrop {
  std::vector<std::string> value;
};
struct NativeEvent {
  void* handle;
  unsigned int msg;
  uint64_t wparam;
  int64_t lparam;
};
using window_event_t =
    std::variant<Move, Resize, State, Fullscreen, DragDrop, NativeEvent>;
}  // namespace window_event

struct WindowConfig {
  WindowConfig();
  ~WindowConfig();

  std::string id;
  std::string title;
  int icon;
  int x, y, width, height;
};

class Window {
 public:
  static const int kDefault;

  using EventCallback = std::function<bool(window_event::window_event_t)>;

  enum State { Normal, Minimize, Maximize };
  struct Rect {
    int x, y, width, height;
  };

  virtual ~Window() noexcept;

  virtual void Show(State state) = 0;
  virtual bool Update() = 0;
  virtual void Move(int x, int y) = 0;
  virtual void Resize(int width, int height, bool client_size = true) = 0;

  virtual State GetState() const = 0;
  virtual void* GetHandle() const = 0;
  virtual Rect GetWindowRect() const = 0;
  virtual Rect GetClientRect() const = 0;
  virtual bool IsActive() const = 0;
  virtual bool IsTopmost() const = 0;
  virtual bool IsFrame() const = 0;
  virtual bool IsBorderlessFullscreen() const = 0;
  virtual void SetTitle(const std::string& title) = 0;
  virtual void SetTopmost(bool enabled) = 0;
  virtual void SetFrame(bool enabled) = 0;
  virtual void SetBorderlessFullscreen(bool enabled) = 0;

  void AddEventListener(EventCallback event_cb) {
    event_cb_.push_back(event_cb);
  }

  void EnterFullscreen(bool borderless) {
    if (borderless) {
      if (!IsBorderlessFullscreen()) {
        SetBorderlessFullscreen(true);
      }
    } else {
      if (!IsBorderlessFullscreen()) {
        if (GetState() == State::Normal) {
          Show(State::Maximize);
        }
      }
    }
  }

  void ExitFullscreen() {
    if (IsBorderlessFullscreen()) {
      SetBorderlessFullscreen(false);
    } else {
      Show(State::Normal);
    }
  }

 protected:
  bool dispatchEvent(const window_event::window_event_t& event) const {
    bool handled = false;
    for (const auto& cb : event_cb_) {
      handled |= cb(event);
    }
    return handled;
  }

  std::vector<EventCallback> event_cb_;
  std::mutex event_queue_mutex_;
  std::queue<window_event::window_event_t> event_queue_;
};

std::unique_ptr<Window> CreatePlatformWindow(const WindowConfig& config);

}  // namespace rad
