#pragma once

#ifdef __cplusplus

// stl
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <format>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

#define _USE_MATH_DEFINES
#include <math.h>

// embed resources
#include <embed/ms_regular.h>
#include <embed/ms_extralight.h>
#include <embed/license.h>
#include <embed/icon.h>

// direct3d
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>

// dwrite
#include <d2d1_3.h>
#include <dwrite_3.h>

// winrt
#include <winrt/base.h>

// com
#include <combaseapi.h>
#include <comdef.h>
#include <shellapi.h>
#include <shlobj_core.h>
#include <shlwapi.h>
#include <shobjidl_core.h>
#include <wrl.h>

// ole
#include <ole2.h>
#include <oleidl.h>

// windows
#include <conio.h>
#include <physicalmonitorenumerationapi.h>
#include <highlevelmonitorconfigurationapi.h>
#include <windows.h>
#include <wincodec.h>
#undef min
#undef max
#undef small

// third_party
#include <D3D12MemAlloc.h>
#include <linalg.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imconfig.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <nlohmann/json.hpp>

#include <entt.hpp>

#endif

