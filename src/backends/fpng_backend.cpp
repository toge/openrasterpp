#include "openraster_fpng.hpp"
#include "src/core/archive_xml_provider.hpp"
#include <fpng.h>
#include <mutex>

namespace ora::fpng {

namespace {

/**
 * @brief fpng を使用した OpenRaster Provider 実装です。
 */
class FpngOraProvider : public detail::ArchiveXmlProvider {
public:
  FpngOraProvider() {
    static std::once_flag init_flag;
    std::call_once(init_flag, []() {
      ::fpng::fpng_init();
    });
  }

  auto encode_png(std::span<const uint8_t> rgba, unsigned int width, unsigned int height)
      -> std::expected<std::vector<uint8_t>, Error> {
    std::vector<uint8_t> png;
    // fpng::fpng_encode_image_to_memory supports 24/32bpp
    if (!::fpng::fpng_encode_image_to_memory(rgba.data(), width, height, 4, png)) {
      return detail::make_unexpected(Error::Code::PngEncodeFailed, "fpng", "failed to encode image");
    }
    return png;
  }

  auto decode_png(std::span<const uint8_t> data) -> std::expected<DecodedImage, Error> {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    std::vector<uint8_t> rgba;

    int const result = ::fpng::fpng_decode_memory(data.data(), static_cast<uint32_t>(data.size()), rgba, width, height, channels, 4);
    if (result != ::fpng::FPNG_DECODE_SUCCESS) {
      return detail::make_unexpected(Error::Code::PngDecodeFailed, "fpng", "error code: " + std::to_string(result));
    }

    return DecodedImage{
      .rgba = std::move(rgba),
      .width = width,
      .height = height
    };
  }
};

} // namespace

template<typename Provider>
auto encode_png_internal(ImageBuffer const& image) -> std::expected<std::vector<uint8_t>, Error> {
  Provider provider;
  return ora::util::encode_png(provider, image);
}

auto encode_png(ImageBuffer const& image) -> std::expected<std::vector<uint8_t>, Error> {
  return encode_png_internal<FpngOraProvider>(image);
}

auto read(std::string_view filename) -> std::expected<OraDocument, Error> {
  FpngOraProvider provider;
  return ora::read(provider, filename);
}

auto write(std::string_view filename, OraDocument const& doc) -> std::expected<void, Error> {
  FpngOraProvider provider;
  return ora::write(provider, filename, doc);
}

auto render_preview_and_thumbnail(OraDocument& doc) -> std::expected<void, Error> {
  FpngOraProvider provider;
  return ora::util::render_preview_and_thumbnail(provider, doc);
}

} // namespace ora::fpng
