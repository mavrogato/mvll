
#include <mvll/channel.hpp>

#include <string_view>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("凡猫でもわかる双方向対話テスト", "[mvll][channel]") {
    using namespace std::string_view_literals;

    // 1. コルーチン（お客さん）の定義にゃ
    // T = string (外に出す値), U = int (中に入る値)
    auto chat_coro = []() -> mvll::channel<std::string_view, int> {
        // 最初の一杯を注文するにゃ！
        auto order = "蕎麦ちょうだいにゃ！"sv; 
        int* val = co_yield &order;
        
        // 届いた値（わんこそば）をチェックして、次を注文するにゃ
        if (val && *val == 1) {
            auto order_again = "おかわりにゃ！"sv;
            val = co_yield &order_again;
        }
        
        if (val && *val == 2) {
            auto order_stop = "ごちそうさまにゃ！"sv;
            co_yield &order_stop;
        }
    };

    // 2. チャンネル（お店）の準備にゃ
    auto ch = chat_coro();
    // auto h = ch.handle;
    // auto& promise = h.promise();

    // --- ここからが実況中継にゃ ---

    // [Step 1] 最初の一押し（お店の暖簾をくぐるにゃ）
    // h.resume();
    //ch.start();

    // 客: 「蕎麦ちょうだいにゃ！」
    // REQUIRE(*promise.pull_ptr == "蕎麦ちょうだいにゃ！");
    REQUIRE(*ch.pull() == "蕎麦ちょうだいにゃ！");

    // [Step 2] 1杯目を放り込むにゃ
    int soba1 = 1;
    // promise.push = soba1; // 掲示板に場所を書くにゃ
    // h.resume(); // 「はい、1杯目にゃ！」
    ch.push(&soba1);
    
    // 客: 「おかわりにゃ！」
    //REQUIRE(*promise.pull == "おかわりにゃ！");
    REQUIRE(*ch.pull() == "おかわりにゃ！");

    // [Step 3] 2杯目を放り込むにゃ
    int soba2 = 2;
    // promise.push = soba2; // 掲示板を書き換えるにゃ
    // h.resume(); // 「はい、2杯目にゃ！」
    ch.push(&soba2);
    
    // 客: 「ごちそうさまにゃ！」
    // REQUIRE(*promise.pull == "ごちそうさまにゃ！");
    REQUIRE(*ch.pull() == "ごちそうさまにゃ！");

    // [Final] お店を出るにゃ
    // h.resume();
    // REQUIRE(h.done());
    //ch.resume();
    //REQUIRE(ch.done());
    ch.push(nullptr);
    REQUIRE(ch.done());
}

TEST_CASE("awaiterによるコルーチン間のワープテスト", "[mvll][channel]") {
    using namespace std::string_view_literals;

    // 1. 後ろに控えている「次の工程」担当にゃ（Bさん）
    auto second_step = []() -> mvll::channel<std::string_view, int> {
        // Aさんから PUSH されるのを待つにゃ
        auto token = "後任のBにゃ、準備OKにゃ！"sv;
        int* val = co_yield &token;
        
        if (val && *val == 99) {
            auto token = "Bが99を受け取ったにゃ！完了にゃ！"sv;
            co_yield &token;
        }
    };

    auto ch_b = second_step();
    // ch_b.handle.resume(); // Bさんを待機状態にするにゃ
    //ch_b.start();

    // 2. 最初に動く「前任」担当にゃ（Aさん）
    auto first_step = [&]([[maybe_unused]] auto target_handle) -> mvll::channel<std::string_view, int> {
        auto token = "前任のAにゃ、これからBにバトンタッチするにゃ！"sv;
        co_yield &token;

        // ★ ここが awaiter の使いどころにゃ！
        // Bさんのハンドル(target_handle)に、値「99」を詰めてワープするにゃ！
        // co_await mvll::channel<std::string_view, int>::awaiter{ target_handle, 99 };
        int pass_over_value = 99;
        co_await ch_b.pass(&pass_over_value);

        // 対称転送（return target）された場合、Aさんのここにはもう戻ってこないにゃ...
        // （final_suspendまで一気に行くか、そのまま消滅する運命にゃ）
    };

    auto ch_a = first_step(ch_b.handle);
    
    // --- 現場猫のシミュレーション開始にゃ！ ---

    // [Step 1] Aさんを起動
    //ch_a.start();
    REQUIRE(*ch_a.pull() == "前任のAにゃ、これからBにバトンタッチするにゃ！"sv);

    // [Step 2] Aさんに「ワープ許可」を出すにゃ
    // ユーザーが A の push() を叩くと、Aの中で co_await awaiter が発動するにゃ！
    ch_a.push(nullptr); // 値は何でもいいにゃ、トリガーだにゃ

    // [Step 3] 魔法が起きたにゃ！
    // Aを叩いたはずなのに、制御がBにワープして、Bが値を書き換えているにゃ！
    REQUIRE(*ch_b.pull() == "Bが99を受け取ったにゃ！完了にゃ！"sv);

    REQUIRE(ch_a.done() == true);
    REQUIRE(ch_b.handle.done() == false);
    ch_b.push(nullptr);
    // REQUIRE(ch_b.handle.done() == true);
}

