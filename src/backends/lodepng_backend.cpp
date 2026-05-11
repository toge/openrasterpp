#include "openraster_lodepng.hpp"

#include <array>
#include <iomanip>
#include <sstream>
#include <tuple>
#include <utility>

#include "lodepng.h"
#include "unzip.h"
#include "zip.h"

namespace ora {

namespace {

auto escape_xml(std::string_view str) -> std::string {
  auto res = std::string{};
  for (auto const c : str) {
    switch (c) {
      case '<':
        res += "&lt;";
        break;
      case '>':
        res += "&gt;";
        break;
      case '&':
        res += "&amp;";
        break;
      case '\'':
        res += "&apos;";
        break;
      case '"':
        res += "&quot;";
        break;
      default:
        res += c;
        break;
    }
  }
  return res;
}

auto generate_node_xml(Node const& node, int indent_level, std::ostringstream& ss) -> std::string {
  auto xml = std::string(static_cast<std::size_t>(indent_level) * 2U, ' ');
  ss.str("");
  ss.clear();
  ss << node.opacity;
  auto const opacity_str = ss.str();
  auto const vis = node.visible ? "visible" : "hidden";
  auto const mode = std::string{to_string(node.blend_mode)};
  auto const name = escape_xml(node.name);
  if (node.type == Node::Type::Layer) {
    xml += "<layer name='" + name + "' src='data/" + name + ".png' x='" + std::to_string(node.x) +
      "' y='" + std::to_string(node.y) + "' visibility='" + vis + "' opacity='" +
      opacity_str + "' composite-op='" + mode + "'/>\n";
    return xml;
  }
  xml += "<stack name='" + name + "' x='" + std::to_string(node.x) + "' y='" +
    std::to_string(node.y) + "' visibility='" + vis + "' opacity='" + opacity_str +
    "' composite-op='" + mode + "'>\n";
  for (auto const& child : node.children) {
    xml += generate_node_xml(child, indent_level + 1, ss);
  }
  xml += std::string(static_cast<std::size_t>(indent_level) * 2U, ' ') + "</stack>\n";
  return xml;
}

class LodepngOraProvider {
public:
  auto open_archive(std::string_view path, ArchiveMode mode) -> std::expected<void, Error> {
    if (mode == ArchiveMode::Read) {
      unzip_ = unzOpen(std::string{path}.c_str());
      if (!unzip_) {
        return detail::make_unexpected(Error::Code::ZipOpenFailed, path);
      }
    } else {
      zip_ = zipOpen(std::string{path}.c_str(), 0);
      if (!zip_) {
        return detail::make_unexpected(Error::Code::ZipCreateFailed, path);
      }
    }
    return {};
  }

  auto close_archive() -> void {
    if (unzip_) {
      std::ignore = unzClose(unzip_);
      unzip_ = nullptr;
    }
    if (zip_) {
      std::ignore = zipClose(zip_, nullptr);
      zip_ = nullptr;
    }
  }

