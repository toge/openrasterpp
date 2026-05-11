#include "catch2/catch_test_macros.hpp"
#include "openraster_lodepng.hpp"

#include <concepts>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

static_assert(std::same_as<
  decltype(ora::lodepng::read(std::declval<std::string_view>())),
  std::expected<ora::OraDocument, ora::Error>
>);

static_assert(std::same_as<
  decltype(ora::lodepng::write(
    std::declval<std::string_view>(),
    std::declval<ora::OraDocument const&>()
  )),
  std::expected<void, ora::Error>
>);

static_assert(std::same_as<
  decltype(ora::lodepng::encode_png(std::declval<ora::ImageBuffer const&>())),
  std::expected<std::vector<uint8_t>, ora::Error>
>);

static_assert(std::same_as<
  decltype(ora::lodepng::render_preview_and_thumbnail(std::declval<ora::OraDocument&>())),
  std::expected<void, ora::Error>
>);

} // namespace

TEST_CASE("lodepng backend scaffolding exposes the planned API", "[lodepng-backend]") {
  SUCCEED();
}
