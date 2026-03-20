#pragma once


#include <unordered_map>
#include <map>
#include <list>
#include "CachePolicy.h"
#include <mutex>

namespace Mycache{

template <typename Key ,typename Value>
class LFUCache : public MycachePolicy<Key,Value>{
public:
    explicit LFUCache(int capacity, int maxAverageNum = 1000000)
    :capacity_(capacity)
    ,minFreq_(0)
    ,maxAverageNum_(maxAverageNum)
    ,curAverageNum_(0)
    ,curTotalNum_(0){}
    

    void put(Key key,Value value) override{
        if(capacity_ <=0) return;

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = keyToNode_.find(key);
        if(it!=keyToNode_.end()){
            it->second.value = std::move(value);
            touch(key);
            return;
        }
        if(static_cast<int>(keyToNode_.size())>=capacity_){
            KickOut();
            // auto &lst = freqToKeys_[minFreq_];
            // Key victim = lst.front();
            // lst.pop_front();
            // keyToNode_.erase(victim);
        }

        minFreq_ = 1;
        auto &lst1 = freqToKeys_[1];
        lst1.push_back(key);
        auto iter = std::prev(lst1.end());
        keyToNode_.emplace(key,Node(std::move(value),1,iter));
        addFreqNum(1);
    }


    bool get(Key key,Value& value) override{
        std::lock_guard<std::mutex> lock(mutex_);
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
        addFreqNum(1);
        if(curAverageNum_ > maxAverageNum_){
            handleOverMaxAverageNum();
        }
    }

    void KickOut(){
        auto itList = freqToKeys_.find(minFreq_);
        if(itList==freqToKeys_.end()||itList->second.empty()) return ;
        auto &lst = itList->second;
        Key victim = lst.front();
        lst.pop_front();

        auto itNode = keyToNode_.find(victim);
        if(itNode != keyToNode_.end()){
            decreaseFreqNum(static_cast<int>(itNode->second.freq));
            keyToNode_.erase(itNode);
        }

        if(lst.empty()){

        }
    }

    void addFreqNum(int delta){
        curTotalNum_ +=delta;
        if(keyToNode_.empty()){
            curAverageNum_ = 0;
        }
        else{
            curAverageNum_ = curTotalNum_ / static_cast<int>(keyToNode_.size());
        }
    }

    void decreaseFreqNum(int num){
        curTotalNum_ -=num;
        if(curTotalNum_<0) curTotalNum_=0;
        if(keyToNode_.empty()){
            curAverageNum_=0;
        }
        else{
            curAverageNum_ = curTotalNum_ / static_cast<int>(keyToNode_.size());
        }
    }

    void handleOverMaxAverageNum(){
        if(keyToNode_.empty()) return;

        int decay = maxAverageNum_ / 2;
        if(decay <=0) decay =1;

        for(auto &pair: keyToNode_){
            Key k = pair.first;
            Node &node = pair.second;

            size_t oldFreq = node.freq;
            auto &oldList = freqToKeys_[oldFreq];
            oldList.erase(node.it);

            int newFreqInt = static_cast<int>(oldFreq) - decay;
            if(newFreqInt < 1) newFreqInt =1;
            size_t newFreq = static_cast<size_t>(newFreqInt);

            int delta = static_cast<int>(newFreq) - static_cast<int>(oldFreq);
            curTotalNum_ +=delta;

            auto &newList = freqToKeys_[newFreq];
            newList.push_back(k);
            auto newIter = std::prev(newList.end());

            node.freq = newFreq;
            node.it = newIter;

        }
        updateMinFreq();
        if(!keyToNode_.empty()){
            curAverageNum_ = curTotalNum_ / static_cast<int>(keyToNode_.size());
        }
        else{
            curAverageNum_ = 0;
        }
    }

    void updateMinFreq() {
        minFreq_ = 0;
        for (auto &p : freqToKeys_) {
            if (!p.second.empty()) {
                minFreq_ = p.first;
                break;
            }
        }
        if (minFreq_ == 0) minFreq_ = 1;
    }

private:
    int capacity_;
    size_t minFreq_;

    int maxAverageNum_;
    int curAverageNum_;
    int curTotalNum_;

    typename std::unordered_map<Key,Node> keyToNode_;

    typename std::map<size_t,std::list<Key>> freqToKeys_;

    std::mutex mutex_;
};

template <typename Key,typename Value>
class HashLFUCache: public MycachePolicy<Key,Value>{
public:
    HashLFUCache(size_t capacity,int sliceNum,int maxAverageNum = 10)
    :capacity_(capacity)
    ,sliceNum_(sliceNum>0?sliceNum:static_cast<int>(std::thread::hardware_concurrency()))
    {
        size_t sliceSize = (capacity_ + sliceNum_ -1)/sliceNum_;
        if(sliceSize < 1) sliceSize=1;
        for(int  i=0;i<sliceNum_;i++){
            lfuSlice_.emplace_back(
                std::make_unique<LFUCache<Key,Value>>(static_cast<int>(sliceSize),maxAverageNum)

            );
        }
    }

    void put(Key key,Value value) override{
        lfuSlice_[index(key)]->put(key,std::move(value));
    }

    bool get(Key key,Value& value) override{
        return lfuSlice_[index(key)]->get(key,value);
    }

    Value get(Key key)override{
        Value v{};
        get(key,v);
        return v;
    }


private:
    size_t index(Key key) const{
        return std::hash<Key>{}(key) % static_cast<size_t>(sliceNum_);

    }

    size_t capacity_;
    int sliceNum_;
    std::vector<std::unique_ptr<LFUCache<Key,Value>>> lfuSlice_;


};

}