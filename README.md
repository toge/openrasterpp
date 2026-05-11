# openrasterpp

`openrasterpp` は OpenRaster（`.ora`）ドキュメントを扱う C++26 ライブラリです。  
現在は **core + PNG backend target 分離** 構成です。

## ターゲット構成

- `openrasterpp::openrasterpp-core`
  - ORA モデル
  - `stack.xml` の parse/serialize
  - ZIP/package 処理
  - provider/concept ベースの read/write フロー
  - PNG 実装ライブラリへの直接依存なし
- `openrasterpp::openrasterpp-png-lodepng`
  - lodepng を使う PNG backend
  - `ora::lodepng::*` façade API を提供

## 公開 API

### Core API（backend 非依存）

- `ora::OraDocument`
- `ora::Node`
- `ora::ImageBuffer`
- `ora::read(provider, ...)`
- `ora::write(provider, ...)`
- `ora::util::encode_png(provider, ...)`
- `ora::util::render_preview_and_thumbnail(provider, ...)`

### lodepng backend API（backend 明示）

- `#include <openraster_lodepng.hpp>`
- `ora::lodepng::read(...)`
- `ora::lodepng::write(...)`
- `ora::lodepng::encode_png(...)`
- `ora::lodepng::render_preview_and_thumbnail(...)`

## 必要要件

- CMake 3.25+
- C++26 compiler
- `std::expected` を含む標準ライブラリ実装

## 依存関係

- core: `minizip-ng`
- backend (`png-lodepng`): `lodepng`
- test: `Catch2`

## ビルド

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PWD/vcpkg_installed/x64-linux"

cmake --build build --parallel 4
```

## テスト

```bash
ctest --test-dir build -V
sh ./test.sh
```

`test.sh` は install 後の core/backend smoke（configure/build）まで実行します。

## インストール

```bash
cmake --install build --prefix "$PWD/build/install-prefix"
```

## 利用方法

### 1) core のみ使う

```cmake
find_package(openrasterpp CONFIG REQUIRED COMPONENTS core)

target_link_libraries(my_app PRIVATE openrasterpp::openrasterpp-core)
```

### 2) lodepng backend を使う

```cmake
find_package(openrasterpp CONFIG REQUIRED COMPONENTS png-lodepng)

target_link_libraries(my_app PRIVATE openrasterpp::openrasterpp-png-lodepng)
```

```cpp
#include <openraster_lodepng.hpp>

auto result = ora::lodepng::read("example.ora");
```

## 最小例（backend 明示）

```cpp
#include <iostream>

#include <openraster_lodepng.hpp>

auto main() -> int {
  auto background = ora::util::blank_image(256, 256, 255);
  if (!background) {
    std::cerr << background.error().message << '\n';
    return 1;
  }

  auto background_png = ora::lodepng::encode_png(*background);
  if (!background_png) {
    std::cerr << background_png.error().message << '\n';
    return 1;
  }

  auto doc = ora::OraDocument{
    .width = 256,
    .height = 256,
    .root_nodes = {ora::layer("background")},
    .layer_images = {{"background", std::move(*background_png)}},
    .merged_image_png = std::nullopt,
    .thumbnail_png = std::nullopt
  };

  if (auto result = ora::lodepng::render_preview_and_thumbnail(doc); !result) {
    std::cerr << result.error().message << '\n';
    return 1;
  }

  if (auto result = ora::lodepng::write("example.ora", doc); !result) {
    std::cerr << result.error().message << '\n';
    return 1;
  }

  auto loaded = ora::lodepng::read("example.ora");
  if (!loaded) {
    std::cerr << loaded.error().message << '\n';
    return 1;
  }

  std::cout << "Loaded document: " << loaded->width << "x" << loaded->height << '\n';
  return 0;
}
```

## 移行ガイド

### CMake

旧:

```cmake
find_package(openrasterpp CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE openrasterpp::openrasterpp)
```

新:

```cmake
find_package(openrasterpp CONFIG REQUIRED COMPONENTS png-lodepng)
target_link_libraries(my_app PRIVATE openrasterpp::openrasterpp-png-lodepng)
```

### API

旧:

```cpp
#include <openraster.hpp>
auto doc = ora::read("a.ora");
```

新:

```cpp
#include <openraster_lodepng.hpp>
auto doc = ora::lodepng::read("a.ora");
```

`ora::read(...)` など backend 非明示 wrapper は廃止されています。  
高度な用途では `ora::read(provider, ...)` の provider API を使ってください。

## backend 追加方法

1. `src/backends/<name>_backend.cpp` を追加して PNG codec を実装
2. `OraProvider` 契約を満たす provider を作り、`ora::read/write` テンプレートへ接続
3. `openraster_<name>.hpp` façade を追加（公開面に 3rd-party ヘッダを露出しない）
4. CMake に `openrasterpp-png-<name>` target と component を追加
5. install smoke に新 backend の consumer 経路を追加
