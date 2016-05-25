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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "hsa/hsa.h"
#include "hsa/hsa_ext_finalize.h"

#define check(msg, status) \
if (status != HSA_STATUS_SUCCESS) { \
    printf("%s failed.\n", #msg); \
    exit(1); \
} else { \
   printf("%s succeeded.\n", #msg); \
}

/*
 * Loads a BRIG module from a specified file. This
 * function does not validate the module.
 */
int load_module_from_file(const char* file_name, hsa_ext_module_t* module) {
    int rc = -1;

    FILE *fp = fopen(file_name, "rb");

    rc = fseek(fp, 0, SEEK_END);

    size_t file_size = (size_t) (ftell(fp) * sizeof(char));

    rc = fseek(fp, 0, SEEK_SET);

    char* buf = (char*) malloc(file_size);

    memset(buf,0,file_size);

    size_t read_size = fread(buf,sizeof(char),file_size,fp);

    if(read_size != file_size) {
        free(buf);
    } else {
        rc = 0;
        *module = (hsa_ext_module_t) buf;
    }

    fclose(fp);

    return rc;
}

/*
 * Determines if the given agent is of type HSA_DEVICE_TYPE_GPU
 * and sets the value of data to the agent handle if it is.
 */
static hsa_status_t get_gpu_agent(hsa_agent_t agent, void *data) {
    hsa_status_t status;
    hsa_device_type_t device_type;
    status = hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &device_type);
    if (HSA_STATUS_SUCCESS == status && HSA_DEVICE_TYPE_GPU == device_type) {
        hsa_agent_t* ret = (hsa_agent_t*)data;
        *ret = agent;
        return HSA_STATUS_INFO_BREAK;
    }
    return HSA_STATUS_SUCCESS;
}

/*
 * Determines if a memory region can be used for kernarg
 * allocations.
 */
static hsa_status_t get_kernarg_memory_region(hsa_region_t region, void* data) {
    hsa_region_segment_t segment;
    hsa_region_get_info(region, HSA_REGION_INFO_SEGMENT, &segment);
    if (HSA_REGION_SEGMENT_GLOBAL != segment) {
        return HSA_STATUS_SUCCESS;
    }

    hsa_region_global_flag_t flags;
    hsa_region_get_info(region, HSA_REGION_INFO_GLOBAL_FLAGS, &flags);
    if (flags & HSA_REGION_GLOBAL_FLAG_KERNARG) {
        hsa_region_t* ret = (hsa_region_t*) data;
        *ret = region;
        return HSA_STATUS_INFO_BREAK;
    }

    return HSA_STATUS_SUCCESS;
}

/*
 * Determines if a memory region can be used for fine grained
 * allocations.
 */
static hsa_status_t get_fine_grained_memory_region(hsa_region_t region, void* data) {
    hsa_region_segment_t segment;
    hsa_region_get_info(region, HSA_REGION_INFO_SEGMENT, &segment);
    if (HSA_REGION_SEGMENT_GLOBAL != segment) {
        return HSA_STATUS_SUCCESS;
    }

    hsa_region_global_flag_t flags;
    hsa_region_get_info(region, HSA_REGION_INFO_GLOBAL_FLAGS, &flags);
    if (flags & HSA_REGION_GLOBAL_FLAG_FINE_GRAINED) {
        hsa_region_t* ret = (hsa_region_t*) data;
        *ret = region;
        return HSA_STATUS_INFO_BREAK;
    }

    return HSA_STATUS_SUCCESS;
}

int main(int argc, char **argv) {
    hsa_status_t err;

    err = hsa_init();
    check(Initializing the hsa runtime, err);

    /*
     * Determine if the finalizer 1.0 extension is supported.
     */
    bool support;

    err = hsa_system_extension_supported(HSA_EXTENSION_FINALIZER, 1, 0, &support);

    check(Checking finalizer 1.0 extension support, err);

    /*
     * Generate the finalizer function table.
     */
    hsa_ext_finalizer_1_00_pfn_t table_1_00;

    err = hsa_system_get_extension_table(HSA_EXTENSION_FINALIZER, 1, 0, &table_1_00);

    check(Generating function table for finalizer, err);

    /* 
     * Iterate over the agents and pick the gpu agent using 
     * the get_gpu_agent callback.
     */
    hsa_agent_t agent;
    err = hsa_iterate_agents(get_gpu_agent, &agent);
    if(err == HSA_STATUS_INFO_BREAK) { 
        err = HSA_STATUS_SUCCESS; 
    } else {
        /*
         * No GPU agent was found.
         */
        err = HSA_STATUS_ERROR; 
    }
    check(Getting a gpu agent, err);

    /*
     * Query the name of the agent.
     */
    char name[64] = { 0 };
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_NAME, name);
    check(Querying the agent name, err);
    printf("The agent name is %s.\n", name);

    /*
     * Query the maximum size of the queue.
     */
    uint32_t queue_size = 0;
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_QUEUE_MAX_SIZE, &queue_size);
    check(Querying the agent maximum queue size, err);
    printf("The maximum queue size is %u.\n", (unsigned int) queue_size);

    /*
     * Create a queue using the maximum size.
     */
    hsa_queue_t* queue; 
    err = hsa_queue_create(agent, queue_size, HSA_QUEUE_TYPE_SINGLE, NULL, NULL, UINT32_MAX, UINT32_MAX, &queue);
    check(Creating the queue, err);

    /*
     * Obtain the agent's machine model
     */
    hsa_machine_model_t machine_model;
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_MACHINE_MODEL, &machine_model);
    check("Obtaining machine model",err);

    /*
     * Obtain the agent's profile
     */
    hsa_profile_t profile;
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_PROFILE, &profile);
    check("Getting agent profile",err);

    /*
     * Load the BRIG binary.
     */
    hsa_ext_module_t module;
    if(HSA_PROFILE_FULL == profile) {
        load_module_from_file("vector_copy_full.brig",&module);
    } else {
        load_module_from_file("vector_copy_base.brig",&module);
    }

    /*
     * Create hsa program.
     */
    hsa_ext_program_t program;
    memset(&program,0,sizeof(hsa_ext_program_t));
    err = table_1_00.hsa_ext_program_create(machine_model, profile, HSA_DEFAULT_FLOAT_ROUNDING_MODE_DEFAULT, NULL, &program);
    check(Create the program, err);

    /*
     * Add the BRIG module to hsa program.
     */
    err = table_1_00.hsa_ext_program_add_module(program, module);
    check(Adding the brig module to the program, err);

    /*
     * Determine the agents ISA.
     */
    hsa_isa_t isa;
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_ISA, &isa);
    check(Query the agents isa, err);

    /*
     * Finalize the program and extract the code object.
     */
    hsa_ext_control_directives_t control_directives;
    memset(&control_directives, 0, sizeof(hsa_ext_control_directives_t));
    hsa_code_object_t code_object;
    err = table_1_00.hsa_ext_program_finalize(program, isa, 0, control_directives, "", HSA_CODE_OBJECT_TYPE_PROGRAM, &code_object);
    check(Finalizing the program, err);

    /*
     * Destroy the program, it is no longer needed.
     */
    err=table_1_00.hsa_ext_program_destroy(program);
    check(Destroying the program, err);

    /*
     * Create the empty executable.
     */
    hsa_executable_t executable;
    err = hsa_executable_create(profile, HSA_EXECUTABLE_STATE_UNFROZEN, "", &executable);
    check(Create the executable, err);

    /*
     * Load the code object.
     */
    err = hsa_executable_load_code_object(executable, agent, code_object, "");
    check(Loading the code object, err);

    /*
     * Freeze the executable; it can now be queried for symbols.
     */
    err = hsa_executable_freeze(executable, "");
    check(Freeze the executable, err);

   /*
    * Extract the symbol from the executable.
    */
    hsa_executable_symbol_t symbol;
    err = hsa_executable_get_symbol(executable, NULL, "&__vector_copy_kernel", agent, 0, &symbol);
    check(Extract the symbol from the executable, err);

    /*
     * Extract dispatch information from the symbol
     */
    uint64_t kernel_object;
    uint32_t kernarg_segment_size;
    uint32_t group_segment_size;
    uint32_t private_segment_size;
    err = hsa_executable_symbol_get_info(symbol, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT, &kernel_object);
    check(Extracting the symbol from the executable, err);
    err = hsa_executable_symbol_get_info(symbol, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_KERNARG_SEGMENT_SIZE, &kernarg_segment_size);
    check(Extracting the kernarg segment size from the executable, err);
    err = hsa_executable_symbol_get_info(symbol, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_GROUP_SEGMENT_SIZE, &group_segment_size);
    check(Extracting the group segment size from the executable, err);
    err = hsa_executable_symbol_get_info(symbol, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_PRIVATE_SEGMENT_SIZE, &private_segment_size);
    check(Extracting the private segment from the executable, err);

    /*
     * Create a signal to wait for the dispatch to finish.
     */ 
    hsa_signal_t signal;
    err=hsa_signal_create(1, 0, NULL, &signal);
    check(Creating a HSA signal, err);

    hsa_region_t finegrained_region;
    finegrained_region.handle=(uint64_t)-1;
    hsa_agent_iterate_regions(agent, get_fine_grained_memory_region, &finegrained_region);
    err = (finegrained_region.handle == (uint64_t)-1) ? HSA_STATUS_ERROR : HSA_STATUS_SUCCESS;
    check(Finding a fine grained memory region, err);

    /*
     * Allocate and initialize the kernel arguments from the fine
     * grained memory region.
     */
    char* in;
    err=hsa_memory_allocate(finegrained_region, 1024*1024*4, (void*) &in);
    check(Allocating argument memory for input parameter, err);
    memset(in, 1, 1024*1024*4);

    char* out;
    err=hsa_memory_allocate(finegrained_region, 1024*1024*4, (void*) &out);
    check(Allocating argument memory for output parameter, err);
    memset(out, 0, 1024*1024*4);

    struct __attribute__ ((aligned(16))) args_t {
        void* in;
        void* out;
    } args;

    args.in=in;
    args.out=out;

    /*
     * Find a memory region that supports kernel arguments.
     */
    hsa_region_t kernarg_region;
    kernarg_region.handle=(uint64_t)-1;
    hsa_agent_iterate_regions(agent, get_kernarg_memory_region, &kernarg_region);
    err = (kernarg_region.handle == (uint64_t)-1) ? HSA_STATUS_ERROR : HSA_STATUS_SUCCESS;
    check(Finding a kernarg memory region, err);
    void* kernarg_address = NULL;

    /*
     * Allocate the kernel argument buffer from the correct region.
     */   
    err = hsa_memory_allocate(kernarg_region, kernarg_segment_size, &kernarg_address);
    check(Allocating kernel argument memory buffer, err);
    memcpy(kernarg_address, &args, sizeof(args));
 
    /*
     * Obtain the current queue write index.
     */
    uint64_t index = hsa_queue_load_write_index_relaxed(queue);

    /*
     * Write the aql packet at the calculated queue index address.
     */
    const uint32_t queueMask = queue->size - 1;
    hsa_kernel_dispatch_packet_t* dispatch_packet = &(((hsa_kernel_dispatch_packet_t*)(queue->base_address))[index&queueMask]);

    dispatch_packet->setup  |= 1 << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;
    dispatch_packet->workgroup_size_x = (uint16_t)256;
    dispatch_packet->workgroup_size_y = (uint16_t)1;
    dispatch_packet->workgroup_size_z = (uint16_t)1;
    dispatch_packet->grid_size_x = (uint32_t) (1024*1024);
    dispatch_packet->grid_size_y = 1;
    dispatch_packet->grid_size_z = 1;
    dispatch_packet->completion_signal = signal;
    dispatch_packet->kernel_object = kernel_object;
    dispatch_packet->kernarg_address = (void*) kernarg_address;
    dispatch_packet->private_segment_size = private_segment_size;
    dispatch_packet->group_segment_size = group_segment_size;

    uint16_t header = 0;
    header |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE;
    header |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE;
    header |= HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE;

    __atomic_store_n((uint16_t*)(&dispatch_packet->header), header, __ATOMIC_RELEASE);

    /*
     * Increment the write index and ring the doorbell to dispatch the kernel.
     */
    hsa_queue_store_write_index_relaxed(queue, index+1);
    hsa_signal_store_relaxed(queue->doorbell_signal, index);
    check(Dispatching the kernel, err);

    /*
     * Wait on the dispatch completion signal until the kernel is finished.
     */
    hsa_signal_value_t value = hsa_signal_wait_acquire(signal, HSA_SIGNAL_CONDITION_LT, 1, UINT64_MAX, HSA_WAIT_STATE_BLOCKED);

    /*
     * Validate the data in the output buffer.
     */
    int valid=1;
    int fail_index=0;
    for(int i=0; i<1024*1024; i++) {
        if(out[i]!=in[i]) {
            fail_index=i;
            valid=0;
            break;
        }
    }

    if(valid) {
        printf("Passed validation.\n");
    } else {
        printf("VALIDATION FAILED!\nBad index: %d\n", fail_index);
    }

    /*
     * Cleanup all allocated resources.
     */
    err = hsa_memory_free(kernarg_address);
    check(Freeing kernel argument memory buffer, err);

    err=hsa_signal_destroy(signal);
    check(Destroying the signal, err);

    err=hsa_executable_destroy(executable);
    check(Destroying the executable, err);

    err=hsa_code_object_destroy(code_object);
    check(Destroying the code object, err);

    err=hsa_queue_destroy(queue);
    check(Destroying the queue, err);
    
    err = hsa_memory_free(in);
    check(Freeing in argument memory buffer, err);

    err = hsa_memory_free(out);
    check(Freeing out argument memory buffer, err);

    err=hsa_shut_down();
    check(Shutting down the runtime, err);

    return 0;
}
