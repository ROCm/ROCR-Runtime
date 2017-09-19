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

#ifndef HSA_RUNTME_CORE_SIGNAL_CPP_
#define HSA_RUNTME_CORE_SIGNAL_CPP_

#include "core/inc/signal.h"
#include "core/util/timer.h"
#include <algorithm>

namespace core {

uint32_t Signal::WaitAny(uint32_t signal_count, const hsa_signal_t* hsa_signals,
                         const hsa_signal_condition_t* conds, const hsa_signal_value_t* values,
                         uint64_t timeout, hsa_wait_state_t wait_hint,
                         hsa_signal_value_t* satisfying_value) {
  hsa_signal_handle* signals =
      reinterpret_cast<hsa_signal_handle*>(const_cast<hsa_signal_t*>(hsa_signals));
  uint32_t prior = 0;
  for (uint32_t i = 0; i < signal_count; i++)
    prior = Max(prior, atomic::Increment(&signals[i]->waiting_));

  MAKE_SCOPE_GUARD([&]() {
    for (uint32_t i = 0; i < signal_count; i++)
      atomic::Decrement(&signals[i]->waiting_);
  });

  // Allow only the first waiter to sleep (temporary, known to be bad).
  if (prior != 0) wait_hint = HSA_WAIT_STATE_ACTIVE;

  // Ensure that all signals in the list can be slept on.
  if (wait_hint != HSA_WAIT_STATE_ACTIVE) {
    for (uint32_t i = 0; i < signal_count; i++) {
      if (signals[i]->EopEvent() == NULL) {
        wait_hint = HSA_WAIT_STATE_ACTIVE;
        break;
      }
    }
  }

  const uint32_t small_size = 10;
  HsaEvent* short_evts[small_size];
  HsaEvent** evts = NULL;
  uint32_t unique_evts = 0;
  if (wait_hint != HSA_WAIT_STATE_ACTIVE) {
    if (signal_count > small_size)
      evts = new HsaEvent* [signal_count];
    else
      evts = short_evts;
    for (uint32_t i = 0; i < signal_count; i++)
      evts[i] = signals[i]->EopEvent();
    std::sort(evts, evts + signal_count);
    HsaEvent** end = std::unique(evts, evts + signal_count);
    unique_evts = uint32_t(end - evts);
  }
  MAKE_SCOPE_GUARD([&]() {
    if (signal_count > small_size) delete[] evts;
  });

  int64_t value;

  timer::fast_clock::time_point start_time = timer::fast_clock::now();

  // Set a polling timeout value
  const timer::fast_clock::duration kMaxElapsed = std::chrono::microseconds(200);

  // Convert timeout value into the fast_clock domain
  uint64_t hsa_freq;
  HSA::hsa_system_get_info(HSA_SYSTEM_INFO_TIMESTAMP_FREQUENCY, &hsa_freq);
  const timer::fast_clock::duration fast_timeout =
      timer::duration_from_seconds<timer::fast_clock::duration>(
          double(timeout) / double(hsa_freq));

  bool condition_met = false;
  while (true) {
    for (uint32_t i = 0; i < signal_count; i++) {
      if (signals[i]->invalid_) return uint32_t(-1);

      // Handling special event.
      if (signals[i]->EopEvent() != NULL) {
        const HSA_EVENTTYPE event_type =
            signals[i]->EopEvent()->EventData.EventType;
        if (event_type == HSA_EVENTTYPE_MEMORY) {
          const HsaMemoryAccessFault& fault =
              signals[i]->EopEvent()->EventData.EventData.MemoryAccessFault;
          const uint32_t* failure =
              reinterpret_cast<const uint32_t*>(&fault.Failure);
          if (*failure != 0) {
            return i;
          }
        }
      }

      value =
          atomic::Load(&signals[i]->signal_.value, std::memory_order_relaxed);

      switch (conds[i]) {
        case HSA_SIGNAL_CONDITION_EQ: {
          condition_met = (value == values[i]);
          break;
        }
        case HSA_SIGNAL_CONDITION_NE: {
          condition_met = (value != values[i]);
          break;
        }
        case HSA_SIGNAL_CONDITION_GTE: {
          condition_met = (value >= values[i]);
          break;
        }
        case HSA_SIGNAL_CONDITION_LT: {
          condition_met = (value < values[i]);
          break;
        }
        default:
          return uint32_t(-1);
      }
      if (condition_met) {
        if (satisfying_value != NULL) *satisfying_value = value;
        return i;
      }
    }

    timer::fast_clock::time_point time = timer::fast_clock::now();
    if (time - start_time > fast_timeout) {
      return uint32_t(-1);
    }

    if (wait_hint == HSA_WAIT_STATE_ACTIVE) {
      continue;
    }

    if (time - start_time < kMaxElapsed) {
    //  os::uSleep(20);
      continue;
    }

    uint32_t wait_ms;
    auto time_remaining = fast_timeout - (time - start_time);
    uint64_t ct=timer::duration_cast<std::chrono::milliseconds>(
      time_remaining).count();
    wait_ms = (ct>0xFFFFFFFEu) ? 0xFFFFFFFEu : ct;
    hsaKmtWaitOnMultipleEvents(evts, unique_evts, false, wait_ms);
  }
}

SignalGroup::SignalGroup(uint32_t num_signals, const hsa_signal_t* hsa_signals)
    : count(num_signals) {
  if (count != 0) {
    signals = new hsa_signal_t[count];
  } else {
    signals = NULL;
  }
  if (signals == NULL) return;
  for (unsigned i = 0; i < count; i++) signals[i] = hsa_signals[i];
}

}  // namespace core

#endif  // header guard
