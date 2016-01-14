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
#include "linux/kfd_ioctl.h"
#include <stdlib.h>
#include <string.h>

static bool *is_device_debugged;
int debug_get_reg_status(uint32_t node_id, bool* is_debugged);

HSAKMT_STATUS init_device_debugging_memory(unsigned int NumNodes)
{
	unsigned int i;

	is_device_debugged = malloc(NumNodes * sizeof(bool));
	if (is_device_debugged == NULL)
		return HSAKMT_STATUS_NO_MEMORY;

	for (i = 0; i < NumNodes; i++)
		is_device_debugged[i] = false;

	return HSAKMT_STATUS_SUCCESS;
}

void destroy_device_debugging_memory(void)
{
	if (is_device_debugged)
		free(is_device_debugged);
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtDbgRegister(
    HSAuint32       NodeId      //IN
    )
{
	HSAKMT_STATUS result;
	uint32_t gpu_id;
	CHECK_KFD_OPEN();

	if (is_device_debugged == NULL)
		return HSAKMT_STATUS_NO_MEMORY;

	result = validate_nodeid(NodeId, &gpu_id);
	if (result != HSAKMT_STATUS_SUCCESS)
		return result;

	struct kfd_ioctl_dbg_register_args args;
	memset(&args, 0, sizeof(args));
	args.gpu_id = gpu_id;
	long  err = kmtIoctl(kfd_fd, AMDKFD_IOC_DBG_REGISTER, &args);

	if (err == 0) {
		is_device_debugged[NodeId] = true;
		result = HSAKMT_STATUS_SUCCESS;
	} else
		result = HSAKMT_STATUS_ERROR;

	return (result);
}

/* =============================================================================== */

HSAKMT_STATUS
HSAKMTAPI
hsaKmtDbgUnregister(
    HSAuint32       NodeId      //IN
    )
{
	HSAKMT_STATUS result;
	uint32_t gpu_id;
	CHECK_KFD_OPEN();

	if (is_device_debugged == NULL)
		return HSAKMT_STATUS_NO_MEMORY;

	result = validate_nodeid(NodeId, &gpu_id);
	if (result != HSAKMT_STATUS_SUCCESS)
		return result;

	struct kfd_ioctl_dbg_unregister_args args;
	memset(&args, 0, sizeof(args));
	args.gpu_id = gpu_id;
	long  err = kmtIoctl(kfd_fd, AMDKFD_IOC_DBG_UNREGISTER, &args);
	if (err == 0) {
		is_device_debugged[NodeId] = false;
		result = HSAKMT_STATUS_SUCCESS;
	} else
		result = HSAKMT_STATUS_ERROR;

	return (result);
}

/* =============================================================================== */

HSAKMT_STATUS
HSAKMTAPI
hsaKmtDbgWavefrontControl(
    HSAuint32           NodeId,         //IN
    HSA_DBG_WAVEOP      Operand,        //IN
    HSA_DBG_WAVEMODE    Mode,           //IN
    HSAuint32           TrapId,         //IN
    HsaDbgWaveMessage*  DbgWaveMsgRing  //IN (? - see thunk API doc!)
    )
{
	HSAKMT_STATUS result;
	uint32_t gpu_id;

	struct kfd_ioctl_dbg_wave_control_args *args;

	CHECK_KFD_OPEN();

	result = validate_nodeid(NodeId, &gpu_id);
	if (result != HSAKMT_STATUS_SUCCESS)
		return result;


/* Determine Size  of the ioctl buffer */

	uint32_t buff_size = sizeof(Operand)+
					 	 sizeof(Mode)  + sizeof(TrapId) +
						 sizeof(DbgWaveMsgRing->DbgWaveMsg)+ sizeof(DbgWaveMsgRing->MemoryVA) + sizeof(*args);


	args = (struct kfd_ioctl_dbg_wave_control_args*) malloc(buff_size);
	if (args == NULL)
	{
		return HSAKMT_STATUS_ERROR;
	}

	memset(args, 0, buff_size);

	args->gpu_id = gpu_id;
	args->buf_size_in_bytes = buff_size;

	/* increment pointer to the start of the non fixed part */

	unsigned char* run_ptr = (unsigned char*)args + sizeof(*args);

	/* save variable content pointer for kfd */
	args->content_ptr = (void *)run_ptr;

	/* insert items, and increment pointer accordingly */

	*((HSA_DBG_WAVEOP*)run_ptr)   =  Operand;
	run_ptr += sizeof(Operand);


	*((HSA_DBG_WAVEMODE*)run_ptr) =  Mode;
	run_ptr += sizeof(Mode);


	*((HSAuint32*)run_ptr)       =  TrapId;
	run_ptr += sizeof(TrapId);

	*((HsaDbgWaveMessageAMD*)run_ptr)       =  DbgWaveMsgRing->DbgWaveMsg;
		run_ptr += sizeof(DbgWaveMsgRing->DbgWaveMsg);

	*((void**)run_ptr)       =  DbgWaveMsgRing->MemoryVA;
	run_ptr += sizeof(DbgWaveMsgRing->MemoryVA);

	/* send to kernel */

	long err = kmtIoctl(kfd_fd, AMDKFD_IOC_DBG_WAVE_CONTROL, args);

	free (args);

	if (err == 0)
		return HSAKMT_STATUS_SUCCESS;
	else
		return HSAKMT_STATUS_ERROR;
}


/* =============================================================================== */

HSAKMT_STATUS
HSAKMTAPI
hsaKmtDbgAddressWatch(
    HSAuint32           NodeId,         //IN
    HSAuint32           NumWatchPoints, //IN
    HSA_DBG_WATCH_MODE  WatchMode[],    //IN
    void*               WatchAddress[], //IN
    HSAuint64           WatchMask[],    //IN, optional
    HsaEvent*           WatchEvent[]    //IN, optional
    )
{
	HSAKMT_STATUS result;
	uint32_t gpu_id;

	/* determine the size of the watch mask and event buffers
	 * the value is NULL if and only if  no vector data should be attached
	 *
	 */

	uint32_t watch_mask_items  = WatchMask[0] > 0      ? NumWatchPoints:1;
	uint32_t watch_event_items = WatchEvent != NULL    ? NumWatchPoints:0;

	struct kfd_ioctl_dbg_address_watch_args *args;
	HSAuint32		 i = 0;

	CHECK_KFD_OPEN();

	result = validate_nodeid(NodeId, &gpu_id);
	if (result != HSAKMT_STATUS_SUCCESS)
		return result;

	if (NumWatchPoints > MAX_ALLOWED_NUM_POINTS)
		return HSAKMT_STATUS_INVALID_PARAMETER;

/* Size and structure of the ioctl buffer is  dynamic in this case
 * Here we calculate the buff size.
 */

	uint32_t buff_size =sizeof(NumWatchPoints)+
					(	sizeof(WatchMode[0]) +
						sizeof(WatchAddress[0]))*NumWatchPoints +
						watch_mask_items*sizeof(HSAuint64) +
						watch_event_items*sizeof(HsaEvent*)+
						sizeof(*args);


	args = (struct kfd_ioctl_dbg_address_watch_args*) malloc(buff_size);
	if (args == NULL)
	{
		return HSAKMT_STATUS_ERROR;
	}

	memset(args, 0, buff_size);

	args->gpu_id = gpu_id;
	args->buf_size_in_bytes = buff_size;


	/* increment pointer to the start of the non fixed part */

	unsigned char* run_ptr = (unsigned char*)args + sizeof(*args);

	/* save variable content pointer for kfd */
	args->content_ptr = (void *)run_ptr;
	/* insert items, and increment pointer accordingly */

	*((HSAuint32*)run_ptr) =  NumWatchPoints;
	run_ptr += sizeof(NumWatchPoints);

	for (i=0; i < NumWatchPoints; i++)
	{
		*((HSA_DBG_WATCH_MODE*)run_ptr) 	=  WatchMode[i];
		run_ptr 	+= sizeof(WatchMode[i]);
	}

	for (i=0; i < NumWatchPoints; i++)
	{
		*((void**)run_ptr) 	=  WatchAddress[i];
		run_ptr 	+= sizeof(WatchAddress[i]);
	}

	for (i=0; i < watch_mask_items; i++)
	{
		*((HSAuint64*)run_ptr) 	=  WatchMask[i];
		run_ptr 	+= sizeof(WatchMask[i]);
	}

	for (i=0; i < watch_event_items; i++)
	{
		*((HsaEvent**)run_ptr) 	=  WatchEvent[i];
		run_ptr 	+= sizeof(WatchEvent[i]);
	}

	/* send to kernel */

	long err = kmtIoctl(kfd_fd, AMDKFD_IOC_DBG_ADDRESS_WATCH, args);

	free (args);

	if (err == 0)
	{
		return HSAKMT_STATUS_SUCCESS;
	}
	else
	{
		return HSAKMT_STATUS_ERROR;
	}
}

/* =============================================================================== */
int debug_get_reg_status(uint32_t node_id, bool* is_debugged)
{
	*is_debugged = NULL;
	if (is_device_debugged == NULL)
		return -1;
	else  {
		*is_debugged = is_device_debugged[node_id];
		return 0;
	}
}
