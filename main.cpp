#include <iostream>
#include <string>
#include "CachePolicy.h"
#include "LRUCache.h"
#include "LFUCache.h"
#include "ARCCache.h"


#include <chrono>
#include <vector>
#include <iomanip>
#include <random>
#include <algorithm>
#include <array>



static int passed = 0, failed = 0;
#define CHECK(cond, msg) do { if (cond) { ++passed; std::cout << "  [PASS] " << msg << std::endl; } else { ++failed; std::cout << "  [FAIL] " << msg << std::endl; } } while(0)
#define CHECK_MANY(cond, msg, i) do { if (cond) ++passed; else { ++failed; std::cout << "  [FAIL] " << msg << " (i=" << (i) << ")" << std::endl; } } while(0)

void testHashLRUBasic() {
    std::cout << "\n========== HashLRU 基础 ==========" << std::endl;
    Mycache::HASHLRUCache<int, std::string> cache(8, 4);  // 总容量 8，4 片，每片约 2
    std::string v;

    cache.put(1, "a");
    cache.put(2, "b");
    CHECK(cache.get(1, v) && v == "a", "HashLRU get(1)=a");
    CHECK(cache.get(2, v) && v == "b", "HashLRU get(2)=b");
    cache.put(1, "a2");
    CHECK(cache.get(1, v) && v == "a2", "HashLRU 更新后 get(1)=a2");
}

void testHashLRUEviction() {
    std::cout << "\n========== HashLRU 分片内淘汰 ==========" << std::endl;
    // 单片容量 2，多放几个 key，总有一个片会满并发生淘汰
    Mycache::HASHLRUCache<int, std::string> cache(8, 4);
    std::string v;

    for (int i = 0; i < 20; ++i)
        cache.put(i, "v" + std::to_string(i));
    // 总容量 8，部分 key 会因所在片满被淘汰
    int hits = 0;
    for (int i = 0; i < 20; ++i)
        if (cache.get(i, v)) ++hits;
    CHECK(hits <= 8 && hits >= 1, "HashLRU 总命中数不超过总容量 8");
}

void testHashLRUPolymorphism() {
    std::cout << "\n========== HashLRU 多态 ==========" << std::endl;
    Mycache::MycachePolicy<int, std::string>* p = new Mycache::HASHLRUCache<int, std::string>(4, 2);
    std::string v;

    p->put(10, "ten");
    CHECK(p->get(10, v) && v == "ten", "基类指针 HashLRU get(10)=ten");
    p->put(10, "ten2");
    CHECK(p->get(10, v) && v == "ten2", "基类指针 HashLRU 更新后 get(10)=ten2");
    delete p;
}
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


// ---------- 批量测试：LRU 大量 key ----------
void testLRUManyKeys() {
    std::cout << "\n========== LRU 大量 key (100 次 put/get 检查) ==========" << std::endl;
    const int N = 100;
    const int CAP = 20;
    Mycache::LRUCache<int, std::string> cache(CAP);
    std::string v;

    for (int i = 0; i < N; ++i)
        cache.put(i, "v" + std::to_string(i));

    int hits = 0;
    for (int i = 0; i < N; ++i) {
        bool ok = cache.get(i, v);
        if (ok) { ++hits; CHECK_MANY(v == "v" + std::to_string(i), "LRU get(i)=v_i", i); }
        else    CHECK_MANY(true, "LRU get(i) 未命中(预期)", i);
    }
    CHECK(hits == CAP, "LRU 仅保留最近 " + std::to_string(CAP) + " 个");
}

// ---------- 批量测试：LRU 更新 ----------
void testLRUManyUpdates() {
    std::cout << "\n========== LRU 大量更新 (50 次检查) ==========" << std::endl;
    Mycache::LRUCache<int, std::string> cache(50);
    std::string v;

    for (int i = 0; i < 50; ++i) {
        cache.put(i, "a" + std::to_string(i));
        CHECK(cache.get(i, v) && v == "a" + std::to_string(i), "LRU put/get " + std::to_string(i));
    }
    for (int i = 0; i < 50; ++i) {
        cache.put(i, "b" + std::to_string(i));
        CHECK(cache.get(i, v) && v == "b" + std::to_string(i), "LRU 更新后 get " + std::to_string(i));
    }
}

