#pragma once

#include "CachePolicy.h"
#include <memory>
#include <unordered_map>
#include <map>
#include <list>


namespace Mycache{

template <typename Key,typename Value>
class ArcNode{
public:
    Key key_;
    Value value_;
    size_t accessCount_;
    std::weak_ptr<ArcNode> prev_;
    std::shared_ptr<ArcNode> next_;


    ArcNode():accessCount_(1),next_(nullptr){}
    ArcNode(Key k,Value v):key_(k),value_(v),accessCount_(1),next_(nullptr){}


    Key getKey() const {return key_;}
    Value getValue() const {return value_;}
    size_t getAccessCount() const {return accessCount_;}
    void setValue(const Value& v){value_=v;}
    void incrementAccessCount() {accessCount_++;}

    template<typename K ,typename V> friend class ArcLruPart;
    template<typename K ,typename V> friend class ArcLfuPart;


};

template<typename Key,typename Value>
class ArcLruPart{
public:
    using NodeType = ArcNode<Key,Value>;
    using NodePtr = std::shared_ptr<NodeType>;
    using NodeMap = std::unordered_map<Key,NodePtr>;


    ArcLruPart(size_t capacity,size_t transformThreshold)
    :capacity_(capacity)
    ,transformThreshold_(transformThreshold)
    ,ghostCapacity_(capacity){
        mainHead_ = std::make_shared<NodeType>();
        mainTail_ = std::make_shared<NodeType>();
        mainHead_->next_ = mainTail_;
        mainTail_->prev_ = mainHead_;
        ghostHead_ = std::make_shared<NodeType>();
        ghostTail_ = std::make_shared<NodeType>();
        ghostHead_->next_ = ghostTail_;
        ghostTail_->prev_ = ghostHead_;
    }


    bool put(Key key,Value value){
        if(capacity_==0) return false;
        auto it = mainCache_.find(key);
        if(it!=mainCache_.end()){
            it->second->setValue(value);
            moveToFront(it->second);
            return true;
        }
        if(mainCache_.size()>=capacity_) evictLeastRecent();
        NodePtr node =std::make_shared<NodeType>(key,value);
        mainCache_[key]=node;
        addToFront(node);
        return true;
    }

    bool get(Key key,Value& value, bool& shouldTransform){
        auto it = mainCache_.find(key);
        if(it == mainCache_.end()) return false;
        NodePtr node = it->second;
        moveToFront(node);
        node->incrementAccessCount();
        shouldTransform = (node->getAccessCount() >= transformThreshold_);
        value = node->getValue();
        return true; 
    }


    bool checkGhost(Key key){
        auto it = ghostCache_.find(key);
        if(it==ghostCache_.end()) return false;
        removeFromGhost(it->second);
        ghostCache_.erase(it);
        return true;
    }

    void increaseCapacity(){ ++capacity_;}
    bool decreaseCapacity(){
        if(capacity_<=0) return false;
        if(mainCache_.size()==capacity_) evictLeastRecent();
        --capacity_;
        return true;
    }

private:
    void moveToFront(NodePtr node){
        if(node->prev_.expired()|| !node->next_) return;
        auto prev = node->prev_.lock();
        prev->next_ = node->next_;
        node->next_->prev_ = prev;
        node->next_ = nullptr;
        addToFront(node);
    }

    void addToFront(NodePtr node){
        node->next_ = mainHead_->next_;
        node->prev_ = mainHead_;
        mainHead_->next_->prev_ = node;
        mainHead_->next_ = node ;
    }

    void evictLeastRecent(){
        NodePtr lr = mainTail_->prev_.lock();
        if(!lr||lr == mainHead_) return;
        removeFromMain(lr);
        if(ghostCache_.size()>=ghostCapacity_) removeOldestGhost();
        addToGhost(lr);
        mainCache_.erase(lr->getKey());
    }

    void removeFromMain(NodePtr node){
        if(node->prev_.expired()||!node->next_) return;
        auto prev = node->prev_.lock();
        prev->next_ = node->next_;
        node->next_->prev_ = prev;
        node->next_ = nullptr;
    }

    void addToGhost(NodePtr node){
        node->accessCount_ = 1;
        node->next_ = ghostHead_->next_;
        node->prev_ = ghostHead_;
        ghostHead_->next_->prev_ = node;
        ghostHead_->next_ = node;
        ghostCache_[node->getKey()] = node;
    }

    void removeFromGhost(NodePtr node){
        if(node->prev_.expired()||!node->next_) return;
        auto prev = node->prev_.lock();
        prev->next_ = node->next_;
        node->next_->prev_ = prev;
        node->next_ = nullptr;

    }

    void removeOldestGhost(){
        NodePtr old = ghostTail_->prev_.lock();
        if(!old||old== ghostHead_) return;
        removeFromGhost(old);
        ghostCache_.erase(old->getKey());
    }
    

    size_t capacity_,ghostCapacity_,transformThreshold_;
    NodeMap mainCache_,ghostCache_;
    NodePtr mainHead_,mainTail_,ghostHead_,ghostTail_;
};


template <typename Key,typename Value>
class ArcLfuPart{
public:
    using NodeType = ArcNode<Key,Value>;
    using NodePtr = std::shared_ptr<NodeType>;
    using NodeMap = std::unordered_map<Key,NodePtr>;
    using FreqMap = std::map<size_t,std::list<NodePtr>>;