  auto read_entry(std::string_view path) -> std::expected<std::vector<uint8_t>, Error> {
    if (!unzip_) {
      return detail::make_unexpected(Error::Code::ZipReadFailed, path, "archive not open");
    }
    if (unzLocateFile(unzip_, std::string{path}.c_str(), 1) != UNZ_OK) {
      return detail::make_unexpected(Error::Code::InvalidOraDocument, path, "entry not found");
    }
    auto info = unz_file_info64{};
    if (unzGetCurrentFileInfo64(unzip_, &info, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK) {
      return detail::make_unexpected(Error::Code::ZipReadFailed, path);
    }
    if (unzOpenCurrentFile(unzip_) != UNZ_OK) {
      return detail::make_unexpected(Error::Code::ZipReadFailed, path);
    }

    auto data = std::vector<uint8_t>{};
    data.reserve(static_cast<std::size_t>(info.uncompressed_size));
    auto buffer = std::array<uint8_t, 8192>{};
    while (true) {
      auto const read = unzReadCurrentFile(unzip_, buffer.data(), static_cast<unsigned int>(buffer.size()));
      if (read < 0) {
        std::ignore = unzCloseCurrentFile(unzip_);
        return detail::make_unexpected(Error::Code::ZipReadFailed, path);
      }
      if (read == 0) {
        break;
      }
      data.insert(data.end(), buffer.begin(), buffer.begin() + read);
    }
    std::ignore = unzCloseCurrentFile(unzip_);
    return data;
  }

  auto write_entry(std::string_view path, std::span<const uint8_t> data, CompressionLevel level)
      -> std::expected<void, Error> {
    if (!zip_) {
      return detail::make_unexpected(Error::Code::ZipAddFileFailed, path, "archive not open");
    }
    auto info = zip_fileinfo{};
    auto const method = level == CompressionLevel::None ? 0 : 8;
    auto const zip_level = level == CompressionLevel::Best ? 9 : (level == CompressionLevel::None ? 0 : -1);
    if (zipOpenNewFileInZip(
          zip_,
          std::string{path}.c_str(),
          &info,
          nullptr,
          0,
          nullptr,
          0,
          nullptr,
          method,
          zip_level
        ) != ZIP_OK) {
      return detail::make_unexpected(Error::Code::ZipAddFileFailed, path);
    }
    if (zipWriteInFileInZip(zip_, data.data(), static_cast<unsigned int>(data.size())) != ZIP_OK) {
      std::ignore = zipCloseFileInZip(zip_);
      return detail::make_unexpected(Error::Code::ZipAddFileFailed, path);
    }
    std::ignore = zipCloseFileInZip(zip_);
    return {};
  }

  auto encode_png(std::span<const uint8_t> rgba, unsigned int width, unsigned int height)
      -> std::expected<std::vector<uint8_t>, Error> {
    auto png = std::vector<uint8_t>{};
    if (auto const err = ::lodepng::encode(png, rgba.data(), width, height); err != 0) {
      return detail::make_unexpected(Error::Code::PngEncodeFailed, "buffer", ::lodepng_error_text(err));
    }
    return png;
  }

  auto decode_png(std::span<const uint8_t> data) -> std::expected<DecodedImage, Error> {
    auto image = DecodedImage{};
    if (auto const err = ::lodepng::decode(
          image.rgba,
          image.width,
          image.height,
          data.data(),
          data.size()
        ); err != 0) {
      return detail::make_unexpected(Error::Code::PngDecodeFailed, "buffer", ::lodepng_error_text(err));
    }
    return image;
  }

  auto serialize_stack(OraDocument const& doc) -> std::string {
    auto xml = std::string{
      "<?xml version='1.0' encoding='UTF-8'?>\n<image version='0.0.5' w='" +
      std::to_string(doc.width) + "' h='" + std::to_string(doc.height) + "'>\n"
    };
    auto ss = std::ostringstream{};
    ss << std::fixed << std::setprecision(2);
    for (auto const& node : doc.root_nodes) {
      xml += generate_node_xml(node, 1, ss);
    }
    xml += "</image>\n";
    return xml;
  }

  auto deserialize_stack(std::span<const uint8_t> xml_bytes) -> std::expected<OraDocument, Error> {
    return detail::deserialize_stack(xml_bytes);
  }

private:
  unzFile unzip_ = nullptr;
  zipFile zip_ = nullptr;
};

} // namespace

auto read(std::string_view filename) -> std::expected<OraDocument, Error> {
  auto provider = LodepngOraProvider{};
  return read(provider, filename);
}

namespace util {

auto encode_png(ImageBuffer const& image) -> std::expected<std::vector<uint8_t>, Error> {
  auto provider = LodepngOraProvider{};
  return encode_png(provider, image);
}

auto render_preview_and_thumbnail(OraDocument& doc) -> std::expected<void, Error> {
  auto provider = LodepngOraProvider{};
  return render_preview_and_thumbnail(provider, doc);
}

} // namespace util

auto write(std::string_view filename, OraDocument const& doc) -> std::expected<void, Error> {
  auto provider = LodepngOraProvider{};
  return write(provider, filename, doc);
}

namespace lodepng {

auto read(std::string_view filename) -> std::expected<OraDocument, Error> {
  auto provider = LodepngOraProvider{};
  return ora::read(provider, filename);
}

auto write(std::string_view filename, OraDocument const& doc) -> std::expected<void, Error> {
  auto provider = LodepngOraProvider{};
  return ora::write(provider, filename, doc);
}

auto encode_png(ImageBuffer const& image) -> std::expected<std::vector<uint8_t>, Error> {
  auto provider = LodepngOraProvider{};
  return ora::util::encode_png(provider, image);
}

auto render_preview_and_thumbnail(OraDocument& doc) -> std::expected<void, Error> {
  auto provider = LodepngOraProvider{};
  return ora::util::render_preview_and_thumbnail(provider, doc);
}

} // namespace lodepng

} // namespace ora
