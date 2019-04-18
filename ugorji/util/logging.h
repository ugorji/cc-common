#ifndef _incl_ugorji_util_logging_
#define _incl_ugorji_util_logging_

#include <atomic>
#include <mutex>

namespace ugorji { 
namespace util { 

class Log {
public:
    enum Level { ALL, TRACE, DEBUG, INFO, WARNING, ERROR, SEVERE, OFF };
    static std::string LevelToString(Level l, bool shortForm);
    static Log& getInstance() {
        static Log instance;
        return instance;
    }
    int fd_ = 2; //default: stderr
    Level minLevel_ = INFO;
    bool Logf(Level l, std::string file, int line, const char* format, ...);
    bool Logv(Level l, std::string file, int line, const char* format, va_list ap);
private:
    std::atomic<uint64_t> Seq_;
    std::mutex mu_;
    //Log() : fd_(2), minLevel_(INFO) {};
    Log() {};
    Log(Log const&);
    void operator=(Log const&);
};

} // end namespace util
} // end namespace ugorji

#define LOG(level, format, ...) ugorji::util::Log::getInstance().Logf(ugorji::util::Log::level, __FILE__, __LINE__, format, __VA_ARGS__)
// #define LOGTRACE(format, ...) LOG(TRACE, format, ...)

#endif //_incl_ugorji_util_logging_
