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

#ifndef _HSAKMTTYPES_H_
#define _HSAKMTTYPES_H_

//the definitions and THUNK API are version specific - define the version numbers here
#define HSAKMT_VERSION_MAJOR    0
#define HSAKMT_VERSION_MINOR    99


#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN64) || defined(_WINDOWS) || defined(_WIN32)

    #if defined(_WIN32)
        #define HSAKMTAPI  __stdcall
    #else
        #define HSAKMTAPI
    #endif

    typedef unsigned char      HSAuint8;
    typedef char               HSAint8;
    typedef unsigned short     HSAuint16;
    typedef signed short       HSAint16;
    typedef unsigned __int32   HSAuint32;
    typedef signed __int64     HSAint64;
    typedef unsigned __int64   HSAuint64;

#elif defined(__linux__)

#include <stdbool.h>
#include <stdint.h>

    #define HSAKMTAPI

    typedef uint8_t     HSAuint8;
    typedef int8_t      HSAint8;
    typedef uint16_t	HSAuint16;
    typedef int16_t	HSAint16;
    typedef uint32_t	HSAuint32;
    typedef int64_t	HSAint64;
    typedef uint64_t	HSAuint64;

#endif

typedef void*              HSA_HANDLE;
typedef HSAuint64          HSA_QUEUEID;

// This is included in order to force the alignments to be 4 bytes so that
// it avoids extra padding added by the compiler when a 64-bit binary is generated.
#pragma pack(push, hsakmttypes_h, 4)

//
// HSA STATUS codes returned by the KFD Interfaces
//

typedef enum _HSAKMT_STATUS
{
    HSAKMT_STATUS_SUCCESS                      = 0,  // Operation successful
    HSAKMT_STATUS_ERROR                        = 1,  // General error return if not otherwise specified
    HSAKMT_STATUS_DRIVER_MISMATCH              = 2,  // User mode component is not compatible with kernel HSA driver

    HSAKMT_STATUS_INVALID_PARAMETER            = 3,  // KFD identifies input parameters invalid
    HSAKMT_STATUS_INVALID_HANDLE               = 4,  // KFD identifies handle parameter invalid
    HSAKMT_STATUS_INVALID_NODE_UNIT            = 5,  // KFD identifies node or unit parameter invalid

    HSAKMT_STATUS_NO_MEMORY                    = 6,  // No memory available (when allocating queues or memory)
    HSAKMT_STATUS_BUFFER_TOO_SMALL             = 7,  // A buffer needed to handle a request is too small

    HSAKMT_STATUS_NOT_IMPLEMENTED              = 10, // KFD function is not implemented for this set of paramters
    HSAKMT_STATUS_NOT_SUPPORTED                = 11, // KFD function is not supported on this node
    HSAKMT_STATUS_UNAVAILABLE                  = 12, // KFD function is not available currently on this node (but
                                                  // may be at a later time)

    HSAKMT_STATUS_KERNEL_IO_CHANNEL_NOT_OPENED = 20, // KFD driver path not opened
    HSAKMT_STATUS_KERNEL_COMMUNICATION_ERROR   = 21, // user-kernel mode communication failure
    HSAKMT_STATUS_KERNEL_ALREADY_OPENED        = 22, // KFD driver path already opened
    HSAKMT_STATUS_HSAMMU_UNAVAILABLE           = 23, // ATS/PRI 1.1 (Address Translation Services) not available
                                                  // (IOMMU driver not installed or not-available)

    HSAKMT_STATUS_WAIT_FAILURE                 = 30, // The wait operation failed
    HSAKMT_STATUS_WAIT_TIMEOUT                 = 31, // The wait operation timed out

    HSAKMT_STATUS_MEMORY_ALREADY_REGISTERED    = 35, // Memory buffer already registered
    HSAKMT_STATUS_MEMORY_NOT_REGISTERED        = 36, // Memory buffer not registered
    HSAKMT_STATUS_MEMORY_ALIGNMENT             = 37, // Memory parameter not aligned

} HSAKMT_STATUS;

//
// HSA KFD interface version information. Calling software has to validate that it meets
// the minimum interface version as described in the API specification.
// All future structures will be extended in a backward compatible fashion.
//

typedef struct _HsaVersionInfo
{
    HSAuint32    KernelInterfaceMajorVersion;    // supported kernel interface major version
    HSAuint32    KernelInterfaceMinorVersion;    // supported kernel interface minor version
} HsaVersionInfo;

//
// HSA Topology Discovery Infrastructure structure definitions.
// The infrastructure implementation is based on design specified in the Kernel HSA Driver ADD
// The discoverable data is retrieved from ACPI structures in the platform infrastructure, as defined
// in the "Heterogeneous System Architecture Detail Topology" specification.
//
// The following structure is returned on a call to hsaKmtAcquireSystemProperties() as output.
// When the call is made within a process context, a "snapshot" of the topology information
// is taken within the KFD to avoid any changes during the enumeration process.
// The Snapshot is released when hsaKmtReleaseSystemProperties() is called
// or when the process exits or is terminated.
//

typedef struct _HsaSystemProperties
{
    HSAuint32    NumNodes;         // the number of "H-NUMA" memory nodes.
                                   // each node represents a discoverable node of the system
                                   // All other enumeration is done on a per-node basis

    HSAuint32    PlatformOem;      // identifies HSA platform, reflects the OEMID in the CRAT
    HSAuint32    PlatformId;       // HSA platform ID, reflects OEM TableID in the CRAT
    HSAuint32    PlatformRev;      // HSA platform revision, reflects Platform Table Revision ID
} HsaSystemProperties;

