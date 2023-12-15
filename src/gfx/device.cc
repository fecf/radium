#include "device.h"

#include <cassert>
#include <stdexcept>
#include <system_error>

#include "base/color.h"
#include "base/minlog.h"
#include "base/text.h"
#include "base/thread.h"
#include "shader/compose.ps.h"
#include "shader/compose.vs.h"
#include "shader/imgui.ps.h"
#include "shader/imgui.vs.h"
#include "shader/primary.ps.h"
#include "shader/primary.vs.h"

#include <imgui.h>

#include <conio.h>
#include <physicalmonitorenumerationapi.h>
#include <highlevelmonitorconfigurationapi.h>

#include <linalg.h>
using namespace linalg::aliases;

#include <d3dx12.h>
#include <d3d12.h>
#include <D3D12MemAlloc.h>
#include <wil/result_macros.h>

namespace {

constexpr size_t kBackBufferCount = 2;
constexpr size_t kInflightFrameCount = 2;
constexpr size_t kMaxWaitableLatency = kInflightFrameCount;
constexpr size_t kMaxConstantBufferElementCount = 16;
#ifdef _DEBUG
constexpr UINT kDxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#else
constexpr UINT kDxgiFactoryFlags = 0;
#endif
constexpr D3D_FEATURE_LEVEL kMinFeatureLevel = D3D_FEATURE_LEVEL_12_1;

enum RootSignatureType : int {
  RS_ENGINE_CONSTANTS = 0,
  RS_SHADER_CONSTANTS,
  RS_SRV,
  RS_SAMPLER,
  RS_COUNT,
};

}  // namespace

// #ifdef _DEBUG
// extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 610;}
// extern "C" { __declspec(dllexport) extern const char8_t* D3D12SDKPath = u8"..\\..\\..\\..\\src\\third_party\\D3D12\\"; }
// #endif

namespace rad {

namespace gfx {

Resource::Resource()
    : id(),
      resource(),
      allocation(),
      size(),
      pitch(),
      srv(),
      cbv(),
      uav(),
      rtv(),
      type() {}

Resource::~Resource() {
  if (srv.cpu.ptr) {
    srv.parent->FreeDescriptor(srv);
  }
  if (rtv.cpu.ptr) {
    rtv.parent->FreeDescriptor(rtv);
  }
}

void* Resource::Map() {
  void* ptr{};
  CD3DX12_RANGE range{};
  THROW_IF_FAILED(resource->Map(0, &range, &ptr));
  return ptr;
}

void Resource::Unmap() {
  resource->Unmap(0, NULL);
}

void Resource::Upload(const void* data, size_t size) {
  void* ptr = Map();
  ::memcpy_s(ptr, size, data, size);
  Unmap();
}

DebugLayer::DebugLayer() {
  THROW_IF_FAILED(::D3D12GetDebugInterface(IID_PPV_ARGS(&debug_)));
  debug_->EnableDebugLayer();
  debug_->SetEnableAutoName(TRUE);

  /*
  debug_->SetEnableGPUBasedValidation(TRUE);
  debug_->SetEnableSynchronizedCommandQueueValidation(TRUE);

  THROW_IF_FAILED(::D3D12GetDebugInterface(IID_PPV_ARGS(&dred_)));
  dred_->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
  dred_->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
  dred_->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
  */

  THROW_IF_FAILED(::DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgidebug_)));
  dxgidebug_->EnableLeakTrackingForThread();
}

DebugLayer::~DebugLayer() { ReportLiveObjects(); }

void DebugLayer::Initialize(ID3D12Device* device) {
  /*
  D3D12_MESSAGE_ID hide[] = {
    D3D12_MESSAGE_ID::D3D12_MESSAGE_ID_CREATE_COMMANDLIST12,
    D3D12_MESSAGE_ID::D3D12_MESSAGE_ID_DESTROY_COMMANDLIST12,
  };
  D3D12_INFO_QUEUE_FILTER filter{};
  filter.DenyList.NumIDs = 2;
  filter.DenyList.pIDList = hide;

  HRESULT hr;
  hr = device->QueryInterface<ID3D12InfoQueue>(&info_queue_);
  assert(SUCCEEDED(hr));
  hr = info_queue_->AddStorageFilterEntries(&filter);
  assert(SUCCEEDED(hr));
  */
}

void DebugLayer::ReportLiveObjects() {
  THROW_IF_FAILED(dxgidebug_->ReportLiveObjects(
      DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL)));
}

DescriptorHeap::DescriptorHeap(ComPtr<ID3D12Device> device,
    D3D12_DESCRIPTOR_HEAP_TYPE type, int reserved, int total, bool visible)
    : type_(type),
      reserved_(reserved),
      total_(total),
      visible_(visible),
      current_(),
      active_(),
      increment_() {
  assert(total >= reserved);

  D3D12_DESCRIPTOR_HEAP_DESC desc{};
  desc.NumDescriptors = total;
  desc.Type = type;
  desc.Flags = visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  desc.NodeMask = 0;

  THROW_IF_FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap_)));
  start_.cpu = heap_->GetCPUDescriptorHandleForHeapStart();
  if (visible_) {
    start_.gpu = heap_->GetGPUDescriptorHandleForHeapStart();
  }
  increment_ = device->GetDescriptorHandleIncrementSize(type);
}

Descriptor DescriptorHeap::GetNewDescriptor() { 
  std::lock_guard lock(mutex_);

  uint32_t id = 0;
  if (!free_.empty()) {
    id = free_.back();
    free_.pop_back();
  } else {
    if (current_ >= total_) {
      assert(false && "can't get new descriptor");
      return {};
    }

    id = reserved_ + current_;
    current_++;
    assert(current_ <= total_);
  } 

  D3D12_CPU_DESCRIPTOR_HANDLE cpu = start_.cpu;
  D3D12_GPU_DESCRIPTOR_HANDLE gpu = start_.gpu;
  cpu.ptr += static_cast<uint64_t>(id) * increment_;
  gpu.ptr += static_cast<uint64_t>(id) * increment_;

  Descriptor desc{};
  desc.cpu = cpu;
  desc.gpu = gpu;
  desc.heap_id = id;
  desc.parent = this;

  active_++;
  return desc;
}

void DescriptorHeap::FreeDescriptor(const Descriptor& desc) {
  std::lock_guard lock(mutex_);
  assert(desc.heap_id >= reserved_);
  if (desc.cpu.ptr == NULL) {
    return;
  }
  free_.push_back(desc.heap_id);
  assert(active_ > 0);
  active_--;
}

Descriptor DescriptorHeap::GetDescriptor(int index) {
  assert(index >= reserved_ && index < total_);

  D3D12_CPU_DESCRIPTOR_HANDLE cpu = start_.cpu;
  D3D12_GPU_DESCRIPTOR_HANDLE gpu = start_.gpu;
  cpu.ptr += static_cast<uint64_t>(index) * increment_;
  gpu.ptr += static_cast<uint64_t>(index) * increment_;

  Descriptor desc;
  desc.heap_id = index;
  desc.cpu = cpu;
  desc.gpu = gpu;
  desc.parent = this;
  return desc;
}

Descriptor DescriptorHeap::GetReservedDescriptor(int index) {
  assert(index < reserved_);

  D3D12_CPU_DESCRIPTOR_HANDLE cpu = start_.cpu;
  D3D12_GPU_DESCRIPTOR_HANDLE gpu = start_.gpu;
  cpu.ptr += static_cast<uint64_t>(index) * increment_;
  gpu.ptr += static_cast<uint64_t>(index) * increment_;

  Descriptor desc;
  desc.heap_id = index;
  desc.cpu = cpu;
  desc.gpu = gpu;
  desc.parent = this;
  return desc;
}

CommandList::CommandList(
    ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type)
    : device_(device),
      type_(type),
      live_cmd_allocator_count_(),
      live_cmd_list_count_() {}

