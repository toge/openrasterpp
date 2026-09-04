#ifndef OPENRASTERPP_ARCHIVE_XML_PROVIDER_HPP__
#define OPENRASTERPP_ARCHIVE_XML_PROVIDER_HPP__

#include "openraster.hpp"

#if defined(__wasi__)
// WASI では minizip(-ng) が動作しないため、zlib 直利用の内蔵 ZIP 実装を使う。
// minizip ヘッダは不要。既存環境（Linux/macOS/Windows）には影響しない。
#include <map>
#else
#include "unzip.h"
#include "zip.h"
#endif

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
#if defined(__wasi__)
  std::string archive_path_;
  ArchiveMode archive_mode_ = ArchiveMode::Read;
  // 書き込み時に集めたエントリ。close 時に一括で ZIP 化する。
  std::map<std::string, std::vector<uint8_t>, std::less<>> pending_entries_;
  // 読み込み時にメモリへ展開した ZIP 全体。
  std::vector<uint8_t> archive_bytes_;
#else
  unzFile unzip_ = nullptr;
  zipFile zip_ = nullptr;
#endif
};

} // namespace ora::detail

#endif // OPENRASTERPP_ARCHIVE_XML_PROVIDER_HPP__
