#pragma once 

#include <string>
#include <list>
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
    if(node->prev_.expired()||!node->next_){
        auto prev = node->prev_.lock();
        prev->next_ = node->next_;
        node->next_->prev_ =prev;
        node->next_ = nullptr;
    }
}

template <typename Key,typename Value>
void LRUCache<Key,Value>::insertNode(NodePtr node){
    node->next_ = dummyHead_->next_;
    node->prev_ = dummyHead_;
    dummyHead_->next_ = node;
    dummyTail_->prev_ = node;
}

template <typename Key,typename Value>
void LRUCache<Key,Value>::evictLeastRecent(){
    NodePtr cur = dummyTail_->prev_.lock();
    if(!cur || cur == dummyHead_ ) return;
    removeNode(cur);
    nodeMap_.erase(cur->getKey());
}

}