typedef union 
{
    HSAuint32 Value;
    struct 
    {
        unsigned int uCode    : 10;  // ucode packet processor version
        unsigned int Major    :  6;  // GFXIP Major engine version
        unsigned int Minor    :  8;  // GFXIP Minor engine version
        unsigned int Stepping :  8;  // GFXIP Stepping info
    }ui32;
} HSA_ENGINE_ID;

typedef union
{
    HSAuint32 Value;
    struct
    {
        unsigned int HotPluggable        : 1;    // the node may be removed by some system action
                                                 // (event will be sent)
        unsigned int HSAMMUPresent       : 1;    // This node has an ATS/PRI 1.1 compatible
                                                 // translation agent in the system (e.g. IOMMUv2)
        unsigned int SharedWithGraphics  : 1;    // this HSA nodes' GPU function is also used for OS primary
                                                 // graphics render (= UI)
        unsigned int QueueSizePowerOfTwo : 1;    // This node GPU requires the queue size to be a power of 2 value
        unsigned int QueueSize32bit      : 1;    // This node GPU requires the queue size to be less than 4GB
        unsigned int QueueIdleEvent      : 1;    // This node GPU supports notification on Queue Idle
        unsigned int VALimit             : 1;    // This node GPU has limited VA range for platform
                                                 // (typical 40bit). Affects shared VM use for 64bit apps
        unsigned int WatchPointsSupported: 1;	 // Indicates if Watchpoints are available on the node.
        unsigned int WatchPointsTotalBits: 4;    // ld(Watchpoints) available. To determine the number use 2^value

        unsigned int DoorbellType        : 2;    // 0: This node has pre-1.0 doorbell characteristic
                                                 // 1: This node has 1.0 doorbell characteristic
                                                 // 2,3: reserved for future use
        unsigned int AQLQueueDoubleMap    : 1;	 // The unit needs a VA “double map”
        unsigned int Reserved            : 17;
    } ui32;
} HSA_CAPABILITY;


//
// HSA node properties. This structure is an output parameter of hsaKmtGetNodeProperties()
// The application or runtime can use the information herein to size the topology management structures
// Unless there is some very weird setup, there is at most one "GPU" device (with a certain number
// of throughput compute units (= SIMDs) associated with a H-NUMA node.
//

#define HSA_PUBLIC_NAME_SIZE        64   // Marketing name string size

typedef struct _HsaNodeProperties
{
    HSAuint32       NumCPUCores;       // # of latency (= CPU) cores present on this HSA node.
                                       // This value is 0 for a HSA node with no such cores,
                                       // e.g a "discrete HSA GPU"
    HSAuint32       NumFComputeCores;  // # of HSA throughtput (= GPU) FCompute cores ("SIMD") present in a node.
                                       // This value is 0 if no FCompute cores are present (e.g. pure "CPU node").
    HSAuint32       NumMemoryBanks;    // # of discoverable memory bank affinity properties on this "H-NUMA" node.
    HSAuint32       NumCaches;         // # of discoverable cache affinity properties on this "H-NUMA"  node.

    HSAuint32       NumIOLinks;        // # of discoverable IO link affinity properties of this node
                                       // connecting to other nodes.

    HSAuint32       CComputeIdLo;      // low value of the logical processor ID of the latency (= CPU)
                                       // cores available on this node
    HSAuint32       FComputeIdLo;      // low value of the logical processor ID of the throughput (= GPU)
                                       // units available on this node

    HSA_CAPABILITY  Capability;        // see above

    HSAuint32       MaxWavesPerSIMD;   // This identifies the max. number of launched waves per SIMD.
                                       // If NumFComputeCores is 0, this value is ignored.
    HSAuint32       LDSSizeInKB;       // Size of Local Data Store in Kilobytes per SIMD Wavefront
    HSAuint32       GDSSizeInKB;       // Size of Global Data Store in Kilobytes shared across SIMD Wavefronts

    HSAuint32       WaveFrontSize;     // Number of SIMD cores per wavefront executed, typically 64,
                                       // may be 32 or a different value for some HSA based architectures

    HSAuint32       NumShaderBanks;    // Number of Shader Banks or Shader Engines, typical values are 1 or 2


    HSAuint32       NumArrays;         // Number of SIMD arrays per engine
    HSAuint32       NumCUPerArray;     // Number of Compute Units (CU) per SIMD array
    HSAuint32       NumSIMDPerCU;      // Number of SIMD representing a Compute Unit (CU)

    HSAuint32       MaxSlotsScratchCU; // Number of temp. memory ("scratch") wave slots available to access,
                                       // may be 0 if HW has no restrictions

    HSA_ENGINE_ID   EngineId;          // Identifier (rev) of the GPU uEngine or Firmware, may be 0

    HSAuint16       VendorId;          // GPU vendor id; 0 on latency (= CPU)-only nodes
    HSAuint16       DeviceId;          // GPU device id; 0 on latency (= CPU)-only nodes

    HSAuint32       LocationId;        // GPU BDF (Bus/Device/function number) - identifies the device
                                       // location in the overall system
    HSAuint64       LocalMemSize;       // Local memory size
    HSAuint32       MaxEngineClockMhzFCompute;  // maximum engine clocks for CPU and
    HSAuint32       MaxEngineClockMhzCCompute;  // GPU function, including any boost caopabilities,

    HSAuint16       MarketingName[HSA_PUBLIC_NAME_SIZE];   // Public name of the "device" on the node (board or APU name).
                                       // Unicode string
    HSAuint8        AMDName[HSA_PUBLIC_NAME_SIZE];   //CAL Name of the "device", ASCII
    HSAuint8        Reserved[64];
} HsaNodeProperties;