std::shared_ptr<CommandSubmission> CommandList::Get() {
  std::unique_lock lock(mutex_);

  ComPtr<ID3D12CommandAllocator> allocator;
  if (!free_cmd_allocator_.empty()) {
    allocator = free_cmd_allocator_.front();
    free_cmd_allocator_.erase(free_cmd_allocator_.begin());
    HRESULT hr = allocator->Reset();
    if (FAILED(hr)) {
      throw std::runtime_error("failed to ID3D12CommandAllocator::Reset().");
    }
  } else {
    THROW_IF_FAILED(device_->CreateCommandAllocator(type_, IID_PPV_ARGS(&allocator)));
    HRESULT hr = allocator->Reset();
    if (FAILED(hr)) {
      throw std::runtime_error("failed to ID3D12CommandAllocator::Reset().");
    }
    live_cmd_allocator_count_++;
  }

  ComPtr<ID3D12GraphicsCommandList> list;
  if (!free_cmd_list_.empty()) {
    list = free_cmd_list_.front();
    free_cmd_list_.erase(free_cmd_list_.begin());
    THROW_IF_FAILED(list->Reset(allocator.Get(), NULL));
  } else {
    ComPtr<ID3D12Device9> device9;
    THROW_IF_FAILED(device_.As(&device9));
    THROW_IF_FAILED(device9->CreateCommandList1(
        0, type_, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&list)));
    THROW_IF_FAILED(list->Reset(allocator.Get(), NULL));
    live_cmd_list_count_++;
  }

  return std::shared_ptr<CommandSubmission>(
      new CommandSubmission{this, allocator, list});
}

void CommandList::ReleaseCommandAllocator(
    ComPtr<ID3D12CommandAllocator> cmd_allocator) {
  std::lock_guard lock(mutex_);
  free_cmd_allocator_.push_back(cmd_allocator);
}

void CommandList::ReleaseCommandList(ComPtr<ID3D12GraphicsCommandList> cmd_list) {
  std::lock_guard lock(mutex_);
  free_cmd_list_.push_back(cmd_list);
}

CommandQueue::CommandQueue(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type)
    : type_(type), fence_value_(1), last_completed_fence_value_(0), handle_() {
  D3D12_COMMAND_QUEUE_DESC desc{};
  desc.Type = type_;
  desc.NodeMask = 0;
  device->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue_));
  THROW_IF_FAILED(
      device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));
  fence_->Signal(last_completed_fence_value_);
  handle_ = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
  assert(handle_ != INVALID_HANDLE_VALUE);
}

CommandQueue::~CommandQueue() { CloseHandle(handle_); }

void CommandQueue::InsertWait(uint64_t value) {
  queue_->Wait(fence_.Get(), value);
}

void CommandQueue::InsertWaitForQueueFence(CommandQueue* q, uint64_t value) {
  queue_->Wait(q->GetFence().Get(), value);
}

void CommandQueue::InsertWaitForQueue(CommandQueue* q) {
  queue_->Wait(q->GetFence().Get(), q->GetNextFenceValue() - 1);
}

void CommandQueue::WaitForFenceCPUBlocking(uint64_t value) {
  if (last_completed_fence_value_ < value) {
    last_completed_fence_value_ = (std::max)(last_completed_fence_value_, fence_->GetCompletedValue());
  }
  if (last_completed_fence_value_ >= value) {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_event_);
  fence_->SetEventOnCompletion(value, handle_);
  WaitForSingleObjectEx(handle_, INFINITE, false);
  last_completed_fence_value_ = value;
}

void CommandQueue::WaitForIdle() { WaitForFenceCPUBlocking(fence_value_ - 1); }

uint64_t CommandQueue::Dispatch(std::shared_ptr<CommandSubmission> command_submission) {
  THROW_IF_FAILED(command_submission->cmd_list->Close());

  ID3D12CommandList* command_list = command_submission->cmd_list.Get();

  queue_->ExecuteCommandLists(1, &command_list);
  command_submission->parent->ReleaseCommandList(command_submission->cmd_list);

  {
    std::lock_guard lock(mutex_inflight_);
    inflight_[fence_value_].push_back(command_submission);
  }

  return SignalFence();
}

uint64_t CommandQueue::SignalFence() {
  std::lock_guard<std::mutex> lock(mutex_fence_);
  queue_->Signal(fence_.Get(), fence_value_);

  {
    std::lock_guard lock(mutex_inflight_);
    std::erase_if(inflight_, [=](auto& kv) {
      if (fence_value_ > kv.first + kInflightFrameCount + 1) {
        for (auto& v : kv.second) {
          v->parent->ReleaseCommandAllocator(v->cmd_allocator);
        }
        return true;
      }
      return false;
    });
  }

  return fence_value_++;
}

ResourceDestructor::ResourceDestructor() {}

ResourceDestructor::~ResourceDestructor() { Notify(UINT64_MAX); }

void ResourceDestructor::Enqueue(Resource* resource, uint64_t frame) {
  assert(resource);
  std::lock_guard lock(mutex_);
  resources_[frame].push_back(resource);
}

void ResourceDestructor::Notify(uint64_t frame) {
  std::lock_guard lock(mutex_);
  std::erase_if(resources_, [=](const auto& kv) { 
    if (frame > kv.first + kInflightFrameCount) {
      for (Resource* resource : kv.second) {
        delete resource;
      }
      return true;
    }
    return false;
  });
}

uint64_t ResourceDestructor::count() const {
  std::lock_guard lock(mutex_);
  uint64_t count = 0;
  for (const auto& [seq, vec] : resources_) {
    count += vec.size();
  }
  return count;
}

