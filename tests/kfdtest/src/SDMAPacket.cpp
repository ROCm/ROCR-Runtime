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

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "SDMAPacket.hpp"
#include "KFDTestUtil.hpp"

/* Byte/dword count in many SDMA packets is 1-based in AI, meaning a
 * count of 1 is encoded as 0.
 */
#define SDMA_COUNT(c) (g_TestGPUFamilyId < FAMILY_AI ? (c) : (c)-1)

SDMAWriteDataPacket::SDMAWriteDataPacket(void):
    packetData(NULL) {
}

SDMAWriteDataPacket::SDMAWriteDataPacket(void* destAddr, unsigned int data):
    packetData(NULL) {
    InitPacket(destAddr, data);
}

SDMAWriteDataPacket::SDMAWriteDataPacket(void* destAddr, unsigned int ndw,
                                         void *data):
    packetData(NULL) {
    InitPacket(destAddr, ndw, data);
}

SDMAWriteDataPacket::~SDMAWriteDataPacket(void) {
    if (packetData)
        free(packetData);
}

void SDMAWriteDataPacket::InitPacket(void* destAddr, unsigned int data) {
    InitPacket(destAddr, 1, &data);
}

void SDMAWriteDataPacket::InitPacket(void* destAddr, unsigned int ndw,
                                     void *data) {
    packetSize = sizeof(SDMA_PKT_WRITE_UNTILED) +
        (ndw - 1) * sizeof(unsigned int);
    packetData = reinterpret_cast<SDMA_PKT_WRITE_UNTILED *>(calloc(1, packetSize));

    packetData->HEADER_UNION.op = SDMA_OP_WRITE;
    packetData->HEADER_UNION.sub_op = SDMA_SUBOP_WRITE_LINEAR;

    SplitU64(reinterpret_cast<HSAuint64>(destAddr),
             packetData->DST_ADDR_LO_UNION.DW_1_DATA,  // dst_addr_31_0
             packetData->DST_ADDR_HI_UNION.DW_2_DATA);  // dst_addr_63_32

    packetData->DW_3_UNION.count = SDMA_COUNT(ndw);
    memcpy(&packetData->DATA0_UNION.DW_4_DATA, data, ndw*sizeof(unsigned int));
}

#define BITS (21)
#define TWO_MEG (1 << BITS)
SDMACopyDataPacket::~SDMACopyDataPacket(void) {
    free(packetData);
}

SDMACopyDataPacket::SDMACopyDataPacket(void *const dsts[], void *src, int n, unsigned int surfsize) {
    int32_t size = 0, i;
    void **dst = reinterpret_cast<void**>(malloc(sizeof(void*) * n));
    const int singlePacketSize = sizeof(SDMA_PKT_COPY_LINEAR) +
                        sizeof(SDMA_PKT_COPY_LINEAR::DST_ADDR[0]) * n;

    if (n > 2)
        WARN() << "SDMACopyDataPacket does not support more than 2 dst addresses!" << std::endl;

    memcpy(dst, dsts, sizeof(void*) * n);

    packetSize = ((surfsize + TWO_MEG - 1) >> BITS) * singlePacketSize;

    SDMA_PKT_COPY_LINEAR *pSDMA = reinterpret_cast<SDMA_PKT_COPY_LINEAR *>(malloc(packetSize));
    packetData = pSDMA;

    while (surfsize > 0) {
        /* SDMA support maximum 0x3fffe0 byte in one copy, take 2M here */
        if (surfsize > TWO_MEG)
            size = TWO_MEG;
        else
            size = surfsize;

        memset(pSDMA, 0, singlePacketSize);
        pSDMA->HEADER_UNION.op           = SDMA_OP_COPY;
        pSDMA->HEADER_UNION.sub_op       = SDMA_SUBOP_COPY_LINEAR;
        pSDMA->HEADER_UNION.broadcast       = n > 1 ? 1 : 0;
        pSDMA->COUNT_UNION.count             = SDMA_COUNT(size);
        SplitU64(reinterpret_cast<HSAuint64>(src),
                 pSDMA->SRC_ADDR_LO_UNION.DW_3_DATA,  // src_addr_31_0
                 pSDMA->SRC_ADDR_HI_UNION.DW_4_DATA);  // src_addr_63_32

        for (i = 0; i < n; i++)
            SplitU64(reinterpret_cast<HSAuint64>(dst[i]),
                    pSDMA->DST_ADDR[i].DST_ADDR_LO_UNION.DW_5_DATA,  // dst_addr_31_0
                    pSDMA->DST_ADDR[i].DST_ADDR_HI_UNION.DW_6_DATA);  // dst_addr_63_32

        pSDMA = reinterpret_cast<SDMA_PKT_COPY_LINEAR *>(reinterpret_cast<char *>(pSDMA) + singlePacketSize);
        for (i = 0; i < n; i++)
            dst[i] = reinterpret_cast<char *>(dst[i]) + size;
        src = reinterpret_cast<char *>(src) + size;
        surfsize -= size;
    }
    free(dst);
}

