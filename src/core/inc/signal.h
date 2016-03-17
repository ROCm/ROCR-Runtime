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

// HSA runtime C++ interface file.

#ifndef HSA_RUNTME_CORE_INC_SIGNAL_H_
#define HSA_RUNTME_CORE_INC_SIGNAL_H_

#include "hsakmt.h"

#include "core/common/shared.h"

#include "core/inc/runtime.h"
#include "core/inc/checked.h"

#include "core/util/utils.h"

#include "inc/amd_hsa_signal.h"

namespace core {
class Signal;

/// @brief Helper structure to simplify conversion of amd_signal_t and
/// core::Signal object.
struct SharedSignal {
  amd_signal_t amd_signal;
  Signal* core_signal;
};

/// @brief An abstract base class which helps implement the public hsa_signal_t
/// type (an opaque handle) and its associated APIs. At its core, signal uses
/// a 32 or 64 bit value. This value can be waitied on or signaled atomically
/// using specified memory ordering semantics.
class Signal : public Checked<0x71FCCA6A3D5D5276>,
               public Shared<SharedSignal, AMD_SIGNAL_ALIGN_BYTES> {
 public:
  /// @brief Constructor initializes the signal with initial value.
  explicit Signal(hsa_signal_value_t initial_value)
      : Shared(), signal_(shared_object()->amd_signal) {
    if (!Shared::IsSharedObjectAllocationValid()) {
      invalid_ = true;
      return;
    }

    shared_object()->core_signal = this;

    signal_.kind = AMD_SIGNAL_KIND_INVALID;
    signal_.value = initial_value;
    invalid_ = false;
    waiting_ = 0;
    retained_ = 0;
  }

  virtual ~Signal() { signal_.kind = AMD_SIGNAL_KIND_INVALID; }

  bool IsValid() const {
    if (CheckedType::IsValid() && !invalid_) return true;
    return false;
  }

  /// @brief Converts from this implementation class to the public
  /// hsa_signal_t type - an opaque handle.
  static __forceinline hsa_signal_t Convert(Signal* signal) {
    const uint64_t handle =
        (signal != NULL && signal->IsValid())
            ? static_cast<uint64_t>(
                  reinterpret_cast<uintptr_t>(&signal->signal_))
            : 0;
    const hsa_signal_t signal_handle = {handle};
    return signal_handle;
  }

  /// @brief Converts from this implementation class to the public
  /// hsa_signal_t type - an opaque handle.
  static __forceinline const hsa_signal_t Convert(const Signal* signal) {
    const uint64_t handle =
        (signal != NULL && signal->IsValid())
            ? static_cast<uint64_t>(
                  reinterpret_cast<uintptr_t>(&signal->signal_))
            : 0;
    const hsa_signal_t signal_handle = {handle};
    return signal_handle;
  }

  /// @brief Converts from public hsa_signal_t type (an opaque handle) to
  /// this implementation class object.
  static __forceinline Signal* Convert(hsa_signal_t signal) {
    return (signal.handle != 0)
               ? reinterpret_cast<const SharedSignal*>(
                     static_cast<uintptr_t>(signal.handle) -
                     (reinterpret_cast<uintptr_t>(
                          &reinterpret_cast<SharedSignal*>(1234)->amd_signal) -
                      uintptr_t(1234)))->core_signal
               : NULL;
  }

  // Below are various methods corresponding to the APIs, which load/store the
  // signal value or modify the existing signal value automically and with
  // specified memory ordering semantics.
  virtual hsa_signal_value_t LoadRelaxed() = 0;
  virtual hsa_signal_value_t LoadAcquire() = 0;

  virtual void StoreRelaxed(hsa_signal_value_t value) = 0;
  virtual void StoreRelease(hsa_signal_value_t value) = 0;

  virtual hsa_signal_value_t WaitRelaxed(hsa_signal_condition_t condition,
                                         hsa_signal_value_t compare_value,
                                         uint64_t timeout,
                                         hsa_wait_state_t wait_hint) = 0;
  virtual hsa_signal_value_t WaitAcquire(hsa_signal_condition_t condition,
                                         hsa_signal_value_t compare_value,
                                         uint64_t timeout,
                                         hsa_wait_state_t wait_hint) = 0;

  virtual void AndRelaxed(hsa_signal_value_t value) = 0;
  virtual void AndAcquire(hsa_signal_value_t value) = 0;
  virtual void AndRelease(hsa_signal_value_t value) = 0;
  virtual void AndAcqRel(hsa_signal_value_t value) = 0;

  virtual void OrRelaxed(hsa_signal_value_t value) = 0;
  virtual void OrAcquire(hsa_signal_value_t value) = 0;
  virtual void OrRelease(hsa_signal_value_t value) = 0;
  virtual void OrAcqRel(hsa_signal_value_t value) = 0;

  virtual void XorRelaxed(hsa_signal_value_t value) = 0;
  virtual void XorAcquire(hsa_signal_value_t value) = 0;
  virtual void XorRelease(hsa_signal_value_t value) = 0;
  virtual void XorAcqRel(hsa_signal_value_t value) = 0;

  virtual void AddRelaxed(hsa_signal_value_t value) = 0;
  virtual void AddAcquire(hsa_signal_value_t value) = 0;
  virtual void AddRelease(hsa_signal_value_t value) = 0;
  virtual void AddAcqRel(hsa_signal_value_t value) = 0;

  virtual void SubRelaxed(hsa_signal_value_t value) = 0;
  virtual void SubAcquire(hsa_signal_value_t value) = 0;
  virtual void SubRelease(hsa_signal_value_t value) = 0;
  virtual void SubAcqRel(hsa_signal_value_t value) = 0;

  virtual hsa_signal_value_t ExchRelaxed(hsa_signal_value_t value) = 0;
  virtual hsa_signal_value_t ExchAcquire(hsa_signal_value_t value) = 0;
  virtual hsa_signal_value_t ExchRelease(hsa_signal_value_t value) = 0;
  virtual hsa_signal_value_t ExchAcqRel(hsa_signal_value_t value) = 0;

  virtual hsa_signal_value_t CasRelaxed(hsa_signal_value_t expected,
                                        hsa_signal_value_t value) = 0;
  virtual hsa_signal_value_t CasAcquire(hsa_signal_value_t expected,
                                        hsa_signal_value_t value) = 0;
  virtual hsa_signal_value_t CasRelease(hsa_signal_value_t expected,
                                        hsa_signal_value_t value) = 0;
  virtual hsa_signal_value_t CasAcqRel(hsa_signal_value_t expected,
                                       hsa_signal_value_t value) = 0;

  //-------------------------
  // implementation specific
  //-------------------------
  typedef void* rtti_t;

  /// @brief Returns the address of the value.
  virtual hsa_signal_value_t* ValueLocation() const = 0;

  /// @brief Applies only to InterrupEvent type, returns the event used to.
  /// Returns NULL for DefaultEvent Type.
  virtual HsaEvent* EopEvent() = 0;

  /// @brief Waits until any signal in the list satisfies its condition or
  /// timeout is reached.
  /// Returns the index of a satisfied signal.  Returns -1 on timeout and
  /// errors.
  static uint32_t WaitAny(uint32_t signal_count, hsa_signal_t* hsa_signals,
                          hsa_signal_condition_t* conds,
                          hsa_signal_value_t* values, uint64_t timeout_hint,
                          hsa_wait_state_t wait_hint,
                          hsa_signal_value_t* satisfying_value);

  __forceinline bool IsType(rtti_t id) { return _IsA(id); }

  /// @brief Allows special case interaction with signal destruction cleanup.
  void Retain() { atomic::Increment(&retained_); }
  void Release() { atomic::Decrement(&retained_); }

  /// @brief Checks if signal is currently in use such that it should not be
  /// deleted.
  bool InUse() const { return (retained_ != 0) || (waiting_ != 0); }

  /// @brief Checks if signal is currently in use by a wait API.
  bool InWaiting() const { return waiting_ != 0; }

  /// @brief Structure which defines key signal elements like type and value.
  /// Address of this struct is used as a value for the opaque handle of type
  /// hsa_signal_t provided to the public API.
  amd_signal_t& signal_;

 protected:
  /// @brief Simple RTTI type checking helper
  /// Returns true if the object can be converted to the query type via
  /// static_cast.
  /// Do not use directly.  Use IsType in the desired derived type instead.
  virtual bool _IsA(rtti_t id) const = 0;

  /// @variable  Indicates if signal is valid or not.
  volatile bool invalid_;

  /// @variable Indicates number of runtime threads waiting on this signal.
  /// Value of zero means no waits.
  volatile uint32_t waiting_;

  volatile uint32_t retained_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Signal);
};

struct hsa_signal_handle {
  hsa_signal_t signal;

  hsa_signal_handle() {}
  hsa_signal_handle(hsa_signal_t Signal) { signal = Signal; }
  operator hsa_signal_t() { return signal; }
  Signal* operator->() { return core::Signal::Convert(signal); }
};
static_assert(
    sizeof(hsa_signal_handle) == sizeof(hsa_signal_t),
    "hsa_signal_handle and hsa_signal_t must have identical binary layout.");
static_assert(
    sizeof(hsa_signal_handle[2]) == sizeof(hsa_signal_t[2]),
    "hsa_signal_handle and hsa_signal_t must have identical binary layout.");

}  // namespace core
#endif  // header guard
