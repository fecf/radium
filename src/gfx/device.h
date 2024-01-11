#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <d3dcommon.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>
using namespace Microsoft::WRL;

#include <json.hpp>
#include <linalg.h>
using namespace linalg::aliases;

#include "base/thread.h"

namespace D3D12MA {
class Allocator;
class Allocation;
}  // namespace D3D12MA

struct ImDrawData;

namespace rad {

namespace gfx {

#pragma pack(push, 1)
struct EngineConstants {
  float mvp[4][4];
  int array_src_width;
  int array_src_height;
};
#pragma pack(pop)
constexpr int kEngineConstantsElementCount = sizeof(EngineConstants) / 4;

class DebugLayer {
 public:
  DebugLayer();
  ~DebugLayer();

  void Initialize(ID3D12Device* device);
  void ReportLiveObjects();

 private:
  ComPtr<ID3D12Debug6> debug_;
  ComPtr<IDXGIDebug1> dxgidebug_;
  ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> dred_;
  ComPtr<ID3D12InfoQueue> info_queue_;
};

class DescriptorHeap;
class Descriptor {
 public:
  Descriptor() : cpu(), gpu(), heap_id(), parent() {}
  D3D12_CPU_DESCRIPTOR_HANDLE cpu;
  D3D12_GPU_DESCRIPTOR_HANDLE gpu;
  int heap_id;
  DescriptorHeap* parent;
};

class Resource {
  friend class Device;

 public:
  Resource();
  virtual ~Resource();

  uint64_t id;
  enum class Type {
    Buffer,
    VertexBuffer,
    IndexBuffer,
    ConstantBuffer,
    Texture,
    TextureArray,
    RenderTarget,
  };
  Type type;
  ComPtr<ID3D12Resource> resource;
  ComPtr<D3D12MA::Allocation> allocation;
  size_t size;
  int pitch;
  Descriptor cbv;
  Descriptor srv;
  Descriptor rtv;
  Descriptor uav;

  void* Map();
  void Unmap();
  void Upload(const void* data, size_t size);
};

class DescriptorHeap {
 public:
  DescriptorHeap(ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE type,
      int reserved, int total, bool shader_visible);

  D3D12_DESCRIPTOR_HEAP_TYPE type() const { return type_; }
  ID3D12DescriptorHeap* get() const { return heap_.Get(); }
  Descriptor start() const { return start_; }
  int count() const { return total_; }

  Descriptor GetNewDescriptor();
  Descriptor GetDescriptor(int index);
  Descriptor GetReservedDescriptor(int index);
  void FreeDescriptor(const Descriptor& descriptor);

 private:
  D3D12_DESCRIPTOR_HEAP_TYPE type_;
  int reserved_;
  int total_;
  bool visible_;
  Descriptor start_;
  ComPtr<ID3D12DescriptorHeap> heap_;

  std::vector<int> free_;
  int current_;
  int active_;
  int increment_;
  std::mutex mutex_;
};

class CommandList;
class CommandSubmission {
 public:
  CommandList* parent;
  ComPtr<ID3D12CommandAllocator> cmd_allocator;
  ComPtr<ID3D12GraphicsCommandList> cmd_list;
};
class CommandList {
 public:
  CommandList(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type);
  std::shared_ptr<CommandSubmission> Get();

  void ReleaseCommandAllocator(ComPtr<ID3D12CommandAllocator> cmd_allocator);
  void ReleaseCommandList(ComPtr<ID3D12GraphicsCommandList> cmd_list);

 private:
  ComPtr<ID3D12Device> device_;
  D3D12_COMMAND_LIST_TYPE type_;

  std::mutex mutex_;
  std::vector<ComPtr<ID3D12CommandAllocator>> free_cmd_allocator_;
  std::vector<ComPtr<ID3D12GraphicsCommandList>> free_cmd_list_;
  uint64_t live_cmd_list_count_;
  uint64_t live_cmd_allocator_count_;
};

class CommandQueue {
 public:
  CommandQueue(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type);
  ~CommandQueue();

  void InsertWait(uint64_t value);
  void InsertWaitForQueueFence(CommandQueue* queue, uint64_t value);
  void InsertWaitForQueue(CommandQueue* queue);
  void WaitForFenceCPUBlocking(uint64_t value);
  void WaitForIdle();

  uint64_t Dispatch(std::shared_ptr<CommandSubmission> command_submission);
  uint64_t GetNextFenceValue() { return fence_value_; }
  uint64_t SignalFence();

  ComPtr<ID3D12CommandQueue> GetQueue() { return queue_; }
  ComPtr<ID3D12Fence> GetFence() { return fence_; }

 private:
  D3D12_COMMAND_LIST_TYPE type_;
  ComPtr<ID3D12CommandQueue> queue_;
  ComPtr<ID3D12Fence> fence_;
  uint64_t fence_value_;
  uint64_t last_completed_fence_value_;
  HANDLE handle_;
  std::mutex mutex_fence_;
  std::mutex mutex_event_;
  std::mutex mutex_inflight_;
  std::map<uint64_t, std::vector<std::shared_ptr<CommandSubmission>>> inflight_;
};

struct DrawCall {
  float4 viewport;  // x, y, w, h
  int4 scissor;     // x, y, w, h

  // shader mandatory variables
  float4x4 mvp;
  int array_src_width = 0;
  int array_src_height = 0;

  // shader resources
  std::weak_ptr<Resource> shader_resource;
  std::weak_ptr<Resource> vertex_buffer;
  std::weak_ptr<Resource> index_buffer;
  int vertex_start = 0;
  int vertex_count = 0;
  int index_start = 0;
  std::vector<uint8_t> constant_buffer;
};

class ResourceDestructor {
 public:
  ResourceDestructor();
  ~ResourceDestructor();

