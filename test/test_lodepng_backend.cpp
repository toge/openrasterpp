#include "catch2/catch_test_macros.hpp"

#include <filesystem>
#include <string>
#include <tuple>
#include <vector>

#include "openraster_lodepng.hpp"

namespace {

auto make_document() -> ora::OraDocument {
  auto layer = ora::util::blank_image(2, 1, 255);
  REQUIRE(layer.has_value());
  layer->rgba_mut()[0] = 255;
  layer->rgba_mut()[1] = 64;
  layer->rgba_mut()[2] = 32;
  layer->rgba_mut()[3] = 255;

  auto layer_png = ora::lodepng::encode_png(*layer);
  REQUIRE(layer_png.has_value());

  return ora::OraDocument{
    .width = 2,
    .height = 1,
    .root_nodes = {ora::layer("layer-1")},
    .layer_images = {{"layer-1", std::move(*layer_png)}},
    .merged_image_png = std::nullopt,
    .thumbnail_png = std::nullopt
  };
}

} // namespace

TEST_CASE("lodepng backend encodes image buffers", "[lodepng-backend]") {
  auto image = ora::util::blank_image(2, 1, 255);
  REQUIRE(image.has_value());

  auto png = ora::lodepng::encode_png(*image);

  REQUIRE(png.has_value());
  REQUIRE(png->size() >= 8U);
  CHECK((*png)[0] == 0x89);
  CHECK((*png)[1] == 0x50);
  CHECK((*png)[2] == 0x4e);
  CHECK((*png)[3] == 0x47);
}

TEST_CASE("lodepng backend renders preview assets", "[lodepng-backend]") {
  auto doc = make_document();

  auto result = ora::lodepng::render_preview_and_thumbnail(doc);

  REQUIRE(result.has_value());
  REQUIRE(doc.merged_image_png.has_value());
  REQUIRE(doc.thumbnail_png.has_value());
  CHECK(doc.merged_image_png->size() >= 8U);
  CHECK(doc.thumbnail_png->size() >= 8U);
}

TEST_CASE("lodepng backend round-trips read/write", "[lodepng-backend]") {
  auto doc = make_document();
  REQUIRE(ora::lodepng::render_preview_and_thumbnail(doc).has_value());

  auto const path = std::filesystem::path{"test-lodepng-roundtrip.ora"};
  std::ignore = std::filesystem::remove(path);

  auto write_result = ora::lodepng::write(path.string(), doc);
  REQUIRE(write_result.has_value());

  auto read_result = ora::lodepng::read(path.string());
  REQUIRE(read_result.has_value());
  CHECK(read_result->width == 2);
  CHECK(read_result->height == 1);
  CHECK(read_result->layer_images.contains("layer-1"));

  std::ignore = std::filesystem::remove(path);
}
