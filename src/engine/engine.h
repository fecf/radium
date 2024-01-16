#pragma once

#include <memory>
#include <vector>

#include "window.h"
#include "image/image.h"
#include "engine/shader/primary.h"

#include <linalg.h>
using namespace linalg::aliases;

#include <entt.hpp>

namespace rad {
class Engine;
}
rad::Engine& engine();
entt::registry& world();

namespace rad {

namespace gfx {
class Device;
class Resource;
struct DrawCall;
}  // namespace gfx

constexpr int kTileSize = 2048;

struct Texture {
  uint64_t id() const;
  std::shared_ptr<gfx::Resource> resource;
  int width = 0;
  int height = 0;
  int array_size = 0;
  int array_src_width = 0;
  int array_src_height = 0;
  ColorPrimaries color_primaries = ColorPrimaries::Unknown;
  TransferCharacteristics transfer_characteristics = TransferCharacteristics::Unknown;
};

struct Model {
  std::shared_ptr<gfx::Resource> vertex_buffer;
  std::shared_ptr<gfx::Resource> index_buffer;
  int vertex_count = 0;
  int index_count = 0;
};

struct Material {
  float alpha = 1.0f;
  float4 tint = float4(1.0f, 0.0f, 0.0f, 1.0f);
  std::shared_ptr<Texture> texture;
};

struct Mesh {
  std::shared_ptr<Model> model;
  std::shared_ptr<Material> material;
  bool enabled = true;
  int order = 0;
};

struct Transform {
  float3 translate;
  float3 rotate;
  float3 scale;
};

class Engine {
 public:
  friend entt::registry& ::world();

  Engine();
  ~Engine();

  bool Initialize(const WindowConfig& config);
  void Destroy();
  void Draw();
  bool BeginFrame();
  void EndFrame();
  
  Window* GetWindow() const;
  nlohmann::json GetStats() const;

  std::unique_ptr<Texture> CreateTexture(const Image* image, bool tiled = false);
  std::unique_ptr<Model> CreatePlane();

 private:
  std::unique_ptr<gfx::Device> device_;
  std::unique_ptr<Window> window_;
  entt::registry world_;
  bool rendering_;
};

}  // namespace rad

