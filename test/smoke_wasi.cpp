/**
 * @file smoke_wasi.cpp
 * @brief WASI (wasip1) 用のスモークテストです。Catch2 を使わず、wasmtime で実行します。
 *
 * lodepng/stb バックエンドの encode、プレビュー生成、ORA の write/read
 * ラウンドトリップが例外なし・ファイル I/O ありで動作することを確認します。
 * hosted 環境でもビルド・実行できます。
 */
#include "openraster_lodepng.hpp"
#include "openraster_stb.hpp"

namespace {

auto check(bool condition, int& failures, int code) -> void {
  if (!condition) {
    failures = code;
  }
}

} // namespace

int main() {
  auto failures = 0;

  auto img = ora::util::blank_image(2, 1, 255);
  check(img.has_value(), failures, 1);
  if (failures != 0) {
    return failures;
  }

  auto png = ora::lodepng::encode_png(*img);
  check(png.has_value() && png->size() >= 8U, failures, 2);
  check((*png)[0] == 0x89 && (*png)[1] == 0x50 && (*png)[2] == 0x4e && (*png)[3] == 0x47, failures, 3);
  if (failures != 0) {
    return failures;
  }

  auto stb_png = ora::stb::encode_png(*img);
  check(stb_png.has_value() && stb_png->size() >= 8U, failures, 4);
  if (failures != 0) {
    return failures;
  }

  auto doc = ora::OraDocument{
    .width = 2,
    .height = 1,
    .root_nodes = {ora::layer("layer-1")},
    .layer_images = {{"layer-1", *png}},
    .merged_image_png = std::nullopt,
    .thumbnail_png = std::nullopt,
  };
  check(ora::lodepng::render_preview_and_thumbnail(doc).has_value(), failures, 5);
  check(doc.merged_image_png.has_value() && doc.thumbnail_png.has_value(), failures, 6);
  if (failures != 0) {
    return failures;
  }

  check(ora::lodepng::write("test-wasi-roundtrip.ora", doc).has_value(), failures, 7);
  if (failures != 0) {
    return failures;
  }

  auto read = ora::lodepng::read("test-wasi-roundtrip.ora");
  check(read.has_value(), failures, 8);
  if (failures != 0) {
    return failures;
  }
  check(read->width == 2 && read->height == 1, failures, 9);
  check(read->layer_images.contains("layer-1"), failures, 10);

  return failures;
}
