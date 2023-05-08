////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
// 
// Copyright (c) 2014-2020, Advanced Micro Devices, Inc. All rights reserved.
// 
// Developed by:
// 
//                 AMD Research and AMD HSA Software Development
// 
//                 Advanced Micro Devices, Inc.
// 
//                 www.amd.com
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal with the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
// 
//  - Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimers.
//  - Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimers in
//    the documentation and/or other materials provided with the distribution.
//  - Neither the names of Advanced Micro Devices, Inc,
//    nor the names of its contributors may be used to endorse or promote
//    products derived from this Software without specific prior written
//    permission.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS WITH THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////

#ifdef __linux__
#include "core/util/os.h"
#include "core/util/utils.h"

#include <link.h>
#include <dlfcn.h>
#include <pthread.h>
#include <limits.h>
#include <sched.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include "core/inc/runtime.h"

namespace rocr {
namespace os {

struct ThreadArgs {
  void* entry_args;
  ThreadEntry entry_function;
};

void* __stdcall ThreadTrampoline(void* arg) {
  ThreadArgs* ar = (ThreadArgs*)arg;
  ThreadEntry CallMe = ar->entry_function;
  void* Data = ar->entry_args;
  delete ar;
  CallMe(Data);
  return nullptr;
}

// Thread container allows multiple waits and separate close (destroy).
class os_thread {
 public:
  explicit os_thread(ThreadEntry function, void* threadArgument, uint stackSize)
      : thread(0), lock(nullptr), state(RUNNING) {
    std::unique_ptr<ThreadArgs> args(new ThreadArgs);
    lock = CreateMutex();
    if (lock == nullptr) return;

    args->entry_args = threadArgument;
    args->entry_function = function;

    pthread_attr_t attrib;
    pthread_attr_init(&attrib);

    if (stackSize != 0) {
      stackSize = Max(uint(PTHREAD_STACK_MIN), stackSize);
      stackSize = AlignUp(stackSize, 4096);
      int err = pthread_attr_setstacksize(&attrib, stackSize);
      assert(err == 0 && "pthread_attr_setstacksize failed.");
    }

    if (core::Runtime::runtime_singleton_->flag().override_cpu_affinity()) {
      int cores = get_nprocs_conf();
      cpu_set_t* cpuset = CPU_ALLOC(cores);
      CPU_ZERO_S(CPU_ALLOC_SIZE(cores), cpuset);
      for (int i = 0; i < cores; i++) {
        CPU_SET(i, cpuset);
      }
      int err = pthread_attr_setaffinity_np(&attrib, CPU_ALLOC_SIZE(cores), cpuset);
      assert(err == 0 && "pthread_attr_setaffinity_np failed.");
      CPU_FREE(cpuset);
    }

    int err = pthread_create(&thread, &attrib, ThreadTrampoline, args.get());

    // Probably a stack size error since system limits can be different from PTHREAD_STACK_MIN
    // Attempt to grow the stack within reason.
    if ((err == EINVAL) && stackSize != 0) {
      while (stackSize < 20 * 1024 * 1024) {
        stackSize *= 2;
        pthread_attr_setstacksize(&attrib, stackSize);
        err = pthread_create(&thread, &attrib, ThreadTrampoline, args.get());
        if (err != EINVAL) break;
      }
    }

    pthread_attr_destroy(&attrib);
    if (err == 0)
      args.release();
    else
      thread = 0;
  }

  os_thread(os_thread&& rhs) {
    thread = rhs.thread;
    lock = rhs.lock;
    state = int(rhs.state);
    rhs.thread = 0;
    rhs.lock = nullptr;
  }

  os_thread(os_thread&) = delete;

  ~os_thread() {
    if (lock != nullptr) DestroyMutex(lock);
    if ((state == RUNNING) && (thread != 0)) pthread_detach(thread);
  }

  bool Valid() { return (lock != nullptr) && (thread != 0); }

  bool Wait() {
    if (state == FINISHED) return true;
    AcquireMutex(lock);
    if (state == FINISHED) {
      ReleaseMutex(lock);
      return true;
    }
    int err = pthread_join(thread, NULL);
    bool success = (err == 0);
    if (success) state = FINISHED;
    ReleaseMutex(lock);
    return success;
  }

