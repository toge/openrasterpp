#include "catch2/catch_test_macros.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>

#include "openraster_fpng.hpp"

namespace {

auto reference_rgba() -> std::array<uint8_t, 16> {
  return {
    255, 64, 32, 255,
    0, 128, 255, 200,
    10, 20, 30, 40,
    250, 251, 252, 1
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

  auto layer_png = ora::fpng::encode_png(layer);
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

} // namespace

TEST_CASE("fpng backend encodes image buffers", "[fpng-backend]") {
  auto image = make_reference_image();

  auto png = ora::fpng::encode_png(image);

  REQUIRE(png.has_value());
  REQUIRE(png->size() >= 8U);
  CHECK((*png)[0] == 0x89);
  CHECK((*png)[1] == 0x50);
  CHECK((*png)[2] == 0x4e);
  CHECK((*png)[3] == 0x47);
}

TEST_CASE("fpng backend renders preview assets", "[fpng-backend]") {
  auto doc = make_document();

  auto result = ora::fpng::render_preview_and_thumbnail(doc);

  REQUIRE(result.has_value());
  REQUIRE(doc.merged_image_png.has_value());
  REQUIRE(doc.thumbnail_png.has_value());
  CHECK(doc.merged_image_png->size() >= 8U);
  CHECK(doc.thumbnail_png->size() >= 8U);
}

TEST_CASE("fpng backend round-trips read/write", "[fpng-backend]") {
  auto doc = make_document();
  REQUIRE(ora::fpng::render_preview_and_thumbnail(doc).has_value());

  auto const path = std::filesystem::path{"test-fpng-roundtrip.ora"};
  std::ignore = std::filesystem::remove(path);

  auto write_result = ora::fpng::write(path.string(), doc);
  REQUIRE(write_result.has_value());

  auto read_result = ora::fpng::read(path.string());
  REQUIRE(read_result.has_value());
  CHECK(read_result->width == 2);
  CHECK(read_result->height == 2);
  CHECK(read_result->layer_images.contains("layer-1"));

  std::ignore = std::filesystem::remove(path);
}