typedef enum _HSA_HEAPTYPE
{
    HSA_HEAPTYPE_SYSTEM                = 0,
    HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC   = 1, // CPU "visible" part of GPU device local memory (for discrete GPU)
    HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE  = 2, // CPU "invisible" part of GPU device local memory (for discrete GPU)
                                            // All HSA accessible memory is per definition "CPU visible"
                                            // "Private memory" is relevant for graphics interop only.
    HSA_HEAPTYPE_GPU_GDS               = 3, // GPU internal memory (GDS)
    HSA_HEAPTYPE_GPU_LDS               = 4, // GPU internal memory (LDS)
    HSA_HEAPTYPE_GPU_SCRATCH           = 5, // GPU special memory (scratch)
    HSA_HEAPTYPE_DEVICE_SVM            = 6, // sys-memory mapped by device page tables

    HSA_HEAPTYPE_NUMHEAPTYPES,
    HSA_HEAPTYPE_SIZE                  = 0xFFFFFFFF
} HSA_HEAPTYPE;

typedef union
{
    HSAuint32 MemoryProperty;
    struct
    {
        unsigned int HotPluggable      : 1; // the memory may be removed by some system action,
                                            // memory should be used for temporary data
        unsigned int NonVolatile       : 1; // memory content is preserved across a power-off cycle.
        unsigned int Reserved          :30;
    } ui32;
} HSA_MEMORYPROPERTY;


//
// Discoverable HSA Memory properties.
// The structure is the output parameter of the hsaKmtGetNodeMemoryProperties() function
//

typedef struct _HsaMemoryProperties
{
    HSA_HEAPTYPE    HeapType;          // system or frame buffer,
    union
    {
        HSAuint64   SizeInBytes;       // physical memory size of the memory range in bytes
        struct
        {
            HSAuint32 SizeInBytesLow;  // physical memory size of the memory range in bytes (lower 32bit)
            HSAuint32 SizeInBytesHigh; // physical memory size of the memory range in bytes (higher 32bit)
        } ui32;
    };
    HSA_MEMORYPROPERTY  Flags;         // See definitions above

    HSAuint32    Width;                // memory width - the number of parallel bits of the memory interface
    HSAuint32    MemoryClockMax;       // memory clock for the memory, this allows computing the available bandwidth
                                       // to the memory when needed
    HSAuint64    VirtualBaseAddress;   // if set to value != 0, indicates the virtual base address of the memory
                                       // in process virtual space
} HsaMemoryProperties;

//
// Discoverable Cache Properties. (optional).
// The structure is the output parameter of the hsaKmtGetNodeMemoryProperties() function
// Any of the parameters may be 0 (= not defined)
//

#define HSA_CPU_SIBLINGS            256
#define HSA_PROCESSORID_ALL         0xFFFFFFFF

typedef union
{
    HSAuint32 Value;
    struct
    {
        unsigned int Data           : 1;
        unsigned int Instruction    : 1;
        unsigned int CPU            : 1;
        unsigned int HSACU          : 1;
        unsigned int Reserved       :28;
    } ui32;
} HsaCacheType;

typedef struct _HaCacheProperties
{
    HSAuint32    ProcessorIdLow;   // Identifies the processor number

    HSAuint32    CacheLevel;       // Integer representing level: 1, 2, 3, 4, etc
    HSAuint32    CacheSize;        // Size of the cache
    HSAuint32    CacheLineSize;    // Cache line size in bytes
    HSAuint32    CacheLinesPerTag; // Cache lines per Cache Tag
    HSAuint32    CacheAssociativity; // Cache Associativity
    HSAuint32    CacheLatency;     // Cache latency in ns
    HsaCacheType CacheType;
    HSAuint32    SiblingMap[HSA_CPU_SIBLINGS];
} HsaCacheProperties;


//
// Discoverable CPU Compute Properties. (optional).
// The structure is the output parameter of the hsaKmtGetCComputeProperties() function
// Any of the parameters may be 0 (= not defined)
//

typedef struct _HsaCComputeProperties
{
    HSAuint32    SiblingMap[HSA_CPU_SIBLINGS];
} HsaCComputeProperties;

//
// Discoverable IoLink Properties (optional).
// The structure is the output parameter of the hsaKmtGetIoLinkProperties() function.
// Any of the parameters may be 0 (= not defined)
//

typedef enum _HSA_IOLINKTYPE {
    HSA_IOLINKTYPE_UNDEFINED      = 0,
    HSA_IOLINKTYPE_HYPERTRANSPORT = 1,
    HSA_IOLINKTYPE_PCIEXPRESS     = 2,
    HSA_IOLINKTYPE_AMBA           = 3,
    HSA_IOLINKTYPE_MIPI           = 4,
    HSA_IOLINK_TYPE_QPI_1_1       = 5,
    HSA_IOLINK_TYPE_RESERVED1     = 6,
    HSA_IOLINK_TYPE_RESERVED2     = 7,
    HSA_IOLINK_TYPE_RAPID_IO      = 8,
    HSA_IOLINK_TYPE_INFINIBAND    = 9,
    HSA_IOLINK_TYPE_RESERVED3     = 10,
    HSA_IOLINKTYPE_OTHER          = 11,
    HSA_IOLINKTYPE_NUMIOLINKTYPES,
    HSA_IOLINKTYPE_SIZE           = 0xFFFFFFFF
} HSA_IOLINKTYPE;

typedef union
{
    HSAuint32 LinkProperty;
    struct
    {
        unsigned int Override          : 1;  // bus link properties are determined by this structure
                                             // not by the HSA_IOLINKTYPE. The other flags are valid
                                             // only if this bit is set to one
        unsigned int NonCoherent       : 1;  // The link doesn't support coherent transactions
                                             // memory accesses across must not be set to "host cacheable"!
        unsigned int NoAtomics32bit    : 1;  // The link doesn't support 32bit-wide atomic transactions
        unsigned int NoAtomics64bit    : 1;  // The link doesn't support 64bit-wide atomic transactions
        unsigned int NoPeerToPeerDMA   : 1;  // The link doesn't allow device P2P access
        unsigned int Reserved          :27;
    } ui32;
} HSA_LINKPROPERTY;


