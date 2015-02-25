#include "hsa_base.h"

void HSA::SetBrigFileAndKernelName(char * brig_file_name, char *kernel_name)
{
	strcpy(hsa_brig_file_name, brig_file_name);
	strcpy(hsa_kernel_name, kernel_name);
}

HSA::HSA()
{

}

HSA::~HSA()
{

}


bool HSA::HsaInit()
{
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

	return true;
}

double HSA::Run(int dim, int group_x, int group_y, int group_z, int s_size, int grid_x, int grid_y, int grid_z, void* kernel_args, int kernel_args_size)
{
	hsa_queue_t* local_command_queue;
	/*  
	 * Create a queue using the maximum size.
	 */
	err = hsa_queue_create(device, queue_size, HSA_QUEUE_TYPE_MULTI, NULL, NULL, 0, 0, &local_command_queue);
	check(Creating the queue, err);

	/*  
	 * Load BRIG, encapsulated in an ELF container, into a BRIG module.
	 */
	//char file_name[128] = "transpose_kernel.brig";
	hsa_ext_brig_module_t* local_brig_module;
	err = (hsa_status_t)create_brig_module_from_brig_file(hsa_brig_file_name, &local_brig_module);
	check(Creating the brig module from vector_copy.brig, err);

	/*  
	 * Create hsa program.
	 */
	hsa_ext_program_handle_t local_hsa_program;
	err = hsa_ext_program_create(&device, 1, HSA_EXT_BRIG_MACHINE_LARGE, HSA_EXT_BRIG_PROFILE_FULL, &local_hsa_program);
	check(Creating the hsa program, err);

	/*  
	 * Add the BRIG module to hsa program.
	 */
	hsa_ext_brig_module_handle_t local_module;
	err = hsa_ext_add_module(local_hsa_program, local_brig_module, &local_module);
	check(Adding the local brig module to the program, err);

	/*  
	 * Construct finalization request list.
	 */
	hsa_ext_finalization_request_t local_finalization_request_list;
	local_finalization_request_list.module = local_module;
	local_finalization_request_list.program_call_convention = 0;
	//char kernel_name[128] = "&__OpenCL_matrixTranspose_kernel";
	err = find_symbol_offset(local_brig_module, hsa_kernel_name, &local_finalization_request_list.symbol);
	check(Finding the symbol offset for the kernel, err);

	/*  
	 * Finalize the hsa program.
	 */
	err = hsa_ext_finalize_program(local_hsa_program, device, 1, &local_finalization_request_list, NULL, NULL, 0, NULL, 0); 
	check(Finalizing the program, err);

	/*  
	 * Destroy the brig module. The program was successfully created the kernel
	 * symbol was found and the program was finalized, so it is no longer needed.
	 */
	destroy_brig_module(local_brig_module);

	/*  
	 *  Get the hsa code descriptor address.
	 */
	hsa_ext_code_descriptor_t *local_hsa_code_descriptor;
	err = hsa_ext_query_kernel_descriptor_address(local_hsa_program, local_module, local_finalization_request_list.symbol, &local_hsa_code_descriptor);
	check(Querying the kernel descriptor address, err);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/*
	 * Create a signal to wait for the dispatch to finish.
	 */
	hsa_signal_t local_signal;
	err=hsa_signal_create(1, 0, NULL, &local_signal);
	check(Creating a HSA signal, err);


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
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/*
	 * Find a memory region that supports kernel arguments.
	 */
	hsa_region_t local_kernarg_region;
	local_kernarg_region.handle = 0;

	hsa_agent_iterate_regions(device, get_kernarg, &local_kernarg_region);
	err = (local_kernarg_region.handle== 0) ? HSA_STATUS_ERROR : HSA_STATUS_SUCCESS;
	check(Finding a kernarg memory region, err);
	void* local_kernel_arg_buffer = NULL;

	size_t local_kernel_arg_buffer_size = local_hsa_code_descriptor->kernarg_segment_byte_size;

	/*
	 * Allocate the kernel argument buffer from the correct region.
	 */
	err = hsa_memory_allocate(local_kernarg_region, local_kernel_arg_buffer_size, &local_kernel_arg_buffer);
	check(Allocating kernel argument memory buffer, err);
	memcpy(local_kernel_arg_buffer, kernel_args, kernel_args_size);

	local_dispatch_packet.kernel_object = local_hsa_code_descriptor->code.handle;
	// Assume our kernel receives no arguments
	local_dispatch_packet.kernarg_address = local_kernel_arg_buffer;

	/*
	 * Register the memory region for the argument buffer.
	 */
	err = hsa_memory_register(kernel_args, kernel_args_size);
	
	check(Registering the argument buffer, err);

	/*	
	 * Obtain the current queue write index.
	 */
	uint64_t index = hsa_queue_load_write_index_relaxed(local_command_queue);

	/*	
	 * Write the aql packet at the calculated queue index address.
	 */
	const uint32_t queueMask = local_command_queue->size - 1;
	((hsa_kernel_dispatch_packet_t*)(local_command_queue->base_address))[index&queueMask]=local_dispatch_packet;

	/*	
	 * Increment the write index and ring the doorbell to dispatch the kernel.
	 */
	hsa_queue_store_write_index_relaxed(local_command_queue, index+1);
	
#ifdef TIME
	 PerfTimer perf_timer_0;
	 int timer_idx_0 = perf_timer_0.CreateTimer();
        perf_timer_0.StartTimer(timer_idx_0);
#endif
	hsa_signal_store_release(local_command_queue->doorbell_signal, index);

	/*	
	 * Wait on the dispatch signal until all kernel are finished.
	 */
	while (hsa_signal_wait_acquire(local_signal, HSA_SIGNAL_CONDITION_EQ, 0, UINT64_MAX, HSA_WAIT_STATE_ACTIVE)  != 0);

#ifdef TIME
	perf_timer_0.StopTimer(timer_idx_0);
#endif
	/*
	 * Cleanup all allocated resources.
	 */

	err=hsa_signal_destroy(local_signal);
	check(Destroying the local_signal, err);

	err=hsa_ext_program_destroy(local_hsa_program);
	check(Destroying the program, err);

	err=hsa_queue_destroy(local_command_queue);
	check(Destroying the queue, err);

#ifdef TIME
	double ret = perf_timer_0.ReadTimer(timer_idx_0);
#endif

	return 0;

}


void HSA::Close()
{
	err=hsa_shut_down();
	check(Shutting down the runtime, err);
}

