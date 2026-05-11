/**
 * @file openraster.cpp
 * @brief `openraster.hpp` で宣言した OpenRaster API の既定実装を提供します。
 */

#include "openraster.hpp"

#include <cctype>
#include <array>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <expected>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <set>
#include <tuple>
#include <utility>


namespace ora {

namespace {

/**
 * @brief 線形色空間上の RGB 値です。
 */
struct linear_rgb {
  float red;   ///< 赤成分です。
  float green; ///< 緑成分です。
  float blue;  ///< 青成分です。
};

/**
 * @brief 線形色空間上の RGBA 値です。
 */
struct linear_rgba {
  linear_rgb rgb; ///< RGB 成分です。
  float alpha;    ///< アルファ成分です。
};

/**
 * @brief Porter-Duff 合成時に使う係数です。
 */
struct composite_factors {
  float source;   ///< ソース側へ掛ける係数です。
  float backdrop; ///< 背景側へ掛ける係数です。
};

constexpr auto clamp_unit(float value) -> float { return std::clamp(value, 0.0f, 1.0f); }
constexpr auto absolute_difference(float lhs, float rhs) -> float { return lhs < rhs ? rhs - lhs : lhs - rhs; }
constexpr auto approximately_equal(float lhs, float rhs, float epsilon = 1.0e-5f) -> bool { return absolute_difference(lhs, rhs) <= epsilon; }
constexpr auto clamp_rgb(linear_rgb color) -> linear_rgb { return {clamp_unit(color.red), clamp_unit(color.green), clamp_unit(color.blue)}; }
constexpr auto add(linear_rgb lhs, linear_rgb rhs) -> linear_rgb { return {lhs.red + rhs.red, lhs.green + rhs.green, lhs.blue + rhs.blue}; }
constexpr auto multiply(linear_rgb color, float scalar) -> linear_rgb { return {color.red * scalar, color.green * scalar, color.blue * scalar}; }
constexpr auto min_component(linear_rgb color) -> float { return std::min({color.red, color.green, color.blue}); }
constexpr auto max_component(linear_rgb color) -> float { return std::max({color.red, color.green, color.blue}); }

constexpr auto constexpr_sqrt(float value) -> float {
  if (value <= 0.0f) return 0.0f;
  auto current = value > 1.0f ? value : 1.0f;
  for (int i = 0; i < 8; ++i) current = 0.5f * (current + value / current);
  return current;
}

auto srgb_to_linear_component(float value) -> float {
  return (value <= 0.04045f) ? (value / 12.92f) : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

auto linear_to_srgb_component(float value) -> float {
  auto const clamped = clamp_unit(value);
  return (clamped <= 0.0031308f) ? (12.92f * clamped) : (1.055f * std::pow(clamped, 1.0f / 2.4f) - 0.055f);
}

constexpr auto byte_to_unit(uint8_t value) -> float { return static_cast<float>(value) / 255.0f; }
constexpr auto unit_to_byte(float value) -> uint8_t { return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f); }
constexpr auto luminosity(linear_rgb color) -> float { return 0.3f * color.red + 0.59f * color.green + 0.11f * color.blue; }
constexpr auto saturation(linear_rgb color) -> float { return max_component(color) - min_component(color); }

constexpr auto clip_color(linear_rgb color) -> linear_rgb {
  auto const L = luminosity(color); auto const n = min_component(color); auto const x = max_component(color);
  if (n < 0.0f) color = { L + ((color.red - L) * L) / (L - n), L + ((color.green - L) * L) / (L - n), L + ((color.blue - L) * L) / (L - n) };
  if (x > 1.0f) color = { L + ((color.red - L) * (1.0f - L)) / (x - L), L + ((color.green - L) * (1.0f - L)) / (x - L), L + ((color.blue - L) * (1.0f - L)) / (x - L) };
  return color;
}

constexpr auto set_luminosity(linear_rgb color, float target_luminosity) -> linear_rgb {
  auto const delta = target_luminosity - luminosity(color);
  return clip_color({color.red + delta, color.green + delta, color.blue + delta});
}

constexpr auto set_saturation(linear_rgb color, float target_saturation) -> linear_rgb {
  auto components = std::array<float, 3>{color.red, color.green, color.blue};
  auto min_idx = 0; if (components[1] < components[min_idx]) min_idx = 1; if (components[2] < components[min_idx]) min_idx = 2;
  auto max_idx = 0; if (components[1] > components[max_idx]) max_idx = 1; if (components[2] > components[max_idx]) max_idx = 2;
  auto mid_idx = 3 - min_idx - max_idx;
  if (components[max_idx] > components[min_idx]) {
    components[mid_idx] = ((components[mid_idx] - components[min_idx]) * target_saturation) / (components[max_idx] - components[min_idx]);
    components[max_idx] = target_saturation;
  } else components[mid_idx] = components[max_idx] = 0.0f;
  components[min_idx] = 0.0f;
  return {components[0], components[1], components[2]};
}

constexpr auto blend_src_over(linear_rgb, linear_rgb s) -> linear_rgb { return s; }
constexpr auto blend_multiply(linear_rgb b, linear_rgb s) -> linear_rgb { return {b.red * s.red, b.green * s.green, b.blue * s.blue}; }
constexpr auto blend_screen(linear_rgb b, linear_rgb s) -> linear_rgb { return {b.red + s.red - b.red * s.red, b.green + s.green - b.green * s.green, b.blue + s.blue - b.blue * s.blue}; }
constexpr auto blend_overlay(linear_rgb b, linear_rgb s) -> linear_rgb {
  auto const f = [](float bv, float sv) { return bv <= 0.5f ? 2.0f * bv * sv : 1.0f - 2.0f * (1.0f - bv) * (1.0f - sv); };
  return {f(b.red, s.red), f(b.green, s.green), f(b.blue, s.blue)};
}
constexpr auto blend_darken(linear_rgb b, linear_rgb s) -> linear_rgb { return {std::min(b.red, s.red), std::min(b.green, s.green), std::min(b.blue, s.blue)}; }
constexpr auto blend_lighten(linear_rgb b, linear_rgb s) -> linear_rgb { return {std::max(b.red, s.red), std::max(b.green, s.green), std::max(b.blue, s.blue)}; }
constexpr auto blend_color_dodge(linear_rgb b, linear_rgb s) -> linear_rgb {
  auto const f = [](float bv, float sv) { return approximately_equal(bv, 0.0f) ? 0.0f : (approximately_equal(sv, 1.0f) ? 1.0f : std::min(1.0f, bv / (1.0f - sv))); };
  return {f(b.red, s.red), f(b.green, s.green), f(b.blue, s.blue)};
}
constexpr auto blend_color_burn(linear_rgb b, linear_rgb s) -> linear_rgb {
  auto const f = [](float bv, float sv) { return approximately_equal(bv, 1.0f) ? 1.0f : (approximately_equal(sv, 0.0f) ? 0.0f : 1.0f - std::min(1.0f, (1.0f - bv) / sv)); };
  return {f(b.red, s.red), f(b.green, s.green), f(b.blue, s.blue)};
}
constexpr auto blend_hard_light(linear_rgb b, linear_rgb s) -> linear_rgb {
  auto const f = [](float bv, float sv) { return sv <= 0.5f ? 2.0f * bv * sv : 1.0f - 2.0f * (1.0f - bv) * (1.0f - sv); };
  return {f(b.red, s.red), f(b.green, s.green), f(b.blue, s.blue)};
}
constexpr auto blend_soft_light(linear_rgb b, linear_rgb s) -> linear_rgb {
  auto const f = [](float bv, float sv) {
    if (sv <= 0.5f) return bv - (1.0f - 2.0f * sv) * bv * (1.0f - bv);
    auto const d = bv <= 0.25f ? ((16.0f * bv - 12.0f) * bv + 4.0f) * bv : constexpr_sqrt(bv);
    return bv + (2.0f * sv - 1.0f) * (d - bv);
  };
  return {f(b.red, s.red), f(b.green, s.green), f(b.blue, s.blue)};
}
constexpr auto blend_difference(linear_rgb b, linear_rgb s) -> linear_rgb { return {absolute_difference(b.red, s.red), absolute_difference(b.green, s.green), absolute_difference(b.blue, s.blue)}; }
constexpr auto blend_exclusion(linear_rgb b, linear_rgb s) -> linear_rgb { return {b.red + s.red - 2.0f * b.red * s.red, b.green + s.green - 2.0f * b.green * s.green, b.blue + s.blue - 2.0f * b.blue * s.blue}; }
constexpr auto blend_hue(linear_rgb b, linear_rgb s) -> linear_rgb { return set_luminosity(set_saturation(s, saturation(b)), luminosity(b)); }
constexpr auto blend_saturation(linear_rgb b, linear_rgb s) -> linear_rgb { return set_luminosity(set_saturation(b, saturation(s)), luminosity(b)); }
constexpr auto blend_color(linear_rgb b, linear_rgb s) -> linear_rgb { return set_luminosity(s, luminosity(b)); }
constexpr auto blend_luminosity(linear_rgb b, linear_rgb s) -> linear_rgb { return set_luminosity(b, luminosity(s)); }

constexpr auto apply_blend_function(BlendMode mode, linear_rgb b, linear_rgb s) -> linear_rgb {
  switch (mode) {
    case BlendMode::SrcOver: return blend_src_over(b, s);
    case BlendMode::Multiply: return blend_multiply(b, s);
    case BlendMode::Screen: return blend_screen(b, s);
    case BlendMode::Overlay: return blend_overlay(b, s);
    case BlendMode::Darken: return blend_darken(b, s);
    case BlendMode::Lighten: return blend_lighten(b, s);
    case BlendMode::ColorDodge: return blend_color_dodge(b, s);
    case BlendMode::ColorBurn: return blend_color_burn(b, s);
    case BlendMode::HardLight: return blend_hard_light(b, s);
    case BlendMode::SoftLight: return blend_soft_light(b, s);
    case BlendMode::Difference: return blend_difference(b, s);
    case BlendMode::Exclusion: return blend_exclusion(b, s);
    case BlendMode::Hue: return blend_hue(b, s);
    case BlendMode::Saturation: return blend_saturation(b, s);
    case BlendMode::Color: return blend_color(b, s);
    case BlendMode::Luminosity: return blend_luminosity(b, s);
    default: return s;
  }
}

constexpr auto composite_mode_factors(BlendMode mode, float as, float ab) -> composite_factors {
  switch (mode) {
    case BlendMode::Plus: return {1.0f, 1.0f};
    case BlendMode::DstIn: return {0.0f, as};
    case BlendMode::DstOut: return {0.0f, 1.0f - as};
    case BlendMode::SrcAtop: return {ab, 1.0f - as};
    case BlendMode::DstAtop: return {1.0f - ab, as};
    default: return {1.0f, 1.0f - as};
  }
}

constexpr auto uses_porter_duff_only(BlendMode mode) -> bool {
  return mode == BlendMode::Plus || mode == BlendMode::DstIn || mode == BlendMode::DstOut || mode == BlendMode::SrcAtop || mode == BlendMode::DstAtop;
}

/**
 * @brief RGBA バッファから 1 ピクセルを線形色空間へ展開して読み取ります。
 * @param rgba 元の RGBA バッファです。
 * @param index 読み取り開始位置です。
 * @return 線形色空間へ変換したピクセル値です。
 */
auto read_pixel(std::span<const uint8_t> rgba, std::size_t index) -> linear_rgba {
  return { { srgb_to_linear_component(byte_to_unit(rgba[index + 0])), srgb_to_linear_component(byte_to_unit(rgba[index + 1])), srgb_to_linear_component(byte_to_unit(rgba[index + 2])) }, byte_to_unit(rgba[index + 3]) };
}

/**
 * @brief 線形色空間の 1 ピクセルを RGBA バッファへ書き戻します。
 * @param rgba 書き込み先 RGBA バッファです。
 * @param index 書き込み開始位置です。
 * @param pixel 書き込むピクセル値です。
 */
auto write_pixel(std::vector<uint8_t>& rgba, std::size_t index, linear_rgba pixel) -> void {
  auto const crgb = clamp_rgb(pixel.rgb);
  rgba[index + 0] = unit_to_byte(linear_to_srgb_component(crgb.red));
  rgba[index + 1] = unit_to_byte(linear_to_srgb_component(crgb.green));
  rgba[index + 2] = unit_to_byte(linear_to_srgb_component(crgb.blue));
  rgba[index + 3] = unit_to_byte(pixel.alpha);
}

constexpr auto compose_pixel(BlendMode mode, linear_rgba b, linear_rgba s) -> linear_rgba {
  auto const interacted_s = uses_porter_duff_only(mode) ? s.rgb : add(multiply(s.rgb, 1.0f - b.alpha), multiply(apply_blend_function(mode, b.rgb, s.rgb), b.alpha));
  auto const factors = composite_mode_factors(mode, s.alpha, b.alpha);
  auto const out_a = s.alpha * factors.source + b.alpha * factors.backdrop;
  if (out_a <= 0.0f) return {{0,0,0}, 0};
  auto const out_pre = add(multiply(interacted_s, s.alpha * factors.source), multiply(b.rgb, b.alpha * factors.backdrop));
  return {clamp_rgb(multiply(out_pre, 1.0f / out_a)), clamp_unit(out_a)};
}

/**
 * @brief 基本的な XML エンティティ参照をデコードします。
 * @param str デコード対象文字列です。
 * @return 実体参照を復元した文字列です。
 */
auto decode_xml_entities(std::string_view str) -> std::string {
  auto res = std::string{};
  for (size_t i = 0; i < str.size(); ++i) {
    if (str[i] == '&') {
      auto end = str.find(';', i);
      if (end != std::string_view::npos) {
        auto entity = str.substr(i, end - i + 1);
        if (entity == "&lt;") { res += '<'; i = end; continue; }
        else if (entity == "&gt;") { res += '>'; i = end; continue; }
        else if (entity == "&amp;") { res += '&'; i = end; continue; }
        else if (entity == "&apos;") { res += '\''; i = end; continue; }
        else if (entity == "&quot;") { res += '"'; i = end; continue; }
      }
    }
    res += str[i];
  }
  return res;
}

/**
 * @brief `stack.xml` 解析中に扱う単一タグの表現です。
 */
struct xml_tag {
  enum { Start, End, SelfClosing } kind;
  std::string name;
  std::map<std::string, std::string> attrs;
};

/**
 * @brief XML 文字列から次のタグを 1 つだけ読み取ります。
 * @param xml 入力 XML 文字列です。
 * @param pos 読み取り位置です。返却時には次の探索位置へ進みます。
 * @return 読み取ったタグ、終端に達した場合は `std::nullopt` を返します。
 */
auto parse_next_tag(std::string_view xml, size_t& pos) -> std::optional<xml_tag> {
  pos = xml.find('<', pos); if (pos == std::string_view::npos) return std::nullopt;
  if (xml.substr(pos, 4) == "<!--") { pos = xml.find("-->", pos) + 3; return parse_next_tag(xml, pos); }
  if (xml[pos+1] == '?') { pos = xml.find("?>", pos) + 2; return parse_next_tag(xml, pos); }
  xml_tag tag; pos++;
  if (xml[pos] == '/') { tag.kind = xml_tag::End; pos++; } else tag.kind = xml_tag::Start;
  size_t start = pos; while (pos < xml.size() && !isspace(xml[pos]) && xml[pos] != '/' && xml[pos] != '>') pos++;
  tag.name = std::string{xml.substr(start, pos - start)};
  while (pos < xml.size() && xml[pos] != '>' && xml[pos] != '/') {
    while (pos < xml.size() && isspace(xml[pos])) pos++;
    if (xml[pos] == '>' || xml[pos] == '/') break;
    size_t attr_start = pos; while (pos < xml.size() && !isspace(xml[pos]) && xml[pos] != '=') pos++;
    std::string attr_name{xml.substr(attr_start, pos - attr_start)};
    pos = xml.find('=', pos) + 1; while (pos < xml.size() && isspace(xml[pos])) pos++;
    char q = xml[pos++]; size_t val_start = pos; pos = xml.find(q, pos);
    tag.attrs[attr_name] = decode_xml_entities(xml.substr(val_start, pos - val_start));
    pos++;
  }
  if (xml[pos] == '/') { tag.kind = xml_tag::SelfClosing; pos++; }
  pos++; return tag;
}

} // namespace

/**
 * @brief ZIP と PNG を使った既定の ORA 入出力 Provider 実装です。
 */

namespace detail {

/**
 * @brief `Error` を `std::unexpected` として生成します。
 * @param code エラー種別です。
 * @param target エラー対象です。
 * @param detail 補足説明です。
 * @return 生成した `std::unexpected<Error>` を返します。
 */
auto make_unexpected(Error::Code code, std::string_view target, std::string_view detail) -> std::unexpected<Error> {
  return std::unexpected(Error{code, std::string{target} + (detail.empty() ? "" : ": ") + std::string{detail}});
}

/**
 * @brief `stack.xml` を走査して `OraDocument` の構造を復元します。
 * @param xml_bytes `stack.xml` の UTF-8 バイト列です。
 * @return 復元したドキュメント、失敗時はエラーを返します。
 */
auto deserialize_stack(std::span<const uint8_t> xml_bytes) -> std::expected<OraDocument, Error> {
  try {
    std::string_view xml(reinterpret_cast<const char*>(xml_bytes.data()), xml_bytes.size());
    OraDocument doc;
    size_t pos = 0;
    std::set<std::string> layer_names;
    std::vector<std::size_t> path;

    auto get_attr = [](const std::map<std::string, std::string>& attrs, const std::string& key, const std::string& def) {
      auto it = attrs.find(key);
      return it != attrs.end() ? it->second : def;
    };
    auto get_target = [&doc](const std::vector<std::size_t>& current_path) -> std::vector<Node>* {
      auto* current = &doc.root_nodes;
      for (auto const idx : current_path) {
        if (idx >= current->size()) {
          return nullptr;
        }
        current = &(*current)[idx].children;
      }
      return current;
    };

    while (auto tag = parse_next_tag(xml, pos)) {
      if (tag->name == "image" && tag->kind != xml_tag::End) {
        auto const w_str = get_attr(tag->attrs, "w", "0");
        auto const h_str = get_attr(tag->attrs, "h", "0");
        if (w_str.find_first_not_of("0123456789") != std::string::npos) throw std::invalid_argument("invalid width");
        doc.width = std::stoul(w_str);
        doc.height = std::stoul(h_str);
      } else if (tag->name == "stack") {
        if (tag->kind == xml_tag::End) {
          if (!path.empty()) {
            path.pop_back();
          }
        } else {
          auto const name = get_attr(tag->attrs, "name", "");
          auto const x = std::stoi(get_attr(tag->attrs, "x", "0"));
          auto const y = std::stoi(get_attr(tag->attrs, "y", "0"));
          auto const vis = get_attr(tag->attrs, "visibility", "visible") == "visible";
          auto const opacity = std::stof(get_attr(tag->attrs, "opacity", "1.0"));
          auto const blend_str = get_attr(tag->attrs, "composite-op", "svg:src-over");
          auto blend = from_string(blend_str);
          if (!blend) return detail::make_unexpected(Error::Code::XmlParseFailed, blend_str, "invalid blend mode");

          auto* target = get_target(path);
          if (target == nullptr) return detail::make_unexpected(Error::Code::InvalidOraDocument, "stack.xml", "invalid stack nesting");

          target->push_back(Node{Node::Type::Stack, name, x, y, vis, opacity, *blend, {}});
          if (tag->kind == xml_tag::Start) {
            path.push_back(target->size() - 1);
          }
        }
      } else if (tag->name == "layer" && tag->kind != xml_tag::End) {
        auto const name = get_attr(tag->attrs, "name", "");
        if (not layer_names.insert(name).second) return detail::make_unexpected(Error::Code::InvalidOraDocument, name, "duplicate layer name");

        auto const x = std::stoi(get_attr(tag->attrs, "x", "0"));
        auto const y = std::stoi(get_attr(tag->attrs, "y", "0"));
        auto const vis = get_attr(tag->attrs, "visibility", "visible") == "visible";
        auto const opacity = std::stof(get_attr(tag->attrs, "opacity", "1.0"));
        auto const blend_str = get_attr(tag->attrs, "composite-op", "svg:src-over");
        auto blend = from_string(blend_str);
        if (!blend) return detail::make_unexpected(Error::Code::XmlParseFailed, blend_str, "invalid blend mode");

        auto* target = get_target(path);
        if (target == nullptr) return detail::make_unexpected(Error::Code::InvalidOraDocument, "stack.xml", "invalid layer nesting");

        target->push_back(Node{Node::Type::Layer, name, x, y, vis, opacity, *blend, {}});
      }
    }
    return doc;
  } catch (const std::exception& e) {
    return detail::make_unexpected(Error::Code::XmlParseFailed, "stack.xml", e.what());
  }
}

/**
 * @brief レイヤー画像をキャンバスへ直接合成します。
 * @param canvas 合成先キャンバスです。
 * @param cw キャンバス幅です。
 * @param ch キャンバス高さです。
 * @param layer 合成するレイヤー画像です。
 * @param lx 合成位置の X オフセットです。
 * @param ly 合成位置の Y オフセットです。
 * @param opacity レイヤー不透明度です。
 * @param mode 合成モードです。
 */
auto blend_layer(std::vector<uint8_t>& canvas, unsigned int cw, unsigned int ch, const ImageBuffer& layer, int lx, int ly, float opacity, BlendMode mode) -> void {
  for (unsigned int y = 0; y < layer.height(); ++y) {
    int cy = static_cast<int>(y) + ly; if (cy < 0 || cy >= static_cast<int>(ch)) continue;
    for (unsigned int x = 0; x < layer.width(); ++x) {
      int cx = static_cast<int>(x) + lx; if (cx < 0 || cx >= static_cast<int>(cw)) continue;
      auto s = read_pixel(layer.rgba(), (y * layer.width() + x) * 4); s.alpha *= opacity; if (s.alpha <= 0.0f) continue;
      auto b = read_pixel(canvas, (cy * cw + cx) * 4); write_pixel(canvas, (cy * cw + cx) * 4, compose_pixel(mode, b, s));
    }
  }
}

/**
 * @brief ノード列を再帰的に走査して 1 枚のキャンバスへ合成します。
 * @param canvas 合成先キャンバスです。
 * @param cw キャンバス幅です。
 * @param ch キャンバス高さです。
 * @param nodes 合成対象ノード列です。
 * @param layer_images レイヤー画像の辞書です。
 * @param parent_x 親スタックの X オフセットです。
 * @param parent_y 親スタックの Y オフセットです。
 * @return 合成に成功した場合は空の `expected`、失敗時はエラーを返します。
 */
auto process_blend(std::vector<uint8_t>& canvas, unsigned int cw, unsigned int ch,
                   std::span<const Node> nodes, const std::map<std::string, ImageBuffer>& layer_images,
                   int parent_x, int parent_y) -> std::expected<void, Error> {
  for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
    auto const& node = *it; if (!node.visible) continue;
    auto const current_x = parent_x + node.x; auto const current_y = parent_y + node.y;
    if (node.type == Node::Type::Layer) {
      if (auto found = layer_images.find(node.name); found != layer_images.end()) blend_layer(canvas, cw, ch, found->second, current_x, current_y, node.opacity, node.blend_mode);
      else return make_unexpected(Error::Code::InvalidOraDocument, node.name, "layer image not found");
    } else {
      std::vector<uint8_t> group_canvas(cw * ch * 4, 0);
      if (auto res = process_blend(group_canvas, cw, ch, node.children, layer_images, current_x, current_y); !res) return res;
      auto group_image = ImageBuffer::create(cw, ch, std::move(group_canvas));
      blend_layer(canvas, cw, ch, *group_image, 0, 0, node.opacity, node.blend_mode);
    }
  }
  return {};
}

/**
 * @brief 画像を単純平均で縮小・拡大します。
 * @param src 入力 RGBA バッファです。
 * @param sw 入力幅です。
 * @param sh 入力高さです。
 * @param dw 出力幅です。
 * @param dh 出力高さです。
 * @return リサイズ後の RGBA バッファです。
 */
auto resize_image(const std::vector<uint8_t>& src, unsigned int sw, unsigned int sh, unsigned int dw, unsigned int dh) -> std::vector<uint8_t> {
  std::vector<uint8_t> dst(dw * dh * 4);
  for (unsigned int y = 0; y < dh; ++y) {
    for (unsigned int x = 0; x < dw; ++x) {
      unsigned int r=0, g=0, b=0, a=0, c=0;
      unsigned int y0 = y * sh / dh, y1 = std::max(y0+1, (y+1)*sh/dh);
      unsigned int x0 = x * sw / dw, x1 = std::max(x0+1, (x+1)*sw/dw);
      for (unsigned int sy=y0; sy<y1; ++sy) for (unsigned int sx=x0; sx<x1; ++sx) {
        auto i = (sy*sw+sx)*4; r+=src[i]; g+=src[i+1]; b+=src[i+2]; a+=src[i+3]; c++;
      }
      auto i = (y*dw+x)*4; dst[i]=r/c; dst[i+1]=g/c; dst[i+2]=b/c; dst[i+3]=a/c;
    }
  }
  return dst;
}

} // namespace detail