Swapchain::Swapchain(ComPtr<IDXGIFactory7> factory,
    ComPtr<ID3D12CommandQueue> command_queue, HWND hwnd, int width, int height,
    int back_buffer_count, int max_waitable_frames)
    : flags_(DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT),
      waitable_object_(NULL) {
  DXGI_SWAP_CHAIN_DESC1 desc{};
  desc.Width = width;
  desc.Height = height;
  desc.Format = kSwapchainFormat;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = back_buffer_count;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Scaling = DXGI_SCALING_NONE;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
  desc.Flags = flags_;

  DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsdesc{};
  fsdesc.Windowed = TRUE;

  ComPtr<IDXGISwapChain1> swapchain;
  THROW_IF_FAILED(factory->CreateSwapChainForHwnd(command_queue.Get(), hwnd,
      &desc, &fsdesc, nullptr, swapchain.GetAddressOf()));
  THROW_IF_FAILED(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
  THROW_IF_FAILED(swapchain.As(&swapchain_));

  swapchain_->SetMaximumFrameLatency(max_waitable_frames);
  waitable_object_ = swapchain_->GetFrameLatencyWaitableObject();

  UINT supported_cs = 0;
  THROW_IF_FAILED(
      swapchain_->CheckColorSpaceSupport(kSwapchainColorSpace, &supported_cs));
  assert(supported_cs & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT);
  THROW_IF_FAILED(swapchain_->SetColorSpace1(kSwapchainColorSpace));

  buffers_.resize(back_buffer_count);
  for (int i = 0; i < back_buffer_count; ++i) {
    THROW_IF_FAILED(swapchain_->GetBuffer((UINT)i, IID_PPV_ARGS(&buffers_[i])));
    wchar_t name[25]{};
    swprintf_s(name, L"RenderTarget%u", i);
    buffers_[i]->SetName(name);
  }

  UpdateStats();
}

Swapchain::~Swapchain() {}

void Swapchain::Wait() const {
  ::WaitForSingleObjectEx(waitable_object_, 1000, true);
}

void Swapchain::Present() {
  swapchain_->Present(1, 0);
}

int Swapchain::GetCurrentBackBufferIndex() const {
  return (int)swapchain_->GetCurrentBackBufferIndex();
}

void Swapchain::UpdateStats() {
  ComPtr<IDXGIOutput> output;
  THROW_IF_FAILED(swapchain_->GetContainingOutput(&output));
  THROW_IF_FAILED(output.As(&output_));

  // get hmonitor
  DXGI_OUTPUT_DESC1 desc1;
  output_->GetDesc1(&desc1);
  HMONITOR hMonitor = desc1.Monitor;

  // HMONITOR to device name
  MONITORINFOEXW mi{sizeof(mi)};
  ::GetMonitorInfoW(hMonitor, &mi);

  // device name to path info
  UINT path_count = 0, mode_count = 0;
  ::GetDisplayConfigBufferSizes(
      QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count);
  std::vector<DISPLAYCONFIG_PATH_INFO> path_vec(path_count);
  std::vector<DISPLAYCONFIG_MODE_INFO> mode_vec(mode_count);
  ::QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count,
      (DISPLAYCONFIG_PATH_INFO*)path_vec.data(), &mode_count,
      (DISPLAYCONFIG_MODE_INFO*)mode_vec.data(), nullptr);

  int target_path_idx = -1;
  for (UINT i = 0; i < path_count; ++i) {
    DISPLAYCONFIG_SOURCE_DEVICE_NAME source_device_name{};
    source_device_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
    source_device_name.header.size = sizeof(source_device_name);
    source_device_name.header.adapterId = path_vec[i].sourceInfo.adapterId;
    source_device_name.header.id = path_vec[i].sourceInfo.id;

    HRESULT hr = ::DisplayConfigGetDeviceInfo(&source_device_name.header);
    assert(SUCCEEDED(hr));
    if (SUCCEEDED(hr)) {
      if (::wcscmp(mi.szDevice, source_device_name.viewGdiDeviceName) == 0) {
        // Found the source which matches this hmonitor. The paths are given
        // in path-priority order so the first found is the most desired,
        // unless we later find an internal.
        if (target_path_idx == -1 ||
            path_vec[i].targetInfo.outputTechnology !=
                DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL ||
            path_vec[i].targetInfo.outputTechnology !=
                DISPLAYCONFIG_OUTPUT_TECHNOLOGY_UDI_EMBEDDED ||
            path_vec[i].targetInfo.outputTechnology !=
                DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED) {
          target_path_idx = i;
        }
      }
    }
  }

  LONG ret = 0;
  DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO get_advanced_color_info{};
  get_advanced_color_info.header.type = DISPLAYCONFIG_DEVICE_INFO_TYPE:: DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
  get_advanced_color_info.header.size = sizeof(get_advanced_color_info);
  get_advanced_color_info.header.adapterId = path_vec[target_path_idx].targetInfo.adapterId;
  get_advanced_color_info.header.id = path_vec[target_path_idx].targetInfo.id;
  ret = ::DisplayConfigGetDeviceInfo(&get_advanced_color_info.header);

  DISPLAYCONFIG_SDR_WHITE_LEVEL get_sdr_white_level;
  get_sdr_white_level.header.type = DISPLAYCONFIG_DEVICE_INFO_TYPE::DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
  get_sdr_white_level.header.size = sizeof(get_sdr_white_level);
  get_sdr_white_level.header.adapterId = path_vec[target_path_idx].targetInfo.adapterId;
  get_sdr_white_level.header.id = path_vec[target_path_idx].targetInfo.id;
  ret = ::DisplayConfigGetDeviceInfo(&get_sdr_white_level.header);

  stats_ = nlohmann::json{};
  auto get_color_encoding_name = [](DISPLAYCONFIG_COLOR_ENCODING encoding) {
    switch (encoding) {
      case DISPLAYCONFIG_COLOR_ENCODING_RGB:
        return "rgb";
      case DISPLAYCONFIG_COLOR_ENCODING_YCBCR444:
        return "ycbcr444";
      case DISPLAYCONFIG_COLOR_ENCODING_YCBCR422:
        return "ycbcr422";
      case DISPLAYCONFIG_COLOR_ENCODING_YCBCR420:
        return "ycbcr420";
      case DISPLAYCONFIG_COLOR_ENCODING_INTENSITY:
        return "intensity";
      case DISPLAYCONFIG_COLOR_ENCODING_FORCE_UINT32:
        return "force_uint32";
      default:
        return "unknown";
    }
  };
  stats_["advanced_color_info"] = nlohmann::json{
      {"supported", get_advanced_color_info.advancedColorSupported > 0},
      {"enabled", get_advanced_color_info.advancedColorEnabled > 0},
      {"force_disabled", get_advanced_color_info.advancedColorSupported > 0},
      {"bits_per_channel", get_advanced_color_info.bitsPerColorChannel},
      {"color_encoding", get_color_encoding_name(get_advanced_color_info.colorEncoding)},
      {"wide_color_enforced", get_advanced_color_info.wideColorEnforced > 0},
  };

// not working without TakeOwnership()
//   DXGI_GAMMA_CONTROL gamma{};
//   output_->GetGammaControl(&gamma);
//   constexpr int gamma_curve_size = sizeof(gamma.GammaCurve) / sizeof(gamma.GammaCurve[0]);
//   for (int i = 0; i < gamma_curve_size; i += gamma_curve_size / 10) {
//     stats_["gamma_curve"]["summary"].push_back(std::format("({:.4f}, {:.4f}, {:.4f})", gamma.GammaCurve[i].Red, gamma.GammaCurve[i].Green, gamma.GammaCurve[i].Blue));
//   }
//   stats_["gamma_curve"]["offset"] = std::format("({:.4f}, {:.4f}, {:.4f})", gamma.Offset.Red, gamma.Offset.Green, gamma.Offset.Blue);
//   stats_["gamma_curve"]["scale"] = std::format("({:.4f}, {:.4f}, {:.4f})", gamma.Scale.Red, gamma.Scale.Green, gamma.Scale.Blue);

  stats_["output"]["min_luminance"] = desc1.MinLuminance;
  stats_["output"]["max_luminance"] = desc1.MaxLuminance;
  stats_["output"]["max_fullframe_luminance"] = desc1.MaxFullFrameLuminance;
  stats_["output"]["red_primary"] = std::format("{:.4f}, {:.4f}", desc1.RedPrimary[0], desc1.RedPrimary[1]);
  stats_["output"]["green_primary"] = std::format("{:.4f}, {:.4f}", desc1.GreenPrimary[0], desc1.GreenPrimary[1]);
  stats_["output"]["blue_primary"] = std::format("{:.4f}, {:.4f}", desc1.BluePrimary[0], desc1.BluePrimary[1]);
  stats_["output"]["white_point"] = std::format("{:.4f}, {:.4f}", desc1.WhitePoint[0], desc1.WhitePoint[1]);
  stats_["output"]["rotation"] = desc1.Rotation;
  stats_["output"]["device_name"] = rad::to_string(desc1.DeviceName);
}

void Swapchain::Resize(int width, int height) {
  size_t back_buffer_count = buffers_.size();
  buffers_.clear();
  THROW_IF_FAILED(swapchain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, flags_));

  buffers_.resize(back_buffer_count);
  for (int i = 0; i < (int)buffers_.size(); ++i) {
    THROW_IF_FAILED(swapchain_->GetBuffer((UINT)i, IID_PPV_ARGS(&buffers_[i])));
    wchar_t name[25]{};
    swprintf_s(name, L"RenderTarget%u", i);
    buffers_[i]->SetName(name);
  }

  UpdateStats();
}

Device::Device()
    : frame_(-1),
      frame_index_(0),
      resource_id_(0),
      hwnd_(),
      width_(),
      height_() {
  CreateDeviceResources();
}

Device::~Device() {
  DestroyResources();
}

void Device::CreateDeviceResources() {
#ifdef _DEBUG
  debug_layer_ = std::unique_ptr<DebugLayer>(new DebugLayer());
#endif
  THROW_IF_FAILED(::CreateDXGIFactory2(kDxgiFactoryFlags, IID_PPV_ARGS(&factory_)));
  THROW_IF_FAILED(factory_->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter_)));
  THROW_IF_FAILED(::D3D12CreateDevice(adapter_.Get(), kMinFeatureLevel, IID_PPV_ARGS(&d3d_)));
#ifdef _DEBUG
  debug_layer_->Initialize(d3d_.Get());
