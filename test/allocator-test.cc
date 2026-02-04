
#include <mvll/allocator.hpp>
#include <mvll/memory.hpp>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <vector>
#include <iostream>
#include <cassert>

struct trace_record {
    struct entry { std::size_t s, a, n; } *buf{};
    std::size_t cap{1024};
    std::size_t pos{};
    constexpr trace_record(std::size_t cap = 1024) : buf{new entry[cap]}, cap{cap}, pos{0} {}
    constexpr ~trace_record() noexcept {
        delete[] buf;
        cap = pos = 0;
        buf = nullptr;
    }
    constexpr void on_allocate(entry&& rec) {
        if (cap <= pos) {
            cap = (cap == 0 ? 1024 : cap * 2);
            entry* tmp = new entry[cap];
            std::copy_n(buf, pos, tmp);
            delete [] std::exchange(buf, tmp);
        }
        buf[pos++] = std::move(rec);
    }
    template <std::size_t N>
    constexpr std::array<entry, N> to_array() noexcept {
        std::array<entry, N> ret{};
        std::copy_n(buf, std::min(N, pos), ret.begin());
        return ret;
    }
};

template <class T>
struct traced_allocator {
    using value_type = T;
    trace_record* tracer;
    constexpr traced_allocator(trace_record* t) : tracer{t} {}
    template <class U>
    constexpr traced_allocator(traced_allocator<U> const& other) noexcept : tracer{other.tracer} {}
    constexpr T* allocate(std::size_t n) {
        tracer->on_allocate({sizeof (T), alignof (T), n});
        return std::allocator<T>().allocate(n);
    }
    constexpr void deallocate(T* p, std::size_t n) noexcept {
        std::allocator<T>().deallocate(p, n);
    }
};

consteval auto test() {
    trace_record res(0);
    {
        // カスタムアロケータを渡して vector を操作
        std::vector<int, traced_allocator<int>> v(traced_allocator<int>{&res});
        v.push_back(1); // 1回目の確保が発生
        v.push_back(2); // 2回目の確保（再確保）が発生する可能性がある
        v.push_back(3); // 3回目
    }
    return res.to_array<16>();
}
constexpr auto result = test();
static_assert(result[0].s == sizeof (int));
static_assert(result[0].a == alignof (int));
static_assert(result[0].n == 1);
static_assert(result[1].s == sizeof (int));
static_assert(result[1].a == alignof (int));
static_assert(result[1].n == 2);
static_assert(result[2].s == sizeof (int));
static_assert(result[2].a == alignof (int));
static_assert(result[2].n == 4);
static_assert(result[3].s == 0);
static_assert(result[3].a == 0);
static_assert(result[3].n == 0);


#if 0
// 足跡を記録する構造体
struct AllocationRecord {
    std::size_t size;
    std::size_t align;
    std::size_t count;
};

struct Tracer {
    AllocationRecord history[10] = {}; // constexpr内の履歴保持用
    std::size_t count = 0;

    constexpr void on_allocate(std::size_t s, std::size_t a, std::size_t n) {
        if (count < 10) {
            history[count++] = {s, a, n};
        }
    }
};

template <class T>
struct HookAlloc {
    using value_type = T;
    Tracer* tracer;

    constexpr HookAlloc(Tracer* t) noexcept : tracer(t) {}
    template <class U>
    constexpr HookAlloc(const HookAlloc<U>& other) noexcept : tracer(other.tracer) {}

    constexpr T* allocate(std::size_t n) {
        tracer->on_allocate(sizeof(T), alignof(T), n);
        return std::allocator<T>{}.allocate(n);
    }
    constexpr void deallocate(T* p, std::size_t n) {
        std::allocator<T>{}.deallocate(p, n);
    }
    // operator== は省略（今回のケースではデフォルトで十分）
};

// 2. 覗き見テスト
consteval auto peek_vector_behavior() {
    Tracer t;
    {
        // カスタムアロケータを渡して vector を操作
        std::vector<int, HookAlloc<int>> v(HookAlloc<int>{&t});
        v.push_back(1); // 1回目の確保が発生
        v.push_back(2); // 2回目の確保（再確保）が発生する可能性がある
        v.push_back(3); // 3回目
    }
    return t;
}
static_assert(peek_vector_behavior().count == 3);
static_assert(peek_vector_behavior().history[0].size == 4);
static_assert(peek_vector_behavior().history[0].align == alignof(int));


// 3. static_assert で検証
void test() {
    constexpr auto t = peek_vector_behavior();
    // 最初のallocate呼び出しをチェック
    static_assert(t.count > 0);
    static_assert(t.history[0].size == sizeof(int));
    static_assert(t.history[0].count >= 1);
}
#endif

#if 0
template <class T>
class Allocator {
public:
    constexpr T* allocate(std::size_t n) const {
        return std::allocator<T>().allocate(n);
    }
    constexpr void deallocate(T* p, std::size_t n) const noexcept {
        std::allocator<T>().deallocate(p, n);
    }
};

consteval auto cmain() {
    mvll::move_only_erased_box<Allocator> box{std::vector<bool>(42)};
    return *(box.get<std::vector<bool>>());
}
static_assert(cmain().size() == 42);
#endif

TEST_CASE("sandbox", "[mvll][allocator]") {
    std::clog << "Hi" << std::endl;
    std::clog << "Bye" << std::endl;
}