/**
 * @brief 入力 RGBA バイト列から `ImageBuffer` を生成します。
 * @param w 画像幅です。
 * @param h 画像高さです。
 * @param rgba RGBA バイト列です。
 * @return 妥当な画像バッファ、失敗時はエラーを返します。
 */
auto ImageBuffer::create(unsigned int w, unsigned int h, std::vector<uint8_t> rgba) -> std::expected<ImageBuffer, Error> {
  if (w==0 || h==0 || rgba.size() != static_cast<size_t>(w)*h*4) return detail::make_unexpected(Error::Code::InvalidImageBuffer, "invalid dimensions or size");
  return ImageBuffer(w, h, std::move(rgba));
}

/**
 * @brief 指定サイズの空画像を生成します。
 * @param w 画像幅です。
 * @param h 画像高さです。
 * @param a 初期アルファ値です。
 * @return 生成した空画像、失敗時はエラーを返します。
 */
namespace util {

auto blank_image(unsigned int w, unsigned int h, uint8_t a) -> std::expected<ImageBuffer, Error> {
  std::vector<uint8_t> rgba(static_cast<size_t>(w)*h*4, 0);
  if (a!=0) for (size_t i=3; i<rgba.size(); i+=4) rgba[i]=a;
  return ImageBuffer::create(w, h, std::move(rgba));
}

} // namespace util