#endif
  destructor_ = std::unique_ptr<ResourceDestructor>(new ResourceDestructor());

  rtv_staging_heap_ = std::unique_ptr<DescriptorHeap>(new DescriptorHeap(d3d_, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 0, 16, false));
  dsv_staging_heap_ = std::unique_ptr<DescriptorHeap>(new DescriptorHeap(d3d_, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, 1, false));
  srv_staging_heap_ = std::unique_ptr<DescriptorHeap>(new DescriptorHeap(d3d_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 16, 1024, false));
  sampler_heap_ = std::unique_ptr<DescriptorHeap>(new DescriptorHeap(d3d_, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2, 2, true));
  for (int i = 0; i < kInflightFrameCount; ++i) {
    srv_heap_[i] = std::unique_ptr<DescriptorHeap>(new DescriptorHeap(d3d_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 16, 1024, true));
  }
  cmd_list_ = std::unique_ptr<CommandList>(new CommandList(d3d_, D3D12_COMMAND_LIST_TYPE_DIRECT));
  render_queue_ = std::unique_ptr<CommandQueue>(new CommandQueue(d3d_.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT));
  copy_queue_ = std::unique_ptr<CommandQueue>(new CommandQueue(d3d_.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT));
  destructor_ = std::unique_ptr<ResourceDestructor>(new ResourceDestructor());

  CD3DX12_ROOT_PARAMETER1 root_params[RS_COUNT];
  root_params[RS_ENGINE_CONSTANTS].InitAsConstants(kEngineConstantsElementCount, 0);
  root_params[RS_SHADER_CONSTANTS].InitAsConstants(kMaxConstantBufferElementCount, 1);
  CD3DX12_DESCRIPTOR_RANGE1 srv(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  root_params[RS_SRV].InitAsDescriptorTable(1, &srv, D3D12_SHADER_VISIBILITY_PIXEL);
  CD3DX12_DESCRIPTOR_RANGE1 sampler(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 2, 0);
  root_params[RS_SAMPLER].InitAsDescriptorTable(1, &sampler, D3D12_SHADER_VISIBILITY_PIXEL);

  ComPtr<ID3DBlob> signature;
  D3D12_ROOT_SIGNATURE_FLAGS flags =
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
  desc.Init_1_1(RS_COUNT, root_params, 0, nullptr, flags);
  THROW_IF_FAILED(::D3DX12SerializeVersionedRootSignature(
      &desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, NULL));
  THROW_IF_FAILED(d3d_->CreateRootSignature(0, signature->GetBufferPointer(),
      signature->GetBufferSize(), IID_PPV_ARGS(&root_signature_)));

  D3D12_SAMPLER_DESC sampler_desc{};
  sampler_desc.Filter = D3D12_FILTER::D3D12_FILTER_MIN_MAG_MIP_POINT;
  sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
  sampler_desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  sampler_desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  sampler_desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  d3d_->CreateSampler(&sampler_desc, sampler_heap_->GetReservedDescriptor(0).cpu);
  sampler_desc.Filter = D3D12_FILTER::D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  d3d_->CreateSampler(&sampler_desc, sampler_heap_->GetReservedDescriptor(1).cpu);

  D3D12MA::ALLOCATOR_DESC allocator_desc{};
  allocator_desc.pDevice = d3d_.Get();
  allocator_desc.pAdapter = adapter_.Get();
  THROW_IF_FAILED(D3D12MA::CreateAllocator(&allocator_desc, &allocator_));

  if (ImGui::GetCurrentContext() == NULL) {
    ImGui::CreateContext();
  }
  ImGuiIO& io = ImGui::GetIO();
  assert(io.BackendRendererUserData == NULL);
  io.BackendRendererUserData = NULL;
  io.BackendRendererName = "imgui_impl_dx12";
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
  imgui_pass_ = {};
  for (int i = 0; i < kInflightFrameCount; ++i) {
    imgui_pass_[i].imgui_ib.reset();
    imgui_pass_[i].imgui_vb.reset();
    imgui_pass_[i].imgui_ib_size = 0;
    imgui_pass_[i].imgui_vb_size = 0;
  }

  // imgui pipeline
  {
    CD3DX12_PIPELINE_STATE_STREAM stream{};
    stream.pRootSignature = root_signature_.Get();
    stream.VS = CD3DX12_SHADER_BYTECODE(imgui_vs, sizeof(imgui_vs));
    stream.PS = CD3DX12_SHADER_BYTECODE(imgui_ps, sizeof(imgui_ps));

    static const D3D12_INPUT_ELEMENT_DESC inputs[3]{
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ImDrawVert, pos),
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ImDrawVert, uv),
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(ImDrawVert, col),
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    stream.InputLayout = D3D12_INPUT_LAYOUT_DESC{inputs, 3};

    CD3DX12_BLEND_DESC blend{};
    blend.AlphaToCoverageEnable = false;
    blend.RenderTarget[0].BlendEnable = true;
    blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    stream.BlendState = blend;

    CD3DX12_RASTERIZER_DESC rasterizer{};
    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE;
    rasterizer.FrontCounterClockwise = FALSE;
    rasterizer.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizer.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizer.DepthClipEnable = true;
    rasterizer.MultisampleEnable = FALSE;
    rasterizer.AntialiasedLineEnable = FALSE;
    rasterizer.ForcedSampleCount = 0;
    rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    stream.RasterizerState = rasterizer;

    CD3DX12_DEPTH_STENCIL_DESC1 ds{};
    ds.DepthEnable = false;
    ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    ds.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    ds.StencilEnable = false;
    ds.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    ds.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    ds.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    ds.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    ds.BackFace = ds.FrontFace;
    stream.DepthStencilState = ds;

    D3D12_STREAM_OUTPUT_DESC stream_output_desc{};
    stream.StreamOutput = stream_output_desc;

    D3D12_RT_FORMAT_ARRAY rt{};
    rt.NumRenderTargets = 1;
    rt.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    stream.RTVFormats = rt;

    stream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    stream.SampleDesc = CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC();
    stream.NodeMask = 0;
    stream.CachedPSO = {};
    stream.SampleMask = UINT_MAX;
    stream.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    stream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    stream.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    D3D12_PIPELINE_STATE_STREAM_DESC state_stream_desc{sizeof(stream), &stream};
    THROW_IF_FAILED(d3d_->CreatePipelineState(
        &state_stream_desc, IID_PPV_ARGS(&imgui_pipeline_)));
  }

  // main pipeline
  {
    CD3DX12_PIPELINE_STATE_STREAM stream{};
    stream.pRootSignature = root_signature_.Get();
    stream.VS = CD3DX12_SHADER_BYTECODE(primary_vs, sizeof(primary_vs));
    stream.PS = CD3DX12_SHADER_BYTECODE(primary_ps, sizeof(primary_ps));

    static const D3D12_INPUT_ELEMENT_DESC inputs[2]{
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    stream.InputLayout = D3D12_INPUT_LAYOUT_DESC{inputs, 2};

    // CD3DX12_BLEND_DESC blend = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
    // stream.BlendState = blend;

    CD3DX12_BLEND_DESC blend{};
    blend.AlphaToCoverageEnable = false;
    blend.RenderTarget[0].BlendEnable = true;
    blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    stream.BlendState = blend;

    CD3DX12_RASTERIZER_DESC rasterizer = CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT());
    // rasterizer.FillMode = D3D12_FILL_MODE_WIREFRAME;
    stream.RasterizerState = rasterizer;

    CD3DX12_DEPTH_STENCIL_DESC1 ds{};
    ds.DepthEnable = false;
    ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    ds.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    ds.StencilEnable = false;
    ds.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    ds.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    ds.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    ds.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    ds.BackFace = ds.FrontFace;
    stream.DepthStencilState = ds;

    D3D12_STREAM_OUTPUT_DESC stream_output{};
    stream.StreamOutput = stream_output;

    D3D12_RT_FORMAT_ARRAY rt{};
    rt.NumRenderTargets = 1;
    rt.RTFormats[0] = Swapchain::kSwapchainFormat;
    stream.RTVFormats = rt;

    stream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    stream.SampleDesc = CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC();
    stream.NodeMask = 0;
    stream.CachedPSO = {};
    stream.SampleMask = UINT_MAX;
    stream.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    stream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    stream.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    D3D12_PIPELINE_STATE_STREAM_DESC state_stream_desc{sizeof(stream), &stream};
    THROW_IF_FAILED(d3d_->CreatePipelineState(&state_stream_desc, IID_PPV_ARGS(&main_pipeline_)));
  }

  // compose pipeline
  {
    CD3DX12_PIPELINE_STATE_STREAM stream{};
    stream.pRootSignature = root_signature_.Get();
    stream.VS = CD3DX12_SHADER_BYTECODE(compose_vs, sizeof(compose_vs));
    stream.PS = CD3DX12_SHADER_BYTECODE(compose_ps, sizeof(compose_ps));

    static const D3D12_INPUT_ELEMENT_DESC inputs[2]{
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    stream.InputLayout = D3D12_INPUT_LAYOUT_DESC{inputs, 2};

    CD3DX12_BLEND_DESC blend{};
    blend.AlphaToCoverageEnable = false;
    blend.RenderTarget[0].BlendEnable = true;
    blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
    blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
    blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    stream.BlendState = blend;

    CD3DX12_RASTERIZER_DESC rasterizer =
        CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT());
    stream.RasterizerState = rasterizer;

    CD3DX12_DEPTH_STENCIL_DESC1 ds{};
    ds.DepthEnable = false;
    ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    ds.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    ds.StencilEnable = false;
    ds.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    ds.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    ds.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    ds.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    ds.BackFace = ds.FrontFace;
    stream.DepthStencilState = ds;

    D3D12_STREAM_OUTPUT_DESC stream_output{};
    stream.StreamOutput = stream_output;

    D3D12_RT_FORMAT_ARRAY rt{};
    rt.NumRenderTargets = 1;
    rt.RTFormats[0] = Swapchain::kSwapchainFormat;
    stream.RTVFormats = rt;

    stream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    stream.SampleDesc = CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC();
    stream.NodeMask = 0;
    stream.CachedPSO = {};
    stream.SampleMask = UINT_MAX;
    stream.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    stream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    stream.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    D3D12_PIPELINE_STATE_STREAM_DESC state_stream_desc{sizeof(stream), &stream};
    THROW_IF_FAILED(d3d_->CreatePipelineState(&state_stream_desc, IID_PPV_ARGS(&compose_pipeline_)));
  }

  // create compose vertex buffer
  {
    std::vector<InputLayout> data = CreateQuad(2.0f);  // -1.0f ~ 1.0f
    compose_quad_vb_ = CreateDynamicBuffer(sizeof(InputLayout) * 6);
    compose_quad_vb_->Upload(data.data(), sizeof(InputLayout) * 6);
  }
}

