#include "hsa_base_util.h"
#include "HSAILAmdExt.h"


void HSA_UTIL::GetHsailNameAndKernelName(char * file_name, char *kernel_name)
{
	strcpy(hail_file_name, file_name);
	strcpy(hsa_kernel_name, kernel_name);
}

HSA_UTIL::HSA_UTIL()
{
#ifdef TIME
    	base_kernel_time_idx = base_timer.CreateTimer();
	base_setup_time_idx = base_timer.CreateTimer();
#endif
}

HSA_UTIL::~HSA_UTIL()
{

}


bool HSA_UTIL::HsaInit()
{
#ifdef TIME
       base_timer.StartTimer(base_setup_time_idx);
#endif

 	err = hsa_init();
 	check(Initializing the hsa runtime, err);

	/* 
	 * Iterate over the agents and pick the gpu agent using 
	 * the find_gpu callback.
	 */
	err = hsa_iterate_agents(find_gpu, &device);
	check(Calling hsa_iterate_agents, err);

	err = (device.handle== 0) ? HSA_STATUS_ERROR : HSA_STATUS_SUCCESS;
	check(Checking if the GPU device is non-zero, err);

	if (err == HSA_STATUS_ERROR)
		return false;

	/*
	 * Query the maximum size of the queue.
	 */
	err = hsa_agent_get_info(device, HSA_AGENT_INFO_QUEUE_MAX_SIZE, &queue_size);
	check(Querying the device maximum queue size, err);

	/*  
	 * Create a queue using the maximum size.
	 */
	err = hsa_queue_create(device, queue_size, HSA_QUEUE_TYPE_MULTI, NULL, NULL, 0, 0, &command_queue);
	check(Creating the queue, err);

        amd::hsail::registerExtensions();
        if (!tool.assembleFromFile(hail_file_name)) {
          std::cout << tool.output();
          return false;
        }
        module = tool.brigModule();

	// Create hsail program.
	err = hsa_ext_program_create(HSA_MACHINE_MODEL_LARGE, HSA_PROFILE_FULL, HSA_DEFAULT_FLOAT_ROUNDING_MODE_ZERO, NULL, &hsa_program);
	check("Error in creating program object", err);

	// Add hsail module.
	//cout << "hsail file name = " << hail_file_name << endl;
	err = hsa_ext_program_add_module(hsa_program, module);
	check("Error in adding module to program object", err);

	// Finalize hsail program.
        hsa_isa_t isa = {0};
        err = hsa_agent_get_info(device, HSA_AGENT_INFO_ISA, &isa);
        check("Get hsa agent info isa", err);

	hsa_ext_control_directives_t control_directives;
	memset(&control_directives, 0, sizeof(hsa_ext_control_directives_t));

	err = hsa_ext_program_finalize(hsa_program,
			isa,
			0,
			control_directives,
			NULL, //"-g -O0 -dump-isa",
			HSA_CODE_OBJECT_TYPE_PROGRAM,
			&code_object);
	check("Error in finalizing program object", err);

	// Create executable.
	err = hsa_executable_create(HSA_PROFILE_FULL, HSA_EXECUTABLE_STATE_UNFROZEN, "", &hsaExecutable);
	check("Error in creating executable object", err);

	// Load code object.
	err = hsa_executable_load_code_object(hsaExecutable, device, code_object, "");
	check("Error in loading executable object", err);

	// Freeze executable.
	err = hsa_executable_freeze(hsaExecutable, "");
	check("Error in freezing executable object", err);

	// Get symbol handle.
	hsa_executable_symbol_t kernelSymbol;
	err = hsa_executable_get_symbol(hsaExecutable, NULL,  hsa_kernel_name, device, 0, &kernelSymbol);
	check("get symbol handle", err);

	// Get code handle.
	
	err = hsa_executable_symbol_get_info(kernelSymbol, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT, &codeHandle);
	check("Get code handle", err);

#ifdef TIME
	base_timer.StopTimer(base_setup_time_idx);
#endif


	//hsa_region_t local_kernarg_region;
	kernarg_region.handle = 0;
	hsa_agent_iterate_regions(device, get_kernarg, &kernarg_region);
	err = (kernarg_region.handle== 0) ? HSA_STATUS_ERROR : HSA_STATUS_SUCCESS;
	check(Finding a kernarg memory region, err);

	return true;
}

