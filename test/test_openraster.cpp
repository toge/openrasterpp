#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include "openraster.hpp"

namespace {

struct RecordingProvider {
  std::map<std::string, std::vector<uint8_t>> read_entries;
  std::map<std::string, std::vector<uint8_t>> written_entries;
  int encode_png_calls = 0;
  int decode_png_calls = 0;
  bool fail_decode = false;
  std::optional<ora::OraDocument> deserialized_doc;
  std::optional<ora::DecodedImage> decoded_image;

  auto open_archive(std::string_view, ora::ArchiveMode) -> std::expected<void, ora::Error> {
    return {};
  }

  auto close_archive() -> void {}

  auto read_entry(std::string_view path) -> std::expected<std::vector<uint8_t>, ora::Error> {
    auto const key = std::string{path};
    if (auto const it = read_entries.find(key); it != read_entries.end()) {
      return it->second;
    }
    return std::unexpected(ora::Error{ora::Error::Code::InvalidOraDocument, key});
  }

  auto write_entry(std::string_view path, std::span<const uint8_t> data, ora::CompressionLevel)
      -> std::expected<void, ora::Error> {
    written_entries[std::string{path}] = std::vector<uint8_t>(data.begin(), data.end());
    return {};
  }

  auto encode_png(std::span<const uint8_t>, unsigned int width, unsigned int height)
      -> std::expected<std::vector<uint8_t>, ora::Error> {
    ++encode_png_calls;
    return std::vector<uint8_t>{
      0x89,
      0x50,
      0x4e,
      0x47,
      static_cast<uint8_t>((width >> 8U) & 0xffU),
      static_cast<uint8_t>(width & 0xffU),
      static_cast<uint8_t>((height >> 8U) & 0xffU),
      static_cast<uint8_t>(height & 0xffU)
    };
  }

  auto decode_png(std::span<const uint8_t>)
      -> std::expected<ora::DecodedImage, ora::Error> {
    ++decode_png_calls;
    if (fail_decode) {
      return std::unexpected(ora::Error{
        ora::Error::Code::PngDecodeFailed,
        "invalid png"
      });
    }
    if (decoded_image.has_value()) {
      return *decoded_image;
    }
    return ora::DecodedImage{
      .rgba = {255, 0, 0, 255},
      .width = 1,
      .height = 1
    };
  }

  auto serialize_stack(ora::OraDocument const& doc) -> std::string {
    return "<?xml version='1.0' encoding='UTF-8'?><image version='0.0.5' w='" +
      std::to_string(doc.width) + "' h='" + std::to_string(doc.height) + "'></image>";
  }

  auto deserialize_stack(std::span<const uint8_t>)
      -> std::expected<ora::OraDocument, ora::Error> {
    if (deserialized_doc.has_value()) {
      return *deserialized_doc;
    }
    return std::unexpected(ora::Error{ora::Error::Code::XmlParseFailed, "unused"});
  }
};

auto make_test_document() -> ora::OraDocument {
  return ora::OraDocument{
    .width = 1,
    .height = 1,
    .root_nodes = {},
    .layer_images = {},
    .merged_image_png = std::vector<uint8_t>{1, 2, 3, 4},
    .thumbnail_png = std::vector<uint8_t>{5, 6, 7, 8},
  };
}

auto make_invalid_png_bytes() -> std::vector<uint8_t> {
  return {0x00, 0x01, 0x02, 0x03};
}

auto make_provider_png(unsigned int width, unsigned int height) -> std::vector<uint8_t> {
  return {
    0x89,
    0x50,
    0x4e,
    0x47,
    static_cast<uint8_t>((width >> 8U) & 0xffU),
    static_cast<uint8_t>(width & 0xffU),
    static_cast<uint8_t>((height >> 8U) & 0xffU),
    static_cast<uint8_t>(height & 0xffU)
  };
}

} // namespace

TEST_CASE("OraDocument initialization") {
  auto doc = ora::OraDocument{};
  doc.width = 100;
  doc.height = 200;

  SECTION("Empty document") {
    CHECK(doc.width == 100);
    CHECK(doc.height == 200);
    CHECK(doc.root_nodes.empty());
    CHECK(doc.layer_images.empty());
    CHECK_FALSE(doc.merged_image_png.has_value());
    CHECK_FALSE(doc.thumbnail_png.has_value());
  }
}

