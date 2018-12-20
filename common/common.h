#pragma once
#include <string>
#include <sstream>
#include <memory>
#include <functional>
#include <thread>
#include <string.h>

#include "types.h"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define LOG_(ARGS, cc, lvl) do{std::ostringstream __l_sstr; __l_sstr << ARGS; Common::trace_log(lvl, __l_sstr.str(), cc, __FILENAME__, __LINE__);}while(0)
#define LOG(ARGS) LOG_(ARGS, false, Common::LogLevel::DBG)
#define LOG_CONS(ARGS) LOG_(ARGS, true, Common::LogLevel::DBG)
#define THROW_ERR(ARGS) do {std::ostringstream __l_sstr; __l_sstr << ARGS;  __l_sstr << "(" << __FILENAME__ << ":" << __LINE__ << ")"; throw std::logic_error(__l_sstr.str());}while(0)

#define LOGI(ARGS) LOG_(ARGS, false, Common::LogLevel::INF)
#define LOGE(ARGS) LOG_(ARGS, false, Common::LogLevel::ERR)
#define LOGW(ARGS) LOG_(ARGS, false, Common::LogLevel::WRN)

#define TRY try
#define CATCH_(ARGS, lvl, rethrow) catch(std::exception& e) {LOG_(ARGS << " " << e.what(), false, lvl); if(rethrow) throw;} catch(...) {LOG_(ARGS << " undefined error", false, lvl); if(rethrow) throw;}
#define CATCH_ERR(ARGS) CATCH_(ARGS, Common::LogLevel::ERR, false)
#define CATCH_WRN(ARGS) CATCH_(ARGS, Common::LogLevel::WRN, false)

#define LOG_USECOUNT(var) LOG(#var << ".use_count " << var.use_count())

#ifndef NDEBUG
#define LOGD(ARGS) LOG(ARGS)
#else
#define LOGD(ARGS)
#endif
/*#undef LOG
#define LOG(ARGS)*/

#define MANDATORY_PTR(ptr) ptr?ptr:throw std::logic_error("Mandatory ptr is null: "#ptr)

#define CASE_RET_STR(val) case val: return #val;
#define CASE_RET_STR2(en, val) case en::val: return #val;

namespace Common
{

LogLevel str_to_loglevel(const char* lvl);
void initialize_log(std::string indent);
void trace_log(LogLevel level, const std::string& str, bool copy_to_console, const char* file, int line);
int get_sys_log_level(LogLevel level);
std::string demangle_type_name(const char* tname);
void register_thread(const std::thread::id& id, const std::string& name);
void register_current_thread(const std::string& name);

int get_objects_count();
void dump_objects_count(unsigned mincount = 0);

}



