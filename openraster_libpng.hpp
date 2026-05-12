#ifndef OPENRASTER_LIBPNG_HPP__
#define OPENRASTER_LIBPNG_HPP__

#include "openraster.hpp"

namespace ora::libpng {

[[nodiscard]]
auto read(std::string_view filename) -> std::expected<OraDocument, Error>;

[[nodiscard]]
auto write(std::string_view filename, OraDocument const& doc) -> std::expected<void, Error>;

[[nodiscard]]
auto encode_png(ImageBuffer const& image) -> std::expected<std::vector<uint8_t>, Error>;

[[nodiscard]]
auto render_preview_and_thumbnail(OraDocument& doc) -> std::expected<void, Error>;

} // namespace ora::libpng

#endif // OPENRASTER_LIBPNG_HPP__