TEST_CASE("ImageBuffer creation") {
  auto buffer = ora::util::blank_image(10, 10, 255);
  REQUIRE(buffer.has_value());
  CHECK(buffer->width() == 10);
  CHECK(buffer->height() == 10);
  CHECK(buffer->rgba().size() == 10 * 10 * 4);
  CHECK(buffer->rgba()[3] == 255);
}

TEST_CASE("encode_png helper converts ImageBuffer into png bytes", "[png-helper]") {
  auto provider = RecordingProvider{};
  auto buffer = ora::util::blank_image(1, 1, 255);
  REQUIRE(buffer.has_value());

  auto const png = ora::util::encode_png(provider, *buffer);

  REQUIRE(png.has_value());
  CHECK(*png == make_provider_png(1, 1));
  CHECK(provider.encode_png_calls == 1);
}

TEST_CASE("BlendMode string conversion") {
  CHECK(ora::to_string(ora::BlendMode::SrcOver) == "svg:src-over");
  CHECK(ora::from_string("svg:src-over") == ora::BlendMode::SrcOver);
  CHECK(ora::from_string("invalid") == std::nullopt);
}

TEST_CASE("BlendMode::Hue does not corrupt blue channel") {
  auto backdrop = ora::util::blank_image(1, 1, 255);
  REQUIRE(backdrop.has_value());
  backdrop->rgba_mut()[0] = 255;
  backdrop->rgba_mut()[1] = 0;
  backdrop->rgba_mut()[2] = 0;

  auto source = ora::util::blank_image(1, 1, 255);
  REQUIRE(source.has_value());
  source->rgba_mut()[0] = 0;
  source->rgba_mut()[1] = 0;
  source->rgba_mut()[2] = 255;

  auto canvas = std::vector<uint8_t>(4, 0);
  canvas[0] = 255;
  canvas[3] = 255;

  ora::detail::blend_layer(canvas, 1, 1, *source, 0, 0, 1.0f, ora::BlendMode::Hue);

  CHECK(canvas[0] != canvas[2]);
}

TEST_CASE("Nested stacks parse correctly without dangling pointer") {
  auto const xml = std::string{R"(<?xml version='1.0' encoding='UTF-8'?>
<image version='0.0.5' w='100' h='100'>
  <stack name='group1' composite-op='svg:src-over' opacity='1.0' visibility='visible' x='0' y='0'>
    <layer name='layer1' src='data/layer1.png' composite-op='svg:src-over' opacity='1.0' visibility='visible' x='0' y='0'/>
  </stack>
  <stack name='group2' composite-op='svg:src-over' opacity='1.0' visibility='visible' x='0' y='0'>
    <layer name='layer2' src='data/layer2.png' composite-op='svg:src-over' opacity='1.0' visibility='visible' x='0' y='0'/>
  </stack>
  <stack name='group3' composite-op='svg:src-over' opacity='1.0' visibility='visible' x='0' y='0'>
    <layer name='layer3' src='data/layer3.png' composite-op='svg:src-over' opacity='1.0' visibility='visible' x='0' y='0'/>
  </stack>
</image>
)"};

  auto const xml_bytes = std::span<const uint8_t>(
    reinterpret_cast<const uint8_t*>(xml.data()),
    xml.size()
  );
  auto const doc = ora::detail::deserialize_stack(xml_bytes);

  REQUIRE(doc.has_value());
  REQUIRE(doc->root_nodes.size() == 3);
  CHECK(doc->root_nodes[0].children.size() == 1);
  CHECK(doc->root_nodes[1].children.size() == 1);
  CHECK(doc->root_nodes[2].children.size() == 1);
}