/**
 * @brief `BlendMode` を OpenRaster の属性文字列へ変換します。
 * @param m 変換対象モードです。
 * @return `composite-op` 属性に使う文字列です。
 */
auto to_string(BlendMode m) -> std::string_view {
  static const std::map<BlendMode, std::string_view> map = {
    {BlendMode::SrcOver, "svg:src-over"}, {BlendMode::Multiply, "svg:multiply"}, {BlendMode::Screen, "svg:screen"}, {BlendMode::Overlay, "svg:overlay"},
    {BlendMode::Darken, "svg:darken"}, {BlendMode::Lighten, "svg:lighten"}, {BlendMode::ColorDodge, "svg:color-dodge"}, {BlendMode::ColorBurn, "svg:color-burn"},
    {BlendMode::HardLight, "svg:hard-light"}, {BlendMode::SoftLight, "svg:soft-light"}, {BlendMode::Difference, "svg:difference"}, {BlendMode::Exclusion, "svg:exclusion"},
    {BlendMode::Hue, "svg:hue"}, {BlendMode::Saturation, "svg:saturation"}, {BlendMode::Color, "svg:color"}, {BlendMode::Luminosity, "svg:luminosity"},
    {BlendMode::Plus, "svg:plus"}, {BlendMode::DstIn, "svg:dst-in"}, {BlendMode::DstOut, "svg:dst-out"}, {BlendMode::SrcAtop, "svg:src-atop"}, {BlendMode::DstAtop, "svg:dst-atop"}
  };
  return map.at(m);
}

/**
 * @brief OpenRaster の属性文字列から `BlendMode` を復元します。
 * @param sv 属性文字列です。
 * @return 対応モード、未対応なら `std::nullopt` を返します。
 */
auto from_string(std::string_view sv) -> std::optional<BlendMode> {
  for (int i=0; i<=static_cast<int>(BlendMode::DstAtop); ++i) {
    auto m = static_cast<BlendMode>(i); if (::ora::to_string(m) == sv) return m;
  }
  return std::nullopt;
}

/**
 * @brief 既定 Provider で ORA ドキュメントを読み込みます。
 * @param filename 読み込むファイル名またはパスです。
 * @return 読み込んだドキュメント、失敗時はエラーを返します。
 */

} // namespace ora