typedef struct _HsaIoLinkProperties
{
    HSA_IOLINKTYPE  IoLinkType;      // see above
    HSAuint32    VersionMajor;       // Bus interface version (optional)
    HSAuint32    VersionMinor;       // Bus interface version (optional)

    HSAuint32    NodeFrom;           //
    HSAuint32    NodeTo;             //

    HSAuint32    Weight;             // weight factor (derived from CDIT)

    HSAuint32    MinimumLatency;     // minimum cost of time to transfer (rounded to ns)
    HSAuint32    MaximumLatency;     // maximum cost of time to transfer (rounded to ns)
    HSAuint32    MinimumBandwidth;   // minimum interface Bandwidth in MB/s
    HSAuint32    MaximumBandwidth;   // maximum interface Bandwidth in MB/s
    HSAuint32    RecTransferSize;    // recommended transfer size to reach maximum bandwidth in Bytes
    HSA_LINKPROPERTY Flags;          // override flags (may be active for specific platforms)
} HsaIoLinkProperties;

//
// Memory allocation definitions for the KFD HSA interface
//

typedef struct _HsaMemFlags
{
    union
    {
        struct
        {
            unsigned int NonPaged    : 1; // default = 0: pageable memory
            unsigned int CachePolicy : 2; // see HSA_CACHING_TYPE
            unsigned int ReadOnly    : 1; // default = 0: Read/Write memory
            unsigned int PageSize    : 2; // see HSA_PAGE_SIZE
            unsigned int HostAccess  : 1; // default = 0: GPU access only
            unsigned int NoSubstitute: 1; // default = 0: if specific memory is not available on node (e.g. on
                                          // discrete GPU local), allocation may fall back to system memory node 0
                                          // memory (= always available). Otherwise no allocation is possible.
            unsigned int GDSMemory   : 1; // default = 0: If set, the allocation will occur in GDS heap.
                                          // HostAccess must be 0, all other flags (except NoSubstitute) should
                                          // be 0 when setting this entry to 1. GDS allocation may fail due to
                                          // limited resources. Application code is required to work without
                                          // any allocated GDS memory using regular memory.
                                          // Allocation fails on any node without GPU function.
            unsigned int Scratch     : 1; // default = 0: If set, the allocation will occur in GPU "scratch area".
                                          // HostAccess must be 0, all other flags (except NoSubstitute) should be 0
                                          // when setting this entry to 1. Scratch allocation may fail due to limited
                                          // resources. Application code is required to work without any allocation.
                                          // Allocation fails on any node without GPU function.
            unsigned int AtomicAccessFull: 1; // default = 0: If set, the memory will be allocated and mapped to allow 
                                              // atomic ops processing. On AMD APU, this will use the ATC path on system 
                                              // memory, irrespective of the NonPaged flag setting (= if NonPaged is set, 
                                              // the memory is pagelocked but mapped through IOMMUv2 instead of GPUVM). 
                                              // All atomic ops must be supported on this memory.
            unsigned int AtomicAccessPartial: 1; // default = 0: See above for AtomicAccessFull description, however 
                                                 // focused on AMD discrete GPU that support PCIe atomics; the memory 
                                                 // allocation is mapped to allow for PCIe atomics to operate on system 
                                                 // memory, irrespective of NonPaged set or the presence of an ATC path 
                                                 // in the system. The atomic operations supported are limited to SWAP, 
                                                 // CompareAndSwap (CAS) and FetchAdd (this PCIe op allows both atomic 
                                                 // increment and decrement via 2-complement arithmetic), which are the 
                                                 // only atomic ops directly supported in PCI Express.
                                                 // On AMD APU, setting this flag will allocate the same type of memory 
                                                 // as AtomicAccessFull, but it will be considered compatible with 
                                                 // discrete GPU atomic operations access.
            unsigned int ExecuteAccess: 1; // default = 0: Identifies if memory is primarily used for data or accessed 
                                           // for executable code (e.g. queue memory) by the host CPU or the device. 
                                           // Influences the page attribute setting within the allocation
            unsigned int CoarseGrain : 1;  // default = 0: The memory can be accessed assuming cache
                                           // coherency maintained by link infrastructure and HSA agents.
                                           // 1: memory consistency needs to be enforced at
                                           // synchronization points at dispatch or other software
                                           // enforced synchronization boundaries.
            unsigned int AQLQueueMemory: 1; // default = 0; If 1: The caller indicates that the memory will be used as AQL queue memory.
					    // The KFD will ensure that the memory returned is allocated in the optimal memory location
					    // and optimal alignment requirements
            unsigned int Reserved    : 17;

        } ui32;
        HSAuint32 Value;
    };
} HsaMemFlags;

typedef struct _HsaMemMapFlags
{
    union
    {
        struct
        {
            unsigned int Reserved1      :  1; //
            unsigned int CachePolicy    :  2; // see HSA_CACHING_TYPE
            unsigned int ReadOnly       :  1; // memory is not modified while mapped
            	    	    	    	      // allows migration scale-out
	    unsigned int PageSize	    :  2; // see HSA_PAGE_SIZE, hint to use
					  // this page size if possible and
					  // smaller than default
	    unsigned int HostAccess     :  1; // default = 0: GPU access only
	    unsigned int Migrate        :  1; // Hint: Allows migration to local mem
						  // of mapped GPU(s), instead of mapping
						  // physical location
            unsigned int Probe          :  1;     // default = 0: Indicates that a range
                                                  // will be mapped by the process soon,
						  // but does not initiate a map operation
						  // may trigger eviction of nonessential
						  // data from the memory, reduces latency
						  // “cleanup hint” only, may be ignored
            unsigned int Reserved       : 23;
        } ui32;
        HSAuint32 Value;
    };
} HsaMemMapFlags;