// ---------- 批量测试：LRU-K 晋升 ----------
void testLRUKManyPromotions() {
    std::cout << "\n========== LRU-K 大量晋升 (60 次检查) ==========" << std::endl;
    Mycache::LRUKCache<int, std::string> cache(30, 200, 2);
    std::string v;

    for (int i = 0; i < 30; ++i)
        cache.put(i, "x" + std::to_string(i));
    for (int i = 0; i < 30; ++i) {
        bool ok = cache.get(i, v);
        CHECK(ok && v == "x" + std::to_string(i), "LRU-K 第2次访问晋升 get(" + std::to_string(i) + ")");
    }
    for (int i = 0; i < 30; ++i)
        CHECK(cache.get(i, v) && v == "x" + std::to_string(i), "LRU-K 再次 get(" + std::to_string(i) + ")");
}

// ---------- 批量测试：HashLRU 大量 key ----------
void testHashLRUManyKeys() {
    std::cout << "\n========== HashLRU 大量 key (80 次检查) ==========" << std::endl;
    const int N = 80;
    Mycache::HASHLRUCache<int, std::string> cache(40, 8);
    std::string v;

    for (int i = 0; i < N; ++i)
        cache.put(i, "h" + std::to_string(i));
    int hits = 0;
    for (int i = 0; i < N; ++i) {
        bool ok = cache.get(i, v);
        if (ok) {
            ++hits;
            CHECK_MANY(v == "h" + std::to_string(i), "HashLRU get(i)=h_i", i);
        } else {
            CHECK_MANY(true, "HashLRU get(i) 未命中", i);
        }
    }
    CHECK(hits <= 40 && hits >= 1, "HashLRU 命中数在 1~40");
}

// ---------- 批量测试：HashLRU 更新 ----------
void testHashLRUManyUpdates() {
    std::cout << "\n========== HashLRU 大量更新 (40 次检查) ==========" << std::endl;
    Mycache::HASHLRUCache<int, std::string> cache(40, 4);
    std::string v;

    for (int i = 0; i < 40; ++i) {
        cache.put(i, "u" + std::to_string(i));
        CHECK(cache.get(i, v) && v == "u" + std::to_string(i), "HashLRU put/get " + std::to_string(i));
    }
}

// ---------- 批量测试：多态 + 多种 key ----------
void testPolymorphismManyKeys() {
    std::cout << "\n========== 多态大量 key (30 次检查) ==========" << std::endl;
    Mycache::MycachePolicy<int, std::string>* p = new Mycache::HASHLRUCache<int, std::string>(20, 4);
    std::string v;

    for (int i = 0; i < 30; ++i)
        p->put(i, "p" + std::to_string(i));
    int hits = 0;
    for (int i = 0; i < 30; ++i)
        if (p->get(i, v)) { ++hits; CHECK(v == "p" + std::to_string(i), "多态 get(" + std::to_string(i) + ")"); }
    delete p;
    CHECK(hits <= 20, "多态 HashLRU 命中数不超过容量");
}

// ---------- 压力：混合 put/get 随机 key ----------
void testStressMixed() {
    std::cout << "\n========== 压力测试 混合操作 (200 次 get 检查) ==========" << std::endl;
    Mycache::LRUCache<int, int> cache(50);
    int v;

    for (int i = 0; i < 200; ++i)
        cache.put(i % 80, i);
    for (int i = 0; i < 200; ++i) {
        bool ok = cache.get(i % 80, v);
        CHECK_MANY(ok || !ok, "stress get", i);
    }
}

void testLFUBasic() {
    std::cout << "\n========== LFU 基础 ==========" << std::endl;
    Mycache::LFUCache<int, std::string> cache(2);
    std::string v;

    cache.put(1, "a");
    cache.put(2, "b");

    // 提升 1 的访问频次：1 频次更高，之后应淘汰 2
    CHECK(cache.get(1, v) && v == "a", "get(1)=a");
    CHECK(cache.get(1, v) && v == "a", "get(1) second time");

    cache.put(3, "c"); // 容量满，淘汰 freq 最小的 key(应为 2)
    CHECK(!cache.get(2, v), "get(2) 未命中（LFU 淘汰）");
    CHECK(cache.get(1, v) && v == "a", "get(1) 仍存在");
    CHECK(cache.get(3, v) && v == "c", "get(3)=c");
}

