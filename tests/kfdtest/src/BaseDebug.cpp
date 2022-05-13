/*
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "BaseDebug.hpp"
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/kfd_ioctl.h>
#include <fcntl.h>
#include "unistd.h"

BaseDebug::BaseDebug(void) {
}

BaseDebug::~BaseDebug(void) {
    /*
     * If the process is still attached, close and destroy the polling file
     * descriptor.  Note that on process termination, the KFD automatically
     * disables processes that are still runtime enabled and debug enabled
     * so we don't do it here.
     */
    if (m_Pid) {
        close(m_Fd.fd);
        unlink(m_Fd_Name);
    }
}

// Creates temp file descriptor and debug attaches.
HSAKMT_STATUS BaseDebug::Attach(struct kfd_runtime_info *rInfo,
                                int rInfoSize,
                                unsigned int pid,
                                uint64_t exceptionEnable) {
    struct kfd_ioctl_dbg_trap_args args = {0};
    char fd_name[32];

    memset(&args, 0x00, sizeof(args));

    mkfifo(m_Fd_Name, 0666);
    m_Fd.fd = open(m_Fd_Name, O_CLOEXEC | O_NONBLOCK | O_RDWR);
    m_Fd.events = POLLIN | POLLRDNORM;

    args.pid = pid;
    args.op = KFD_IOC_DBG_TRAP_ENABLE;
    args.enable.rinfo_ptr = (uint64_t)rInfo;
    args.enable.rinfo_size = rInfoSize;
    args.enable.dbg_fd = m_Fd.fd;
    args.enable.exception_mask = exceptionEnable;

    if (hsaKmtDebugTrapIoctl(&args, NULL)) {
        close(m_Fd.fd);
        unlink(m_Fd_Name);
        return HSAKMT_STATUS_ERROR;
    }

    m_Pid = pid;

    return HSAKMT_STATUS_SUCCESS;
}


void BaseDebug::Detach(void) {
    struct kfd_ioctl_dbg_trap_args args = {0};

    memset(&args, 0x00, sizeof(args));

    args.pid = m_Pid;
    args.op = KFD_IOC_DBG_TRAP_DISABLE;

    hsaKmtDebugTrapIoctl(&args, NULL);

    close(m_Fd.fd);
    unlink(m_Fd_Name);

    m_Pid = 0;
    m_Fd.fd = 0;
    m_Fd.events = 0;
}

HSAKMT_STATUS BaseDebug::SendRuntimeEvent(uint64_t exceptions, int gpuId, int queueId)
{
    struct kfd_ioctl_dbg_trap_args args = {0};

    memset(&args, 0x00, sizeof(args));

    args.pid = m_Pid;
    args.op = KFD_IOC_DBG_TRAP_SEND_RUNTIME_EVENT;
    args.send_runtime_event.exception_mask = exceptions;
    args.send_runtime_event.gpu_id = gpuId;
    args.send_runtime_event.queue_id = queueId;

    return hsaKmtDebugTrapIoctl(&args, NULL);
}

HSAKMT_STATUS BaseDebug::QueryDebugEvent(uint64_t *exceptions,
                                         uint32_t *gpuId, uint32_t *queueId,
                                         int timeoutMsec)
{
    struct kfd_ioctl_dbg_trap_args args = {0};
    HSAKMT_STATUS result;
    int r = poll(&m_Fd, 1, timeoutMsec);

    if (r > 0) {
        char tmp[r];

        read(m_Fd.fd, tmp, sizeof(tmp));
    } else {
        return HSAKMT_STATUS_ERROR;
    }

    memset(&args, 0x00, sizeof(args));

    args.pid = m_Pid;
    args.op = KFD_IOC_DBG_TRAP_QUERY_DEBUG_EVENT;
    args.query_debug_event.exception_mask = *exceptions;

    result = hsaKmtDebugTrapIoctl(&args, NULL);

    *exceptions = args.query_debug_event.exception_mask;

    if (gpuId)
        *gpuId = args.query_debug_event.gpu_id;

    if (queueId)
        *queueId = args.query_debug_event.queue_id;

    return result;
}
