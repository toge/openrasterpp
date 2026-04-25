#include "catch2/catch_test_macros.hpp"
#include "openraster.hpp"

// DefaultOraProviderはprivateなので、read関数を通じてテストする
// あるいは、テスト用にスタックパース部分を検証する仕組みが必要

TEST_CASE("Issue #2: Nested stack parsing") {
    // 擬似的なstack.xmlデータ
    std::string xml = R"(<?xml version='1.0' encoding='UTF-8'?>
<image version='0.0.5' w='100' h='100'>
  <stack name='group1' x='10' y='20' visibility='visible' opacity='1.0' composite-op='svg:src-over'>
    <layer name='layer1' src='data/layer1.png' x='0' y='0' visibility='visible' opacity='1.0' composite-op='svg:src-over'/>
  </stack>
</image>
)";
    
    // 現在のコードではProviderが隠蔽されているため、テストが難しい。
    // しかし、deserialize_stackがstackを無視しているのはコードリーディングから明らか。
}
