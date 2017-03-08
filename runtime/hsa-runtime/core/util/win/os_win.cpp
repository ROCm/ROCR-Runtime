////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2014 ADVANCED MICRO DEVICES, INC.
//
// AMD is granting you permission to use this software and documentation(if any)
// (collectively, the "Materials") pursuant to the terms and conditions of the
// Software License Agreement included with the Materials.If you do not have a
// copy of the Software License Agreement, contact your AMD representative for a
// copy.
//
// WARRANTY DISCLAIMER : THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND.AMD DISCLAIMS ALL WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY,
// INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE, NON - INFRINGEMENT, THAT THE
// SOFTWARE WILL RUN UNINTERRUPTED OR ERROR - FREE OR WARRANTIES ARISING FROM
// CUSTOM OF TRADE OR COURSE OF USAGE.THE ENTIRE RISK ASSOCIATED WITH THE USE OF
// THE SOFTWARE IS ASSUMED BY YOU.Some jurisdictions do not allow the exclusion
// of implied warranties, so the above exclusion may not apply to You.
//
// LIMITATION OF LIABILITY AND INDEMNIFICATION : AMD AND ITS LICENSORS WILL NOT,
// UNDER ANY CIRCUMSTANCES BE LIABLE TO YOU FOR ANY PUNITIVE, DIRECT,
// INCIDENTAL, INDIRECT, SPECIAL OR CONSEQUENTIAL DAMAGES ARISING FROM USE OF
// THE SOFTWARE OR THIS AGREEMENT EVEN IF AMD AND ITS LICENSORS HAVE BEEN
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.In no event shall AMD's total
// liability to You for all damages, losses, and causes of action (whether in
// contract, tort (including negligence) or otherwise) exceed the amount of $100
// USD.  You agree to defend, indemnify and hold harmless AMD and its licensors,
// and any of their directors, officers, employees, affiliates or agents from
// and against any and all loss, damage, liability and other expenses (including
// reasonable attorneys' fees), resulting from Your use of the Software or
// violation of the terms and conditions of this Agreement.
//
// U.S.GOVERNMENT RESTRICTED RIGHTS : The Materials are provided with
// "RESTRICTED RIGHTS." Use, duplication, or disclosure by the Government is
// subject to the restrictions as set forth in FAR 52.227 - 14 and DFAR252.227 -
// 7013, et seq., or its successor.Use of the Materials by the Government
// constitutes acknowledgement of AMD's proprietary rights in them.
//
// EXPORT RESTRICTIONS: The Materials may be subject to export restrictions as
//                      stated in the Software License Agreement.
//
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32  // Are we compiling for windows?
#define NOMINMAX

#include "core/util/os.h"

#include <algorithm>
#include <process.h>
#include <string>
#include <windows.h>

#include <emmintrin.h>
#include <pmmintrin.h>
#include <xmmintrin.h>

#undef Yield
#undef CreateMutex

