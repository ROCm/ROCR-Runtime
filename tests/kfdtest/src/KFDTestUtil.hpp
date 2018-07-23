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

#ifndef __KFD__TEST__UTIL__H__
#define __KFD__TEST__UTIL__H__

#include <gtest/gtest.h>
#include <vector>
#include "OSWrapper.hpp"
#include "GoogleTestExtension.hpp"
#include "hsakmt.h"

class BaseQueue;
#define ARRAY_SIZE(_x) (sizeof(_x)/sizeof(_x[0]))

// @brief: waits until the value is written to the buffer or until time out if received through args
bool WaitOnValue(const volatile unsigned int *buf, unsigned int value);

void SplitU64(const unsigned long long value, unsigned int& rLoPart, unsigned int& rHiPart);

bool GetHwCapabilityHWS();

HSAKMT_STATUS CreateQueueTypeEvent(bool ManualReset, bool IsSignaled, unsigned int NodeId, HsaEvent** Event);

bool is_dgpu();
bool isTonga(const HsaNodeProperties *props);
unsigned int FamilyIdFromNode(const HsaNodeProperties * props);


class HsaMemoryBuffer {
 public:
    static const HsaMemoryBuffer Null;

 public:
    HsaMemoryBuffer(HSAuint64 size, unsigned int node, bool zero = true, bool isLocal = false,
                bool isExec = false, bool isScratch = false, bool isReadOnly = false);
    HsaMemoryBuffer(void *addr, HSAuint64 size);
    template<typename RetType>
    RetType As() {
        return reinterpret_cast<RetType>(m_pBuf);
    }

    template<typename RetType>
    const RetType As() const {
        return reinterpret_cast<const RetType>(m_pBuf);
    }

    /* Fill @size bytes of buffer with @value starting from @offset
     * If @size is 0, the whole buffer is filled with @value
     */
    void Fill(unsigned char value, HSAuint64 offset = 0, HSAuint64 size = 0);
    void Fill(HSAuint32 value, HSAuint64 offset = 0, HSAuint64 size = 0);
    void Fill(int value, HSAuint64 offset = 0, HSAuint64 size = 0) {
              Fill((HSAuint32)value, offset, size);
    }
    void Fill(HSAuint32 value, BaseQueue& baseQueue,
              HSAuint64 offset = 0, HSAuint64 size = 0);

    bool IsPattern(HSAuint64 location, HSAuint32 pattern);
    bool IsPattern(HSAuint64 location, HSAuint32 pattern,
                   BaseQueue& baseQueue, volatile HSAuint32 *tmp);

    unsigned int Size();
    HsaMemFlags Flags();
    unsigned int Node() const;

    int MapMemToNodes(unsigned int *nodes, unsigned int nodes_num);
    int UnmapMemToNodes(unsigned int *nodes, unsigned int nodes_num);

    void *GetUserPtr() { return m_pUser; }
    bool isLocal() { return m_Local; }
    ~HsaMemoryBuffer();

 private:
    // disable copy
    HsaMemoryBuffer(const HsaMemoryBuffer&);
    const HsaMemoryBuffer& operator=(const HsaMemoryBuffer&);

    void UnmapAllNodes();
    HsaMemoryBuffer();

 private:
    HsaMemFlags m_Flags;
    HSAuint64 m_Size;
    void* m_pUser;
    void* m_pBuf;
    bool m_Local;
    unsigned int m_Node;
    unsigned short m_MappedNodes;
};



class HsaInteropMemoryBuffer {
 public:
    HsaInteropMemoryBuffer(unsigned long long device_handle, unsigned long long buffer_handle, unsigned long long size, unsigned int node);

    template<typename RetType>
    RetType As() {
        return reinterpret_cast<RetType>(m_pBuf);
    }

    template<typename RetType>
    const RetType As() const {
        return reinterpret_cast<const RetType>(m_pBuf);
    }

    unsigned int Size();

    ~HsaInteropMemoryBuffer();

 private:
    // disable copy
    HsaInteropMemoryBuffer(const HsaInteropMemoryBuffer&);
    const HsaInteropMemoryBuffer& operator=(const HsaInteropMemoryBuffer&);

 private:
    unsigned long long m_Size;
    void* m_pBuf;
    unsigned long long m_graphic_handle;
    unsigned int m_Node;
};

// Class HsaNodeInfo - Gather and store all HSA node information from Thunk.
//
class HsaNodeInfo {
    // List containing HsaNodeProperties of all Nodes available
    std::vector<HsaNodeProperties*> m_HsaNodeProps;

    // List of HSA Nodes that contain a GPU. This includes both APU and dGPU
    std::vector<int> m_NodesWithGPU;

    // List of HSA Nodes with CPU only
    std::vector<int> m_NodesWithoutGPU;

 public:
    HsaNodeInfo();
    ~HsaNodeInfo();

    bool Init(int NumOfNodes);

    // This function should be soon depricated. This for transistion purpose only
    // Currently, KfdTest is designed to test only ONE node. This function acts
    // as transistion.
    const HsaNodeProperties* HsaDefaultGPUNodeProperties() const;
    const int HsaDefaultGPUNode() const;

    // Future use the following two functions to support multi-GPU.
    // const std::vector<int>& GpuNodes = GetNodesWithGPU()
    // for (..GpuNodes.size()..) GetNodeProperties(GpuNodes.at(i))
    //
    const std::vector<int>& GetNodesWithGPU() const;

    // @param node index of the node we are looking at
    // @param nodeProperties HsaNodeProperties returned
    const HsaNodeProperties* GetNodeProperties(int NodeNum) const;

    void PrintNodeInfo() const;
    const bool IsGPUNodeLargeBar(int node) const;
    // @brief Find the first available Large-BAR GPU node
    // @return: Node ID if successful or -1
    const int FindLargeBarGPUNode() const;
};

#endif  // __KFD__TEST__UTIL__H__
