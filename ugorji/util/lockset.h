#ifndef _incl_ugorji_util_lockset_
#define _incl_ugorji_util_lockset_

#include <string>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <mutex>

namespace ugorji { 
namespace util { 

class Lock {
public:
    std::mutex mu_;
    std::atomic<int> count_;
};

class LockSetLock {
private:
    void unlock();
public:
    std::vector<Lock*> locks_;
    ~LockSetLock();
};

class LockSet {
private:
    std::unordered_map<std::string, Lock*> m_;
    std::vector<Lock*> tofree_;
    std::mutex mu_;
public:
    ~LockSet();
    void locksFor(std::vector<std::string>& keys, LockSetLock* lsl);
    Lock* lockFor(const std::string& key);
};

// class LockSetLockGuard {
// private:
//     ugorji::util::LockSetLock& ls_;
// public:
//     LockSetLockGuard(ugorji::util::LockSetLock& ls) : ls_(ls) {}
//     ~LockSetLockGuard() {
//         ls_.unlock();
//         delete ls_;
//     }
// };

} // end namespace util
} // end namespace ugorji

#endif // _incl_ugorji_util_lockset_
