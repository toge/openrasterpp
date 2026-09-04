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
- `openrasterpp::openrasterpp-png-libspng`
  - libspng を使う PNG backend
  - `ora::libspng::*` façade API を提供
- `openrasterpp::openrasterpp-png-libpng`
  - libpng を使う PNG backend
  - `ora::libpng::*` façade API を提供
- `openrasterpp::openrasterpp-png-stb`
  - stb を使う PNG backend（内部で `stb_image` + `stb_image_write` を利用）
  - `ora::stb::*` façade API を提供

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

### libspng backend API（backend 明示）

- `#include <openraster_libspng.hpp>`
- `ora::libspng::read(...)`
- `ora::libspng::write(...)`
- `ora::libspng::encode_png(...)`
- `ora::libspng::render_preview_and_thumbnail(...)`

### libpng backend API（backend 明示）

- `#include <openraster_libpng.hpp>`
- `ora::libpng::read(...)`
- `ora::libpng::write(...)`
- `ora::libpng::encode_png(...)`
- `ora::libpng::render_preview_and_thumbnail(...)`

### stb backend API（backend 明示）

- `#include <openraster_stb.hpp>`
- `ora::stb::read(...)`
- `ora::stb::write(...)`
- `ora::stb::encode_png(...)`
- `ora::stb::render_preview_and_thumbnail(...)`

## 必要要件

- CMake 3.25+
- C++26 compiler
- `std::expected` を含む標準ライブラリ実装

## 依存関係

- core: `minizip-ng`, `fast-float`（数値パース用、ヘッダオンリー）
- backend (`png-lodepng`): `lodepng`
- backend (`png-libspng`): `libspng`
- backend (`png-libpng`): `libpng`
- backend (`png-stb`): `stb`（内部で `stb_image` + `stb_image_write`）
- test: `Catch2`

## ビルド

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PWD/vcpkg_installed/x64-linux"

cmake --build build --parallel 4
```

## WASI (wasip1) ビルド

WASI 検出時（`CMAKE_SYSTEM_NAME=WASI`）のみ別経路を使い、既存環境の動作は変えていません。
例外が使えない、setjmp/longjmp が使えない、シグナルハンドラを定義できない、などの制限の回避です。

wasip2がwasi-sdkでサポートされた場合には再検討します。


- ZIP: minizip(-ng) ではなく zlib 直利用の内蔵実装（`src/core/zip_wasi.cpp`）
- lodepng: 例外参照のない C API 版（`lodepng-c` + `src/backends/lodepng_c_wrap.c`）
- `deserialize_stack` は全環境で例外なし化（`throw/stoi/stof` 廃止）
- WASI では `-march=native` 無効、`-fno-exceptions`、libpng/libspng/fpng は既定 OFF

```bash
cmake -S . -B build-wasi \
  -G "Unix Makefiles" \
  --toolchain ~/vm/wasi-sdk/share/cmake/wasi-sdk-p1.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=~/vm/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_MANIFEST_MODE=OFF \
  -DVCPKG_TARGET_TRIPLET=wasm32-wasip1 \
  -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=~/vm/wasi-sdk/share/cmake/wasi-sdk-p1.cmake \
  -DCMAKE_PREFIX_PATH=~/vm/vcpkg/installed/wasm32-wasip1 \
  -DBUILD_TEST=OFF

cmake --build build-wasi --parallel 4
```

実行例（wasmedge）:

```bash
wasmedge --dir .:. your_app.wasm
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

### 3) libspng backend を使う

```cmake
find_package(openrasterpp CONFIG REQUIRED COMPONENTS png-libspng)

target_link_libraries(my_app PRIVATE openrasterpp::openrasterpp-png-libspng)
```

```cpp
#include <openraster_libspng.hpp>

auto result = ora::libspng::read("example.ora");
```

### 4) libpng backend を使う

```cmake
find_package(openrasterpp CONFIG REQUIRED COMPONENTS png-libpng)

target_link_libraries(my_app PRIVATE openrasterpp::openrasterpp-png-libpng)
```

```cpp
#include <openraster_libpng.hpp>

auto result = ora::libpng::read("example.ora");
```

### 5) stb backend を使う

```cmake
find_package(openrasterpp CONFIG REQUIRED COMPONENTS png-stb)

target_link_libraries(my_app PRIVATE openrasterpp::openrasterpp-png-stb)
```

```cpp
#include <openraster_stb.hpp>

auto result = ora::stb::read("example.ora");
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


```cmake
find_package(openrasterpp CONFIG REQUIRED COMPONENTS png-lodepng)
target_link_libraries(my_app PRIVATE openrasterpp::openrasterpp-png-lodepng)
```

### API

```cpp
#include <openraster_lodepng.hpp>
auto doc = ora::lodepng::read("a.ora");
```

## backend 追加方法

1. `src/backends/<name>_backend.cpp` を追加して PNG codec を実装
2. `OraProvider` 契約を満たす provider を作り、`ora::read/write` テンプレートへ接続
3. `openraster_<name>.hpp` façade を追加（公開面に 3rd-party ヘッダを露出しない）
4. CMake に `openrasterpp-png-<name>` target と component を追加
5. install smoke に新 backend の consumer 経路を追加
