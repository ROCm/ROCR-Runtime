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

#ifndef _HSAKMT_H_
#define _HSAKMT_H_

#include "hsakmttypes.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
  "Opens" the HSA kernel driver for user-kernel mode communication.

  On Windows, this function gets a handle to the KFD's AMDKFDIO device object that
  is responsible for user-kernel communication, this handle is used internally by
  the thunk library to send device I/O control to the HSA kernel driver.
  No other thunk library function may be called unless the user-kernel communication
  channel is opened first.

  On Linux this call opens the "/dev/kfd" device file to establish a communication
  path to the kernel.
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtOpenKFD( void );

/**
  "Closes" the user-kernel communication path.

  On Windows, the handle obtained by the hsaKmtOpenKFD() function is closed;
  no other communication with the kernel driver is possible after the successful
  execution of the saKmdCloseKFD() function. Depending on the failure reason,
  the user-kernel communication path may or may not be still active.

  On Linux the function closes the "dev/kfd" device file.
  No further communication to the kernel driver is allowed until hsaKmtOpenKFD()
  function is called again.
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtCloseKFD( void );


/**
  Returns the user-kernel interface version supported by KFD.
  Higher major numbers usually add new features to KFD and may break user-kernel
  compatibility; higher minor numbers define additional functionality associated
  within a major number.
  The calling software should validate that it meets the minimum interface version
  as described in the API specification.
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtGetVersion(
    HsaVersionInfo*  VersionInfo    //OUT
    );

/**
  The function takes a "snapshot" of the topology information within the KFD
  to avoid any changes during the enumeration process.
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtAcquireSystemProperties(
    HsaSystemProperties*  SystemProperties    //OUT
    );

/**
  Releases the topology "snapshot" taken by hsaKmtAcquireSystemProperties()
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtReleaseSystemProperties( void ) ;

/**
  Retrieves the discoverable sub-properties for a given HSA
  node. The parameters returned allow the application or runtime to size the
  management structures necessary to store the information.
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtGetNodeProperties(
    HSAuint32               NodeId,            //IN
    HsaNodeProperties*      NodeProperties     //OUT
    );

/**
  Retrieves the memory properties of a specific HSA node.
  the memory pointer passed as MemoryProperties is sized as
  NumBanks * sizeof(HsaMemoryProperties). NumBanks is retrieved with the
  hsaKmtGetNodeProperties() call.

  Some of the data returned is optional. Not all implementations may return all
  parameters in the hsaMemoryProperties.
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtGetNodeMemoryProperties(
    HSAuint32             NodeId,             //IN
    HSAuint32             NumBanks,           //IN
    HsaMemoryProperties*  MemoryProperties    //OUT
    );

/**
  Retrieves the cache properties of a specific HSA node and processor ID.
  ProcessorID refers to either a CPU core or a SIMD unit as enumerated earlier
  via the hsaKmtGetNodeProperties() call.
  The memory pointer passed as CacheProperties is sized as
  NumCaches * sizeof(HsaCacheProperties). NumCaches is retrieved with the
  hsaKmtGetNodeProperties() call.

  The data returned is optional. Not all implementations may return all
  parameters in the CacheProperties.
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtGetNodeCacheProperties(
    HSAuint32           NodeId,         //IN
    HSAuint32           ProcessorId,    //IN
    HSAuint32           NumCaches,      //IN
    HsaCacheProperties* CacheProperties //OUT
    );

/**
  Retrieves the HSA IO affinity properties of a specific HSA node.
  the memory pointer passed as Properties is sized as
  NumIoLinks * sizeof(HsaIoLinkProperties). NumIoLinks is retrieved with the
  hsaKmtGetNodeProperties() call.

  The data returned is optional. Not all implementations may return all
  parameters in the IoLinkProperties.
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtGetNodeIoLinkProperties(
    HSAuint32            NodeId,            //IN
    HSAuint32            NumIoLinks,        //IN
    HsaIoLinkProperties* IoLinkProperties  //OUT
    );



/**
  Creates an operating system event associated with a HSA event ID
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtCreateEvent(
    HsaEventDescriptor* EventDesc,              //IN
    bool                ManualReset,            //IN
    bool                IsSignaled,             //IN
    HsaEvent**          Event                   //OUT
    );

/**
  Destroys an operating system event associated with a HSA event ID
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtDestroyEvent(
    HsaEvent*   Event    //IN
    );

/**
  Sets the specified event object to the signaled state
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtSetEvent(
    HsaEvent*  Event    //IN
    );

/**
  Sets the specified event object to the non-signaled state
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtResetEvent(
    HsaEvent*  Event    //IN
    );

/**
  Queries the state of the specified event object
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtQueryEventState(
    HsaEvent*  Event    //IN
    );

/**
  Checks the current state of the event object. If the object's state is
  nonsignaled, the calling thread enters the wait state.

 The function returns when one of the following occurs:
- The specified event object is in the signaled state.
- The time-out interval elapses.
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtWaitOnEvent(
    HsaEvent*   Event,          //IN
    HSAuint32   Milliseconds    //IN
    );

/**
  Checks the current state of multiple event objects.

 The function returns when one of the following occurs:
- Either any one or all of the specified objects are in the signaled state
  - if "WaitOnAll" is "true" the function returns when the state of all
    objects in array is signaled
  - if "WaitOnAll" is "false" the function returns when the state of any
    one of the objects is set to signaled
- The time-out interval elapses.
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtWaitOnMultipleEvents(
    HsaEvent*   Events[],       //IN
    HSAuint32   NumEvents,      //IN
    bool        WaitOnAll,      //IN
    HSAuint32   Milliseconds    //IN
    );

/**
  new TEMPORARY function definition - to be used only on "Triniti + Southern Islands" platform
  If used on other platforms the function will return HSAKMT_STATUS_ERROR
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtReportQueue(
    HSA_QUEUEID     QueueId,        //IN
    HsaQueueReport* QueueReport     //OUT
    );

/**
  Creates a GPU queue with user-mode access rights
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtCreateQueue(
    HSAuint32           NodeId,           //IN
    HSA_QUEUE_TYPE      Type,             //IN
    HSAuint32           QueuePercentage,  //IN
    HSA_QUEUE_PRIORITY  Priority,         //IN
    void*               QueueAddress,     //IN
    HSAuint64           QueueSizeInBytes, //IN
    HsaEvent*           Event,            //IN
    HsaQueueResource*   QueueResource     //OUT
    );

/**
  Updates a queue
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtUpdateQueue(
    HSA_QUEUEID         QueueId,        //IN
    HSAuint32           QueuePercentage,//IN
    HSA_QUEUE_PRIORITY  Priority,       //IN
    void*               QueueAddress,   //IN
    HSAuint64           QueueSize,      //IN
    HsaEvent*           Event           //IN
    );

/**
  Destroys a queue
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtDestroyQueue(
    HSA_QUEUEID         QueueId         //IN
    );

/**
  Set cu mask for a queue
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtSetQueueCUMask(
    HSA_QUEUEID         QueueId,        //IN
    HSAuint32           CUMaskCount,    //IN
    HSAuint32*          QueueCUMask     //IN
    );

HSAKMT_STATUS
HSAKMTAPI
hsaKmtGetQueueInfo(
    HSA_QUEUEID QueueId,	//IN
    HsaQueueInfo *QueueInfo	//IN
);

/**
  Allows an HSA process to set/change the default and alternate memory coherency, before starting to dispatch. 
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtSetMemoryPolicy(
    HSAuint32       Node,                       //IN
    HSAuint32       DefaultPolicy,     	   	    //IN  
    HSAuint32       AlternatePolicy,       	    //IN  
    void*           MemoryAddressAlternate,     //IN (page-aligned)
    HSAuint64       MemorySizeInBytes   	    //IN (page-aligned)
    );
/**
  Allocates a memory buffer that may be accessed by the GPU
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtAllocMemory(
    HSAuint32       PreferredNode,          //IN
    HSAuint64       SizeInBytes,            //IN  (multiple of page size)
    HsaMemFlags     MemFlags,               //IN
    void**          MemoryAddress           //IN/OUT (page-aligned)
    );

/**
  Frees a memory buffer
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtFreeMemory(
    void*       MemoryAddress,      //IN (page-aligned)
    HSAuint64   SizeInBytes         //IN
    );

/**
  Registers with KFD a memory buffer that may be accessed by the GPU
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtRegisterMemory(
    void*       MemoryAddress,      //IN (cache-aligned)
    HSAuint64   MemorySizeInBytes   //IN (cache-aligned)
    );


/**
  Registers with KFD a memory buffer that may be accessed by specific GPUs
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtRegisterMemoryToNodes(
    void        *MemoryAddress,     // IN (cache-aligned)
    HSAuint64   MemorySizeInBytes,  // IN (cache-aligned)
    HSAuint64   NumberOfNodes,      // IN
    HSAuint32*  NodeArray           // IN
    );


/**
  Registers with KFD a memory buffer with memory attributes
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtRegisterMemoryWithFlags(
    void        *MemoryAddress,     // IN (cache-aligned)
    HSAuint64   MemorySizeInBytes,  // IN (cache-aligned)
    HsaMemFlags MemFlags            // IN
    );

/**
  Registers with KFD a graphics buffer and returns graphics metadata
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtRegisterGraphicsHandleToNodes(
    HSAuint64       GraphicsResourceHandle,        //IN
    HsaGraphicsResourceInfo *GraphicsResourceInfo, //OUT
    HSAuint64       NumberOfNodes,                 //IN
    HSAuint32*      NodeArray                      //IN
    );

/**
 Export a memory buffer for sharing with other processes

 NOTE: for the current revision of the thunk spec, SizeInBytes
 must match whole allocation.
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtShareMemory(
	void                  *MemoryAddress,     // IN
	HSAuint64             SizeInBytes,        // IN
	HsaSharedMemoryHandle *SharedMemoryHandle // OUT
);

/**
 Register shared memory handle
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtRegisterSharedHandle(
	const HsaSharedMemoryHandle *SharedMemoryHandle, // IN
	void                        **MemoryAddress,     // OUT
	HSAuint64                   *SizeInBytes         // OUT
);

/**
 Register shared memory handle to specific nodes only
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtRegisterSharedHandleToNodes(
	const HsaSharedMemoryHandle *SharedMemoryHandle, // IN
	void                        **MemoryAddress,     // OUT
	HSAuint64                   *SizeInBytes,        // OUT
	HSAuint64                   NumberOfNodes,       // OUT
	HSAuint32*                  NodeArray            // OUT
);

/**
 Copy data from the GPU address space of the process identified
 by Pid. Size Copied will return actual amount of data copied.
 If return is not SUCCESS, partial copies could have happened.
 */
