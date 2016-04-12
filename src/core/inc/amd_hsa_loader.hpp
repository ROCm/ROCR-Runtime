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

#ifndef AMD_HSA_LOADER_HPP
#define AMD_HSA_LOADER_HPP

#include <cstddef>
#include <cstdint>
#include "hsa.h"
#include "hsa_ext_image.h"
#include "amd_hsa_elf.h"
#include <string>
#include <mutex>
#include <vector>

/// @brief Major version of the AMD HSA Loader. Major versions are not backwards
/// compatible.
#define AMD_HSA_LOADER_VERSION_MAJOR 0

/// @brief Minor version of the AMD HSA Loader. Minor versions are backwards
/// compatible.
#define AMD_HSA_LOADER_VERSION_MINOR 5

/// @brief Descriptive version of the AMD HSA Loader.
#define AMD_HSA_LOADER_VERSION "AMD HSA Loader v0.05 (June 16, 2015)"

enum hsa_ext_symbol_info_t {
  HSA_EXT_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT_SIZE = 100,
  HSA_EXT_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT_ALIGN = 101,
};

typedef uint32_t hsa_symbol_info32_t;
typedef hsa_executable_symbol_t hsa_symbol_t;
typedef hsa_executable_symbol_info_t hsa_symbol_info_t;

/// @brief Loaded code object handle.
typedef struct amd_loaded_code_object_s {
  uint64_t handle;
} amd_loaded_code_object_t;

/// @brief Loaded code object attributes.
enum amd_loaded_code_object_info_t {
  AMD_LOADED_CODE_OBJECT_INFO_ELF_IMAGE = 0,
  AMD_LOADED_CODE_OBJECT_INFO_ELF_IMAGE_SIZE = 1
};

/// @brief Loaded segment handle.
typedef struct amd_loaded_segment_s {
  uint64_t handle;
} amd_loaded_segment_t;

/// @brief Loaded segment attributes.
enum amd_loaded_segment_info_t {
  AMD_LOADED_SEGMENT_INFO_TYPE = 0,
  AMD_LOADED_SEGMENT_INFO_ELF_BASE_ADDRESS = 1,
  AMD_LOADED_SEGMENT_INFO_LOAD_BASE_ADDRESS = 2,
  AMD_LOADED_SEGMENT_INFO_SIZE = 3
};

namespace amd {
namespace hsa {
namespace loader {

//===----------------------------------------------------------------------===//
// Context.                                                                   //
//===----------------------------------------------------------------------===//

class Context {
public:
  virtual ~Context() {}

  virtual hsa_isa_t IsaFromName(const char *name) = 0;

  virtual bool IsaSupportedByAgent(hsa_agent_t agent, hsa_isa_t isa) = 0;

  virtual void* SegmentAlloc(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, size_t size, size_t align, bool zero) = 0;

  virtual bool SegmentCopy(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, void* dst, size_t offset, const void* src, size_t size) = 0;

  virtual void SegmentFree(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, void* seg, size_t size) = 0;

  virtual void* SegmentAddress(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, void* seg, size_t offset) = 0;

  virtual void* SegmentHostAddress(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, void* seg, size_t offset) = 0;

  virtual bool SegmentFreeze(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, void* seg, size_t size) = 0;

  virtual bool ImageExtensionSupported() = 0;

  virtual hsa_status_t ImageCreate(
    hsa_agent_t agent,
    hsa_access_permission_t image_permission,
    const hsa_ext_image_descriptor_t *image_descriptor,
    const void *image_data,
    hsa_ext_image_t *image_handle) = 0;

  virtual hsa_status_t ImageDestroy(
    hsa_agent_t agent, hsa_ext_image_t image_handle) = 0;

  virtual hsa_status_t SamplerCreate(
    hsa_agent_t agent,
    const hsa_ext_sampler_descriptor_t *sampler_descriptor,
    hsa_ext_sampler_t *sampler_handle) = 0;

  virtual hsa_status_t SamplerDestroy(
    hsa_agent_t agent, hsa_ext_sampler_t sampler_handle) = 0;

protected:
  Context() {}

private:
  Context(const Context &c);
  Context& operator=(const Context &c);
};

//===----------------------------------------------------------------------===//
// Symbol.                                                                    //
//===----------------------------------------------------------------------===//

class Symbol {
public:
  static hsa_symbol_t Handle(Symbol *symbol) {
    hsa_symbol_t symbol_handle =
      {reinterpret_cast<uint64_t>(symbol)};
    return symbol_handle;
  }

  static Symbol* Object(hsa_symbol_t symbol_handle) {
    Symbol *symbol =
      reinterpret_cast<Symbol*>(symbol_handle.handle);
    return symbol;
  }

  virtual ~Symbol() {}

  virtual bool GetInfo(hsa_symbol_info32_t symbol_info, void *value) = 0;

protected:
  Symbol() {}

private:
  Symbol(const Symbol &s);
  Symbol& operator=(const Symbol &s);
};

//===----------------------------------------------------------------------===//
// LoadedCodeObject.                                                          //
//===----------------------------------------------------------------------===//

class LoadedCodeObject {
public:
  static amd_loaded_code_object_t Handle(LoadedCodeObject *object) {
    amd_loaded_code_object_t handle =
      {reinterpret_cast<uint64_t>(object)};
    return handle;
  }

  static LoadedCodeObject* Object(amd_loaded_code_object_t handle) {
    LoadedCodeObject *object =
      reinterpret_cast<LoadedCodeObject*>(handle.handle);
    return object;
  }

  virtual ~LoadedCodeObject() {}

