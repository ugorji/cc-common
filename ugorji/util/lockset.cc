#include <algorithm>
#include "lockset.h"

namespace ugorji { 
namespace util { 

void LockSetLock::unlock() {
    for(auto l : locks_) {
        l->mu_.unlock();
        l->count_--;
    }
    locks_.clear();
}

LockSetLock::~LockSetLock() {
    unlock();
}

void LockSet::locksFor(std::vector<std::string>& keys, LockSetLock& lsl) {
    size_t initsz = lsl.locks_.size();
    std::sort(keys.begin(), keys.end());
    mu_.lock();
    std::string last = "";
    for(size_t i = 0; i < keys.size(); i++) {
        if(keys[i] == "" || keys[i] == last) continue;
        last = keys[i];
        lsl.locks_.push_back(lockFor(keys[i]));
    }
    mu_.unlock();
    for(size_t i = initsz; i < lsl.locks_.size(); i++) {
        lsl.locks_[i]->mu_.lock();
    }
}

Lock* LockSet::lockFor(const std::string& key) {
    Lock* lk;
    std::unordered_map<std::string, std::unique_ptr<Lock>>::iterator it = m_.find(key);
    if(it == m_.end()) {
        auto lk2 = std::make_unique<Lock>();
        lk = lk2.get();
        m_[key] = std::move(lk2);
    } else {
        lk = it->second.get();
    }
    lk->count_++;
    return lk;
}

} // end namespace util
} // end namespace ugorji