typedef struct _HsaGraphicsResourceInfo {
    void       *MemoryAddress;      // For use in hsaKmtMapMemoryToGPU(Nodes)
    HSAuint64  SizeInBytes;         // Buffer size
    const void *Metadata;           // Pointer to metadata owned by Thunk
    HSAuint32  MetadataSizeInBytes; // Size of metadata
    HSAuint32  Reserved;            // Reserved for future use, will be set to 0
} HsaGraphicsResourceInfo;

typedef enum _HSA_CACHING_TYPE
{
    HSA_CACHING_CACHED        = 0,
    HSA_CACHING_NONCACHED     = 1,
    HSA_CACHING_WRITECOMBINED = 2,
    HSA_CACHING_RESERVED      = 3,
    HSA_CACHING_NUM_CACHING,
    HSA_CACHING_SIZE          = 0xFFFFFFFF
} HSA_CACHING_TYPE;

typedef enum _HSA_PAGE_SIZE
{
    HSA_PAGE_SIZE_4KB         = 0,
    HSA_PAGE_SIZE_64KB        = 1,  //64KB pages, not generally available in systems
    HSA_PAGE_SIZE_2MB         = 2,
    HSA_PAGE_SIZE_1GB         = 3,  //1GB pages, not generally available in systems
} HSA_PAGE_SIZE;


typedef enum _HSA_DEVICE
{
    HSA_DEVICE_CPU  = 0,
    HSA_DEVICE_GPU  = 1,
    MAX_HSA_DEVICE  = 2
} HSA_DEVICE;


typedef enum _HSA_QUEUE_PRIORITY
{
    HSA_QUEUE_PRIORITY_MINIMUM        = -3,
    HSA_QUEUE_PRIORITY_LOW            = -2,
    HSA_QUEUE_PRIORITY_BELOW_NORMAL   = -1,
    HSA_QUEUE_PRIORITY_NORMAL         =  0,
    HSA_QUEUE_PRIORITY_ABOVE_NORMAL   =  1,
    HSA_QUEUE_PRIORITY_HIGH           =  2,
    HSA_QUEUE_PRIORITY_MAXIMUM        =  3,
    HSA_QUEUE_PRIORITY_NUM_PRIORITY,
    HSA_QUEUE_PRIORITY_SIZE           = 0xFFFFFFFF
} HSA_QUEUE_PRIORITY;

typedef enum _HSA_QUEUE_TYPE
{
    HSA_QUEUE_COMPUTE            = 1,  // AMD PM4 compatible Compute Queue
    HSA_QUEUE_SDMA               = 2,  // SDMA Queue, used for data transport and format conversion (e.g. (de-)tiling, etc).
    HSA_QUEUE_MULTIMEDIA_DECODE  = 3,  // reserved, for HSA multimedia decode queue
    HSA_QUEUE_MULTIMEDIA_ENCODE  = 4,  // reserved, for HSA multimedia encode queue

    // the following values indicate a queue type permitted to reference OS graphics
    // resources through the interoperation API. See [5] "HSA Graphics Interoperation
    // specification" for more details on use of such resources.

    HSA_QUEUE_COMPUTE_OS           = 11, // AMD PM4 compatible Compute Queue
    HSA_QUEUE_SDMA_OS              = 12, // SDMA Queue, used for data transport and format conversion (e.g. (de-)tiling, etc).
    HSA_QUEUE_MULTIMEDIA_DECODE_OS = 13, // reserved, for HSA multimedia decode queue
    HSA_QUEUE_MULTIMEDIA_ENCODE_OS = 14,  // reserved, for HSA multimedia encode queue

    HSA_QUEUE_COMPUTE_AQL          = 21, // HSA AQL packet compatible Compute Queue
    HSA_QUEUE_DMA_AQL              = 22, // HSA AQL packet compatible DMA Queue

    // more types in the future

    HSA_QUEUE_TYPE_SIZE            = 0xFFFFFFFF     //aligns to 32bit enum
} HSA_QUEUE_TYPE;

typedef struct _HsaQueueResource
{
    HSA_QUEUEID     QueueId;    /** queue ID */
    /** Doorbell address to notify HW of a new dispatch */
    union
    {
        HSAuint32*  Queue_DoorBell;
        HSAuint64*  Queue_DoorBell_aql;
        HSAuint64   QueueDoorBell;
    };

    /** virtual address to notify HW of queue write ptr value */
    union
    {
        HSAuint32*  Queue_write_ptr;
        HSAuint64*  Queue_write_ptr_aql;
        HSAuint64   QueueWptrValue;
    };

    /** virtual address updated by HW to indicate current read location */
    union
    {
        HSAuint32*  Queue_read_ptr;
        HSAuint64*  Queue_read_ptr_aql;
        HSAuint64   QueueRptrValue;
    };

} HsaQueueResource;


//TEMPORARY structure definition - to be used only on "Triniti + Southern Islands" platform
typedef struct _HsaQueueReport
{
    HSAuint32     VMID;         //Required on SI to dispatch IB in primary ring
    void*         QueueAddress; //virtual address of UM mapped compute ring
    HSAuint64     QueueSize;    //size of the UM mapped compute ring
} HsaQueueReport;



typedef enum _HSA_DBG_WAVEOP
{
    HSA_DBG_WAVEOP_HALT        = 1, //Halts a wavefront
    HSA_DBG_WAVEOP_RESUME      = 2, //Resumes a wavefront
    HSA_DBG_WAVEOP_KILL        = 3, //Kills a wavefront
    HSA_DBG_WAVEOP_DEBUG       = 4, //Causes wavefront to enter debug mode
    HSA_DBG_WAVEOP_TRAP        = 5, //Causes wavefront to take a trap
    HSA_DBG_NUM_WAVEOP         = 5,
    HSA_DBG_MAX_WAVEOP         = 0xFFFFFFFF
} HSA_DBG_WAVEOP;