HSAKMT_STATUS
HSAKMTAPI
hsaKmtProcessVMRead(
	HSAuint32                 Pid,                     // IN
	HsaMemoryRange            *LocalMemoryArray,       // IN
	HSAuint64                 LocalMemoryArrayCount,   // IN
	HsaMemoryRange            *RemoteMemoryArray,      // IN
	HSAuint64                 RemoteMemoryArrayCount,  // IN
	HSAuint64                 *SizeCopied              // OUT
);

/**
 Write data to the GPU address space of the process identified
 by Pid. See also hsaKmtProcessVMRead.
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtProcessVMWrite(
	HSAuint32                 Pid,                     // IN
	HsaMemoryRange            *LocalMemoryArray,       // IN
	HSAuint64                 LocalMemoryArrayCount,   // IN
	HsaMemoryRange            *RemoteMemoryArray,      // IN
	HSAuint64                 RemoteMemoryArrayCount,  // IN
	HSAuint64                 *SizeCopied              // OUT
);

/**
  Unregisters with KFD a memory buffer
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtDeregisterMemory(
    void*       MemoryAddress  //IN
    );


/**
  Ensures that the memory is resident and can be accessed by GPU
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtMapMemoryToGPU(
    void*           MemoryAddress,     //IN (page-aligned)
    HSAuint64       MemorySizeInBytes, //IN (page-aligned)
    HSAuint64*      AlternateVAGPU     //OUT (page-aligned)     
    );

/**
  Ensures that the memory is resident and can be accessed by GPUs
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtMapMemoryToGPUNodes(
    void*           MemoryAddress,         //IN (page-aligned)
    HSAuint64       MemorySizeInBytes,     //IN (page-aligned)
    HSAuint64*      AlternateVAGPU,        //OUT (page-aligned)
    HsaMemMapFlags  MemMapFlags,           //IN
    HSAuint64       NumberOfNodes,         //IN
    HSAuint32*      NodeArray              //IN
    );

/**
  Releases the residency of the memory
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtUnmapMemoryToGPU(
    void*           MemoryAddress       //IN (page-aligned)
    );


/**
  Notifies the kernel driver that a process wants to use GPU debugging facilities
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtMapGraphicHandle(
                HSAuint32          NodeId,                              //IN
                HSAuint64          GraphicDeviceHandle,                 //IN
                HSAuint64          GraphicResourceHandle,               //IN
                HSAuint64          GraphicResourceOffset,               //IN
                HSAuint64          GraphicResourceSize,                 //IN
                HSAuint64*         FlatMemoryAddress            //OUT
                );


/**
  Stub for Unmap Graphic Handle
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtUnmapGraphicHandle(
                HSAuint32          NodeId,                      //IN
                HSAuint64          FlatMemoryAddress,           //IN
                HSAuint64              SizeInBytes              //IN
                );

/**
  Allocate GWS resource for a queue
 */

