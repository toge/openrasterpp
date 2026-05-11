#ifndef OPENRASTERPP_ARCHIVE_XML_PROVIDER_HPP__
#define OPENRASTERPP_ARCHIVE_XML_PROVIDER_HPP__

#include "openraster.hpp"

#include "unzip.h"
#include "zip.h"

namespace ora::detail {

class ArchiveXmlProvider {
public:
  ArchiveXmlProvider() = default;
  ArchiveXmlProvider(ArchiveXmlProvider const&) = delete;
  auto operator=(ArchiveXmlProvider const&) -> ArchiveXmlProvider& = delete;
  ArchiveXmlProvider(ArchiveXmlProvider&&) = delete;
  auto operator=(ArchiveXmlProvider&&) -> ArchiveXmlProvider& = delete;
  ~ArchiveXmlProvider();

  auto open_archive(std::string_view path, ArchiveMode mode) -> std::expected<void, Error>;
  auto close_archive() -> void;

  auto read_entry(std::string_view path) -> std::expected<std::vector<uint8_t>, Error>;
  auto write_entry(std::string_view path, std::span<const uint8_t> data, CompressionLevel level)
      -> std::expected<void, Error>;

  auto serialize_stack(OraDocument const& doc) -> std::string;
  auto deserialize_stack(std::span<const uint8_t> xml_bytes) -> std::expected<OraDocument, Error>;

private:
  unzFile unzip_ = nullptr;
  zipFile zip_ = nullptr;
};

} // namespace ora::detail

#endif // OPENRASTERPP_ARCHIVE_XML_PROVIDER_HPP__