typedef enum _HSA_DBG_WAVEMODE
{
    HSA_DBG_WAVEMODE_SINGLE               = 0,  //send command to a single wave
    //Broadcast to all wavefronts of all processes is not supported for HSA user mode
    HSA_DBG_WAVEMODE_BROADCAST_PROCESS    = 2,  //send to waves within current process
    HSA_DBG_WAVEMODE_BROADCAST_PROCESS_CU = 3,  //send to waves within current process on CU
    HSA_DBG_NUM_WAVEMODE                  = 3,
    HSA_DBG_MAX_WAVEMODE                  = 0xFFFFFFFF
} HSA_DBG_WAVEMODE;


typedef enum _HSA_DBG_WAVEMSG_TYPE
{
    HSA_DBG_WAVEMSG_AUTO    = 0,
    HSA_DBG_WAVEMSG_USER    = 1,
    HSA_DBG_WAVEMSG_ERROR   = 2,
    HSA_DBG_NUM_WAVEMSG,
    HSA_DBG_MAX_WAVEMSG     = 0xFFFFFFFF
} HSA_DBG_WAVEMSG_TYPE;

typedef enum _HSA_DBG_WATCH_MODE
{
    HSA_DBG_WATCH_READ        = 0, //Read operations only
    HSA_DBG_WATCH_NONREAD     = 1, //Write or Atomic operations only
    HSA_DBG_WATCH_ATOMIC      = 2, //Atomic Operations only
    HSA_DBG_WATCH_ALL         = 3, //Read, Write or Atomic operations
    HSA_DBG_WATCH_NUM,
    HSA_DBG_WATCH_SIZE        = 0xFFFFFFFF
} HSA_DBG_WATCH_MODE;


//This structure is hardware specific and may change in the future
typedef struct _HsaDbgWaveMsgAMDGen2
{
    HSAuint32      Value;
    HSAuint32      Reserved2;

} HsaDbgWaveMsgAMDGen2;

typedef union _HsaDbgWaveMessageAMD
{
    HsaDbgWaveMsgAMDGen2    WaveMsgInfoGen2;
    //for future HsaDbgWaveMsgAMDGen3;
} HsaDbgWaveMessageAMD;

typedef struct _HsaDbgWaveMessage
{
    void*                   MemoryVA;         // ptr to associated host-accessible data
    HsaDbgWaveMessageAMD    DbgWaveMsg;
} HsaDbgWaveMessage;


//
// HSA sync primitive, Event and HW Exception notification API definitions
// The API functions allow the runtime to define a so-called sync-primitive, a SW object
// combining a user-mode provided "syncvar" and a scheduler event that can be signaled
// through a defined GPU interrupt. A syncvar is a process virtual memory location of
// a certain size that can be accessed by CPU and GPU shader code within the process to set
// and query the content within that memory. The definition of the content is determined by
// the HSA runtime and potentially GPU shader code interfacing with the HSA runtime.
// The syncvar values may be commonly written through an PM4 WRITE_DATA packet in the
// user mode instruction stream.
// The OS scheduler event is typically associated and signaled by an interrupt issued by
// the GPU, but other HSA system interrupt conditions from other HW (e.g. IOMMUv2) may be
// surfaced by the KFD by this mechanism, too.
//

// these are the new definitions for events
typedef enum _HSA_EVENTTYPE
{
    HSA_EVENTTYPE_SIGNAL                     = 0, //user-mode generated GPU signal
    HSA_EVENTTYPE_NODECHANGE                 = 1, //HSA node change (attach/detach)
    HSA_EVENTTYPE_DEVICESTATECHANGE          = 2, //HSA device state change( start/stop )
    HSA_EVENTTYPE_HW_EXCEPTION               = 3, //GPU shader exception event
    HSA_EVENTTYPE_SYSTEM_EVENT               = 4, //GPU SYSCALL with parameter info
    HSA_EVENTTYPE_DEBUG_EVENT                = 5, //GPU signal for debugging
    HSA_EVENTTYPE_PROFILE_EVENT              = 6, //GPU signal for profiling
    HSA_EVENTTYPE_QUEUE_EVENT                = 7, //GPU signal queue idle state (EOP pm4)
    HSA_EVENTTYPE_MEMORY                     = 8, //GPU signal for signaling memory access faults and memory subsystem issues
    //...
    HSA_EVENTTYPE_MAXID,
    HSA_EVENTTYPE_TYPE_SIZE                  = 0xFFFFFFFF
} HSA_EVENTTYPE;

typedef HSAuint32  HSA_EVENTID;

//
// Subdefinitions for various event types: Syncvar
//

typedef struct _HsaSyncVar
{
    union
    {
        void*       UserData;           //pointer to user mode data
        HSAuint64   UserDataPtrValue;   //64bit compatibility of value
    } SyncVar;
    HSAuint64       SyncVarSize;
} HsaSyncVar;

//
// Subdefinitions for various event types: NodeChange
//

typedef enum _HSA_EVENTTYPE_NODECHANGE_FLAGS
{
    HSA_EVENTTYPE_NODECHANGE_ADD     = 0,
    HSA_EVENTTYPE_NODECHANGE_REMOVE  = 1,
    HSA_EVENTTYPE_NODECHANGE_SIZE    = 0xFFFFFFFF
} HSA_EVENTTYPE_NODECHANGE_FLAGS;

typedef struct _HsaNodeChange
{
    HSA_EVENTTYPE_NODECHANGE_FLAGS Flags;   // HSA node added/removed on the platform
} HsaNodeChange;

//
// Sub-definitions for various event types: DeviceStateChange
//

