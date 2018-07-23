/*
 * Copyright (C) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "KFDTestUtil.hpp"

#include <stdlib.h>
#include <algorithm>
#include <vector>
#include "BaseQueue.hpp"
#include "Dispatch.hpp"
#include "SDMAPacket.hpp"

bool WaitOnValue(const volatile unsigned int *buf, unsigned int value) {
    unsigned int  timeOut = g_TestTimeOut;

    while (timeOut > 0 && *buf != value) {
        Delay(1);

        if (timeOut != HSA_EVENTTIMEOUT_INFINITE)
            timeOut--;
    }

    return *buf == value;
}

void SplitU64(const unsigned long long value, unsigned int& rLoPart, unsigned int& rHiPart) {
    rLoPart = static_cast<unsigned int>(value);
    rHiPart = static_cast<unsigned int>(value >> 32);
}

bool GetHwCapabilityHWS() {
    unsigned int value = 0;
    bool valExists = ReadDriverConfigValue(CONFIG_HWS, value);

    /* HWS is enabled by default, so... */
    return ( (!valExists) || ( value > 0));
}

HSAKMT_STATUS CreateQueueTypeEvent(
    bool                ManualReset,            // IN
    bool                IsSignaled,             // IN
    unsigned int        NodeId,                 // IN
    HsaEvent**          Event                   // OUT
    ) {
    HsaEventDescriptor Descriptor;

// TODO Create per-OS header with this sort of definitions
#ifdef _WIN32
    Descriptor.EventType = HSA_EVENTTYPE_QUEUE_EVENT;
#else
    Descriptor.EventType = HSA_EVENTTYPE_SIGNAL;
#endif
    Descriptor.SyncVar.SyncVar.UserData = (void*)0xABCDABCD;
    Descriptor.NodeId = NodeId;

    return hsaKmtCreateEvent(&Descriptor, ManualReset, IsSignaled, Event);
}

static bool is_dgpu_dev = false;

bool is_dgpu() {
    return is_dgpu_dev;
}

unsigned int FamilyIdFromNode(const HsaNodeProperties *props) {
    unsigned int familyId = FAMILY_UNKNOWN;

    switch (props->EngineId.ui32.Major) {
    case 7:
        if (props->EngineId.ui32.Minor == 0) {
            if (props->EngineId.ui32.Stepping == 0)
                familyId = FAMILY_KV;
            else
                familyId = FAMILY_CI;
        }
        break;
    case 8:
        familyId = FAMILY_VI;
        if (props->EngineId.ui32.Stepping == 1)
            familyId = FAMILY_CZ;
        break;
    case 9:
        familyId = FAMILY_AI;
        if (props->EngineId.ui32.Stepping == 2)
            familyId = FAMILY_RV;
        break;
    }

    if (familyId == FAMILY_KV || familyId == FAMILY_CZ || familyId == FAMILY_RV)
        is_dgpu_dev = false;
    else
        is_dgpu_dev = true;

    return familyId;
}

bool isTonga(const HsaNodeProperties *props) {
    /* Tonga has some workarounds in the thunk that cause certain failures */
    if (props->EngineId.ui32.Major == 8 && props->EngineId.ui32.Stepping == 2) {
        return true;
    }

    return false;
}

const HsaMemoryBuffer HsaMemoryBuffer::Null;

HsaMemoryBuffer::HsaMemoryBuffer(HSAuint64 size, unsigned int node, bool zero, bool isLocal, bool isExec, bool isScratch, bool isReadOnly)
    :m_Size(size),
    m_pUser(NULL),
    m_pBuf(NULL),
    m_Local(isLocal),
    m_Node(node) {
    m_Flags.Value = 0;

    if (isScratch) {
        m_Flags.ui32.Scratch = 1;
        m_Flags.ui32.HostAccess = 1;
    } else {
        m_Flags.ui32.PageSize = HSA_PAGE_SIZE_4KB;

        if (isLocal) {
            m_Flags.ui32.HostAccess = 0;
            m_Flags.ui32.NonPaged = 1;
        } else {
            m_Flags.ui32.HostAccess = 1;
            m_Flags.ui32.NonPaged = 0;
        }

        if (isExec)
            m_Flags.ui32.ExecuteAccess = 1;
    }
    if (isReadOnly)
        m_Flags.ui32.ReadOnly = 1;

    EXPECT_SUCCESS(hsaKmtAllocMemory( m_Node, m_Size, m_Flags, &m_pBuf));
    if (is_dgpu()) {
        EXPECT_SUCCESS(hsaKmtMapMemoryToGPU(m_pBuf, m_Size, NULL));
        m_MappedNodes = 1 << m_Node;
    }

    if (zero && !isLocal)
        Fill(0);
}

