#include "engine.h"

#include "window.h"
#include "base/minlog.h"
#include "base/thread.h"
#include "gfx/device.h"
#include "engine/shader/primary.h"

#include <imgui_impl_win32.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include <DirectXMath.h>

#ifdef IMGUI_VERSION
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

rad::Engine& engine() {
  static rad::Engine engine;
  return engine;
}

flecs::world& world() { return engine().world_; }

namespace rad {

Texture::Texture(std::shared_ptr<gfx::Resource> resource, int width, int height,
    ColorSpace color_space, int array_size, int array_src_width,
    int array_src_height)
    : resource_(resource),
      width_(width),
      height_(height),
      color_space_(color_space),
      array_size_(array_size),
      array_src_width_(array_src_width),
      array_src_height_(array_src_height) {}

uint64_t Texture::id() const {
  return static_cast<uint64_t>(resource_->id);
}

Engine::Engine() : rendering_(false) {
  world_.component<Transform>();
  world_.component<Render>(); 
}

Engine::~Engine() { Destroy(); }

bool Engine::Initialize(const WindowConfig& base_window_config) {
  try {
    device_ = std::unique_ptr<gfx::Device>(new gfx::Device());
  } catch (std::exception& ex) {
    LOG_F(FATAL, "failed to Device::Device() %s", ex.what());
    return false;
  }

  WindowConfig window_config(base_window_config);
  window_ = CreatePlatformWindow(window_config);
  window_->AddEventListener([this](const window_event::window_event_t& data) {
    bool handled = false;
    auto* native = std::get_if<window_event::NativeEvent>(&data);
    if (native) {
      LRESULT ret = ImGui_ImplWin32_WndProcHandler(
          (HWND)native->handle, native->msg, native->wparam, native->lparam);
      handled |= (ret > 0);
    }
    auto* resize = std::get_if<window_event::Resize>(&data);
    if (resize) {
      device_->Resize(resize->width, resize->height);
    }
    return handled;
  });

  if (!window_) {
    LOG_F(FATAL, "failed to create window.");
    return false;
  }
  device_->SetWindow((HWND)window_->GetHandle());
  ImGui_ImplWin32_Init((HWND)window_->GetHandle());

  return true;
}

void Engine::Destroy() {
  imgui_font_atlas_.reset();
  window_.reset();
  device_.reset();
}

Window* Engine::GetWindow() const { return window_.get(); }

std::unique_ptr<Texture> Engine::CreateTexture(std::shared_ptr<Image> image, bool tiled) {
  assert(image);

  DXGI_FORMAT dxgi_format;
  int bpp = 0;
  if (image->format() == ImageFormat::BGRA8) {
    dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM;
    bpp = 4;
  } else if (image->format() == ImageFormat::RGBA16) {
    dxgi_format = DXGI_FORMAT_R16G16B16A16_UNORM;
    bpp = 8;
  } else if (image->format() == ImageFormat::RGBA32F) {
    dxgi_format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    bpp = 16;
  } else if (image->format() == ImageFormat::RGBA8) {
    dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM;
    bpp = 4;
  }

  if (tiled) {
    int tile = kTileSize;
    int rows = (image->height() + tile - 1) / tile;
    int cols = (image->width() + tile - 1) / tile;
    int array_size = rows * cols;

    std::shared_ptr<gfx::Resource> resource =
        device_->CreateTextureArray(tile, tile, array_size, dxgi_format);
    if (!resource) {
      return {};
    }
    resource->resource->SetName(L"tiled_texture");

    std::vector<gfx::Device::UploadDesc> descs;
    for (int y = 0; y < rows; ++y) {
      for (int x = 0; x < cols; ++x) {
        int dst_offset_x = 0;
        int dst_offset_y = 0;
        int src_offset_x = tile * x;
        int src_offset_y = tile * y;
        int src_remain_width = std::max(0, image->width() - src_offset_x);
        int src_remain_height = std::max(0, image->height() - src_offset_y);
        int copy_width = std::max(0, std::min(src_remain_width, tile));
        int copy_height = std::max(0, std::min(src_remain_height, tile));
        assert(src_offset_x >= 0 && src_offset_y >= 0);
        assert(dst_offset_x >= 0 && dst_offset_y >= 0);
        assert(src_remain_width > 0 && src_remain_height > 0);
        assert(copy_width > 0 && copy_height > 0);

        DXGI_FORMAT dxgi_format;
        int bpp = 0;
        if (image->format() == ImageFormat::BGRA8) {
          dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM;
          bpp = 4;
        } else if (image->format() == ImageFormat::RGBA16) {
          dxgi_format = DXGI_FORMAT_R16G16B16A16_UNORM;
          bpp = 8;
        } else if (image->format() == ImageFormat::RGBA32F) {
          dxgi_format = DXGI_FORMAT_R32G32B32A32_FLOAT;
          bpp = 16;
        } else if (image->format() == ImageFormat::RGBA8) {
          dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM;
          bpp = 4;
        } else {
          throw std::runtime_error("unsupported format.");
          return nullptr;
        }
        int width_in_bytes = image->width() * bpp;
        size_t src_offset =
            (src_offset_y * image->stride()) + (src_offset_x * bpp);

        gfx::Device::UploadDesc desc{};
        desc.dst_subresource_index = y * cols + x;
        desc.dst_x = dst_offset_x * bpp;
        desc.dst_y = dst_offset_y;
        desc.src = image->data() + src_offset;
        desc.src_width_in_bytes = copy_width * bpp;
        desc.src_pitch = (int)image->stride();
        desc.src_height = copy_height;
        descs.push_back(desc);
      }
    }
    device_->UploadResource2DBatch(resource, descs);

    return std::make_unique<Texture>(resource, tile, tile, image->colorspace(), array_size, image->width(), image->height());
  } else {
    std::shared_ptr<gfx::Resource> resource =
        device_->CreateTexture(image->width(), image->height(), dxgi_format);
    if (!resource) {
      return nullptr;
    }
    resource->resource->SetName(L"texture");

    gfx::Device::UploadDesc desc{};
    desc.dst_subresource_index = 0;
    desc.dst_x = 0;
    desc.dst_y = 0;
    desc.src = image->data();
    desc.src_width_in_bytes = image->width() * bpp;
    desc.src_pitch = (int)image->stride();
    desc.src_height = image->height();
    device_->UploadResource2DBatch(resource, {desc});

    return std::make_unique<Texture>(resource, image->width(), image->height(), image->colorspace());
  }
}

std::unique_ptr<Mesh> Engine::CreateMesh() {
  const float x = 0.5f;
  const float y = 0.5f;
  const float z = 0.5f;

  std::vector<gfx::InputLayout> data(6);
  data[0].pos = {-x, +y, z, 0.0f};
  data[1].pos = {+x, -y, z, 0.0f};
  data[2].pos = {-x, -y, z, 0.0f};
  data[0].uv = {0.0f, 0.0f};
  data[1].uv = {1.0f, 1.0f};
  data[2].uv = {0.0f, 1.0f};
  data[3].pos = {+x, -y, z, 0.0f};
  data[4].pos = {-x, +y, z, 0.0f};
  data[5].pos = {+x, +y, z, 0.0f};
  data[3].uv = {1.0f, 1.0f};
  data[4].uv = {0.0f, 0.0f};
  data[5].uv = {1.0f, 0.0f};

  std::shared_ptr<gfx::Resource> vb = device_->CreateDynamicBuffer(
      sizeof(gfx::InputLayout) * data.size());
  vb->Upload(data.data(), sizeof(gfx::InputLayout) * data.size());
  return std::make_unique<Mesh>(vb, 6); 
}

void Engine::Draw() {
  using namespace linalg;

  Window::Rect rect = window_->GetClientRect();
  float4 viewport{0, 0, (float)rect.width, (float)rect.height};
  int4 scissor{0, 0, rect.width, rect.height};

  std::vector<gfx::DrawCall> drawcalls;

  static auto q = world_.query_builder<const Render>()
               .order_by<Render>(
                   [](flecs::entity_t e1, const Render* a, flecs::entity_t e2,
                       const Render* b) { return (a->priority - b->priority); })
               .build();
  q.each([&](flecs::entity_t e, const Render& re) {
    using namespace linalg;

    gfx::DrawCall dc{};
    dc.viewport = viewport;
    dc.scissor = scissor;

    auto* tf = world_.entity(e).get<Transform>();
    float4x4 v = identity;
    float3 t = tf ? tf->translate : float3(0, 0, 0);
    float3 r = tf ? tf->rotate : float3(0, 0, 0);
    float3 s = tf ? tf->scale : float3(1, 1, 1);
    if (tf) {
      if (any(t)) v = mul(v, translation_matrix(t));
      if (any(r)) {
        auto rx = rotation_quat(float3{1.0f, 0.0f, 0.0f}, (float)(r.x * M_PI / 180.0f));
        auto ry = rotation_quat(float3{0.0f, 1.0f, 0.0f}, (float)(r.y * M_PI / 180.0f));
        auto rz = rotation_quat(float3{0.0f, 0.0f, 1.0f}, (float)(r.z * M_PI / 180.0f));
        v = mul(v, rotation_matrix(rx));
        v = mul(v, rotation_matrix(ry));
        v = mul(v, rotation_matrix(rz));
      }
      if (any(s)) v = mul(v, scaling_matrix(s));
    }

    const float L = -rect.width / 2.0f;
    const float R = rect.width / 2.0f;
    const float T = rect.height / 2.0f;
    const float B = -rect.height / 2.0f;
    float4x4 p{
        {2.0f / (R - L), 0.0f, 0.0f, 0.0f},
        {0.0f, 2.0f / (T - B), 0.0f, 0.0f},
        {0.0f, 0.0f, 0.5f, 0.0f},
        {(R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f},
    };

    float4x4 mvp = identity;
    mvp = mul(mvp, p);
    mvp = mul(mvp, v);
    dc.mvp = mvp;

    if (re.mesh) {
      dc.vertex_buffer = re.mesh->vertex();
      dc.vertex_count  = re.mesh->vertex_count();
      dc.vertex_start  = re.mesh->vertex_start();
      dc.index_buffer  = re.mesh->index();
      dc.index_start   = re.mesh->index_start();

      if (re.texture) {
        dc.shader_resource  = re.texture->resource();
        dc.array_src_width  = re.texture->array_src_width();
        dc.array_src_height = re.texture->array_src_height();

        Constants constants{};
        constants.alpha = re.alpha;
        constants.filter = ::Filter::Bilinear;
        constants.cs_src = ::ColorSpace::sRGB;
        constants.cs_dst = ::ColorSpace::Linear;
        dc.constant_buffer.resize(sizeof(constants));
        memcpy(dc.constant_buffer.data(), &constants, sizeof(constants));
      }
    }

    drawcalls.emplace_back(dc);
  });

  device_->Submit(drawcalls);
}

bool Engine::BeginFrame() {
  if (rendering_) {
    return true;
  }
  rendering_ = true;

  if (!window_->Update()) {
    rendering_ = false;
    return false;
  }

  device_->Prepare();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
  return true;
}

void Engine::EndFrame() {
  ImGui::Render();
  device_->Render();
  rendering_ = false;
}

nlohmann::json Engine::make_stats() const {
  nlohmann::json json{
      {
          "engine",
          {
          },
      },
      {
          "rhi", device_->make_rhi_stats(),
      },
      {
          "device", device_->make_device_stats(),
      },
  };
  return json;
}

}  // namespace rad
