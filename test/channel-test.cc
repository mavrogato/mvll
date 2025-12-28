
#include <mvll/channel.hpp>

#include <string_view>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>


TEST_CASE("凡猫でもわかる双方向対話テスト", "[mvll][channel]") {
    // 1. コルーチン（お客さん）の定義にゃ
    // T = string (外に出す値), U = int (中に入る値)
    auto chat_coro = []() -> mvll::channel<std::string_view, int> {
        // 最初の一杯を注文するにゃ！
        int* val = co_yield "蕎麦ちょうだいにゃ！"; 
        
        // 届いた値（わんこそば）をチェックして、次を注文するにゃ
        if (val && *val == 1) {
            val = co_yield "おかわりにゃ！";
        }
        
        if (val && *val == 2) {
            co_yield "ごちそうさまにゃ！";
        }
    };

    // 2. チャンネル（お店）の準備にゃ
    auto ch = chat_coro();
    auto h = ch.handle;
    auto& promise = h.promise();

    // --- ここからが実況中継にゃ ---

    // [Step 1] 最初の一押し（お店の暖簾をくぐるにゃ）
    h.resume(); 
    // 客: 「蕎麦ちょうだいにゃ！」
    REQUIRE(*promise.pull_ptr == "蕎麦ちょうだいにゃ！");

    // [Step 2] 1杯目を放り込むにゃ
    // int soba1 = 1;
    // promise.push = soba1; // 掲示板に場所を書くにゃ
    // h.resume(); // 「はい、1杯目にゃ！」
    ch.push(1);
    
    // 客: 「おかわりにゃ！」
    //REQUIRE(*promise.pull == "おかわりにゃ！");
    REQUIRE(ch.pull() == "おかわりにゃ！");

    // [Step 3] 2杯目を放り込むにゃ
    // int soba2 = 2;
    // promise.push = soba2; // 掲示板を書き換えるにゃ
    // h.resume(); // 「はい、2杯目にゃ！」
    ch.push(2);
    
    // 客: 「ごちそうさまにゃ！」
    // REQUIRE(*promise.pull == "ごちそうさまにゃ！");
    REQUIRE(ch.pull() == "ごちそうさまにゃ！");

    // [Final] お店を出るにゃ
    // h.resume();
    // REQUIRE(h.done());
    ch.resume();
    REQUIRE(ch.done());
}

TEST_CASE("awaiterによるコルーチン間のワープテスト", "[mvll][channel]") {
    using namespace std::string_view_literals;

    // 1. 後ろに控えている「次の工程」担当にゃ（Bさん）
    auto second_step = []() -> mvll::channel<std::string_view, int> {
        // Aさんから PUSH されるのを待つにゃ
        int* val = co_yield "後任のBにゃ、準備OKにゃ！"sv;
        
        if (val && *val == 99) {
            co_yield "Bが99を受け取ったにゃ！完了にゃ！"sv;
        }
    };

    auto ch_b = second_step();
    ch_b.handle.resume(); // Bさんを待機状態にするにゃ

    // 2. 最初に動く「前任」担当にゃ（Aさん）
    auto first_step = [&]([[maybe_unused]] auto target_handle) -> mvll::channel<std::string_view, int> {
        co_yield "前任のAにゃ、これからBにバトンタッチするにゃ！"sv;

        // ★ ここが awaiter の使いどころにゃ！
        // Bさんのハンドル(target_handle)に、値「99」を詰めてワープするにゃ！
        // co_await mvll::channel<std::string_view, int>::awaiter{ target_handle, 99 };
        co_await ch_b.pass(99);

        // 対称転送（return target）された場合、Aさんのここにはもう戻ってこないにゃ...
        // （final_suspendまで一気に行くか、そのまま消滅する運命にゃ）
    };

    auto ch_a = first_step(ch_b.handle);
    
    // --- 現場猫のシミュレーション開始にゃ！ ---

    // [Step 1] Aさんを起動
    ch_a.resume();
    REQUIRE(ch_a.pull() == "前任のAにゃ、これからBにバトンタッチするにゃ！"sv);

    // [Step 2] Aさんに「ワープ許可」を出すにゃ
    // ユーザーが A の push() を叩くと、Aの中で co_await awaiter が発動するにゃ！
    ch_a.push(0); // 値は何でもいいにゃ、トリガーだにゃ

    // [Step 3] 魔法が起きたにゃ！
    // Aを叩いたはずなのに、制御がBにワープして、Bが値を書き換えているにゃ！
    REQUIRE(ch_b.pull() == "Bが99を受け取ったにゃ！完了にゃ！"sv);
    
    REQUIRE(ch_b.handle.done() == false);
    ch_b.resume();
    REQUIRE(ch_b.handle.done() == true);
}
