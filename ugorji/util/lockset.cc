#include <algorithm>
#include "lockset.h"

namespace ugorji { 
namespace util { 

void LockSetLock::unlock() {
    for(int i = 0; i < locks_.size(); i++) {
        if(locks_[i] != nullptr) {
            locks_[i]->mu_.unlock();
            locks_[i]->count_--;
        }
    }
    locks_.clear();
}

LockSetLock::~LockSetLock() {
    unlock();
}

void LockSet::locksFor(std::vector<std::string>& keys, LockSetLock* lsl) {
    std::sort(keys.begin(), keys.end());
    mu_.lock();
    std::string last = "";
    for(int i = 0; i < keys.size(); i++) {
        if(keys[i] == "" || keys[i] == last) continue;
        last = keys[i];
        lsl->locks_.push_back(lockFor(keys[i]));
    }
    mu_.unlock();
    for(int i = 0; i < lsl->locks_.size(); i++) {
        lsl->locks_[i]->mu_.lock();
    }
}

Lock* LockSet::lockFor(const std::string& key) {
    Lock* lk;
    std::unordered_map<std::string, Lock*>::iterator it = m_.find(key);
    if(it == m_.end()) {
        lk = new Lock;
        tofree_.push_back(lk);
        m_[key] = lk;
    } else {
        lk = it->second;
    }
    lk->count_++;
    return lk;
}

LockSet::~LockSet() {
    for(int i = 0; i < tofree_.size(); i++) delete tofree_[i];
}

} // end namespace util
} // end namespace ugorji
