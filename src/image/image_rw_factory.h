#pragma once

#include <memory>
#include <string>
#include <vector>

namespace rad {

class ImageDecoder;

class IImageRWDefine {
 public:
  virtual ~IImageRWDefine();
  virtual const std::string& name() const = 0;
  virtual const std::vector<std::string>& extensions() const = 0;
  virtual std::unique_ptr<ImageDecoder> create() const = 0;
};

template <typename imagerw_t>
class ImageRWDefine : public IImageRWDefine {
 public:
  ImageRWDefine() = delete;
  ImageRWDefine(const std::string& name, const std::vector<std::string>& extensions) : name_(name), extensions_(extensions) {}
  virtual ~ImageRWDefine() {}

  const std::string& name() const override { return name_; }
  const std::vector<std::string>& extensions() const override { return extensions_; };
  std::unique_ptr<ImageDecoder> create() const override { return std::unique_ptr<ImageDecoder>(new imagerw_t()); }

 private:
  std::string name_;
  std::vector<std::string> extensions_;
};

class ImageRWFactory {
 public:
  ImageRWFactory();
  ImageRWFactory(const ImageRWFactory&) = delete;
  ImageRWFactory& operator=(const ImageRWFactory&) = delete;

  std::unique_ptr<ImageDecoder> CreatePreferredImageRW(const std::string& path);
  std::vector<std::string> GetSupportedExtensions();
  const std::vector<std::unique_ptr<IImageRWDefine>>& GetImageRWDefines() { return defines_; }

 private:
  std::vector<std::unique_ptr<IImageRWDefine>> defines_;
};

}  // namespace rad
