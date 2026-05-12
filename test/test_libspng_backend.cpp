#include "catch2/catch_test_macros.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <spng.h>

#include "openraster_libspng.hpp"

namespace {

struct DecodedPng {
  unsigned int width;
  unsigned int height;
  std::vector<uint8_t> rgba;
};

auto reference_rgba() -> std::array<uint8_t, 16> {
  return {
    255, 64, 32, 255,
    0, 128, 255, 200,
    10, 20, 30, 40,
    250, 251, 252, 1
  };
}

auto decode_png_rgba(std::span<const uint8_t> png_bytes) -> DecodedPng {
  auto ctx = std::unique_ptr<spng_ctx, decltype(&::spng_ctx_free)>{
    ::spng_ctx_new(0),
    ::spng_ctx_free
  };
  REQUIRE(ctx != nullptr);
  REQUIRE(::spng_set_png_buffer(ctx.get(), png_bytes.data(), png_bytes.size()) == 0);

  auto ihdr = spng_ihdr{};
  REQUIRE(::spng_get_ihdr(ctx.get(), &ihdr) == 0);

  auto decoded_size = size_t{};
  REQUIRE(::spng_decoded_image_size(ctx.get(), SPNG_FMT_RGBA8, &decoded_size) == 0);

  auto rgba = std::vector<uint8_t>(decoded_size);
  REQUIRE(::spng_decode_image(ctx.get(), rgba.data(), rgba.size(), SPNG_FMT_RGBA8, 0) == 0);

  return DecodedPng{
    .width = ihdr.width,
    .height = ihdr.height,
    .rgba = std::move(rgba)
  };
}

auto make_reference_image() -> ora::ImageBuffer {
  auto image = ora::util::blank_image(2, 2, 0);
  REQUIRE(image.has_value());

  auto const expected = reference_rgba();
  std::ranges::copy(expected, image->rgba_mut().begin());

  return std::move(*image);
}

auto make_document() -> ora::OraDocument {
  auto layer = make_reference_image();

  auto layer_png = ora::libspng::encode_png(layer);
  REQUIRE(layer_png.has_value());

  return ora::OraDocument{
    .width = 2,
    .height = 2,
    .root_nodes = {ora::layer("layer-1")},
    .layer_images = {{"layer-1", std::move(*layer_png)}},
    .merged_image_png = std::nullopt,
    .thumbnail_png = std::nullopt
  };
}

auto make_invalid_document() -> ora::OraDocument {
  return ora::OraDocument{
    .width = 2,
    .height = 1,
    .root_nodes = {ora::layer("layer-1")},
    .layer_images = {{"layer-1", {0x00, 0x01, 0x02, 0x03}}},
    .merged_image_png = std::nullopt,
    .thumbnail_png = std::nullopt
  };
}

auto with_preview_assets(ora::OraDocument doc) -> ora::OraDocument {
  auto valid_doc = make_document();
  REQUIRE(ora::libspng::render_preview_and_thumbnail(valid_doc).has_value());
  doc.merged_image_png = valid_doc.merged_image_png;
  doc.thumbnail_png = valid_doc.thumbnail_png;
  return doc;
}

} // namespace

TEST_CASE("libspng backend encodes image buffers", "[libspng-backend]") {
  auto image = make_reference_image();

  auto png = ora::libspng::encode_png(image);

  REQUIRE(png.has_value());
  REQUIRE(png->size() >= 8U);
  CHECK((*png)[0] == 0x89);
  CHECK((*png)[1] == 0x50);
  CHECK((*png)[2] == 0x4e);
  CHECK((*png)[3] == 0x47);

  auto const decoded = decode_png_rgba(*png);
  auto const expected = reference_rgba();
  CHECK(decoded.width == 2);
  CHECK(decoded.height == 2);
  CHECK(decoded.rgba[0] == expected[0]);
  CHECK(decoded.rgba[1] == expected[1]);
  CHECK(decoded.rgba[2] == expected[2]);
  CHECK(decoded.rgba[3] == expected[3]);
  CHECK(decoded.rgba[12] == expected[12]);
  CHECK(decoded.rgba[13] == expected[13]);
  CHECK(decoded.rgba[14] == expected[14]);
  CHECK(decoded.rgba[15] == expected[15]);
  CHECK(decoded.rgba == std::vector<uint8_t>(expected.begin(), expected.end()));
}

TEST_CASE("libspng backend renders preview assets", "[libspng-backend]") {
  auto doc = make_document();

  auto result = ora::libspng::render_preview_and_thumbnail(doc);

  REQUIRE(result.has_value());
  REQUIRE(doc.merged_image_png.has_value());
  REQUIRE(doc.thumbnail_png.has_value());
  CHECK(doc.merged_image_png->size() >= 8U);
  CHECK(doc.thumbnail_png->size() >= 8U);

  auto const expected = reference_rgba();
  auto const merged = decode_png_rgba(*doc.merged_image_png);
  CHECK(merged.width == 2);
  CHECK(merged.height == 2);
  CHECK(merged.rgba == std::vector<uint8_t>(expected.begin(), expected.end()));

  auto const thumbnail = decode_png_rgba(*doc.thumbnail_png);
  CHECK(thumbnail.width == 2);
  CHECK(thumbnail.height == 2);
  CHECK(thumbnail.rgba == std::vector<uint8_t>(expected.begin(), expected.end()));
}

TEST_CASE("libspng backend round-trips read/write", "[libspng-backend]") {
  auto doc = make_document();
  REQUIRE(ora::libspng::render_preview_and_thumbnail(doc).has_value());

  auto const path = std::filesystem::path{"test-libspng-roundtrip.ora"};
  std::ignore = std::filesystem::remove(path);

  auto write_result = ora::libspng::write(path.string(), doc);
  REQUIRE(write_result.has_value());

  auto read_result = ora::libspng::read(path.string());
  REQUIRE(read_result.has_value());
  CHECK(read_result->width == 2);
  CHECK(read_result->height == 2);
  CHECK(read_result->layer_images.contains("layer-1"));

  auto const decoded = decode_png_rgba(read_result->layer_images.at("layer-1"));
  auto const expected = reference_rgba();
  CHECK(decoded.width == 2);
  CHECK(decoded.height == 2);
  CHECK(decoded.rgba[4] == expected[4]);
  CHECK(decoded.rgba[5] == expected[5]);
  CHECK(decoded.rgba[6] == expected[6]);
  CHECK(decoded.rgba[7] == expected[7]);
  CHECK(decoded.rgba == std::vector<uint8_t>(expected.begin(), expected.end()));

  std::ignore = std::filesystem::remove(path);
}

TEST_CASE("libspng backend rejects invalid layer png bytes during read", "[libspng-backend]") {
  auto doc = with_preview_assets(make_invalid_document());

  auto const path = std::filesystem::path{"test-libspng-invalid-read.ora"};
  std::ignore = std::filesystem::remove(path);

  REQUIRE(ora::libspng::write(path.string(), doc).has_value());

  auto read_result = ora::libspng::read(path.string());

  REQUIRE_FALSE(read_result.has_value());
  CHECK(read_result.error().code == ora::Error::Code::PngDecodeFailed);
  CHECK(read_result.error().message.contains("libspng"));

  std::ignore = std::filesystem::remove(path);
}

TEST_CASE("libspng backend rejects invalid layer png bytes during render", "[libspng-backend]") {
  auto doc = make_invalid_document();

  auto result = ora::libspng::render_preview_and_thumbnail(doc);

  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().code == ora::Error::Code::PngDecodeFailed);
  CHECK(result.error().message.contains("libspng"));
}
