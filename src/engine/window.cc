#include "window.h"

#include <cassert>
#include <thread>

#include "base/minlog.h"
#include "base/text.h"
#include "base/platform.h"

namespace rad {

const int Window::kDefault = INT32_MAX;

WindowConfig::WindowConfig()
    : id("Unnamed"),
      title("Unnamed"),
      icon(),
      x(Window::kDefault),
      y(Window::kDefault),
      width(Window::kDefault),
      height(Window::kDefault) {}

WindowConfig::~WindowConfig() {}

namespace windows {

LRESULT CALLBACK staticWindowProc(
    HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

class DropTarget : public IDropTarget {
 public:
  DropTarget() = delete;
  DropTarget(std::function<void(const std::vector<std::string>&)> callback)
      : callback_(callback) {}

  ULONG AddRef() { return 1; }
  ULONG Release() { return 0; }
  HRESULT QueryInterface(REFIID riid, void** obj) {
    if (riid == IID_IDropTarget) {
      *obj = this;
      return S_OK;
    }
    *obj = NULL;
    return E_NOINTERFACE;
  }
  HRESULT DragEnter(IDataObject*, DWORD, POINTL, DWORD* effect) {
    *effect &= DROPEFFECT_COPY;
    return S_OK;
  }
  HRESULT DragLeave() { return S_OK; }
  HRESULT DragOver(DWORD, POINTL, DWORD* effect) {
    *effect &= DROPEFFECT_COPY;
    return S_OK;
  }
  HRESULT Drop(IDataObject* data, DWORD, POINTL, DWORD* effect) {
    FORMATETC fmte = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM stgm;
    if (SUCCEEDED(data->GetData(&fmte, &stgm))) {
      HDROP hdrop = (HDROP)stgm.hGlobal;
      UINT file_count = DragQueryFileW(hdrop, 0xFFFFFFFF, NULL, 0);
      for (UINT i = 0; i < file_count; i++) {
        TCHAR path[4096];
        UINT cch = DragQueryFileW(hdrop, i, path, sizeof(path));
        std::vector<std::string> paths;
        if (cch > 0 && cch < MAX_PATH) {
          paths.push_back(to_string(path));
        }
        if (paths.size()) {
          callback_(paths);
        }
      }
      ::ReleaseStgMedium(&stgm);
    }
    *effect &= DROPEFFECT_COPY;
    return S_OK;
  }

 private:
  std::function<void(const std::vector<std::string>&)> callback_;
};

class WindowImpl : public rad::Window {
 public:
  friend LRESULT CALLBACK staticWindowProc(
      HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

  WindowImpl() = delete;
  WindowImpl(const WindowConfig& config)
      : hwnd_(),
        config_(config),
        wp_(),
        client_rect_(),
        window_rect_(),
        state_(State::Normal),
        active_(false),
        topmost_(false),
        frame_(true),
        fullscreen_(false) {
    // register window class
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = rad::windows::staticWindowProc;
    wc.hInstance = ::GetModuleHandleW(NULL);
    wc.hIcon = ::LoadIconW(wc.hInstance, MAKEINTRESOURCE(config.icon));
    wc.hCursor = ::LoadCursorW(NULL, IDC_ARROW);
    std::wstring id = to_wstring(config.id);
    wc.lpszClassName = id.c_str();
    ATOM atom = ::RegisterClassExW(&wc);
    if (atom == NULL) {
      throw std::runtime_error("falied to RegisterClassEx().");
    }

    // create window
    const HWND parent = NULL;
    const HMENU menu = NULL;
    const HINSTANCE instance = ::GetModuleHandleW(NULL);
    const LPVOID param = static_cast<LPVOID>(this);
    const DWORD exstyle = 0;
    const DWORD style = WS_OVERLAPPEDWINDOW;
    int x = config.x == kDefault ? CW_USEDEFAULT : config.x;
    int y = config.y == kDefault ? CW_USEDEFAULT : config.y;
    int width = config.width == kDefault ? CW_USEDEFAULT : config.width;
    int height = config.height == kDefault ? CW_USEDEFAULT : config.height;
    hwnd_ = ::CreateWindowExW(exstyle, MAKEINTATOM(atom),
        to_wstring(config.title).c_str(), style, x, y, width, height, parent,
        menu, instance, param);
    if (hwnd_ == NULL) {
      throw std::runtime_error("failed to CreateWindowEx().");
    }

    // register dnd
    HRESULT hr = ::OleInitialize(nullptr);
    if (FAILED(hr)) {
      throw std::runtime_error("failed to OleInitialize().");
    }
    drop_target_ = std::unique_ptr<DropTarget>(
        new DropTarget([this](const std::vector<std::string>& d) {
          dispatchEvent(window_event::DragDrop{d});
        }));
    hr = ::RegisterDragDrop(hwnd_, drop_target_.get());
    if (FAILED(hr)) {
      throw std::runtime_error("failed to RegisterDragDrop().");
    }
  }

  virtual ~WindowImpl() noexcept {
    ::RevokeDragDrop(hwnd_);
    ::OleUninitialize();
    if (hwnd_ != NULL) {
      ::DestroyWindow(hwnd_);
    }
  }

  virtual void Show(State state) override {
    if (state == State::Normal) {
      ::ShowWindow(hwnd_, SW_SHOWDEFAULT);
      ::UpdateWindow(hwnd_);
    } else if (state == State::Maximize) {
      ::ShowWindow(hwnd_, SW_SHOWMAXIMIZED);
      ::UpdateWindow(hwnd_);
    } else if (state == State::Minimize) {
      HWND next = ::GetWindow(hwnd_, GW_HWNDNEXT);
      while (true) {
        HWND temp = ::GetParent(next);
        if (!temp) break;
        next = temp;
      }
      ::ShowWindow(hwnd_, SW_SHOWMINIMIZED);
      ::SetForegroundWindow(next);
    }
  }

  virtual bool Update() override {
    MSG msg{};
    while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
    return msg.message != WM_QUIT;
  }

  virtual void Move(int x, int y) override {
    if (x == window_rect_.x && y == window_rect_.y) return;
    if (x == kDefault || y == kDefault) return;
    if (state_ != State::Normal) return;
    if (fullscreen_) return;

    BOOL ret = ::SetWindowPos(hwnd_, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
    if (!ret) {
      throw std::runtime_error("failed SetWindowPos().");
    }
  }
  virtual void Resize(int width, int height, bool client_size) override {
    if (width == kDefault || height == kDefault) return;
    if (state_ != State::Normal) return;
    if (fullscreen_) return;

    if (client_size) {
      RECT rect{window_rect_.x, window_rect_.y, window_rect_.x + width,
          window_rect_.y + height};
      ::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
      width = rect.right - rect.left;
      height = rect.bottom - rect.top;
    }
    if (width == window_rect_.width && height == window_rect_.height) return;

    BOOL ret = ::SetWindowPos(hwnd_, HWND_TOP, 0, 0, width, height, SWP_NOMOVE);
    if (!ret) {
      throw std::runtime_error("failed SetWindowPos().");
    }
  }
  virtual State GetState() const override { return state_; }
  virtual void* GetHandle() const override { return (void*)hwnd_; }
  virtual Rect GetWindowRect() const override { return window_rect_; }
  virtual Rect GetClientRect() const override { return client_rect_; }
  virtual bool IsActive() const override { return active_; }
  virtual bool IsTopmost() const override { return topmost_; }
  virtual bool IsFrame() const override { return frame_; }
  virtual bool IsBorderlessFullscreen() const override { return fullscreen_; }
  virtual void SetTitle(const std::string& title) override {
    ::SetWindowTextW(hwnd_, to_wstring(title).c_str());
  }
  virtual void SetTopmost(bool enabled) override {
    ::SetWindowPos(hwnd_, enabled ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE);
    topmost_ = enabled;
  }
  virtual void SetFrame(bool enabled) override {
    if (fullscreen_) return;

    frame_ = enabled;
    LONG_PTR style = ::GetWindowLongPtr(hwnd_, GWL_STYLE);
    BOOL ret = TRUE;
    if (enabled) {
      ::SetWindowLongPtr(hwnd_, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
      ::SetWindowPlacement(hwnd_, &wp_);
      ::SetWindowPos(hwnd_, NULL, 0, 0, 0, 0,
          SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
              SWP_FRAMECHANGED);
    } else {
      style |= WS_VISIBLE;
      style &= ~WS_OVERLAPPEDWINDOW;
      ::SetWindowLongPtr(hwnd_, GWL_STYLE, style);
      wp_ = {sizeof(WINDOWPLACEMENT)};
      ::GetWindowPlacement(hwnd_, &wp_);
      ::SetWindowPos(hwnd_, NULL, 0, 0, 0, 0,
          SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
    }
  }
  virtual void SetBorderlessFullscreen(bool enabled) override {
    LONG_PTR style = ::GetWindowLongPtr(hwnd_, GWL_STYLE);
    BOOL ret = TRUE;
    if (enabled) {
      fullscreen_ = enabled;
      style |= WS_VISIBLE;
      style &= ~WS_OVERLAPPEDWINDOW;
      ::SetWindowLongPtr(hwnd_, GWL_STYLE, style);

      wp_ = {sizeof(WINDOWPLACEMENT)};
      ::GetWindowPlacement(hwnd_, &wp_);
      HMONITOR monitor = ::MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
      MONITORINFO mi{sizeof(mi)};
      ::GetMonitorInfo(monitor, &mi);
      ::SetWindowPos(hwnd_, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
          mi.rcMonitor.right - mi.rcMonitor.left,
          mi.rcMonitor.bottom - mi.rcMonitor.top,
          SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
      dispatchEvent(window_event::Fullscreen{true});
      return;
    } else {
      if (frame_) {
        style |= WS_OVERLAPPEDWINDOW;
      }
      ::SetWindowLongPtr(hwnd_, GWL_STYLE, style);
      ::SetWindowPlacement(hwnd_, &wp_);
      ::SetWindowPos(hwnd_, NULL, 0, 0, 0, 0,
          SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
              SWP_FRAMECHANGED);
      fullscreen_ = false;
      dispatchEvent(window_event::Fullscreen{false});
      return;
    }
  }

  LRESULT windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
      case WM_CREATE:
        wp_ = {sizeof(WINDOWPLACEMENT)};
        ::GetWindowPlacement(hwnd, &wp_);

        RECT rect;
        ::GetWindowRect(hwnd, &rect);
        window_rect_.x = rect.left;
        window_rect_.y = rect.top;
        window_rect_.width = rect.right - rect.left;
        window_rect_.height = rect.bottom - rect.top;
        ::GetClientRect(hwnd, &rect);
        client_rect_.x = rect.left;
        client_rect_.y = rect.top;
        client_rect_.width = rect.right - rect.left;
        client_rect_.height = rect.bottom - rect.top;

        if (wp_.showCmd == SW_NORMAL) {
          state_ = State::Normal;
        } else if (wp_.showCmd == SW_MAXIMIZE) {
          state_ = State::Maximize;
        } else if (wp_.showCmd == SW_MINIMIZE) {
          state_ = State::Minimize;
        }

        break;
      case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
      case WM_ACTIVATE:
        active_ = (wparam == 1 || wparam == 2);
        break;
      case WM_WINDOWPOSCHANGED: {
        WINDOWPLACEMENT wndpl{sizeof(WINDOWPLACEMENT)};
        ::GetWindowPlacement(hwnd, &wndpl);
        State new_state = state_;
        if (wndpl.showCmd == SW_NORMAL) {
          new_state = State::Normal;
        } else if (wndpl.showCmd == SW_MAXIMIZE) {
          new_state = State::Maximize;
        } else if (wndpl.showCmd == SW_MINIMIZE) {
          new_state = State::Minimize;
        }
        if (state_ != new_state) {
          state_ = new_state;
          dispatchEvent(window_event::State{(int)new_state});
        }
        break;
      }
      case WM_MOVE: {
        RECT rect{};
        ::GetWindowRect(hwnd, &rect);
        if (state_ == State::Normal && !fullscreen_) {
          window_rect_.x = rect.left;
          window_rect_.y = rect.top;
          window_rect_.width = rect.right - rect.left;
          window_rect_.height = rect.bottom - rect.top;
        }
        ::GetClientRect(hwnd, &rect);
        client_rect_.x = rect.left;
        client_rect_.y = rect.top;
        client_rect_.width = rect.right - rect.left;
        client_rect_.height = rect.bottom - rect.top;
        dispatchEvent(window_event::Move{rect.left, rect.top});
        break;
      }
      case WM_SIZE: {
        if ((state_ == State::Normal) && !fullscreen_) {
          RECT rect{};
          ::GetWindowRect(hwnd, &rect);
          window_rect_.x = rect.left;
          window_rect_.y = rect.top;
          window_rect_.width = rect.right - rect.left;
          window_rect_.height = rect.bottom - rect.top;
        }
        int width = LOWORD(lparam);
        int height = HIWORD(lparam);
        client_rect_.width = width;
        client_rect_.height = height;
        dispatchEvent(window_event::Resize{width, height});
        break;
      }
      case WM_ERASEBKGND:
        return TRUE;
      case WM_ENTERSIZEMOVE:
        break;
      case WM_EXITSIZEMOVE: {
        RECT rect{};
        ::GetClientRect(hwnd, &rect);
        client_rect_.width = rect.right - rect.left;
        client_rect_.height = rect.bottom - rect.top;
        ::GetWindowRect(hwnd, &rect);
        window_rect_.width = rect.right - rect.left;
        window_rect_.height = rect.bottom - rect.top;
        dispatchEvent(window_event::Resize{client_rect_.width, client_rect_.height});
        break;
      }
      case WM_SYSCOMMAND:
        if ((wparam & 0xfff0) == SC_KEYMENU) {  // Disable ALT application menu
          return 0;
        }
        break;
    }

    bool handled = dispatchEvent(window_event::NativeEvent{hwnd, msg, wparam, lparam});
    if (handled) {
      return 1;
    }

    return ::DefWindowProc(hwnd, msg, wparam, lparam);
  }

 private:
  WindowConfig config_;
  HWND hwnd_;
  WINDOWPLACEMENT wp_;
  Rect window_rect_;
  Rect client_rect_;
  State state_;
  bool active_;
  bool topmost_;
  bool frame_;
  bool fullscreen_;

  std::unique_ptr<DropTarget> drop_target_;
};

LRESULT CALLBACK staticWindowProc(
    HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (msg == WM_CREATE) {
    Window* impl = (Window*)((LPCREATESTRUCT)lparam)->lpCreateParams;
    ::SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)impl);
  }

  WindowImpl* impl = (WindowImpl*)::GetWindowLongPtr(hwnd, GWLP_USERDATA);
  return impl ? impl->windowProc(hwnd, msg, wparam, lparam)
              : ::DefWindowProc(hwnd, msg, wparam, lparam);
}

}  // namespace windows

Window::~Window() {}

std::unique_ptr<Window> CreatePlatformWindow(const WindowConfig& config) {
  return std::unique_ptr<Window>(new windows::WindowImpl(config));
}

}  // namespace rad
