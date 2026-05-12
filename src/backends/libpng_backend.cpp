#include "openraster_libpng.hpp"

#include "src/core/archive_xml_provider.hpp"

#include <png.h>

#include <string_view>
#include <utility>

namespace ora {

namespace {

class ScopedPngImage {
public:
  ScopedPngImage() {
    image_.version = PNG_IMAGE_VERSION;
  }

  ScopedPngImage(ScopedPngImage const&) = delete;
  auto operator=(ScopedPngImage const&) -> ScopedPngImage& = delete;

  ~ScopedPngImage() {
    ::png_image_free(&image_);
  }

  [[nodiscard]]
  auto get() -> png_image* {
    return &image_;
  }

  [[nodiscard]]
  auto get() const -> png_image const* {
    return &image_;
  }

private:
  png_image image_{};
};

[[nodiscard]]
auto make_libpng_error(Error::Code code, std::string_view message_detail) -> std::unexpected<Error> {
  return detail::make_unexpected(code, "libpng buffer", message_detail);
}

[[nodiscard]]
auto make_libpng_error(Error::Code code, png_image const& image, std::string_view fallback_detail)
    -> std::unexpected<Error> {
  if (image.message[0] != '\0') {
    return make_libpng_error(code, image.message);
  }
  return make_libpng_error(code, fallback_detail);
}

class LibpngOraProvider {
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
    auto image = ScopedPngImage{};
    image.get()->width = width;
    image.get()->height = height;
    image.get()->format = PNG_FORMAT_RGBA;

    auto png_size = png_alloc_size_t{};
    if (::png_image_write_to_memory(
          image.get(),
          nullptr,
          &png_size,
          0,
          rgba.data(),
          0,
          nullptr
        ) == 0) {
      return make_libpng_error(Error::Code::PngEncodeFailed, *image.get(), "failed to size libpng output");
    }

    auto png = std::vector<uint8_t>(static_cast<std::size_t>(png_size));
    if (::png_image_write_to_memory(
          image.get(),
          png.data(),
          &png_size,
          0,
          rgba.data(),
          0,
          nullptr
        ) == 0) {
      return make_libpng_error(Error::Code::PngEncodeFailed, *image.get(), "failed to encode libpng output");
    }

    png.resize(static_cast<std::size_t>(png_size));
    return png;
  }

  auto decode_png(std::span<const uint8_t> data) -> std::expected<DecodedImage, Error> {
    auto image = ScopedPngImage{};
    if (::png_image_begin_read_from_memory(image.get(), data.data(), data.size()) == 0) {
      return make_libpng_error(Error::Code::PngDecodeFailed, *image.get(), "failed to begin libpng decode");
    }

    image.get()->format = PNG_FORMAT_RGBA;
    auto rgba = std::vector<uint8_t>(static_cast<std::size_t>(PNG_IMAGE_SIZE(*image.get())));
    if (::png_image_finish_read(image.get(), nullptr, rgba.data(), 0, nullptr) == 0) {
      return make_libpng_error(Error::Code::PngDecodeFailed, *image.get(), "failed to decode libpng image");
    }

    return DecodedImage{
      .rgba = std::move(rgba),
      .width = image.get()->width,
      .height = image.get()->height
    };
  }

private:
  detail::ArchiveXmlProvider archive_;
};

} // namespace

template auto read<LibpngOraProvider>(LibpngOraProvider&, std::string_view)
    -> std::expected<OraDocument, Error>;
template auto write<LibpngOraProvider>(LibpngOraProvider&, std::string_view, OraDocument const&)
    -> std::expected<void, Error>;
template auto util::encode_png<LibpngOraProvider>(LibpngOraProvider&, ImageBuffer const&)
    -> std::expected<std::vector<uint8_t>, Error>;
template auto util::render_preview_and_thumbnail<LibpngOraProvider>(LibpngOraProvider&, OraDocument&)
    -> std::expected<void, Error>;

namespace libpng {

auto read(std::string_view filename) -> std::expected<OraDocument, Error> {
  auto provider = LibpngOraProvider{};
  return ora::read(provider, filename);
}

auto write(std::string_view filename, OraDocument const& doc) -> std::expected<void, Error> {
  auto provider = LibpngOraProvider{};
  return ora::write(provider, filename, doc);
}

auto encode_png(ImageBuffer const& image) -> std::expected<std::vector<uint8_t>, Error> {
  auto provider = LibpngOraProvider{};
  return ora::util::encode_png(provider, image);
}

auto render_preview_and_thumbnail(OraDocument& doc) -> std::expected<void, Error> {
  auto provider = LibpngOraProvider{};
  return ora::util::render_preview_and_thumbnail(provider, doc);
}

} // namespace libpng

} // namespace ora
