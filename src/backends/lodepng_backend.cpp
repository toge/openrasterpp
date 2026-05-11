#include "openraster_lodepng.hpp"

#include "src/core/archive_xml_provider.hpp"

#include "lodepng.h"

#include <utility>

namespace ora {

namespace {

class LodepngOraProvider {
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
    auto png = std::vector<uint8_t>{};
    if (auto const err = ::lodepng::encode(png, rgba.data(), width, height); err != 0) {
      return detail::make_unexpected(Error::Code::PngEncodeFailed, "buffer", ::lodepng_error_text(err));
    }
    return png;
  }

  auto decode_png(std::span<const uint8_t> data) -> std::expected<DecodedImage, Error> {
    auto image = DecodedImage{};
    if (auto const err = ::lodepng::decode(
          image.rgba,
          image.width,
          image.height,
          data.data(),
          data.size()
        ); err != 0) {
      return detail::make_unexpected(Error::Code::PngDecodeFailed, "buffer", ::lodepng_error_text(err));
    }
    return image;
  }

private:
  detail::ArchiveXmlProvider archive_;
};

} // namespace

template auto read<LodepngOraProvider>(LodepngOraProvider&, std::string_view)
    -> std::expected<OraDocument, Error>;
template auto write<LodepngOraProvider>(LodepngOraProvider&, std::string_view, OraDocument const&)
    -> std::expected<void, Error>;
template auto util::encode_png<LodepngOraProvider>(LodepngOraProvider&, ImageBuffer const&)
    -> std::expected<std::vector<uint8_t>, Error>;
template auto util::render_preview_and_thumbnail<LodepngOraProvider>(LodepngOraProvider&, OraDocument&)
    -> std::expected<void, Error>;

namespace lodepng {

auto read(std::string_view filename) -> std::expected<OraDocument, Error> {
  auto provider = LodepngOraProvider{};
  return ora::read(provider, filename);
}

auto write(std::string_view filename, OraDocument const& doc) -> std::expected<void, Error> {
  auto provider = LodepngOraProvider{};
  return ora::write(provider, filename, doc);
}

auto encode_png(ImageBuffer const& image) -> std::expected<std::vector<uint8_t>, Error> {
  auto provider = LodepngOraProvider{};
  return ora::util::encode_png(provider, image);
}

auto render_preview_and_thumbnail(OraDocument& doc) -> std::expected<void, Error> {
  auto provider = LodepngOraProvider{};
  return ora::util::render_preview_and_thumbnail(provider, doc);
}

} // namespace lodepng

} // namespace ora
