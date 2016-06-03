////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2016, Advanced Micro Devices, Inc. All rights reserved.
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

#include "executable.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <atomic>
#include <fstream>
#include <libelf.h>
#include "amd_hsa_elf.h"
#include "amd_hsa_kernel_code.h"
#include "amd_hsa_code.hpp"
#include "amd_hsa_code_util.hpp"
#include "amd_options.hpp"

using namespace amd::hsa;
using namespace amd::hsa::common;

namespace amd {
namespace hsa {
namespace loader {

class LoaderOptions {
public:
  explicit LoaderOptions(std::ostream &error = std::cerr);

  const amd::options::NoArgOption* Help() const { return &help; }
  const amd::options::NoArgOption* DumpCode() const { return &dump_code; }
  const amd::options::NoArgOption* DumpIsa() const { return &dump_isa; }
  const amd::options::NoArgOption* DumpExec() const { return &dump_exec; }
  const amd::options::NoArgOption* DumpAll() const { return &dump_all; }
  const amd::options::ValueOption<std::string>* DumpDir() const { return &dump_dir; }

  bool ParseOptions(const std::string& options);
  void Reset();
  void PrintHelp(std::ostream& out) const;

private:
  /// @brief Copy constructor - not available.
  LoaderOptions(const LoaderOptions&);

  /// @brief Assignment operator - not available.
  LoaderOptions& operator=(const LoaderOptions&);

  amd::options::NoArgOption help;
  amd::options::NoArgOption dump_code;
  amd::options::NoArgOption dump_isa;
  amd::options::NoArgOption dump_exec;
  amd::options::NoArgOption dump_all;
  amd::options::ValueOption<std::string> dump_dir;
  amd::options::OptionParser option_parser;
};

LoaderOptions::LoaderOptions(std::ostream& error) :
  help("help", "print help"),
  dump_code("dump-code", "Dump finalizer output code object"),
  dump_isa("dump-isa", "Dump finalizer output to ISA text file"),
  dump_exec("dump-exec", "Dump executable to text file"),
  dump_all("dump-all", "Dump all finalizer input and output (as above)"),
  dump_dir("dump-dir", "Dump directory"),
  option_parser(false, error)
{
  option_parser.AddOption(&help);
  option_parser.AddOption(&dump_code);
  option_parser.AddOption(&dump_isa);
  option_parser.AddOption(&dump_exec);
  option_parser.AddOption(&dump_all);
  option_parser.AddOption(&dump_dir);
}

bool LoaderOptions::ParseOptions(const std::string& options) {
  return option_parser.ParseOptions(options.c_str());
}

void LoaderOptions::Reset() {
  option_parser.Reset();
}

void LoaderOptions::PrintHelp(std::ostream& out) const {
  option_parser.PrintHelp(out);
}

static const char *LOADER_DUMP_PREFIX = "amdcode";

Loader* Loader::Create(Context* context)
{
  return new AmdHsaCodeLoader(context);
}

void Loader::Destroy(Loader *loader)
{
  delete loader;
}

Executable* AmdHsaCodeLoader::CreateExecutable(
  hsa_profile_t profile, const char *options)
{
  std::lock_guard<std::mutex> lock(executables_mutex);

  executables.push_back(new ExecutableImpl(profile, context, executables.size()));
  return executables.back();
}

void AmdHsaCodeLoader::DestroyExecutable(Executable *executable)
{
  std::lock_guard<std::mutex> lock(executables_mutex);
  executables[((ExecutableImpl*)executable)->id()] = nullptr;
  delete executable;
}

hsa_status_t AmdHsaCodeLoader::IterateExecutables(
  hsa_status_t (*callback)(
    hsa_executable_t executable,
    void *data),
  void *data)
{
  std::lock_guard<std::mutex> lock(executables_mutex);
  assert(callback);

  for (auto &exec : executables) {
    hsa_status_t status = callback(Executable::Handle(exec), data);
    if (status != HSA_STATUS_SUCCESS) {
      return status;
    }
  }

  return HSA_STATUS_SUCCESS;
}

uint64_t AmdHsaCodeLoader::FindHostAddress(uint64_t device_address)
{
  if (device_address == 0) {
    return 0;
  }
  std::lock_guard<std::mutex> lock(executables_mutex);
  for (auto &exec : executables) {
    if (exec != nullptr) {
      uint64_t host_address = exec->FindHostAddress(device_address);
      if (host_address != 0) {
        return host_address;
      }
    }
  }
  return 0;
}

//===----------------------------------------------------------------------===//
// SymbolImpl.                                                                    //
//===----------------------------------------------------------------------===//

bool SymbolImpl::GetInfo(hsa_symbol_info32_t symbol_info, void *value) {
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_TYPE) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_TYPE)),
    "attributes are not compatible"
  );
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_TYPE) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_TYPE)),
    "attributes are not compatible"
  );
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_NAME_LENGTH) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_NAME_LENGTH)),
    "attributes are not compatible"
  );
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_NAME) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_NAME)),
    "attributes are not compatible"
  );
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_MODULE_NAME_LENGTH) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_MODULE_NAME_LENGTH)),
    "attributes are not compatible"
  );
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_MODULE_NAME) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_MODULE_NAME)),
    "attributes are not compatible"
  );
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_LINKAGE) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_LINKAGE)),
    "attributes are not compatible"
  );
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_IS_DEFINITION) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_IS_DEFINITION)),
    "attributes are not compatible"
  );

  assert(value);

  switch (symbol_info) {
    case HSA_CODE_SYMBOL_INFO_TYPE: {
      *((hsa_symbol_kind_t*)value) = kind;
      break;
    }
    case HSA_CODE_SYMBOL_INFO_NAME_LENGTH: {
      std::string matter = "";

      if (linkage == HSA_SYMBOL_LINKAGE_PROGRAM) {
        assert(name.rfind(":") == std::string::npos);
        matter = name;
      } else {
        assert(name.rfind(":") != std::string::npos);
        matter = name.substr(name.rfind(":") + 1);
      }

      *((uint32_t*)value) = matter.size() + 1;
      break;
    }
    case HSA_CODE_SYMBOL_INFO_NAME: {
      std::string matter = "";

      if (linkage == HSA_SYMBOL_LINKAGE_PROGRAM) {
        assert(name.rfind(":") == std::string::npos);
        matter = name;
      } else {
        assert(name.rfind(":") != std::string::npos);
        matter = name.substr(name.rfind(":") + 1);
      }

      memset(value, 0x0, matter.size() + 1);
      memcpy(value, matter.c_str(), matter.size());
      break;
    }
    case HSA_CODE_SYMBOL_INFO_MODULE_NAME_LENGTH: {
      std::string matter = "";

      if (linkage == HSA_SYMBOL_LINKAGE_PROGRAM) {
        assert(name.find(":") == std::string::npos);
        *((uint32_t*)value) = 0;
        return true;
      }

      assert(name.find(":") != std::string::npos);
      matter = name.substr(0, name.find(":"));

      *((uint32_t*)value) = matter.size() + 1;
      break;
    }
    case HSA_CODE_SYMBOL_INFO_MODULE_NAME: {
      std::string matter = "";

      if (linkage == HSA_SYMBOL_LINKAGE_PROGRAM) {
        assert(name.find(":") == std::string::npos);
        return true;
      }

      assert(name.find(":") != std::string::npos);
      matter = name.substr(0, name.find(":"));

      memset(value, 0x0, matter.size() + 1);
      memcpy(value, matter.c_str(), matter.size());
      break;
    }
    case HSA_CODE_SYMBOL_INFO_LINKAGE: {
      *((hsa_symbol_linkage_t*)value) = linkage;
      break;
    }
    case HSA_CODE_SYMBOL_INFO_IS_DEFINITION: {
      *((bool*)value) = is_definition;
      break;
    }
    case HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT:
    case HSA_EXECUTABLE_SYMBOL_INFO_VARIABLE_ADDRESS: {
      if (!is_loaded) {
        return false;
      }
      *((uint64_t*)value) = address;
      break;
    }
    case HSA_EXECUTABLE_SYMBOL_INFO_AGENT: {
      if (!is_loaded) {
        return false;
      }
      *((hsa_agent_t*)value) = agent;
      break;
    }
    default: {
      return false;
    }
  }

  return true;
}