#if 0
// 1. 普通の関数として定義（寿命問題を排除）
mvll::channel<int, int> recursive_func(int n, int& res) {
    if (n <= 0) { res = 0x64AD; co_return; }
    
    // 自分自身を関数として呼び出す（Deducing thisを使わない）
    auto next_ch = recursive_func(n - 1, res);
    //next_ch.start();
    next_ch.push(nullptr);
    int dummy = 0;
    co_await next_ch.pass(&dummy);
}

TEST_CASE("channelによる極限ワープ (寿命問題排除テスト)", "[mvll][channel]") {
    int check = 0;
    auto root_ch = recursive_func(2, check); 
    //root_ch.start();
    root_ch.push(nullptr);
    REQUIRE(check == 0x64AD);
}

TEST_CASE("channelによる極限ワープ (真のStackless証明)", "[mvll][channel]") {
    int check = 0;
    const int depth = 1000000; // 今度こそ100万回！
    auto root_ch = recursive_func(depth, check); 
    //root_ch.start(); // 最初のスイッチだけ押すにゃ
    REQUIRE(check == 0x64AD);
    REQUIRE(root_ch.done());
}

TEST_CASE("channelによる極限ワープ (Symmetric Transfer証明)", "[mvll][channel]") {
    using namespace std::string_view_literals;

    // 再帰的に「次の自分」を呼び出し続けるコルーチン
    // self: 自分自身のチャネル
    // n: 残りのワープ回数
    auto recursive_warp = []<class Rec>(this Rec&& self, int n, auto& final_result) -> mvll::channel<int, int> {
        if (n <= 0) {
            final_result = 0x64AD; // 完了の合言葉にゃ
            co_return;
        }

        // 次の階層のチャネルを生成するにゃ
        auto next_ch = std::forward<Rec>(self)(n - 1, final_result);
        //next_ch.start();

        // ★ ここが運命の Symmetric Transfer にゃ！
        // next_ch に制御を移し、自分はスタックを明け渡して眠るにゃ。
        // これが stackless なら、100万回やってもスタックは1段分しか使わないにゃ。
        int dummy = 0;
        co_await next_ch.pass(&dummy);
    };

    // --- 実行開始にゃ ---

    int final_check = 0;
    const int depth = 1'000'000; // 100万階層！スタックなら確実に昇天するにゃ。

    // 最初のチャネルを起動するにゃ
    auto root_ch = recursive_warp(depth, final_check);
    
    // 100万階層の「ワープの連鎖」のトリガーを引くにゃ！
    //root_ch.start();
    root_ch.push(nullptr);

    // 無事に100万人の手を渡って、最後の人が合言葉を書き込んだかチェックにゃ
    REQUIRE(final_check == 0x64AD);
    
    // 全てのチャネルが done になっているはずだにゃ（final_suspendの連鎖帰還）
    REQUIRE(root_ch.done());
}
#endif