HSAKMT_STATUS
HSAKMTAPI
hsaKmtAllocQueueGWS(
                HSA_QUEUEID        QueueId,                     //IN
                HSAuint32          nGWS,                        //IN
                HSAuint32          *firstGWS                    //OUT
                );

/**
  Notifies the kernel driver that a process wants to use GPU debugging facilities
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtDbgRegister(
    HSAuint32       NodeId      //IN
    );

/**
  Detaches the debugger process from the HW debug established by hsaKmtDbgRegister() API
*/

HSAKMT_STATUS 
HSAKMTAPI 
hsaKmtDbgUnregister(
    HSAuint32       NodeId      //IN
    );

/**
  Controls a wavefront
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtDbgWavefrontControl(
    HSAuint32           NodeId,         //IN
    HSA_DBG_WAVEOP      Operand,        //IN
    HSA_DBG_WAVEMODE    Mode,           //IN
    HSAuint32           TrapId,         //IN
    HsaDbgWaveMessage*  DbgWaveMsgRing  //IN
    );

/**
  Sets watch points on memory address ranges to generate exception events when the
  watched addresses are  accessed
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtDbgAddressWatch(
    HSAuint32           NodeId,         //IN
    HSAuint32           NumWatchPoints, //IN
    HSA_DBG_WATCH_MODE  WatchMode[],    //IN
    void*               WatchAddress[], //IN
    HSAuint64           WatchMask[],    //IN, optional
    HsaEvent*           WatchEvent[]    //IN, optional
    );

/**
   Suspend the execution of a set of queues. A queue that is suspended
   allows the wave context save state to be inspected and modified. If a
   queue is already suspended it remains suspended. A suspended queue
   can be resumed by hsaKmtDbgQueueResume().

   For each node that has a queue suspended, a sequentially consistent
   system scope release will be performed that synchronizes with a
   sequentially consistent system scope acquire performed by this
   call. This ensures any memory updates performed by the suspended
   queues are visible to the thread calling this operation.

   Pid is the process that owns the queues that are to be supended or
   resumed. If the value is -1 then the Pid of the process calling
   hsaKmtQueueSuspend or hsaKmtQueueResume is used.

   NumQueues is the number of queues that are being requested to
   suspend or resume.

   Queues is a pointer to an array with NumQueues entries of
   HSA_QUEUEID. The queues in the list must be for queues that exist
   for Pid, and can be a mixture of queues for different nodes.

   GracePeriod to wait after initialiating context save before forcing
   waves to context save. A value of 0 indicates no grace period.
   It is ignored by hsaKmtQueueResume.

   Flags is a bit set of the values defined by HSA_DBG_NODE_CONTROL.
   Returns:
    - HSAKMT_STATUS_SUCCESS if successful.
    - HSAKMT_STATUS_INVALID_HANDLE if any QueueId is invalid for Pid.
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtQueueSuspend(
        HSAuint32    Pid,          // IN
        HSAuint32    NumQueues,    // IN
        HSA_QUEUEID *Queues,       // IN
        HSAuint32    GracePeriod,  // IN
        HSAuint32    Flags);       // IN

/**
   Resume the execution of a set of queues. If a queue is not
   suspended by hsaKmtDbgQueueSuspend() then it remains executing. Any
   changes to the wave state data will be used when the waves are
   restored. Changes to the control stack data will have no effect.

   For each node that has a queue resumed, a sequentially consistent
   system scope release will be performed that synchronizes with a
   sequentially consistent system scope acquire performed by all
   queues being resumed. This ensures any memory updates performed by
   the thread calling this operation are visible to the resumed
   queues.

   For each node that has a queue resumed, the instruction cache will
   be invalidated. This ensures any instruction code updates performed
   by the thread calling this operation are visible to the resumed
   queues.

   Pid is the process that owns the queues that are to be supended or
   resumed. If the value is -1 then the Pid of the process calling
   hsaKmtQueueSuspend or hsaKmtQueueResume is used.

   NumQueues is the number of queues that are being requested to
   suspend or resume.

   Queues is a pointer to an array with NumQueues entries of
   HSA_QUEUEID. The queues in the list must be for queues that exist
   for Pid, and can be a mixture of queues for different nodes.

   Flags is a bit set of the values defined by HSA_DBG_NODE_CONTROL.
   Returns:
    - HSAKMT_STATUS_SUCCESS if successful
    - HSAKMT_STATUS_INVALID_HANDLE if any QueueId is invalid.
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtQueueResume(
            HSAuint32   Pid,         // IN
            HSAuint32   NumQueues,   // IN
            HSA_QUEUEID *Queues,     // IN
            HSAuint32   Flags);      // IN

/**
  Enable debug trap for NodeId. If QueueId is INVALID_QUEUEID then
  enable for all queues on NodeId, otherwise enable only for QueueId.
  Return file descriptor PollFd where on poll wake, fd has readable
  FIFO data for pending debug events.

  When debug trap is enabled the trap handler behavior changes
  depending on architecture of the node and can include the following:

  - Initialize Trap Temp Registers: All new waves are launched with
    specific trap temp registers initialized with:

    - HSA dispatch packet address of the wave.

    - X, Y, Z grid and work-group position of the wave within the
      dispatch.

    - The scratch backing memory address.

  - Enable wave launch trap override. hsaKmtEnableDebugTrap() sets the
    TrapMask to 0 and the TrapOverride to HSA_DBG_TRAP_OVERRIDE_OR and
    they can be changed by hsaKmtSetWaveLaunchTrapOverride().

  If debug trap is already enabled for NodeId, any features controlled
  by it are still reset to their default values as defined above.

  Returns:
    - HSAKMT_STATUS_SUCCESS if successful.

    - HSAKMT_STATUS_INVALID_HANDLE if:

      - NodeId is invalid

      - QueueId is not INVALID_QUEUE, or is not a valid queue of
        NodeId.

    - HSAKMT_STATUS_UNAVAILABLE if debugging is not available to this
      process. For example, there may be a limit on number of
      processes that can perform debugging at the same time.

    - HSAKMT_STATUS_NOT_SUPPORTED if debug trap is not supported by
      NodeId, or if QueueId is not INVALID_QUEUEID and NodeId does not
      support per queue enabling.
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtEnableDebugTrap(
    HSAuint32	NodeId, //IN
    HSA_QUEUEID	QueueId //IN
    );


/* Similar to EnableDebugTrap with polling fd return*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtEnableDebugTrapWithPollFd(
    HSAuint32	NodeId, //IN
    HSA_QUEUEID	QueueId, //IN
    HSAint32	*PollFd //OUT
    );

/**
  Disable debug trap enabled by hsaKmtEnableDebugTrap(). If debug trap
  is not currently enabled not action is taken.

  Returns:
    - HSAKMT_STATUS_SUCCESS if successful.

    - HSAKMT_STATUS_INVALID_HANDLE if NodeId is invalid.

    - HSAKMT_STATUS_NOT_SUPPORTED if debug trap not supported for NodeId.
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtDisableDebugTrap(
    HSAuint32 NodeId //IN
    );


/**
  Query pending debug event set by ptrace.

  Can query by target QueueId.  If QueueId is INVALID_QUEUEID, return the
  first queue id that has a pending event.  Option to clear pending event
  after query is used by the ClearEvents parameter.

  Pending debug event type will be returned in EventsReceived parameter and is
  defined by HSA_DEBUG_EVENT_TYPE.  Suspended state of queue is returned in
  IsSuspended.

  Returns:
    - HSAKMT_STATUS_SUCCESS if successful
 */
