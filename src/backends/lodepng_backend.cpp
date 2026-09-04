#include "openraster_lodepng.hpp"

#include "src/core/archive_xml_provider.hpp"

#include "lodepng.h"

#if defined(__wasi__)
// lodepng.h の C API 宣言は C++ リンケージだが実体は C のため、
// C ソースのラッパー経由で呼ぶ（直接呼ぶと undefined symbol になる）。
extern "C" {
auto ora_lodepng_encode32(unsigned char** out, size_t* outsize, const unsigned char* image, unsigned w, unsigned h) -> unsigned;
auto ora_lodepng_decode32(unsigned char** out, unsigned* w, unsigned* h, const unsigned char* in, size_t insize) -> unsigned;
auto ora_lodepng_error_text(unsigned code) -> const char*;
}
#endif

#include <cstdlib>
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
#if defined(__wasi__)
    // WASI では C++ ラッパー（例外参照あり）を避け、C API を使う。
    auto* out = static_cast<unsigned char*>(nullptr);
    auto out_size = static_cast<size_t>(0);
    if (auto const err = ::ora_lodepng_encode32(&out, &out_size, rgba.data(), width, height); err != 0) {
      return detail::make_unexpected(Error::Code::PngEncodeFailed, "buffer", ::ora_lodepng_error_text(err));
    }
    auto png = std::vector<uint8_t>(out, out + out_size);
    std::free(out);
    return png;
#else
    auto png = std::vector<uint8_t>{};
    if (auto const err = ::lodepng::encode(png, rgba.data(), width, height); err != 0) {
      return detail::make_unexpected(Error::Code::PngEncodeFailed, "buffer", ::lodepng_error_text(err));
    }
    return png;
#endif
  }

  auto decode_png(std::span<const uint8_t> data) -> std::expected<DecodedImage, Error> {
#if defined(__wasi__)
    auto image = DecodedImage{};
    auto* out = static_cast<unsigned char*>(nullptr);
    auto w = 0U;
    auto h = 0U;
    if (auto const err = ::ora_lodepng_decode32(&out, &w, &h, data.data(), data.size()); err != 0) {
      return detail::make_unexpected(Error::Code::PngDecodeFailed, "buffer", ::ora_lodepng_error_text(err));
    }
    image.rgba.assign(out, out + static_cast<size_t>(w) * h * 4U);
    std::free(out);
    image.width = w;
    image.height = h;
    return image;
#else
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
#endif
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
