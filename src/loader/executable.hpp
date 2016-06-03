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

#ifndef HSA_RUNTIME_CORE_LOADER_EXECUTABLE_HPP_
#define HSA_RUNTIME_CORE_LOADER_EXECUTABLE_HPP_

#include <array>
#include <cassert>
#include <cstdint>
#include <libelf.h>
#include <list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "hsa.h"
#include "hsa_ext_image.h"
#include "amd_hsa_loader.hpp"
#include "amd_hsa_code.hpp"
#include "amd_hsa_kernel_code.h"
#include "amd_hsa_locks.hpp"

namespace amd {
namespace hsa {
namespace loader {

class MemoryAddress;
class SymbolImpl;
class KernelSymbol;
class VariableSymbol;
class ExecutableImpl;

//===----------------------------------------------------------------------===//
// SymbolImpl.                                                                //
//===----------------------------------------------------------------------===//

typedef uint32_t symbol_attribute32_t;

class SymbolImpl: public Symbol {
public:
  virtual ~SymbolImpl() {}

  bool IsKernel() const {
    return HSA_SYMBOL_KIND_KERNEL == kind;
  }
  bool IsVariable() const {
    return HSA_SYMBOL_KIND_VARIABLE == kind;
  }

  bool is_loaded;
  hsa_symbol_kind_t kind;
  std::string name;
  hsa_symbol_linkage_t linkage;
  bool is_definition;
  uint64_t address;
  hsa_agent_t agent;

protected:
  SymbolImpl(const bool &_is_loaded,
             const hsa_symbol_kind_t &_kind,
             const std::string &_name,
             const hsa_symbol_linkage_t &_linkage,
             const bool &_is_definition,
             const uint64_t &_address = 0)
    : is_loaded(_is_loaded)
    , kind(_kind)
    , name(_name)
    , linkage(_linkage)
    , is_definition(_is_definition)
    , address(_address) {}

  virtual bool GetInfo(hsa_symbol_info32_t symbol_info, void *value);

private:
  SymbolImpl(const SymbolImpl &s);
  SymbolImpl& operator=(const SymbolImpl &s);
};

//===----------------------------------------------------------------------===//
// KernelSymbol.                                                              //
//===----------------------------------------------------------------------===//

class KernelSymbol final: public SymbolImpl {
public:
  KernelSymbol(const bool &_is_loaded,
               const std::string &_name,
               const hsa_symbol_linkage_t &_linkage,
               const bool &_is_definition,
               const uint32_t &_kernarg_segment_size,
               const uint32_t &_kernarg_segment_alignment,
               const uint32_t &_group_segment_size,
               const uint32_t &_private_segment_size,
               const bool &_is_dynamic_callstack,
               const uint32_t &_size,
               const uint32_t &_alignment,
               const uint64_t &_address = 0)
    : SymbolImpl(_is_loaded,
                 HSA_SYMBOL_KIND_KERNEL,
                 _name,
                 _linkage,
                 _is_definition,
                 _address)
    , kernarg_segment_size(_kernarg_segment_size)
    , kernarg_segment_alignment(_kernarg_segment_alignment)
    , group_segment_size(_group_segment_size)
    , private_segment_size(_private_segment_size)
    , is_dynamic_callstack(_is_dynamic_callstack)
    , size(_size)
    , alignment(_alignment) {}

  ~KernelSymbol() {}

  bool GetInfo(hsa_symbol_info32_t symbol_info, void *value);

  uint32_t kernarg_segment_size;
  uint32_t kernarg_segment_alignment;
  uint32_t group_segment_size;
  uint32_t private_segment_size;
  bool is_dynamic_callstack;
  uint32_t size;
  uint32_t alignment;
  amd_runtime_loader_debug_info_t debug_info;

private:
  KernelSymbol(const KernelSymbol &ks);
  KernelSymbol& operator=(const KernelSymbol &ks);
};

//===----------------------------------------------------------------------===//
// VariableSymbol.                                                            //
//===----------------------------------------------------------------------===//

class VariableSymbol final: public SymbolImpl {
public:
  VariableSymbol(const bool &_is_loaded,
                 const std::string &_name,
                 const hsa_symbol_linkage_t &_linkage,
                 const bool &_is_definition,
                 const hsa_variable_allocation_t &_allocation,
                 const hsa_variable_segment_t &_segment,
                 const uint32_t &_size,
                 const uint32_t &_alignment,
                 const bool &_is_constant,
                 const bool &_is_external = false,
                 const uint64_t &_address = 0)
    : SymbolImpl(_is_loaded,
                 HSA_SYMBOL_KIND_VARIABLE,
                 _name,
                 _linkage,
                 _is_definition,
                 _address)
    , allocation(_allocation)
    , segment(_segment)
    , size(_size)
    , alignment(_alignment)
    , is_constant(_is_constant)
    , is_external(_is_external) {}

  ~VariableSymbol() {}

  bool GetInfo(hsa_symbol_info32_t symbol_info, void *value);

