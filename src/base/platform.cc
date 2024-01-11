#include "platform.h"
#include "text.h"

#include <windows.h>
#include <ole2.h>
#include <ShObjIdl_core.h>
#include <ShlObj_core.h>
#include <oleidl.h>
#include <shellapi.h>
#include <wrl.h>
using namespace Microsoft::WRL;

#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>

namespace rad {

namespace platform {

std::string getCurrentPath() {
  TCHAR buf[4096];
  ::GetModuleFileNameW(NULL, buf, sizeof(buf));
  return to_string(buf);
}

std::string getCurrentDirectory() {
  TCHAR filename[4096]{};
  ::GetModuleFileNameW(NULL, filename, 4096);

  std::filesystem::path fspath(filename);
  fspath = fspath.make_preferred().parent_path();
  return to_string(fspath.u8string());
}

std::string getUserDirectory() {
  TCHAR path[4096]{};
  HRESULT hr = ::SHGetFolderPathW(
      NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path);
  if (FAILED(hr)) {
    return "";
  }
  std::filesystem::path fspath(path);
  fspath = fspath.make_preferred();
  return to_string(fspath.u8string());
}

std::string getTempDirectory() {
  TCHAR path[4096];
  HRESULT hr = SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_DEFAULT, path);
  if (FAILED(hr)) {
    return "";
  }
  ::wcscat_s(path, L"\\Temp");
  return to_string(path);
}

std::string getFontDirectory() {
  TCHAR path[MAX_PATH]{};
  HRESULT hr = ::SHGetFolderPathW(NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_DEFAULT, path);
  if (FAILED(hr)) {
    return "";
  }
  std::filesystem::path fspath(path);
  fspath = fspath.make_preferred();
  return to_string(fspath.u8string());
}

std::vector<std::string> getCommandLineArgs() {
  std::wstring commandline = ::GetCommandLineW();
  if (commandline.empty()) {
    return {};
  }
  int argc = 0;
  LPWSTR* argv = ::CommandLineToArgvW(commandline.c_str(), &argc);
  if (argc <= 0 || argv == NULL) {
    return {};
  }
  std::vector<std::string> args;
  for (int i = 0; i < argc; ++i) {
    args.push_back(to_string(argv[i]));
  }
  return args;
}

std::string errorMessage(unsigned long id) {
  std::wstring buf(1024, 0);
  DWORD ret = ::FormatMessageW(
      FORMAT_MESSAGE_FROM_SYSTEM, nullptr, id, 0, buf.data(), 1024, nullptr);
  if (ret < 0) {
    DWORD error = ::GetLastError();
    throw std::runtime_error(std::to_string(error));
  }
  return to_string(buf);
}

std::string ShowOpenFileDialog(void* parent, const std::string& name, const std::string& default_folder) {
  ComPtr<IFileOpenDialog> dialog;
  HRESULT hr;
  hr = ::CoCreateInstance(
      CLSID_FileOpenDialog, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
  if (FAILED(hr)) {
    throw std::runtime_error("CoCreateInstance().");
  }

  hr = dialog->SetTitle(to_wstring(name).c_str());
  if (FAILED(hr)) {
    throw std::runtime_error("IFileOpenDialog::SetTitle().");
  }

  ComPtr<IShellItem> folder;
  if (!default_folder.empty()) {
    std::filesystem::path fspath = rad::to_wstring(default_folder);
    if (fspath.has_parent_path()) fspath = fspath.parent_path();

    hr = ::SHCreateItemFromParsingName(
        fspath.wstring().c_str(), NULL, IID_PPV_ARGS(&folder));
    if (SUCCEEDED(hr)) {
      hr = dialog->SetFolder(folder.Get());
    }
  }

  hr = dialog->Show((HWND)parent);
  if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
    return {};
  } else if (FAILED(hr)) {
    throw std::runtime_error("failed IFileOpenDialog::Show().");
  }

  ComPtr<IShellItem> com_shell_item_;
  hr = dialog->GetResult(&com_shell_item_);
  if (FAILED(hr)) {
    throw std::runtime_error("failed IFileOpenDialog::GetResult().");
  }

  PWSTR path{};
  hr = com_shell_item_->GetDisplayName(SIGDN_FILESYSPATH, &path);
  if (FAILED(hr)) {
    throw std::runtime_error("failed IShellItem::GetDisplayName().");
  }

  std::string ret = to_string(path);
  ::CoTaskMemFree(path);
  return ret;
}

std::string ShowOpenFolderDialog(void* parent, const std::string& name) {
  ComPtr<IFileOpenDialog> dialog;
  HRESULT hr;
  hr = ::CoCreateInstance(
      CLSID_FileOpenDialog, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
  if (FAILED(hr)) {
    throw std::runtime_error("CoCreateInstance().");
  }

  hr = dialog->SetTitle(to_wstring(name).c_str());
  if (FAILED(hr)) {
    throw std::runtime_error("IFileOpenDialog::SetTitle().");
  }

  hr = dialog->SetOptions(FOS_PICKFOLDERS);
  if (FAILED(hr)) {
    throw std::runtime_error("IFileOpenDialog::SetOptions().");
  }

  hr = dialog->Show((HWND)parent);
  if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
    return {};
  } else if (FAILED(hr)) {
    throw std::runtime_error("failed IFileOpenDialog::Show().");
  }

  ComPtr<IShellItem> com_shell_item_;
  hr = dialog->GetResult(&com_shell_item_);
  if (FAILED(hr)) {
    throw std::runtime_error("failed IFileOpenDialog::GetResult().");
  }

  PWSTR path{};
  hr = com_shell_item_->GetDisplayName(SIGDN_FILESYSPATH, &path);
  if (FAILED(hr)) {
    throw std::runtime_error("failed IShellItem::GetDisplayName().");
  }

  std::string ret = to_string(path);
  ::CoTaskMemFree(path);
  return ret;
}

std::string ShowSaveDialog(void* parent, const std::string& name,
    const std::string& extension, const std::string& default_folder) {
  ComPtr<IFileSaveDialog> dialog;

  HRESULT hr = ::CoCreateInstance(
      CLSID_FileSaveDialog, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
  if (FAILED(hr)) {
    throw std::runtime_error("CoCreateInstance().");
  }

  hr = dialog->SetTitle(to_wstring(name).c_str());
  if (FAILED(hr)) {
    throw std::runtime_error("IFileSaveDialog::SetTitle().");
  }

  ComPtr<IShellItem> folder;
  if (!default_folder.empty()) {
    std::filesystem::path fspath = rad::to_wstring(default_folder);
    if (fspath.has_parent_path()) fspath = fspath.parent_path();

    hr = ::SHCreateItemFromParsingName(
        fspath.wstring().c_str(), NULL, IID_PPV_ARGS(&folder));
    if (SUCCEEDED(hr)) {
      dialog->SetFolder(folder.Get());
    }
  }

  hr = dialog->Show((HWND)parent);
  if (FAILED(hr)) {
    throw std::runtime_error("failed IFileSaveDialog::Show().");
  }

  ComPtr<IShellItem> com_shell_item;
  hr = dialog->GetResult(&com_shell_item);
  if (FAILED(hr) || !com_shell_item) {
    throw std::runtime_error("failed IFileSaveDialog::GetResult().");
  }

  PWSTR path{};
  hr = com_shell_item->GetDisplayName(SIGDN_FILESYSPATH, &path);
  if (FAILED(hr)) {
    throw std::runtime_error("failed IShellItem::GetDisplayName().");
  }

  return to_string(path);
}

bool OpenFolder(const std::string& path) {
  std::filesystem::path fspath(rad::to_wstring(path));
  std::error_code ec;
  if (!std::filesystem::exists(fspath, ec) || ec) {
    return false;
  }

  std::wstring param = std::format(L"/select,\"{}\"", fspath.wstring());
  HINSTANCE ret = ::ShellExecuteW(NULL, NULL, L"explorer.exe", param.c_str(), NULL, SW_SHOWNORMAL);
  return static_cast<int>(reinterpret_cast<uintptr_t>(ret)) >= 32;
}

void OpenURL(const std::string& url) {
  ::ShellExecuteW(0, 0, to_wstring(url).c_str(), 0, 0, SW_SHOW);
}

void OpenControlPanelAppsDefaults() {
  ComPtr<IApplicationActivationManager> activator;
  HRESULT hr = CoCreateInstance(CLSID_ApplicationActivationManager, nullptr,
      CLSCTX_INPROC, IID_IApplicationActivationManager, (void**)&activator);

  if (SUCCEEDED(hr)) {
    DWORD pid;
    hr = activator->ActivateApplication(
        L"windows.immersivecontrolpanel_cw5n1h2txyewy"
        L"!microsoft.windows.immersivecontrolpanel",
        L"page=SettingsPageAppsDefaults", AO_NONE, &pid);
    if (SUCCEEDED(hr)) {
      // Do not check error because we could at least open
      // the "Default apps" setting.
      activator->ActivateApplication(
          L"windows.immersivecontrolpanel_cw5n1h2txyewy"
          L"!microsoft.windows.immersivecontrolpanel",
          L"page=SettingsPageAppsDefaults"
          L"&target=SystemSettings_DefaultApps_Browser",
          AO_NONE, &pid);
    }
  }
}

bool OpenContextMenu(void* parent, const std::string& path) {
  ITEMIDLIST* tmp = 0;
  std::wstring wpath = to_wstring(path);
  std::replace(wpath.begin(), wpath.end(), '/', '\\');

  HRESULT result;
  result = ::SHParseDisplayName(wpath.c_str(), 0, &tmp, 0, 0);
  if (!SUCCEEDED(result) || !tmp) return false;

  std::shared_ptr<ITEMIDLIST> id(
      tmp, [](ITEMIDLIST* ptr) { ::CoTaskMemFree(ptr); });

  ComPtr<IShellFolder> shell_folder = 0;
  LPCITEMIDLIST id_child = 0;
  result =
      ::SHBindToParent(id.get(), IID_IShellFolder, &shell_folder, &id_child);
  if (!SUCCEEDED(result) || !shell_folder) return false;

  ComPtr<IContextMenu> context_menu = 0;
  result = shell_folder->GetUIObjectOf(
      (HWND)parent, 1, &id_child, IID_IContextMenu, 0, &context_menu);
  if (!SUCCEEDED(result) || !shell_folder) return false;

  HMENU menu = ::CreatePopupMenu();
  if (!menu) return false;
  if (SUCCEEDED(
          context_menu->QueryContextMenu(menu, 0, 1, 0x7FFF, CMF_NORMAL))) {
    POINT pt{};
    ::GetCursorPos(&pt);
    int cmd =
        ::TrackPopupMenuEx(menu, TPM_RETURNCMD, pt.x, pt.y, (HWND)parent, NULL);
    if (cmd > 0) {
      CMINVOKECOMMANDINFOEX info{sizeof(info)};
      info.fMask = CMIC_MASK_UNICODE;
      info.hwnd = (HWND)parent;
      info.lpVerb = MAKEINTRESOURCEA(cmd - 1);
      info.lpVerbW = MAKEINTRESOURCEW(cmd - 1);
      info.nShow = SW_SHOWNORMAL;
      context_menu->InvokeCommand((LPCMINVOKECOMMANDINFO)&info);
    }
  }
  ::DestroyMenu(menu);
  return true;
}

std::string GetCurrentProcessPath() { 
  TCHAR buf[1024];
  ::GetModuleFileNameW(NULL, buf, 1024);
  return to_string(buf);
}

bool SetClipboardBinary(const void* data, size_t size, const std::string& type) {
  if (!::OpenClipboard(NULL)) {
    return false;
  }
  if (!::EmptyClipboard()) {
    return false;
  }
  HGLOBAL hglobal = ::GlobalAlloc(GMEM_MOVEABLE, size);
  if (hglobal == NULL) {
    return false;
  }
  LPVOID ptr = ::GlobalLock(hglobal);
  if (ptr == NULL) {
    return false;
  }
  ::memcpy(ptr, data, size);
  if (::GlobalUnlock(hglobal) != 0 || ::GetLastError() != NO_ERROR) {
    // returns zero = unlocked
    return false;
  }
  static UINT format =
      ::RegisterClipboardFormatW(to_wstring(type).c_str());  // e.g. PNG
  HANDLE handle = ::SetClipboardData(format, hglobal);
  if (!handle) {
    return false;
  }
  ::CloseClipboard();
  return true;
}

bool SetClipboardText(const std::string& text) {
  if (!::OpenClipboard(NULL)) {
    return false;
  }
  if (!::EmptyClipboard()) {
    return false;
  }
  HGLOBAL hglobal = ::GlobalAlloc(GMEM_MOVEABLE, text.size());
  if (hglobal == NULL) {
    return false;
  }
  LPVOID ptr = ::GlobalLock(hglobal);
  if (ptr == NULL) {
    return false;
  }
  ::memcpy(ptr, text.data(), text.size());
  if (!::GlobalUnlock(hglobal)) {
    return false;
  }
  HANDLE handle = ::SetClipboardData(CF_TEXT, hglobal);
  if (!handle) {
    return false;
  }
  ::CloseClipboard();
  return true;
}

void RestartCurrentProcess() {
  std::atexit([] {
    wchar_t buf[4096];
    ::GetModuleFileNameW(NULL, buf, sizeof(buf));
    std::wstring name = std::filesystem::path(buf).filename().wstring();

    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    BOOL ret = CreateProcessW(
        buf, (LPWSTR)name.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (ret == FALSE) {
      DWORD err = ::GetLastError();
      ::OutputDebugStringW(std::wstring(
          L"failed to RestartCurrentProcess() err=" + std::to_wstring(err))
                               .c_str());
      return;
    }
  });

  ::PostQuitMessage(0);
}

}  // namespace platform

}  // namespace rad

