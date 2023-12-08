#include "wuffs_rw.h"

#include "base/io.h"

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__STATIC_FUNCTIONS
#include <wuffs-unsupported-snapshot.c>

namespace rad {

std::unique_ptr<Image> WuffsRW::Read(const uint8_t* data, size_t size) {
  class DecodeImageCallbacks : public wuffs_aux::DecodeImageCallbacks {
   private:
    std::string HandleMetadata(const wuffs_base__more_information& minfo,
        wuffs_base__slice_u8 raw) override {
      return wuffs_aux::DecodeImageCallbacks::HandleMetadata(minfo, raw);
    }

    void Done(wuffs_aux::DecodeImageResult& result,
        wuffs_aux::sync_io::Input& input, wuffs_aux::IOBuffer& buffer,
        wuffs_base__image_decoder::unique_ptr image_decoder) override {
      return wuffs_aux::DecodeImageCallbacks::Done(
          result, input, buffer, std::move(image_decoder));
    }
  };
  DecodeImageCallbacks callbacks;

  wuffs_aux::sync_io::MemoryInput input(data, size);
  wuffs_aux::DecodeImageResult res = wuffs_aux::DecodeImage(callbacks, input);

  int width = res.pixbuf.pixcfg.width();
  int height = res.pixbuf.pixcfg.height();
  uint8_t* ptr = res.pixbuf.plane(0).ptr;
  size_t stride = res.pixbuf.plane(0).stride;

  std::unique_ptr<Image> image(
      new Image(width, height, stride, ImageFormat::RGBA8, 4, ColorSpace::sRGB,
          ptr, [](void* ptr) { delete ptr; }));

  res.pixbuf_mem_owner.release();

  return image;
}

}  // namespace rad