namespace os {

static_assert(sizeof(LibHandle) == sizeof(HMODULE),
              "OS abstraction size mismatch");
static_assert(sizeof(LibHandle) == sizeof(::HANDLE),
              "OS abstraction size mismatch");
static_assert(sizeof(Mutex) == sizeof(::HANDLE),
              "OS abstraction size mismatch");
static_assert(sizeof(Thread) == sizeof(::HANDLE),
              "OS abstraction size mismatch");
static_assert(sizeof(EventHandle) == sizeof(::HANDLE),
              "OS abstraction size mismatch");

LibHandle LoadLib(std::string filename) {
  HMODULE ret = LoadLibrary(filename.c_str());
  return *(LibHandle*)&ret;
}

void* GetExportAddress(LibHandle lib, std::string export_name) {
  return GetProcAddress(*(HMODULE*)&lib, export_name.c_str());
}

void CloseLib(LibHandle lib) { FreeLibrary(*(::HMODULE*)&lib); }

Mutex CreateMutex() { return CreateEvent(NULL, false, true, NULL); }

bool TryAcquireMutex(Mutex lock) {
  return WaitForSingleObject(*(::HANDLE*)&lock, 0) == WAIT_OBJECT_0;
}

bool AcquireMutex(Mutex lock) {
  return WaitForSingleObject(*(::HANDLE*)&lock, INFINITE) == WAIT_OBJECT_0;
}

void ReleaseMutex(Mutex lock) { SetEvent(*(::HANDLE*)&lock); }

void DestroyMutex(Mutex lock) { CloseHandle(*(::HANDLE*)&lock); }

void Sleep(int delay_in_millisecond) { ::Sleep(delay_in_millisecond); }

void uSleep(int delayInUs) { ::Sleep(delayInUs / 1000); }

void YieldThread() { ::Sleep(0); }

struct ThreadArgs {
  void* entry_args;
  ThreadEntry entry_function;
};

unsigned __stdcall ThreadTrampoline(void* arg) {
  ThreadArgs* thread_args = (ThreadArgs*)arg;
  ThreadEntry entry = thread_args->entry_function;
  void* data = thread_args->entry_args;
  delete thread_args;
  entry(data);
  _endthreadex(0);
  return 0;
}

Thread CreateThread(ThreadEntry entry_function, void* entry_argument,
                    uint stack_size) {
  ThreadArgs* thread_args = new ThreadArgs();
  thread_args->entry_args = entry_argument;
  thread_args->entry_function = entry_function;
  uintptr_t ret =
      _beginthreadex(NULL, stack_size, ThreadTrampoline, thread_args, 0, NULL);
  return *(Thread*)&ret;
}

void CloseThread(Thread thread) { CloseHandle(*(::HANDLE*)&thread); }

bool WaitForThread(Thread thread) {
  return WaitForSingleObject(*(::HANDLE*)&thread, INFINITE) == WAIT_OBJECT_0;
}

bool WaitForAllThreads(Thread* threads, uint thread_count) {
  return WaitForMultipleObjects(thread_count, threads, TRUE, INFINITE) ==
         WAIT_OBJECT_0;
}

void SetEnvVar(std::string env_var_name, std::string env_var_value) {
  SetEnvironmentVariable(env_var_name.c_str(), env_var_value.c_str());
}

std::string GetEnvVar(std::string env_var_name) {
  char* buff;
  DWORD char_count = GetEnvironmentVariable(env_var_name.c_str(), NULL, 0);
  if (char_count == 0) return "";
  buff = (char*)alloca(sizeof(char) * char_count);
  GetEnvironmentVariable(env_var_name.c_str(), buff, char_count);
  buff[char_count - 1] = '\0';
  std::string ret = buff;
  return ret;
}

size_t GetUserModeVirtualMemorySize() {
  SYSTEM_INFO system_info = {0};
  GetSystemInfo(&system_info);
  return ((size_t)system_info.lpMaximumApplicationAddress + 1);
}

size_t GetUsablePhysicalHostMemorySize() {
  MEMORYSTATUSEX memory_status = {0};
  memory_status.dwLength = sizeof(memory_status);
  if (GlobalMemoryStatusEx(&memory_status) == 0) {
    return 0;
  }

  const size_t physical_size = static_cast<size_t>(memory_status.ullTotalPhys);
  return std::min(GetUserModeVirtualMemorySize(), physical_size);
}

uintptr_t GetUserModeVirtualMemoryBase() { return (uintptr_t)0; }

// Os event wrappers
EventHandle CreateOsEvent(bool auto_reset, bool init_state) {
  EventHandle evt = reinterpret_cast<EventHandle>(
      CreateEvent(NULL, (BOOL)(!auto_reset), (BOOL)init_state, NULL));
  return evt;
}

int DestroyOsEvent(EventHandle event) {
  if (event == NULL) {
    return -1;
  }
  return CloseHandle(reinterpret_cast<::HANDLE>(event));
}

int WaitForOsEvent(EventHandle event, unsigned int milli_seconds) {
  if (event == NULL) {
    return -1;
  }

  int ret_code =
      WaitForSingleObject(reinterpret_cast<::HANDLE>(event), milli_seconds);
  if (ret_code == WAIT_TIMEOUT) {
    ret_code = 0x14003;  // 0x14003 indicates timeout
  }
  return ret_code;
}

int SetOsEvent(EventHandle event) {
  if (event == NULL) {
    return -1;
  }
  return SetEvent(reinterpret_cast<::HANDLE>(event));
}

int ResetOsEvent(EventHandle event) {
  if (event == NULL) {
    return -1;
  }
  return ResetEvent(reinterpret_cast<::HANDLE>(event));
}

uint64_t ReadAccurateClock() {
  uint64_t ret;
  QueryPerformanceCounter((LARGE_INTEGER*)&ret);
  return ret;
}

uint64_t AccurateClockFrequency() {
  uint64_t ret;
  QueryPerformanceFrequency((LARGE_INTEGER*)&ret);
  return ret;
}
}

#endif
