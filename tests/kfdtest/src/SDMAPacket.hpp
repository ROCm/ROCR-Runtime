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

#ifndef __KFD_SDMA_PACKET__H__
#define __KFD_SDMA_PACKET__H__

#include "BasePacket.hpp"
#include "sdma_pkt_struct.h"

// @class SDMSPacket: marks a group of all SDMA packets
class SDMAPacket : public BasePacket {
 public:
        SDMAPacket(void) {}
        virtual ~SDMAPacket(void) {}

        virtual PACKETTYPE PacketType() const { return PACKETTYPE_SDMA; }
};

class SDMAWriteDataPacket : public SDMAPacket {
 public:
    // empty constructor, befor using the packet call the init func
    SDMAWriteDataPacket(void);
    // this contructor will also init the packet, no need for adittional calls
    SDMAWriteDataPacket(void* destAddr, unsigned int data);
    SDMAWriteDataPacket(void* destAddr, unsigned int ndw, void *data);

    virtual ~SDMAWriteDataPacket(void);

    // @returns a pointer to the packet
    virtual const void *GetPacket() const  { return packetData; }
    // @breif initialise the packet
    void InitPacket(void* destAddr, unsigned int data);
    void InitPacket(void* destAddr, unsigned int ndw, void *data);
    // @returns the packet size in bytes
    virtual unsigned int SizeInBytes() const { return packetSize; }

 protected:
    // SDMA_PKT_WRITE_UNTILED struct contains all the packets data
    SDMA_PKT_WRITE_UNTILED *packetData;
    unsigned int packetSize;
};

class SDMACopyDataPacket : public SDMAPacket {
 public:
    // this contructor will also init the packet, no need for adittional calls
    SDMACopyDataPacket(void *dest, void *src, unsigned int size);

    virtual ~SDMACopyDataPacket(void);

    // @returns a pointer to the packet
    virtual const void *GetPacket() const  { return packetData; }

    // @returns the packet size in bytes
    virtual unsigned int SizeInBytes() const { return packetSize; }

 protected:
    // SDMA_PKT_COPY_LINEAR struct contains all the packets data
    SDMA_PKT_COPY_LINEAR  *packetData;

    unsigned int packetSize;
};

class SDMAFillDataPacket : public SDMAPacket {
 public:
    // this contructor will also init the packet, no need for adittional calls
    SDMAFillDataPacket(void *dest, unsigned int data, unsigned int size);

    virtual ~SDMAFillDataPacket(void);

    // @returns a pointer to the packet
    virtual const void *GetPacket() const  { return m_PacketData; }

    // @returns the packet size in bytes
    virtual unsigned int SizeInBytes() const { return m_PacketSize; }

 protected:
    // SDMA_PKT_CONSTANT_FILL struct contains all the packets data
    SDMA_PKT_CONSTANT_FILL  *m_PacketData;

    unsigned int m_PacketSize;
};

class SDMAFencePacket : public SDMAPacket {
 public:
    // empty constructor, befor using the packet call the init func
    SDMAFencePacket(void);
    // this contructor will also init the packet, no need for adittional calls
    SDMAFencePacket(void* destAddr, unsigned int data);

    virtual ~SDMAFencePacket(void);

    // @returns a pointer to the packet
    virtual const void *GetPacket() const  { return &packetData; }
    // @brief initialise the packet
    void InitPacket(void* destAddr, unsigned int data);
    // @returns the packet size in bytes
    virtual unsigned int SizeInBytes() const { return sizeof(SDMA_PKT_FENCE ); }

 protected:
    // SDMA_PKT_FENCE struct contains all the packets data
    SDMA_PKT_FENCE  packetData;
};

class SDMATrapPacket : public SDMAPacket {
 public:
    // empty constructor, befor using the packet call the init func
    SDMATrapPacket(unsigned int eventID = 0);

    virtual ~SDMATrapPacket(void);

    // @returns a pointer to the packet
    virtual const void *GetPacket() const  { return &packetData; }
    // @brief initialise the packet
    void InitPacket(unsigned int eventID);
    // @returns the packet size in bytes
    virtual unsigned int SizeInBytes() const { return sizeof(SDMA_PKT_TRAP); }

 protected:
    // SDMA_PKT_TRAP struct contains all the packets data
    SDMA_PKT_TRAP  packetData;
};

#endif  // __KFD_SDMA_PACKET__H__
