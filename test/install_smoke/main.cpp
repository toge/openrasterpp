#include <openraster_lodepng.hpp>

#if defined(OPENRASTERPP_INSTALL_SMOKE_EXPECT_BACKEND_TARGET_MISSING)
#error "openrasterpp::openrasterpp-png-lodepng is not exported from the installed package yet"
#endif

auto main() -> int {
  auto image = ora::util::blank_image(1, 1, 255);
  if (!image.has_value()) {
    return 1;
  }

  auto png = ora::lodepng::encode_png(*image);
  return png.has_value() && png->size() > 8 ? 0 : 1;
}