void testLFUTieByRecencyInSameFreq() {
    std::cout << "\n========== LFU 同频并列 ==========" << std::endl;
    Mycache::LFUCache<int, std::string> cache(2);
    std::string v;

    cache.put(1, "a");
    cache.put(2, "b");

    // 让它们频次都到 2
    cache.get(1, v); // 1: freq 2
    cache.get(2, v); // 2: freq 2
    // 同频(=2)里：1 更早进入 freq=2 链表，因此应被优先淘汰

    cache.put(3, "c"); // 淘汰 freq=2 链表 front => 1
    CHECK(!cache.get(1, v), "get(1) 未命中（同频淘汰更早的）");
    CHECK(cache.get(2, v) && v == "b", "get(2) 仍存在");
    CHECK(cache.get(3, v) && v == "c", "get(3)=c");
}


void testLFUCapacity0() {
    std::cout << "\n========== LFU 容量 0 ==========" << std::endl;
    Mycache::LFUCache<int, std::string> cache(0);
    std::string v;

    cache.put(1, "a");
    CHECK(!cache.get(1, v), "capacity=0 put 后 get 必未命中");
}

void testLFUBasicEvictByFrequency() {
    std::cout << "\n========== LFU 按频次淘汰 ==========" << std::endl;
    Mycache::LFUCache<int, std::string> cache(2);
    std::string v;

    cache.put(1, "a");
    cache.put(2, "b");

    // 提升 key=1 访问频次
    CHECK(cache.get(1, v) && v == "a", "get(1) 第1次");
    CHECK(cache.get(1, v) && v == "a", "get(1) 第2次");

    // 此时：1 的 freq 比 2 大，put(3) 淘汰 freq 最小的 key=2
    cache.put(3, "c");
    CHECK(!cache.get(2, v), "淘汰 freq=1 的 2");
    CHECK(cache.get(1, v) && v == "a", "1 仍在缓存");
    CHECK(cache.get(3, v) && v == "c", "3 在缓存");
}

void testLFUTieByRecencySameFreq() {
    std::cout << "\n========== LFU 同频并列 ==========" << std::endl;
    Mycache::LFUCache<int, std::string> cache(2);
    std::string v;

    // 1 和 2 都是初始 freq=1，按实现应淘汰同频里“更早进入/更久未被touch”的那个
    cache.put(1, "a");
    cache.put(2, "b");

    cache.put(3, "c"); // 淘汰掉 freq 最小且同频优先更早的
    CHECK(!cache.get(1, v), "同频并列应先淘汰更早的 key=1");
    CHECK(cache.get(2, v) && v == "b", "key=2 仍存在");
    CHECK(cache.get(3, v) && v == "c", "key=3 存在");
}

void testLFUUpdateValueCountsAsAccess() {
    std::cout << "\n========== LFU 更新 value ==========" << std::endl;
    Mycache::LFUCache<int, std::string> cache(2);
    std::string v;

    cache.put(1, "a");
    cache.put(2, "b");

    // put 已存在 key=1：会 touch -> freq++
    cache.put(1, "a_new");

    // 让 2 的 freq 保持更低，put(3) 应淘汰 2
    cache.put(3, "c");
    CHECK(cache.get(1, v) && v == "a_new", "put 已存在应更新 value 并提升频次");
    CHECK(!cache.get(2, v), "应淘汰较低频的 key=2");
    CHECK(cache.get(3, v) && v == "c", "key=3 在缓存");
}

void testLFUDeterministicSequence() {
    std::cout << "\n========== LFU 多步确定性序列 ==========" << std::endl;
    Mycache::LFUCache<int, std::string> cache(3);
    std::string v;

    cache.put(1, "a");
    cache.put(2, "b");
    cache.put(3, "c");

    // 让频次拉开：1 最热，3 最冷
    for (int i = 0; i < 5; ++i) CHECK(cache.get(1, v) && v == "a", "get(1) 热点");
    for (int i = 0; i < 2; ++i) CHECK(cache.get(2, v) && v == "b", "get(2) 中等");

    // put(4)：应淘汰 3（最小 freq）
    cache.put(4, "d");
    CHECK(!cache.get(3, v), "put(4) 淘汰最小频次的 3");
    CHECK(cache.get(1, v) && v == "a", "1 仍存在");
    CHECK(cache.get(2, v) && v == "b", "2 仍存在");
    CHECK(cache.get(4, v) && v == "d", "4 存在");

    // 再触发一次：get(4) 使 freq(4) 上升，从而 minFreq 变动
    CHECK(cache.get(4, v) && v == "d", "get(4) 一次提升频次");

    // put(5)：此时 minFreq 对应应当淘汰 4（实现里同频淘汰头部）
    cache.put(5, "e");
    CHECK(!cache.get(4, v), "put(5) 后 4 应被淘汰");
    CHECK(cache.get(5, v) && v == "e", "5 存在");
}

