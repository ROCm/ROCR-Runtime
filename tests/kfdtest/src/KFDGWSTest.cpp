/*
 * Copyright (C) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "KFDGWSTest.hpp"
#include "PM4Queue.hpp"
#include "PM4Packet.hpp"
#include "Dispatch.hpp"

/* Shader to initialize gws counter to 1*/
const char* gfx9_10_GwsInit =
"\
shader GwsInit\n\
type(CS)\n\
wave_size(32)\n\
    s_mov_b32 m0, 0\n\
    s_nop 0\n\
    s_load_dword s16, s[0:1], 0x0 glc\n\
    s_waitcnt 0\n\
    v_mov_b32 v0, s16\n\
    s_waitcnt 0\n\
    ds_gws_init v0 gds:1 offset0:0\n\
    s_waitcnt 0\n\
    s_endpgm\n\
    end\n\
";

/* Atomically increase a value in memory
 * This is expected to be executed from
 * multiple work groups simultaneously.
 * GWS semaphore is used to guarantee
 * the operation is atomic.
 */
const char* gfx9_AtomicIncrease =
"\
shader AtomicIncrease\n\
asic(GFX9)\n\
type(CS)\n\
/* Assume src address in s0, s1 */\n\
    s_mov_b32 m0, 0\n\
    s_nop 0\n\
    ds_gws_sema_p gds:1 offset0:0\n\
    s_waitcnt 0\n\
    s_load_dword s16, s[0:1], 0x0 glc\n\
    s_waitcnt 0\n\
    s_add_u32 s16, s16, 1\n\
    s_store_dword s16, s[0:1], 0x0 glc\n\
    s_waitcnt lgkmcnt(0)\n\
    ds_gws_sema_v gds:1 offset0:0\n\
    s_waitcnt 0\n\
    s_endpgm\n\
    end\n\
";

const char* gfx10_AtomicIncrease =
"\
shader AtomicIncrease\n\
asic(GFX10)\n\
type(CS)\n\
wave_size(32)\n\
/* Assume src address in s0, s1 */\n\
    s_mov_b32 m0, 0\n\
    s_mov_b32 exec_lo, 0x1\n\
    v_mov_b32 v0, s0\n\
    v_mov_b32 v1, s1\n\
    ds_gws_sema_p gds:1 offset0:0\n\
    s_waitcnt 0\n\
    flat_load_dword v2, v[0:1] glc:1 dlc:1\n\
    s_waitcnt 0\n\
    v_add_nc_u32 v2, v2, 1\n\
    flat_store_dword v[0:1], v2\n\
    s_waitcnt_vscnt null, 0\n\
    ds_gws_sema_v gds:1 offset0:0\n\
    s_waitcnt 0\n\
    s_endpgm\n\
    end\n\
";

void KFDGWSTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    m_pIsaGen = IsaGenerator::Create(m_FamilyId);

    ROUTINE_END
}

void KFDGWSTest::TearDown() {
    ROUTINE_START

    if (m_pIsaGen)
        delete m_pIsaGen;
    m_pIsaGen = NULL;

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

TEST_F(KFDGWSTest, Allocate) {
    TEST_START(TESTPROFILE_RUNALL);

    HSAuint32 firstGWS;
    PM4Queue queue;
    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";
    const HsaNodeProperties *pNodeProperties = m_NodeInfo.HsaDefaultGPUNodeProperties();
    if (!pNodeProperties || !pNodeProperties->NumGws) {
        LOG() << "Skip test: GPU node doesn't support GWS" << std::endl;
        return;
    }

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));
    ASSERT_SUCCESS(hsaKmtAllocQueueGWS(queue.GetResource()->QueueId,
			    pNodeProperties->NumGws,&firstGWS));
    EXPECT_EQ(0, firstGWS);
    EXPECT_SUCCESS(queue.Destroy());

    TEST_END
}

TEST_F(KFDGWSTest, Semaphore) {
    TEST_START(TESTPROFILE_RUNALL);

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";
    const HsaNodeProperties *pNodeProperties = m_NodeInfo.HsaDefaultGPUNodeProperties();
    HSAuint32 firstGWS;
    HSAuint32 numResources = 1;
    PM4Queue queue;

    if (!pNodeProperties || !pNodeProperties->NumGws) {
        LOG() << "Skip test: GPU node doesn't support GWS" << std::endl;
        return;
    }

    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
    HsaMemoryBuffer buffer(PAGE_SIZE, defaultGPUNode, true, false, false);
    ASSERT_SUCCESS(queue.Create(defaultGPUNode));
    ASSERT_SUCCESS(hsaKmtAllocQueueGWS(queue.GetResource()->QueueId,
			    pNodeProperties->NumGws,&firstGWS));
    EXPECT_EQ(0, firstGWS);

    m_pIsaGen = IsaGenerator::Create(m_FamilyId);
    m_pIsaGen->CompileShader(gfx9_10_GwsInit, "GwsInit", isaBuffer);
    Dispatch dispatch0(isaBuffer);
    buffer.Fill(numResources, 0, 4);
    dispatch0.SetArgs(buffer.As<void*>(), NULL);
    dispatch0.Submit(queue);
    dispatch0.Sync();

    const char *pAtomicIncrease;
    if (m_FamilyId <= FAMILY_AL)
        pAtomicIncrease = gfx9_AtomicIncrease;
    else
        pAtomicIncrease = gfx10_AtomicIncrease;

    m_pIsaGen->CompileShader(pAtomicIncrease, "AtomicIncrease", isaBuffer);

    Dispatch dispatch(isaBuffer);
    dispatch.SetArgs(buffer.As<void*>(), NULL);
    dispatch.SetDim(1024, 16, 16);

    dispatch.Submit(queue);
    dispatch.Sync();

    EXPECT_EQ(1024*16*16+1, *buffer.As<uint32_t *>());
    EXPECT_SUCCESS(queue.Destroy());

    TEST_END
}