//===----------------------------------------------------------------------===//
// KernelSymbol.                                                              //
//===----------------------------------------------------------------------===//

bool KernelSymbol::GetInfo(hsa_symbol_info32_t symbol_info, void *value) {
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_KERNEL_KERNARG_SEGMENT_SIZE) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_KERNARG_SEGMENT_SIZE)),
    "attributes are not compatible"
  );
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_KERNEL_KERNARG_SEGMENT_ALIGNMENT) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_KERNARG_SEGMENT_ALIGNMENT)),
    "attributes are not compatible"
  );
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_KERNEL_GROUP_SEGMENT_SIZE) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_GROUP_SEGMENT_SIZE)),
    "attributes are not compatible"
  );
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_KERNEL_PRIVATE_SEGMENT_SIZE) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_PRIVATE_SEGMENT_SIZE)),
    "attributes are not compatible"
  );
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_KERNEL_DYNAMIC_CALLSTACK) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_DYNAMIC_CALLSTACK)),
    "attributes are not compatible"
  );

  assert(value);

  switch (symbol_info) {
    case HSA_CODE_SYMBOL_INFO_KERNEL_KERNARG_SEGMENT_SIZE: {
      *((uint32_t*)value) = kernarg_segment_size;
      break;
    }
    case HSA_CODE_SYMBOL_INFO_KERNEL_KERNARG_SEGMENT_ALIGNMENT: {
      *((uint32_t*)value) = kernarg_segment_alignment;
      break;
    }
    case HSA_CODE_SYMBOL_INFO_KERNEL_GROUP_SEGMENT_SIZE: {
      *((uint32_t*)value) = group_segment_size;
      break;
    }
    case HSA_CODE_SYMBOL_INFO_KERNEL_PRIVATE_SEGMENT_SIZE: {
      *((uint32_t*)value) = private_segment_size;
      break;
    }
    case HSA_CODE_SYMBOL_INFO_KERNEL_DYNAMIC_CALLSTACK: {
      *((bool*)value) = is_dynamic_callstack;
      break;
    }
    case HSA_EXT_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT_SIZE: {
      *((uint32_t*)value) = size;
      break;
    }
    case HSA_EXT_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT_ALIGN: {
      *((uint32_t*)value) = alignment;
      break;
    }
    default: {
      return SymbolImpl::GetInfo(symbol_info, value);
    }
  }

  return true;
}

//===----------------------------------------------------------------------===//
// VariableSymbol.                                                            //
//===----------------------------------------------------------------------===//

bool VariableSymbol::GetInfo(hsa_symbol_info32_t symbol_info, void *value) {
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_VARIABLE_ALLOCATION) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_VARIABLE_ALLOCATION)),
    "attributes are not compatible"
  );
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_VARIABLE_SEGMENT) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_VARIABLE_SEGMENT)),
    "attributes are not compatible"
  );
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_VARIABLE_ALIGNMENT) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_VARIABLE_ALIGNMENT)),
    "attributes are not compatible"
  );
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_VARIABLE_SIZE) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_VARIABLE_SIZE)),
    "attributes are not compatible"
  );
  static_assert(
    (symbol_attribute32_t(HSA_CODE_SYMBOL_INFO_VARIABLE_IS_CONST) ==
     symbol_attribute32_t(HSA_EXECUTABLE_SYMBOL_INFO_VARIABLE_IS_CONST)),
    "attributes are not compatible"
  );

  switch (symbol_info) {
    case HSA_CODE_SYMBOL_INFO_VARIABLE_ALLOCATION: {
      *((hsa_variable_allocation_t*)value) = allocation;
      break;
    }
    case HSA_CODE_SYMBOL_INFO_VARIABLE_SEGMENT: {
      *((hsa_variable_segment_t*)value) = segment;
      break;
    }
    case HSA_CODE_SYMBOL_INFO_VARIABLE_ALIGNMENT: {
      *((uint32_t*)value) = alignment;
      break;
    }
    case HSA_CODE_SYMBOL_INFO_VARIABLE_SIZE: {
      *((uint32_t*)value) = size;
      break;
    }
    case HSA_CODE_SYMBOL_INFO_VARIABLE_IS_CONST: {
      *((bool*)value) = is_constant;
      break;
    }
    default: {
      return SymbolImpl::GetInfo(symbol_info, value);
    }
  }

  return true;
}

