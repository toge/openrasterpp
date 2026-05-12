#include "catch2/catch_test_macros.hpp"

#include <filesystem>
#include <string>
#include <vector>

#include "openraster_libspng.hpp"

namespace {

auto make_document() -> ora::OraDocument {
  auto layer = ora::util::blank_image(2, 1, 255);
  REQUIRE(layer.has_value());
  layer->rgba_mut()[0] = 255;
  layer->rgba_mut()[1] = 64;
  layer->rgba_mut()[2] = 32;
  layer->rgba_mut()[3] = 255;

  auto layer_png = ora::libspng::encode_png(*layer);
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
  auto image = ora::util::blank_image(2, 1, 255);
  REQUIRE(image.has_value());

  auto png = ora::libspng::encode_png(*image);

  REQUIRE(png.has_value());
  REQUIRE(png->size() >= 8U);
  CHECK((*png)[0] == 0x89);
  CHECK((*png)[1] == 0x50);
  CHECK((*png)[2] == 0x4e);
  CHECK((*png)[3] == 0x47);
}

TEST_CASE("libspng backend renders preview assets", "[libspng-backend]") {
  auto doc = make_document();

  auto result = ora::libspng::render_preview_and_thumbnail(doc);

  REQUIRE(result.has_value());
  REQUIRE(doc.merged_image_png.has_value());
  REQUIRE(doc.thumbnail_png.has_value());
  CHECK(doc.merged_image_png->size() >= 8U);
  CHECK(doc.thumbnail_png->size() >= 8U);
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
  CHECK(read_result->height == 1);
  CHECK(read_result->layer_images.contains("layer-1"));

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