  void Enqueue(Resource* resource, uint64_t frame);
  void Notify(uint64_t frame);
  uint64_t count() const;

 private:
  mutable std::mutex mutex_;
  std::map<uint64_t, std::vector<Resource*>> resources_;
};

class Swapchain {
 public:
  Swapchain(ComPtr<IDXGIFactory7> factory,
      ComPtr<ID3D12CommandQueue> command_queue, HWND hwnd, int width,
      int height, int back_buffer_count, int max_waitable_frames);
  ~Swapchain();

  int GetCurrentBackBufferIndex() const;
  ComPtr<IDXGISwapChain4> GetSwapchain() const { return swapchain_; }
  ComPtr<ID3D12Resource> GetBackBuffer(int index) const {
    return buffers_[index];
  }
  void Wait() const;
  void Present();
  void Resize(int width, int height);

  void UpdateStats();
  nlohmann::json GetStats() const { return stats_; }

  static const DXGI_FORMAT kSwapchainFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
  static const DXGI_COLOR_SPACE_TYPE kSwapchainColorSpace =
      DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;

 private:
  ComPtr<IDXGISwapChain4> swapchain_;
  ComPtr<IDXGIOutput6> output_;
  std::vector<ComPtr<ID3D12Resource>> buffers_;
  DWORD flags_;
  HANDLE waitable_object_;

  nlohmann::json stats_;
};

struct ImGuiPass {
  std::shared_ptr<Resource> imgui_ib;
  std::shared_ptr<Resource> imgui_vb;
  int imgui_ib_size;
  int imgui_vb_size;
};

struct InputLayout {
  float4 pos;
  float2 uv;
};

class Device {
 public:
  Device();
  ~Device();

  void CreateDeviceResources();
  void CreateWindowDependentResources();
  void DestroyResources();
  void SetWindow(HWND window);
  bool Resize(int width, int height);

  void Prepare();
  void Render();
  void Submit(const std::vector<DrawCall>& draw_calls);

  std::shared_ptr<Resource> CreateBuffer(size_t size);
  std::shared_ptr<Resource> CreateDynamicBuffer(size_t size);
  std::shared_ptr<Resource> CreateTexture(
      int width, int height, DXGI_FORMAT format);
  std::shared_ptr<Resource> CreateTextureArray(
      int width, int height, int size, DXGI_FORMAT format);
  std::shared_ptr<Resource> CreateRenderTarget(
      int width, int height, DXGI_FORMAT format);

  nlohmann::json make_rhi_stats() const;
  nlohmann::json make_device_stats() const;

  struct UploadDesc {
    const uint8_t* src;
    int src_pitch;
    int src_width_in_bytes;
    int src_height;
    int dst_x;
    int dst_y;
    int dst_subresource_index;
  };
  void UploadResource2DBatch(
      std::shared_ptr<Resource> dst, const std::vector<UploadDesc>& desc);

 private:
  std::shared_ptr<Resource> createResource(ComPtr<ID3D12Resource> resource,
      ComPtr<D3D12MA::Allocation> allocation, Resource::Type type, size_t size,
      int pitch);
  std::weak_ptr<Resource> queryResource(int resource_id);
  void handleDeviceLost(HRESULT hr);
  void renderImGui(ComPtr<ID3D12GraphicsCommandList> cmdlist);
  void renderImGuiResetContext(ComPtr<ID3D12GraphicsCommandList> cmdlist,
      ImDrawData* dd, ImGuiPass* frame);

 private:
  ComPtr<IDXGIFactory7> factory_;
  ComPtr<IDXGIAdapter3> adapter_;
  ComPtr<ID3D12Device8> d3d_;
  ComPtr<D3D12MA::Allocator> allocator_;
  std::unique_ptr<DebugLayer> debug_layer_;

  int64_t frame_;
  int frame_index_;

  HWND hwnd_;
  int width_;
  int height_;
  std::unique_ptr<Swapchain> swapchain_;

  ComPtr<ID3D12RootSignature> root_signature_;
  ComPtr<ID3D12Resource> depth_stencil_;

  // command allocators, command queues
  std::unique_ptr<CommandList> cmd_list_;
  std::unique_ptr<CommandQueue> render_queue_;
  std::unique_ptr<CommandQueue> copy_queue_;

  // descriptor heaps
  std::unique_ptr<DescriptorHeap> rtv_staging_heap_;
  std::unique_ptr<DescriptorHeap> dsv_staging_heap_;
  std::unique_ptr<DescriptorHeap> srv_staging_heap_;
  std::unique_ptr<DescriptorHeap> sampler_heap_;
  std::array<std::unique_ptr<DescriptorHeap>, 3> srv_heap_;

  // render targets
  std::array<std::shared_ptr<Resource>, 3> main_rt_;
  std::array<std::shared_ptr<Resource>, 3> offscreen_rt_;

  // main render pass
  ComPtr<ID3D12PipelineState> main_pipeline_;
  std::array<std::vector<DrawCall>, 3> main_drawcalls_;

  // offscreen render pass
  ComPtr<ID3D12PipelineState> imgui_pipeline_;
  std::array<ImGuiPass, 3> imgui_pass_;

  // compositor pass
  ComPtr<ID3D12PipelineState> compose_pipeline_;
  std::shared_ptr<Resource> compose_quad_vb_;

  // resource management
  mutable std::mutex mutex_resource_map_;
  std::map<uint64_t, std::weak_ptr<Resource>> resource_map_;
  std::unique_ptr<ResourceDestructor> destructor_;
  std::atomic<uint64_t> resource_id_;
};

}  // namespace gfx

}  // namespace rad
