#include "common.h"
#include <fstream>
#include <chrono>
#include <mutex>
#include <vector>
#include <thread>
#include <list>
#include <iostream>
#include <sys/time.h>
#include <cmath>
#include <syslog.h>

#include "object.h"
#include <boost/core/demangle.hpp>

namespace Common
{

namespace
{
    std::ofstream m_file_log;
    std::mutex g_log_mutex;
    std::mutex g_thred_reg_mx;
    LogLevel g_cons_force_level = LogLevel::NO;
    std::string g_syslog_ident;
}

LogLevel str_to_loglevel(const char* lvl)
{
    if (lvl)
    {
        if (!strcmp("INF", lvl))
            return Common::LogLevel::INF;
        else
        if (!strcmp("ERR", lvl))
            return Common::LogLevel::ERR;
        else
        if (!strcmp("WRN", lvl))
            return Common::LogLevel::WRN;
        else
        if (!strcmp("DBG", lvl))
            return Common::LogLevel::DBG;
    }

    return Common::LogLevel::NO;
}

void initialize_log(std::string indent)
{
    if (indent.empty())
        indent = "log";

    m_file_log.open(indent + ".log", std::ofstream::out|std::ofstream::app);
    g_syslog_ident = indent;

    g_cons_force_level = Common::LogLevel::DBG;
}

namespace {
// maxinum 10 named threads, for lockfree using
std::vector< std::pair<std::thread::id, std::string > > m_threads(10);
    
std::string thread_name(const std::thread::id& id)
{
    for(size_t i = 0; i < m_threads.size(); ++i) {
        if (m_threads[i].first == id)
            return m_threads[i].second;
    }
    
    return "";
}

}

void register_thread(const std::thread::id& id, const std::string& name)
{
    std::unique_lock<std::mutex> lock(g_thred_reg_mx);
    
    for(size_t i = 0; i < m_threads.size(); ++i) {
        if (m_threads[i].first == std::thread::id()) {
            m_threads[i].first = id;
            m_threads[i].second = name;
            break;
        }
    }

    pthread_setname_np(pthread_self(), name.c_str());
}

void register_current_thread(const std::string& name)
{
    auto id = std::this_thread::get_id();
    register_thread(id, name);
}

namespace {

struct count_holder
{
    std::string name;
    RefCountType ref_count;

    count_holder(const std::string& nm)
        : name(nm)
    {}

    bool operator< (const count_holder& r) const
    {
        return name < r.name;
    }
};

typedef std::list< count_holder > counter_container;

static counter_container& counter_instance()
{
    static counter_container objcounter;
    return objcounter;
}

std::mutex gCounterMx;
}

RefCountType& register_counter(const char* name)
{
    const char* tname = name ? name : "undef_type";
    std::string nm = demangle_type_name( tname );

    std::unique_lock<std::mutex> lock(gCounterMx);
    counter_instance().emplace_back(nm);
    counter_instance().back().ref_count = 0;
    return counter_instance().back().ref_count;
}

std::string demangle_type_name(const char* tname)
{
    return boost::core::demangle( tname );
}

int get_objects_count()
{
    int total = 0;
    std::unique_lock<std::mutex> lock(gCounterMx);
    //for ()
    return total;
}

void dump_objects_count(unsigned mincount)
{
    counter_container::iterator ite;
    {
        std::unique_lock<std::mutex> lock(gCounterMx);
        ite = counter_instance().end();
    }

    std::ostringstream sstr;

    for (auto it = counter_instance().begin(); it != ite; ++it)
    {
        unsigned count = it->ref_count;
        if (count >= mincount)
            sstr << it->name << ": " << count << std::endl;
    }

    LOG("Objects count: " << std::endl << sstr.str());
}


const char* LogLevel2Prefix(LogLevel level)
{
    switch(level)
    {
    case LogLevel::INF:
        return "I>";
    case LogLevel::ERR:
        return "E>";
    case LogLevel::WRN:
        return "W>";
    case LogLevel::DBG:
        return "D>";
    default:
        break;
    }

    return "L>";
}

int get_sys_log_level(LogLevel level)
{
    switch(level)
    {
    case LogLevel::INF:
        return LOG_INFO;
    case LogLevel::ERR:
        return LOG_ERR;
    case LogLevel::WRN:
        return LOG_WARNING;
    case LogLevel::DBG:
        return LOG_DEBUG;
    default:
        break;
    }

    return LOG_ALERT;
}

void trace_log(LogLevel level, const std::string& str, bool copy_to_console, const char* file, int line)
{
    //syslog(GetSyslogLevel(level), "%s", str.c_str());

    char buffer[26];
    int millisec;
    struct tm* tm_info;
    struct timeval tv;

    gettimeofday(&tv, nullptr);

    millisec = static_cast<int>(lrint(tv.tv_usec/1000.0)); // Round to nearest millisec
    if (millisec>=1000)
    { // Allow for rounding up to nearest second
        millisec -=1000;
        tv.tv_sec++;
    }

    tm_info = localtime(&tv.tv_sec);

    strftime(buffer, 26, "%y:%m:%d %H:%M:%S", tm_info);
    char smilli[12];
    snprintf(smilli, 12, ".%03d", millisec);

    std::string tname = thread_name(std::this_thread::get_id());
    
    auto dump = [&](std::ostream& strm, LogLevel colorLevel = LogLevel::NO) {
        switch(colorLevel)
        {
        case LogLevel::INF:
            strm << "\e[1;32m";break;
        case LogLevel::ERR:
            strm << "\e[1;31m";break;
        case LogLevel::WRN:
            strm << "\e[1;33m";break;
        default:
            break;
        }

        strm << LogLevel2Prefix(level) << buffer << smilli << " t:";
        if (tname.empty())
            strm << std::this_thread::get_id();
        else
            strm << tname;
        
        strm << " " << str << " (" << file << ":" << line << ")" << std::endl;
        if (colorLevel != LogLevel::NO)
            strm << "\e[m";
    };
    

    std::unique_lock<std::mutex> lock(g_log_mutex);
    dump(m_file_log);

    if (copy_to_console || g_cons_force_level >= level)
        dump(std::cout, level);
    lock.unlock();
}

}