bool LoadedCodeObjectImpl::GetInfo(amd_loaded_code_object_info_t attribute, void *value)
{
  assert(value);

  switch (attribute) {
    case AMD_LOADED_CODE_OBJECT_INFO_ELF_IMAGE:
      ((hsa_code_object_t*)value)->handle = reinterpret_cast<uint64_t>(elf_data);
      break;
    case AMD_LOADED_CODE_OBJECT_INFO_ELF_IMAGE_SIZE:
      *((size_t*)value) = elf_size;
      break;
    default: {
      return false;
    }
  }

  return true;
}

hsa_status_t LoadedCodeObjectImpl::IterateLoadedSegments(
  hsa_status_t (*callback)(
    amd_loaded_segment_t loaded_segment,
    void *data),
  void *data)
{
  assert(callback);

  for (auto &loaded_segment : loaded_segments) {
    hsa_status_t status = callback(LoadedSegment::Handle(loaded_segment), data);
    if (status != HSA_STATUS_SUCCESS) {
      return status;
    }
  }

  return HSA_STATUS_SUCCESS;
}

void LoadedCodeObjectImpl::Print(std::ostream& out)
{
  out << "Code Object" << std::endl;
}

bool Segment::GetInfo(amd_loaded_segment_info_t attribute, void *value)
{
  assert(value);

  switch (attribute) {
    case AMD_LOADED_SEGMENT_INFO_TYPE: {
      *((amdgpu_hsa_elf_segment_t*)value) = segment;
      break;
    }
    case AMD_LOADED_SEGMENT_INFO_ELF_BASE_ADDRESS: {
      *((uint64_t*)value) = vaddr;
      break;
    }
    case AMD_LOADED_SEGMENT_INFO_LOAD_BASE_ADDRESS: {
      *((uint64_t*)value) = reinterpret_cast<uint64_t>(this->Address(this->VAddr()));
      break;
    }
    case AMD_LOADED_SEGMENT_INFO_SIZE: {
      *((size_t*)value) = size;
      break;
    }
    default: {
      return false;
    }
  }

  return true;
}

uint64_t Segment::Offset(uint64_t addr)
{
  assert(IsAddressInSegment(addr));
  return addr - vaddr;
}

void* Segment::Address(uint64_t addr)
{
  return owner->context()->SegmentAddress(segment, agent, ptr, Offset(addr));
}

bool Segment::Freeze()
{
  return !frozen ? (frozen = owner->context()->SegmentFreeze(segment, agent, ptr, size)) : true;
}

bool Segment::IsAddressInSegment(uint64_t addr)
{
  return vaddr <= addr && addr < vaddr + size;
}

void Segment::Copy(uint64_t addr, const void* src, size_t size)
{
  // loader must do copies before freezing.
  assert(!frozen);

  if (size > 0) {
    owner->context()->SegmentCopy(segment, agent, ptr, Offset(addr), src, size);
  }
}

void Segment::Print(std::ostream& out)
{
  out << "Segment" << std::endl
    << "    Type: " << AmdHsaElfSegmentToString(segment)
    << "    Size: " << size
    << "    VAddr: " << vaddr << std::endl
    << "    Ptr: " << std::hex << ptr << std::dec
    << std::endl;
}

void Segment::Destroy()
{
  owner->context()->SegmentFree(segment, agent, ptr, size);
}

//===----------------------------------------------------------------------===//
// ExecutableImpl.                                                                //
//===----------------------------------------------------------------------===//

ExecutableImpl::ExecutableImpl(const hsa_profile_t &_profile, Context *context, size_t id)
  : Executable()
  , profile_(_profile)
  , context_(context)
  , id_(id)
  , state_(HSA_EXECUTABLE_STATE_UNFROZEN)
  , program_allocation_segment(nullptr)
{
}

ExecutableImpl::~ExecutableImpl() {
  for (ExecutableObject* o : objects) {
    o->Destroy();
    delete o;
  }
  objects.clear();

  for (auto &symbol_entry : program_symbols_) {
    delete symbol_entry.second;
  }
  for (auto &symbol_entry : agent_symbols_) {
    delete symbol_entry.second;
  }
}

hsa_status_t ExecutableImpl::DefineProgramExternalVariable(
  const char *name, void *address)
{
  WriterLockGuard<ReaderWriterLock> writer_lock(rw_lock_);
  assert(name);
  assert(address);

  if (HSA_EXECUTABLE_STATE_FROZEN == state_) {
    return HSA_STATUS_ERROR_FROZEN_EXECUTABLE;
  }

  auto symbol_entry = program_symbols_.find(std::string(name));
  if (symbol_entry != program_symbols_.end()) {
    return HSA_STATUS_ERROR_VARIABLE_ALREADY_DEFINED;
  }

  program_symbols_.insert(
    std::make_pair(std::string(name),
                   new VariableSymbol(true,
                                      std::string(name),
                                      HSA_SYMBOL_LINKAGE_PROGRAM,
                                      true,
                                      HSA_VARIABLE_ALLOCATION_PROGRAM,
                                      HSA_VARIABLE_SEGMENT_GLOBAL,
                                      0,     // TODO: size.
                                      0,     // TODO: align.
                                      false, // TODO: const.
                                      true,
                                      reinterpret_cast<uint64_t>(address))));
  return HSA_STATUS_SUCCESS;
}

hsa_status_t ExecutableImpl::DefineAgentExternalVariable(
  const char *name,
  hsa_agent_t agent,
  hsa_variable_segment_t segment,
  void *address)
{
  WriterLockGuard<ReaderWriterLock> writer_lock(rw_lock_);
  assert(name);
  assert(address);

  if (HSA_EXECUTABLE_STATE_FROZEN == state_) {
    return HSA_STATUS_ERROR_FROZEN_EXECUTABLE;
  }

  auto symbol_entry = agent_symbols_.find(std::make_pair(std::string(name), agent));
  if (symbol_entry != agent_symbols_.end()) {
    return HSA_STATUS_ERROR_VARIABLE_ALREADY_DEFINED;
  }

  agent_symbols_.insert(
    std::make_pair(std::make_pair(std::string(name), agent),
                   new VariableSymbol(true,
                                      std::string(name),
                                      HSA_SYMBOL_LINKAGE_PROGRAM,
                                      true,
                                      HSA_VARIABLE_ALLOCATION_AGENT,
                                      segment,
                                      0,     // TODO: size.
                                      0,     // TODO: align.
                                      false, // TODO: const.
                                      true,
                                      reinterpret_cast<uint64_t>(address))));
  return HSA_STATUS_SUCCESS;
}