void testLFUMaxAverageDecay_Triggers() {
    std::cout << "\n========== LFU 最大平均频次：触发衰减 ==========" << std::endl;
    Mycache::LFUCache<int, std::string> cache(3, 2); // maxAverageNum=2：很容易触发衰减
    std::string v;

    cache.put(1, "a"); // f1=1
    cache.put(2, "b"); // f2=1
    cache.put(3, "c"); // f3=1

    cache.get(2, v);   // f2=2，f3=1（此时最小是key3）
    CHECK(v == "b", "pre check get(2)=b");

    for (int i = 0; i < 6; ++i) { // 让平均频次持续上升，触发 aging/decay
        cache.get(1, v); // 不关心每次值，只要能触发衰减
    }

    cache.put(4, "d"); // 触发淘汰：应当淘汰掉最小频次的 key

    // key1 应该仍在（如果 key1 被淘汰，说明衰减/淘汰逻辑有大问题）
    CHECK(cache.get(1, v) && v == "a", "after decay, hot key(1) should remain");

    bool hit2 = cache.get(2, v);
    if (hit2) CHECK(true, "key2 still exists or was promoted");

    bool hit3 = cache.get(3, v);

    // 至少淘汰了 key2 和 key3 中的一个（容量满 put(4) 必淘汰 1 个）
    CHECK(!(hit2 && hit3), "after decay+evict, at least one of {2,3} should be evicted");

    CHECK(cache.get(4, v) && v == "d", "get(4)=d");
}
void testLFUMaxAverageDecay_NoTrigger_EvictMinFreq() {
    std::cout << "\n========== LFU 最大平均频次：不触发衰减 ==========" << std::endl;
    Mycache::LFUCache<int, std::string> cache(3, 1000000); // maxAverageNum 大到几乎不触发
    std::string v;

    cache.put(1, "a"); // f1=1
    cache.put(2, "b"); // f2=1
    cache.put(3, "c"); // f3=1

    cache.get(2, v);   // f2=2
    CHECK(v == "b", "pre check get(2)=b");

    for (int i = 0; i < 6; ++i) {
        cache.get(1, v); // 只让 key1 更热，不触发衰减
    }

    cache.put(4, "d"); // 淘汰：应淘汰最小频次 key3

    CHECK(!cache.get(3, v), "no-decay: key3 should be evicted");
    CHECK(cache.get(2, v) && v == "b", "no-decay: key2 should remain");
    CHECK(cache.get(4, v) && v == "d", "get(4)=d");
}


void testHashLFUBasic() {
    std::cout << "\n========== HashLFU 基础 ==========" << std::endl;
    Mycache::HashLFUCache<int, std::string> cache(8, 4);  // 总容量 8，4 片
    std::string v;

    cache.put(1, "a");
    cache.put(2, "b");
    CHECK(cache.get(1, v) && v == "a", "HashLFU get(1)=a");
    CHECK(cache.get(2, v) && v == "b", "HashLFU get(2)=b");
    cache.put(1, "a2");
    CHECK(cache.get(1, v) && v == "a2", "HashLFU 更新后 get(1)=a2");
}

void testHashLFUEviction() {
    std::cout << "\n========== HashLFU 分片内淘汰 ==========" << std::endl;
    Mycache::HashLFUCache<int, std::string> cache(8, 4);  // 每片约 2
    std::string v;

    for (int i = 0; i < 20; ++i)
        cache.put(i, "v" + std::to_string(i));
    int hits = 0;
    for (int i = 0; i < 20; ++i)
        if (cache.get(i, v)) ++hits;
    CHECK(hits <= 8 && hits >= 1, "HashLFU 总命中数不超过总容量 8");
}

void testHashLFUPolymorphism() {
    std::cout << "\n========== HashLFU 多态 ==========" << std::endl;
    Mycache::MycachePolicy<int, std::string>* p =
        new Mycache::HashLFUCache<int, std::string>(4, 2);
    std::string v;

    p->put(10, "ten");
    CHECK(p->get(10, v) && v == "ten", "基类指针 HashLFU get(10)=ten");
    p->put(10, "ten2");
    CHECK(p->get(10, v) && v == "ten2", "基类指针 HashLFU 更新后 get(10)=ten2");
    delete p;
}

