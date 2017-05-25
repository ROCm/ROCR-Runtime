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
    void**          MemoryAddress           //OUT (page-aligned)
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
     HsaMemMapFlags    MemMapFlags,               //IN
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

#ifdef __cplusplus
}   //extern "C"
#endif

#endif //_HSAKMT_H_

