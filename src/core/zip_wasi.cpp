/**
 * @file zip_wasi.cpp
 * @brief WASI 用の内蔵 ZIP 実装です。zlib の raw deflate のみ使います。
 *
 * minizip(-ng) は wasip1 で動作しないため（ビルド失敗・実行時クラッシュ）、
 * WASI ビルド時のみ `ArchiveXmlProvider` の実体をこのファイルで提供します。
 * 既存環境（Linux/macOS/Windows）は `openraster_core.cpp` 側の
 * minizip-ng 実装をそのまま使うため影響ありません。
 *
 * 対応範囲: store（無圧縮）と deflate の読み書き。ZIP64・分割・暗号化は対象外。
 */

#include "src/core/archive_xml_provider.hpp"

#if defined(__wasi__)

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#include <zlib.h>

namespace ora::detail {

namespace {

auto put_le16(std::vector<uint8_t>& out, uint16_t value) -> void {
  out.push_back(static_cast<uint8_t>(value & 0xFFU));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFU));
}

auto put_le32(std::vector<uint8_t>& out, uint32_t value) -> void {
  for (int i = 0; i < 4; ++i) {
    out.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFFU));
  }
}

auto get_le16(const uint8_t* p) -> uint16_t {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

auto get_le32(const uint8_t* p) -> uint32_t {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

auto deflate_raw(std::span<const uint8_t> in) -> std::vector<uint8_t> {
  auto out = std::vector<uint8_t>(deflateBound(nullptr, static_cast<uLong>(in.size())));
  auto stream = z_stream{};
  stream.next_in = const_cast<Bytef*>(in.data());
  stream.avail_in = static_cast<uInt>(in.size());
  stream.next_out = out.data();
  stream.avail_out = static_cast<uInt>(out.size());
  if (deflateInit2(&stream, 6, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
    return {};
  }
  auto const result = deflate(&stream, Z_FINISH);
  out.resize(out.size() - stream.avail_out);
  deflateEnd(&stream);
  if (result != Z_STREAM_END) {
    return {};
  }
  return out;
}

auto inflate_raw(std::span<const uint8_t> in, size_t expected) -> std::vector<uint8_t> {
  auto out = std::vector<uint8_t>(expected != 0 ? expected : in.size() * 3 + 64);
  auto stream = z_stream{};
  stream.next_in = const_cast<Bytef*>(in.data());
  stream.avail_in = static_cast<uInt>(in.size());
  stream.next_out = out.data();
  stream.avail_out = static_cast<uInt>(out.size());
  if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
    return {};
  }
  auto const result = inflate(&stream, Z_FINISH);
  out.resize(out.size() - stream.avail_out);
  inflateEnd(&stream);
  if (result != Z_STREAM_END) {
    return {};
  }
  return out;
}

struct CentralRecord {
  uint32_t crc;
  uint32_t compressed_size;
  uint32_t uncompressed_size;
  uint32_t local_header_offset;
  uint16_t method;
};

auto build_zip(const std::map<std::string, std::vector<uint8_t>, std::less<>>& entries, bool compress) -> std::vector<uint8_t> {
  auto out = std::vector<uint8_t>{};
  auto records = std::vector<CentralRecord>{};
  auto names = std::vector<std::string>{};
  for (auto const& [name, data] : entries) {
    auto const crc = static_cast<uint32_t>(crc32(0, data.data(), static_cast<uInt>(data.size())));
    auto const compressible = compress && name != "mimetype" && !data.empty();
    auto deflated = compressible ? deflate_raw(data) : std::vector<uint8_t>{};
    auto use_deflate = compressible && !deflated.empty() && deflated.size() < data.size();
    auto const& stored = use_deflate ? deflated : data;
    auto const method = use_deflate ? 8 : 0;
    auto const offset = static_cast<uint32_t>(out.size());
    put_le32(out, 0x04034B50U);
    put_le16(out, 20);
    put_le16(out, 0x0800);
    put_le16(out, static_cast<uint16_t>(method));
    put_le16(out, 0);
    put_le16(out, 0x21);
    put_le32(out, crc);
    put_le32(out, static_cast<uint32_t>(stored.size()));
    put_le32(out, static_cast<uint32_t>(data.size()));
    put_le16(out, static_cast<uint16_t>(name.size()));
    put_le16(out, 0);
    out.insert(out.end(), name.begin(), name.end());
    out.insert(out.end(), stored.begin(), stored.end());
    records.push_back({crc, static_cast<uint32_t>(stored.size()), static_cast<uint32_t>(data.size()), offset, static_cast<uint16_t>(method)});
    names.push_back(name);
  }
  auto const central_offset = static_cast<uint32_t>(out.size());
  for (size_t i = 0; i < names.size(); ++i) {
    put_le32(out, 0x02014B50U);
    put_le16(out, 20);
    put_le16(out, 20);
    put_le16(out, 0x0800);
    put_le16(out, records[i].method);
    put_le16(out, 0);
    put_le16(out, 0x21);
    put_le32(out, records[i].crc);
    put_le32(out, records[i].compressed_size);
    put_le32(out, records[i].uncompressed_size);
    put_le16(out, static_cast<uint16_t>(names[i].size()));
    put_le16(out, 0);
    put_le16(out, 0);
    put_le16(out, 0);
    put_le16(out, 0);
    put_le32(out, 0);
    put_le32(out, records[i].local_header_offset);
    out.insert(out.end(), names[i].begin(), names[i].end());
  }
  auto const central_size = static_cast<uint32_t>(out.size()) - central_offset;
  put_le32(out, 0x06054B50U);
  put_le16(out, 0);
  put_le16(out, 0);
  put_le16(out, static_cast<uint16_t>(names.size()));
  put_le16(out, static_cast<uint16_t>(names.size()));
  put_le32(out, central_size);
  put_le32(out, central_offset);
  put_le16(out, 0);
  return out;
}

auto find_entry(std::span<const uint8_t> zip, std::string_view name, std::vector<uint8_t>& out) -> bool {
  if (zip.size() < 22) {
    return false;
  }
  auto eocd = static_cast<size_t>(-1);
  auto const lo = zip.size() > 22 + 65536 ? zip.size() - 22 - 65536 : 0;
  for (auto i = zip.size() - 22;; --i) {
    if (get_le32(zip.data() + i) == 0x06054B50U) {
      eocd = i;
      break;
    }
    if (i == lo) {
      break;
    }
  }
  if (eocd == static_cast<size_t>(-1)) {
    return false;
  }
  auto const count = get_le16(zip.data() + eocd + 10);
  auto pos = static_cast<size_t>(get_le32(zip.data() + eocd + 16));
  for (int i = 0; i < count; ++i) {
    if (pos + 46 > zip.size() || get_le32(zip.data() + pos) != 0x02014B50U) {
      return false;
    }
    auto const method = get_le16(zip.data() + pos + 10);
    auto const compressed_size = get_le32(zip.data() + pos + 20);
    auto const uncompressed_size = get_le32(zip.data() + pos + 24);
    auto const name_len = get_le16(zip.data() + pos + 28);
    auto const extra_len = get_le16(zip.data() + pos + 30);
    auto const comment_len = get_le16(zip.data() + pos + 32);
    auto const header_offset = static_cast<size_t>(get_le32(zip.data() + pos + 42));
    std::string_view entry_name(reinterpret_cast<const char*>(zip.data() + pos + 46), name_len);
    if (entry_name == name) {
      auto const name_len_local = static_cast<size_t>(get_le16(zip.data() + header_offset + 26));
      auto const extra_len_local = static_cast<size_t>(get_le16(zip.data() + header_offset + 28));
      auto const data_begin = header_offset + 30 + name_len_local + extra_len_local;
      std::span<const uint8_t> body(zip.data() + data_begin, compressed_size);
      if (method == 0) {
        out.assign(body.begin(), body.end());
        return true;
      }
      if (method == 8) {
        out = inflate_raw(body, uncompressed_size);
        return !out.empty() || uncompressed_size == 0;
      }
      return false;
    }
    pos += 46 + name_len + extra_len + comment_len;
  }
  return false;
}

auto read_whole_file(const std::string& path, std::vector<uint8_t>& out) -> bool {
  auto* file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) {
    return false;
  }
  std::fseek(file, 0, SEEK_END);
  auto const size = std::ftell(file);
  std::fseek(file, 0, SEEK_SET);
  out.resize(size > 0 ? static_cast<size_t>(size) : 0);
  auto const read = out.empty() ? 0 : std::fread(out.data(), 1, out.size(), file);
  std::fclose(file);
  return static_cast<long>(read) == size;
}

auto write_whole_file(const std::string& path, std::span<const uint8_t> data) -> bool {
  auto* file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) {
    return false;
  }
  auto const written = std::fwrite(data.data(), 1, data.size(), file);
  std::fclose(file);
  return written == data.size();
}

} // namespace

