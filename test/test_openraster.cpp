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
