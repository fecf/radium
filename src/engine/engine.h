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
  Mesh(std::shared_ptr<gfx::Resource> vertex, int vertex_count,
      int vertex_start = 0, std::shared_ptr<gfx::Resource> index = {},
      int index_start = 0)
      : vertex_(vertex),
        vertex_count_(vertex_count),
        vertex_start_(vertex_start),
        index_(index),
        index_start_(index_start) {}
  ~Mesh() {}

  Mesh(const Mesh&) = delete;
  Mesh& operator=(const Mesh&) = delete;
  Mesh(Mesh&&) = default;
  Mesh& operator=(Mesh&&) = default;

  const std::shared_ptr<gfx::Resource> vertex() const { return vertex_; };
  int vertex_count() const { return vertex_count_; }
  int vertex_start() const { return vertex_start_; }
  const std::shared_ptr<gfx::Resource> index() const { return index_; };
  int index_start() const { return index_start_; }

 private:
  std::shared_ptr<gfx::Resource> vertex_;
  int vertex_count_;
  int vertex_start_;

  std::shared_ptr<gfx::Resource> index_;
  int index_start_;
};

struct Texture {
  Texture(std::shared_ptr<gfx::Resource> resource, int width, int height,
      ColorSpace color_space, int array_size = 1, int array_src_width = 0,
      int array_src_height = 0);
  ~Texture() {}

  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;
  Texture(Texture&&) = default;
  Texture& operator=(Texture&&) = default;

  uint64_t id() const;
  const std::shared_ptr<gfx::Resource> resource() const { return resource_; }
  int width() const { return width_; }
  int height() const { return height_; }
  int array_size() const { return array_size_; }
  int array_src_width() const { return array_src_width_; }
  int array_src_height() const { return array_src_height_; }
  ColorSpace color_space() const { return color_space_; }

 private:
  std::shared_ptr<gfx::Resource> resource_;
  int width_;
  int height_;
  int array_size_;
  int array_src_width_;
  int array_src_height_;
  ColorSpace color_space_;
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

  std::unique_ptr<Texture> CreateTexture(const Image* image, bool tiled = false);
  std::unique_ptr<Mesh> CreateMesh();

 private:
  std::unique_ptr<gfx::Device> device_;
  std::unique_ptr<Window> window_;
  std::unique_ptr<Texture> imgui_font_atlas_;
  bool rendering_;
  entt::registry world_;
};

}  // namespace rad

