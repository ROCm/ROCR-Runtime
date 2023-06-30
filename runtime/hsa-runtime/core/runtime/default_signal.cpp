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

#include "core/inc/default_signal.h"
#include "core/util/timer.h"

#if defined(__i386__) || defined(__x86_64__)
#include <mwaitxintrin.h>
#define MWAITX_ECX_TIMER_ENABLE 0x2  // BIT(1)
#endif

namespace rocr {
namespace core {

int DefaultSignal::rtti_id_ = 0;
int BusyWaitSignal::rtti_id_ = 0;

BusyWaitSignal::BusyWaitSignal(SharedSignal* abi_block, bool enableIPC)
    : Signal(abi_block, enableIPC) {
  signal_.kind = AMD_SIGNAL_KIND_USER;
  signal_.event_mailbox_ptr = NULL;
}

hsa_signal_value_t BusyWaitSignal::LoadRelaxed() {
  return hsa_signal_value_t(
      atomic::Load(&signal_.value, std::memory_order_relaxed));
}

hsa_signal_value_t BusyWaitSignal::LoadAcquire() {
  return hsa_signal_value_t(
      atomic::Load(&signal_.value, std::memory_order_acquire));
}

void BusyWaitSignal::StoreRelaxed(hsa_signal_value_t value) {
  atomic::Store(&signal_.value, int64_t(value), std::memory_order_relaxed);
}

void BusyWaitSignal::StoreRelease(hsa_signal_value_t value) {
  atomic::Store(&signal_.value, int64_t(value), std::memory_order_release);
}

hsa_signal_value_t BusyWaitSignal::WaitRelaxed(hsa_signal_condition_t condition,
                                               hsa_signal_value_t compare_value, uint64_t timeout,
                                               hsa_wait_state_t wait_hint) {
  Retain();
  MAKE_SCOPE_GUARD([&]() { Release(); });

  waiting_++;
  MAKE_SCOPE_GUARD([&]() { waiting_--; });
  bool condition_met = false;
  int64_t value;

  debug_warning_n((!g_use_interrupt_wait || isIPC()) &&
                  "Use of non-host signal in host signal wait API.", 10);

  timer::fast_clock::time_point start_time, time;
  start_time = timer::fast_clock::now();

  // Set a polling timeout value
  // Should be a few times bigger than null kernel latency
  const timer::fast_clock::duration kMaxElapsed = std::chrono::microseconds(200);

  uint64_t hsa_freq;
  HSA::hsa_system_get_info(HSA_SYSTEM_INFO_TIMESTAMP_FREQUENCY, &hsa_freq);
  const timer::fast_clock::duration fast_timeout =
      timer::duration_from_seconds<timer::fast_clock::duration>(
          double(timeout) / double(hsa_freq));

#if defined(__i386__) || defined(__x86_64__)
  if (g_use_mwaitx) _mm_monitorx(const_cast<int64_t*>(&signal_.value), 0, 0);
#endif

  while (true) {
    if (!IsValid()) return 0;

    value = atomic::Load(&signal_.value, std::memory_order_relaxed);

    switch (condition) {
      case HSA_SIGNAL_CONDITION_EQ: {
        condition_met = (value == compare_value);
        break;
      }
      case HSA_SIGNAL_CONDITION_NE: {
        condition_met = (value != compare_value);
        break;
      }
      case HSA_SIGNAL_CONDITION_GTE: {
        condition_met = (value >= compare_value);
        break;
      }
      case HSA_SIGNAL_CONDITION_LT: {
        condition_met = (value < compare_value);
        break;
      }
      default:
        return 0;
    }
    if (condition_met) return hsa_signal_value_t(value);

    time = timer::fast_clock::now();
    if (time - start_time > fast_timeout) {
      value = atomic::Load(&signal_.value, std::memory_order_relaxed);
      return hsa_signal_value_t(value);
    }

    if (time - start_time > kMaxElapsed) {
      os::uSleep(20);
#if defined(__i386__) || defined(__x86_64__)
    } else if (g_use_mwaitx) {
      _mm_mwaitx(0, 60000, MWAITX_ECX_TIMER_ENABLE);  // 60000 ~20us on a 1.5Ghz CPU
      _mm_monitorx(const_cast<int64_t*>(&signal_.value), 0, 0);
#endif
    }
  }
}

hsa_signal_value_t BusyWaitSignal::WaitAcquire(hsa_signal_condition_t condition,
                                               hsa_signal_value_t compare_value, uint64_t timeout,
                                               hsa_wait_state_t wait_hint) {
  hsa_signal_value_t ret =
      WaitRelaxed(condition, compare_value, timeout, wait_hint);
  std::atomic_thread_fence(std::memory_order_acquire);
  return ret;
}

void BusyWaitSignal::AndRelaxed(hsa_signal_value_t value) {
  atomic::And(&signal_.value, int64_t(value), std::memory_order_relaxed);
}

void BusyWaitSignal::AndAcquire(hsa_signal_value_t value) {
  atomic::And(&signal_.value, int64_t(value), std::memory_order_acquire);
}

void BusyWaitSignal::AndRelease(hsa_signal_value_t value) {
  atomic::And(&signal_.value, int64_t(value), std::memory_order_release);
}

void BusyWaitSignal::AndAcqRel(hsa_signal_value_t value) {
  atomic::And(&signal_.value, int64_t(value), std::memory_order_acq_rel);
}

void BusyWaitSignal::OrRelaxed(hsa_signal_value_t value) {
  atomic::Or(&signal_.value, int64_t(value), std::memory_order_relaxed);
}

void BusyWaitSignal::OrAcquire(hsa_signal_value_t value) {
  atomic::Or(&signal_.value, int64_t(value), std::memory_order_acquire);
}

void BusyWaitSignal::OrRelease(hsa_signal_value_t value) {
  atomic::Or(&signal_.value, int64_t(value), std::memory_order_release);
}

void BusyWaitSignal::OrAcqRel(hsa_signal_value_t value) {
  atomic::Or(&signal_.value, int64_t(value), std::memory_order_acq_rel);
}

void BusyWaitSignal::XorRelaxed(hsa_signal_value_t value) {
  atomic::Xor(&signal_.value, int64_t(value), std::memory_order_relaxed);
}

void BusyWaitSignal::XorAcquire(hsa_signal_value_t value) {
  atomic::Xor(&signal_.value, int64_t(value), std::memory_order_acquire);
}

void BusyWaitSignal::XorRelease(hsa_signal_value_t value) {
  atomic::Xor(&signal_.value, int64_t(value), std::memory_order_release);
}

void BusyWaitSignal::XorAcqRel(hsa_signal_value_t value) {
  atomic::Xor(&signal_.value, int64_t(value), std::memory_order_acq_rel);
}

void BusyWaitSignal::AddRelaxed(hsa_signal_value_t value) {
  atomic::Add(&signal_.value, int64_t(value), std::memory_order_relaxed);
}

void BusyWaitSignal::AddAcquire(hsa_signal_value_t value) {
  atomic::Add(&signal_.value, int64_t(value), std::memory_order_acquire);
}

void BusyWaitSignal::AddRelease(hsa_signal_value_t value) {
  atomic::Add(&signal_.value, int64_t(value), std::memory_order_release);
}

void BusyWaitSignal::AddAcqRel(hsa_signal_value_t value) {
  atomic::Add(&signal_.value, int64_t(value), std::memory_order_acq_rel);
}

void BusyWaitSignal::SubRelaxed(hsa_signal_value_t value) {
  atomic::Sub(&signal_.value, int64_t(value), std::memory_order_relaxed);
}

void BusyWaitSignal::SubAcquire(hsa_signal_value_t value) {
  atomic::Sub(&signal_.value, int64_t(value), std::memory_order_acquire);
}

void BusyWaitSignal::SubRelease(hsa_signal_value_t value) {
  atomic::Sub(&signal_.value, int64_t(value), std::memory_order_release);
}

void BusyWaitSignal::SubAcqRel(hsa_signal_value_t value) {
  atomic::Sub(&signal_.value, int64_t(value), std::memory_order_acq_rel);
}

hsa_signal_value_t BusyWaitSignal::ExchRelaxed(hsa_signal_value_t value) {
  return hsa_signal_value_t(atomic::Exchange(&signal_.value, int64_t(value),
                                             std::memory_order_relaxed));
}

hsa_signal_value_t BusyWaitSignal::ExchAcquire(hsa_signal_value_t value) {
  return hsa_signal_value_t(atomic::Exchange(&signal_.value, int64_t(value),
                                             std::memory_order_acquire));
}

hsa_signal_value_t BusyWaitSignal::ExchRelease(hsa_signal_value_t value) {
  return hsa_signal_value_t(atomic::Exchange(&signal_.value, int64_t(value),
                                             std::memory_order_release));
}

hsa_signal_value_t BusyWaitSignal::ExchAcqRel(hsa_signal_value_t value) {
  return hsa_signal_value_t(atomic::Exchange(&signal_.value, int64_t(value),
                                             std::memory_order_acq_rel));
}

hsa_signal_value_t BusyWaitSignal::CasRelaxed(hsa_signal_value_t expected,
                                              hsa_signal_value_t value) {
  return hsa_signal_value_t(atomic::Cas(&signal_.value, int64_t(value),
                                        int64_t(expected),
                                        std::memory_order_relaxed));
}

hsa_signal_value_t BusyWaitSignal::CasAcquire(hsa_signal_value_t expected,
                                              hsa_signal_value_t value) {
  return hsa_signal_value_t(atomic::Cas(&signal_.value, int64_t(value),
                                        int64_t(expected),
                                        std::memory_order_acquire));
}

hsa_signal_value_t BusyWaitSignal::CasRelease(hsa_signal_value_t expected,
                                              hsa_signal_value_t value) {
  return hsa_signal_value_t(atomic::Cas(&signal_.value, int64_t(value),
                                        int64_t(expected),
                                        std::memory_order_release));
}

hsa_signal_value_t BusyWaitSignal::CasAcqRel(hsa_signal_value_t expected,
                                             hsa_signal_value_t value) {
  return hsa_signal_value_t(atomic::Cas(&signal_.value, int64_t(value),
                                        int64_t(expected),
                                        std::memory_order_acq_rel));
}

}  // namespace core
}  // namespace rocr
