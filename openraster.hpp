/**
 * @file openraster.hpp
 * @brief OpenRaster（`.ora`）ドキュメントを読み書きする公開 API を定義します。
 */

#ifndef OPENRASTER_HPP__
#define OPENRASTER_HPP__

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

/**
 * @namespace ora
 * @brief OpenRaster ドキュメントの構築・入出力・合成を提供する名前空間です。
 */
namespace ora {

/**
 * @brief OpenRasterで定義されているブレンドモード
 */
enum class BlendMode {
  SrcOver,      ///< 通常合成（`svg:src-over`）です。
  Multiply,     ///< 乗算（`svg:multiply`）です。
  Screen,       ///< スクリーン（`svg:screen`）です。
  Overlay,      ///< オーバーレイ（`svg:overlay`）です。
  Darken,       ///< 比較暗（`svg:darken`）です。
  Lighten,      ///< 比較明（`svg:lighten`）です。
  ColorDodge,   ///< 覆い焼きカラー（`svg:color-dodge`）です。
  ColorBurn,    ///< 焼き込みカラー（`svg:color-burn`）です。
  HardLight,    ///< ハードライト（`svg:hard-light`）です。
  SoftLight,    ///< ソフトライト（`svg:soft-light`）です。
  Difference,   ///< 差の絶対値（`svg:difference`）です。
  Exclusion,    ///< 除外（`svg:exclusion`）です。
  Hue,          ///< 色相を適用します（`svg:hue`）。
  Saturation,   ///< 彩度を適用します（`svg:saturation`）。
  Color,        ///< 色を適用します（`svg:color`）。
  Luminosity,   ///< 輝度を適用します（`svg:luminosity`）。
  Plus,         ///< 加算合成（`svg:plus`）です。
  DstIn,        ///< 背景を前景アルファで切り抜きます（`svg:dst-in`）。
  DstOut,       ///< 背景から前景アルファ領域を除外します（`svg:dst-out`）。
  SrcAtop,      ///< 前景を背景上に重ねます（`svg:src-atop`）。
  DstAtop       ///< 背景を前景上に重ねます（`svg:dst-atop`）。
};

/**
 * @brief BlendModeをOpenRasterの属性文字列に変換
 * @param mode 変換対象のブレンドモードです。
 * @return `stack.xml` の `composite-op` に書き込める属性値を返します。
 */
[[nodiscard]]
auto to_string(BlendMode mode) -> std::string_view;

/**
 * @brief OpenRasterの属性文字列をBlendModeに変換
 * @param sv `stack.xml` の `composite-op` 属性文字列です。
 * @return 対応するブレンドモード、未対応の場合は `std::nullopt` を返します。
 */
[[nodiscard]]
auto from_string(std::string_view sv) -> std::optional<BlendMode>;

/**
 * @brief ライブラリ内の失敗理由を表現するエラー情報です。
 */
struct Error {
  /**
   * @brief エラーの大分類です。
   */
  enum class Code {
    ZipCreateFailed,   ///< ZIP アーカイブの新規作成に失敗しました。
    ZipAddFileFailed,  ///< ZIP アーカイブへのエントリ追加に失敗しました。
    ZipOpenFailed,     ///< ZIP アーカイブのオープンに失敗しました。
    ZipReadFailed,     ///< ZIP アーカイブの読み取りに失敗しました。
    XmlParseFailed,    ///< `stack.xml` の解析に失敗しました。
    PngEncodeFailed,   ///< PNG エンコードに失敗しました。
    PngDecodeFailed,   ///< PNG デコードに失敗しました。
    LayerImageNotFound,///< 想定していたレイヤー画像が見つかりませんでした。
    InvalidOraDocument,///< ORA ドキュメント構造が不正です。
    InvalidImageBuffer,///< 画像バッファのサイズまたは寸法が不正です。
  };