SDMACopyDataPacket::SDMACopyDataPacket(void* dst, void *src, unsigned int surfsize) {
    new (this)SDMACopyDataPacket(&dst, src, 1, surfsize);
}

SDMAFillDataPacket::~SDMAFillDataPacket() {
    free(m_PacketData);
}

SDMAFillDataPacket::SDMAFillDataPacket(void *dst, unsigned int data, unsigned int size) {
    unsigned int copy_size;
    SDMA_PKT_CONSTANT_FILL *pSDMA;

    /* SDMA support maximum 0x3fffe0 byte in one copy. Use 2M copy_size */
    m_PacketSize = ((size + TWO_MEG - 1) >> BITS) * sizeof(SDMA_PKT_CONSTANT_FILL);
    pSDMA = reinterpret_cast<SDMA_PKT_CONSTANT_FILL *>(calloc(1, m_PacketSize));
    m_PacketData = pSDMA;

    while (size > 0) {
        if (size > TWO_MEG)
            copy_size = TWO_MEG;
        else
            copy_size = size;

        pSDMA->HEADER_UNION.op = SDMA_OP_CONST_FILL;
        pSDMA->HEADER_UNION.sub_op = 0;

        /* If both size and address are DW aligned, then use DW fill */
        if (!(copy_size & 0x3) && !((HSAuint64)dst & 0x3))
            pSDMA->HEADER_UNION.fillsize = 2; /* DW Fill */
        else
            pSDMA->HEADER_UNION.fillsize = 0; /* Byte Fill */

        pSDMA->COUNT_UNION.count = SDMA_COUNT(copy_size);

        SplitU64(reinterpret_cast<HSAuint64>(dst),
            pSDMA->DST_ADDR_LO_UNION.DW_1_DATA, /*dst_addr_31_0*/
            pSDMA->DST_ADDR_HI_UNION.DW_2_DATA); /*dst_addr_63_32*/

        pSDMA->DATA_UNION.DW_3_DATA = data;
        pSDMA++;

        dst = reinterpret_cast<char *>(dst) + copy_size;
        size -= copy_size;
    }
}

SDMAFencePacket::SDMAFencePacket(void) {
}

SDMAFencePacket::SDMAFencePacket(void* destAddr, unsigned int data) {
    InitPacket(destAddr, data);
}

SDMAFencePacket::~SDMAFencePacket(void) {
}

void SDMAFencePacket::InitPacket(void* destAddr, unsigned int data) {
    memset(&packetData, 0, SizeInBytes());

    packetData.HEADER_UNION.op = SDMA_OP_FENCE;

    SplitU64(reinterpret_cast<HSAuint64>(destAddr),
             packetData.ADDR_LO_UNION.DW_1_DATA, /*dst_addr_31_0*/
             packetData.ADDR_HI_UNION.DW_2_DATA); /*dst_addr_63_32*/

    packetData.DATA_UNION.data = data;
}


SDMATrapPacket::SDMATrapPacket(unsigned int eventID) {
    InitPacket(eventID);
}

SDMATrapPacket::~SDMATrapPacket(void) {
}

void SDMATrapPacket::InitPacket(unsigned int eventID) {
    memset(&packetData, 0, SizeInBytes());

    packetData.HEADER_UNION.op = SDMA_OP_TRAP;
    packetData.INT_CONTEXT_UNION.int_context = eventID;
}

SDMATimePacket::SDMATimePacket(void *destaddr) {
    InitPacket(destaddr);
}

SDMATimePacket::~SDMATimePacket(void) {
}

void SDMATimePacket::InitPacket(void *destaddr) {
    memset(&packetData, 0, SizeInBytes());

    packetData.HEADER_UNION.op = SDMA_OP_TIMESTAMP;
    packetData.HEADER_UNION.sub_op = 1 << 1; /* Get Global GPU Timestamp*/

    if (reinterpret_cast<unsigned long long>(destaddr) & 0x1f)
        WARN() << "SDMATimePacket dst address must aligned to 32bytes boundary" << std::endl;

    SplitU64(reinterpret_cast<unsigned long long>(destaddr),
            packetData.ADDR_LO_UNION.DW_1_DATA, /*dst_addr_31_0*/
            packetData.ADDR_HI_UNION.DW_2_DATA); /*dst_addr_63_32*/
}
