#pragma once 

#include <string>
#include <vector>
#include <list>
#include <thread>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "CachePolicy.h"

template <typename Key,typename Value> class LruNode;

namespace Mycache{
template <typename Key,typename Value> class LruNode;

template <typename Key,typename Value>
class LruNode{
    public:
    Key key_;
    Value value_;
    std::weak_ptr<LruNode> prev_;
    std::shared_ptr<LruNode> next_;

    LruNode(Key k,Value v) : key_(k),value_(v){}

    Key getKey() const{return key_;}
    Value getValue() const{return value_;}
    void setValue(const Value& v){value_ = v;}

    template < typename K, typename V> friend class LRUCache;
    };


template <typename Key,typename Value>
class LRUCache : public MycachePolicy<Key,Value> {
public:
    using Node = LruNode<Key,Value>;
    using NodePtr = std::shared_ptr<Node>;
    using NodeMap = std::unordered_map<Key,NodePtr>;

    explicit LRUCache(int capacity): capacity_(capacity){
        initList();
    }

    void put(Key key,Value value) override;
    bool get(Key key,Value &value) override;
    Value get(Key key) override;
    void remove(Key key){
        auto it = nodeMap_.find(key);
        if(it == nodeMap_.end()) return;
        removeNode(it->second);
        nodeMap_.erase(it);
    }

private:
    void initList();
    void addNewNode(Key key,Value value);//加一个节点
    void moveToMostRecent(NodePtr node);//将节点移到最前面
    void removeNode(NodePtr node);//移除
    void insertNode(NodePtr node);//插入节点
    void evictLeastRecent();//淘汰一个

    int capacity_;
    NodeMap nodeMap_;
    NodePtr dummyHead_;
    NodePtr dummyTail_;
    std::mutex mutex_;
};

template <typename Key,typename Value>
void LRUCache<Key,Value>::initList(){
    dummyHead_ = std::make_shared<Node>(Key(),Value());
    dummyTail_ = std::make_shared<Node>(Key(),Value());
    dummyHead_->next_ = dummyTail_;
    dummyTail_->prev_ = dummyHead_;
}

template <typename Key,typename Value>
void LRUCache<Key,Value>::put(Key k,Value v){
    if(capacity_<=0) return;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodeMap_.find(k);
    if(it!=nodeMap_.end()){
        it->second->setValue(v);
        moveToMostRecent(it->second);
        return;
    }
    addNewNode(k,v);
}

template <typename Key,typename Value>
bool LRUCache<Key,Value>::get(Key k,Value &v){
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodeMap_.find(k);
    if(it == nodeMap_.end()) return false;
    moveToMostRecent(it->second);
    v = it->second->getValue();
    return true;
}

template <typename Key,typename Value>
Value LRUCache<Key,Value>::get(Key k){
    Value v{};
    get(k,v);
    return v;
}

template <typename Key,typename Value>
void LRUCache<Key,Value>::addNewNode(Key k,Value v){
    if(nodeMap_.size() >= capacity_){
        evictLeastRecent();
    }
    NodePtr  cur = std::make_shared<Node>(k,v);
    insertNode(cur);
    nodeMap_[k]=cur;
}

template <typename Key,typename Value>
void LRUCache<Key,Value>::moveToMostRecent(NodePtr node){
    removeNode(node);
    insertNode(node);
}

template <typename Key,typename Value>
void LRUCache<Key,Value>::removeNode(NodePtr node){
    if(node->prev_.expired()||!node->next_) return;
    auto prev = node->prev_.lock();
    prev->next_ = node->next_;
    node->next_->prev_ =prev;
    node->next_ = nullptr;

}

template <typename Key,typename Value>
void LRUCache<Key,Value>::insertNode(NodePtr node){
    node->next_ = dummyHead_->next_;
    node->prev_ = dummyHead_;
    dummyHead_->next_->prev_ = node;
    dummyHead_->next_ = node;
}

template <typename Key,typename Value>
void LRUCache<Key,Value>::evictLeastRecent(){
    NodePtr cur = dummyTail_->prev_.lock();
    if(!cur || cur == dummyHead_ ) return;
    removeNode(cur);
    nodeMap_.erase(cur->getKey());
}


template <typename Key,typename Value>
class LRUKCache : public LRUCache<Key,Value>{
public:
    using Base = LRUCache<Key,Value>;

    LRUKCache(int capacity,int historyCapacity,int k)
    : Base(capacity)
    ,historyList_(std::make_unique<LRUCache<Key,size_t>>(historyCapacity))
    ,k_(k){}

    void put(Key key,Value value) override;
    bool get(Key key,Value& value) override;
    Value get(Key key) override;

private:
    int k_;
    std::unique_ptr<LRUCache<Key,size_t>> historyList_;
    std::unordered_map<Key,Value> historyValueMap_;
    mutable std::mutex mutex_;
};

template <typename Key,typename Value>
bool LRUKCache<Key,Value>::get(Key key,Value& value){
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = historyList_->get(key);
    count++;
    historyList_->put(key,count);
    bool hit = Base::get(key,value);
    if(hit) return true;
    auto it = historyValueMap_.find(key);
    if(count>=k_ &&it!=historyValueMap_.end()){
        Value storedValue = it->second;
        historyList_->remove(key);
        historyValueMap_.erase(key);
        Base::put(key,storedValue);
        value = storedValue;
        return true;
    }
    return false;

}

template <typename Key,typename Value>
Value LRUKCache<Key,Value>::get(Key key){
    Value v{};
    get(key,v);
    return v;
}

template <typename Key,typename Value>
void LRUKCache<Key,Value>::put(Key key,Value value){
    std::lock_guard<std::mutex> lock(mutex_);
    Value tmp{}; 
    bool hit = Base::get(key,tmp);
    if(hit){
        Base::put(key,value);
        return;
    }
    size_t count = historyList_->get(key);
    count++;
    historyList_->put(key,count);
    historyValueMap_[key] = value;
    if(count>=k_){
        historyList_->remove(key);
        historyValueMap_.erase(key);
        Base::put(key,value);
    }
}

template <typename Key,typename Value>
class HASHLRUCache : public MycachePolicy<Key,Value>{
public:
    HASHLRUCache(size_t capacity,int sliceNum):
    capacity_(capacity),
    sliceNum_(sliceNum>0?sliceNum:static_cast<int>(std::thread::hardware_concurrency())){
        size_t sliceSize = (capacity_ + sliceNum_ -1) / sliceNum_;
        if(sliceSize<0) sliceSize=1;
        for(int i=0;i<sliceNum_;i++){
            lruslices_.emplace_back(std::make_unique<LRUCache<Key,Value>>(static_cast<int>(sliceSize)));
        }
    }
    void put(Key key,Value value) override;
    bool get(Key key,Value &value) override;
    Value get(Key key)override;

private:
    size_t hashKey(Key key) const{
        return std::hash<Key>{}(key);
    }
    size_t index(Key key) const{
        return hashKey(key) %static_cast<size_t>(sliceNum_);
    }

    size_t capacity_;
    int sliceNum_;
    std::vector<std::unique_ptr<LRUCache<Key,Value>>> lruslices_;
};

template <typename Key,typename Value>
void HASHLRUCache<Key,Value>::put(Key key,Value value){
    lruslices_[index(key)]->put(key,value);
}

template<typename Key,typename Value>
bool HASHLRUCache<Key,Value>::get(Key key,Value &value){
    return lruslices_[index(key)]->get(key,value);
}

template<typename Key,typename Value>
Value HASHLRUCache<Key,Value>::get(Key key){
    Value v{};
    get(key,v);
    return v;
}

}