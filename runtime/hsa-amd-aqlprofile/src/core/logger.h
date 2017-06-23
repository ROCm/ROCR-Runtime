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
#include <mutex>
#include <map>

namespace aql_profile {

class Logger {
 public:
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

  typedef void (Logger::*manip_t)();
  Logger& operator<<(manip_t f) {
    (this->*f)();
    return *this;
  }

  void begm() { messaging = true; }
  void endl() { resetStreaming(); }

  static const std::string& LastMessage() {
    Logger& logger = Instance();
    std::lock_guard<std::mutex> lck(mutex);
    return logger.message[GetTid()];
  }

  static Logger& Instance() {
    std::lock_guard<std::mutex> lck(mutex);
    if (instance == NULL) instance = new Logger();
    return *instance;
  }

  static void Destroy() {
    std::lock_guard<std::mutex> lck(mutex);
    if (instance != NULL) delete instance;
    instance = NULL;
  }

 private:
  static uint32_t GetPid() { return syscall(__NR_getpid); }
  static uint32_t GetTid() { return syscall(__NR_gettid); }

  Logger() : file(NULL), dirty(false), streaming(false), messaging(false) {
    const char* path = getenv("HSA_VEN_AMD_AQLPROFILE_LOG");
    if (path != NULL) {
      file = fopen("/tmp/aql_profile_log.txt", "a");
    }
    resetStreaming();
  }

  ~Logger() {
    if (file != NULL) {
      if (dirty) put("\n");
      fclose(file);
    }
  }

  void resetStreaming() {
    std::lock_guard<std::mutex> lck(mutex);
    if (messaging) {
      message[GetTid()] = "";
    }
    messaging = false;
    streaming = false;
  }

  void put(const std::string& m) {
    std::lock_guard<std::mutex> lck(mutex);
    if (messaging) {
      message[GetTid()] += m;
    }
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
    tm tm_info;
    localtime_r(&rawtime, &tm_info);
    char tm_str[26];
    strftime(tm_str, 26, "%Y-%m-%d %H:%M:%S", &tm_info);
    std::ostringstream oss;
    oss << "\n<" << tm_str << std::dec << " pid" << GetPid() << " tid" << GetTid() << "> " << m;
    put(oss.str());
  }

  FILE* file;
  bool dirty;
  bool streaming;
  bool messaging;

  static std::mutex mutex;
  static Logger* instance;
  std::map<uint32_t, std::string> message;
};

}  // namespace aql_profile

#define ERR_LOGGING                                                                                \
  (aql_profile::Logger::Instance() << aql_profile::Logger::endl                                    \
                                   << "Error: " << __FUNCTION__                                    \
                                   << "(): " << aql_profile::Logger::begm)
#define INFO_LOGGING                                                                               \
  (aql_profile::Logger::Instance() << aql_profile::Logger::endl                                    \
                                   << "Info: " << __FUNCTION__                                     \
                                   << "(): " << aql_profile::Logger::begm)

#endif  // _LOGGER_H_
