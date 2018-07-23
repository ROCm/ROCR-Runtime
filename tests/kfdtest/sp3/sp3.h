/*
 * Copyright (C) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __SP3_H__
#define __SP3_H__

#ifdef __cplusplus
extern "C" {
#endif


/// @file sp3.h
/// @brief sp3 API
#include <stdint.h>

// Export tags
#define SP3_EXPORT


/// @defgroup sp3main SP3 Main API
///
/// Main API to assemble and disassemble SP3 shaders.
///
/// @{


/// Valid shader stages.
enum sp3_shtype {
    SP3_SHTYPE_NONE = -1,
    SP3_SHTYPE_PS   = 0,
    SP3_SHTYPE_VS   = 1,
    SP3_SHTYPE_GS   = 2,
    SP3_SHTYPE_ES   = 3,
    SP3_SHTYPE_HS   = 4,
    SP3_SHTYPE_LS   = 5,
    SP3_SHTYPE_CS   = 6,
    SP3_SHTYPE_ACV  = 7,
};

/// Assorted constants used by sp3 API.
enum sp3_count {
    SP3_NUM_MRT     = 8,    ///< Maximum number of render targets supported.
    SP3_NUM_STRM    = 4,    ///< Maximum number of streams supported.
};

/// Disassembly flags. Bitwise-OR flags to set options.
enum sp3_flag {
    SP3DIS_NO_STATE     = 0x01, ///< Do not include state header at top of shader.
    SP3DIS_NO_BINARY    = 0x02, ///< Do not include comments with raw binary microcode.
    SP3DIS_COMMENTS     = 0x04, ///< Do not include comments.
    SP3DIS_NO_GPR_COUNT = 0x08, ///< Do not include GPR allocation counts.
    SP3DIS_FORCEVALID   = 0x10, ///< Force all bytes of microcode to be disassembled.
    SP3DIS_NO_ASIC      = 0x20, ///< Do not emit the asic header at top of shader.
};

/// Shader context. Contains no user-visible fields.
struct sp3_context;

/// Memory object. Contains no user-visible fields.
struct sp3_vma;

/// VM addresses are 64-bit and the address unit is 32 bits
typedef uint64_t sp3_vmaddr;

/// Storage entry for register streams.
struct sp3_reg {
    uint32_t index;             ///< One of the MM aperture register addresses.
    uint32_t value;             ///< 32-bit register data.
};

/// Bits for a single instruction.
struct sp3_inst_bits {
    uint32_t val[5];            ///< Largest single instruction in any backend is 5 dwords.
};

/// Wrapped shader metadata.
///
/// After generation, shaders are encapsulated in sp3_shader structures.
///
/// Those structures contain the shader binary, its register stream, constants and constant
/// buffers and metadata needed for SC compatibility.
///
struct sp3_shader {
    enum sp3_shtype type;       ///< One of the SHTYPE_* constants.
    uint32_t asic_int;          ///< Internal ASIC index. Do not use.
    char asic[0x100];           ///< ASIC name as a string ("RV870" etc).
    uint32_t size;              ///< Size of the compiled shader, in 32-bit words.
    uint32_t nsgprs;            ///< Number of scalar GPRs used.
    uint32_t nvgprs;            ///< Number of vector GPRs used.
    uint32_t nsvgprs;           ///< Number of shared vector GPRs used.
    uint32_t nsgprs_manual_alloc;
    uint32_t nvgprs_manual_alloc;
    uint32_t nsvgprs_manual_alloc;
    uint32_t trap_present;
    uint32_t user_sgpr_count;
    uint32_t scratch_en;
    uint32_t dispatch_draw_en;
    uint32_t so_en;
    uint32_t so_base0_en;
    uint32_t so_base1_en;
    uint32_t so_base2_en;
    uint32_t so_base3_en;
    uint32_t oc_lds_en;
    uint32_t tg_size_en;
    uint32_t tidig_comp_cnt;    ///< Number of components(-1) enabled for thread id in group
    uint32_t tgid_x_en;
    uint32_t tgid_y_en;
    uint32_t tgid_z_en;
    uint32_t wave_cnt_en;
    uint32_t primgen_en;
    uint32_t pc_base_en;
    uint32_t sgpr_scratch;
    uint32_t sgpr_psvs_state;
    uint32_t sgpr_gs2vs_offset;
    uint32_t sgpr_so_write_index;
    uint32_t sgpr_so_base_offset0;
    uint32_t sgpr_so_base_offset1;
    uint32_t sgpr_so_base_offset2;
    uint32_t sgpr_so_base_offset3;
    uint32_t sgpr_offchip_lds;
    uint32_t sgpr_is_offchip;
    uint32_t sgpr_ring_offset;
    uint32_t sgpr_gs_wave_id;
    uint32_t sgpr_global_wave_id;
    uint32_t sgpr_tg_size;
    uint32_t sgpr_tgid_x;
    uint32_t sgpr_tgid_y;
    uint32_t sgpr_tgid_z;
    uint32_t sgpr_tf_base;
    uint32_t sgpr_pc_base;
    uint32_t sgpr_wave_cnt;
    uint32_t wave_size;         ///< Number of threads in a wavefront (only certain ASICs; 0 = don't care).
    uint32_t pc_exports;        ///< Range of parameters exported (if VS).
    uint32_t pos_export;        ///< Shader executes a position export (if VS).
    uint32_t cb_exports;        ///< Range of MRTs exported (if PS).
    uint32_t mrtz_export_format;///< Export format of the mrtz export.
    uint32_t z_export;          ///< Shader executes a Z export (if PS).
    uint32_t pops_en;           ///< Shader is POPS (PS)
    uint32_t pops_num_samples;  ///<  (PS)
    uint32_t load_collision_waveid;     ///< Shader sets load collision waveid (if PS).
    uint32_t load_intrawave_collision;  ///< Shader is in intrawave mode (if PS).
    uint32_t stencil_test_export;       ///< Shader exports stencil (if PS).
    uint32_t stencil_op_export; ///< Shader exports stencil (if PS).
    uint32_t kill_used;         ///< Shader executes ALU KILL operations.
    uint32_t cb_masks[SP3_NUM_MRT];     ///< Component masks for each MRT exported (if PS).
    uint32_t emit_used;         ///< EMIT opcodes used (if GS).
    uint32_t covmask_export;    ///< Shader exports coverage mask (if PS).
    uint32_t mask_export;       ///< Shader exports mask (if PS).
    uint32_t strm_used[SP3_NUM_STRM];   ///< Streamout operations used (map).
    uint32_t scratch_used;      ///< Scratch SMX exports used.
    uint32_t scratch_itemsize;  ///< Scratch ring item size.
    uint32_t reduction_used;    ///< Reduction SMX exports used.
    uint32_t ring_used;         ///< ESGS/GSVS ring SMX exports used.
    uint32_t ring_itemsize;     ///< ESGS/GSVS ring item size (for ES/GS respectively).
    uint32_t vertex_size[4];    ///< GSVS ring vertex size (for GS).
    uint32_t mem_used;          ///< Raw memory SMX exports used.
    uint32_t rats_used;         ///< Mask of RATs (UAVs) used
    uint32_t group_size[3];     ///< Wavefront group size (for ELF files).
    uint32_t alloc_lds;         ///< Number of LDS bytes allocated for wave group. (translates to lds_size in CS and LS)
    uint32_t *data;             ///< Shader binary data.
    uint32_t nregs;             ///< Number of register writes in the stream.
    uint64_t crc64;             ///< CRC64 of compiled shader, may be used for identification/fingerprinting.
    uint32_t crc32;             ///< 32-bit CRC of compiled shader (based on crc64), may be used for identification/fingerprinting.
    struct sp3_reg *regs;       ///< Register writes (index-value pairs).
    struct sp3_shader *merged_2nd_shader;   ///< Merged es/gs, ls/hs shader, this points to start of the second shader (only certain ASICs).
};

/// Comment callback.
typedef const char *(*sp3_comment_cb)(void *, int);


/// Get version of the sp3 library.
///
/// @return String containing the version number.
///
SP3_EXPORT const char *sp3_version(void);

/// Create a new sp3 context.
///
/// @return A new context for use in assembling and disassembling shaders. Free with sp3_close().
///
SP3_EXPORT struct sp3_context *sp3_new(void);

/// Set option for sp3.
///
/// @param state sp3 context.
/// @param option Option name. Unknown options will raise an error.
/// @param value Option value. NULL is used to represent value-less options.
///
/// Currently supported options:
///
/// Werror (boolean) -- indicates whether warnings should be treated as errors.
///
/// wave_size (integer) -- sets the wave size being used by the draw calls that will be using
/// this shader.  Ignored in certain ASICs.  You may set this to 32, 64 or the special value 0
/// to indicate no preference on wave size.  The shader will be checked to ensure it is
/// compatible with the size specified here.
///
/// omit_version (boolean) -- omit generation of the S_VERSION opcode.
///
/// omit_code_end (boolean) -- omit generation of the S_CODE_END footer.
///
SP3_EXPORT void sp3_set_option(
    struct sp3_context *state,
    const char *option,
    const char *value);

/// Parse a file into a context.
///
/// Use sp3_compile to generate binary microcode after the shader is parsed.
///
/// @param state Context to use for parsing.
/// @param file File to read. If NULL, parse from stdin.
///
SP3_EXPORT void sp3_parse_file(struct sp3_context *state, const char *file);

/// Parse a string into a context.
///
/// Use sp3_compile to generate binary microcode after the shader is parsed.
///
/// @param state Context to use for parsing.
/// @param string String to parse.
///
SP3_EXPORT void sp3_parse_string(struct sp3_context *state, const char *string);

/// Parse a file from the standard library into a context.
///
/// Use sp3_compile to generate binary microcode after the shader is parsed.
///
/// @param state Context to use for parsing.
/// @param name Path to the standard library; files in this directory are parsed.
///
SP3_EXPORT void sp3_parse_library(struct sp3_context *state, const char *name);

/// Call a sp3 function.
///
SP3_EXPORT void sp3_call(struct sp3_context *state, const char *func);

/// Compile a shader program that has been parsed into the context.
///
/// @param state sp3 context.
/// @param cffunc Name of clause to call. By convention, this is "main".
/// @return A compiled and linked shader.  Free memory with sp3_free_shader().
///
SP3_EXPORT struct sp3_shader *sp3_compile(
    struct sp3_context *state,
    const char *cffunc);

/// Free a sp3_shader.
///
/// @param sh Shader object to delete.
///
SP3_EXPORT void sp3_free_shader(struct sp3_shader *sh);

/// Get current ASIC name set for a context.
///
/// @param state Context to query.
/// @return Name of ASIC.
///
SP3_EXPORT const char *sp3_getasic(struct sp3_context *state);

/// Set current ASIC name for a context.
///
/// @param state Context to modify.
/// @param chip Case-insensitive string representing the ASIC to compile or disassemble for.
///
SP3_EXPORT void sp3_setasic(struct sp3_context *state, const char *chip);

/// Set global variable in context to an integer.
///
SP3_EXPORT void sp3_set_param_int(
    struct sp3_context *state,
    const char *name,
    int32_t value);

/// Set global variable in context to an integer vector.
///
SP3_EXPORT void sp3_set_param_intvec(
    struct sp3_context *state,
    const char *name,
    uint32_t size,
    const int32_t *value);

/// Set global variable in context to a float.
///
SP3_EXPORT void sp3_set_param_float(
    struct sp3_context *state,
    const char *name,
    float value);

/// Set global variable in context to a float vector.
///
SP3_EXPORT void sp3_set_param_floatvec(
    struct sp3_context *state,
    const char *name,
    uint32_t size,
    const float *value);

/// Set error message header.
///
/// @param state Context to modify.
/// @param str Text to include in error message header.
///
SP3_EXPORT void sp3_set_error_header(struct sp3_context *state, const char *str);

/// Get ASIC metrics for the ASIC in current state.
///
/// Used by ELF tools to fill in some CAL fields.
///
/// @param state Context to query.
/// @param name Name of ASIC metric.
/// @return Value of ASIC metric.
///
SP3_EXPORT int sp3_asicinfo(struct sp3_context *state, const char *name);

/// Free a context allocated by sp3_new/open/parse.
///
/// @param state Context to delete.
///
SP3_EXPORT void sp3_close(struct sp3_context *state);

/// Disassemble a shader.
///
/// This call is likely to change to something that will take a filled sp3_shader structure
/// later on.
///
/// @param state sp3 context (use sp3_new to allocate and sp3_setasic to set ASIC).
/// @param bin Memory map with the opcodes (see sp3-vm.h).
/// @param base Start of the shader in the memory map (in VM entries, i.e. 32-bit words).
/// @param name Same to give the disassembled shader.
/// @param shader_type One of the SHTYPE_* constants.
/// @param include Literal text to include in the CF clause (NULL includes nothing).
/// @param max_len Maximum length of CF clause. Matters if SP3DIS_FORCEVALID is set.
/// @param flags A bitmask of SP3DIS_* flags.
///
/// @return Shader disassembly as a string. Free memory with sp3_free().
///
SP3_EXPORT char *sp3_disasm(
    struct sp3_context *state,
    struct sp3_vma *bin,
    sp3_vmaddr base,
    const char *name,
    enum sp3_shtype shader_type,
    const char *include,
    uint32_t max_len,
    uint32_t flags);

/// Disassemble a single shader instruction.
///
/// This call is likely to change to something that will take a filled sp3_shader structure
/// later on.
///
/// @param state sp3 context (use sp3_new to allocate and sp3_setasic to set ASIC).
/// @param inst Pointer to dwords containing instruction (exact number of dwords required depends on instruction).
/// @param base Start of the shader in the memory map (in VM entries, i.e. 32-bit words).
/// @param addr Address of the instruction being disassembled (in VM entries, i.e. 32-bit words).
/// @param shader_type One of the SHTYPE_* constants.
/// @param flags A mask of SP3DIS_* flags.
///
/// @return Shader disassembly as a string. Free memory with sp3_free().
///
SP3_EXPORT char *sp3_disasm_inst(
    struct sp3_context *state,
    const struct sp3_inst_bits *inst,
    sp3_vmaddr base,
    sp3_vmaddr addr,
    enum sp3_shtype shader_type,
    uint32_t flags);

/// Parse a register stream.
///
/// Can be called before sp3_disasm to preset things like ALU, boolean and loop constants.
///
/// This call is likely to merge with sp3_disasm later on.
///
/// @param state sp3 context to fill with state.
/// @param nregs Number of register entries.
/// @param regs Register stream to parse.
/// @param shader_type One of the SHTYPE_* constants.
///
SP3_EXPORT void sp3_setregs(
    struct sp3_context *state,
    uint32_t nregs,
    const struct sp3_reg *regs,
    enum sp3_shtype shader_type);


/// Set shader comments
///
/// @param state sp3 context.
/// @param map Map of comments (0 for no comment, other values will be passed to the callback).
/// @param f_top Callback returning comment to place above the opcode.
/// @param f_right Callback returning comment to place to the right of the opcode.
/// @param ctx Void pointer to pass to comment callbacks.
///
SP3_EXPORT void sp3_setcomments(
    struct sp3_context *state,
    struct sp3_vma *map,
    sp3_comment_cb f_top,
    sp3_comment_cb f_right,
    void *ctx);

/// Set alternate shader entry points
///
/// Used for disassembly; this marks an additional location in memory
/// (besides the start address) where shader code may be found. Generally
/// required for jump tables and any case where the shader may perform
/// indirect jumps to ensure that disassembly locates all shader
/// instructions.
///
/// @param state sp3 context (use sp3_new to allocate and sp3_setasic to set ASIC).
/// @param addr Address of the instruction being disassembled (in VM entries, i.e. 32-bit words).
///
SP3_EXPORT void sp3_setentrypoint(
    struct sp3_context *state,
    sp3_vmaddr addr);

/// Clear alternate shader entry points.
///
/// Clear all entry points previously set with sp3_setentrypoint.
///
/// @param state sp3 context (use sp3_new to allocate and sp3_setasic to set ASIC).
///
SP3_EXPORT void sp3_clearentrypoints(struct sp3_context *state);

/// Free memory allocated by sp3.
///
/// Windows DLLs that allocate memory have to free it. This function
/// should be used to free the result of sp3_disasm, sp3_compile etc.
///
SP3_EXPORT void sp3_free(void *ptr);

/// SP3 API to merge two shaders given file names as input.
///
SP3_EXPORT struct sp3_shader* sp3_merge_shaders(
    struct sp3_context *pointer,
    const char *first_file,
    const char *second_file);

/// SP3 API to merge two shaders given shader strings as input.
///
SP3_EXPORT struct sp3_shader* sp3_merge_shader_strings(
    struct sp3_context *pointer,
    const char *first_string,
    const char *second_string);


/// @}


/// @defgroup sp3vm SP3 Memory Objects
///
/// The VM API is used to manage virtual memory maps.  Those maps are used for binary storage
/// for disassembly, as they can naturally mirror the GPU's memory map (so no register
/// translation is needed).
///
/// @{

/// Callback function that will fill a VMA on demand
///
/// The VMA to be filled will be specified through the request address.
/// The callback should fill the VMA using sp3_vm_write calls.
///
typedef void (* sp3_vmfill)(struct sp3_vma *vm, sp3_vmaddr addr, void *ctx);

/// Create a new VM that is empty.
///
/// Free the object with sp3_vm_free().
///
/// @return New VM object.
///
SP3_EXPORT
struct sp3_vma *sp3_vm_new(void);

/// Create a new VM that has a sp3_vmfill callback.
///
/// Free the object with sp3_vm_free().
///
/// @param fill Function used to populate data in VM. The function will be pass the new VM object, the address and a context.
/// @param ctx User-specified context. Passed to the fill function and not used by sp3 itself.
/// @return New VM object.
///
SP3_EXPORT
struct sp3_vma *sp3_vm_new_fill(sp3_vmfill fill, void *ctx);

/// Create a new VM from an array of words.
///
/// Free the object with sp3_vm_free().
///
/// @param base VM address to load array at.
/// @param len Number of 32-bit words in the array.
/// @param data Pointer to the array.
/// @return New VM object.
///
SP3_EXPORT
struct sp3_vma *sp3_vm_new_ptr(sp3_vmaddr base, sp3_vmaddr len, const uint32_t *data);

/// Find a VMA, optionally adding it.
///
/// @param vm VM to search in.
/// @param addr Address to search for.
/// @param add Flag indicating whether a failure should result in adding a new VMA.
/// @return VM object matching the specified address.
///
SP3_EXPORT
struct sp3_vma *sp3_vm_find(struct sp3_vma *vm, sp3_vmaddr addr, uint32_t add);

/// Write a word to a VM.
///
/// @param vm VM to write.
/// @param addr Address to write.
/// @param val 32-bits of data to write.
///
SP3_EXPORT
void sp3_vm_write(struct sp3_vma *vm, sp3_vmaddr addr, uint32_t val);

/// Read a word from a VM.
///
/// @param vm VM to read.
/// @param addr Address to read.
/// @return 32-bits of data at specified address.
///
SP3_EXPORT
uint32_t sp3_vm_read(struct sp3_vma *vm, sp3_vmaddr addr);

/// Probe VM for presence.
///
/// @param vm VM to probe.
/// @param addr Address to search for.
/// @return 1 if the specified address is backed in the VM, 0 otherwise.
///
SP3_EXPORT
int sp3_vm_present(struct sp3_vma *vm, sp3_vmaddr addr);

/// Return base address of VM.
///
/// @param vm VM to query.
/// @return Base address.
///
SP3_EXPORT
sp3_vmaddr sp3_vm_base(struct sp3_vma *vm);

/// Return next VM.
///
/// @param vm VM to query.
/// @return Next VM in list.
///
SP3_EXPORT
struct sp3_vma *sp3_vm_next(struct sp3_vma *vm);

/// Free a VM and all its storage.
///
/// Use this function to free memory allocated by sp3_vm_new, sp3_vm_new_fill and
/// sp3_vm_new_ptr.
///
/// @param vm VM to free.
///
SP3_EXPORT
void sp3_vm_free(struct sp3_vma *vm);


/// @}


#ifdef __cplusplus
}
#endif


#endif /* __SP3_H__ */
