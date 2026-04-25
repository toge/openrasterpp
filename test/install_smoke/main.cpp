#include <openraster.hpp>

auto main() -> int {
  auto const mode = ora::to_string(ora::BlendMode::SrcOver);
  return mode == "svg:src-over" ? 0 : 1;
}
