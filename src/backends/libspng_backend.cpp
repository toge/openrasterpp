#include "openraster_libspng.hpp"

#include "src/core/archive_xml_provider.hpp"

#include <spng.h>

#include <cstdlib>
#include <memory>
#include <utility>

namespace ora {

namespace {

[[nodiscard]]
auto make_libspng_error(Error::Code code, std::string_view message_detail) -> std::unexpected<Error> {
  return detail::make_unexpected(code, "libspng buffer", message_detail);
}

class LibspngOraProvider {
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
    auto ctx_ = std::unique_ptr<spng_ctx, decltype(&::spng_ctx_free)>{
      ::spng_ctx_new(SPNG_CTX_ENCODER),
      ::spng_ctx_free
    };
    if (!ctx_) {
      return make_libspng_error(Error::Code::PngEncodeFailed, "failed to create encoder context");
    }

    if (auto const err = ::spng_set_option(ctx_.get(), SPNG_ENCODE_TO_BUFFER, 1); err != 0) {
      return make_libspng_error(Error::Code::PngEncodeFailed, ::spng_strerror(err));
    }

    auto ihdr = spng_ihdr{
      .width = width,
      .height = height,
      .bit_depth = 8,
      .color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA,
      .compression_method = 0,
      .filter_method = 0,
      .interlace_method = 0
    };
    if (auto const err = ::spng_set_ihdr(ctx_.get(), &ihdr); err != 0) {
      return make_libspng_error(Error::Code::PngEncodeFailed, ::spng_strerror(err));
    }

    if (auto const err = ::spng_encode_image(
          ctx_.get(),
          const_cast<uint8_t*>(rgba.data()),
          rgba.size(),
          SPNG_FMT_RGBA8,
          SPNG_ENCODE_FINALIZE
        ); err != 0) {
      return make_libspng_error(Error::Code::PngEncodeFailed, ::spng_strerror(err));
    }

    auto png_size = size_t{};
    auto png_error = 0;
    auto png_ = std::unique_ptr<unsigned char, decltype(&std::free)>{
      static_cast<unsigned char*>(::spng_get_png_buffer(ctx_.get(), &png_size, &png_error)),
      &std::free
    };
    if (!png_) {
      return make_libspng_error(Error::Code::PngEncodeFailed, ::spng_strerror(png_error));
    }

    return std::vector<uint8_t>(png_.get(), png_.get() + png_size);
  }

  auto decode_png(std::span<const uint8_t> data) -> std::expected<DecodedImage, Error> {
    auto ctx_ = std::unique_ptr<spng_ctx, decltype(&::spng_ctx_free)>{
      ::spng_ctx_new(0),
      ::spng_ctx_free
    };
    if (!ctx_) {
      return make_libspng_error(Error::Code::PngDecodeFailed, "failed to create decoder context");
    }

    if (auto const err = ::spng_set_png_buffer(ctx_.get(), data.data(), data.size()); err != 0) {
      return make_libspng_error(Error::Code::PngDecodeFailed, ::spng_strerror(err));
    }

    auto ihdr = spng_ihdr{};
    if (auto const err = ::spng_get_ihdr(ctx_.get(), &ihdr); err != 0) {
      return make_libspng_error(Error::Code::PngDecodeFailed, ::spng_strerror(err));
    }

    auto decoded_size = size_t{};
    if (auto const err = ::spng_decoded_image_size(ctx_.get(), SPNG_FMT_RGBA8, &decoded_size); err != 0) {
      return make_libspng_error(Error::Code::PngDecodeFailed, ::spng_strerror(err));
    }

    auto rgba = std::vector<uint8_t>(decoded_size);
    if (auto const err = ::spng_decode_image(
          ctx_.get(),
          rgba.data(),
          rgba.size(),
          SPNG_FMT_RGBA8,
          0
        ); err != 0) {
      return make_libspng_error(Error::Code::PngDecodeFailed, ::spng_strerror(err));
    }

    return DecodedImage{
      .rgba = std::move(rgba),
      .width = ihdr.width,
      .height = ihdr.height
    };
  }

private:
  detail::ArchiveXmlProvider archive_;
};

} // namespace

template auto read<LibspngOraProvider>(LibspngOraProvider&, std::string_view)
    -> std::expected<OraDocument, Error>;
template auto write<LibspngOraProvider>(LibspngOraProvider&, std::string_view, OraDocument const&)
    -> std::expected<void, Error>;
template auto util::encode_png<LibspngOraProvider>(LibspngOraProvider&, ImageBuffer const&)
    -> std::expected<std::vector<uint8_t>, Error>;
template auto util::render_preview_and_thumbnail<LibspngOraProvider>(LibspngOraProvider&, OraDocument&)
    -> std::expected<void, Error>;

namespace libspng {

auto read(std::string_view filename) -> std::expected<OraDocument, Error> {
  auto provider = LibspngOraProvider{};
  return ora::read(provider, filename);
}

auto write(std::string_view filename, OraDocument const& doc) -> std::expected<void, Error> {
  auto provider = LibspngOraProvider{};
  return ora::write(provider, filename, doc);
}

auto encode_png(ImageBuffer const& image) -> std::expected<std::vector<uint8_t>, Error> {
  auto provider = LibspngOraProvider{};
  return ora::util::encode_png(provider, image);
}

auto render_preview_and_thumbnail(OraDocument& doc) -> std::expected<void, Error> {
  auto provider = LibspngOraProvider{};
  return ora::util::render_preview_and_thumbnail(provider, doc);
}

} // namespace libspng

} // namespace ora
