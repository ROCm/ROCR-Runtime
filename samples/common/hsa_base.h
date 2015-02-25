#ifndef __HSA_BASE__
#define __HSA_BASE__


#include <vector>
#include "hsa.h"
#include "hsa_ext_finalize.h"
#include "elf_utils.h"
#include "hsatimer.h"
#include "utilities.h"

class HSA{
    public:
	    HSA();
	    ~HSA();

	public:
	    void SetBrigFileAndKernelName(char *brig_file_name, char *kernel_name);
	    bool HsaInit();
        void Close();
	double Run(int dim, int group_x, int group_y, int group_z, int s_size, int grid_x, int grid_y, int grid_z, void* kernel_args, int kernel_args_size);

	public:
		hsa_status_t err;
		uint32_t queue_size;
		hsa_agent_t device;

		char hsa_brig_file_name[128];
		char hsa_kernel_name[128];

		hsa_queue_t* command_queue;
		hsa_signal_t hsa_signal;
		hsa_ext_brig_module_t* brig_module;
		hsa_ext_brig_module_handle_t module;
		hsa_ext_program_handle_t hsa_program;
		hsa_ext_code_descriptor_t *hsa_code_descriptor;
		hsa_kernel_dispatch_packet_t dispatch_packet; // needs to be set manually each time	
		hsa_region_t hsa_kernarg_region;
};


#endif

