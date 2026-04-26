#include "catch2/catch_test_macros.hpp"
#include "openraster.hpp"

namespace {

struct RecordingProvider {
    std::map<std::string, std::vector<uint8_t>> written_entries;
    int encode_png_calls = 0;

    auto open_archive(std::string_view, ora::ArchiveMode) -> std::expected<void, ora::Error> {
        return {};
    }

    auto close_archive() -> void {}

    auto read_entry(std::string_view) -> std::expected<std::vector<uint8_t>, ora::Error> {
        return std::unexpected(ora::Error{ora::Error::Code::InvalidOraDocument, "unused"});
    }

    auto write_entry(std::string_view path, std::span<const uint8_t> data, ora::CompressionLevel)
        -> std::expected<void, ora::Error> {
        written_entries[std::string{path}] = std::vector<uint8_t>(data.begin(), data.end());
        return {};
    }

    auto encode_png(std::span<const uint8_t>, unsigned int, unsigned int)
        -> std::expected<std::vector<uint8_t>, ora::Error> {
        ++encode_png_calls;
        return std::vector<uint8_t>{0x89, 0x50, 0x4e, 0x47};
    }

    auto decode_png(std::span<const uint8_t>)
        -> std::expected<ora::DecodedImage, ora::Error> {
        return std::unexpected(ora::Error{ora::Error::Code::PngDecodeFailed, "unused"});
    }

    auto serialize_stack(ora::OraDocument const&) -> std::string {
        return "<?xml version='1.0' encoding='UTF-8'?><image version='0.0.5' w='1' h='1'></image>";
    }

    auto deserialize_stack(std::span<const uint8_t>)
        -> std::expected<ora::OraDocument, ora::Error> {
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

} // namespace

TEST_CASE("OraDocument initialization") {
    ora::OraDocument doc;
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
    auto buffer = ora::ImageBuffer::make_blank(10, 10, 255);
    REQUIRE(buffer.has_value());
    CHECK(buffer->width() == 10);
    CHECK(buffer->height() == 10);
    CHECK(buffer->rgba().size() == 10 * 10 * 4);
    CHECK(buffer->rgba()[3] == 255); // Alpha channel
}

TEST_CASE("BlendMode string conversion") {
    CHECK(ora::to_string(ora::BlendMode::SrcOver) == "svg:src-over");
    CHECK(ora::from_string("svg:src-over") == ora::BlendMode::SrcOver);
    CHECK(ora::from_string("invalid") == std::nullopt);
}

TEST_CASE("BlendMode::Hue does not corrupt blue channel") {
    auto backdrop = ora::ImageBuffer::make_blank(1, 1, 255);
    REQUIRE(backdrop.has_value());
    backdrop->rgba_mut()[0] = 255;
    backdrop->rgba_mut()[1] = 0;
    backdrop->rgba_mut()[2] = 0;

    auto source = ora::ImageBuffer::make_blank(1, 1, 255);
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
        xml.size());
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

TEST_CASE("write still encodes layer images under data/", "[write-assets]") {
    auto provider = RecordingProvider{};
    auto doc = make_test_document();
    auto layer = ora::ImageBuffer::make_blank(1, 1, 255);
    REQUIRE(layer.has_value());
    doc.root_nodes.push_back(ora::layer("layer-1"));
    doc.layer_images.emplace("layer-1", std::move(*layer));

    auto const result = ora::write(provider, "ignored.ora", doc);

    REQUIRE(result.has_value());
    CHECK(provider.written_entries.contains("data/layer-1.png"));
    CHECK(provider.encode_png_calls == 1);
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
