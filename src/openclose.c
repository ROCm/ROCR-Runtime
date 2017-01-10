/*
 * Copyright © 2014 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including
 * the next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "libhsakmt.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "fmm.h"

static const char kfd_device_name[] = "/dev/kfd";
static const char tmp_file[] = "/var/lock/.amd_hsa_thunk_lock";
int amd_hsa_thunk_lock_fd = 0;
static pid_t parent_pid = -1;


static bool is_forked_child(void)
{
	pid_t cur_pid = getpid();

	if (parent_pid == -1) {
		parent_pid = cur_pid;
		return false;
	}

	if (parent_pid != cur_pid)
		return true;

	return false;
}

/* Call this from the child process after fork. This will clear all
 * data that is duplicated from the parent process, that is not valid
 * in the child.
 * The topology information is duplicated from the parent is valid
 * in the child process so it is not cleared
 */
static void clear_after_fork(void)
{
	clear_process_doorbells();
	clear_events_page();
	fmm_clear_all_mem();
	destroy_device_debugging_memory();
	kfd_open_count = 0;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtOpenKFD(void)
{
	HSAKMT_STATUS result;
	int fd;
	HsaSystemProperties sys_props;

	pthread_mutex_lock(&hsakmt_mutex);

	/* If the process has forked, the child process must re-initialize
	 * it's connection to KFD. Any references tracked by kfd_open_count
	 * belong to the parent
	 */
	if (is_forked_child())
		clear_after_fork();

	if (kfd_open_count == 0)
	{
		fd = open(kfd_device_name, O_RDWR | O_CLOEXEC);

		if (fd != -1) {
			kfd_fd = fd;
			kfd_open_count = 1;
		} else {
			result = HSAKMT_STATUS_KERNEL_IO_CHANNEL_NOT_OPENED;
			goto open_failed;
		}

		result = topology_sysfs_get_system_props(&sys_props);
		if (result != HSAKMT_STATUS_SUCCESS)
			goto topology_sysfs_failed;

		result = fmm_init_process_apertures(sys_props.NumNodes);
		if (result != HSAKMT_STATUS_SUCCESS)
			goto init_process_aperture_failed;

		result = init_process_doorbells(sys_props.NumNodes);
		if (result != HSAKMT_STATUS_SUCCESS)
			goto init_doorbell_failed;

		if (init_device_debugging_memory(sys_props.NumNodes) != HSAKMT_STATUS_SUCCESS)
			printf("Insufficient Memory. Debugging unavailable\n");

		if (init_counter_props(sys_props.NumNodes) != HSAKMT_STATUS_SUCCESS)
			printf("Insufficient Memory. Performance Counter information unavailable\n");

		amd_hsa_thunk_lock_fd = open(tmp_file,
				O_CREAT | //create the file if it's not present.
				O_RDWR, //only need write access for the internal locking semantics.
				S_IRUSR | S_IWUSR); //permissions on the file, 600 here.
	}
	else
	{
		kfd_open_count++;
		result = HSAKMT_STATUS_SUCCESS;
	}

	pthread_mutex_unlock(&hsakmt_mutex);
	return result;

init_doorbell_failed:
	fmm_destroy_process_apertures();
init_process_aperture_failed:
topology_sysfs_failed:
	close(fd);
open_failed:
	pthread_mutex_unlock(&hsakmt_mutex);

	return result;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtCloseKFD(void)
{
	HSAKMT_STATUS result;

	pthread_mutex_lock(&hsakmt_mutex);

	if (kfd_open_count > 0)
	{
		if (--kfd_open_count == 0)
		{
			destroy_counter_props();
			destroy_device_debugging_memory();
			destroy_process_doorbells();
			fmm_destroy_process_apertures();
			close(kfd_fd);

			if (amd_hsa_thunk_lock_fd > 0) {
				close(amd_hsa_thunk_lock_fd);
				unlink(tmp_file);
			}

		}

		result = HSAKMT_STATUS_SUCCESS;
	}
	else
	{
		result = HSAKMT_STATUS_KERNEL_IO_CHANNEL_NOT_OPENED;
	}

	pthread_mutex_unlock(&hsakmt_mutex);

	return result;
}
