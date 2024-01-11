#pragma once

#include <vector>
#include <string>

namespace rad {

namespace platform {

std::string getCurrentPath();
std::string getCurrentDirectory();
std::string getUserDirectory();
std::string getTempDirectory();
std::string getFontDirectory();
std::vector<std::string> getCommandLineArgs();
std::string errorMessage(unsigned long win32_error_code);

// Common Dialog
std::string ShowOpenFileDialog(void* parent, const std::string& name,
    const std::string& default_folder = {});
std::string ShowOpenFolderDialog(void* parent, const std::string& name);
std::string ShowSaveDialog(void* parent, const std::string& name,
    const std::string& extension, const std::string& default_folder = {});

// Shell
bool OpenFolder(const std::string& dir);
void OpenURL(const std::string& url);
void OpenControlPanelAppsDefaults();
bool OpenContextMenu(void* parent, const std::string& path);
std::string GetCurrentProcessPath();

// Clipboard
bool SetClipboardBinary(const void* data, size_t size, const std::string& type);
bool SetClipboardText(const std::string& text);

// Process
void RestartCurrentProcess();

}  // namespace platform

}  // namespace rad