Symbol* ExecutableImpl::GetSymbol(
  const char *module_name,
  const char *symbol_name,
  hsa_agent_t agent,
  int32_t call_convention)
{
  ReaderLockGuard<ReaderWriterLock> reader_lock(rw_lock_);
  return this->GetSymbolInternal(module_name, symbol_name, agent, call_convention);
}

Symbol* ExecutableImpl::GetSymbolInternal(
  const char *module_name,
  const char *symbol_name,
  hsa_agent_t agent,
  int32_t call_convention)
{
  assert(module_name);
  assert(symbol_name);

  std::string mangled_name = std::string(symbol_name);
  if (mangled_name.empty()) {
    return nullptr;
  }
  if (!std::string(module_name).empty()) {
    mangled_name.insert(0, "::");
    mangled_name.insert(0, std::string(module_name));
  }

  auto program_symbol = program_symbols_.find(mangled_name);
  if (program_symbol != program_symbols_.end()) {
    return program_symbol->second;
  }
  auto agent_symbol = agent_symbols_.find(std::make_pair(mangled_name, agent));
  if (agent_symbol != agent_symbols_.end()) {
    return agent_symbol->second;
  }
  return nullptr;
}

hsa_status_t ExecutableImpl::IterateSymbols(
  iterate_symbols_f callback, void *data)
{
  ReaderLockGuard<ReaderWriterLock> reader_lock(rw_lock_);
  assert(callback);

  for (auto &symbol_entry : program_symbols_) {
    hsa_status_t hsc =
      callback(Executable::Handle(this), Symbol::Handle(symbol_entry.second), data);
    if (HSA_STATUS_SUCCESS != hsc) {
      return hsc;
    }
  }
  for (auto &symbol_entry : agent_symbols_) {
    hsa_status_t hsc =
      callback(Executable::Handle(this), Symbol::Handle(symbol_entry.second), data);
    if (HSA_STATUS_SUCCESS != hsc) {
      return hsc;
    }
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t ExecutableImpl::IterateLoadedCodeObjects(
  hsa_status_t (*callback)(
    amd_loaded_code_object_t loaded_code_object,
    void *data),
  void *data)
{
  ReaderLockGuard<ReaderWriterLock> reader_lock(rw_lock_);
  assert(callback);

  for (auto &loaded_code_object : loaded_code_objects) {
    hsa_status_t status = callback(LoadedCodeObject::Handle(loaded_code_object), data);
    if (status != HSA_STATUS_SUCCESS) {
      return status;
    }
  }

  return HSA_STATUS_SUCCESS;
}

uint64_t ExecutableImpl::FindHostAddress(uint64_t device_address)
{
  for (auto &obj : loaded_code_objects) {
    assert(obj);
    for (auto &seg : obj->LoadedSegments()) {
      assert(seg);
      uint64_t paddr = (uint64_t)(uintptr_t)seg->Address(seg->VAddr());
      if (paddr <= device_address && device_address < paddr + seg->Size()) {
        void *haddr = context_->SegmentHostAddress(
          seg->ElfSegment(), seg->Agent(), seg->Ptr(), device_address - paddr);
        return nullptr == haddr ? 0 : (uint64_t)(uintptr_t)haddr;
      }
    }
  }
  return 0;
}

#define HSAERRCHECK(hsc)                                                       \
  if (hsc != HSA_STATUS_SUCCESS) {                                             \
    assert(false);                                                             \
    return hsc;                                                                \
  }                                                                            \


hsa_status_t ExecutableImpl::GetInfo(
    hsa_executable_info_t executable_info, void *value)
{
  ReaderLockGuard<ReaderWriterLock> reader_lock(rw_lock_);

  assert(value);

  switch (executable_info) {
    case HSA_EXECUTABLE_INFO_PROFILE: {
      *((hsa_profile_t*)value) = profile_;;
      break;
    }
    case HSA_EXECUTABLE_INFO_STATE: {
      *((hsa_executable_state_t*)value) = state_;
      break;
    }
    default: {
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
    }
  }

  return HSA_STATUS_SUCCESS;
}

static uint32_t NextLoaderDumpNum()
{
  static std::atomic_uint_fast32_t dumpN(1);
  return dumpN++;
}

hsa_status_t ExecutableImpl::LoadCodeObject(
  hsa_agent_t agent,
  hsa_code_object_t code_object,
  const char *options,
  amd_loaded_code_object_t *loaded_code_object)
{
  return LoadCodeObject(agent, code_object, 0, options, loaded_code_object);
}

hsa_status_t ExecutableImpl::LoadCodeObject(
  hsa_agent_t agent,
  hsa_code_object_t code_object,
  size_t code_object_size,
  const char *options,
  amd_loaded_code_object_t *loaded_code_object)
{
  WriterLockGuard<ReaderWriterLock> writer_lock(rw_lock_);
  if (HSA_EXECUTABLE_STATE_FROZEN == state_) {
    return HSA_STATUS_ERROR_FROZEN_EXECUTABLE;
  }

  LoaderOptions loaderOptions;
  if (options && !loaderOptions.ParseOptions(options)) {
    return HSA_STATUS_ERROR;
  }

  const char *options_append = getenv("LOADER_OPTIONS_APPEND");
  if (options_append && !loaderOptions.ParseOptions(options_append)) {
    return HSA_STATUS_ERROR;
  }

  code.reset(new code::AmdHsaCode());

  if (!code->InitAsHandle(code_object)) {
    return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
  }

  uint32_t dumpNum = 0;
  if (loaderOptions.DumpAll()->is_set() ||
      loaderOptions.DumpExec()->is_set() ||
      loaderOptions.DumpCode()->is_set() ||
      loaderOptions.DumpIsa()->is_set()) {
    dumpNum = NextLoaderDumpNum();
  }

  if (loaderOptions.DumpAll()->is_set() || loaderOptions.DumpCode()->is_set()) {
    if (!code->SaveToFile(amd::hsa::DumpFileName(loaderOptions.DumpDir()->value(), LOADER_DUMP_PREFIX, "co", dumpNum))) {
      // Ignore error.
    }
  }
  if (loaderOptions.DumpAll()->is_set() || loaderOptions.DumpIsa()->is_set()) {
    if (!code->PrintToFile(amd::hsa::DumpFileName(loaderOptions.DumpDir()->value(), LOADER_DUMP_PREFIX, "isa", dumpNum))) {
      // Ignore error.
    }
  }

  std::string codeIsa;
  if (!code->GetNoteIsa(codeIsa)) { return HSA_STATUS_ERROR_INVALID_CODE_OBJECT; }

  hsa_isa_t objectsIsa = context_->IsaFromName(codeIsa.c_str());
  if (!objectsIsa.handle) { return HSA_STATUS_ERROR_INVALID_ISA_NAME; }

  if (!context_->IsaSupportedByAgent(agent, objectsIsa)) { return HSA_STATUS_ERROR_INCOMPATIBLE_ARGUMENTS; }

  uint32_t majorVersion, minorVersion;
  if (!code->GetNoteCodeObjectVersion(&majorVersion, &minorVersion)) {
    return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
  }

  if (majorVersion != 1 && majorVersion != 2) { return HSA_STATUS_ERROR_INVALID_CODE_OBJECT; }

  uint32_t codeHsailMajor;
  uint32_t codeHsailMinor;
  hsa_profile_t codeProfile;
  hsa_machine_model_t codeMachineModel;
  hsa_default_float_rounding_mode_t codeRoundingMode;
  if (!code->GetNoteHsail(&codeHsailMajor, &codeHsailMinor, &codeProfile, &codeMachineModel, &codeRoundingMode)) {
    codeProfile = HSA_PROFILE_FULL;
  }
  if (profile_ != codeProfile) {
    return HSA_STATUS_ERROR_INCOMPATIBLE_ARGUMENTS;
  }

  hsa_status_t status;

  objects.push_back(new LoadedCodeObjectImpl(this, agent, code->ElfData(), code->ElfSize()));
  loaded_code_objects.push_back((LoadedCodeObjectImpl*)objects.back());

  for (size_t i = 0; i < code->DataSegmentCount(); ++i) {
    status = LoadSegment(agent, code->DataSegment(i), majorVersion, code->Machine());
    if (status != HSA_STATUS_SUCCESS) { return status; }
  }

  for (size_t i = 0; i < code->SymbolCount(); ++i) {
    status = LoadSymbol(agent, code->GetSymbol(i));
    if (status != HSA_STATUS_SUCCESS) { return status; }
  }

  status = ApplyRelocations(agent, code.get());
  if (status != HSA_STATUS_SUCCESS) { return status; }

  code.reset();

  if (loaderOptions.DumpAll()->is_set() || loaderOptions.DumpExec()->is_set()) {
    if (!PrintToFile(amd::hsa::DumpFileName(loaderOptions.DumpDir()->value(), LOADER_DUMP_PREFIX, "exec", dumpNum))) {
      // Ignore error.
    }
  }

  if (nullptr != loaded_code_object) { *loaded_code_object = LoadedCodeObject::Handle(loaded_code_objects.back()); }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t ExecutableImpl::LoadSegment(hsa_agent_t agent, code::Segment* s,
                                         uint32_t majorVersion, uint16_t machine)
{
  if (majorVersion >= 2)
    return LoadSegmentV2(agent, s, machine);
  else
    return LoadSegmentV1(agent, s);

}

hsa_status_t ExecutableImpl::LoadSegmentV1(hsa_agent_t agent, code::Segment* s)
{
  assert(s->type() < PT_LOOS + AMDGPU_HSA_SEGMENT_LAST);
  if (s->memSize() == 0)
    return HSA_STATUS_SUCCESS;
  amdgpu_hsa_elf_segment_t segment = (amdgpu_hsa_elf_segment_t)(s->type() - PT_LOOS);
  Segment *new_seg = nullptr;
  bool need_alloc = true;
  if (segment == AMDGPU_HSA_SEGMENT_GLOBAL_PROGRAM && nullptr != program_allocation_segment) {
    new_seg = program_allocation_segment;
    need_alloc = false;
  }
  if (need_alloc) {
    void* ptr = context_->SegmentAlloc(segment, agent, s->memSize(), s->align(), true);
    if (!ptr) { return HSA_STATUS_ERROR_OUT_OF_RESOURCES; }
    new_seg = new Segment(this, agent, segment, ptr, s->memSize(), s->vaddr());
    new_seg->Copy(s->vaddr(), s->data(), s->imageSize());
    objects.push_back(new_seg);

    if (segment == AMDGPU_HSA_SEGMENT_GLOBAL_PROGRAM) {
      program_allocation_segment = new_seg;
    }
  }
  assert(new_seg);
  loaded_code_objects.back()->LoadedSegments().push_back(new_seg);
  return HSA_STATUS_SUCCESS;
}

hsa_status_t ExecutableImpl::LoadSymbol(hsa_agent_t agent, code::Symbol* sym)
{
  if (sym->IsDeclaration()) {
    return LoadDeclarationSymbol(agent, sym);
  } else {
    return LoadDefinitionSymbol(agent, sym);
  }
}

hsa_status_t ExecutableImpl::LoadDefinitionSymbol(hsa_agent_t agent, code::Symbol* sym)
{
  if (sym->IsAgent()) {
    auto agent_symbol = agent_symbols_.find(std::make_pair(sym->Name(), agent));
    if (agent_symbol != agent_symbols_.end()) {
      // TODO(spec): this is not spec compliant.
      return HSA_STATUS_ERROR_VARIABLE_ALREADY_DEFINED;
    }
  } else {
    auto program_symbol = program_symbols_.find(sym->Name());
    if (program_symbol != program_symbols_.end()) {
      // TODO(spec): this is not spec compliant.
      return HSA_STATUS_ERROR_VARIABLE_ALREADY_DEFINED;
    }
  }

  uint64_t address = SymbolAddress(agent, sym);
  if (!address) { return HSA_STATUS_ERROR_INVALID_CODE_OBJECT; }

  SymbolImpl *symbol = nullptr;
  if (sym->IsVariableSymbol()) {
    symbol = new VariableSymbol(true,
                       sym->Name(),
                       sym->Linkage(),
                       true, // sym->IsDefinition()
                       sym->Allocation(),
                       sym->Segment(),
                       sym->Size(),
                       sym->Alignment(),
                       sym->IsConst(),
                       false,
                       address);
  } else if (sym->IsKernelSymbol()) {
      amd_kernel_code_t akc;
      sym->GetSection()->getData(sym->SectionOffset(), &akc, sizeof(akc));

      uint32_t kernarg_segment_size =
        uint32_t(akc.kernarg_segment_byte_size);
      uint32_t kernarg_segment_alignment =
        uint32_t(1 << akc.kernarg_segment_alignment);
      uint32_t group_segment_size =
        uint32_t(akc.workgroup_group_segment_byte_size);
      uint32_t private_segment_size =
        uint32_t(akc.workitem_private_segment_byte_size);
      bool is_dynamic_callstack =
        AMD_HSA_BITS_GET(akc.kernel_code_properties, AMD_KERNEL_CODE_PROPERTIES_IS_DYNAMIC_CALLSTACK) ? true : false;

      uint64_t size = sym->Size();

      if (!size && sym->SectionOffset() < sym->GetSection()->size()) {
        // ORCA Runtime relies on symbol size equal to size of kernel ISA. If symbol size is 0 in ELF,
        // calculate end of segment - symbol value.
        size = sym->GetSection()->size() - sym->SectionOffset();
      }
      KernelSymbol *kernel_symbol = new KernelSymbol(true,
                                      sym->Name(),
                                      sym->Linkage(),
                                      true, // sym->IsDefinition()
                                      kernarg_segment_size,
                                      kernarg_segment_alignment,
                                      group_segment_size,
                                      private_segment_size,
                                      is_dynamic_callstack,
                                      size,
                                      256,
                                      address);
      kernel_symbol->debug_info.elf_raw = code->ElfData();
      kernel_symbol->debug_info.elf_size = code->ElfSize();
      kernel_symbol->debug_info.kernel_name = kernel_symbol->name.c_str();
      kernel_symbol->debug_info.owning_segment = (void*)SymbolSegment(agent, sym)->Address(sym->GetSection()->addr());
      symbol = kernel_symbol;

      // \todo kzhuravl 10/15/15 This is a debugger backdoor: needs to be
      // removed.
      uint64_t target_address = sym->GetSection()->addr() + sym->SectionOffset() + ((size_t)(&((amd_kernel_code_t*)0)->runtime_loader_kernel_symbol));
      uint64_t source_value = (uint64_t) (uintptr_t) &kernel_symbol->debug_info;
      SymbolSegment(agent, sym)->Copy(target_address, &source_value, sizeof(source_value));
  } else {
    assert(!"Unexpected symbol type in LoadDefinitionSymbol");
    return HSA_STATUS_ERROR;
  }
  assert(symbol);
  if (sym->IsAgent()) {
    agent_symbols_.insert(std::make_pair(std::make_pair(sym->Name(), agent), symbol));
  } else {
    program_symbols_.insert(std::make_pair(sym->Name(), symbol));
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t ExecutableImpl::LoadDeclarationSymbol(hsa_agent_t agent, code::Symbol* sym)
{
  auto program_symbol = program_symbols_.find(sym->Name());
  if (program_symbol == program_symbols_.end()) {
    auto agent_symbol = agent_symbols_.find(std::make_pair(sym->Name(), agent));
    if (agent_symbol == agent_symbols_.end()) {
      // TODO(spec): this is not spec compliant.
      return HSA_STATUS_ERROR_VARIABLE_UNDEFINED;
    }
  }
  return HSA_STATUS_SUCCESS;
}

Segment* ExecutableImpl::VirtualAddressSegment(uint64_t vaddr)
{
  for (auto &seg : loaded_code_objects.back()->LoadedSegments()) {
    if (seg->IsAddressInSegment(vaddr)) {
      return seg;
    }
  }
  return 0;
}

uint64_t ExecutableImpl::SymbolAddress(hsa_agent_t agent, code::Symbol* sym)
{
  code::Section* sec = sym->GetSection();
  Segment* seg = SectionSegment(agent, sec);
  return nullptr == seg ? 0 : (uint64_t) (uintptr_t) seg->Address(sym->VAddr());
}

uint64_t ExecutableImpl::SymbolAddress(hsa_agent_t agent, elf::Symbol* sym)
{
  elf::Section* sec = sym->section();
  Segment* seg = SectionSegment(agent, sec);
  uint64_t vaddr = sec->addr() + sym->value();
  return nullptr == seg ? 0 : (uint64_t) (uintptr_t) seg->Address(vaddr);
}

Segment* ExecutableImpl::SymbolSegment(hsa_agent_t agent, code::Symbol* sym)
{
  return SectionSegment(agent, sym->GetSection());
}

Segment* ExecutableImpl::SectionSegment(hsa_agent_t agent, code::Section* sec)
{
  for (Segment* seg : loaded_code_objects.back()->LoadedSegments()) {
    if (seg->IsAddressInSegment(sec->addr())) {
      return seg;
    }
  }
  return 0;
}

hsa_status_t ExecutableImpl::ApplyRelocations(hsa_agent_t agent, amd::hsa::code::AmdHsaCode *c)
{
  hsa_status_t status = HSA_STATUS_SUCCESS;
  for (size_t i = 0; i < c->RelocationSectionCount(); ++i) {
    if (c->GetRelocationSection(i)->targetSection()) {
      status = ApplyStaticRelocationSection(agent, c->GetRelocationSection(i));
    } else {
      // Dynamic relocations are supported starting code object v2.1.
      uint32_t majorVersion, minorVersion;
      if (!c->GetNoteCodeObjectVersion(&majorVersion, &minorVersion)) {
        return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
      }
      if (majorVersion < 2) {
        return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
      }
      if (majorVersion == 2 && minorVersion < 1) {
        return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
      }
      status = ApplyDynamicRelocationSection(agent, c->GetRelocationSection(i));
    }
    if (status != HSA_STATUS_SUCCESS) { return status; }
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t ExecutableImpl::ApplyStaticRelocationSection(hsa_agent_t agent, amd::hsa::code::RelocationSection* sec)
{
  // Skip link-time relocations (if any).
  if (!(sec->targetSection()->flags() & SHF_ALLOC)) { return HSA_STATUS_SUCCESS; }
  hsa_status_t status = HSA_STATUS_SUCCESS;
  for (size_t i = 0; i < sec->relocationCount(); ++i) {
    status = ApplyStaticRelocation(agent, sec->relocation(i));
    if (status != HSA_STATUS_SUCCESS) { return status; }
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t ExecutableImpl::ApplyStaticRelocation(hsa_agent_t agent, amd::hsa::code::Relocation *rel)
{
  hsa_status_t status = HSA_STATUS_SUCCESS;
  amd::elf::Symbol* sym = rel->symbol();
  code::RelocationSection* rsec = rel->section();
  code::Section* sec = rsec->targetSection();
  Segment* rseg = SectionSegment(agent, sec);
  size_t reladdr = sec->addr() + rel->offset();
  switch (rel->type()) {
    case R_AMDGPU_32_LOW:
    case R_AMDGPU_32_HIGH:
    case R_AMDGPU_64:
    {
      uint64_t addr;
      switch (sym->type()) {
        case STT_OBJECT:
        case STT_SECTION:
        case STT_AMDGPU_HSA_KERNEL:
        case STT_AMDGPU_HSA_INDIRECT_FUNCTION:
          addr = SymbolAddress(agent, sym);
          if (!addr) { return HSA_STATUS_ERROR_INVALID_CODE_OBJECT; }
          break;
        case STT_COMMON: {
          hsa_agent_t sagent = agent;
          if (STA_AMDGPU_HSA_GLOBAL_PROGRAM == ELF64_ST_AMDGPU_ALLOCATION(sym->other())) {
            sagent.handle = 0;
          }
          SymbolImpl* esym = (SymbolImpl*) GetSymbolInternal("", sym->name().c_str(), sagent, 0);
          if (!esym) { return HSA_STATUS_ERROR_VARIABLE_UNDEFINED; }
          addr = esym->address;
          break;
        }
        default:
          return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
      }
      addr += rel->addend();

      uint32_t addr32 = 0;
      switch (rel->type()) {
        case R_AMDGPU_32_HIGH:
          addr32 = uint32_t((addr >> 32) & 0xFFFFFFFF);
          rseg->Copy(reladdr, &addr32, sizeof(addr32));
          break;
        case R_AMDGPU_32_LOW:
          addr32 = uint32_t(addr & 0xFFFFFFFF);
          rseg->Copy(reladdr, &addr32, sizeof(addr32));
          break;
        case R_AMDGPU_64:
          rseg->Copy(reladdr, &addr, sizeof(addr));
          break;
        default:
          return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
      }
      break;
    }

    case R_AMDGPU_INIT_SAMPLER:
    {
      if (STT_AMDGPU_HSA_METADATA != sym->type() ||
          SHT_PROGBITS != sym->section()->type() ||
          !(sym->section()->flags() & SHF_MERGE)) {
        return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
      }
      amdgpu_hsa_sampler_descriptor_t desc;
      if (!sym->section()->getData(sym->value(), &desc, sizeof(desc))) {
        return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
      }
      if (AMDGPU_HSA_METADATA_KIND_INIT_SAMP != desc.kind) {
        return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
      }

      hsa_ext_sampler_descriptor_t hsa_sampler_descriptor;
      hsa_sampler_descriptor.coordinate_mode =
        hsa_ext_sampler_coordinate_mode_t(desc.coord);
      hsa_sampler_descriptor.filter_mode =
        hsa_ext_sampler_filter_mode_t(desc.filter);
      hsa_sampler_descriptor.address_mode =
        hsa_ext_sampler_addressing_mode_t(desc.addressing);

      hsa_ext_sampler_t hsa_sampler = {0};
      status = context_->SamplerCreate(agent, &hsa_sampler_descriptor, &hsa_sampler);
      if (status != HSA_STATUS_SUCCESS) { return status; }
      assert(hsa_sampler.handle);
      rseg->Copy(reladdr, &hsa_sampler, sizeof(hsa_sampler));
      break;
    }

    case R_AMDGPU_INIT_IMAGE:
    {
      if (STT_AMDGPU_HSA_METADATA != sym->type() ||
          SHT_PROGBITS != sym->section()->type() ||
          !(sym->section()->flags() & SHF_MERGE)) {
        return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
      }

      amdgpu_hsa_image_descriptor_t desc;
      if (!sym->section()->getData(sym->value(), &desc, sizeof(desc))) {
        return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
      }
      if (AMDGPU_HSA_METADATA_KIND_INIT_ROIMG != desc.kind &&
          AMDGPU_HSA_METADATA_KIND_INIT_WOIMG != desc.kind &&
          AMDGPU_HSA_METADATA_KIND_INIT_RWIMG != desc.kind) {
        return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
      }

      hsa_ext_image_format_t hsa_image_format;
      hsa_image_format.channel_order =
        hsa_ext_image_channel_order_t(desc.channel_order);
      hsa_image_format.channel_type =
        hsa_ext_image_channel_type_t(desc.channel_type);

      hsa_ext_image_descriptor_t hsa_image_descriptor;
      hsa_image_descriptor.geometry =
        hsa_ext_image_geometry_t(desc.geometry);
      hsa_image_descriptor.width = size_t(desc.width);
      hsa_image_descriptor.height = size_t(desc.height);
      hsa_image_descriptor.depth = size_t(desc.depth);
      hsa_image_descriptor.array_size = size_t(desc.array);
      hsa_image_descriptor.format = hsa_image_format;

      hsa_access_permission_t hsa_image_permission = HSA_ACCESS_PERMISSION_RO;
      switch (desc.kind) {
        case AMDGPU_HSA_METADATA_KIND_INIT_ROIMG: {
          hsa_image_permission = HSA_ACCESS_PERMISSION_RO;
          break;
        }
        case AMDGPU_HSA_METADATA_KIND_INIT_WOIMG: {
          hsa_image_permission = HSA_ACCESS_PERMISSION_WO;
          break;
        }
        case AMDGPU_HSA_METADATA_KIND_INIT_RWIMG: {
          hsa_image_permission = HSA_ACCESS_PERMISSION_RW;
          break;
        }
        default: {
          assert(false);
          return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
        }
      }

      hsa_ext_image_t hsa_image = {0};
      status = context_->ImageCreate(agent, hsa_image_permission,
                                  &hsa_image_descriptor,
                                  NULL, // TODO: image_data?
                                  &hsa_image);
      if (status != HSA_STATUS_SUCCESS) { return status; }
      rseg->Copy(reladdr, &hsa_image, sizeof(hsa_image));
      break;
    }

    default:
      // Ignore.
      break;
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t ExecutableImpl::ApplyDynamicRelocationSection(hsa_agent_t agent, amd::hsa::code::RelocationSection* sec)
{
  hsa_status_t status = HSA_STATUS_SUCCESS;
  for (size_t i = 0; i < sec->relocationCount(); ++i) {
    status = ApplyDynamicRelocation(agent, sec->relocation(i));
    if (status != HSA_STATUS_SUCCESS) { return status; }
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t ExecutableImpl::ApplyDynamicRelocation(hsa_agent_t agent, amd::hsa::code::Relocation *rel)
{
  Segment* relSeg = VirtualAddressSegment(rel->offset());
  uint64_t symAddr = 0;
  switch (rel->symbol()->type()) {
    case STT_OBJECT:
    case STT_AMDGPU_HSA_KERNEL:
    {
      Segment* symSeg = VirtualAddressSegment(rel->symbol()->value());
      symAddr = reinterpret_cast<uint64_t>(symSeg->Address(rel->symbol()->value()));
      break;
    }

    // External symbols, they must be defined prior loading.
    case STT_NOTYPE:
    {
      // TODO: Only agent allocation variables are supported in v2.1. How will
      // we distinguish between program allocation and agent allocation
      // variables?
      auto agent_symbol = agent_symbols_.find(std::make_pair(rel->symbol()->name(), agent));
      if (agent_symbol == agent_symbols_.end()) {
        // External symbols must be defined prior loading.
        return HSA_STATUS_ERROR_VARIABLE_UNDEFINED;
      }
      symAddr = agent_symbol->second->address;
      break;
    }

    default:
      // Only objects and kernels are supported in v2.1.
      return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
  }
  symAddr += rel->addend();

  switch (rel->type()) {
    case R_AMDGPU_32_HIGH:
    {
      uint32_t symAddr32 = uint32_t((symAddr >> 32) & 0xFFFFFFFF);
      relSeg->Copy(rel->offset(), &symAddr32, sizeof(symAddr32));
      break;
    }

    case R_AMDGPU_32_LOW:
    {
      uint32_t symAddr32 = uint32_t(symAddr & 0xFFFFFFFF);
      relSeg->Copy(rel->offset(), &symAddr32, sizeof(symAddr32));
      break;
    }

    case R_AMDGPU_64:
    {
      relSeg->Copy(rel->offset(), &symAddr, sizeof(symAddr));
      break;
    }

    case R_AMDGPU_INIT_IMAGE:
    case R_AMDGPU_INIT_SAMPLER:
      // Images and samplers are not supported in v2.1.
      return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;

    default:
      // Ignore.
      break;
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t ExecutableImpl::Freeze(const char *options) {
  amd::hsa::common::WriterLockGuard<amd::hsa::common::ReaderWriterLock> writer_lock(rw_lock_);
  if (HSA_EXECUTABLE_STATE_FROZEN == state_) {
    return HSA_STATUS_ERROR_FROZEN_EXECUTABLE;
  }

  for (auto &lco : loaded_code_objects) {
    for (auto &ls : lco->LoadedSegments()) {
      ls->Freeze();
    }
  }

  state_ = HSA_EXECUTABLE_STATE_FROZEN;
  return HSA_STATUS_SUCCESS;
}

hsa_status_t ExecutableImpl::LoadSegmentV2(hsa_agent_t agent, code::Segment* s, uint16_t machine)
{
  amdgpu_hsa_elf_segment_t segment;

  if (s->memSize() == 0)
    return HSA_STATUS_SUCCESS;

  // FIXME: Should support EM_HSA_VENDOR
  if (machine == EM_AMDGPU) {
    if (s->flags() & PF_X)
      segment = AMDGPU_HSA_SEGMENT_CODE_AGENT;
    else if (s->flags() & PF_W)
      segment = AMDGPU_HSA_SEGMENT_GLOBAL_AGENT;
    else {
      assert (s->flags() & PF_R);
      segment = AMDGPU_HSA_SEGMENT_READONLY_AGENT;
    }
  } else { // EM_HSA_SHARED
    segment = AMDGPU_HSA_SEGMENT_GLOBAL_PROGRAM;
  }

  void* ptr = context_->SegmentAlloc(segment, agent, s->memSize(), s->align(), true);
  if (!ptr) { return HSA_STATUS_ERROR_OUT_OF_RESOURCES; }

  Segment *new_seg = new Segment(this, agent, segment, ptr, s->memSize(), s->vaddr());
  new_seg->Copy(s->vaddr(), s->data(), s->imageSize());
  objects.push_back(new_seg);
  assert(new_seg);

  loaded_code_objects.back()->LoadedSegments().push_back(new_seg);
  return HSA_STATUS_SUCCESS;
}

void ExecutableImpl::Print(std::ostream& out)
{
  out << "AMD Executable" << std::endl;
  out << "  Id: " << id()
      << "  Profile: " << HsaProfileToString(profile())
      << std::endl << std::endl;
  out << "Loaded Objects (total " << objects.size() << ")" << std::endl;
  size_t i = 0;
  for (ExecutableObject* o : objects) {
    out << "Loaded Object " << i++ << ": ";
    o->Print(out);
    out << std::endl;
  }
  out << "End AMD Executable" << std::endl;
}

bool ExecutableImpl::PrintToFile(const std::string& filename)
{
  std::ofstream out(filename);
  if (out.fail()) { return false; }
  Print(out);
  return out.fail();
}

} // namespace loader
} // namespace hsa
} // namespace amd