  Code code;             ///< エラー種別です。
  std::string message;   ///< 人が読める補足メッセージです。
};

/**
 * @brief 生のRGBA画像データを保持するクラス
 */
class ImageBuffer {
public:
  /**
   * @brief 既存の RGBA バイト列から画像バッファを生成します。
   * @param width 画像の幅（ピクセル）です。
   * @param height 画像の高さ（ピクセル）です。
   * @param rgba `width * height * 4` バイトの RGBA データです。
   * @return 妥当な場合は `ImageBuffer`、不正なサイズの場合はエラーを返します。
   */
  [[nodiscard]]
  static auto create(unsigned int width, unsigned int height, std::vector<uint8_t> rgba)
      -> std::expected<ImageBuffer, Error>;

  /**
   * @brief ゼロ初期化済みバッファを生成するファクトリ
   * @param width 画像の幅（ピクセル）です。
   * @param height 画像の高さ（ピクセル）です。
   * @param alpha 各ピクセルへ設定する初期アルファ値です。
   * @return 生成した空画像、寸法が不正な場合はエラーを返します。
   */
  [[nodiscard]]
  static auto make_blank(unsigned int width, unsigned int height, uint8_t alpha = 0)
      -> std::expected<ImageBuffer, Error>;

  ImageBuffer(const ImageBuffer&) = default;
  ImageBuffer(ImageBuffer&&) noexcept = default;
  auto operator=(const ImageBuffer&) -> ImageBuffer& = default;
  auto operator=(ImageBuffer&&) noexcept -> ImageBuffer& = default;
  ~ImageBuffer() = default;

  /**
   * @brief 画像の幅を返します。
   * @return 幅（ピクセル）です。
   */
  [[nodiscard]] auto width() const noexcept -> unsigned int { return width_; }

  /**
   * @brief 画像の高さを返します。
   * @return 高さ（ピクセル）です。
   */
  [[nodiscard]] auto height() const noexcept -> unsigned int { return height_; }

  /**
   * @brief 読み取り専用の RGBA バイト列ビューを返します。
   * @return 先頭から `width() * height() * 4` バイトの読み取り専用スパンです。
   */
  [[nodiscard]] auto rgba() const noexcept -> std::span<const uint8_t> { return rgba_; }

  /**
   * @brief 書き込み可能な RGBA バイト列ビューを返します。
   * @return 画像本体を直接編集できるスパンです。
   */
  [[nodiscard]] auto rgba_mut() noexcept -> std::span<uint8_t> { return rgba_; }

private:
  ImageBuffer(unsigned int width, unsigned int height, std::vector<uint8_t> rgba)
      : width_(width), height_(height), rgba_(std::move(rgba)) {}

  unsigned int width_;              ///< 画像の幅（ピクセル）です。
  unsigned int height_;             ///< 画像の高さ（ピクセル）です。
  std::vector<uint8_t> rgba_;       ///< 1 ピクセルあたり 4 バイトの RGBA バッファです。
};

/**
 * @brief レイヤーまたはスタック（グループ）を表現するノード
 */
struct Node {
  /**
   * @brief ノード種別です。
   */
  enum class Type {
    Layer, ///< 単一画像を持つレイヤーです。
    Stack  ///< 子ノードを束ねるスタック（グループ）です。
  } type;