    ArcLfuPart(size_t capacity, size_t )
    :capacity_(capacity)
    ,ghostCapacity_(capacity)
    ,minFreq_(0)
    {
        ghostHead_ = std::make_shared<NodeType>();
        ghostTail_ = std::make_shared<NodeType>();
        ghostHead_->next_ = ghostTail_;
        ghostTail_->prev_ = ghostHead_;
    }

    bool put(Key key,Value value){
        if(capacity_==0) return false;
        auto it = mainCache_.find(key);
        if(it != mainCache_.end()){
            it->second->setValue(value);
            updateFreq(it->second);
            return true;
        }
        if(mainCache_.size()>=capacity_) evictLeastFrequent();
        NodePtr node = std::make_shared<NodeType>(key,value);
        mainCache_[key]=node;
        freqMap_[1].push_back(node);
        minFreq_=1;
        return true;
    }

    bool get(Key key,Value& value){
        auto it = mainCache_.find(key);
        if(it==mainCache_.end()) return false;
        value = it->second->getValue();
        updateFreq(it->second);
        return true;
    }

    Value get(Key key){
        Value v{};
        get(key,v);
        return v;
    }



    bool contain(Key key) const{return mainCache_.find(key)!=mainCache_.end();}

    bool checkGhost(Key key){
        auto it = ghostCache_.find(key);
        if(it == ghostCache_.end()) return false;
        removeFromGhost(it->second);
        ghostCache_.erase(it);
        return true;
    }

    void increaseCapacity(){
        capacity_++;
    }
    bool decreaseCapacity(){
        if(capacity_<=0)return false;
        if(mainCache_.size() == capacity_) evictLeastFrequent();
        --capacity_;
        return true;
    }

private:
    void updateFreq(NodePtr node){
        size_t oldF = node->getAccessCount();
        node->incrementAccessCount();
        size_t newF = node->getAccessCount();
        freqMap_[oldF].remove(node);
        if(freqMap_[oldF].empty()){
            freqMap_.erase(oldF);
            if(oldF==minFreq_) minFreq_ =newF;
        }
        if(freqMap_.find(newF) == freqMap_.end()) freqMap_[newF] = std::list<NodePtr>();
        freqMap_[newF].push_back(node);
    }


    void evictLeastFrequent(){
        if(freqMap_.empty() || freqMap_[minFreq_].empty()) return;
        NodePtr node = freqMap_[minFreq_].front();
        freqMap_[minFreq_].pop_front();
        if(freqMap_[minFreq_].empty()){
            freqMap_.erase(minFreq_);
            minFreq_ = freqMap_.empty()?1:freqMap_.begin()->first;
        }
        if(ghostCache_.size()>=ghostCapacity_) removeOldestGhost();
        addToGhost(node);
        mainCache_.erase(node->getKey());
    }

    void addToGhost(NodePtr node){
        node->next_ = ghostTail_;
        node->prev_ = ghostTail_->prev_;
        if(!ghostTail_->prev_.expired()) ghostTail_->prev_.lock()->next_ = node;
        ghostTail_->prev_ = node;
        ghostCache_[node->getKey()] = node;
    }

    void removeFromGhost(NodePtr node){
        if(node->prev_.expired()||!node->next_) return;
        auto prev = node->prev_.lock();
        prev->next_ = node->next_;
        node->next_->prev_ = prev;
        node->next_ = nullptr;
    }

    void removeOldestGhost(){
        NodePtr old = ghostHead_->next_;
        if(old==ghostTail_) return;
        removeFromGhost(old);
        ghostCache_.erase(old->getKey());
    }

    size_t capacity_,ghostCapacity_;
    size_t minFreq_;
    NodeMap mainCache_,ghostCache_;
    FreqMap freqMap_;
    NodePtr ghostHead_,ghostTail_;
};


template <typename Key,typename Value>
class ARCCache:public MycachePolicy<Key,Value>{
public:
    explicit ARCCache(size_t capacity = 10,size_t transformThreshold=2)
    :capacity_(capacity)
    ,transformThreshold_(transformThreshold)
    ,lruPart_(std::make_unique<ArcLruPart<Key,Value>>(capacity,transformThreshold))
    ,lfuPart_(std::make_unique<ArcLfuPart<Key,Value>>(capacity,transformThreshold)){}

    void put(Key key,Value value)override{
        checkGhostCaches(key);
        bool inlfu =lfuPart_->contain(key);
        lruPart_->put(key,value);
        if(inlfu) lfuPart_->put(key,value);
    }

    bool get(Key key,Value& value)override{
        checkGhostCaches(key);
        bool shouldTransform =false;
        if(lruPart_->get(key,value,shouldTransform)){
            if(shouldTransform) lfuPart_->put(key,value);
            return true;
        }
        return lfuPart_->get(key,value);
    }

    Value get(Key key)override{
        Value v{};
        get(key,v);
        return v;
    }




private:
    bool checkGhostCaches(Key key){
        if(lruPart_->checkGhost(key)){
            if(lfuPart_->decreaseCapacity()) lruPart_->increaseCapacity();
            return true;
        }
        if(lfuPart_->checkGhost(key)){
            if(lruPart_->decreaseCapacity()) lfuPart_->increaseCapacity();
            return true;
        }
        return false;
    }

    size_t capacity_,transformThreshold_;
    std::unique_ptr<ArcLruPart<Key,Value>> lruPart_;
    std::unique_ptr<ArcLfuPart<Key,Value>> lfuPart_;

};

}