TEST_CASE("write stores caller-provided merged image and thumbnail bytes", "[write-assets]") {
  auto provider = RecordingProvider{};
  auto doc = make_test_document();

  auto const result = ora::write(provider, "ignored.ora", doc);

  REQUIRE(result.has_value());
  CHECK(provider.written_entries.at("mergedimage.png") == *doc.merged_image_png);
  CHECK(provider.written_entries.at("Thumbnails/thumbnail.png") == *doc.thumbnail_png);
  CHECK(provider.encode_png_calls == 0);
}

TEST_CASE("read stores layer images as png bytes", "[png-read]") {
  auto provider = RecordingProvider{};
  auto const layer_png = make_provider_png(1, 1);
  provider.read_entries.emplace("mimetype", std::vector<uint8_t>{'i', 'm', 'a', 'g', 'e', '/', 'o', 'p', 'e', 'n', 'r', 'a', 's', 't', 'e', 'r'});
  provider.read_entries.emplace("stack.xml", std::vector<uint8_t>{'<', 'x', 'm', 'l', '/', '>'});
  provider.read_entries.emplace("data/layer-1.png", layer_png);
  provider.deserialized_doc = ora::OraDocument{
    .width = 1,
    .height = 1,
    .root_nodes = {ora::layer("layer-1")},
    .layer_images = {},
    .merged_image_png = std::nullopt,
    .thumbnail_png = std::nullopt,
  };

  auto const result = ora::read(provider, "ignored.ora");

  REQUIRE(result.has_value());
  CHECK(result->layer_images.at("layer-1") == layer_png);
  CHECK(provider.decode_png_calls == 1);
}

TEST_CASE("read rejects invalid layer png bytes", "[png-read]") {
  auto provider = RecordingProvider{};
  provider.fail_decode = true;
  provider.read_entries.emplace("mimetype", std::vector<uint8_t>{'i', 'm', 'a', 'g', 'e', '/', 'o', 'p', 'e', 'n', 'r', 'a', 's', 't', 'e', 'r'});
  provider.read_entries.emplace("stack.xml", std::vector<uint8_t>{'<', 'x', 'm', 'l', '/', '>'});
  provider.read_entries.emplace("data/layer-1.png", make_invalid_png_bytes());
  provider.deserialized_doc = ora::OraDocument{
    .width = 1,
    .height = 1,
    .root_nodes = {ora::layer("layer-1")},
    .layer_images = {},
    .merged_image_png = std::nullopt,
    .thumbnail_png = std::nullopt,
  };

  auto const result = ora::read(provider, "ignored.ora");

  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().code == ora::Error::Code::PngDecodeFailed);
}

TEST_CASE("write stores provided layer png bytes under data/", "[png-layer-write]") {
  auto provider = RecordingProvider{};
  auto doc = make_test_document();
  auto const layer_png = make_provider_png(1, 1);
  doc.root_nodes.push_back(ora::layer("layer-1"));
  doc.layer_images.emplace("layer-1", layer_png);

  auto const result = ora::write(provider, "ignored.ora", doc);

  REQUIRE(result.has_value());
  CHECK(provider.written_entries.at("data/layer-1.png") == layer_png);
  CHECK(provider.encode_png_calls == 0);
}

TEST_CASE("write rejects missing caller-provided render assets", "[write-assets]") {
  auto provider = RecordingProvider{};

  SECTION("missing thumbnail") {
    auto doc = make_test_document();
    doc.thumbnail_png.reset();

    auto const result = ora::write(provider, "ignored.ora", doc);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == ora::Error::Code::InvalidOraDocument);
  }

  SECTION("missing merged image") {
    auto doc = make_test_document();
    doc.merged_image_png.reset();

    auto const result = ora::write(provider, "ignored.ora", doc);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == ora::Error::Code::InvalidOraDocument);
  }
}

TEST_CASE("render_preview_and_thumbnail sets preview assets on OraDocument", "[render-helper]") {
  auto provider = RecordingProvider{};
  auto doc = ora::OraDocument{
    .width = 2,
    .height = 1,
    .root_nodes = {ora::layer("layer-1")},
    .layer_images = {{"layer-1", make_provider_png(1, 1)}},
    .merged_image_png = std::nullopt,
    .thumbnail_png = std::nullopt,
  };

  auto const result = ora::util::render_preview_and_thumbnail(provider, doc);

  REQUIRE(result.has_value());
  CHECK(doc.merged_image_png == make_provider_png(2, 1));
  CHECK(doc.thumbnail_png == make_provider_png(2, 1));
  CHECK(provider.decode_png_calls == 1);
  CHECK(provider.encode_png_calls == 2);
}