ArchiveXmlProvider::~ArchiveXmlProvider() {
  close_archive();
}

auto ArchiveXmlProvider::open_archive(std::string_view path, ArchiveMode mode) -> std::expected<void, Error> {
  archive_path_ = std::string{path};
  archive_mode_ = mode;
  if (mode == ArchiveMode::Read) {
    if (!read_whole_file(archive_path_, archive_bytes_)) {
      return make_unexpected(Error::Code::ZipOpenFailed, path);
    }
  } else {
    pending_entries_.clear();
  }
  return {};
}

auto ArchiveXmlProvider::close_archive() -> void {
  if (archive_mode_ == ArchiveMode::Write && !archive_path_.empty() && !pending_entries_.empty()) {
    auto zip = build_zip(pending_entries_, true);
    write_whole_file(archive_path_, zip);
    pending_entries_.clear();
  }
  // read→close→read の再利用に備え、都度クリアする。
  archive_bytes_.clear();
  archive_bytes_.shrink_to_fit();
  archive_path_.clear();
}

auto ArchiveXmlProvider::read_entry(std::string_view path) -> std::expected<std::vector<uint8_t>, Error> {
  auto out = std::vector<uint8_t>{};
  if (!find_entry(archive_bytes_, path, out)) {
    return make_unexpected(Error::Code::InvalidOraDocument, path, "entry not found");
  }
  return out;
}

auto ArchiveXmlProvider::write_entry(std::string_view path, std::span<const uint8_t> data, CompressionLevel /*level*/)
    -> std::expected<void, Error> {
  pending_entries_[std::string{path}] = std::vector<uint8_t>(data.begin(), data.end());
  return {};
}

} // namespace ora::detail

#endif // defined(__wasi__)