 private:
  pthread_t thread;
  Mutex lock;
  std::atomic<int> state;
  enum { FINISHED = 0, RUNNING = 1 };
};

static_assert(sizeof(LibHandle) == sizeof(void*), "OS abstraction size mismatch");
static_assert(sizeof(Mutex) == sizeof(pthread_mutex_t*), "OS abstraction size mismatch");
static_assert(sizeof(SharedMutex) == sizeof(pthread_rwlock_t*), "OS abstraction size mismatch");
static_assert(sizeof(Thread) == sizeof(os_thread*), "OS abstraction size mismatch");

LibHandle LoadLib(std::string filename) {
  void* ret = dlopen(filename.c_str(), RTLD_LAZY);
  if (ret == nullptr) debug_print("LoadLib(%s) failed: %s\n", filename.c_str(), dlerror());
  return *(LibHandle*)&ret;
}

void* GetExportAddress(LibHandle lib, std::string export_name) {
  void* ret = dlsym(*(void**)&lib, export_name.c_str());

  // dlsym searches the given library and all the library's load dependencies.
  // Remaining code limits symbol lookup to only the library handle given.
  // This lookup pattern matches Windows.
  if (ret == NULL) return ret;

  link_map* map;
  int err = dlinfo(*(void**)&lib, RTLD_DI_LINKMAP, &map);
  assert(err != -1 && "dlinfo failed.");

  Dl_info info;
  err = dladdr(ret, &info);
  assert(err != 0 && "dladdr failed.");

  if (strcmp(info.dli_fname, map->l_name) == 0) return ret;

  return NULL;
}

void CloseLib(LibHandle lib) { dlclose(*(void**)&lib); }

/*
 * @brief Look for a symbol called "HSA_AMD_TOOL_PRIORITY" across all loaded
 * shared libraries, and if found, store the name of the library
 *
 * @param[in]: info A dl_phdr_info struct pointer, which contains information
 * about library's load address, header, and name.
 *
 * @param[in]: size integer size of dl_phdr_info struct
 *
 * @param[out]: data copy of the data argument to dl_phdr_iterate call
 *
 * @retval:: Return 0 on Success. If callback returns a non-zero value,
 * dl_iterate_phdr() will stop processing, even if there are unprocessed
 * shared objects.
 */

static int callback(struct dl_phdr_info* info, size_t size, void* data) {
  std::vector<std::string>* loadedToolsLib = (std::vector<std::string>*)data;
  assert(loadedToolsLib != nullptr);
  /*
   * Check if lib name is not empty and its not a "vdso.so" lib,
   * The vDSO is a special shared object file that is built into the Linux kernel.
   * It is not a regular shared library and thus does not have all the properties
   * of regular shared libraries. The way the vDSO is loaded and organized in memory
   * is different from regular shared libraries and it's not guaranteed that it
   * will have a specific segment or section. Hence its skipped.
   */

  if ((info) && (info->dlpi_name[0] != '\0')) {
    if (std::string(info->dlpi_name).find("vdso.so") != std::string::npos) return 0;

    /*
     * Iterate through the program headers of the loaded lib and check for PT_DYNAMIC program
     * header. If the PT_DYNAMIC program header is found, use dlpi_addr and dlpi_phdr members
     * of dl_phdr_info struct to get the address of the dynamic section of the loaded
     * library in memory
     */

    for (int i = 0; i < info->dlpi_phnum; i++) {
      if (info->dlpi_phdr[i].p_type == PT_DYNAMIC) {
        Elf64_Dyn* dyn_section = (Elf64_Dyn*)(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);

        char* strings = nullptr;
        Elf64_Xword limit = 0;

        /*
         * The dynamic section is searched for DT_STRTAB (address of string table),
         * and DT_STRSZ (size of string table)
         * DT_NULL - Marks the end of the _DYNAMIC array
         */

        for (int j = 0;; j++) {
          if (dyn_section[j].d_tag == DT_NULL) break;

          if (dyn_section[j].d_tag == DT_STRTAB) strings = (char*)(dyn_section[j].d_un.d_ptr);

          if (dyn_section[j].d_tag == DT_STRSZ) limit = dyn_section[j].d_un.d_val;
        }

        if (strings == nullptr) debug_print("String table not found");

        /*
         * Hacky lookup, if string and symbol tables are found,
         * iterate through the strings in string table and check if
         * any string matches "HSA_AMD_TOOL_PRIORITY".
         * If yes, then add the name of the library to the vector of
         * lib names
         */
        if (strings != nullptr) {
          char* end = strings + limit;
          while (strings < end) {
            if (strcmp(strings, "HSA_AMD_TOOL_PRIORITY") == 0) {
              loadedToolsLib->push_back(info->dlpi_name);
              return 0;
            }
            strings += (strlen(strings) + 1);
          }
        }
      }
    }
  }
  return 0;
}

std::vector<LibHandle> GetLoadedToolsLib() {
  std::vector<LibHandle> ret;
  std::vector<std::string> names;

  /* Iterate through all of the loaded shared libraries in the process */
  dl_iterate_phdr(callback, &names);

  if (!names.empty()) {
    for (auto& name : names) ret.push_back(LoadLib(name));
  }

  return ret;
}

std::string GetLibraryName(LibHandle lib) {
  link_map *map;
  if(dlinfo(lib, RTLD_DI_LINKMAP, &map)!=0)
    return "";
  return map->l_name;
}

Mutex CreateMutex() {
  pthread_mutex_t* mutex = new pthread_mutex_t;
  pthread_mutex_init(mutex, NULL);
  return *(Mutex*)&mutex;
}

bool TryAcquireMutex(Mutex lock) {
  return pthread_mutex_trylock(*(pthread_mutex_t**)&lock) == 0;
}

bool AcquireMutex(Mutex lock) {
  return pthread_mutex_lock(*(pthread_mutex_t**)&lock) == 0;
}

void ReleaseMutex(Mutex lock) {
  pthread_mutex_unlock(*(pthread_mutex_t**)&lock);
}

void DestroyMutex(Mutex lock) {
  pthread_mutex_destroy(*(pthread_mutex_t**)&lock);
  delete *(pthread_mutex_t**)&lock;
}

void Sleep(int delay_in_millisec) { usleep(delay_in_millisec * 1000); }

void uSleep(int delayInUs) { usleep(delayInUs); }

void YieldThread() { sched_yield(); }

Thread CreateThread(ThreadEntry function, void* threadArgument, uint stackSize) {
  os_thread* result = new os_thread(function, threadArgument, stackSize);
  if (!result->Valid()) {
    delete result;
    return nullptr;
  }

  return reinterpret_cast<Thread>(result);
}

void CloseThread(Thread thread) { delete reinterpret_cast<os_thread*>(thread); }

bool WaitForThread(Thread thread) { return reinterpret_cast<os_thread*>(thread)->Wait(); }

bool WaitForAllThreads(Thread* threads, uint threadCount) {
  for (uint i = 0; i < threadCount; i++) WaitForThread(threads[i]);
  return true;
}

bool IsEnvVarSet(std::string env_var_name) {
  char* buff = NULL;
  buff = getenv(env_var_name.c_str());
  return (buff != NULL);
}

void SetEnvVar(std::string env_var_name, std::string env_var_value) {
  setenv(env_var_name.c_str(), env_var_value.c_str(), 1);
}

std::string GetEnvVar(std::string env_var_name) {
  char* buff;
  buff = getenv(env_var_name.c_str());
  std::string ret;
  if (buff) {
    ret = buff;
  }
  return ret;
}

size_t GetUserModeVirtualMemorySize() {
#ifdef _LP64
  // https://www.kernel.org/doc/Documentation/x86/x86_64/mm.txt :
  // user space is 0000000000000000 - 00007fffffffffff (=47 bits)
  return (size_t)(0x800000000000);
#else
  return (size_t)(0xffffffff);  // ~4GB
#endif
}

size_t GetUsablePhysicalHostMemorySize() {
  struct sysinfo info = {0};
  if (sysinfo(&info) != 0) {
    return 0;
  }

  const size_t physical_size =
      static_cast<size_t>(info.totalram * info.mem_unit);
  return std::min(GetUserModeVirtualMemorySize(), physical_size);
}

uintptr_t GetUserModeVirtualMemoryBase() { return (uintptr_t)0; }

// Os event implementation
typedef struct EventDescriptor_ {
  pthread_cond_t event;
  pthread_mutex_t mutex;
  bool state;
  bool auto_reset;
} EventDescriptor;

EventHandle CreateOsEvent(bool auto_reset, bool init_state) {
  EventDescriptor* eventDescrp;
  eventDescrp = (EventDescriptor*)malloc(sizeof(EventDescriptor));

  pthread_mutex_init(&eventDescrp->mutex, NULL);
  pthread_cond_init(&eventDescrp->event, NULL);
  eventDescrp->auto_reset = auto_reset;
  eventDescrp->state = init_state;

  EventHandle handle = reinterpret_cast<EventHandle>(eventDescrp);

  return handle;
}

int DestroyOsEvent(EventHandle event) {
  if (event == NULL) {
    return -1;
  }

  EventDescriptor* eventDescrp = reinterpret_cast<EventDescriptor*>(event);
  int ret_code = pthread_cond_destroy(&eventDescrp->event);
  ret_code |= pthread_mutex_destroy(&eventDescrp->mutex);
  free(eventDescrp);
  return ret_code;
}

int WaitForOsEvent(EventHandle event, unsigned int milli_seconds) {
  if (event == NULL) {
    return -1;
  }

  EventDescriptor* eventDescrp = reinterpret_cast<EventDescriptor*>(event);
  // Event wait time is 0 and state is non-signaled, return directly
  if (milli_seconds == 0) {
    int tmp_ret = pthread_mutex_trylock(&eventDescrp->mutex);
    if (tmp_ret == EBUSY) {
      // Timeout
      return 1;
    }
  }

  int ret_code = 0;
  pthread_mutex_lock(&eventDescrp->mutex);
  if (!eventDescrp->state) {
    if (milli_seconds == 0) {
      ret_code = 1;
    } else {
      struct timespec ts;
      struct timeval tp;

      ret_code = gettimeofday(&tp, NULL);
      ts.tv_sec = tp.tv_sec;
      ts.tv_nsec = tp.tv_usec * 1000;

      unsigned int sec = milli_seconds / 1000;
      unsigned int mSec = milli_seconds % 1000;

      ts.tv_sec += sec;
      ts.tv_nsec += mSec * 1000000;

      // More then one second, add 1 sec to the tv_sec elem
      if (ts.tv_nsec > 1000000000) {
        ts.tv_sec += 1;
        ts.tv_nsec = ts.tv_nsec - 1000000000;
      }

      ret_code =
          pthread_cond_timedwait(&eventDescrp->event, &eventDescrp->mutex, &ts);
      // Time out
      if (ret_code == 110) {
        ret_code = 0x14003;  // 1 means time out in HSA
      }

      if (ret_code == 0 && eventDescrp->auto_reset) {
        eventDescrp->state = false;
      }
    }
  } else if (eventDescrp->auto_reset) {
    eventDescrp->state = false;
  }
  pthread_mutex_unlock(&eventDescrp->mutex);

  return ret_code;
}

int SetOsEvent(EventHandle event) {
  if (event == NULL) {
    return -1;
  }

  EventDescriptor* eventDescrp = reinterpret_cast<EventDescriptor*>(event);
  int ret_code = 0;
  ret_code = pthread_mutex_lock(&eventDescrp->mutex);
  eventDescrp->state = true;
  ret_code = pthread_mutex_unlock(&eventDescrp->mutex);
  ret_code |= pthread_cond_signal(&eventDescrp->event);

  return ret_code;
}

int ResetOsEvent(EventHandle event) {
  if (event == NULL) {
    return -1;
  }

  EventDescriptor* eventDescrp = reinterpret_cast<EventDescriptor*>(event);
  int ret_code = 0;
  ret_code = pthread_mutex_lock(&eventDescrp->mutex);
  eventDescrp->state = false;
  ret_code = pthread_mutex_unlock(&eventDescrp->mutex);

  return ret_code;
}

static double invPeriod = 0.0;

uint64_t ReadAccurateClock() {
  if (invPeriod == 0.0) AccurateClockFrequency();
  timespec time;
  int err = clock_gettime(CLOCK_MONOTONIC_RAW, &time);
  assert(err == 0 && "clock_gettime(CLOCK_MONOTONIC_RAW,...) failed");
  return (uint64_t(time.tv_sec) * 1000000000ull + uint64_t(time.tv_nsec)) * invPeriod;
}

uint64_t AccurateClockFrequency() {
  static clockid_t clock = CLOCK_MONOTONIC;
  static std::atomic<bool> first(true);
  // Check kernel version - not a concurrency concern.
  // use non-RAW for getres due to bug in older 2.6.x kernels
  if (first.load(std::memory_order_acquire)) {
    utsname kernelInfo;
    if (uname(&kernelInfo) == 0) {
      try {
        std::string ver = kernelInfo.release;
        size_t idx;
        int major = std::stoi(ver, &idx);
        int minor = std::stoi(ver.substr(idx + 1));
        if ((major >= 4) && (minor >= 4)) {
          clock = CLOCK_MONOTONIC_RAW;
        }
      } catch (...) {
        // Kernel version string doesn't conform to the standard pattern.
        // Keep using the "safe" (non-RAW) clock.
      }
    }
    first.store(false, std::memory_order_release);
  }
  timespec time;
  int err = clock_getres(clock, &time);
  assert(err == 0 && "clock_getres(CLOCK_MONOTONIC(_RAW),...) failed");
  assert(time.tv_sec == 0 &&
         "clock_getres(CLOCK_MONOTONIC(_RAW),...) returned very low frequency "
         "(<1Hz).");
  assert(time.tv_nsec < 0xFFFFFFFF &&
         "clock_getres(CLOCK_MONOTONIC(_RAW),...) returned very low frequency "
         "(<1Hz).");
  if (invPeriod == 0.0) invPeriod = 1.0 / double(time.tv_nsec);
  return 1000000000ull / uint64_t(time.tv_nsec);
}

SharedMutex CreateSharedMutex() {
  pthread_rwlockattr_t attrib;
  int err = pthread_rwlockattr_init(&attrib);
  if (err != 0) {
    assert(false && "rw lock attribute init failed.");
    return nullptr;
  }
  err = pthread_rwlockattr_setkind_np(&attrib, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
  assert(err == 0 && "Set rw lock attribute failure.");

  pthread_rwlock_t* lock = new pthread_rwlock_t;
  err = pthread_rwlock_init(lock, &attrib);
  if (err != 0) {
    assert(false && "rw lock init failed.");
    return nullptr;
  }

  pthread_rwlockattr_destroy(&attrib);
  return lock;
}

bool TryAcquireSharedMutex(SharedMutex lock) {
  int err = pthread_rwlock_trywrlock(*(pthread_rwlock_t**)&lock);
  return err == 0;
}

bool AcquireSharedMutex(SharedMutex lock) {
  int err = pthread_rwlock_wrlock(*(pthread_rwlock_t**)&lock);
  return err == 0;
}

void ReleaseSharedMutex(SharedMutex lock) {
  int err = pthread_rwlock_unlock(*(pthread_rwlock_t**)&lock);
  assert(err == 0 && "SharedMutex unlock failed.");
}

bool TrySharedAcquireSharedMutex(SharedMutex lock) {
  int err = pthread_rwlock_tryrdlock(*(pthread_rwlock_t**)&lock);
  return err == 0;
}

bool SharedAcquireSharedMutex(SharedMutex lock) {
  int err = pthread_rwlock_rdlock(*(pthread_rwlock_t**)&lock);
  return err == 0;
}

void SharedReleaseSharedMutex(SharedMutex lock) {
  int err = pthread_rwlock_unlock(*(pthread_rwlock_t**)&lock);
  assert(err == 0 && "SharedMutex unlock failed.");
}

void DestroySharedMutex(SharedMutex lock) {
  pthread_rwlock_destroy(*(pthread_rwlock_t**)&lock);
  delete *(pthread_rwlock_t**)&lock;
}

static uint64_t sys_clock_period_ = 0;

uint64_t ReadSystemClock() {
  struct timespec ts;
  clock_gettime(CLOCK_BOOTTIME, &ts);
  uint64_t time = (uint64_t(ts.tv_sec) * 1000000000 + uint64_t(ts.tv_nsec));
  if (sys_clock_period_ != 1)
    return time / sys_clock_period_;
  else
    return time;
}

uint64_t SystemClockFrequency() {
  struct timespec ts;
  clock_getres(CLOCK_BOOTTIME, &ts);
  sys_clock_period_ = (uint64_t(ts.tv_sec) * 1000000000 + uint64_t(ts.tv_nsec));
  return 1000000000 / sys_clock_period_;
}

}   //  namespace os
}   //  namespace rocr

#endif