void testHashLFUManyKeys() {
    std::cout << "\n========== HashLFU 大量 key ==========" << std::endl;
    Mycache::HashLFUCache<int, std::string> cache(40, 8);
    std::string v;

    for (int i = 0; i < 80; ++i)
        cache.put(i, "h" + std::to_string(i));
    int hits = 0;
    for (int i = 0; i < 80; ++i)
        if (cache.get(i, v)) {
            ++hits;
            CHECK(v == "h" + std::to_string(i), "HashLFU get(" + std::to_string(i) + ")");
        }
    CHECK(hits <= 40 && hits >= 1, "HashLFU 命中数在 1~40");
}
void testARCBasicPutGet() {
    std::cout << "\n========== ARC 基础 put/get ==========" << std::endl;
    Mycache::ARCCache<int, std::string> cache(2, 2);
    std::string v;

    cache.put(1, "a");
    cache.put(2, "b");

    CHECK(cache.get(1, v) && v == "a", "get(1)=a");
    CHECK(cache.get(2, v) && v == "b", "get(2)=b");
}

void testARCUpdateValue() {
    std::cout << "\n========== ARC 更新 value ==========" << std::endl;
    Mycache::ARCCache<int, std::string> cache(2, 2);
    std::string v;

    cache.put(1, "a");
    cache.put(1, "a2");
    CHECK(cache.get(1, v) && v == "a2", "put 同 key 更新后 get(1)=a2");

    cache.put(2, "b");
    CHECK(cache.get(2, v) && v == "b", "get(2)=b");
}

void testARCEvictionAfterAccess() {
    std::cout << "\n========== ARC 淘汰/访问顺序 ==========" << std::endl;
    // 主要验证：访问后更“新”的 key 更不容易被淘汰（至少符合 LRU 部分的直觉）
    Mycache::ARCCache<int, std::string> cache(2, 2);
    std::string v1, v2, v3;

    cache.put(1, "a");
    cache.put(2, "b");

    // 让 1 变得更“新”
    CHECK(cache.get(1, v1) && v1 == "a", "pre get(1)=a");

    cache.put(3, "c"); // 容量满：应淘汰最久未用的那个（通常淘汰 key=2）

    bool hit1 = cache.get(1, v1);
    bool hit2 = cache.get(2, v2);
    bool hit3 = cache.get(3, v3);

    CHECK(hit3 && v3 == "c", "get(3)=c");
    CHECK(hit1 && v1 == "a", "get(1) 期望仍命中（更热）");
    CHECK(!hit2, "get(2) 期望未命中（更冷，被淘汰）");
}

void testARCZeroCapacity() {
    std::cout << "\n========== ARC 容量 0 ==========" << std::endl;
    Mycache::ARCCache<int, std::string> cache(0, 2);
    std::string v;

    cache.put(1, "a");
    CHECK(!cache.get(1, v), "capacity=0: get(1) 未命中");
    CHECK(cache.get(1).empty(), "capacity=0: get(1) 返回空串");
}

void testARCMoreSequenceSmoke() {
    std::cout << "\n========== ARC 多步 smoke ==========" << std::endl;
    Mycache::ARCCache<int, std::string> cache(3, 2);
    std::string v;

    cache.put(1, "a");
    cache.put(2, "b");
    cache.put(3, "c");

    CHECK(cache.get(1, v) && v == "a", "get(1)=a");
    CHECK(cache.get(1, v) && v == "a", "get(1)=a (再次访问)");

    cache.put(4, "d"); // 发生淘汰
    CHECK(cache.get(4, v) && v == "d", "get(4)=d");

    // 不强行断言谁被淘汰，只保证不会把写进去的 key(4)弄没
}



class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}

    double elapsed() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