HSAKMT_STATUS
HSAKMTAPI
hsaKmtQueryDebugEvent(
    HSAuint32			NodeId, // IN
    HSAuint32			Pid, // IN
    HSAuint32			*QueueId, // IN/OUT
    bool			ClearEvents, // IN
    HSA_DEBUG_EVENT_TYPE	*EventsReceived, // OUT
    bool			*IsSuspended, // OUT
    bool			*IsNew //OUT
    );

/**
  Newly created queue snapshot per ptraced process.

  Returns queue snapshot including queue id, gpuid, context save base address,
  queue status word, queue address and size, and queue read and write pointer.

  ClearEvents set will clear new queue bit and queue status word bits.

  Returns:
    - HSAKMT_STATUS_SUCCESS if successful
 */
HSAKMT_STATUS
HSAKMTAPI
hsaKmtGetQueueSnapshot(
    HSAuint32			NodeId, // IN
    HSAuint32			Pid, // IN
    bool			ClearEvents, // IN
    void			*SnapshotBuf, // IN
    HSAuint32			*QssEntries // IN/OUT
    );

/**
  Send the host trap
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtSendHostTrap(
    HSAuint32	NodeId, //IN
    HSAuint32	Pid //IN
    );

/**
  Set the trap override mask. When debug trap is enabled by
  hsaKmtEnableDebugTrap() each wave launched has its initial
  MODE.excp_en register overriden by TrapMask as specified by
  TrapOverride.

  An error is returned if debug trap is not currently enabled for
  NodeId. Debug trap is enabled by hsaKmtEnableDebugTrap() which
  initializes TrapMask to 0 and TrapOverride to
  HSA_DBG_TRAP_OVERRIDE_OR.

  Returns:
    - HSAKMT_STATUS_SUCCESS if successful.

    - HSAKMT_STATUS_NOT_SUPPORTED if wave launch trap override is not
      supported by NodeId.

    - HSAKMT_STATUS_INVALID_HANDLE if NodeId is invalid.

    - HSAKMT_STATUS_INVALID_PARAMETER if TrapOverride is invalid.

    - HSAKMT_STATUS_ERROR if debug trap is not currently enabled by
      hsaKmtEnableDebugTrap() for NodeId.
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtSetWaveLaunchTrapOverride(
    HSAuint32             NodeId,       //IN
    HSA_DBG_TRAP_OVERRIDE TrapOverride, //IN
    HSA_DBG_TRAP_MASK     TrapMask      //IN
    );

/**
  Set the mode in which all future waves will be launched for
  NodeId.

  Returns:
    - HSAKMT_STATUS_SUCCESS if successful.

    - HSAKMT_STATUS_UNAVAILABLE if debugging is not available to this
      process. For example, there may be a limit on number of
      processes that can perform debugging at the same time.

    - HSAKMT_STATUS_NOT_SUPPORTED if the WaveLaunchMode requested is
      not supported by the NodeId. Different implementations and
      different nodes within an implementation can support different
      sets of launch modes. Only HSA_DBG_WAVE_LAUNCH_MODE_NORMAL mode
      is supported by all.

    - HSAKMT_STATUS_INVALID_HANDLE if NodeId is not a valid node.

    - HSAKMT_STATUS_INVALID_PARAMETER if WaveLaunchMode is not a valid
      value.
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtSetWaveLaunchMode(
    HSAuint32                NodeId,        //IN
    HSA_DBG_WAVE_LAUNCH_MODE WaveLaunchMode //IN
    );

/**
  * Get the major and minor version of the kernel debugger support.
  *
  * Returns:
  *  - HSAKMT_STATUS_SUCCESS if successful.
  *
  *  - HSAKMT_STATUS_INVALID_HANDLE if NodeId is invalid.
  *
  *  - HSAKMT_STATUS_NOT_SUPPORTED if debug trap not supported for NodeId.
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtGetKernelDebugTrapVersionInfo(
    HSAuint32 *Major,  //Out
    HSAuint32 *Minor   //Out
    );

/**
  * Get the major and minor version of the thunk debugger support.
*/
void
HSAKMTAPI
hsaKmtGetThunkDebugTrapVersionInfo(
    HSAuint32 *Major,  //Out
    HSAuint32 *Minor   //Out
    );