typedef enum _HSA_EVENTTYPE_DEVICESTATECHANGE_FLAGS
{
    HSA_EVENTTYPE_DEVICESTATUSCHANGE_START     = 0, //device started (and available)
    HSA_EVENTTYPE_DEVICESTATUSCHANGE_STOP      = 1, //device stopped (i.e. unavailable)
    HSA_EVENTTYPE_DEVICESTATUSCHANGE_SIZE      = 0xFFFFFFFF
} HSA_EVENTTYPE_DEVICESTATECHANGE_FLAGS;

typedef struct _HsaDeviceStateChange
{
    HSAuint32                           NodeId;     // F-NUMA node that contains the device
    HSA_DEVICE                          Device;     // device type: GPU or CPU
    HSA_EVENTTYPE_DEVICESTATECHANGE_FLAGS Flags;    // event flags
} HsaDeviceStateChange;

//
// Sub-definitions for various event types: Memory exception
//

typedef enum _HSA_EVENTID_MEMORYFLAGS
{
    HSA_EVENTID_MEMORY_RECOVERABLE           = 0, //access fault, recoverable after page adjustment
    HSA_EVENTID_MEMORY_FATAL_PROCESS         = 1, //memory access requires process context destruction, unrecoverable
    HSA_EVENTID_MEMORY_FATAL_VM              = 2, //memory access requires all GPU VA context destruction, unrecoverable
} HSA_EVENTID_MEMORYFLAGS;

typedef struct _HsaAccessAttributeFailure
{
    unsigned int NotPresent  : 1;  // Page not present or supervisor privilege 
    unsigned int ReadOnly    : 1;  // Write access to a read-only page
    unsigned int NoExecute   : 1;  // Execute access to a page marked NX
    unsigned int GpuAccess   : 1;  // Host access only
    unsigned int ECC         : 1;  // ECC failure (if supported by HW)
    unsigned int Imprecise   : 1;  // Can't determine the exact fault address
    unsigned int Reserved    : 26; // must be 0
} HsaAccessAttributeFailure;

// data associated with HSA_EVENTID_MEMORY
typedef struct _HsaMemoryAccessFault
{
    HSAuint32                       NodeId;             // H-NUMA node that contains the device where the memory access occurred
    HSAuint64                       VirtualAddress;     // virtual address this occurred on
    HsaAccessAttributeFailure       Failure;            // failure attribute
    HSA_EVENTID_MEMORYFLAGS         Flags;              // event flags
} HsaMemoryAccessFault;

typedef struct _HsaEventData
{
    HSA_EVENTTYPE   EventType;      //event type

    union
    {
        // return data associated with HSA_EVENTTYPE_SIGNAL and other events
        HsaSyncVar              SyncVar;

        // data associated with HSA_EVENTTYPE_NODE_CHANGE
        HsaNodeChange           NodeChangeState;

        // data associated with HSA_EVENTTYPE_DEVICE_STATE_CHANGE
        HsaDeviceStateChange    DeviceState;

        // data associated with HSA_EVENTTYPE_MEMORY
        HsaMemoryAccessFault    MemoryAccessFault;

    } EventData;

    // the following data entries are internal to the KFD & thunk itself.

    HSAuint64       HWData1;                    // internal thunk store for Event data  (OsEventHandle)
    HSAuint64       HWData2;                    // internal thunk store for Event data  (HWAddress)
    HSAuint32       HWData3;                    // internal thunk store for Event data  (HWData)
} HsaEventData;


typedef struct _HsaEventDescriptor
{
    HSA_EVENTTYPE   EventType;                  // event type to allocate
    HSAuint32       NodeId;                     // H-NUMA node containing GPU device that is event source
    HsaSyncVar      SyncVar;                    // pointer to user mode syncvar data, syncvar->UserDataPtrValue may be NULL
} HsaEventDescriptor;


typedef struct _HsaEvent
{
    HSA_EVENTID     EventId;
    HsaEventData    EventData;
} HsaEvent;

typedef enum _HsaEventTimeout
{
    HSA_EVENTTIMEOUT_IMMEDIATE  = 0,
    HSA_EVENTTIMEOUT_INFINITE   = 0xFFFFFFFF
} HsaEventTimeOut;

typedef struct _HsaClockCounters
{
    HSAuint64   GPUClockCounter;
    HSAuint64   CPUClockCounter;
    HSAuint64   SystemClockCounter;
    HSAuint64   SystemClockFrequencyHz;
} HsaClockCounters;

#ifndef DEFINE_GUID
typedef struct _HSA_UUID
{
    HSAuint32   Data1;
    HSAuint16   Data2;
    HSAuint16   Data3;
    HSAuint8    Data4[8];
} HSA_UUID;

#define HSA_DEFINE_UUID(name, dw, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const HSA_UUID name = {dw, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}
#else
#define HSA_UUID GUID
#define HSA_DEFINE_UUID DEFINE_GUID
#endif


// GUID that identifies the GPU Shader Sequencer (SQ) block
// {B5C396B6-D310-47E4-86FC-5CC3043AF508}
HSA_DEFINE_UUID(HSA_PROFILEBLOCK_AMD_SQ,
0xb5c396b6, 0xd310, 0x47e4, 0x86, 0xfc, 0x5c, 0xc3, 0x4, 0x3a, 0xf5, 0x8);

// GUID that identifies the GPU Memory Controller (MC) block
// {13900B57-4956-4D98-81D0-68521937F59C}
HSA_DEFINE_UUID(HSA_PROFILEBLOCK_AMD_MC,
0x13900b57, 0x4956, 0x4d98, 0x81, 0xd0, 0x68, 0x52, 0x19, 0x37, 0xf5, 0x9c);