HsaMemoryBuffer::HsaMemoryBuffer(void *addr, HSAuint64 size):
    m_Size(size),
    m_pUser(addr),
    m_pBuf(NULL),
    m_Local(false),
    m_Node(0) {
    HSAuint64 gpuva = 0;
    EXPECT_SUCCESS(hsaKmtRegisterMemory(m_pUser, m_Size));
    EXPECT_SUCCESS(hsaKmtMapMemoryToGPU(m_pUser, m_Size, &gpuva));
    m_pBuf = gpuva ? (void *)gpuva : m_pUser;
}

HsaMemoryBuffer::HsaMemoryBuffer()
    :m_Size(0),
    m_pBuf(NULL) {
}

void HsaMemoryBuffer::Fill(unsigned char value, HSAuint64 offset, HSAuint64 size) {
    HSAuint32 uiValue;

    EXPECT_EQ(m_Local, 0) << "Local Memory. Call Fill(HSAuint32 value, BaseQueue& baseQueue)";

    size = size ? size : m_Size;
    ASSERT_TRUE(size + offset <= m_Size) << "Buffer Overflow" << std::endl;

    if (m_pUser != NULL)
        memset((char *)m_pUser + offset, value, size);
    else if (m_pBuf != NULL)
        memset((char *)m_pBuf + offset, value, size);
    else
        ASSERT_TRUE(0) << "Invalid HsaMemoryBuffer";
}

/* Fill CPU accessible buffer with the value. */
void HsaMemoryBuffer::Fill(HSAuint32 value, HSAuint64 offset, HSAuint64 size) {
    HSAuint64 i;
    HSAuint32 *ptr = NULL;

    EXPECT_EQ(m_Local, 0) << "Local Memory. Call Fill(HSAuint32 value, BaseQueue& baseQueue)";
    size = size ? size : m_Size;
    EXPECT_EQ((size & (sizeof(HSAuint32) - 1)), 0) << "Not word aligned. Call Fill(unsigned char)";
    ASSERT_TRUE(size + offset <= m_Size) << "Buffer Overflow" << std::endl;

    if (m_pUser != NULL)
        ptr = (HSAuint32 *)((char *)m_pUser + offset);
    else if (m_pBuf != NULL)
        ptr = (HSAuint32 *)((char *)m_pBuf + offset);

    ASSERT_NOTNULL(ptr);

    for (i = 0; i < size / sizeof(HSAuint32); i++)
        ptr[i] = value;
}

/* Fill GPU only accessible Local memory with @value using SDMA Constant Fill Command */
void HsaMemoryBuffer::Fill(HSAuint32 value, BaseQueue& baseQueue, HSAuint64 offset, HSAuint64 size) {
    HsaEvent* event = NULL;

    EXPECT_NE(m_Local, 0) << "Not Local Memory. Call Fill(HSAuint32 value)";

    ASSERT_SUCCESS(CreateQueueTypeEvent(false, false, m_Node, &event));
    ASSERT_EQ(baseQueue.GetQueueType(), HSA_QUEUE_SDMA) << "Only SDMA queues supported";

    size = size ? size : m_Size;
    ASSERT_TRUE(size + offset <= m_Size) << "Buffer Overflow" << std::endl;

    baseQueue.PlacePacket(SDMAFillDataPacket((void *)(this->As<char*>() + offset), value, size));
    baseQueue.PlacePacket(SDMAFencePacket((void*)event->EventData.HWData2, event->EventId));
    baseQueue.PlaceAndSubmitPacket(SDMATrapPacket(event->EventId));
    ASSERT_SUCCESS(hsaKmtWaitOnEvent(event, g_TestTimeOut));

    hsaKmtDestroyEvent(event);
}

/* Check if HsaMemoryBuffer[location] has the pattern specified.
 * Return TRUE if correct pattern else return FALSE
 * HsaMemoryBuffer has to be CPU accessible
 */
bool HsaMemoryBuffer::IsPattern(HSAuint64 location, HSAuint32 pattern) {
    HSAuint32 *ptr = NULL;

    EXPECT_EQ(m_Local, 0) << "Local Memory. Call IsPattern(..baseQueue& baseQueue)";

    if (location >= m_Size) /* Out of bounds */
        return false;

    if (m_pUser != NULL)
        ptr = (HSAuint32 *)m_pUser;
    else if (m_pBuf != NULL)
        ptr = (HSAuint32 *)m_pBuf;
    else
        return false;

    if (ptr)
        return (ptr[location/sizeof(HSAuint32)] == pattern);

    return false;
}

/* Check if HsaMemoryBuffer[location] has the pattern specified.
 * Return TRUE if correct pattern else return FALSE
 * HsaMemoryBuffer is supposed to be only GPU accessible
 * Use @baseQueue to copy the HsaMemoryBuffer[location] to stack and check the value
 */

