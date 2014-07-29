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
#include "fmm.h"

static const char kfd_device_name[] = "/dev/kfd";

HSAKMT_STATUS
HSAKMTAPI
hsaKmtOpenKFD(void)
{
	HSAKMT_STATUS result;

	pthread_mutex_lock(&hsakmt_mutex);

	if (kfd_open_count == 0)
	{
		int fd = open(kfd_device_name, O_RDWR | O_CLOEXEC);

		if (fd != -1)
		{
			kfd_fd = fd;
			kfd_open_count = 1;

			result = fmm_init_process_apertures();
			if (result != HSAKMT_STATUS_SUCCESS)
				close(fd);
		}
		else
		{
			result = HSAKMT_STATUS_KERNEL_IO_CHANNEL_NOT_OPENED;
		}
	}
	else
	{
		kfd_open_count++;
		result = HSAKMT_STATUS_SUCCESS;
	}

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
			close(kfd_fd);
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

extern int kfd_ioctl(int cmdcode, void* data)
{
	return ioctl(kfd_fd, cmdcode, data);
}
