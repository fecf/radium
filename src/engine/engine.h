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
}  // namespace gfx

constexpr int kTileSize = 2048;

struct Mesh {
  Mesh(std::shared_ptr<gfx::Resource> vertex_buffer, int vertex_count,
      int vertex_start = 0, std::shared_ptr<gfx::Resource> index_buffer = {},
      int index_start = 0);

  std::shared_ptr<gfx::Resource> vertex_buffer;
  int vertex_count;
  int vertex_start;
  std::shared_ptr<gfx::Resource> index_buffer;
  int index_start;
};

struct Texture {
  Texture(std::shared_ptr<gfx::Resource> resource, int width, int height,
      ColorSpace color_space, int array_size = 1, int array_src_width = 0,
      int array_src_height = 0);

  uint64_t id() const;

  std::shared_ptr<gfx::Resource> resource;
  int width;
  int height;
  int array_size;
  int array_src_width;
  int array_src_height;
  ColorSpace color_space;
};

struct Transform {
  float3 translate;
  float3 rotate;
  float3 scale;
};

struct Render {
  int priority = 0;
  bool bypass = false;

  // mesh
  std::shared_ptr<Mesh> mesh;

  // material
  float alpha = 1.0f;
  float4 color = float4(1.0f, 0.0f, 0.0f, 1.0f);
  std::shared_ptr<Texture> texture;
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
  std::unique_ptr<Mesh> CreateMesh();

 private:
  std::unique_ptr<gfx::Device> device_;
  std::unique_ptr<Window> window_;
  entt::registry world_;
  bool rendering_;
};

}  // namespace rad