bool HsaMemoryBuffer::IsPattern(HSAuint64 location, HSAuint32 pattern, BaseQueue& baseQueue, volatile HSAuint32 *tmp) {
    HsaEvent* event = NULL;
    int ret;

    EXPECT_NE(m_Local, 0) << "Not Local Memory. Call IsPattern(HSAuint64 location, HSAuint32 pattern)";
    EXPECT_EQ(baseQueue.GetQueueType(), HSA_QUEUE_SDMA) << "Only SDMA queues supported";

    if (location >= m_Size) /* Out of bounds */
        return false;

    ret = CreateQueueTypeEvent(false, false, m_Node, &event);
    if (ret)
        return false;

    *tmp = ~pattern;
    baseQueue.PlacePacket(SDMACopyDataPacket((void *)tmp,
            (void *)(this->As<HSAuint64>() + location),
            sizeof(HSAuint32)));
    baseQueue.PlacePacket(SDMAFencePacket((void*)event->EventData.HWData2,
            event->EventId));
    baseQueue.PlaceAndSubmitPacket(SDMATrapPacket(event->EventId));

    ret = hsaKmtWaitOnEvent(event, g_TestTimeOut);
    hsaKmtDestroyEvent(event);
    if (ret)
        return false;

    return WaitOnValue(tmp, pattern);
}

unsigned int HsaMemoryBuffer::Size() {
    return m_Size;
}

HsaMemFlags HsaMemoryBuffer::Flags() {
    return m_Flags;
}

unsigned int HsaMemoryBuffer::Node() const {
    return m_Node;
}

int HsaMemoryBuffer::MapMemToNodes(unsigned int *nodes, unsigned int nodes_num) {
    int ret, bit;

    ret = hsaKmtRegisterMemoryToNodes(m_pBuf, m_Size, nodes_num, nodes);
    if (ret != 0)
        return ret;
    ret = hsaKmtMapMemoryToGPU(m_pBuf, m_Size, NULL);
    if (ret != 0) {
        hsaKmtDeregisterMemory(m_pBuf);
        return ret;
    }

    for (unsigned int i = 0; i < nodes_num; i++) {
        bit = 1 << nodes[i];
        m_MappedNodes |= bit;
    }

    return 0;
}

int HsaMemoryBuffer::UnmapMemToNodes(unsigned int *nodes, unsigned int nodes_num) {
    int ret, bit;

    ret = hsaKmtUnmapMemoryToGPU(m_pBuf);
    if (ret)
        return ret;

    hsaKmtDeregisterMemory(m_pBuf);
    for (unsigned int i = 0; i < nodes_num; i++) {
        bit = 1 << nodes[i];
        m_MappedNodes &= ~bit;
    }

    return 0;
}

void HsaMemoryBuffer::UnmapAllNodes() {
    unsigned int *Arr, size, i, j;
    int bit;

    size = 0;
    for (i = 0; i < 8; i++) {
        bit = 1 << i;
        if (m_MappedNodes & bit)
            size++;
    }

    Arr = (unsigned int *)malloc(sizeof(unsigned int) * size);
    if (!Arr)
        return;

    for (i = 0, j =0; i < 8; i++) {
        bit = 1 << i;
        if (m_MappedNodes & bit)
            Arr[j++] = i;
    }

    /*
     * TODO: when thunk will be updated use hsaKmtRegisterToNodes. and then nodes will be used
     */
    hsaKmtUnmapMemoryToGPU(m_pBuf);
    hsaKmtDeregisterMemory(m_pBuf);

    m_MappedNodes = 0;

    free(Arr);
}

HsaMemoryBuffer::~HsaMemoryBuffer() {
    if (m_pUser != NULL) {
        hsaKmtUnmapMemoryToGPU(m_pUser);
        hsaKmtDeregisterMemory(m_pUser);
    } else if (m_pBuf != NULL) {
        if (is_dgpu()) {
            if (m_MappedNodes) {
                hsaKmtUnmapMemoryToGPU(m_pBuf);
                hsaKmtDeregisterMemory(m_pBuf);
            }
        }
        hsaKmtFreeMemory(m_pBuf, m_Size);
    }
    m_pBuf = NULL;
}

HsaInteropMemoryBuffer::HsaInteropMemoryBuffer(unsigned long long device_handle, unsigned long long buffer_handle, unsigned long long size, unsigned int node)
    :m_Size(0),
     m_pBuf(NULL),
     m_graphic_handle(0),
     m_Node(node) {
    HSAuint64 flat_address;
    EXPECT_SUCCESS(hsaKmtMapGraphicHandle(m_Node, device_handle, buffer_handle, 0, size, &flat_address));
    m_pBuf = (void*)flat_address;
}

HsaInteropMemoryBuffer::~HsaInteropMemoryBuffer() {
    hsaKmtUnmapGraphicHandle(m_Node, (HSAuint64)m_pBuf, m_Size);
}