  hsa_variable_allocation_t allocation;
  hsa_variable_segment_t segment;
  uint32_t size;
  uint32_t alignment;
  bool is_constant;
  bool is_external;

private:
  VariableSymbol(const VariableSymbol &vs);
  VariableSymbol& operator=(const VariableSymbol &vs);
};

//===----------------------------------------------------------------------===//
// Executable.                                                                //
//===----------------------------------------------------------------------===//

class ExecutableImpl;
class LoadedCodeObjectImpl;
class Segment;

class ExecutableObject {
protected:
  ExecutableImpl *owner;
  hsa_agent_t agent;

public:
  ExecutableObject(ExecutableImpl *owner_, hsa_agent_t agent_)
    : owner(owner_), agent(agent_) { }

  ExecutableImpl* Owner() const { return owner; }
  hsa_agent_t Agent() const { return agent; }
  virtual void Print(std::ostream& out) = 0;
  virtual void Destroy() = 0;

  virtual ~ExecutableObject() { }
};

class LoadedCodeObjectImpl : public LoadedCodeObject, public ExecutableObject {
private:
  LoadedCodeObjectImpl(const LoadedCodeObjectImpl&);
  LoadedCodeObjectImpl& operator=(const LoadedCodeObjectImpl&);

  const void *elf_data;
  const size_t elf_size;
  std::vector<Segment*> loaded_segments;

public:
  LoadedCodeObjectImpl(ExecutableImpl *owner_, hsa_agent_t agent_, const void *elf_data_, size_t elf_size_)
    : ExecutableObject(owner_, agent_), elf_data(elf_data_), elf_size(elf_size_) {}

  const void* ElfData() const { return elf_data; }
  size_t ElfSize() const { return elf_size; }
  std::vector<Segment*>& LoadedSegments() { return loaded_segments; }

  bool GetInfo(amd_loaded_code_object_info_t attribute, void *value) override;

  hsa_status_t IterateLoadedSegments(
    hsa_status_t (*callback)(
      amd_loaded_segment_t loaded_segment,
      void *data),
    void *data) override;

  void Print(std::ostream& out) override;

  void Destroy() override {}
};

class Segment : public LoadedSegment, public ExecutableObject {
private:
  amdgpu_hsa_elf_segment_t segment;
  void *ptr;
  size_t size;
  uint64_t vaddr;
  bool frozen;

public:
  Segment(ExecutableImpl *owner_, hsa_agent_t agent_, amdgpu_hsa_elf_segment_t segment_, void* ptr_, size_t size_, uint64_t vaddr_)
    : ExecutableObject(owner_, agent_), segment(segment_),
      ptr(ptr_), size(size_), vaddr(vaddr_), frozen(false) { }

  amdgpu_hsa_elf_segment_t ElfSegment() const { return segment; }
  void* Ptr() const { return ptr; }
  size_t Size() const { return size; }
  uint64_t VAddr() const { return vaddr; }

  bool GetInfo(amd_loaded_segment_info_t attribute, void *value) override;

  uint64_t Offset(uint64_t addr); // Offset within segment. Used together with ptr with loader context functions.

  void* Address(uint64_t addr); // Address in segment. Used for relocations and valid on agent.

  bool Freeze();

  bool IsAddressInSegment(uint64_t addr);
  void Copy(uint64_t addr, const void* src, size_t size);
  void Print(std::ostream& out) override;
  void Destroy() override;
};

class Sampler : public ExecutableObject {
private:
  hsa_ext_sampler_t samp;

public:
  Sampler(ExecutableImpl *owner, hsa_agent_t agent, hsa_ext_sampler_t samp_)
    : ExecutableObject(owner, agent), samp(samp_) { }
  void Print(std::ostream& out) override;
  void Destroy() override;
};

class Image : public ExecutableObject {
private:
  hsa_ext_image_t img;

public:
  Image(ExecutableImpl *owner, hsa_agent_t agent, hsa_ext_image_t img_)
    : ExecutableObject(owner, agent), img(img_) { }
  void Print(std::ostream& out) override;
  void Destroy() override;
};

typedef std::string ProgramSymbol;
typedef std::unordered_map<ProgramSymbol, SymbolImpl*> ProgramSymbolMap;

typedef std::pair<std::string, hsa_agent_t> AgentSymbol;
struct ASC {
  bool operator()(const AgentSymbol &las, const AgentSymbol &ras) const {
    return las.first == ras.first && las.second.handle == ras.second.handle;
  }
};
struct ASH {
  size_t operator()(const AgentSymbol &as) const {
    size_t h = std::hash<std::string>()(as.first);
    size_t i = std::hash<uint64_t>()(as.second.handle);
    return h ^ (i << 1);
  }
};
typedef std::unordered_map<AgentSymbol, SymbolImpl*, ASH, ASC> AgentSymbolMap;

class ExecutableImpl final: public Executable {
public:
  const hsa_profile_t& profile() const {
    return profile_;
  }
  const hsa_executable_state_t& state() const {
    return state_;
  }

  ExecutableImpl(const hsa_profile_t &_profile, Context *context, size_t id);

  ~ExecutableImpl();

  hsa_status_t GetInfo(hsa_executable_info_t executable_info, void *value);

