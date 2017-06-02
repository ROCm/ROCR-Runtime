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

#include "core/inc/amd_debugger.h"
#include "core/inc/amd_loader_context.hpp"
#include "core/inc/amd_aql_queue.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace amd {

void Debugger::HandleFault(const HsaMemoryAccessFault& fault, GpuAgentInt* agent) {
  std::stringstream err;

  uint64_t fault_page_idx = fault.VirtualAddress >> 0xC;
  err << "\nMemory access fault by GPU node " << agent->node_id();
  err << " for address 0x" << std::hex << std::uppercase << fault_page_idx << "xxx (";

  if (fault.Failure.NotPresent == 1) {
    err << "page not present";
  } else if (fault.Failure.ReadOnly == 1) {
    err << "write access to a read-only page";
  } else if (fault.Failure.NoExecute == 1) {
    err << "execute access to a non-executable page";
  } else if (fault.Failure.ECC == 1) {
    err << "uncorrectable ECC failure";
  } else {
    err << "unknown reason";
  }

  err << ")\n\n";

  if (core::Runtime::runtime_singleton_->flag().debug_fault() != Flag::DEBUG_FAULT_ANALYZE) {
    if (agent->isa()->GetMajorVersion() >= 9) {
      err << "For more detail set: HSA_DEBUG_FAULT=\"analyze\"\n\n";
    }

    std::cerr << err.str();
    std::abort();
  }

  WaveStates wave_states = agent->GetWaveStates();

  for (WaveState& wave_state : wave_states) {
#define SQ_WAVE_TRAPSTS_XNACK_ERROR(x) (((x) >> 0x1C) & 0x1)

    if (SQ_WAVE_TRAPSTS_XNACK_ERROR(wave_state.regs.trapsts)) {
      err << "Wavefront found in XNACK error state:\n\n";
      err << "     PC: 0x" << std::setw(0x10) << std::setfill('0') << wave_state.regs.pc << "\n";
      err << "   EXEC: 0x" << std::setw(0x10) << std::setfill('0') << wave_state.regs.exec << "\n";
      err << " STATUS: 0x" << std::setw(0x8) << std::setfill('0') << wave_state.regs.status << "\n";
      err << "TRAPSTS: 0x" << std::setw(0x8) << std::setfill('0') << wave_state.regs.trapsts
          << "\n";
      err << "     M0: 0x" << std::setw(0x8) << std::setfill('0') << wave_state.regs.m0 << "\n\n";

      uint32_t n_sgpr_cols = 4;
      uint32_t n_sgpr_rows = wave_state.num_sgprs / n_sgpr_cols;

      for (uint32_t sgpr_row = 0; sgpr_row < n_sgpr_rows; ++sgpr_row) {
        err << " ";

        for (uint32_t sgpr_col = 0; sgpr_col < n_sgpr_cols; ++sgpr_col) {
          uint32_t sgpr_idx = (sgpr_row * n_sgpr_cols) + sgpr_col;
          uint32_t sgpr_val = wave_state.sgprs[sgpr_idx];

          std::stringstream sgpr_str;
          sgpr_str << "s" << sgpr_idx;

          err << std::setw(6) << std::setfill(' ') << sgpr_str.str();
          err << ": 0x" << std::setw(8) << std::setfill('0') << sgpr_val;
        }

        err << "\n";
      }

      err << "\n";

      uint32_t n_vgpr_cols = 4;
      uint32_t n_vgpr_rows = wave_state.num_vgprs / n_vgpr_cols;

      for (uint32_t lane_idx = 0; lane_idx < wave_state.num_vgpr_lanes; ++lane_idx) {
        err << "Lane 0x" << lane_idx << "\n";

        for (uint32_t vgpr_row = 0; vgpr_row < n_vgpr_rows; ++vgpr_row) {
          err << " ";

          for (uint32_t vgpr_col = 0; vgpr_col < n_vgpr_cols; ++vgpr_col) {
            uint32_t vgpr_idx = (vgpr_row * n_vgpr_cols) + vgpr_col;
            uint32_t vgpr_val = wave_state.vgprs[(vgpr_idx * wave_state.num_vgpr_lanes) + lane_idx];

            std::stringstream vgpr_str;
            vgpr_str << "v" << vgpr_idx;

            err << std::setw(6) << std::setfill(' ') << vgpr_str.str();
            err << ": 0x" << std::setw(8) << std::setfill('0') << vgpr_val;
          }

          err << "\n";
        }
      }

      err << "\n";

      if (wave_state.lds) {
        err << "LDS:\n\n";

        uint32_t n_lds_cols = 4;
        uint32_t n_lds_rows = wave_state.lds_size_dw / n_lds_cols;

        for (uint32_t lds_row = 0; lds_row < n_lds_rows; ++lds_row) {
          uint32_t lds_addr = lds_row * n_lds_cols * 4;

          err << "0x" << std::setw(4) << std::setfill('0') << lds_addr << ":";

          for (uint32_t lds_col = 0; lds_col < n_lds_cols; ++lds_col) {
            uint32_t lds_idx = (lds_row * n_lds_cols) + lds_col;
            uint32_t lds_val = wave_state.lds[lds_idx];

            err << "  0x" << std::setw(8) << std::setfill('0') << lds_val;
          }

          err << "\n";
        }

        err << "\n";
      }

      // Attempt to match the PC to a loaded code object.
      amd::hsa::loader::LoadedCodeObject* pc_code_obj = nullptr;
      uint64_t pc_code_obj_offset = 0;

      auto iter_execs = [&](hsa_executable_t exec) {
        auto iter_code_objs = [&](hsa_loaded_code_object_t code_obj) {
          auto iter_segments = [&](amd_loaded_segment_t segment) {
            auto segment_int = amd::hsa::loader::LoadedSegment::Object(segment);

            uint64_t load_base, load_size;
            segment_int->GetInfo(AMD_LOADED_SEGMENT_INFO_LOAD_BASE_ADDRESS, &load_base);
            segment_int->GetInfo(AMD_LOADED_SEGMENT_INFO_SIZE, &load_size);

            if ((wave_state.regs.pc >= load_base) &&
                (wave_state.regs.pc < (load_base + load_size))) {
              pc_code_obj = amd::hsa::loader::LoadedCodeObject::Object(code_obj);
              pc_code_obj_offset = wave_state.regs.pc - load_base;
            }

            return HSA_STATUS_SUCCESS;
          };

          amd::hsa::loader::LoadedCodeObject::Object(code_obj)->IterateLoadedSegments(
              [](amd_loaded_segment_t segment, void* data) {
                return (*reinterpret_cast<decltype(iter_segments)*>(data))(segment);
              },
              &iter_segments);

          return HSA_STATUS_SUCCESS;
        };

        amd::hsa::loader::Executable::Object(exec)->IterateLoadedCodeObjects(
            [](hsa_loaded_code_object_t code_obj, void* data) {
              return (*reinterpret_cast<decltype(iter_code_objs)*>(data))(code_obj);
            },
            &iter_code_objs);

        return HSA_STATUS_SUCCESS;
      };

      core::Runtime::runtime_singleton_->loader()->IterateExecutables(
          [](hsa_executable_t exec, void* data) {
            return (*reinterpret_cast<decltype(iter_execs)*>(data))(exec);
          },
          &iter_execs);

      if (pc_code_obj) {
        // Write the code object to a temporary file.
        uint64_t elf_addr;
        size_t elf_size;
        pc_code_obj->GetInfo(AMD_LOADED_CODE_OBJECT_INFO_ELF_IMAGE, &elf_addr);
        pc_code_obj->GetInfo(AMD_LOADED_CODE_OBJECT_INFO_ELF_IMAGE_SIZE, &elf_size);

        char code_obj_path[] = "/tmp/hsartXXXXXX";
        int code_obj_fd = ::mkstemp(code_obj_path);
        ::write(code_obj_fd, (const void*)uintptr_t(elf_addr), elf_size);
        ::close(code_obj_fd);

        // Invoke binutils objdump on the code object.
        int pipe_fd[2];
        ::pipe(pipe_fd);

        pid_t pid = ::fork();

        if (pid == 0) {
          ::dup2(pipe_fd[1], STDOUT_FILENO);
          ::dup2(pipe_fd[1], STDERR_FILENO);
          ::close(pipe_fd[0]);
          ::close(pipe_fd[1]);

          // Disassemble X bytes before/after the PC.
          uint32_t disasm_context = 0x20;

          std::stringstream arg_start_addr, arg_stop_addr;
          arg_start_addr << "--start-addr=0x" << std::hex << (pc_code_obj_offset - disasm_context);
          arg_stop_addr << "--stop-addr=0x" << std::hex << (pc_code_obj_offset + disasm_context);

          std::exit(execlp("objdump", "-d", "-S", "-l", arg_start_addr.str().c_str(),
                           arg_stop_addr.str().c_str(), code_obj_path, nullptr));
        }

        // Collect the output of objdump.
        ::close(pipe_fd[1]);

        std::vector<char> objdump_out_buf;
        std::vector<char> buf(0x1000);
        ssize_t n_read_b;

        while ((n_read_b = read(pipe_fd[0], buf.data(), buf.size())) > 0) {
          objdump_out_buf.insert(objdump_out_buf.end(), &buf[0], &buf[n_read_b]);
        }

        ::close(pipe_fd[0]);

        int child_status = 0;
        int ret = ::waitpid(pid, &child_status, 0);

        if (ret != -1 && child_status == 0) {
          // Attempt to trim the leading output from objdump.
          std::string objdump_out(objdump_out_buf.begin(), objdump_out_buf.end());
          size_t trim_start = objdump_out.find(":\n\n") + 3;

          if (trim_start != objdump_out.npos) {
            objdump_out = objdump_out.substr(trim_start);
          }

          // Attempt to add a PC indicator inside the disassembly text.
          std::stringstream pc_offset_find;
          pc_offset_find << std::hex << pc_code_obj_offset << ":\t";
          size_t replace_idx = objdump_out.find(pc_offset_find.str());

          if (replace_idx != objdump_out.npos) {
            std::stringstream pc_offset_replace;
            pc_offset_replace << std::hex << pc_code_obj_offset << ": >>>>>\t";
            objdump_out.replace(replace_idx, pc_offset_find.str().size(), pc_offset_replace.str());
            err << objdump_out << "\n";
          } else {
            err << objdump_out;
            err << "\nPC offset: " << std::hex << pc_code_obj_offset << "\n\n";
          }
        } else {
          err << "(Disassembly unavailable - is amdgcn-capable objdump in PATH?)\n\n";
        }

        ::unlink(code_obj_path);
      } else {
        err << "(Cannot match PC to a loaded code object)\n\n";
      }
    }
  }

  std::cerr << err.str();
  std::abort();
}
}
