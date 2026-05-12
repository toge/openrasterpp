#include "openraster_stb.hpp"

#include "src/core/archive_xml_provider.hpp"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace ora {

namespace {

[[nodiscard]]
auto make_stb_error(Error::Code code, std::string_view detail) -> std::unexpected<Error> {
  auto const message = detail.empty() ? "unknown stb failure" : detail;
  return detail::make_unexpected(code, "stb buffer", message);
}

class StbOraProvider {
public:
  auto open_archive(std::string_view path, ArchiveMode mode) -> std::expected<void, Error> {
    return archive_.open_archive(path, mode);
  }

  auto close_archive() -> void {
    archive_.close_archive();
  }

  auto read_entry(std::string_view path) -> std::expected<std::vector<uint8_t>, Error> {
    return archive_.read_entry(path);
  }

  auto write_entry(std::string_view path, std::span<const uint8_t> data, CompressionLevel level)
      -> std::expected<void, Error> {
    return archive_.write_entry(path, data, level);
  }

  auto serialize_stack(OraDocument const& doc) -> std::string {
    return archive_.serialize_stack(doc);
  }

  auto deserialize_stack(std::span<const uint8_t> xml_bytes) -> std::expected<OraDocument, Error> {
    return archive_.deserialize_stack(xml_bytes);
  }

  auto encode_png(std::span<const uint8_t> rgba, unsigned int width, unsigned int height)
      -> std::expected<std::vector<uint8_t>, Error> {
    auto constexpr max_int = static_cast<unsigned int>(std::numeric_limits<int>::max());
    if (width > max_int || height > max_int || width > max_int / 4U) {
      return make_stb_error(Error::Code::PngEncodeFailed, "image dimensions exceed stb limits");
    }

    auto png = std::vector<uint8_t>{};
    auto const write_png_bytes = [](void* context, void* data, int size) {
      auto& buffer = *static_cast<std::vector<uint8_t>*>(context);
      auto const* bytes = static_cast<uint8_t const*>(data);
      buffer.insert(buffer.end(), bytes, bytes + size);
    };

    if (::stbi_write_png_to_func(
          write_png_bytes,
          &png,
          static_cast<int>(width),
          static_cast<int>(height),
          4,
          rgba.data(),
          static_cast<int>(width * 4U)
        ) == 0) {
      return make_stb_error(Error::Code::PngEncodeFailed, "failed to encode stb output");
    }

    return png;
  }

  auto decode_png(std::span<const uint8_t> data) -> std::expected<DecodedImage, Error> {
    auto constexpr max_int = static_cast<std::size_t>(std::numeric_limits<int>::max());
    if (data.size() > max_int) {
      return make_stb_error(Error::Code::PngDecodeFailed, "image payload exceeds stb limits");
    }

    auto width = 0;
    auto height = 0;
    auto channels = 0;
    auto rgba_ = std::unique_ptr<stbi_uc, decltype(&::stbi_image_free)>{
      ::stbi_load_from_memory(
        data.data(),
        static_cast<int>(data.size()),
        &width,
        &height,
        &channels,
        STBI_rgb_alpha
      ),
      ::stbi_image_free
    };
    if (!rgba_) {
      return make_stb_error(Error::Code::PngDecodeFailed, ::stbi_failure_reason());
    }

    auto const rgba_size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U;
    return DecodedImage{
      .rgba = std::vector<uint8_t>(rgba_.get(), rgba_.get() + rgba_size),
      .width = static_cast<unsigned int>(width),
      .height = static_cast<unsigned int>(height)
    };
  }

private:
  detail::ArchiveXmlProvider archive_;
};

} // namespace

template auto read<StbOraProvider>(StbOraProvider&, std::string_view)
    -> std::expected<OraDocument, Error>;
template auto write<StbOraProvider>(StbOraProvider&, std::string_view, OraDocument const&)
    -> std::expected<void, Error>;
template auto util::encode_png<StbOraProvider>(StbOraProvider&, ImageBuffer const&)
    -> std::expected<std::vector<uint8_t>, Error>;
template auto util::render_preview_and_thumbnail<StbOraProvider>(StbOraProvider&, OraDocument&)
    -> std::expected<void, Error>;

namespace stb {

auto read(std::string_view filename) -> std::expected<OraDocument, Error> {
  auto provider = StbOraProvider{};
  return ora::read(provider, filename);
}

auto write(std::string_view filename, OraDocument const& doc) -> std::expected<void, Error> {
  auto provider = StbOraProvider{};
  return ora::write(provider, filename, doc);
}

auto encode_png(ImageBuffer const& image) -> std::expected<std::vector<uint8_t>, Error> {
  auto provider = StbOraProvider{};
  return ora::util::encode_png(provider, image);
}

auto render_preview_and_thumbnail(OraDocument& doc) -> std::expected<void, Error> {
  auto provider = StbOraProvider{};
  return ora::util::render_preview_and_thumbnail(provider, doc);
}

} // namespace stb

} // namespace ora