double HSA_UTIL::Run(int dim, int group_x, int group_y, int group_z, int s_size, int grid_x, int grid_y, int grid_z, void* kernel_args, int kernel_args_size)
{
#ifdef TIME
		base_timer.StartTimer(base_kernel_time_idx);
#endif

	/*
	 * Create a signal to wait for the dispatch to finish.
	 */
	hsa_signal_t local_signal;
	err=hsa_signal_create(1, 0, NULL, &local_signal);
	check(Creating a HSA_UTIL signal, err);

	/* Initialize the dispatch packet */
	hsa_kernel_dispatch_packet_t local_dispatch_packet;
	memset(&local_dispatch_packet, 0, sizeof(hsa_kernel_dispatch_packet_t));
	/*
	 * Setup the dispatch information.
	 */
	local_dispatch_packet.completion_signal=local_signal;
	local_dispatch_packet.setup |=  dim<< HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;
	local_dispatch_packet.workgroup_size_x = group_x;
	local_dispatch_packet.workgroup_size_y = group_y;
	local_dispatch_packet.workgroup_size_z = group_z;
	local_dispatch_packet.group_segment_size = s_size;
	local_dispatch_packet.grid_size_x = grid_x;
	local_dispatch_packet.grid_size_y = grid_y;
	local_dispatch_packet.grid_size_z = grid_z;
	local_dispatch_packet.header |= HSA_PACKET_TYPE_KERNEL_DISPATCH;
	local_dispatch_packet.header |= HSA_FENCE_SCOPE_AGENT << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE;
	local_dispatch_packet.header |= HSA_FENCE_SCOPE_AGENT << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE;
	local_dispatch_packet.kernel_object = codeHandle;
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/*
	 * Find a memory region that supports kernel arguments.
	 */


/*
	kernarg_region.handle = 0;

	hsa_agent_iterate_regions(device, get_kernarg, &kernarg_region);
	err = (kernarg_region.handle== 0) ? HSA_STATUS_ERROR : HSA_STATUS_SUCCESS;
	check(Finding a kernarg memory region, err);
	
*/

        void* local_kernel_arg_buffer = NULL;
	/*
	 * Allocate the kernel argument buffer from the correct region.
	 */
	err = hsa_memory_allocate(kernarg_region, kernel_args_size, &local_kernel_arg_buffer);
	check(Allocating kernel argument memory buffer, err);
	memcpy(local_kernel_arg_buffer, kernel_args, kernel_args_size);
	local_dispatch_packet.kernarg_address = local_kernel_arg_buffer;

	/*	
	 * Obtain the current queue write index.
	 */
	uint64_t index = hsa_queue_load_write_index_relaxed(command_queue);

	/*	
	 * Write the aql packet at the calculated queue index address.
	 */
	const uint32_t queueMask = command_queue->size - 1;
	((hsa_kernel_dispatch_packet_t*)(command_queue->base_address))[index&queueMask]=local_dispatch_packet;

	/*	
	 * Increment the write index and ring the doorbell to dispatch the kernel.
	 */
	hsa_queue_store_write_index_relaxed(command_queue, index+1);
	hsa_signal_store_release(command_queue->doorbell_signal, index);

	/*	
	 * Wait on the dispatch signal until all kernel are finished.
	 */
	while (hsa_signal_wait_acquire(local_signal, HSA_SIGNAL_CONDITION_EQ, 0, UINT64_MAX, HSA_WAIT_STATE_ACTIVE)  != 0);

#ifdef TIME
	base_timer.StopTimer(base_kernel_time_idx);
#endif

	/*
	 * Cleanup all allocated resources.
	 */

        err = hsa_memory_free(local_kernel_arg_buffer);
        check(Deallocate memory, err);

	err=hsa_signal_destroy(local_signal);
	check(Destroying the local_signal, err);

	return 0;
}

double HSA_UTIL::GetKernelTime()
{
    return base_timer.ReadTimer(base_kernel_time_idx);
}

double HSA_UTIL::GetSetupTime()
{
    return base_timer.ReadTimer(base_setup_time_idx);
}

void HSA_UTIL::Close()
{
	err = hsa_executable_destroy(hsaExecutable); 
	check(Destroying the hsaExecutable, err)

	err = hsa_code_object_destroy(code_object);
	check(Destroying the code_object, err);

	err=hsa_queue_destroy(command_queue);
	check(Destroying the queue, err);

	err=hsa_shut_down();
	check(Shutting down the runtime, err);
}