void Device::CreateWindowDependentResources() {
  render_queue_->WaitForIdle();
  copy_queue_->WaitForIdle();

  for (int i = 0; i < kBackBufferCount; ++i) {
    main_rt_[i].reset();
    offscreen_rt_[i].reset();
  }

  if (swapchain_) {
    try {
      swapchain_->Resize(width_, height_);
    } catch (wil::ResultException& ex) {
      HRESULT hr = ex.GetErrorCode();
      if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        handleDeviceLost(hr);
        return;
      } else {
        assert(false && "unexpected result.");
      }
    }
  } else {
    swapchain_ = std::unique_ptr<Swapchain>(
        new Swapchain(factory_, render_queue_->GetQueue(),
            hwnd_, width_, height_, kBackBufferCount, kMaxWaitableLatency));
  }

  for (int i = 0; i < kBackBufferCount; ++i) {
    ComPtr<ID3D12Resource> resource = swapchain_->GetBackBuffer(i).Get();
    D3D12_RENDER_TARGET_VIEW_DESC desc{};
    desc.Format = Swapchain::kSwapchainFormat;
    desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    Descriptor descriptor = rtv_staging_heap_->GetNewDescriptor();
    d3d_->CreateRenderTargetView(swapchain_->GetBackBuffer(i).Get(), &desc, descriptor.cpu);

    std::shared_ptr<Resource> res(new Resource());
    res->resource = resource;
    res->rtv = descriptor;
    main_rt_[i] = res;
  }

  for (int i = 0; i < kInflightFrameCount; ++i) {
    offscreen_rt_[i] = CreateRenderTarget(width_, height_, DXGI_FORMAT_R8G8B8A8_UNORM);
  }

  // create depth stencil buffer
  D3D12_RESOURCE_DESC ds = CD3DX12_RESOURCE_DESC::Tex2D(
      DXGI_FORMAT_D32_FLOAT, width_, height_, 1, 1);
  ds.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_CLEAR_VALUE depth_clear{};
  depth_clear.Format = DXGI_FORMAT_D32_FLOAT;
  depth_clear.DepthStencil.Depth = 1.0f;
  depth_clear.DepthStencil.Stencil = 0;

  CD3DX12_HEAP_PROPERTIES ds_heap(D3D12_HEAP_TYPE_DEFAULT);
  THROW_IF_FAILED(d3d_->CreateCommittedResource(&ds_heap, D3D12_HEAP_FLAG_NONE,
      &ds, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &depth_clear,
      IID_PPV_ARGS(&depth_stencil_)));

  Descriptor descriptor = dsv_staging_heap_->GetReservedDescriptor(0);
  D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
  dsv.Format = DXGI_FORMAT_D32_FLOAT;
  dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  d3d_->CreateDepthStencilView(depth_stencil_.Get(), &dsv, descriptor.cpu);
}

void Device::DestroyResources() {
  render_queue_->WaitForIdle();
  copy_queue_->WaitForIdle();

  main_rt_ = {};
  offscreen_rt_ = {};
  swapchain_.reset();
  copy_queue_.reset();
  render_queue_.reset();
  depth_stencil_.Reset();

  imgui_pass_ = {};
  main_drawcalls_ = {};
  compose_quad_vb_.reset();

  cmd_list_.reset();
  main_pipeline_.Reset();
  compose_pipeline_.Reset();
  imgui_pipeline_.Reset();
  root_signature_.Reset();
  sampler_heap_.reset();
  srv_heap_ = {};

  destructor_.reset();
  allocator_.Reset();

  rtv_staging_heap_.reset();
  srv_staging_heap_.reset();
  dsv_staging_heap_.reset();

  d3d_.Reset();
  adapter_.Reset();
  factory_.Reset();
#ifdef _DEBUG
  debug_layer_.reset();
#endif
}

void Device::SetWindow(HWND hwnd) {
  hwnd_ = hwnd;
  RECT rc{};
  BOOL ret = ::GetClientRect(hwnd, &rc);
  if (ret == FALSE) {
    LOG_F(WARNING, "failed to GetClientRect().");
  }
  width_ = (int)(rc.right - rc.left);
  height_ = (int)(rc.bottom - rc.top);
  CreateWindowDependentResources();
}

bool Device::Resize(int width, int height) {
  assert(hwnd_ != NULL);
  if (width > 0 && height > 0) {
    width_ = width;
    height_ = height;
    CreateWindowDependentResources();
    return true;
  } else {
    return false;
  }
}

void Device::Prepare() {
  ++frame_;
  frame_index_ = frame_ % kInflightFrameCount;
  swapchain_->Wait();
  render_queue_->WaitForIdle();  // TODO:
}