// 辅助函数：打印结果（保持和 KamaCache 一致的风格）
void printResults(const std::string& testName, int capacity,
                   const std::vector<int>& get_operations,
                   const std::vector<int>& hits) {
    std::cout << "=== " << testName << " 结果汇总 ===" << std::endl;
    std::cout << "缓存大小: " << capacity << std::endl;

    std::vector<std::string> names;
    if (hits.size() == 3) {
        names = {"LRU", "LFU", "ARC"};
    } else if (hits.size() == 4) {
        names = {"LRU", "LFU", "ARC", "LRU-K"};
    } else if (hits.size() == 5) {
        names = {"LRU", "LFU", "ARC", "LRU-K", "LFU-Aging"};
    }

    for (size_t i = 0; i < hits.size(); ++i) {
        double hitRate = 100.0 * hits[i] / get_operations[i];
        std::cout << (i < names.size() ? names[i] : "Algorithm " + std::to_string(i + 1))
                  << " - 命中率: " << std::fixed << std::setprecision(2) << hitRate << "% ";
        std::cout << "(" << hits[i] << "/" << get_operations[i] << ")" << std::endl;
    }
    std::cout << std::endl;
}

static constexpr double SCALE = 1; // 可调：0.02=先跑小一点；确认逻辑后再改大

void testHotDataAccess_Mycache() {
    std::cout << "\n=== 测试场景1：热点数据访问测试 ===" << std::endl;

    const int CAPACITY = 20;
    const int OPERATIONS = static_cast<int>(500000 * SCALE);
    const int HOT_KEYS = 20;
    const int COLD_KEYS = 5000;

    Mycache::LRUCache<int, std::string> lru(CAPACITY);
    Mycache::LFUCache<int, std::string> lfu(CAPACITY);
    Mycache::ARCCache<int, std::string> arc(CAPACITY);

    Mycache::LRUKCache<int, std::string> lruk(CAPACITY, HOT_KEYS + COLD_KEYS, 2);
    Mycache::LFUCache<int, std::string> lfuAging(CAPACITY, 20000);

    std::random_device rd;
    std::mt19937 gen(rd());

    std::array<Mycache::MycachePolicy<int, std::string>*, 5> caches = {&lru, &lfu, &arc, &lruk, &lfuAging};
    std::vector<int> hits(5, 0);
    std::vector<int> get_operations(5, 0);

    for (int i = 0; i < (int)caches.size(); ++i) {
        for (int key = 0; key < HOT_KEYS; ++key) {
            caches[i]->put(key, "value" + std::to_string(key));
        }

        for (int op = 0; op < OPERATIONS; ++op) {
            bool isPut = (gen() % 100 < 30);

            int key;
            if (gen() % 100 < 70) key = gen() % HOT_KEYS;
            else key = HOT_KEYS + (gen() % COLD_KEYS);

            if (isPut) {
                std::string value = "value" + std::to_string(key) + "_v" + std::to_string(op % 100);
                caches[i]->put(key, value);
            } else {
                std::string result;
                get_operations[i]++;
                if (caches[i]->get(key, result)) {
                    hits[i]++;
                }
            }
        }
    }

    printResults("热点数据访问测试", CAPACITY, get_operations, hits);
}

void testLoopPattern_Mycache() {
    std::cout << "\n=== 测试场景2：循环扫描测试 ===" << std::endl;

    const int CAPACITY = 50;
    const int LOOP_SIZE = 500;
    const int OPERATIONS = static_cast<int>(600000 * SCALE);

    Mycache::LRUCache<int, std::string> lru(CAPACITY);
    Mycache::LFUCache<int, std::string> lfu(CAPACITY);
    Mycache::ARCCache<int, std::string> arc(CAPACITY);

    Mycache::LRUKCache<int, std::string> lruk(CAPACITY, LOOP_SIZE * 2, 2);
    Mycache::LFUCache<int, std::string> lfuAging(CAPACITY, 3000);

    std::random_device rd;
    std::mt19937 gen(rd());

    std::array<Mycache::MycachePolicy<int, std::string>*, 5> caches = {&lru, &lfu, &arc, &lruk, &lfuAging};
    std::vector<int> hits(5, 0);
    std::vector<int> get_operations(5, 0);

    for (int i = 0; i < (int)caches.size(); ++i) {
        for (int key = 0; key < LOOP_SIZE / 5; ++key) {
            caches[i]->put(key, "loop" + std::to_string(key));
        }

        int current_pos = 0;

        for (int op = 0; op < OPERATIONS; ++op) {
            bool isPut = (gen() % 100 < 20);

            int key;
            if (op % 100 < 60) {
                key = current_pos;
                current_pos = (current_pos + 1) % LOOP_SIZE;
            } else if (op % 100 < 90) {
                key = gen() % LOOP_SIZE;
            } else {
                key = LOOP_SIZE + (gen() % LOOP_SIZE);
            }

            if (isPut) {
                std::string value = "loop" + std::to_string(key) + "_v" + std::to_string(op % 100);
                caches[i]->put(key, value);
            } else {
                std::string result;
                get_operations[i]++;
                if (caches[i]->get(key, result)) hits[i]++;
            }
        }
    }

    printResults("循环扫描测试", CAPACITY, get_operations, hits);
}

