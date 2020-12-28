#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <mutex>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include "logging.h"

namespace ugorji { 
namespace util { 

std::string Log::LevelToString(Level l, bool shortForm) {
    std::string s;
    switch(l) {
	case ALL:        
        s = "ALL";
        break;
	case TRACE:      
        s = "TRACE"; 
        break;
	case DEBUG:      
        s =  "DEBUG";
        break;
	case INFO:       
        s =  "INFO";
        break;
	case WARNING:    
        s =  "WARNING";
        break;
	case ERROR:      
        s =  "ERROR";
        break;
	case SEVERE:     
        s =  "SEVERE";
        break;
	case OFF:        
        s =  "OFF";
        break;
	default: 
        if(shortForm) return std::to_string(int(l));
        return std::to_string(int(l)) + ":Log_Level:";
    }
    if(shortForm) return s.substr(0, 1);
    return s;
}

bool Log::Logv(Level l, const std::string& file, const int line, const char* format, va_list ap) {
    if(l < minLevel_) return false;
    int slen = std::char_traits<char>::length(format);
    if(slen == 0) return false;
    //std::chrono::time_point<std::chrono::system_clock> start, end;
    auto stime = std::chrono::system_clock::now();
    //auto htime = std::chrono::high_resolution_clock::now();
    std::time_t epoch_time = std::chrono::system_clock::to_time_t(stime);
    std::chrono::microseconds usec = std::chrono::duration_cast<std::chrono::microseconds>(stime.time_since_epoch());
    auto numUsec = usec.count() % 1000000;
    auto tm = gmtime(&epoch_time);
    // size_t i;
    // char timebuf[16] = {0};
    // if((i = strftime(timebuf, 16, "%m%d %H:%M:%S", tm)) < 0) return false;

    auto seq = seq_++;
    size_t i = file.find_last_of("/\\");
    std::string fbase = file;
    if(i != std::string::npos) fbase = file.substr(i + 1);
    auto levelstr = LevelToString(l, true).c_str();
    auto tid = ::syscall(SYS_gettid);
    
    std::lock_guard<std::mutex> lock(mu_);
    
    //I XXXX MMdd HH:MM:SS.ssssss XXXXX file:line] msg ...
    // i = dprintf(fd_, "%s %3lu %s.%06ld %5ld %s:%d] ", 
    //             levelstr, seq, timebuf, numUsec, tid, fbase.c_str(), line);
    // Unfortunately, we cannot dynamically create a va_list, so we have to send multiple requests to write.
    // Since fd's are usually buffered, it helps.

    std::string fmt("%s %3lu %02d%02d %02d:%02d:%02d.%06ld %5ld %s:%d] ");
    if(ansi_colors_ && isatty(fd_)) {
        // http://pueblo.sourceforge.net/doc/manual/ansi_color_codes.html
        std::string color = "";
        switch(l) {
        case TRACE: color="32"; break;   // green
        case DEBUG: color="32"; break;   // green
        case INFO: color="34"; break;    // blue
        case WARNING: color="33"; break; // yellow
        case ERROR: color="31"; break;   // red
        case SEVERE: color="31"; break;  // red
        case ALL: case OFF: break;
        }
        if(&color[0] != 0) fmt = std::string("\033[1;")+color+"m"+fmt+"\033[0m";
    }
    
    int ii = dprintf(fd_, fmt.data(), levelstr, seq,
                tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
                numUsec, tid, fbase.data(), line);
    if(ii < 0) return false;
    ii = vdprintf(fd_, format, ap);
    if(ii < 0) return false;
    if(format[slen-1] != '\n') write(fd_, "\n", 1);
    return true;
}

bool Log::Logf(Level l, const std::string& file, const int line, const char* format, ...) {
    if(l < minLevel_) return false;
    va_list ap;
    va_start(ap, format);
    bool b = Logv(l, file, line, format, ap);
    va_end(ap);
    return b;
}

} // end namespace util
} // end namespace ugorji
