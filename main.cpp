#include <iostream>
#include <string>
#include "CachePolicy.h"
#include "LRUCache.h"

static int passed = 0, failed = 0;
#define CHECK(cond, msg) do { if (cond) { ++passed; std::cout << "  [PASS] " << msg << std::endl; } else { ++failed; std::cout << "  [FAIL] " << msg << std::endl; } } while(0)

void testLRUBasic() {
    std::cout << "\n========== LRU 基础 ==========" << std::endl;
    Mycache::LRUCache<int, std::string> cache(2);
    std::string v;

    cache.put(1, "a");
    cache.put(2, "b");
    CHECK(cache.get(1, v) && v == "a", "get(1)=a");
    cache.put(3, "c");  // 淘汰 2
    CHECK(!cache.get(2, v), "get(2) 未命中");
    CHECK(cache.get(1, v) && v == "a", "get(1)=a");
    CHECK(cache.get(3, v) && v == "c", "get(3)=c");
}

void testLRUUpdate() {
    std::cout << "\n========== LRU 更新已存在 key ==========" << std::endl;
    Mycache::LRUCache<int, std::string> cache(2);
    std::string v;

    cache.put(1, "a");
    cache.put(1, "a2");
    CHECK(cache.get(1, v) && v == "a2", "put 同一 key 后 get 得到新值");
    cache.put(2, "b");
    cache.put(1, "a3");  // 只更新，不淘汰
    CHECK(cache.get(1, v) && v == "a3", "再次更新 get(1)=a3");
    CHECK(cache.get(2, v) && v == "b", "get(2)=b 仍存在");
}

void testLRUCapacity1() {
    std::cout << "\n========== LRU 容量 1 ==========" << std::endl;
    Mycache::LRUCache<int, std::string> cache(1);
    std::string v;

    cache.put(1, "a");
    CHECK(cache.get(1, v) && v == "a", "get(1)=a");
    cache.put(2, "b");  // 淘汰 1
    CHECK(!cache.get(1, v), "get(1) 未命中");
    CHECK(cache.get(2, v) && v == "b", "get(2)=b");
}

void testLRUKPromotion() {
    std::cout << "\n========== LRU-K 晋升 (k=2) ==========" << std::endl;
    Mycache::LRUKCache<int, std::string> cache(3, 20, 2);
    std::string v;

    cache.put(1, "a");  // 仅历史 count=1
    CHECK(cache.get(1, v) && v == "a", "put 算一次访问，第一次 get 即满 k 晋升并命中");
    cache.get(1, v);    // 第 2 次访问，晋升
    CHECK(v == "a", "第 2 次 get(1) 命中并晋升 value=a");

    cache.put(2, "b");
    cache.get(2, v);
    CHECK(v == "b", "put+get 两次后 get(2)=b");
}

void testLRUKEviction() {
    std::cout << "\n========== LRU-K 主缓存淘汰 ==========" << std::endl;
    Mycache::LRUKCache<int, std::string> cache(2, 20, 2);
    std::string v;

    cache.put(1, "a");
    cache.get(1, v);  // 1 进主缓存
    cache.put(2, "b");
    cache.get(2, v);  // 2 进主缓存，主缓存 [1,2]，2 最近
    cache.put(3, "c");
    cache.get(3, v);  // 3 晋升，淘汰 1，主缓存 [2,3]
    CHECK(!cache.get(1, v), "get(1) 未命中（已被淘汰）");
    CHECK(cache.get(2, v) && v == "b", "get(2)=b");
    CHECK(cache.get(3, v) && v == "c", "get(3)=c");
}

void testLRUKUpdateInMain() {
    std::cout << "\n========== LRU-K 主缓存内更新 ==========" << std::endl;
    Mycache::LRUKCache<int, std::string> cache(2, 20, 2);
    std::string v;

    cache.put(1, "a");
    cache.get(1, v);
    cache.put(1, "a_new");  // 已在主缓存，直接更新
    CHECK(cache.get(1, v) && v == "a_new", "get(1)=a_new");
}

void testLRUKk1() {
    std::cout << "\n========== LRU-K k=1（一次即晋升）==========" << std::endl;
    Mycache::LRUKCache<int, std::string> cache(2, 10, 1);
    std::string v;

    cache.put(1, "a");  // k=1，put 即算 1 次，晋升
    CHECK(cache.get(1, v) && v == "a", "k=1 时 put 后 get(1)=a");
    cache.put(2, "b");
    CHECK(cache.get(2, v) && v == "b", "get(2)=b");
}

void testLRUKMultipleAccessBeforePromotion() {
    std::cout << "\n========== LRU-K 多次访问再晋升 (k=3) ==========" << std::endl;
    Mycache::LRUKCache<int, std::string> cache(2, 20, 3);
    std::string v;

    cache.put(1, "x");
    cache.get(1, v);  // count=2
    CHECK(cache.get(1, v) && v == "x", "第 2 次 get(1) 时 count=3 晋升并命中");
    cache.get(1, v);  // count=3，晋升
    CHECK(v == "x", "第 3 次 get(1) 晋升后 value=x");
}

void testPolymorphism() {
    std::cout << "\n========== 多态（基类指针）==========" << std::endl;
    Mycache::MycachePolicy<int, std::string>* p = new Mycache::LRUKCache<int, std::string>(2, 10, 2);
    std::string v;

    p->put(10, "ten");
    p->put(10, "ten2");
    CHECK(p->get(10, v) && v == "ten2", "基类指针 get(10)=ten2");
    delete p;
}

int main() {
    std::cout << "========== Mycache 测试 ==========" << std::endl;

    testLRUBasic();
    testLRUUpdate();
    testLRUCapacity1();
    testLRUKPromotion();
    testLRUKEviction();
    testLRUKUpdateInMain();
    testLRUKk1();
    testLRUKMultipleAccessBeforePromotion();
    testPolymorphism();

    std::cout << "\n========== 结果 ==========" << std::endl;
    std::cout << "通过: " << passed << ", 失败: " << failed << std::endl;
    return failed > 0 ? 1 : 0;
}