TEST_CASE("render_preview_and_thumbnail rejects zero-sized documents", "[render-helper]") {
  auto provider = RecordingProvider{};
  auto doc = ora::OraDocument{
    .width = 0,
    .height = 1,
    .root_nodes = {},
    .layer_images = {},
    .merged_image_png = std::nullopt,
    .thumbnail_png = std::nullopt,
  };

  auto const result = ora::util::render_preview_and_thumbnail(provider, doc);

  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().code == ora::Error::Code::InvalidOraDocument);
}

TEST_CASE("render_preview_and_thumbnail rejects invalid layer png bytes", "[render-helper]") {
  auto provider = RecordingProvider{};
  provider.fail_decode = true;
  auto doc = ora::OraDocument{
    .width = 1,
    .height = 1,
    .root_nodes = {ora::layer("layer-1")},
    .layer_images = {{"layer-1", make_invalid_png_bytes()}},
    .merged_image_png = std::nullopt,
    .thumbnail_png = std::nullopt,
  };

  auto const result = ora::util::render_preview_and_thumbnail(provider, doc);

  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().code == ora::Error::Code::PngDecodeFailed);
}

TEST_CASE("render_preview_and_thumbnail preserves aspect ratio within 256px", "[render-helper]") {
  auto provider = RecordingProvider{};
  provider.decoded_image = ora::DecodedImage{
    .rgba = std::vector<uint8_t>(static_cast<std::size_t>(300) * 150U * 4U, 255),
    .width = 300,
    .height = 150
  };
  auto doc = ora::OraDocument{
    .width = 300,
    .height = 150,
    .root_nodes = {ora::layer("layer-1")},
    .layer_images = {{"layer-1", make_provider_png(1, 1)}},
    .merged_image_png = std::nullopt,
    .thumbnail_png = std::nullopt,
  };

  auto const result = ora::util::render_preview_and_thumbnail(provider, doc);

  REQUIRE(result.has_value());
  CHECK(doc.merged_image_png == make_provider_png(300, 150));
  CHECK(doc.thumbnail_png == make_provider_png(256, 128));
}

TEST_CASE("detail numeric parsing accepts extended float forms and rejects garbage", "[parse]") {
  CHECK(ora::detail::parse_uint("100") == 100UL);
  CHECK_FALSE(ora::detail::parse_uint("12x").has_value());
  CHECK_FALSE(ora::detail::parse_uint("").has_value());
  CHECK_FALSE(ora::detail::parse_uint("99999999999999999999999").has_value());

  CHECK(ora::detail::parse_int("-3") == -3);
  CHECK(ora::detail::parse_int("+3") == 3);
  CHECK_FALSE(ora::detail::parse_int("3.5").has_value());

  CHECK(ora::detail::parse_float("1.0") == 1.0f);
  CHECK(ora::detail::parse_float("1e-1") == Catch::Approx(0.1f));
  CHECK_FALSE(ora::detail::parse_float("abc").has_value());
  CHECK_FALSE(ora::detail::parse_float("1.0trailing").has_value());
}

TEST_CASE("deserialize_stack rejects invalid numeric attributes", "[parse]") {
  auto const xml = std::string{R"(<?xml version='1.0' encoding='UTF-8'?>
<image version='0.0.5' w='100' h='100'>
  <layer name='layer1' src='data/layer1.png' composite-op='svg:src-over' opacity='abc' visibility='visible' x='0' y='0'/>
</image>
)"};
  auto const xml_bytes = std::span<const uint8_t>(
    reinterpret_cast<const uint8_t*>(xml.data()),
    xml.size()
  );
  auto const doc = ora::detail::deserialize_stack(xml_bytes);

  REQUIRE_FALSE(doc.has_value());
  CHECK(doc.error().code == ora::Error::Code::XmlParseFailed);
}