  std::string name;                         ///< レイヤー名またはスタック名です。
  int x = 0;                               ///< 親座標系に対する X オフセットです。
  int y = 0;                               ///< 親座標系に対する Y オフセットです。
  bool visible = true;                     ///< `false` の場合は合成対象から除外されます。
  float opacity = 1.0f;                    ///< 不透明度です。`0.0f` で透明、`1.0f` で不透明です。
  BlendMode blend_mode = BlendMode::SrcOver; ///< 合成時に使用するブレンドモードです。
  std::vector<Node> children;              ///< `Type::Stack` の場合に保持される子ノード群です。
};

/**
 * @brief OpenRaster ドキュメント全体を表現するデータ構造です。
 */
struct OraDocument {
  unsigned int width;   ///< ドキュメント全体の幅（ピクセル）です。
  unsigned int height;  ///< ドキュメント全体の高さ（ピクセル）です。
  std::vector<Node> root_nodes; ///< ルート階層に存在するレイヤー／スタック群です。
  std::map<std::string, std::vector<uint8_t>> layer_images; ///< `data/<layer>.png` に保存される PNG バイト列です。
  std::optional<std::vector<uint8_t>> merged_image_png;     ///< `mergedimage.png` に保存されるプレビュー PNG です。
  std::optional<std::vector<uint8_t>> thumbnail_png;        ///< `Thumbnails/thumbnail.png` に保存されるサムネイル PNG です。
};

/**
 * @brief 単一レイヤーノードを簡潔に生成します。
 * @param name レイヤー名です。
 * @param x 親座標系に対する X オフセットです。
 * @param y 親座標系に対する Y オフセットです。
 * @param visible 表示状態です。
 * @param opacity 不透明度です。
 * @param mode ブレンドモードです。
 * @return 初期化済みの `Node::Type::Layer` ノードを返します。
 */
inline
auto layer(std::string const& name, int x = 0, int y = 0, bool visible = true, float opacity = 1.0f, BlendMode mode = BlendMode::SrcOver) {
  return Node{Node::Type::Layer, name, x, y, visible, opacity, mode, {}};
}

/**
 * @brief スタック（グループ）ノードを簡潔に生成します。
 * @param name スタック名です。
 * @param children 子ノード群です。
 * @param x 親座標系に対する X オフセットです。
 * @param y 親座標系に対する Y オフセットです。
 * @param visible 表示状態です。
 * @param opacity 不透明度です。
 * @param mode ブレンドモードです。
 * @return 初期化済みの `Node::Type::Stack` ノードを返します。
 */
inline
auto stack(std::string const& name, std::vector<Node> const& children, int x = 0, int y = 0, bool visible = true, float opacity = 1.0f, BlendMode mode = BlendMode::SrcOver) {
  return Node{Node::Type::Stack, name, x, y, visible, opacity, mode, children};
}

// --- Provider Abstraction ---

/**
 * @brief アーカイブをどの目的で開くかを指定します。
 */
enum class ArchiveMode {
  Read,  ///< 読み取り専用で開きます。
  Write  ///< 書き込み用に新規作成または更新します。
};

/**
 * @brief ZIP エントリ書き込み時の圧縮強度です。
 */
enum class CompressionLevel {
  None,    ///< 圧縮せずに保存します。
  Default, ///< 標準的な圧縮設定を使います。
  Best     ///< 可能な限り高い圧縮率を目指します。
};

/**
 * @brief PNG デコード後の画像データを表現します。
 */
struct DecodedImage {
  std::vector<uint8_t> rgba; ///< RGBA ピクセルデータです。
  unsigned int width;        ///< 画像の幅（ピクセル）です。
  unsigned int height;       ///< 画像の高さ（ピクセル）です。
};

/**
 * @brief OpenRasterのプラットフォーム依存処理を抽象化するコンセプト
 * @tparam T 満たすべき Provider 実装型です。
 * @details
 * `OraProvider` は、ZIP アーカイブ操作・PNG エンコード/デコード・
 * `stack.xml` のシリアライズ/デシリアライズをまとめて差し替えるための契約です。
 */
template<typename T>
concept OraProvider = requires(T& p, std::string_view path, std::span<const uint8_t> data,
                               unsigned int w, unsigned int h, const OraDocument& doc) {
  // アーカイブ操作
  { p.open_archive(path, ArchiveMode::Read) } -> std::same_as<std::expected<void, Error>>;
  { p.close_archive() };
  { p.read_entry(path) } -> std::same_as<std::expected<std::vector<uint8_t>, Error>>;
  { p.write_entry(path, data, CompressionLevel::Default) } -> std::same_as<std::expected<void, Error>>;

  // 画像コーデック
  { p.encode_png(data, w, h) } -> std::same_as<std::expected<std::vector<uint8_t>, Error>>;
  { p.decode_png(data) } -> std::same_as<std::expected<DecodedImage, Error>>;

  // XML構造化 (stack.xml)
  { p.serialize_stack(doc) } -> std::same_as<std::string>;
  { p.deserialize_stack(data) } -> std::same_as<std::expected<OraDocument, Error>>;
};

namespace detail {

/**
 * @brief 指定情報から `std::unexpected<Error>` を組み立てます。
 * @param code エラー種別です。
 * @param target エラー対象を表す文字列です。
 * @param detail 補足説明です。
 * @return 生成した `std::unexpected<Error>` を返します。
 */
[[nodiscard]] auto make_unexpected(Error::Code code, std::string_view target, std::string_view detail = {}) -> std::unexpected<Error>;

/**
 * @brief RGBA 画像を単純な平均化でリサイズします。
 * @param src 入力 RGBA バッファです。
 * @param sw 入力画像の幅です。
 * @param sh 入力画像の高さです。
 * @param dw 出力画像の幅です。
 * @param dh 出力画像の高さです。
 * @return リサイズ後の RGBA バッファです。
 */
[[nodiscard]] auto resize_image(const std::vector<uint8_t>& src, unsigned int sw, unsigned int sh, unsigned int dw, unsigned int dh) -> std::vector<uint8_t>;

/**
 * @brief `stack.xml` を解析して `OraDocument` の構造部分を復元します。
 * @param xml_bytes `stack.xml` の UTF-8 バイト列です。
 * @return 復元したドキュメント、解析に失敗した場合はエラーを返します。
 */
[[nodiscard]] auto deserialize_stack(std::span<const uint8_t> xml_bytes) -> std::expected<OraDocument, Error>;

/**
 * @brief 1 枚のレイヤー画像をキャンバスへ合成します。
 * @param canvas 合成先 RGBA バッファです。
 * @param cw 合成先キャンバスの幅です。
 * @param ch 合成先キャンバスの高さです。
 * @param layer 合成するレイヤー画像です。
 * @param lx レイヤーの X オフセットです。
 * @param ly レイヤーの Y オフセットです。
 * @param opacity レイヤー不透明度です。
 * @param mode ブレンドモードです。
 */
auto blend_layer(std::vector<uint8_t>& canvas, unsigned int cw, unsigned int ch, const ImageBuffer& layer, int lx, int ly, float opacity, BlendMode mode) -> void;

/**
 * @brief 再帰的にレイヤー・スタックを合成し、最終的な結合画像を生成する
 * @param canvas 合成先キャンバスです。
 * @param cw キャンバス幅です。
 * @param ch キャンバス高さです。
 * @param nodes 合成対象ノード列です。
 * @param layer_images レイヤー名から画像バッファへの対応表です。
 * @param parent_x 親スタック由来の X オフセットです。
 * @param parent_y 親スタック由来の Y オフセットです。
 * @return 合成に成功した場合は空の `expected`、失敗時はエラーを返します。
 */
auto process_blend(std::vector<uint8_t>& canvas, unsigned int cw, unsigned int ch,
                   std::span<const Node> nodes, const std::map<std::string, ImageBuffer>& layer_images,
                   int parent_x = 0, int parent_y = 0) -> std::expected<void, Error>;

} // namespace detail

/**
 * @brief Provider を使って `ImageBuffer` を PNG バイト列へ変換します。
 * @tparam Provider `OraProvider` を満たす実装型です。
 * @param provider PNG エンコードを担当する Provider です。
 * @param image 変換対象の画像です。
 * @return エンコードした PNG バイト列、失敗時はエラーを返します。
 */
template<OraProvider Provider>
[[nodiscard]]
auto encode_png(Provider& provider, const ImageBuffer& image)
    -> std::expected<std::vector<uint8_t>, Error> {
  return provider.encode_png(image.rgba(), image.width(), image.height());
}

/**
 * @brief レイヤー群から `mergedimage.png` と `thumbnail.png` を再生成します。
 * @tparam Provider `OraProvider` を満たす実装型です。
 * @param provider PNG エンコード/デコードを担当する Provider です。
 * @param doc プレビュー画像を書き戻す対象ドキュメントです。
 * @return 生成に成功した場合は空の `expected`、失敗時はエラーを返します。
 * @note 既存の `merged_image_png` と `thumbnail_png` は上書きされます。
 */
template<OraProvider Provider>
auto render_preview_and_thumbnail(Provider& provider, OraDocument& doc)
    -> std::expected<void, Error> {
  if (doc.width == 0 || doc.height == 0) {
    return detail::make_unexpected(Error::Code::InvalidOraDocument, "image", "invalid dimensions");
  }

  auto decoded_layer_images = std::map<std::string, ImageBuffer>{};
  for (auto const& [name, png_bytes] : doc.layer_images) {
    auto decoded = provider.decode_png(png_bytes);
    if (!decoded) {
      return std::unexpected(decoded.error());
    }
    auto buffer = ImageBuffer::create(decoded->width, decoded->height, std::move(decoded->rgba));
    if (!buffer) {
      return std::unexpected(buffer.error());
    }
    decoded_layer_images.emplace(name, std::move(*buffer));
  }

  auto merged_rgba = std::vector<uint8_t>(doc.width * doc.height * 4, 0);
  if (auto result = detail::process_blend(merged_rgba, doc.width, doc.height, doc.root_nodes, decoded_layer_images); !result) {
    return result;
  }

  auto merged_png = provider.encode_png(merged_rgba, doc.width, doc.height);
  if (!merged_png) {
    return std::unexpected(merged_png.error());
  }

  auto thumb_w = doc.width;
  auto thumb_h = doc.height;
  if (doc.width > 256 || doc.height > 256) {
    if (doc.width >= doc.height) {
      thumb_w = 256;
      thumb_h = std::max(1u, doc.height * thumb_w / doc.width);
    } else {
      thumb_h = 256;
      thumb_w = std::max(1u, doc.width * thumb_h / doc.height);
    }
  }

  auto thumb_rgba = thumb_w == doc.width && thumb_h == doc.height
      ? merged_rgba
      : detail::resize_image(merged_rgba, doc.width, doc.height, thumb_w, thumb_h);
  auto thumb_png = provider.encode_png(thumb_rgba, thumb_w, thumb_h);
  if (!thumb_png) {
    return std::unexpected(thumb_png.error());
  }

  doc.merged_image_png = std::move(*merged_png);
  doc.thumbnail_png = std::move(*thumb_png);
  return {};
}

/**
 * @brief OpenRasterファイルの読み込み
 * @tparam Provider `OraProvider` を満たす実装型です。
 * @param provider 入出力処理を担当する Provider です。
 * @param filename 読み込む `.ora` ファイル名またはパスです。
 * @return 読み込んだドキュメント、失敗時はエラーを返します。
 */
template<OraProvider Provider>
[[nodiscard]]
auto read(Provider& provider, std::string_view filename) -> std::expected<OraDocument, Error> {
  auto open_res = provider.open_archive(filename, ArchiveMode::Read);
  if (!open_res) return std::unexpected(open_res.error());
  auto cleanup = [&provider] { provider.close_archive(); };
  auto mime_bytes = provider.read_entry("mimetype");
  if (!mime_bytes || std::string_view(reinterpret_cast<const char*>(mime_bytes->data()), mime_bytes->size()) != "image/openraster") {
    cleanup(); return detail::make_unexpected(Error::Code::InvalidOraDocument, "mimetype");
  }
  auto xml_bytes = provider.read_entry("stack.xml");
  if (!xml_bytes) { cleanup(); return std::unexpected(xml_bytes.error()); }
  auto doc_res = provider.deserialize_stack(*xml_bytes);
  if (!doc_res) { cleanup(); return std::unexpected(doc_res.error()); }
  auto doc = std::move(*doc_res);
  auto load_images = [&](auto self, std::span<const Node> nodes) -> std::expected<void, Error> {
    for (auto const& node : nodes) {
      if (node.type == Node::Type::Layer) {
        auto path = "data/" + node.name + ".png";
        auto data = provider.read_entry(path); if (!data) return std::unexpected(data.error());
        auto img = provider.decode_png(*data); if (!img) return std::unexpected(img.error());
        doc.layer_images.emplace(node.name, std::move(*data));
      } else {
        auto res = self(self, node.children); if (!res) return res;
      }
    }
    return {};
  };
  if (auto res = load_images(load_images, doc.root_nodes); !res) { cleanup(); return std::unexpected(res.error()); }
  cleanup(); return doc;
}

/**
 * @brief OpenRasterファイルの書き込み
 *
 * `doc.layer_images` には各レイヤーの PNG バイト列を、
 * `doc.merged_image_png` と `doc.thumbnail_png` には、呼び出し側が
 * 用意した PNG バイト列を設定しておく必要があります。
 *
 * @tparam Provider `OraProvider` を満たす実装型です。
 * @param provider 入出力処理を担当する Provider です。
 * @param filename 書き込み先 `.ora` ファイル名またはパスです。
 * @param doc 書き込むドキュメントです。
 * @return 書き込みに成功した場合は空の `expected`、失敗時はエラーを返します。
 */
template<OraProvider Provider>
[[nodiscard]]
auto write(Provider& provider, std::string_view filename, const OraDocument& doc) -> std::expected<void, Error> {
  auto open_res = provider.open_archive(filename, ArchiveMode::Write);
  if (!open_res) return std::unexpected(open_res.error());
  auto cleanup = [&provider] { provider.close_archive(); };

  std::string_view mimetype = "image/openraster";
  if (auto res = provider.write_entry("mimetype", std::span(reinterpret_cast<const uint8_t*>(mimetype.data()), mimetype.size()), CompressionLevel::None); !res) { cleanup(); return res; }

  auto xml = provider.serialize_stack(doc);
  if (auto res = provider.write_entry("stack.xml", std::span(reinterpret_cast<const uint8_t*>(xml.data()), xml.size()), CompressionLevel::Default); !res) { cleanup(); return res; }

  for (auto const& [name, png] : doc.layer_images) {
    if (auto res = provider.write_entry("data/" + name + ".png", png, CompressionLevel::Default); !res) { cleanup(); return res; }
  }

  if (!doc.merged_image_png) {
    cleanup(); return detail::make_unexpected(Error::Code::InvalidOraDocument, "mergedimage.png", "caller-provided preview asset is required");
  }
  if (!doc.thumbnail_png) {
    cleanup(); return detail::make_unexpected(Error::Code::InvalidOraDocument, "Thumbnails/thumbnail.png", "caller-provided thumbnail asset is required");
  }

  if (auto res = provider.write_entry("mergedimage.png", *doc.merged_image_png, CompressionLevel::Default); !res) { cleanup(); return res; }
  if (auto res = provider.write_entry("Thumbnails/thumbnail.png", *doc.thumbnail_png, CompressionLevel::Default); !res) { cleanup(); return res; }

  cleanup(); return {};
}

/**
 * @brief 既存のコードベースと互換性を保つためのデフォルトプロバイダ
 * @details ZIP および PNG 処理の既定実装を利用する前方宣言です。
 */
class DefaultOraProvider;

/**
 * @brief デフォルトプロバイダを使用した読み込み（後方互換用）
 * @param filename 読み込む `.ora` ファイル名またはパスです。
 * @return 読み込んだドキュメント、失敗時はエラーを返します。
 */
[[nodiscard]]
auto read(std::string_view filename) -> std::expected<OraDocument, Error>;

/**
 * @brief ImageBuffer を PNG バイト列へ変換
 * @param image 変換対象の画像です。
 * @return PNG バイト列、失敗時はエラーを返します。
 */
[[nodiscard]]
auto encode_png(const ImageBuffer& image) -> std::expected<std::vector<uint8_t>, Error>;

/**
 * @brief レイヤー PNG から mergedimage / thumbnail を生成して doc に設定
 * @param doc 画像を追加設定する対象ドキュメントです。
 * @return 生成に成功した場合は空の `expected`、失敗時はエラーを返します。
 */
auto render_preview_and_thumbnail(OraDocument& doc) -> std::expected<void, Error>;

/**
 * @brief デフォルトプロバイダを使用した書き込み（後方互換用）
 *
 * `doc.layer_images` には各レイヤーの PNG バイト列を、
 * `doc.merged_image_png` と `doc.thumbnail_png` には、呼び出し側が
 * 用意した PNG バイト列を設定しておく必要があります。
 *
 * @param filename 書き込み先 `.ora` ファイル名またはパスです。
 * @param doc 書き込むドキュメントです。
 * @return 書き込みに成功した場合は空の `expected`、失敗時はエラーを返します。
 */
[[nodiscard]]
auto write(std::string_view filename, const OraDocument& doc) -> std::expected<void, Error>;

} // namespace ora

#endif // OPENRASTER_HPP__
