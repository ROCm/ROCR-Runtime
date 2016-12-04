////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
// 
// Copyright (c) 2014-2015, Advanced Micro Devices, Inc. All rights reserved.
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

#include <link.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sched.h>
#include <string>
#include <cstring>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <unistd.h>

namespace os {

static_assert(sizeof(LibHandle) == sizeof(void*),
              "OS abstraction size mismatch");
static_assert(sizeof(Mutex) == sizeof(pthread_mutex_t*),
              "OS abstraction size mismatch");
static_assert(sizeof(Thread) == sizeof(pthread_t),
              "OS abstraction size mismatch");

LibHandle LoadLib(std::string filename) {
  void* ret = dlopen(filename.c_str(), RTLD_LAZY);
  return *(LibHandle*)&ret;
}

void* GetExportAddress(LibHandle lib, std::string export_name) {
  void* ret = dlsym(*(void**)&lib, export_name.c_str());

  // dlsym searches the given library and all the library's load dependencies.
  // Remaining code limits symbol lookup to only the library handle given.
  // This lookup pattern matches Windows.
  if (ret == nullptr) return ret;

  link_map* map;
  int err = dlinfo(*(void**)&lib, RTLD_DI_LINKMAP, &map);
  assert(err != -1 && "dlinfo failed.");

  Dl_info info;
  err = dladdr(ret, &info);
  assert(err != 0 && "dladdr failed.");

  if (strcmp(info.dli_fname, map->l_name) == 0) return ret;

  return nullptr;
}

void CloseLib(LibHandle lib) { dlclose(*(void**)&lib); }

Mutex CreateMutex() {
  pthread_mutex_t* mutex = new pthread_mutex_t;
  pthread_mutex_init(mutex, nullptr);
  return *reinterpret_cast<Mutex*>(&mutex);
}

bool TryAcquireMutex(Mutex lock) {
  return pthread_mutex_trylock(*reinterpret_cast<pthread_mutex_t**>(&lock)) == 0;
}

bool AcquireMutex(Mutex lock) {
  return pthread_mutex_lock(*reinterpret_cast<pthread_mutex_t**>(&lock)) == 0;
}

void ReleaseMutex(Mutex lock) {
  pthread_mutex_unlock(*reinterpret_cast<pthread_mutex_t**>(&lock));
}

void DestroyMutex(Mutex lock) {
  pthread_mutex_destroy(*reinterpret_cast<pthread_mutex_t**>(&lock));
  delete *reinterpret_cast<pthread_mutex_t**>(&lock);
}

void Sleep(int delay_in_millisec) { usleep(delay_in_millisec * 1000); }

void YieldThread() { sched_yield(); }

struct ThreadArgs {
  void* entry_args;
  ThreadEntry entry_function;
};

void* __stdcall ThreadTrampoline(void* arg) {
  ThreadArgs* ar = reinterpret_cast<ThreadArgs*>(arg);
  ThreadEntry CallMe = ar->entry_function;
  void* Data = ar->entry_args;
  delete ar;
  CallMe(Data);
  return nullptr;
}

Thread CreateThread(ThreadEntry function, void* threadArgument,
                    uint stackSize) {
  ThreadArgs* args = new ThreadArgs;
  args->entry_args = threadArgument;
  args->entry_function = function;
  pthread_t thread;
  pthread_attr_t attrib;
  pthread_attr_init(&attrib);
  if (stackSize != 0) pthread_attr_setstacksize(&attrib, stackSize);
  bool success =
      (pthread_create(&thread, &attrib, ThreadTrampoline, args) == 0);
  pthread_attr_destroy(&attrib);
  if (!success) {
    pthread_join(thread, nullptr);
    return nullptr;
  }
  return *reinterpret_cast<Thread*>(&thread);
}

void CloseThread(Thread thread) { pthread_detach(*reinterpret_cast<pthread_t*>(&thread)); }

bool WaitForThread(Thread thread) {
  return pthread_join(*reinterpret_cast<pthread_t*>(&thread), nullptr);
}

bool WaitForAllThreads(Thread* threads, uint threadCount) {
  for (uint i = 0; i < threadCount; i++) WaitForThread(threads[i]);
  return true;
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
  return static_cast<size_t>(0x800000000000);
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

uintptr_t GetUserModeVirtualMemoryBase() { return static_cast<uintptr_t>(0); }

// Os event implementation
typedef struct EventDescriptor_ {
  pthread_cond_t event;
  pthread_mutex_t mutex;
  bool state;
  bool auto_reset;
} EventDescriptor;

EventHandle CreateOsEvent(bool auto_reset, bool init_state) {
  EventDescriptor* eventDescrp;
  eventDescrp = reinterpret_cast<EventDescriptor*>(malloc(sizeof(EventDescriptor)));

  pthread_mutex_init(&eventDescrp->mutex, nullptr);
  pthread_cond_init(&eventDescrp->event, nullptr);
  eventDescrp->auto_reset = auto_reset;
  eventDescrp->state = init_state;

  EventHandle handle = reinterpret_cast<EventHandle>(eventDescrp);

  return handle;
}

int DestroyOsEvent(EventHandle event) {
  if (event == nullptr) {
    return -1;
  }

  EventDescriptor* eventDescrp = reinterpret_cast<EventDescriptor*>(event);
  int ret_code = pthread_cond_destroy(&eventDescrp->event);
  ret_code |= pthread_mutex_destroy(&eventDescrp->mutex);
  free(eventDescrp);
  return ret_code;
}

int WaitForOsEvent(EventHandle event, unsigned int milli_seconds) {
  if (event == nullptr) {
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

      ret_code = gettimeofday(&tp, nullptr);
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
  if (event == nullptr) {
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
  if (event == nullptr) {
    return -1;
  }

  EventDescriptor* eventDescrp = reinterpret_cast<EventDescriptor*>(event);
  int ret_code = 0;
  ret_code = pthread_mutex_lock(&eventDescrp->mutex);
  eventDescrp->state = false;
  ret_code = pthread_mutex_unlock(&eventDescrp->mutex);

  return ret_code;
}

uint64_t ReadAccurateClock() {
  timespec time;
  int err = clock_gettime(CLOCK_MONOTONIC_RAW, &time);
  assert(err == 0 && "clock_gettime(CLOCK_MONOTONIC_RAW,...) failed");
  return uint64_t(time.tv_sec) * 1000000000ull + uint64_t(time.tv_nsec);
}

uint64_t AccurateClockFrequency() {
  timespec time;
  int err = clock_getres(CLOCK_MONOTONIC_RAW, &time);
  assert(err == 0 && "clock_getres(CLOCK_MONOTONIC_RAW,...) failed");
  assert(time.tv_sec == 0 &&
         "clock_getres(CLOCK_MONOTONIC_RAW,...) returned very low frequency "
         "(<1Hz).");
  assert(time.tv_nsec < 0xFFFFFFFF &&
         "clock_getres(CLOCK_MONOTONIC_RAW,...) returned very low frequency "
         "(<1Hz).");
  return uint64_t(time.tv_nsec) * 1000000000ull;
}
}

#endif
