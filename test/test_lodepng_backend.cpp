#include "catch2/catch_test_macros.hpp"
#include "openraster_lodepng.hpp"

TEST_CASE("lodepng backend encodes images", "[lodepng-backend]") {
  auto image = ora::util::blank_image(2, 1, 255);
  REQUIRE(image.has_value());

  auto png = ora::lodepng::encode_png(*image);

  REQUIRE(png.has_value());
  CHECK(png->size() > 8);
}

TEST_CASE("lodepng backend renders preview assets", "[lodepng-backend]") {
  auto image = ora::util::blank_image(2, 1, 255);
  REQUIRE(image.has_value());

  auto layer_png = ora::lodepng::encode_png(*image);
  REQUIRE(layer_png.has_value());

  auto doc = ora::OraDocument{
    .width = 2,
    .height = 1,
    .root_nodes = {ora::layer("layer-1")},
    .layer_images = {{"layer-1", *layer_png}},
    .merged_image_png = std::nullopt,
    .thumbnail_png = std::nullopt,
  };

  REQUIRE(ora::lodepng::render_preview_and_thumbnail(doc).has_value());
  REQUIRE(doc.merged_image_png.has_value());
  REQUIRE(doc.thumbnail_png.has_value());
}

TEST_CASE("lodepng backend round-trips read/write", "[lodepng-backend]") {
  auto image = ora::util::blank_image(1, 1, 255);
  REQUIRE(image.has_value());

  auto layer_png = ora::lodepng::encode_png(*image);
  REQUIRE(layer_png.has_value());

  auto doc = ora::OraDocument{
    .width = 1,
    .height = 1,
    .root_nodes = {ora::layer("layer-1")},
    .layer_images = {{"layer-1", *layer_png}},
    .merged_image_png = std::nullopt,
    .thumbnail_png = std::nullopt,
  };
  REQUIRE(ora::lodepng::render_preview_and_thumbnail(doc).has_value());
  REQUIRE(ora::lodepng::write("test-roundtrip.ora", doc).has_value());

  auto loaded = ora::lodepng::read("test-roundtrip.ora");

  REQUIRE(loaded.has_value());
  CHECK(loaded->width == 1);
  CHECK(loaded->layer_images.contains("layer-1"));
}
