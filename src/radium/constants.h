#pragma once

constexpr const char* kAppName = "radium";
constexpr const char* kAppVersion = "1.0.1.0";
constexpr const char* kAppBuildDateTime = "Build " __DATE__ " " __TIME__;
constexpr const char* kAppURL = "https://github.com/fecf/radium";
constexpr const char* kUpdateHost = "https://github.com";
constexpr const char* kUpdateBinPath = "/fecf/radium/releases/latest/download/radium.zip";
constexpr const char* kUpdateHashPath = "/fecf/radium/releases/latest/download/radium.exe.sha256";

constexpr ImGuiWindowFlags kWindowFlags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus |
    ImGuiWindowFlags_NoFocusOnAppearing;
constexpr ImGuiWindowFlags kWindowFlagsContent =
    kWindowFlags | ImGuiWindowFlags_NoScrollWithMouse;
constexpr ImGuiWindowFlags kWindowFlagsStatic =
    kWindowFlags | ImGuiWindowFlags_AlwaysAutoResize;
constexpr ImGuiWindowFlags kWindowFlagsStaticNoInteraction =
    kWindowFlagsStatic | ImGuiWindowFlags_NoInputs;
constexpr ImGuiWindowFlags kWindowFlagsStaticScroll =
    kWindowFlagsStatic | ImGuiWindowFlags_AlwaysVerticalScrollbar;