void Device::Render() {
  bool main = true;
  bool imgui = true;

  std::shared_ptr<Resource> main_rt = main_rt_[swapchain_->GetCurrentBackBufferIndex()];
  std::shared_ptr<Resource> offscreen_rt = offscreen_rt_[frame_index_];

  d3d_->CopyDescriptorsSimple(srv_staging_heap_->count(),
      srv_heap_[frame_index_]->start().cpu, srv_staging_heap_->start().cpu,
      srv_staging_heap_->type());

  uint64_t fence = 0;

  // imgui (offscreen buffer)
  {
    std::shared_ptr<CommandSubmission> cs = cmd_list_->Get();
    ComPtr<ID3D12GraphicsCommandList> cmd_list = cs->cmd_list;

    constexpr FLOAT color[4]{0.0f, 0.0f, 0.0f, 0.0f};
    cmd_list->ClearRenderTargetView(offscreen_rt->rtv.cpu, color, 0, nullptr);
    cmd_list->OMSetRenderTargets(1, &offscreen_rt->rtv.cpu, FALSE, nullptr);

    if (imgui) {
      renderImGui(cmd_list); 
    }

    cmd_list->SetGraphicsRootSignature(root_signature_.Get());
    ID3D12DescriptorHeap* heaps[]{
        srv_heap_[frame_index_]->get(),
        sampler_heap_->get(),
    };
    cmd_list->SetPipelineState(imgui_pipeline_.Get());
    cmd_list->SetDescriptorHeaps(2, heaps);
    cmd_list->SetGraphicsRootDescriptorTable(
        RS_SAMPLER, sampler_heap_->GetReservedDescriptor(0).gpu);

    CD3DX12_RESOURCE_BARRIER before[]{
        CD3DX12_RESOURCE_BARRIER::Transition(offscreen_rt->resource.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
    };
    cmd_list->ResourceBarrier((UINT)std::ssize(before), before);
    fence = render_queue_->Dispatch(cs);
  }

  // main - compose
  if (main) {
    render_queue_->InsertWait(fence);

    std::shared_ptr<CommandSubmission> cs = cmd_list_->Get();
    ComPtr<ID3D12GraphicsCommandList> cmd_list = cs->cmd_list;

    CD3DX12_RESOURCE_BARRIER before[]{
        CD3DX12_RESOURCE_BARRIER::Transition(depth_stencil_.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_DEPTH_WRITE),
        CD3DX12_RESOURCE_BARRIER::Transition(main_rt->resource.Get(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET),
    };
    cmd_list->ResourceBarrier((UINT)std::ssize(before), before);

    const FLOAT color[4]{-.0f, 0.0f, 0.0f, 0.0f};
    cmd_list->ClearRenderTargetView(main_rt->rtv.cpu, color, 0, nullptr);

    const Descriptor dsv = dsv_staging_heap_->GetReservedDescriptor(0);
    cmd_list->OMSetRenderTargets(1, &main_rt->rtv.cpu, FALSE, &dsv.cpu);
    cmd_list->ClearDepthStencilView(
        dsv.cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    cmd_list->SetGraphicsRootSignature(root_signature_.Get());
    cmd_list->SetPipelineState(main_pipeline_.Get());
    ID3D12DescriptorHeap* heaps[]{
        srv_heap_[frame_index_]->get(),
        sampler_heap_->get(),
    };
    cmd_list->SetDescriptorHeaps(2, heaps);
    cmd_list->SetGraphicsRootDescriptorTable(
        RS_SAMPLER, sampler_heap_->GetReservedDescriptor(0).gpu);

    // submit draw calls
    static std::array<uint8_t, kMaxConstantBufferElementCount * 4> zero_buf{};
    cmd_list->SetGraphicsRoot32BitConstants(RS_SHADER_CONSTANTS,
        kMaxConstantBufferElementCount, zero_buf.data(), 0);

    cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (const DrawCall& dc : main_drawcalls_[frame_index_]) {
      CD3DX12_VIEWPORT vp(
          dc.viewport.x, dc.viewport.y, dc.viewport.z, dc.viewport.w);
      cmd_list->RSSetViewports(1, &vp);
      CD3DX12_RECT scissor(
          dc.scissor.x, dc.scissor.y, dc.scissor.z, dc.scissor.w);
      cmd_list->RSSetScissorRects(1, &scissor);

      EngineConstants engine_constants{};
      memcpy(&engine_constants.mvp, &dc.mvp, sizeof(engine_constants.mvp));
      engine_constants.array_src_width = dc.array_src_width;
      engine_constants.array_src_height = dc.array_src_height;

      cmd_list->SetGraphicsRoot32BitConstants(RS_ENGINE_CONSTANTS, kEngineConstantsElementCount, &engine_constants, 0);
      cmd_list->SetGraphicsRoot32BitConstants(RS_SHADER_CONSTANTS, (UINT)(dc.constant_buffer.size() / 4), dc.constant_buffer.data(), 0);

      auto shader_resource = dc.shader_resource.lock();
      if (shader_resource) {
        const Descriptor& srv = srv_heap_[frame_index_]->GetDescriptor( shader_resource->srv.heap_id);
        cmd_list->SetGraphicsRootDescriptorTable(RS_SRV, srv.gpu);
      }

      auto vertex_buffer = dc.vertex_buffer.lock();
      auto index_buffer = dc.index_buffer.lock();
      if (vertex_buffer) {
        D3D12_VERTEX_BUFFER_VIEW view{};
        view.BufferLocation = vertex_buffer->resource->GetGPUVirtualAddress();
        view.SizeInBytes = (UINT)vertex_buffer->size;
        view.StrideInBytes = sizeof(InputLayout);
        cmd_list->IASetVertexBuffers(0, 1, &view);
      }
      if (index_buffer) {
        D3D12_INDEX_BUFFER_VIEW view{};
        view.BufferLocation = index_buffer->resource->GetGPUVirtualAddress();
        view.SizeInBytes = (UINT)index_buffer->size;
        view.Format = DXGI_FORMAT_R32_UINT;
        cmd_list->DrawIndexedInstanced(
            dc.vertex_count, 1, dc.index_start, dc.vertex_start, 0);
      } else {
        cmd_list->DrawInstanced(dc.vertex_count, 1, dc.vertex_start, 0);
      }
    }

    cmd_list->SetPipelineState(compose_pipeline_.Get());
    cmd_list->SetDescriptorHeaps(2, heaps);
    cmd_list->SetGraphicsRootDescriptorTable(
        RS_SAMPLER, sampler_heap_->GetReservedDescriptor(0).gpu);

    CD3DX12_VIEWPORT vp(0.0f, 0.0f, (FLOAT)width_, (FLOAT)height_);
    CD3DX12_RECT scissor(0, 0, width_, height_);
    const Descriptor& offscreen_srv =
        srv_heap_[frame_index_]->GetDescriptor(offscreen_rt->srv.heap_id);
    cmd_list->RSSetViewports(1, &vp);
    cmd_list->RSSetScissorRects(1, &scissor);
    cmd_list->SetGraphicsRootDescriptorTable(RS_SRV, offscreen_srv.gpu);

    cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    vbv.BufferLocation = compose_quad_vb_->resource->GetGPUVirtualAddress();
    vbv.SizeInBytes = (UINT)compose_quad_vb_->size;
    vbv.StrideInBytes = sizeof(InputLayout);
    cmd_list->IASetVertexBuffers(0, 1, &vbv);
    cmd_list->DrawInstanced(6, 1, 0, 0);

    CD3DX12_RESOURCE_BARRIER after[]{
        CD3DX12_RESOURCE_BARRIER::Transition(depth_stencil_.Get(),
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(offscreen_rt->resource.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET),
        CD3DX12_RESOURCE_BARRIER::Transition(main_rt->resource.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT),
    };
    cmd_list->ResourceBarrier((UINT)std::ssize(after), after);

    fence = render_queue_->Dispatch(cs);
    render_queue_->InsertWait(fence);
  }

  try {
    swapchain_->Present();
  } catch (wil::ResultException& ex) {
    HRESULT hr = ex.GetErrorCode();
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
      handleDeviceLost(hr);
      return;
    }
  }

  fence = render_queue_->SignalFence();
  render_queue_->WaitForIdle();

  destructor_->Notify(frame_);

  std::lock_guard lock(mutex_resource_map_);
  std::erase_if(resource_map_, [](const auto& res) { return res.second.expired(); });
}

void Device::Submit(const std::vector<DrawCall>& draw_calls) {
  main_drawcalls_[frame_index_] = draw_calls;
}

std::shared_ptr<Resource> Device::createResource(
    ComPtr<ID3D12Resource> resource, ComPtr<D3D12MA::Allocation> allocation,
    Resource::Type type, size_t size, int pitch) {
  std::shared_ptr<Resource> ret(new Resource(), [this](Resource* resource) {
    {
      std::lock_guard lock(mutex_resource_map_);
      if (resource_map_.count(resource->id)) {
        resource_map_.erase(resource->id);
      }
    }

    if (resource->type != Resource::Type::RenderTarget) {
      destructor_->Enqueue(resource, frame_ + 3);
    } else {
      delete resource;
    }
  });
  ret->id = resource_id_.fetch_add(1);
  ret->type = type;
  ret->resource = resource;
  ret->allocation = allocation;
  ret->size = size;
  ret->pitch = (pitch == 0) ? (int)size : pitch;
  return ret;
}

std::weak_ptr<Resource> Device::queryResource(int resource_id) {
  std::lock_guard lock(mutex_resource_map_);
  auto it = resource_map_.find(resource_id);
  if (it == resource_map_.end()) {
    return {};
  }
  return it->second;
}

void Device::handleDeviceLost(HRESULT hr) {
  DestroyResources();
  CreateDeviceResources();
  CreateWindowDependentResources();
}

void Device::renderImGui(ComPtr<ID3D12GraphicsCommandList> ctx) {
  ImDrawData* dd = ImGui::GetDrawData();
  if (!dd || (dd->DisplaySize.x <= 0.0f) || (dd->DisplaySize.y <= 0.0f)) {
    return;
  }

  ctx->SetGraphicsRootSignature(root_signature_.Get());
  ctx->SetPipelineState(imgui_pipeline_.Get());

  CD3DX12_VIEWPORT vp(0.0f, 0.0f, (FLOAT)width_, (FLOAT)height_);
  CD3DX12_RECT scissor(0, 0, (LONG)width_, (LONG)height_);
  ctx->RSSetViewports(1, &vp);
  ctx->RSSetScissorRects(1, &scissor);

  ComPtr<ID3D12DescriptorHeap> heap0 = srv_heap_[frame_index_]->get();
  ComPtr<ID3D12DescriptorHeap> heap1 = sampler_heap_->get();
  std::array<ID3D12DescriptorHeap*, 2> heaps{heap0.Get(), heap1.Get()};
  ctx->SetDescriptorHeaps((int)heaps.size(), heaps.data());
  ctx->SetGraphicsRootDescriptorTable(RS_SAMPLER, sampler_heap_->GetReservedDescriptor(0).gpu);

  // Create and grow vertex/index buffers if needed
  ImGuiPass* fr = &imgui_pass_[frame_index_];
  if (!fr->imgui_vb || fr->imgui_vb_size < dd->TotalVtxCount) {
    fr->imgui_vb_size = dd->TotalVtxCount + 5000;
    fr->imgui_vb = CreateDynamicBuffer(fr->imgui_vb_size * sizeof(ImDrawVert));
  }

  if (!fr->imgui_ib || fr->imgui_ib_size < dd->TotalIdxCount) {
    fr->imgui_ib_size = dd->TotalIdxCount + 5000;
    fr->imgui_ib = CreateDynamicBuffer(fr->imgui_ib_size * sizeof(ImDrawIdx));
  }

  // Upload vertex/index data into a single contiguous GPU buffer
  ImDrawVert* vtx_dst = (ImDrawVert*)fr->imgui_vb->Map();
  ImDrawIdx* idx_dst = (ImDrawIdx*)fr->imgui_ib->Map();
  for (int n = 0; n < dd->CmdListsCount; n++) {
    const ImDrawList* drawlist = dd->CmdLists[n];
    ::memcpy(vtx_dst, drawlist->VtxBuffer.Data,
        drawlist->VtxBuffer.Size * sizeof(ImDrawVert));
    ::memcpy(idx_dst, drawlist->IdxBuffer.Data,
        drawlist->IdxBuffer.Size * sizeof(ImDrawIdx));
    vtx_dst += drawlist->VtxBuffer.Size;
    idx_dst += drawlist->IdxBuffer.Size;
  }
  fr->imgui_vb->Unmap();
  fr->imgui_ib->Unmap();

  // Setup desired DX state
  renderImGuiResetContext(ctx.Get(), dd, fr);

  int vtx_offset = 0;
  int idx_offset = 0;
  bool outline = false;

  for (int i = 0; i < dd->CmdListsCount; i++) {
    const ImDrawList* cmd_list = dd->CmdLists[i];

    for (int j = 0; j < cmd_list->CmdBuffer.Size; j++) {
      const ImDrawCmd& cmd = cmd_list->CmdBuffer[j];

      const ImVec2 off = dd->DisplayPos;
      LONG clip_x0 = (LONG)(cmd.ClipRect.x - off.x);
      LONG clip_y0 = (LONG)(cmd.ClipRect.y - off.y);
      LONG clip_x1 = (LONG)(cmd.ClipRect.z - off.x);
      LONG clip_y1 = (LONG)(cmd.ClipRect.w - off.y);
      if (clip_x1 <= clip_x0 || clip_y1 <= clip_y0) {
        continue;
      }
      const D3D12_RECT r{clip_x0, clip_y0, clip_x1, clip_y1};
      ctx->RSSetScissorRects(1, &r);

      // callback
      if (cmd.UserCallback == ImDrawCallback_ResetRenderState) {
        renderImGuiResetContext(ctx.Get(), dd, fr);
      }

      // set texture
      int id = (int)(reinterpret_cast<uint64_t>(cmd.GetTexID()));
      std::weak_ptr<Resource> resource = queryResource(id);

      auto sp = resource.lock();
      if (!sp) {
        continue;
      }
      Descriptor srv = srv_heap_[frame_index_]->GetDescriptor(sp->srv.heap_id);
      ctx->SetGraphicsRootDescriptorTable(RS_SRV, srv.gpu);

      // todo: set texture color space
      ctx->DrawIndexedInstanced(cmd.ElemCount, 1, cmd.IdxOffset + idx_offset,
          cmd.VtxOffset + vtx_offset, 0);
    }
    idx_offset += cmd_list->IdxBuffer.Size;
    vtx_offset += cmd_list->VtxBuffer.Size;
  }
}

void Device::renderImGuiResetContext(ComPtr<ID3D12GraphicsCommandList> ctx, ImDrawData* dd, ImGuiPass* fr) {
  ctx->SetGraphicsRootSignature(root_signature_.Get());
  ctx->SetPipelineState(imgui_pipeline_.Get());

  const float L = dd->DisplayPos.x;
  const float R = dd->DisplayPos.x + dd->DisplaySize.x;
  const float T = dd->DisplayPos.y;
  const float B = dd->DisplayPos.y + dd->DisplaySize.y;

  EngineConstants engine_constants{{
      {2.0f / (R - L), 0.0f, 0.0f, 0.0f},
      {0.0f, 2.0f / (T - B), 0.0f, 0.0f},
      {0.0f, 0.0f, 0.5f, 0.0f},
      {(R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f},
  }};
  ctx->SetGraphicsRoot32BitConstants(
      RS_ENGINE_CONSTANTS, kEngineConstantsElementCount, &engine_constants, 0);

  CD3DX12_VIEWPORT vp(0.0f, 0.0f, dd->DisplaySize.x, dd->DisplaySize.y);
  ctx->RSSetViewports(1, &vp);

  D3D12_VERTEX_BUFFER_VIEW vbv{};
  vbv.BufferLocation = fr->imgui_vb->resource->GetGPUVirtualAddress();
  vbv.SizeInBytes = fr->imgui_vb_size * sizeof(ImDrawVert);
  vbv.StrideInBytes = sizeof(ImDrawVert);
  ctx->IASetVertexBuffers(0, 1, &vbv);

  D3D12_INDEX_BUFFER_VIEW ibv{};
  ibv.BufferLocation = fr->imgui_ib->resource->GetGPUVirtualAddress();
  ibv.SizeInBytes = fr->imgui_ib_size * sizeof(ImDrawIdx);
  ibv.Format =
      sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
  ctx->IASetIndexBuffer(&ibv);
  ctx->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  const float blend_factor[4]{};
  ctx->OMSetBlendFactor(blend_factor);
}

std::vector<InputLayout> Device::CreateQuad(float scale) const {
  std::vector<InputLayout> data(6);
  const float s = scale / 2.0f;
  const float z = 0.1f;
  data[0].pos = {-s, +s, z, 0.0f};
  data[1].pos = {+s, -s, z, 0.0f};
  data[2].pos = {-s, -s, z, 0.0f};
  data[0].uv = {0.0f, 0.0f};
  data[1].uv = {1.0f, 1.0f};
  data[2].uv = {0.0f, 1.0f};
  data[3].pos = {+s, -s, z, 0.0f};
  data[4].pos = {-s, +s, z, 0.0f};
  data[5].pos = {+s, +s, z, 0.0f};
  data[3].uv = {1.0f, 1.0f};
  data[4].uv = {0.0f, 0.0f};
  data[5].uv = {1.0f, 0.0f};
  return data;
}

nlohmann::json Device::make_rhi_stats() const {
  D3D12MA::TotalStatistics allocator_stats;
  allocator_->CalculateStatistics(&allocator_stats);

  uint64_t live_count = 0;
  {
    std::lock_guard lock(mutex_resource_map_);
    for (const auto& [key, wp] : resource_map_) {
      if (!wp.expired()) {
        live_count++;
      }
    } 
  }

  nlohmann::json json = {
      {"frame_count", frame_},
      {"draw_call_count", main_drawcalls_.size()},
      {"live_count", live_count},
      {"pending_delete_count", destructor_->count()},
      {"alloc_bytes", allocator_stats.Total.Stats.BlockBytes -
                               allocator_stats.Total.Stats.AllocationBytes},
      {"alloc_unused_bytes", allocator_stats.Total.Stats.AllocationBytes},
  };
  return json;
}

nlohmann::json Device::make_device_stats() const {
  nlohmann::json json = swapchain_->GetStats();
  return json;
}

std::shared_ptr<Resource> Device::CreateBuffer(size_t size) {
  ComPtr<ID3D12Resource> resource;
  ComPtr<D3D12MA::Allocation> resource_allocation;

  D3D12MA::ALLOCATION_DESC alloc_desc{};
  alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(size);
  allocator_->CreateResource(&alloc_desc, &resource_desc,
      D3D12_RESOURCE_STATE_COPY_DEST, NULL, &resource_allocation,
      IID_PPV_ARGS(&resource));

  resource->SetName(L"Buffer");
  resource_allocation->SetName(L"BufferAllocation");

  return createResource(resource, resource_allocation, Resource::Type::Buffer, size, (int)size);
}

std::shared_ptr<Resource> Device::CreateDynamicBuffer(size_t size) {
  ComPtr<ID3D12Resource> resource;
  ComPtr<D3D12MA::Allocation> resource_allocation;

  D3D12MA::ALLOCATION_DESC alloc_desc{};
  alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
  D3D12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(size);
  allocator_->CreateResource(&alloc_desc, &resource_desc,
      D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &resource_allocation,
      IID_PPV_ARGS(&resource));

  resource->SetName(L"DynamicBuffer");
  resource_allocation->SetName(L"DynamicBufferAllocation");

  return createResource(resource, resource_allocation, Resource::Type::Buffer, size, (int)size);
}

std::shared_ptr<Resource> Device::CreateTexture(
    int width, int height, DXGI_FORMAT format) {
  ComPtr<ID3D12Resource> resource;
  ComPtr<D3D12MA::Allocation> resource_allocation;

  auto desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  d3d_->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, NULL, NULL, NULL);
  size_t size = footprint.Footprint.RowPitch * footprint.Footprint.Height;
  size_t pitch = footprint.Footprint.RowPitch;

  D3D12MA::ALLOCATION_DESC alloc_desc{};
  alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC resource_desc =
      CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);
  HRESULT hr = allocator_->CreateResource(&alloc_desc, &resource_desc,
      D3D12_RESOURCE_STATE_COPY_DEST, NULL, &resource_allocation,
      IID_PPV_ARGS(&resource));
  if (FAILED(hr) || !resource) {
    LOG_F(DEBUG, "failed to CreateResource().");
    return {};
  }

  resource->SetName(L"Texture");
  resource_allocation->SetName(L"TextureAllocation");

  D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc{};
  srvdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvdesc.Format = format;
  srvdesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvdesc.Texture2D.MipLevels = 1;

  Descriptor descriptor = srv_staging_heap_->GetNewDescriptor();
  d3d_->CreateShaderResourceView(resource.Get(), &srvdesc, descriptor.cpu);

  std::shared_ptr<Resource> ret =
      createResource(resource, resource_allocation, Resource::Type::Texture, size, (int)pitch);
  ret->srv = descriptor;

  std::lock_guard lock(mutex_resource_map_);
  assert(resource_map_.count(ret->id) == 0);
  resource_map_[ret->id] = ret;

  return ret;
}

std::shared_ptr<Resource> Device::CreateTextureArray(
    int width, int height, int array_size, DXGI_FORMAT format) {
  std::lock_guard lock(mutex_resource_map_);

  ComPtr<ID3D12Resource> resource;
  ComPtr<D3D12MA::Allocation> resource_allocation;

  D3D12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, array_size, 1);
  std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprint(array_size);
  std::vector<UINT> rows(array_size);
  std::vector<UINT64> row_bytes(array_size);
  std::vector<UINT64> total_bytes(array_size);
  d3d_->GetCopyableFootprints(&resource_desc, 0, array_size, 0, &footprint[0],
      &rows[0], &row_bytes[0], &total_bytes[0]);

  D3D12MA::ALLOCATION_DESC alloc_desc{};
  alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
  HRESULT hr = allocator_->CreateResource(&alloc_desc, &resource_desc,
      D3D12_RESOURCE_STATE_COPY_DEST, NULL, &resource_allocation,
      IID_PPV_ARGS(&resource));
  if (FAILED(hr) || !resource) {
    LOG_F(DEBUG, "failed to CreateResource().");
    return {};
  }

  resource->SetName(L"Texture");
  resource_allocation->SetName(L"TextureAllocation");

  D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc{};
  srvdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvdesc.Format = format;
  srvdesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
  srvdesc.Texture2DArray.ArraySize = array_size;
  srvdesc.Texture2DArray.FirstArraySlice = 0;
  srvdesc.Texture2DArray.PlaneSlice = 0;
  srvdesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
  srvdesc.Texture2DArray.MostDetailedMip = 0;
  srvdesc.Texture2DArray.MipLevels = 1;

  Descriptor descriptor = srv_staging_heap_->GetNewDescriptor();
  d3d_->CreateShaderResourceView(resource.Get(), &srvdesc, descriptor.cpu);

  size_t size = row_bytes[0] * resource_desc.Height;
  size_t pitch = row_bytes[0];
  std::shared_ptr<Resource> ret = createResource(resource, resource_allocation,
      Resource::Type::TextureArray, size, (int)pitch);
  ret->srv = descriptor;

  assert(resource_map_.count(ret->id) == 0);
  resource_map_[ret->id] = ret;

  return ret;
}

std::shared_ptr<Resource> Device::CreateRenderTarget(
    int width, int height, DXGI_FORMAT format) {
  ComPtr<ID3D12Resource> resource;
  ComPtr<D3D12MA::Allocation> resource_allocation;

  auto desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  d3d_->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, NULL, NULL, NULL);
  size_t size = footprint.Footprint.RowPitch * footprint.Footprint.Height;
  size_t pitch = footprint.Footprint.RowPitch;

  D3D12MA::ALLOCATION_DESC alloc_desc{};
  alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
  alloc_desc.ExtraHeapFlags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;
  D3D12_RESOURCE_DESC resource_desc =
      CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);
  resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  D3D12_CLEAR_VALUE clear = {format, {}};
  HRESULT hr = allocator_->CreateResource(&alloc_desc, &resource_desc, D3D12_RESOURCE_STATE_RENDER_TARGET,
      &clear, &resource_allocation, IID_PPV_ARGS(&resource));
  if (FAILED(hr)) {
    LOG_F(DEBUG, "failed to CreateResource().");
    return {};
  }

  resource->SetName(L"RenderTarget");
  resource_allocation->SetName(L"RenderTargetAllocation");

  D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
  rtv_desc.Format = format;
  rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  Descriptor rtv_handle = rtv_staging_heap_->GetNewDescriptor();
  d3d_->CreateRenderTargetView(resource.Get(), &rtv_desc, rtv_handle.cpu);

  D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
  srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv_desc.Format = format;
  srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Texture2D.MipLevels = 1;
  Descriptor srv_handle = srv_staging_heap_->GetNewDescriptor();
  d3d_->CreateShaderResourceView(resource.Get(), &srv_desc, srv_handle.cpu);

  std::shared_ptr<Resource> res = createResource(resource, resource_allocation,
      Resource::Type::RenderTarget, size, (int)pitch);
  res->rtv = rtv_handle;
  res->srv = srv_handle;
  return res;
}

