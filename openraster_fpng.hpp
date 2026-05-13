/**
 * @file openraster_fpng.hpp
 * @brief fpng を使用した OpenRaster PNG バックエンドを提供します。
 */

#ifndef OPENRASTER_FPNG_HPP__
#define OPENRASTER_FPNG_HPP__

#include "openraster.hpp"

namespace ora::fpng {

/**
 * @brief fpng を使用して画像を PNG バイト列へエンコードします。
 * @param image 変換対象の画像です。
 * @return エンコードされた PNG バイト列、またはエラーを返します。
 */
[[nodiscard]]
auto encode_png(ImageBuffer const& image) -> std::expected<std::vector<uint8_t>, Error>;

/**
 * @brief fpng を使用して ORA ファイルを読み込みます。
 * @param filename ファイルパスです。
 * @return 読み込まれたドキュメント、またはエラーを返します。
 */
[[nodiscard]]
auto read(std::string_view filename) -> std::expected<OraDocument, Error>;

/**
 * @brief fpng を使用して ORA ファイルを書き込みます。
 * @param filename ファイルパスです。
 * @param doc 書き込むドキュメントです。
 * @return 成功時は空の expected、失敗時はエラーを返します。
 */
[[nodiscard]]
auto write(std::string_view filename, OraDocument const& doc) -> std::expected<void, Error>;

/**
 * @brief fpng を使用してプレビューとサムネイルを生成します。
 * @param doc 生成結果を書き戻すドキュメントです。
 * @return 成功時は空の expected、失敗時はエラーを返します。
 */
auto render_preview_and_thumbnail(OraDocument& doc) -> std::expected<void, Error>;

} // namespace ora::fpng

#endif // OPENRASTER_FPNG_HPP__
