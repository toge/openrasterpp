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

namespace ora {

/**
 * @brief OpenRasterで定義されているブレンドモード
 */
enum class BlendMode {
  SrcOver,      // svg:src-over (Normal)
  Multiply,     // svg:multiply
  Screen,       // svg:screen
  Overlay,      // svg:overlay
  Darken,       // svg:darken
  Lighten,      // svg:lighten
  ColorDodge,   // svg:color-dodge
  ColorBurn,    // svg:color-burn
  HardLight,    // svg:hard-light
  SoftLight,    // svg:soft-light
  Difference,   // svg:difference
  Exclusion,    // svg:exclusion
  Hue,          // svg:hue
  Saturation,   // svg:saturation
  Color,        // svg:color
  Luminosity,   // svg:luminosity
  Plus,         // svg:plus (Addition)
  DstIn,        // svg:dst-in
  DstOut,       // svg:dst-out
  SrcAtop,      // svg:src-atop
  DstAtop       // svg:dst-atop
};

/**
 * @brief BlendModeをOpenRasterの属性文字列に変換
 */
[[nodiscard]]
auto to_string(BlendMode mode) -> std::string_view;

/**
 * @brief OpenRasterの属性文字列をBlendModeに変換
 */
[[nodiscard]]
auto from_string(std::string_view sv) -> std::optional<BlendMode>;

struct Error {
  enum class Code {
    ZipCreateFailed,
    ZipAddFileFailed,
    ZipOpenFailed,
    ZipReadFailed,
    XmlParseFailed,
    PngEncodeFailed,
    PngDecodeFailed,
    LayerImageNotFound,
    InvalidOraDocument,
    InvalidImageBuffer,
  };

  Code code;
  std::string message;
};

/**
 * @brief 生のRGBA画像データを保持するクラス
 */
class ImageBuffer {
public:
  [[nodiscard]]
  static auto create(unsigned int width, unsigned int height, std::vector<uint8_t> rgba)
      -> std::expected<ImageBuffer, Error>;

  /**
   * @brief ゼロ初期化済みバッファを生成するファクトリ
   */
  [[nodiscard]]
  static auto make_blank(unsigned int width, unsigned int height, uint8_t alpha = 0)
      -> std::expected<ImageBuffer, Error>;

  ImageBuffer(const ImageBuffer&) = default;
  ImageBuffer(ImageBuffer&&) noexcept = default;
  auto operator=(const ImageBuffer&) -> ImageBuffer& = default;
  auto operator=(ImageBuffer&&) noexcept -> ImageBuffer& = default;
  ~ImageBuffer() = default;

  [[nodiscard]] auto width() const noexcept -> unsigned int { return width_; }
  [[nodiscard]] auto height() const noexcept -> unsigned int { return height_; }
  [[nodiscard]] auto rgba() const noexcept -> std::span<const uint8_t> { return rgba_; }
  [[nodiscard]] auto rgba_mut() noexcept -> std::span<uint8_t> { return rgba_; }

private:
  ImageBuffer(unsigned int width, unsigned int height, std::vector<uint8_t> rgba)
      : width_(width), height_(height), rgba_(std::move(rgba)) {}

  unsigned int width_;
  unsigned int height_;
  std::vector<uint8_t> rgba_; // 4 bytes per pixel (R, G, B, A)
};

/**
 * @brief レイヤーまたはスタック（グループ）を表現するノード
 */
struct Node {
  enum class Type { Layer, Stack } type;
  std::string name;
  int x = 0;
  int y = 0;
  bool visible = true;
  float opacity = 1.0f;
  BlendMode blend_mode = BlendMode::SrcOver;

  // Type::Stackの場合のみ使用される子ノード
  std::vector<Node> children;
};

struct OraDocument {
  unsigned int width;
  unsigned int height;
  std::vector<Node> root_nodes;
  // data/<layer>.png に書き出される PNG バイト列
  std::map<std::string, std::vector<uint8_t>> layer_images;
  // mergedimage.png に書き出される PNG バイト列
  std::optional<std::vector<uint8_t>> merged_image_png;
  // Thumbnails/thumbnail.png に書き出される PNG バイト列
  std::optional<std::vector<uint8_t>> thumbnail_png;
};

inline
auto layer(std::string const& name, int x = 0, int y = 0, bool visible = true, float opacity = 1.0f, BlendMode mode = BlendMode::SrcOver) {
  return Node{Node::Type::Layer, name, x, y, visible, opacity, mode, {}};
}

inline
auto stack(std::string const& name, std::vector<Node> const& children, int x = 0, int y = 0, bool visible = true, float opacity = 1.0f, BlendMode mode = BlendMode::SrcOver) {
  return Node{Node::Type::Stack, name, x, y, visible, opacity, mode, children};
}

// --- Provider Abstraction ---

enum class ArchiveMode { Read, Write };
enum class CompressionLevel { None, Default, Best };

struct DecodedImage {
  std::vector<uint8_t> rgba;
  unsigned int width;
  unsigned int height;
};

/**
 * @brief OpenRasterのプラットフォーム依存処理を抽象化するコンセプト
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

[[nodiscard]] auto make_unexpected(Error::Code code, std::string_view target, std::string_view detail = {}) -> std::unexpected<Error>;
[[nodiscard]] auto resize_image(const std::vector<uint8_t>& src, unsigned int sw, unsigned int sh, unsigned int dw, unsigned int dh) -> std::vector<uint8_t>;
[[nodiscard]] auto deserialize_stack(std::span<const uint8_t> xml_bytes) -> std::expected<OraDocument, Error>;
auto blend_layer(std::vector<uint8_t>& canvas, unsigned int cw, unsigned int ch, const ImageBuffer& layer, int lx, int ly, float opacity, BlendMode mode) -> void;

/**
 * @brief 再帰的にレイヤー・スタックを合成し、最終的な結合画像を生成する
 */
auto process_blend(std::vector<uint8_t>& canvas, unsigned int cw, unsigned int ch,
                   std::span<const Node> nodes, const std::map<std::string, ImageBuffer>& layer_images,
                   int parent_x = 0, int parent_y = 0) -> std::expected<void, Error>;

} // namespace detail

template<OraProvider Provider>
[[nodiscard]]
auto encode_png(Provider& provider, const ImageBuffer& image)
    -> std::expected<std::vector<uint8_t>, Error> {
  return provider.encode_png(image.rgba(), image.width(), image.height());
}

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
 */
class DefaultOraProvider;

/**
 * @brief デフォルトプロバイダを使用した読み込み（後方互換用）
 */
[[nodiscard]]
auto read(std::string_view filename) -> std::expected<OraDocument, Error>;

/**
 * @brief ImageBuffer を PNG バイト列へ変換
 */
[[nodiscard]]
auto encode_png(const ImageBuffer& image) -> std::expected<std::vector<uint8_t>, Error>;

/**
 * @brief レイヤー PNG から mergedimage / thumbnail を生成して doc に設定
 */
auto render_preview_and_thumbnail(OraDocument& doc) -> std::expected<void, Error>;

/**
 * @brief デフォルトプロバイダを使用した書き込み（後方互換用）
 *
 * `doc.layer_images` には各レイヤーの PNG バイト列を、
 * `doc.merged_image_png` と `doc.thumbnail_png` には、呼び出し側が
 * 用意した PNG バイト列を設定しておく必要があります。
 */
[[nodiscard]]
auto write(std::string_view filename, const OraDocument& doc) -> std::expected<void, Error>;

} // namespace ora

#endif // OPENRASTER_HPP__