/**
  Set a debug memory access watch point. A memory access of the kind
  specified by WatchMode to an matching address will cause the trap
  handler to be entered. An address matches if, after ANDing the
  watch-addr-mask-lo..watch-addr-mask-hi bits of WatchAddrMask, it
  equals the WatchAddress with the bottom watch-addr-mask-lo bits
  cleared.

  WatchId will be in the range 0 to watch-count - 1. The WatchId
  value will match the address watch exception reported to the trap
  handler.

  hsaKmtGetNodeProperties() can be used to obtain HsaNodeProperties.
  watch-addr-mask-lo and watch-addr-mask-hi can be obtained from
  HsaNodeProperties.Capabilities.WatchAddrMaskLoBit and
  HsaNodeProperties.Capabilities.WatchAddrMaskHiBit respectively.
  watch-count can be obtained from
  2^HsaNodeProperties.Capabilities.WatchPointsTotalBits.

  To cause debug memory address watch points to be reported to the
  trap handler the address watch exception must be enabled. This can
  be accomplished by using hsaKmtSetWaveLaunchTrapOverride() with a
  TrapMask that includes HSA_DBG_TRAP_MASK_DBG_ADDRESS_WATCH.

  Returns:
    - HSAKMT_STATUS_SUCCESS if successful.

    - HSAKMT_STATUS_NOT_SUPPORTED if debug memory watch points are
      not supported for NodeId.

    - HSAKMT_STATUS_UNAVAILABLE if debugging is not available to this
      process. For example, there may be a limit on number of
      processes that can perform debugging at the same time.

    - HSAKMT_STATUS_INVALID_HANDLE if NodeId or WatchId* is invalid.

    - HSAKMT_STATUS_INVALID_PARAMETER if:

      - WatchAddrMask contains non-0 bits outside the inclusive range
        watch-addr-mask-lo to watch-addr-mask-hi.

      - If WatchAddress contain non-0 bits in the inclusive range 0 to
        watch-addr-mask-lo.

      - If WatchMode is not one of the values of HSA_DBG_WATCH_MODE.

      - WatchId is NULL.

    - HSAKMT_STATUS_OUT_OF_RESOURCES if no more watch points are
      available to set currently.
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtSetAddressWatch(
    HSAuint32          NodeId,        //IN
    HSAuint32          Pid,           //IN
    HSA_DBG_WATCH_MODE WatchMode,     //IN
    void*              WatchAddress,  //IN
    HSAuint64          WatchAddrMask, //IN
    HSAuint32*         WatchId        //OUT
    );

/**
  Clear a debug memory access watch point set by
  hsaKmtSetAddressWatch().

  Returns:
    - HSAKMT_STATUS_SUCCESS if successful.

    - HSAKMT_STATUS_NOT_SUPPORTED if debug memory watch points are
      not supported for NodeId.

    - HSAKMT_STATUS_INVALID_HANDLE if NodeId is invalid or WatchId is not valid for this
      NodeId.
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtClearAddressWatch(
    HSAuint32 NodeId, //IN
    HSAuint32 Pid,    //IN
    HSAuint32 WatchId //IN
    );

/**
  Enable precise memory operations.

  When precise memory operations are enabled a wave waits for each
  memory operation to complete before executing further
  operations. This results in more precise reporting of memory related
  events such as memory violation or address watch points.

  Returns:
    - HSAKMT_STATUS_SUCCESS if successful.

    - HSAKMT_STATUS_UNAVAILABLE if precise memory operations is not
      available to this process. For example, the feature may require
      specific privileges.

    - HSAKMT_STATUS_NOT_SUPPORTED if precise memory operations is not
      supported by NodeId.

    - HSAKMT_STATUS_INVALID_HANDLE if NodeId is invalid.
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtEnablePreciseMemoryOperations(
    HSAuint32 NodeId //IN
    );

/**
  Disable precise memory operations enabled by
  hsaKmtEnablePreciseMemoryOperations(). If precise memory operations
  are not currently enabled no action is taken.

  Returns:
    - HSAKMT_STATUS_SUCCESS if successful.

    - HSAKMT_STATUS_INVALID_HANDLE if NodeId is invalid.

    - HSAKMT_STATUS_NOT_SUPPORTED if precise memory operations is not
      supported by NodeId.
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtDisablePreciseMemoryOperations(
    HSAuint32 NodeId //IN
    );

/**
  Gets GPU and CPU clock counters for particular Node
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtGetClockCounters(
    HSAuint32         NodeId,  //IN
    HsaClockCounters* Counters //OUT
    );

/**
  Retrieves information on the available HSA counters
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtPmcGetCounterProperties(
    HSAuint32                   NodeId,             //IN
    HsaCounterProperties**      CounterProperties   //OUT
    );

/**
  Registers a set of (HW) counters to be used for tracing/profiling
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtPmcRegisterTrace(
    HSAuint32           NodeId,             //IN
    HSAuint32           NumberOfCounters,   //IN
    HsaCounter*         Counters,           //IN
    HsaPmcTraceRoot*    TraceRoot           //OUT
    );

/**
  Unregisters a set of (HW) counters used for tracing/profiling
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtPmcUnregisterTrace(
    HSAuint32   NodeId,     //IN
    HSATraceId  TraceId     //IN
    );

/**
  Allows a user mode process to get exclusive access to the defined set of (HW) counters
  used for tracing/profiling
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtPmcAcquireTraceAccess(
    HSAuint32   NodeId,     //IN
    HSATraceId  TraceId     //IN
    );

/**
  Allows a user mode process to release exclusive access to the defined set of (HW) counters
  used for tracing/profiling
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtPmcReleaseTraceAccess(
    HSAuint32   NodeId,     //IN
    HSATraceId  TraceId     //IN
    );

/**
  Starts tracing operation on a previously established set of performance counters
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtPmcStartTrace(
    HSATraceId  TraceId,                //IN
    void*       TraceBuffer,            //IN (page aligned) 
    HSAuint64   TraceBufferSizeBytes    //IN (page aligned)
    );

/**
   Forces an update of all the counters that a previously started trace operation has registered
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtPmcQueryTrace(
    HSATraceId    TraceId   //IN
    );

/**
  Stops tracing operation on a previously established set of performance counters
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtPmcStopTrace(
    HSATraceId  TraceId     //IN
    );

/**
  Sets trap handler and trap buffer to be used for all queues associated with the specified NodeId within this process context
*/

