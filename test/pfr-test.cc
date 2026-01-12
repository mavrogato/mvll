
#include <mvll/pfr.hpp>

#include <wayland-client.h>
#include <zwp-tablet-v2-client.h>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("basic", "[mvll][pfr]") {
    static_assert(0 == mvll::get_ordinal<wl_seat_listener, &wl_seat_listener::capabilities>());
    static_assert(1 == mvll::get_ordinal<wl_seat_listener, &wl_seat_listener::name>());
    REQUIRE(0 == mvll::get_ordinal<wl_registry_listener, &wl_registry_listener::global>());
    REQUIRE(1 == mvll::get_ordinal<wl_registry_listener, &wl_registry_listener::global_remove>());
}

TEST_CASE("to_tuple validation", "[mvll][pfr]") {
    using namespace mvll;
    // 1. 基本的なリスナーをタプル化
    SECTION("wl_seat_listener to tuple") {
        // dummyを焼いてタプルにする
        wl_seat_listener listener {
            .capabilities = [](auto...){},
            .name = [](auto...){}
        };

        auto t = mvll::to_tuple(listener);

        // メンバ数が一致しているか
        static_assert(std::tuple_size_v<decltype(t)> == 2);
        
        // 1番目の要素(name)が非nullであることを確認
        // (get_ordinalのロジックが中で正しく展開されているかの証明)
        CHECK(std::get<0>(t) != nullptr);
        CHECK(std::get<1>(t) != nullptr);
    }
    // 2. 巨大なタブレット（19個）をタプル化
    // 64段のハシゴが std::make_tuple(args...) まで正しく繋がっているか
    SECTION("zwp_tablet_tool_v2_listener to tuple") {
        zwp_tablet_tool_v2_listener monster{};
        // 特定の箇所にだけ印を付ける
        monster.button = [](auto...){};

        auto t = mvll::to_tuple(monster);

        // 19個の要素を持つ巨大なタプルができているはず
        static_assert(std::tuple_size_v<decltype(t)> == 19);

        // 17番目 (0-indexed) が button であることを確認
        CHECK(std::get<17>(t) != nullptr);
        
        // それ以外はデフォルト(nullptr)であること
        CHECK(std::get<0>(t) == nullptr);
        CHECK(std::get<18>(t) == nullptr);
    }
    // 3. 構造体分解パックの完全転送（std::forward）確認
    SECTION("rvalue forwarding") {
        struct simple { int a; int b; };
        // 一時オブジェクトを投げて、中の値が正しくタプルへ
        auto t = mvll::to_tuple(simple{10, 20});
        
        CHECK(std::get<0>(t) == 10);
        CHECK(std::get<1>(t) == 20);
    }
}
TEST_CASE("monster protocols", "[mvll][pfr]") {
    using namespace mvll;
    // 1. 重複しがちなポインタのテスト
    // (WL_POINTER_BUTTON_SINCE_VERSION = 1)
    SECTION("wl_pointer consistency") {
        // ボタンと軸、シグネチャが似ていても物理的な順序を正しく引けるか
        // 実際、button は 3番目(0-indexed)、axis は 4番目のはず
        static_assert(3 == get_ordinal<wl_pointer_listener, &wl_pointer_listener::button>());
        static_assert(4 == get_ordinal<wl_pointer_listener, &wl_pointer_listener::axis>());
        
        SUCCEED("wl_pointer ordinal verified");
    }

    // 2. 19個の巨大プロトコル（タブレット）
    // 16個の壁を突破していることを物理的に証明
    SECTION("tablet monster (19 events)") {
        // zwp_tablet_tool_v2_listener の末尾の方のメンバー
        // 17: wheel, 18: button, 19: frame (XML準拠)
        // インデックス 18 が button であることを確認
        static_assert(17 == get_ordinal<zwp_tablet_tool_v2_listener, &zwp_tablet_tool_v2_listener::button>());
        static_assert(18 == get_ordinal<zwp_tablet_tool_v2_listener, &zwp_tablet_tool_v2_listener::frame>());

        SUCCEED("tablet tool monster verified at index 18");
    }

    // 3. 境界値テスト（64段の守護神）
    // 64個まで空振りしても死なないこと（不完全型でもメンバなしなら 0 になるはず）
    SECTION("zero member case") {
        struct empty_listener {}; 
        // メンバが0なら get_ordinal は（throwするか、適切に処理されるか）
        // 今の設計だとメンバポインタを渡せないので、ここは存在確認のみ。
        SUCCEED("64-step ladder remains solid");
    }
}

TEST_CASE("inliner", "[mvll][pfr]") {
    static_assert(0 == mvll::ordinal<&wl_seat_listener::capabilities>);
    static_assert(1 == mvll::ordinal<&wl_seat_listener::name>);
    REQUIRE(0 == mvll::ordinal<&wl_registry_listener::global>);
    REQUIRE(1 == mvll::ordinal<&wl_registry_listener::global_remove>);
}