void Device::UploadResource2DBatch(
    std::shared_ptr<Resource> dst, const std::vector<UploadDesc>& descs) {
  for (const auto& desc : descs) {
    // pool_.Post([res = std::weak_ptr<Resource>(dst), desc, this] {
      std::shared_ptr<CommandSubmission> cs = cmd_list_->Get();
      ComPtr<ID3D12GraphicsCommandList> cmdlist = cs->cmd_list;

      const uint8_t* src = desc.src;
      int src_pitch = desc.src_pitch;
      int src_width_in_bytes = desc.src_width_in_bytes;
      int src_height = desc.src_height;
      int dst_subresource_index = desc.dst_subresource_index;
      int dst_x_in_bytes = desc.dst_x;
      int dst_y = desc.dst_y;

      // std::shared_ptr<Resource> dst = res.lock();
      // if (!dst) {
      //   return;
      // }

      D3D12_RESOURCE_DESC res_desc = dst->resource->GetDesc();
      ComPtr<ID3D12Resource> staging;
      ComPtr<D3D12MA::Allocation> staging_alloc;
      D3D12MA::ALLOCATION_DESC staging_alloc_desc{};
      staging_alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
      D3D12_RESOURCE_DESC staging_res_desc =
          CD3DX12_RESOURCE_DESC::Buffer(dst->size);
      allocator_->CreateResource(&staging_alloc_desc, &staging_res_desc,
          D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &staging_alloc,
          IID_PPV_ARGS(&staging));
      staging->SetName(L"Staging");
      staging_alloc->SetName(L"StagingAllocation");

      uint8_t* mapped = nullptr;
      CD3DX12_RANGE range{};
      THROW_IF_FAILED(staging->Map(0, &range, (void**)&mapped));
      if (((int)dst->pitch == src_pitch) &&
          ((int)dst->pitch == src_width_in_bytes) &&
          ((int)res_desc.Height == src_height) && (dst_x_in_bytes == 0) &&
          (dst_y == 0)) {
        ::memcpy_s(mapped, dst->size, src, src_pitch * src_height);
      } else {
        int avail_dst_height = std::max(0, (int)res_desc.Height - dst_y);
        int avail_dst_width = std::max(0, (int)dst->pitch - dst_x_in_bytes);
        int copy_height = std::min(src_height, avail_dst_height);
        int copy_width = std::min(src_width_in_bytes, avail_dst_width);
        assert((int)dst->pitch >= copy_width);

        for (int y = 0; y < copy_height; ++y) {
          ::memcpy_s(mapped + ((y + dst_y) * dst->pitch) + (dst_x_in_bytes),
              avail_dst_width, src + (y * src_pitch), copy_width);
        }
      }
      staging->Unmap(0, NULL);

      if (res_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
        D3D12_RESOURCE_BARRIER barrier0 = CD3DX12_RESOURCE_BARRIER::Transition(
            staging.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
        cmdlist->ResourceBarrier(1, &barrier0);

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        d3d_->GetCopyableFootprints(&res_desc, dst_subresource_index, 1, 0,
            &footprint, NULL, NULL, NULL);
        CD3DX12_TEXTURE_COPY_LOCATION dst_loc(
            dst->resource.Get(), dst_subresource_index);
        CD3DX12_TEXTURE_COPY_LOCATION src_loc(staging.Get(), footprint);
        cmdlist->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);

        D3D12_RESOURCE_BARRIER barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
            dst->resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, dst_subresource_index);
        cmdlist->ResourceBarrier(1, &barrier1);
      } else {
        D3D12_RESOURCE_DESC staging_res_desc = staging->GetDesc();
        cmdlist->CopyBufferRegion(dst->resource.Get(), 0, staging.Get(), 0,
            staging_res_desc.DepthOrArraySize);
      }
      copy_queue_->Dispatch(cs);
      copy_queue_->WaitForIdle();
    // });
  }
  pool_.Wait();
}

}  // namespace gfx

}  // namespace rad
