#include "catch2/catch_test_macros.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include <png.h>

#include "openraster_stb.hpp"

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
  auto image = png_image{};
  image.version = PNG_IMAGE_VERSION;

  REQUIRE(::png_image_begin_read_from_memory(&image, png_bytes.data(), png_bytes.size()) != 0);
  image.format = PNG_FORMAT_RGBA;

  auto rgba = std::vector<uint8_t>(PNG_IMAGE_SIZE(image));
  REQUIRE(::png_image_finish_read(&image, nullptr, rgba.data(), 0, nullptr) != 0);

  auto const width = image.width;
  auto const height = image.height;
  ::png_image_free(&image);

  return DecodedPng{
    .width = width,
    .height = height,
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

  auto layer_png = ora::stb::encode_png(layer);
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

auto valid_bmp_bytes() -> std::array<uint8_t, 58> {
  return {
    0x42, 0x4d, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00,
    0x28, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x13, 0x0b, 0x00, 0x00,
    0x13, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x40,
    0x80, 0x00
  };
}

auto make_non_png_document() -> ora::OraDocument {
  auto const bmp = valid_bmp_bytes();
  return ora::OraDocument{
    .width = 1,
    .height = 1,
    .root_nodes = {ora::layer("layer-1")},
    .layer_images = {{"layer-1", {bmp.begin(), bmp.end()}}},
    .merged_image_png = std::nullopt,
    .thumbnail_png = std::nullopt
  };
}

auto with_preview_assets(ora::OraDocument doc) -> ora::OraDocument {
  auto valid_doc = make_document();
  REQUIRE(ora::stb::render_preview_and_thumbnail(valid_doc).has_value());
  doc.merged_image_png = valid_doc.merged_image_png;
  doc.thumbnail_png = valid_doc.thumbnail_png;
  return doc;
}

} // namespace

TEST_CASE("stb backend encodes image buffers", "[stb-backend]") {
  auto image = make_reference_image();

  auto png = ora::stb::encode_png(image);

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

TEST_CASE("stb backend renders preview assets", "[stb-backend]") {
  auto doc = make_document();

  auto result = ora::stb::render_preview_and_thumbnail(doc);

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

TEST_CASE("stb backend round-trips read/write", "[stb-backend]") {
  auto doc = make_document();
  REQUIRE(ora::stb::render_preview_and_thumbnail(doc).has_value());

  auto const path = std::filesystem::path{"test-stb-roundtrip.ora"};
  std::ignore = std::filesystem::remove(path);

  auto write_result = ora::stb::write(path.string(), doc);
  REQUIRE(write_result.has_value());

  auto read_result = ora::stb::read(path.string());
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

TEST_CASE("stb backend rejects invalid layer png bytes during read", "[stb-backend]") {
  auto doc = with_preview_assets(make_invalid_document());

  auto const path = std::filesystem::path{"test-stb-invalid-read.ora"};
  std::ignore = std::filesystem::remove(path);

  REQUIRE(ora::stb::write(path.string(), doc).has_value());

  auto read_result = ora::stb::read(path.string());

  REQUIRE_FALSE(read_result.has_value());
  CHECK(read_result.error().code == ora::Error::Code::PngDecodeFailed);
  CHECK(read_result.error().message.contains("stb"));

  std::ignore = std::filesystem::remove(path);
}

TEST_CASE("stb backend rejects invalid layer png bytes during render", "[stb-backend]") {
  auto doc = make_invalid_document();

  auto result = ora::stb::render_preview_and_thumbnail(doc);

  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().code == ora::Error::Code::PngDecodeFailed);
  CHECK(result.error().message.contains("stb"));
}

TEST_CASE("stb backend rejects valid non-png layer bytes during render", "[stb-backend]") {
  auto doc = make_non_png_document();

  auto result = ora::stb::render_preview_and_thumbnail(doc);

  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().code == ora::Error::Code::PngDecodeFailed);
  CHECK(result.error().message.contains("stb"));
}