// GUID that identifies the IMOMMUv2 HW device
// {80969879-B0F6-4BE6-97F6-6A6300F5101D}
HSA_DEFINE_UUID(HSA_PROFILEBLOCK_AMD_IOMMUV2,
0x80969879, 0xb0f6, 0x4be6, 0x97, 0xf6, 0x6a, 0x63, 0x0, 0xf5, 0x10, 0x1d);

// GUID that identifies the KFD
// {EA9B5AE1-6C3F-44B3-8954-DAF07565A90A}
HSA_DEFINE_UUID(HSA_PROFILEBLOCK_AMD_KERNEL_DRIVER,
0xea9b5ae1, 0x6c3f, 0x44b3, 0x89, 0x54, 0xda, 0xf0, 0x75, 0x65, 0xa9, 0xa);

typedef enum _HSA_PROFILE_TYPE
{
    HSA_PROFILE_TYPE_PRIVILEGED_IMMEDIATE = 0, //immediate access counter (KFD access only)
    HSA_PROFILE_TYPE_PRIVILEGED_STREAMING = 1, //streaming counter, HW continuously
                                               //writes to memory on updates (KFD access only)
    HSA_PROFILE_TYPE_NONPRIV_IMMEDIATE    = 2, //user-queue accessible counter
    HSA_PROFILE_TYPE_NONPRIV_STREAMING    = 3, //user-queue accessible counter
    //...
    HSA_PROFILE_TYPE_NUM,

    HSA_PROFILE_TYPE_SIZE                 = 0xFFFFFFFF      // In order to align to 32-bit value
} HSA_PROFILE_TYPE;


typedef struct _HsaCounterFlags
{
    union
    {
        struct
        {
            unsigned int  Global       : 1;  // counter is global
                                             // (not tied to VMID/WAVE/CU, ...)
            unsigned int  Resettable   : 1;  // counter can be reset by SW
                                             // (always to 0?)
            unsigned int  ReadOnly     : 1;  // counter is read-only
                                             // (but may be reset, if indicated)
            unsigned int  Stream       : 1;  // counter has streaming capability
                                             // (after trigger, updates buffer)
            unsigned int  Reserved     : 28;
        } ui32;
        HSAuint32      Value;
    };
} HsaCounterFlags;


typedef struct _HsaCounter
{
    HSA_PROFILE_TYPE Type;              // specifies the counter type
    HSAuint64        CounterId;         // indicates counter register offset
    HSAuint32        CounterSizeInBits; // indicates relevant counter bits
    HSAuint64        CounterMask;       // bitmask for counter value (if applicable)
    HsaCounterFlags  Flags;             // Property flags (see above)
    HSAuint32        BlockIndex;        // identifies block the counter belongs to,
                                        // value may be 0 to NumBlocks
} HsaCounter;


typedef struct _HsaCounterBlockProperties
{
    HSA_UUID                    BlockId;        // specifies the block location
    HSAuint32                   NumCounters;    // How many counters are available?
                                                // (sizes Counters[] array below)
    HSAuint32                   NumConcurrent;  // How many counter slots are available
                                                // in block?
    HsaCounter                  Counters[1];    // Start of counter array
                                                // (NumCounters elements total)
} HsaCounterBlockProperties;


typedef struct _HsaCounterProperties
{
    HSAuint32                   NumBlocks;      // How many profilable block are available?
                                                // (sizes Blocks[] array below)
    HSAuint32                   NumConcurrent;  // How many blocks slots can be queried
                                                // concurrently by HW?
    HsaCounterBlockProperties   Blocks[1];      // Start of block array
                                                // (NumBlocks elements total)
} HsaCounterProperties;

typedef HSAuint64   HSATraceId;

typedef struct _HsaPmcTraceRoot
{
    HSAuint64                   TraceBufferMinSizeBytes;// (page aligned)
    HSAuint32                   NumberOfPasses;
    HSATraceId                  TraceId;
} HsaPmcTraceRoot;

typedef struct _HsaGpuTileConfig
{
    const HSAuint32 *TileConfig;
    const HSAuint32 *MacroTileConfig;
    HSAuint32 NumTileConfigs;
    HSAuint32 NumMacroTileConfigs;

    HSAuint32 GbAddrConfig;

    HSAuint32 NumBanks;
    HSAuint32 NumRanks;
    /* 9 dwords on 64-bit system */
    HSAuint32 Reserved[7]; /* Round up to 16 dwords for future extension */
} HsaGpuTileConfig;

typedef enum _HSA_POINTER_TYPE {
    HSA_POINTER_UNKNOWN = 0,
    HSA_POINTER_ALLOCATED = 1,           // Allocated with hsaKmtAllocMemory (except scratch)
    HSA_POINTER_REGISTERED_USER = 2,     // Registered user pointer
    HSA_POINTER_REGISTERED_GRAPHICS = 3  // Registered graphics buffer
                                         // (hsaKmtRegisterGraphicsToNodes)
} HSA_POINTER_TYPE;

typedef struct _HsaPointerInfo {
    HSA_POINTER_TYPE   Type;             // Pointer type
    HSAuint32          Node;             // Node where the memory is located
    HsaMemFlags        MemFlags;         // Only valid for HSA_POINTER_ALLOCATED
    void               *CPUAddress;      // Start address for CPU access
    HSAuint64          GPUAddress;       // Start address for GPU access
    HSAuint64          SizeInBytes;      // Size in bytes
    HSAuint32          NRegisteredNodes; // Number of nodes the memory is registered to
    HSAuint32          NMappedNodes;     // Number of nodes the memory is mapped to
    const HSAuint32    *RegisteredNodes; // Array of registered nodes
    const HSAuint32    *MappedNodes;     // Array of mapped nodes
    void               *UserData;        // User data associated with the memory
} HsaPointerInfo;

#pragma pack(pop, hsakmttypes_h)


#ifdef __cplusplus
}   //extern "C"
#endif

#endif //_HSAKMTTYPES_H_
