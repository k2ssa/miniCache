#pragma once


#include <unordered_map>
#include <map>
#include <list>
#include "CachePolicy.h"

namespace Mycache{

template <typename Key ,typename Value>
class LFUCache : public MycachePolicy<Key,Value>{
public:
    explicit LFUCache(int capacity):capacity_(capacity),minFreq_(0){}
    

    void put(Key key,Value value) override{
        if(capacity_ <=0) return;

        auto it = keyToNode_.find(key);
        if(it!=keyToNode_.end()){
            it->second.value = std::move(value);
            touch(key);
            return;
        }
        if(static_cast<int>(keyToNode_.size())>=capacity_){
            auto &lst = freqToKeys_[minFreq_];
            Key victim = lst.front();
            lst.pop_front();
            keyToNode_.erase(victim);
        }

        minFreq_ = 1;
        auto &lst1 = freqToKeys_[1];
        lst1.push_back(key);
        auto iter = std::prev(lst1.end());
        keyToNode_.emplace(key,Node(std::move(value),1,iter));
    }


    bool get(Key key,Value& value) override{
        auto it = keyToNode_.find(key);
        if(it == keyToNode_.end()) return false;
        value = it->second.value;
        touch(key);
        return true;
    }

    Value get(Key key) override{
        Value value{};
        get(key,value);
        return value;
    }

private:
    struct Node{
        Value value;
        size_t freq;
        typename std::list<Key>::iterator it;

        Node(Value v,size_t f,typename std::list<Key>::iterator iter):value(std::move(v)),freq(f),it(iter) {}
    };

    void touch(const Key& key){
        auto it = keyToNode_.find(key);
        if(it==keyToNode_.end()) return;
        Node &node = it->second;
        size_t oldFreq = node.freq;

        auto &oldList = freqToKeys_[oldFreq];
        oldList.erase(node.it);
        if(oldFreq == minFreq_ &&oldList.empty()){
            minFreq_ = oldFreq + 1;
        }

        size_t newFreq = oldFreq+1;
        auto &newList = freqToKeys_[newFreq];
        newList.push_back(key);
        auto newIter = std::prev(newList.end());
        
        node.freq = newFreq;
        node.it = newIter;
    }

private:
    int capacity_;
    size_t minFreq_;

    typename std::unordered_map<Key,Node> keyToNode_;

    typename std::map<size_t,std::list<Key>> freqToKeys_;

};





}