  hsa_status_t DefineProgramExternalVariable(
    const char *name, void *address);

  hsa_status_t DefineAgentExternalVariable(
    const char *name,
    hsa_agent_t agent,
    hsa_variable_segment_t segment,
    void *address);

  hsa_status_t LoadCodeObject(
    hsa_agent_t agent,
    hsa_code_object_t code_object,
    const char *options,
    amd_loaded_code_object_t *loaded_code_object);

  hsa_status_t LoadCodeObject(
    hsa_agent_t agent,
    hsa_code_object_t code_object,
    size_t code_object_size,
    const char *options,
    amd_loaded_code_object_t *loaded_code_object);

  hsa_status_t Freeze(const char *options);

  hsa_status_t Validate(uint32_t *result) {
    amd::hsa::common::ReaderLockGuard<amd::hsa::common::ReaderWriterLock> reader_lock(rw_lock_);
    assert(result);
    *result = 0;
    return HSA_STATUS_SUCCESS;
  }

  Symbol* GetSymbol(
    const char *module_name,
    const char *symbol_name,
    hsa_agent_t agent,
    int32_t call_convention);

  hsa_status_t IterateSymbols(
    iterate_symbols_f callback, void *data);

  hsa_status_t IterateLoadedCodeObjects(
    hsa_status_t (*callback)(
      amd_loaded_code_object_t loaded_code_object,
      void *data),
    void *data);

  uint64_t FindHostAddress(uint64_t device_address) override;

  void Print(std::ostream& out) override;
  bool PrintToFile(const std::string& filename) override;

  Context* context() { return context_; }
  size_t id() { return id_; }

private:
  ExecutableImpl(const ExecutableImpl &e);
  ExecutableImpl& operator=(const ExecutableImpl &e);

  std::unique_ptr<amd::hsa::code::AmdHsaCode> code;

  Symbol* GetSymbolInternal(
    const char *module_name,
    const char *symbol_name,
    hsa_agent_t agent,
    int32_t call_convention);

  hsa_status_t LoadSegment(hsa_agent_t agent, code::Segment* s, uint32_t majorVersion, uint16_t machine);
  hsa_status_t LoadSegmentV1(hsa_agent_t agent, amd::hsa::code::Segment* seg);
  hsa_status_t LoadSegmentV2(hsa_agent_t agent, amd::hsa::code::Segment* seg, uint16_t machine);
  hsa_status_t LoadSymbol(hsa_agent_t agent, amd::hsa::code::Symbol* sym);
  hsa_status_t LoadDefinitionSymbol(hsa_agent_t agent, amd::hsa::code::Symbol* sym);
  hsa_status_t LoadDeclarationSymbol(hsa_agent_t agent, amd::hsa::code::Symbol* sym);

  hsa_status_t ApplyRelocations(hsa_agent_t agent, amd::hsa::code::AmdHsaCode *c);
  hsa_status_t ApplyStaticRelocationSection(hsa_agent_t agent, amd::hsa::code::RelocationSection* sec);
  hsa_status_t ApplyStaticRelocation(hsa_agent_t agent, amd::hsa::code::Relocation *rel);
  hsa_status_t ApplyDynamicRelocationSection(hsa_agent_t agent, amd::hsa::code::RelocationSection* sec);
  hsa_status_t ApplyDynamicRelocation(hsa_agent_t agent, amd::hsa::code::Relocation *rel);

  Segment* VirtualAddressSegment(uint64_t vaddr);
  uint64_t SymbolAddress(hsa_agent_t agent, amd::hsa::code::Symbol* sym);
  uint64_t SymbolAddress(hsa_agent_t agent, amd::elf::Symbol* sym);
  Segment* SymbolSegment(hsa_agent_t agent, amd::hsa::code::Symbol* sym);
  Segment* SectionSegment(hsa_agent_t agent, amd::hsa::code::Section* sec);

  amd::hsa::common::ReaderWriterLock rw_lock_;
  hsa_profile_t profile_;
  Context *context_;
  const size_t id_;
  hsa_executable_state_t state_;

  ProgramSymbolMap program_symbols_;
  AgentSymbolMap agent_symbols_;
  std::vector<ExecutableObject*> objects;
  Segment *program_allocation_segment;
  std::vector<LoadedCodeObjectImpl*> loaded_code_objects;
};

class AmdHsaCodeLoader : public Loader {
private:
  Context* context;
  std::vector<Executable*> executables;
  std::mutex executables_mutex;

public:
  AmdHsaCodeLoader(Context* context_)
    : context(context_) { assert(context); }

  Context* GetContext() const { return context; }

  Executable* CreateExecutable(hsa_profile_t profile, const char *options) override;

  void DestroyExecutable(Executable *executable) override;

  hsa_status_t IterateExecutables(
    hsa_status_t (*callback)(
      hsa_executable_t executable,
      void *data),
    void *data) override;

  uint64_t FindHostAddress(uint64_t device_address) override;
};

} // namespace loader
} // namespace hsa
} // namespace amd

#endif // HSA_RUNTIME_CORE_LOADER_EXECUTABLE_HPP_
