#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/file.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <exception>

namespace aql_profile {

class Logger {
 public:
  void msg(const std::string& m) { log(m); }

  void prn(const char* fmt, ...) {
    const size_t formatted_size = 256;
    char formatted_string[formatted_size];
    va_list argptr;
    va_start(argptr, fmt);
    vsnprintf(formatted_string, formatted_size, fmt, argptr);
    va_end(argptr);
    msg(formatted_string);
  }

  template <typename T> Logger& operator<<(const T& m) {
    std::ostringstream oss;
    oss << m;
    if (!streaming)
      log(oss.str());
    else
      put(oss.str());
    streaming = true;
    return *this;
  }

  typedef void (*manip_t)(Logger&);
  Logger& operator<<(manip_t f) {
    f(*this);
    return *this;
  }

  static void endl(Logger& logger) { logger.streaming = false; }

  Logger() : file(NULL), dirty(false), streaming(false) {
    const char* path = getenv("HSA_EXT_AQL_PROFILE_LOG");
    if (path != NULL) {
      file = fopen("/tmp/aql_profile_log.txt", "a");
    }
  }
  ~Logger() {
    if (file != NULL) {
      if (dirty) put("\n");
      fclose(file);
    }
  }

 private:
  void put(const std::string& m) {
    if (file != NULL) {
      dirty = true;
      flock(fileno(file), LOCK_EX);
      fprintf(file, "%s", m.c_str());
      fflush(file);
      flock(fileno(file), LOCK_UN);
    }
  }

  void log(const std::string& m) {
    const time_t rawtime = time(NULL);
    const tm* tm_info = localtime(&rawtime);
    char tm_str[26];
    strftime(tm_str, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    std::ostringstream oss;
    oss << "\n<" << tm_str << std::dec << " pid" << syscall(__NR_getpid) << " tid"
        << syscall(__NR_gettid) << "> " << m;
    put(oss.str());
  }

  FILE* file;
  bool dirty;
  bool streaming;
};

}  // aql_profile

#define ERR_LOGGING(logger)                                                                        \
  (logger << aql_profile::Logger::endl << "Error: " << __FUNCTION__ << "(): ")

#endif  // _LOGGER_H_