  virtual bool GetInfo(amd_loaded_code_object_info_t attribute, void *value) = 0;

  virtual hsa_status_t IterateLoadedSegments(
    hsa_status_t (*callback)(
      amd_loaded_segment_t loaded_segment,
      void *data),
    void *data) = 0;

protected:
  LoadedCodeObject() {}

private:
  LoadedCodeObject(const LoadedCodeObject&);
  LoadedCodeObject& operator=(const LoadedCodeObject&);
};

//===----------------------------------------------------------------------===//
// LoadedSegment.                                                             //
//===----------------------------------------------------------------------===//

class LoadedSegment {
public:
  static amd_loaded_segment_t Handle(LoadedSegment *object) {
    amd_loaded_segment_t handle =
      {reinterpret_cast<uint64_t>(object)};
    return handle;
  }

  static LoadedSegment* Object(amd_loaded_segment_t handle) {
    LoadedSegment *object =
      reinterpret_cast<LoadedSegment*>(handle.handle);
    return object;
  }

  virtual ~LoadedSegment() {}

  virtual bool GetInfo(amd_loaded_segment_info_t attribute, void *value) = 0;

protected:
  LoadedSegment() {}

private:
  LoadedSegment(const LoadedSegment&);
  LoadedSegment& operator=(const LoadedSegment&);
};

//===----------------------------------------------------------------------===//
// Executable.                                                                //
//===----------------------------------------------------------------------===//

class Executable {
public:
  static hsa_executable_t Handle(Executable *executable) {
    hsa_executable_t executable_handle =
      {reinterpret_cast<uint64_t>(executable)};
    return executable_handle;
  }

  static Executable* Object(hsa_executable_t executable_handle) {
    Executable *executable =
      reinterpret_cast<Executable*>(executable_handle.handle);
    return executable;
  }

  virtual ~Executable() {}

  virtual hsa_status_t GetInfo(
    hsa_executable_info_t executable_info, void *value) = 0;

  virtual hsa_status_t DefineProgramExternalVariable(
    const char *name, void *address) = 0;

  virtual hsa_status_t DefineAgentExternalVariable(
    const char *name,
    hsa_agent_t agent,
    hsa_variable_segment_t segment,
    void *address) = 0;

  virtual hsa_status_t LoadCodeObject(
    hsa_agent_t agent,
    hsa_code_object_t code_object,
    const char *options,
    amd_loaded_code_object_t *loaded_code_object = nullptr) = 0;

  virtual hsa_status_t LoadCodeObject(
    hsa_agent_t agent,
    hsa_code_object_t code_object,
    size_t code_object_size,
    const char *options,
    amd_loaded_code_object_t *loaded_code_object = nullptr) = 0;

  virtual hsa_status_t Freeze(const char *options) = 0;

  virtual hsa_status_t Validate(uint32_t *result) = 0;

  virtual Symbol* GetSymbol(
    const char *module_name,
    const char *symbol_name,
    hsa_agent_t agent,
    int32_t call_convention) = 0;

  typedef hsa_status_t (*iterate_symbols_f)(
    hsa_executable_t executable,
    hsa_symbol_t symbol_handle,
    void *data);

  virtual hsa_status_t IterateSymbols(
    iterate_symbols_f callback, void *data) = 0;

  virtual hsa_status_t IterateLoadedCodeObjects(
    hsa_status_t (*callback)(
      amd_loaded_code_object_t loaded_code_object,
      void *data),
    void *data) = 0;

  virtual uint64_t FindHostAddress(uint64_t device_address) = 0;

  virtual void Print(std::ostream& out) = 0;
  virtual bool PrintToFile(const std::string& filename) = 0;

protected:
  Executable() {}

private:
  Executable(const Executable &e);
  Executable& operator=(const Executable &e);

  static std::vector<Executable*> executables;
  static std::mutex executables_mutex;
};

/// @class Loader
class Loader {
public:
  /// @brief Destructor.
  virtual ~Loader() {}

  /// @brief Creates AMD HSA Loader with specified @p context.
  ///
  /// @param[in] context Context. Must not be null.
  ///
  /// @returns AMD HSA Loader on success, null on failure.
  static Loader* Create(Context* context);

  /// @brief Destroys AMD HSA Loader @p Loader_object.
  ///
  /// @param[in] loader AMD HSA Loader to destroy. Must not be null.
  static void Destroy(Loader *loader);

  /// @returns Context associated with Loader.
  virtual Context* GetContext() const = 0;

  /// @brief Creates empty AMD HSA Executable with specified @p profile,
  /// @p options
  virtual Executable* CreateExecutable(hsa_profile_t profile, const char *options) = 0;

  /// @brief Destroys @p executable
  virtual void DestroyExecutable(Executable *executable) = 0;

  /// @brief Invokes @p callback for each created executable
  virtual hsa_status_t IterateExecutables(
    hsa_status_t (*callback)(
      hsa_executable_t executable,
      void *data),
    void *data) = 0;

  /// @brief Returns host address given @p device_address. If @p device_address
  /// is already host address, returns null pointer. If @p device_address is
  /// invalid address, returns null pointer.
  virtual uint64_t FindHostAddress(uint64_t device_address) = 0;

protected:
  /// @brief Default constructor.
  Loader() {}

private:
  /// @brief Copy constructor - not available.
  Loader(const Loader&);

  /// @brief Assignment operator - not available.
  Loader& operator=(const Loader&);
};


} // namespace loader
} // namespace hsa
} // namespace amd

#endif // AMD_HSA_LOADER_HPP
