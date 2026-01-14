
#include <mvll/fiblet.hpp>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>


// テスト用のデータ構造（酸素）
struct test_args {
    int value;
};

TEST_CASE("fiblet circulation test", "[mvll][fiblet]") {
    using namespace mvll;

    int result = 0;
    fiblet_base body_circ; // 体循環（DSL側）

    // 1. 肺循環（イベント待機側）の構築
    auto listener = [&]() -> fiblet<test_args> {
        for (int i = 0; i < 2; ++i) {
            // ガコン！と待機（ここで体循環へジャンプするはず）
            auto const& args = co_await wait_current;
            result += args.value;
        }
    }();

    // 2. 体循環（DSL側）の構築
    auto command = [&]() -> fiblet_base {
        // 肺から送られてきた結果を確認するだけの簡単なお仕事
        co_await wait_current;
        REQUIRE(result == 10);
        co_await wait_current;
        REQUIRE(result == 30);
        co_return;
    }();

    // 3. 循環器の配線（コネクト）
    // 肺が終わったら体へ飛ぶようにセット（previousの活用）
    listener.connect(command);

    SECTION("push pulse") {
        test_args pulse1{10};
        test_args pulse2{20};

        // 第1拍：肺が目覚め、10を加算し、体（command）へガコンと飛ぶ
        listener.push(&pulse1);
        REQUIRE(result == 10);

        // 第2拍：再び肺が目覚め、20を加算し、体（command）へ飛んで終了
        listener.push(&pulse2);
        REQUIRE(result == 30);
        
        // 最終的に肺も体も終わっていることを確認
        CHECK(listener.handle().done());
        CHECK(command.handle().done());
    }
}