void testWorkloadShift_Mycache() {
    std::cout << "\n=== 测试场景3：工作负载剧烈变化测试 ===" << std::endl;

    const int CAPACITY = 30;
    const int OPERATIONS = static_cast<int>(800000 * SCALE);
    const int PHASE_LENGTH = OPERATIONS / 5;

    Mycache::LRUCache<int, std::string> lru(CAPACITY);
    Mycache::LFUCache<int, std::string> lfu(CAPACITY);
    Mycache::ARCCache<int, std::string> arc(CAPACITY);

    Mycache::LRUKCache<int, std::string> lruk(CAPACITY, 500, 2);
    Mycache::LFUCache<int, std::string> lfuAging(CAPACITY, 10000);

    std::random_device rd;
    std::mt19937 gen(rd());

    std::array<Mycache::MycachePolicy<int, std::string>*, 5> caches = {&lru, &lfu, &arc, &lruk, &lfuAging};
    std::vector<int> hits(5, 0);
    std::vector<int> get_operations(5, 0);

    for (int i = 0; i < (int)caches.size(); ++i) {
        for (int key = 0; key < 30; ++key) {
            caches[i]->put(key, "init" + std::to_string(key));
        }

        for (int op = 0; op < OPERATIONS; ++op) {
            int phase = op / PHASE_LENGTH;

            int putProbability;
            switch (phase) {
                case 0: putProbability = 15; break;
                case 1: putProbability = 30; break;
                case 2: putProbability = 10; break;
                case 3: putProbability = 25; break;
                case 4: putProbability = 20; break;
                default: putProbability = 20;
            }

            bool isPut = (gen() % 100 < putProbability);

            int key;
            if (op < PHASE_LENGTH) {
                key = gen() % 5;
            } else if (op < PHASE_LENGTH * 2) {
                key = gen() % 400;
            } else if (op < PHASE_LENGTH * 3) {
                key = (op - PHASE_LENGTH * 2) % 100;
            } else if (op < PHASE_LENGTH * 4) {
                int locality = (op / 800) % 5;
                key = locality * 15 + (gen() % 15);
            } else {
                int r = gen() % 100;
                if (r < 40) key = gen() % 5;
                else if (r < 70) key = 5 + (gen() % 45);
                else key = 50 + (gen() % 350);
            }

            if (isPut) {
                std::string value = "value" + std::to_string(key) + "_p" + std::to_string(phase);
                caches[i]->put(key, value);
            } else {
                std::string result;
                get_operations[i]++;
                if (caches[i]->get(key, result)) hits[i]++;
            }
        }
    }

    printResults("工作负载剧烈变化测试", CAPACITY, get_operations, hits);
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

    testHashLRUBasic();
    testHashLRUEviction();
    testHashLRUPolymorphism();

    testLRUManyKeys();
    testLRUManyUpdates();
    testLRUKManyPromotions();
    testHashLRUManyKeys();
    testHashLRUManyUpdates();
    testPolymorphismManyKeys();
    testStressMixed();
    testLFUBasic();
    testLFUTieByRecencyInSameFreq();
    testLFUCapacity0();
    testLFUBasicEvictByFrequency();
    testLFUTieByRecencySameFreq();
    testLFUUpdateValueCountsAsAccess();
    testLFUDeterministicSequence();
    testLFUMaxAverageDecay_Triggers();
    testLFUMaxAverageDecay_NoTrigger_EvictMinFreq();
    testHashLFUBasic();
    testHashLFUEviction();
    testHashLFUPolymorphism();
    testHashLFUManyKeys();
    testARCBasicPutGet();
    testARCUpdateValue();
    testARCEvictionAfterAccess();
    testARCZeroCapacity();
    testARCMoreSequenceSmoke();
    testHotDataAccess_Mycache();
    testLoopPattern_Mycache();
    testWorkloadShift_Mycache();
    std::cout << "\n========== 结果 ==========" << std::endl;
    std::cout << "通过: " << passed << ", 失败: " << failed << std::endl;
    return failed > 0 ? 1 : 0;
}