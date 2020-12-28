#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace ugorji { 
namespace util { 

class LockSet;
class LockSetLock;

class Lock {
    friend LockSetLock;
    friend LockSet;
public:
    std::mutex mu_;
    std::atomic<int> count_;
};

// LockSetLock should not be heap allocated.
// Instead, it should only be stack-allocated,
// so we know that the locks it holds are scoped narrowly.
class LockSetLock {
    friend LockSet;
protected:
    // prevent heap allocation
  static void * operator new(std::size_t);
  static void * operator new [] (std::size_t);
private:
    void unlock();
    std::vector<Lock*> locks_;
public:
    ~LockSetLock();
};

class LockSet {
private:
    std::unordered_map<std::string, std::unique_ptr<Lock>> m_;
    std::mutex mu_;
    Lock* lockFor(const std::string& key);
public:
    ~LockSet() {}
    void locksFor(std::vector<std::string>& keys, LockSetLock& lsl);
};

} // end namespace util
} // end namespace ugorji

