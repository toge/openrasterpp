#include "catch2/catch_test_macros.hpp"
#include "openraster.hpp"

TEST_CASE("OraDocument initialization") {
    ora::OraDocument doc;
    doc.width = 100;
    doc.height = 200;
    
    SECTION("Empty document") {
        CHECK(doc.width == 100);
        CHECK(doc.height == 200);
        CHECK(doc.root_nodes.empty());
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
