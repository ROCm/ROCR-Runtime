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

#include "core/inc/interrupt_signal.h"
#include "core/util/timer.h"

namespace core {

HsaEvent* InterruptSignal::CreateEvent() {
  HsaEventDescriptor event_descriptor;
#ifdef __linux__
  event_descriptor.EventType = HSA_EVENTTYPE_SIGNAL;
#else
  event_descriptor.EventType = HSA_EVENTTYPE_QUEUE_EVENT;
#endif
  event_descriptor.SyncVar.SyncVar.UserData = NULL;
  event_descriptor.SyncVar.SyncVarSize = sizeof(hsa_signal_value_t);
  event_descriptor.NodeId = 0;
  HsaEvent* ret = NULL;
  hsaKmtCreateEvent(&event_descriptor, false, false, &ret);
  return ret;
}

int InterruptSignal::rtti_id_ = 0;

void InterruptSignal::DestroyEvent(HsaEvent* evt) { hsaKmtDestroyEvent(evt); }

InterruptSignal::InterruptSignal(hsa_signal_value_t initial_value,
                                 HsaEvent* use_event)
    : Signal(initial_value) {
  if (use_event != NULL) {
    event_ = use_event;
    free_event_ = false;
  } else {
    event_ = CreateEvent();
    free_event_ = true;
  }

  if (event_ != NULL) {
    signal_.event_id = event_->EventId;
    signal_.event_mailbox_ptr = event_->EventData.HWData2;
  } else {
    signal_.event_id = 0;
    signal_.event_mailbox_ptr = 0;
  }
  signal_.kind = AMD_SIGNAL_KIND_USER;
  HSA::hsa_memory_register(this, sizeof(InterruptSignal));

  wait_on_event_ = true;
}

InterruptSignal::~InterruptSignal() {
  invalid_ = true;
  SetEvent();
  while (InUse())
    ;
  if (free_event_) hsaKmtDestroyEvent(event_);
  HSA::hsa_memory_deregister(this, sizeof(InterruptSignal));
}

hsa_signal_value_t InterruptSignal::LoadRelaxed() {
  return hsa_signal_value_t(
      atomic::Load(&signal_.value, std::memory_order_relaxed));
}

hsa_signal_value_t InterruptSignal::LoadAcquire() {
  return hsa_signal_value_t(
      atomic::Load(&signal_.value, std::memory_order_acquire));
}

void InterruptSignal::StoreRelaxed(hsa_signal_value_t value) {
  wait_on_event_ = true;
  atomic::Store(&signal_.value, int64_t(value), std::memory_order_relaxed);
  SetEvent();
}

void InterruptSignal::StoreRelease(hsa_signal_value_t value) {
  wait_on_event_ = true;
  atomic::Store(&signal_.value, int64_t(value), std::memory_order_release);
  SetEvent();
}

hsa_signal_value_t InterruptSignal::WaitRelaxed(
    hsa_signal_condition_t condition, hsa_signal_value_t compare_value,
    uint64_t timeout, hsa_wait_state_t wait_hint) {
  uint32_t prior = atomic::Increment(&waiting_);

  // assert(prior == 0 && "Multiple waiters on interrupt signal!");
  // Allow only the first waiter to sleep (temporary, known to be bad).
  if (prior != 0) wait_hint = HSA_WAIT_STATE_ACTIVE;

  MAKE_SCOPE_GUARD([&]() { atomic::Decrement(&waiting_); });

  int64_t value;

  timer::fast_clock::time_point start_time = timer::fast_clock::now();

  // Set a polling timeout value
  // Exact time is not hugely important, it should just be a short while which
  // is smaller than the thread scheduling quantum (usually around 16ms)
  const timer::fast_clock::duration kMaxElapsed = std::chrono::milliseconds(5);

  uint64_t hsa_freq;
  HSA::hsa_system_get_info(HSA_SYSTEM_INFO_TIMESTAMP_FREQUENCY, &hsa_freq);
  const timer::fast_clock::duration fast_timeout =
      timer::duration_from_seconds<timer::fast_clock::duration>(
          double(timeout) / double(hsa_freq));

  bool condition_met = false;
  while (true) {
    if (invalid_) return 0;

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

    timer::fast_clock::time_point time = timer::fast_clock::now();
    if (time - start_time > kMaxElapsed) {
      if (time - start_time > fast_timeout) {
        value = atomic::Load(&signal_.value, std::memory_order_relaxed);
        return hsa_signal_value_t(value);
      }
      if (wait_on_event_ && wait_hint != HSA_WAIT_STATE_ACTIVE) {
        uint32_t wait_ms;
        auto time_remaining = fast_timeout - (time - start_time);
        if ((timeout == -1) ||
            (time_remaining > std::chrono::milliseconds(uint32_t(-1))))
          wait_ms = uint32_t(-1);
        else
          wait_ms = timer::duration_cast<std::chrono::milliseconds>(
                        time_remaining).count();
        hsaKmtWaitOnEvent(event_, wait_ms);
      }
    }
  }
}

hsa_signal_value_t InterruptSignal::WaitAcquire(
    hsa_signal_condition_t condition, hsa_signal_value_t compare_value,
    uint64_t timeout, hsa_wait_state_t wait_hint) {
  hsa_signal_value_t ret =
      WaitRelaxed(condition, compare_value, timeout, wait_hint);
  std::atomic_thread_fence(std::memory_order_acquire);
  return ret;
}

void InterruptSignal::AndRelaxed(hsa_signal_value_t value) {
  atomic::And(&signal_.value, int64_t(value), std::memory_order_relaxed);
  SetEvent();
}

void InterruptSignal::AndAcquire(hsa_signal_value_t value) {
  atomic::And(&signal_.value, int64_t(value), std::memory_order_acquire);
  SetEvent();
}

void InterruptSignal::AndRelease(hsa_signal_value_t value) {
  atomic::And(&signal_.value, int64_t(value), std::memory_order_release);
  SetEvent();
}

void InterruptSignal::AndAcqRel(hsa_signal_value_t value) {
  atomic::And(&signal_.value, int64_t(value), std::memory_order_acq_rel);
  SetEvent();
}

void InterruptSignal::OrRelaxed(hsa_signal_value_t value) {
  atomic::Or(&signal_.value, int64_t(value), std::memory_order_relaxed);
  SetEvent();
}

void InterruptSignal::OrAcquire(hsa_signal_value_t value) {
  atomic::Or(&signal_.value, int64_t(value), std::memory_order_acquire);
  SetEvent();
}

void InterruptSignal::OrRelease(hsa_signal_value_t value) {
  atomic::Or(&signal_.value, int64_t(value), std::memory_order_release);
  SetEvent();
}

void InterruptSignal::OrAcqRel(hsa_signal_value_t value) {
  atomic::Or(&signal_.value, int64_t(value), std::memory_order_acq_rel);
  SetEvent();
}

void InterruptSignal::XorRelaxed(hsa_signal_value_t value) {
  atomic::Xor(&signal_.value, int64_t(value), std::memory_order_relaxed);
  SetEvent();
}

void InterruptSignal::XorAcquire(hsa_signal_value_t value) {
  atomic::Xor(&signal_.value, int64_t(value), std::memory_order_acquire);
  SetEvent();
}

void InterruptSignal::XorRelease(hsa_signal_value_t value) {
  atomic::Xor(&signal_.value, int64_t(value), std::memory_order_release);
  SetEvent();
}

void InterruptSignal::XorAcqRel(hsa_signal_value_t value) {
  atomic::Xor(&signal_.value, int64_t(value), std::memory_order_acq_rel);
  SetEvent();
}

void InterruptSignal::AddRelaxed(hsa_signal_value_t value) {
  atomic::Add(&signal_.value, int64_t(value), std::memory_order_relaxed);
  SetEvent();
}

void InterruptSignal::AddAcquire(hsa_signal_value_t value) {
  atomic::Add(&signal_.value, int64_t(value), std::memory_order_acquire);
  SetEvent();
}

void InterruptSignal::AddRelease(hsa_signal_value_t value) {
  atomic::Add(&signal_.value, int64_t(value), std::memory_order_release);
  SetEvent();
}

void InterruptSignal::AddAcqRel(hsa_signal_value_t value) {
  atomic::Add(&signal_.value, int64_t(value), std::memory_order_acq_rel);
  SetEvent();
}

void InterruptSignal::SubRelaxed(hsa_signal_value_t value) {
  atomic::Sub(&signal_.value, int64_t(value), std::memory_order_relaxed);
  SetEvent();
}

void InterruptSignal::SubAcquire(hsa_signal_value_t value) {
  atomic::Sub(&signal_.value, int64_t(value), std::memory_order_acquire);
  SetEvent();
}

void InterruptSignal::SubRelease(hsa_signal_value_t value) {
  atomic::Sub(&signal_.value, int64_t(value), std::memory_order_release);
  SetEvent();
}

void InterruptSignal::SubAcqRel(hsa_signal_value_t value) {
  atomic::Sub(&signal_.value, int64_t(value), std::memory_order_acq_rel);
  SetEvent();
}

hsa_signal_value_t InterruptSignal::ExchRelaxed(hsa_signal_value_t value) {
  hsa_signal_value_t ret = hsa_signal_value_t(atomic::Exchange(
      &signal_.value, int64_t(value), std::memory_order_relaxed));
  SetEvent();
  return ret;
}

hsa_signal_value_t InterruptSignal::ExchAcquire(hsa_signal_value_t value) {
  hsa_signal_value_t ret = hsa_signal_value_t(atomic::Exchange(
      &signal_.value, int64_t(value), std::memory_order_acquire));
  SetEvent();
  return ret;
}

hsa_signal_value_t InterruptSignal::ExchRelease(hsa_signal_value_t value) {
  hsa_signal_value_t ret = hsa_signal_value_t(atomic::Exchange(
      &signal_.value, int64_t(value), std::memory_order_release));
  SetEvent();
  return ret;
}

hsa_signal_value_t InterruptSignal::ExchAcqRel(hsa_signal_value_t value) {
  hsa_signal_value_t ret = hsa_signal_value_t(atomic::Exchange(
      &signal_.value, int64_t(value), std::memory_order_acq_rel));
  SetEvent();
  return ret;
}

hsa_signal_value_t InterruptSignal::CasRelaxed(hsa_signal_value_t expected,
                                               hsa_signal_value_t value) {
  hsa_signal_value_t ret = hsa_signal_value_t(
      atomic::Cas(&signal_.value, int64_t(value), int64_t(expected),
                  std::memory_order_relaxed));
  SetEvent();
  return ret;
}

hsa_signal_value_t InterruptSignal::CasAcquire(hsa_signal_value_t expected,
                                               hsa_signal_value_t value) {
  hsa_signal_value_t ret = hsa_signal_value_t(
      atomic::Cas(&signal_.value, int64_t(value), int64_t(expected),
                  std::memory_order_acquire));
  SetEvent();
  return ret;
}

hsa_signal_value_t InterruptSignal::CasRelease(hsa_signal_value_t expected,
                                               hsa_signal_value_t value) {
  hsa_signal_value_t ret = hsa_signal_value_t(
      atomic::Cas(&signal_.value, int64_t(value), int64_t(expected),
                  std::memory_order_release));
  SetEvent();
  return ret;
}

hsa_signal_value_t InterruptSignal::CasAcqRel(hsa_signal_value_t expected,
                                              hsa_signal_value_t value) {
  hsa_signal_value_t ret = hsa_signal_value_t(
      atomic::Cas(&signal_.value, int64_t(value), int64_t(expected),
                  std::memory_order_acq_rel));
  SetEvent();
  return ret;
}

}  // namespace core
