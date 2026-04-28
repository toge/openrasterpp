# openrasterpp

`openrasterpp` は、OpenRaster（`.ora`）ドキュメントを読み書きするための C++26 ライブラリです。
レイヤー構造、ブレンドモード、PNG 画像データ、ZIP ベースの OpenRaster アーカイブを扱うための API を提供します。

## 概要

このライブラリは、OpenRaster ドキュメントを次のような構成要素で表現します。

- `ora::OraDocument`: ドキュメント全体
- `ora::Node`: レイヤーまたはスタック（グループ）
- `ora::ImageBuffer`: RGBA 画像データ
- `ora::read` / `ora::write`: `.ora` ファイルの読み書き

既定のプロバイダを使う簡易 API に加えて、アーカイブ処理・PNG エンコード/デコード・`stack.xml` の処理を差し替えられる provider 抽象化も含まれています。

## API構成

このライブラリの公開 API は、主に次の 2 層に分かれています。

- `namespace ora`
  - ドメインモデルと入出力の主 API です。
  - `ora::OraDocument`、`ora::Node`、`ora::ImageBuffer`
  - `ora::read()` / `ora::write()`
  - `ora::layer()` / `ora::stack()`
- `namespace ora::util`
  - `ora` の主 API を補助する helper 群です。
  - `ora::util::blank_image()`
  - `ora::util::encode_png()`
  - `ora::util::render_preview_and_thumbnail()`

通常は `ora` 直下の型と入出力 API を中心に使い、画像生成や PNG 化、preview / thumbnail の補完が必要な場面で `ora::util` を併用します。

## 主な機能

- OpenRaster（`.ora`）ファイルの読み込み
- OpenRaster（`.ora`）ファイルの書き込み
- レイヤー / スタック構造の表現
- OpenRaster のブレンドモード文字列と列挙値の相互変換
- `find_package(openrasterpp CONFIG REQUIRED)` で利用できる CMake package export

## 必要要件

- CMake 3.25 以上
- C++26 に対応したコンパイラ
- `std::expected` を含む標準ライブラリ実装

## 依存関係

- `minizip-ng`
- `lodepng`
- `Catch2`（テスト時のみ）

## ビルド方法

このリポジトリでは、依存ライブラリを CMake から見つけられる状態にしてからビルドします。
ローカル検証例では、`vcpkg_installed/x64-linux` を `CMAKE_PREFIX_PATH` に渡す方法を使っています。

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PWD/vcpkg_installed/x64-linux"

cmake --build build --parallel 4
```

補助スクリプトもあります。

```bash
sh ./build.sh
```

ただし `build.sh` は環境に応じて Conan / vcpkg の経路を使うため、依存解決方法が環境に合っていることを前提にしてください。

## テスト方法

まず通常の CTest を実行できます。

```bash
ctest --test-dir build -V
```

インストール済み package を使った smoke test まで含める場合は、次の補助スクリプトを使えます。

```bash
sh ./test.sh
```

`test.sh` は `cmake --install` 後に `test/install_smoke` をビルドし、`find_package(openrasterpp CONFIG REQUIRED)` による下流利用を確認します。
このスクリプトは既定ではリポジトリ直下の `vcpkg_installed/x64-linux` を依存プレフィックスとして参照します。

## インストールと利用方法

ライブラリをインストールする例:

```bash
cmake --install build --prefix "$PWD/build/install-prefix"
```

インストール後は `openrasterppConfig.cmake` と export された target が配置されるので、下流プロジェクトから次のように使えます。

```cmake
find_package(openrasterpp CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE openrasterpp::openrasterpp)
```

`openrasterpp` 自体だけでなく、その依存ライブラリも CMake から見つけられる必要があります。
そのため、下流プロジェクトでは `CMAKE_PREFIX_PATH` に `openrasterpp` の install prefix と依存ライブラリの prefix を含めてください。

```bash
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="/path/to/openrasterpp;/path/to/dependencies"
```

このリポジトリのローカル検証例では、次のような形になります。

```bash
cmake -S test/install_smoke -B build/install-smoke \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PWD/build/install-prefix;$PWD/vcpkg_installed/x64-linux"
```

## 使用例

以下は、単純な `.ora` ドキュメントを生成して書き出し、再度読み込む最小例です。

```cpp
#include <iostream>
#include <utility>

#include <openraster.hpp>

auto main() -> int {
  auto background = ora::util::blank_image(256, 256, 255);
  if (!background) {
    std::cerr << background.error().message << '\n';
    return 1;
  }

  auto background_png = ora::util::encode_png(*background);
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

  if (auto result = ora::util::render_preview_and_thumbnail(doc); !result) {
    std::cerr << result.error().message << '\n';
    return 1;
  }

  if (auto result = ora::write("example.ora", doc); !result) {
    std::cerr << result.error().message << '\n';
    return 1;
  }

  auto loaded = ora::read("example.ora");
  if (!loaded) {
    std::cerr << loaded.error().message << '\n';
    return 1;
  }

  std::cout << "Loaded document: "
            << loaded->width << "x" << loaded->height << '\n';
  return 0;
}
```

この例では、ドキュメントの読み書きは `ora`、補助的な画像生成と PNG 変換は `ora::util` を使っています。

`OraDocument::layer_images` には各レイヤーの PNG バイト列を設定します。
空の `ImageBuffer` が必要な場合は `ora::util::blank_image()` を使えます。
RGBA 画像からレイヤーを作る場合は `ora::util::encode_png()` を使ってください。

`ora::write()` は `mergedimage.png` と `Thumbnails/thumbnail.png` を
内部生成しません。必要なら `ora::util::render_preview_and_thumbnail()` で
これらの PNG を `OraDocument` に設定してから `write()` を呼び出します。
`ora::read()` で読み込んだ `OraDocument` をそのまま再書き出しする場合も、
必要なら preview / thumbnail を補ってから `write()` を呼び出します。
