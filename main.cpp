#include <iostream>
#include <string>
#include "CachePolicy.h"
#include "LRUCache.h"


int main(){

    Mycache::LRUCache<int,std::string> cache(2);
    cache.put(1,"a");
    cache.put(1,"a");
    std::string v;
    bool hit1 = cache.get(1, v);
    std::cout << "get(1): hit=" << hit1 << ", value=\"" << v << "\" (期望 hit=1, value=\"a\")" << std::endl;
    cache.put(3, "c");   // 应淘汰 2，缓存里是 1 和 3
    bool hit2 = cache.get(2, v);
    std::cout << "get(2): hit=" << hit2 << ", value=\"" << v << "\" (期望 hit=0，2 已被淘汰)" << std::endl;
    bool hit3 = cache.get(1, v);
    std::cout << "get(1): hit=" << hit3 << ", value=\"" << v << "\" (期望 hit=1, value=\"a\")" << std::endl;
    std::string v3 = cache.get(3);
    std::cout << "get(3): value=\"" << v3 << "\" (期望 \"c\")" << std::endl;
    if (!hit2 && hit1 && hit3 && v3 == "c")
        std::cout << "\n全部通过。" << std::endl;
    else
        std::cout << "\n有用例未通过，请检查 LRU 淘汰逻辑。" << std::endl;
    return 0;
}