HSAKMT_STATUS 
HSAKMTAPI 
hsaKmtSetTrapHandler(
    HSAuint32           NodeId,                   //IN
    void*               TrapHandlerBaseAddress,   //IN
    HSAuint64           TrapHandlerSizeInBytes,   //IN
    void*               TrapBufferBaseAddress,    //IN
    HSAuint64           TrapBufferSizeInBytes     //IN
    );

/**
  Gets image tile configuration.
 */
HSAKMT_STATUS
HSAKMTAPI
hsaKmtGetTileConfig(
    HSAuint32           NodeId,     // IN
    HsaGpuTileConfig*   config      // IN & OUT
    );

/**
  Returns information about pointers
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtQueryPointerInfo(
    const void *        Pointer,        //IN
    HsaPointerInfo *    PointerInfo     //OUT
    );

/**
  Associates user data with a memory allocation
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtSetMemoryUserData(
    const void *    Pointer,    //IN
    void *          UserData    //IN
    );

/**
  Acquire request exclusive use of SPM
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtSPMAcquire(
    HSAuint32	PreferredNode	//IN
    );


/**
  Release exclusive use of SPM
*/
HSAKMT_STATUS
HSAKMTAPI
hsaKmtSPMRelease(
    HSAuint32	PreferredNode	//IN
    );

/**
   Set up the destination user mode buffer for stream performance
   counter data.
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtSPMSetDestBuffer(
	HSAuint32   PreferredNode,		//IN
	HSAuint32   SizeInBytes,		//IN
	HSAuint32   * timeout,			//IN/OUT
	HSAuint32   * SizeCopied,		//OUT
	void        *DestMemoryAddress,		//IN
	bool        *isSPMDataLoss		//OUT
    );

#ifdef __cplusplus
}   //extern "C"
#endif

#endif //_HSAKMT_H_

