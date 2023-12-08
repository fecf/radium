#include "wic_rw.h"

#include "base/io.h"
#include "base/minlog.h"
#include "base/text.h"
#include "base/platform.h"

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shlwapi.lib")
#include <Shlwapi.h>
#include <comdef.h>
#include <wincodecsdk.h>

using namespace rad;

void throw_com_error(HRESULT hr, const char* file, int line) {
  if (FAILED(hr)) {
    _com_error err(hr);
    ::OutputDebugStringW(err.ErrorMessage());
    ::OutputDebugStringW(L"\n");
    throw std::runtime_error(to_string(err.ErrorMessage()));
  }
}

#define CHECK(hr) throw_com_error(hr, __FILE__, __LINE__)

using namespace Microsoft::WRL;

namespace rad {

std::unique_ptr<Image> WicRW::Read(const uint8_t* data, size_t size) {
  try {
    ComPtr<IStream> filestream = ::SHCreateMemStream(data, (UINT)size);
    if (!filestream) {
      return {};
    }

    ComPtr<IWICImagingFactory2> factory;
    CHECK(::CoCreateInstance(CLSID_WICImagingFactory2, NULL,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));

    ComPtr<IWICStream> stream;
    CHECK(factory->CreateStream(stream.GetAddressOf()));
    CHECK(filestream->Seek({}, STREAM_SEEK_SET, NULL));
    CHECK(stream->InitializeFromIStream(filestream.Get()));

    ComPtr<IWICBitmapDecoder> decoder;
    CHECK(factory->CreateDecoderFromStream(
        stream.Get(), NULL, WICDecodeMetadataCacheOnLoad, &decoder));

    UINT frame_count;
    CHECK(decoder->GetFrameCount(&frame_count));
    if (frame_count == 0) {
      return {};
    }

    ComPtr<IWICBitmapFrameDecode> bitmap_frame;
    CHECK(decoder->GetFrame(0, &bitmap_frame));

    // get channels, bit depth
    WICPixelFormatGUID pixel_format_guid;
    CHECK(bitmap_frame->GetPixelFormat(&pixel_format_guid));
    ComPtr<IWICComponentInfo> component_info;
    CHECK(factory->CreateComponentInfo(pixel_format_guid, &component_info));
    ComPtr<IWICPixelFormatInfo2> pixel_format_info;
    CHECK(component_info.As(&pixel_format_info));

    UINT channels, bpp;
    CHECK(pixel_format_info->GetChannelCount(&channels));
    CHECK(pixel_format_info->GetBitsPerPixel(&bpp));

    ImageFormat format = ImageFormat::RGBA8;
    ColorSpace cs = ColorSpace::sRGB;

    ComPtr<IWICFormatConverter> converter;
    CHECK(factory->CreateFormatConverter(&converter));
    if (bpp > 32) {
      CHECK(converter->Initialize(bitmap_frame.Get(),
          GUID_WICPixelFormat128bppRGBAFloat, WICBitmapDitherTypeNone, nullptr,
          0.0f, WICBitmapPaletteTypeCustom));
      format = ImageFormat::RGBA32F;
      cs = ColorSpace::Linear;
    } else {
      CHECK(converter->Initialize(bitmap_frame.Get(),
          GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0f,
          WICBitmapPaletteTypeCustom));
    }

    ComPtr<IWICBitmap> bitmap;
    CHECK(factory->CreateBitmapFromSource(
        converter.Get(), WICBitmapNoCache, &bitmap));

    UINT w, h;
    UINT stride;
    {
      ComPtr<IWICBitmapLock> lock;
      CHECK(bitmap->Lock(NULL, WICBitmapLockRead, &lock));
      CHECK(lock->GetSize(&w, &h));
      CHECK(lock->GetStride(&stride));
    }

    WICRect rect{};
    rect.Width = w;
    rect.Height = h;
    uint8_t* buf = new uint8_t[stride * h];
    CHECK(bitmap->CopyPixels(&rect, stride, stride * h, buf));

    return std::unique_ptr<Image>(new Image(w, h, stride, format, channels, cs,
        buf, [](void* ptr) { delete[] ptr; }));
  } catch (std::exception&) {
    return {};
  }
}

}  // namespace rad
