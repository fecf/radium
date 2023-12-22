#include "base/io.h"
#include "base/minlog.h"
#include "base/thread.h"
#include "nvjpeg_rw.h"

#include <nvjpeg.h>
#pragma comment(lib, "cuda.lib")
#pragma comment(lib, "nvjpeg.lib")

#define check(err)                      \
  if ((err) != NVJPEG_STATUS_SUCCESS) { \
    LOG_F(FATAL, "%d", err); \
return {}; \
  }

namespace rad {

std::unique_ptr<Image> nvJPEGRW::Read(const uint8_t* data, size_t size) {
  cudaStream_t stream = 0;
  nvjpegHandle_t handle;
  check(nvjpegCreateSimple(&handle));

  nvjpegJpegState_t jpeg_handle;
  check(nvjpegJpegStateCreate(handle,&jpeg_handle));

  int ws[4], hs[4], comps[4];
  nvjpegChromaSubsampling_t subsampling;
  nvjpegGetImageInfo(handle, data, size, comps, &subsampling, ws, hs);
  int w = ws[0];
  int h = hs[0];

  nvjpegImage_t destination{};
  cudaMalloc((void**)&destination.channel[0], w * h * 4);
  destination.pitch[0] = w * 4;
  check(nvjpegDecode(handle, jpeg_handle, data, size, NVJPEG_OUTPUT_RGBI, &destination, 0));

  ::cudaStreamSynchronize(0);

  std::vector<uint8_t> buf(w * h * 4, 0);
  cudaMemcpy(buf.data(), destination.channel[0], w * h * 4, cudaMemcpyDefault);
  ::cudaFree(destination.channel[0]);

  const uint8_t* src = buf.data();
  size_t stride = destination.pitch[0];

  std::unique_ptr<Image> image(
      new Image(w, h, w * 4, ImageFormat::RGBA8, 4, ColorSpace::sRGB));
  ::memset(image->data(), 0, w * h * 4);

  uint8_t* dst = image->data();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      uint8_t* dst = image->data() + y * w * 4;
      uint8_t* src = buf.data() + y * stride;
      dst[x * 4 + 0] = 255;
      dst[x * 4 + 1] = src[x * 3 + 1];
      dst[x * 4 + 2] = src[x * 3 + 2];
      dst[x * 4 + 3] = 255;
    }
  }

  return image;
}

}  // namespace rad
