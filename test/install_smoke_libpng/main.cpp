#include <openraster_libpng.hpp>

auto main() -> int {
  auto image = ora::util::blank_image(1, 1, 255);
  if (!image.has_value()) {
    return 1;
  }

  auto png = ora::libpng::encode_png(*image);
  return png.has_value() && png->size() > 8 ? 0 : 1;
}