HsaNodeInfo::HsaNodeInfo() {
}

// Init - Get and store information about all the HSA nodes from the Thunk Library.
// @NumOfNodes - Number to system nodes returned by hsaKmtAcquireSystemProperties
// @Return - false: if no node information is available
//
bool HsaNodeInfo::Init(int NumOfNodes) {
    HsaNodeProperties *nodeProperties;
    _HSAKMT_STATUS status;
    bool ret = false;

    for (int i = 0; i < NumOfNodes; i++) {
        nodeProperties = new HsaNodeProperties();

        status = hsaKmtGetNodeProperties(i, nodeProperties);
        /* this is not a fatal test (not using assert), since even when it fails for one node
         * we want to get information regarding others. */
        EXPECT_SUCCESS(status) << "Node index: " << i << "hsaKmtGetNodeProperties returned status " << status;

        if (status == HSAKMT_STATUS_SUCCESS) {
            m_HsaNodeProps.push_back(nodeProperties);
            ret = true;  // Return true if atleast one information is available

            if (nodeProperties->NumFComputeCores)
                m_NodesWithGPU.push_back(i);
            else
                m_NodesWithoutGPU.push_back(i);
        } else {
            delete nodeProperties;
        }
    }

    return ret;
}

HsaNodeInfo::~HsaNodeInfo() {
    const HsaNodeProperties *nodeProperties;

    for (unsigned int i = 0; i < m_HsaNodeProps.size(); i++)
        delete m_HsaNodeProps.at(i);

    m_HsaNodeProps.clear();
}

const std::vector<int>& HsaNodeInfo::GetNodesWithGPU() const {
    return m_NodesWithGPU;
}

const HsaNodeProperties* HsaNodeInfo::GetNodeProperties(int NodeNum) const {
    return m_HsaNodeProps.at(NodeNum);
}

const HsaNodeProperties* HsaNodeInfo::HsaDefaultGPUNodeProperties() const {
    int NodeNum = HsaDefaultGPUNode();
    if (NodeNum < 0)
        return NULL;
    return GetNodeProperties(NodeNum);
}

const int HsaNodeInfo::HsaDefaultGPUNode() const {
    if (m_NodesWithGPU.size() == 0)
        return -1;

    if (g_TestNodeId >= 0) {
        // Check if this is a valid Id, if so use this else use first
        // available
        for (unsigned int i = 0; i < m_NodesWithGPU.size(); i++) {
            if (g_TestNodeId == m_NodesWithGPU.at(i))
                return g_TestNodeId;
        }
    }

    return m_NodesWithGPU.at(0);
}

void HsaNodeInfo::PrintNodeInfo() const {
    const HsaNodeProperties *nodeProperties;

    for (unsigned int i = 0; i < m_HsaNodeProps.size(); i++) {
        nodeProperties = m_HsaNodeProps.at(i);

        LOG() << "***********************************" << std::endl;
        LOG() << "Node " << i << std::endl;
        LOG() << "NumCPUCores=\t" << nodeProperties->NumCPUCores << std::endl;
        LOG() << "NumFComputeCores=\t" << nodeProperties->NumFComputeCores << std::endl;
        LOG() << "NumMemoryBanks=\t" << nodeProperties->NumMemoryBanks << std::endl;
        LOG() << "VendorId=\t" << nodeProperties->VendorId << std::endl;
        LOG() << "DeviceId=\t" << nodeProperties->DeviceId << std::endl;
        LOG() << "***********************************" << std::endl;
    }

    LOG() << "Default GPU NODE " << HsaDefaultGPUNode() << std::endl;
}

const bool HsaNodeInfo::IsGPUNodeLargeBar(int node) const {
    const HsaNodeProperties *pNodeProperties;

    pNodeProperties = GetNodeProperties(node);
    if (pNodeProperties) {
        HsaMemoryProperties *memoryProperties =
                new HsaMemoryProperties[pNodeProperties->NumMemoryBanks];
        EXPECT_SUCCESS(hsaKmtGetNodeMemoryProperties(node,
                       pNodeProperties->NumMemoryBanks, memoryProperties));
        for (unsigned bank = 0; bank < pNodeProperties->NumMemoryBanks; bank++)
            if (memoryProperties[bank].HeapType ==
                                HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC) {
                delete [] memoryProperties;
                return true;
            }
        delete [] memoryProperties;
    }

    return false;
}

const int HsaNodeInfo::FindLargeBarGPUNode() const {
    const std::vector<int> gpuNodes = GetNodesWithGPU();

    for (unsigned i = 0; i < gpuNodes.size(); i++)
        if (IsGPUNodeLargeBar(gpuNodes.at(i)))
            return gpuNodes.at(i);

    return -1;
}
