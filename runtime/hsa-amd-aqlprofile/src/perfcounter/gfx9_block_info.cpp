#include "gfx9_block_info.h"
#include "gfxip/gfx9/gfx9_offset.h"
#include "gfxip/gfx9/gfx9_typedef.h"

namespace pm4_profile {
/**
 * Table containing CounterGroups which represent AI hardware blocks
 * as defined by \ref GpuBlockInfo structure
 */
GpuBlockInfo Gfx9HwBlocks[] = {
    // Counter block CB
    {"AI_CB0", kHsaAiCounterBlockIdCb0, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_CB,
     CntlMethodBySeAndInstance, 395, AI_COUNTER_NUM_PER_CB, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_CB1", kHsaAiCounterBlockIdCb1, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_CB,
     CntlMethodBySeAndInstance, 395, AI_COUNTER_NUM_PER_CB, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_CB2", kHsaAiCounterBlockIdCb2, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_CB,
     CntlMethodBySeAndInstance, 395, AI_COUNTER_NUM_PER_CB, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_CB3", kHsaAiCounterBlockIdCb3, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_CB,
     CntlMethodBySeAndInstance, 395, AI_COUNTER_NUM_PER_CB, 0, 0, true, 0, 0, false, 0, 0},

    // Temp commented for Vega10
    // Counter block CPF
    /*
    {"AI_CPF", kHsaAiCounterBlockIdCpf, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 19,
    AI_COUNTER_NUM_PER_CPF, 0, 0, true, 0, 0, false, 0, 0},
    */

    // Counter block DB
    {"AI_DB0", kHsaAiCounterBlockIdDb0, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_DB,
     CntlMethodBySeAndInstance, 256, AI_COUNTER_NUM_PER_DB, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_DB1", kHsaAiCounterBlockIdDb1, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_DB,
     CntlMethodBySeAndInstance, 256, AI_COUNTER_NUM_PER_DB, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_DB2", kHsaAiCounterBlockIdDb2, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_DB,
     CntlMethodBySeAndInstance, 256, AI_COUNTER_NUM_PER_DB, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_DB3", kHsaAiCounterBlockIdDb3, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_DB,
     CntlMethodBySeAndInstance, 256, AI_COUNTER_NUM_PER_DB, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block GRBM
    {"AI_GRBM", kHsaAiCounterBlockIdGrbm, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 33,
     AI_COUNTER_NUM_PER_GRBM, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block GRBMSE
    {"AI_GRBMSE", kHsaAiCounterBlockIdGrbmSe, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 14,
     AI_COUNTER_NUM_PER_GRBMSE, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block PA_SU
    {"AI_PA_SU", kHsaAiCounterBlockIdPaSu, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 152,
     AI_COUNTER_NUM_PER_PA_SU, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block PA_SC
    {"AI_PA_SC", kHsaAiCounterBlockIdPaSc, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 396,
     AI_COUNTER_NUM_PER_PA_SC, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block SPI
    {"AI_SPI", kHsaAiCounterBlockIdSpi, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 196,
     AI_COUNTER_NUM_PER_SPI, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block SQ
    {"AI_SQ", kHsaAiCounterBlockIdSq, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 171,
     AI_COUNTER_NUM_PER_SQ, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_SQ_GS", kHsaAiCounterBlockIdSqGs, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 298,
     AI_COUNTER_NUM_PER_SQ, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_SQ_VS", kHsaAiCounterBlockIdSqVs, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 298,
     AI_COUNTER_NUM_PER_SQ, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_SQ_PS", kHsaAiCounterBlockIdSqPs, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 298,
     AI_COUNTER_NUM_PER_SQ, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_SQ_HS", kHsaAiCounterBlockIdSqHs, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 298,
     AI_COUNTER_NUM_PER_SQ, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_SQ_CS", kHsaAiCounterBlockIdSqCs, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 298,
     AI_COUNTER_NUM_PER_SQ, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block SX
    {"AI_SX", kHsaAiCounterBlockIdSx, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 33,
     AI_COUNTER_NUM_PER_SX, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block TA
    {"AI_TA0", kHsaAiCounterBlockIdTa0, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TA,
     CntlMethodBySeAndInstance, 118, AI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TA1", kHsaAiCounterBlockIdTa1, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TA,
     CntlMethodBySeAndInstance, 118, AI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TA2", kHsaAiCounterBlockIdTa2, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TA,
     CntlMethodBySeAndInstance, 118, AI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TA3", kHsaAiCounterBlockIdTa3, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TA,
     CntlMethodBySeAndInstance, 118, AI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TA4", kHsaAiCounterBlockIdTa4, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TA,
     CntlMethodBySeAndInstance, 118, AI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TA5", kHsaAiCounterBlockIdTa5, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TA,
     CntlMethodBySeAndInstance, 118, AI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TA6", kHsaAiCounterBlockIdTa6, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TA,
     CntlMethodBySeAndInstance, 118, AI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TA7", kHsaAiCounterBlockIdTa7, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TA,
     CntlMethodBySeAndInstance, 118, AI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TA8", kHsaAiCounterBlockIdTa8, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TA,
     CntlMethodBySeAndInstance, 118, AI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TA9", kHsaAiCounterBlockIdTa9, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TA,
     CntlMethodBySeAndInstance, 118, AI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TA10", kHsaAiCounterBlockIdTa10, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TA,
     CntlMethodBySeAndInstance, 118, AI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TA11", kHsaAiCounterBlockIdTa11, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TA,
     CntlMethodBySeAndInstance, 118, AI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TA12", kHsaAiCounterBlockIdTa12, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TA,
     CntlMethodBySeAndInstance, 118, AI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TA13", kHsaAiCounterBlockIdTa13, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TA,
     CntlMethodBySeAndInstance, 118, AI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TA14", kHsaAiCounterBlockIdTa14, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TA,
     CntlMethodBySeAndInstance, 118, AI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TA15", kHsaAiCounterBlockIdTa15, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TA,
     CntlMethodBySeAndInstance, 118, AI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block TCA
    {"AI_TCA0", kHsaAiCounterBlockIdTca0, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCA,
     CntlMethodByInstance, 34, AI_COUNTER_NUM_PER_TCA, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCA1", kHsaAiCounterBlockIdTca1, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCA,
     CntlMethodByInstance, 34, AI_COUNTER_NUM_PER_TCA, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block TCC
    {"AI_TCC0", kHsaAiCounterBlockIdTcc0, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCC,
     CntlMethodByInstance, 191, AI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCC1", kHsaAiCounterBlockIdTcc1, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCC,
     CntlMethodByInstance, 191, AI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCC2", kHsaAiCounterBlockIdTcc2, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCC,
     CntlMethodByInstance, 191, AI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCC3", kHsaAiCounterBlockIdTcc3, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCC,
     CntlMethodByInstance, 191, AI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCC4", kHsaAiCounterBlockIdTcc4, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCC,
     CntlMethodByInstance, 191, AI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCC5", kHsaAiCounterBlockIdTcc5, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCC,
     CntlMethodByInstance, 191, AI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCC6", kHsaAiCounterBlockIdTcc6, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCC,
     CntlMethodByInstance, 191, AI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCC7", kHsaAiCounterBlockIdTcc7, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCC,
     CntlMethodByInstance, 191, AI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCC8", kHsaAiCounterBlockIdTcc8, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCC,
     CntlMethodByInstance, 191, AI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCC9", kHsaAiCounterBlockIdTcc9, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCC,
     CntlMethodByInstance, 191, AI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCC10", kHsaAiCounterBlockIdTcc10, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCC,
     CntlMethodByInstance, 191, AI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCC11", kHsaAiCounterBlockIdTcc11, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCC,
     CntlMethodByInstance, 191, AI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCC12", kHsaAiCounterBlockIdTcc12, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCC,
     CntlMethodByInstance, 191, AI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCC13", kHsaAiCounterBlockIdTcc13, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCC,
     CntlMethodByInstance, 191, AI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCC14", kHsaAiCounterBlockIdTcc14, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCC,
     CntlMethodByInstance, 191, AI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCC15", kHsaAiCounterBlockIdTcc15, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCC,
     CntlMethodByInstance, 191, AI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block TD
    {"AI_TD0", kHsaAiCounterBlockIdTd0, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TD,
     CntlMethodBySeAndInstance, 54, AI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TD1", kHsaAiCounterBlockIdTd1, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TD,
     CntlMethodBySeAndInstance, 54, AI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TD2", kHsaAiCounterBlockIdTd2, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TD,
     CntlMethodBySeAndInstance, 54, AI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TD3", kHsaAiCounterBlockIdTd3, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TD,
     CntlMethodBySeAndInstance, 54, AI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TD4", kHsaAiCounterBlockIdTd4, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TD,
     CntlMethodBySeAndInstance, 54, AI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TD5", kHsaAiCounterBlockIdTd5, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TD,
     CntlMethodBySeAndInstance, 54, AI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TD6", kHsaAiCounterBlockIdTd6, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TD,
     CntlMethodBySeAndInstance, 54, AI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TD7", kHsaAiCounterBlockIdTd7, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TD,
     CntlMethodBySeAndInstance, 54, AI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TD8", kHsaAiCounterBlockIdTd8, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TD,
     CntlMethodBySeAndInstance, 54, AI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TD9", kHsaAiCounterBlockIdTd9, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TD,
     CntlMethodBySeAndInstance, 54, AI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TD10", kHsaAiCounterBlockIdTd10, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TD,
     CntlMethodBySeAndInstance, 54, AI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TD11", kHsaAiCounterBlockIdTd11, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TD,
     CntlMethodBySeAndInstance, 54, AI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TD12", kHsaAiCounterBlockIdTd12, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TD,
     CntlMethodBySeAndInstance, 54, AI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TD13", kHsaAiCounterBlockIdTd13, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TD,
     CntlMethodBySeAndInstance, 54, AI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TD14", kHsaAiCounterBlockIdTd14, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TD,
     CntlMethodBySeAndInstance, 54, AI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TD15", kHsaAiCounterBlockIdTd15, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TD,
     CntlMethodBySeAndInstance, 54, AI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block TCP
    {"AI_TCP0", kHsaAiCounterBlockIdTcp0, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, AI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCP1", kHsaAiCounterBlockIdTcp1, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, AI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCP2", kHsaAiCounterBlockIdTcp2, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, AI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCP3", kHsaAiCounterBlockIdTcp3, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, AI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCP4", kHsaAiCounterBlockIdTcp4, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, AI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCP5", kHsaAiCounterBlockIdTcp5, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, AI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCP6", kHsaAiCounterBlockIdTcp6, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, AI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCP7", kHsaAiCounterBlockIdTcp7, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, AI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCP8", kHsaAiCounterBlockIdTcp8, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, AI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCP9", kHsaAiCounterBlockIdTcp9, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, AI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCP10", kHsaAiCounterBlockIdTcp10, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, AI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCP11", kHsaAiCounterBlockIdTcp11, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, AI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCP12", kHsaAiCounterBlockIdTcp12, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, AI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCP13", kHsaAiCounterBlockIdTcp13, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, AI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCP14", kHsaAiCounterBlockIdTcp14, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, AI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"AI_TCP15", kHsaAiCounterBlockIdTcp15, AI_MAX_NUM_SHADER_ENGINES, 2, AI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, AI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block GDS
    {"AI_GDS", kHsaAiCounterBlockIdGds, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 120,
     AI_COUNTER_NUM_PER_GDS, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block VGT
    {"AI_VGT", kHsaAiCounterBlockIdVgt, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 145,
     AI_COUNTER_NUM_PER_VGT, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block IA
    {"AI_IA", kHsaAiCounterBlockIdIa, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 23,
     AI_COUNTER_NUM_PER_IA, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block MC
    {"AI_MC", kHsaAiCounterBlockIdMc, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 22,
     AI_COUNTER_NUM_PER_MC, 0, 0, true, 0, 0, false, 0, 0},

    // Temp commented out for Vega10
    // Counter block SRBM
    /*
    {"AI_SRBM", kHsaAiCounterBlockIdSrbm, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 19,
    AI_COUNTER_NUM_PER_SRBM, 0, 0, true, 0, 0, false, 0, 0},
    */

    // Counter block WD
    {"AI_WD", kHsaAiCounterBlockIdWd, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 36,
     AI_COUNTER_NUM_PER_WD, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block CPG
    // Temp commented for Vega10
    /*
    {"AI_CPG", kHsaAiCounterBlockIdCpg, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 48,
    AI_COUNTER_NUM_PER_CPG, 0, 0, true, 0, 0, false, 0, 0},
    */

    // Counter block CPC
    // Temp commented for Vega10
    {"AI_CPC", kHsaAiCounterBlockIdCpc, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 34,
     AI_COUNTER_NUM_PER_CPC, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block IOMMUV2
    {"AI_IOMMUV2", kHsaAiCounterBlockIdIommuV2, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 25,
     8, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block KernelDriver
    {"AI_KD", kHsaAiCounterBlockIdKernelDriver, AI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 0,
     0, 0, 0, true, 0, 0, false, 0, 0},

    // Name of the last line should be empty to indicate end of all counter groups
    {"", kHsaAiCounterBlockIdBlocksLast, 0, 0, 0, CntlMethodNone, 0, 0, 0, 0, false, 0, 0, false, 0,
     0}};

extern const uint32_t Gfx9HwBlockCount = sizeof(Gfx9HwBlocks) / sizeof(GpuBlockInfo);

/*
 * The following tables contain register addresses of the SQ counter registers
 */

/*
 * SQ
 */
GpuCounterRegInfo AiSqCounterRegAddr[] = {
    {mmSQ_PERFCOUNTER0_SELECT, mmSQ_PERFCOUNTER_CTRL, mmSQ_PERFCOUNTER0_LO, mmSQ_PERFCOUNTER0_HI},
    {mmSQ_PERFCOUNTER1_SELECT, mmSQ_PERFCOUNTER_CTRL, mmSQ_PERFCOUNTER1_LO, mmSQ_PERFCOUNTER1_HI},
    {mmSQ_PERFCOUNTER2_SELECT, mmSQ_PERFCOUNTER_CTRL, mmSQ_PERFCOUNTER2_LO, mmSQ_PERFCOUNTER2_HI},
    {mmSQ_PERFCOUNTER3_SELECT, mmSQ_PERFCOUNTER_CTRL, mmSQ_PERFCOUNTER3_LO, mmSQ_PERFCOUNTER3_HI},
    {mmSQ_PERFCOUNTER4_SELECT, mmSQ_PERFCOUNTER_CTRL, mmSQ_PERFCOUNTER4_LO, mmSQ_PERFCOUNTER4_HI},
    {mmSQ_PERFCOUNTER5_SELECT, mmSQ_PERFCOUNTER_CTRL, mmSQ_PERFCOUNTER5_LO, mmSQ_PERFCOUNTER5_HI},
    {mmSQ_PERFCOUNTER6_SELECT, mmSQ_PERFCOUNTER_CTRL, mmSQ_PERFCOUNTER6_LO, mmSQ_PERFCOUNTER6_HI},
    {mmSQ_PERFCOUNTER7_SELECT, mmSQ_PERFCOUNTER_CTRL, mmSQ_PERFCOUNTER7_LO, mmSQ_PERFCOUNTER7_HI},
    {mmSQ_PERFCOUNTER8_SELECT, mmSQ_PERFCOUNTER_CTRL, mmSQ_PERFCOUNTER8_LO, mmSQ_PERFCOUNTER8_HI},
    {mmSQ_PERFCOUNTER9_SELECT, mmSQ_PERFCOUNTER_CTRL, mmSQ_PERFCOUNTER9_LO, mmSQ_PERFCOUNTER9_HI},
    {mmSQ_PERFCOUNTER10_SELECT, mmSQ_PERFCOUNTER_CTRL, mmSQ_PERFCOUNTER10_LO,
     mmSQ_PERFCOUNTER10_HI},
    {mmSQ_PERFCOUNTER11_SELECT, mmSQ_PERFCOUNTER_CTRL, mmSQ_PERFCOUNTER11_LO,
     mmSQ_PERFCOUNTER11_HI},
    {mmSQ_PERFCOUNTER12_SELECT, mmSQ_PERFCOUNTER_CTRL, mmSQ_PERFCOUNTER12_LO,
     mmSQ_PERFCOUNTER12_HI},
    {mmSQ_PERFCOUNTER13_SELECT, mmSQ_PERFCOUNTER_CTRL, mmSQ_PERFCOUNTER13_LO,
     mmSQ_PERFCOUNTER13_HI},
    {mmSQ_PERFCOUNTER14_SELECT, mmSQ_PERFCOUNTER_CTRL, mmSQ_PERFCOUNTER14_LO,
     mmSQ_PERFCOUNTER14_HI},
    {mmSQ_PERFCOUNTER15_SELECT, mmSQ_PERFCOUNTER_CTRL, mmSQ_PERFCOUNTER15_LO,
     mmSQ_PERFCOUNTER15_HI}};

/*
 * DRMDMA
 */
GpuCounterRegInfo AiDrmdmaCounterRegAddr[] = {
    {mmSDMA0_PERFMON_CNTL, 0, mmSDMA0_PERFCOUNTER0_RESULT, 0},
    {mmSDMA0_PERFMON_CNTL, 0, mmSDMA0_PERFCOUNTER1_RESULT, 0},
    {mmSDMA1_PERFMON_CNTL, 0, mmSDMA1_PERFCOUNTER0_RESULT, 0},
    {mmSDMA1_PERFMON_CNTL, 0, mmSDMA1_PERFCOUNTER1_RESULT, 0},
};

/*
 * IH
 */
GpuCounterRegInfo AiIhCounterRegAddr[] = {{mmIH_PERFMON_CNTL, 0, mmIH_PERFCOUNTER0_RESULT, 0},
                                          {mmIH_PERFMON_CNTL, 0, mmIH_PERFCOUNTER1_RESULT, 0}};

/*
 * CPF
 */
GpuCounterRegInfo AiCpfCounterRegAddr[] = {
    {mmCPF_PERFCOUNTER0_SELECT, 0, mmCPF_PERFCOUNTER0_LO, mmCPF_PERFCOUNTER0_HI},
    {mmCPF_PERFCOUNTER1_SELECT, 0, mmCPF_PERFCOUNTER1_LO, mmCPF_PERFCOUNTER1_HI}};

/*
 * DRM
 */
GpuCounterRegInfo AiDrmCounterRegAddr[] = {
    /*
    {mmDRM_PERFCOUNTER1_SELECT, 0, mmDRM_PERFCOUNTER1_LO, mmDRM_PERFCOUNTER1_HI},
    {mmDRM_PERFCOUNTER2_SELECT, 0, mmDRM_PERFCOUNTER2_LO, mmDRM_PERFCOUNTER2_HI}
    */
};

/*
 * GRBM
 */
GpuCounterRegInfo AiGrbmCounterRegAddr[] = {
    {mmGRBM_PERFCOUNTER0_SELECT, 0, mmGRBM_PERFCOUNTER0_LO, mmGRBM_PERFCOUNTER0_HI},
    {mmGRBM_PERFCOUNTER1_SELECT, 0, mmGRBM_PERFCOUNTER1_LO, mmGRBM_PERFCOUNTER1_HI}};

/*
 * GRBM_SE
 */
GpuCounterRegInfo AiGrbmSeCounterRegAddr[] = {
    {mmGRBM_SE0_PERFCOUNTER_SELECT, 0, mmGRBM_SE0_PERFCOUNTER_LO, mmGRBM_SE0_PERFCOUNTER_HI},
    {mmGRBM_SE1_PERFCOUNTER_SELECT, 0, mmGRBM_SE1_PERFCOUNTER_LO, mmGRBM_SE1_PERFCOUNTER_HI},
    {mmGRBM_SE2_PERFCOUNTER_SELECT, 0, mmGRBM_SE2_PERFCOUNTER_LO, mmGRBM_SE2_PERFCOUNTER_HI},
    {mmGRBM_SE3_PERFCOUNTER_SELECT, 0, mmGRBM_SE3_PERFCOUNTER_LO, mmGRBM_SE3_PERFCOUNTER_HI}};

/*
 * PA_SU
 */
GpuCounterRegInfo AiPaSuCounterRegAddr[] = {
    {mmPA_SU_PERFCOUNTER0_SELECT, 0, mmPA_SU_PERFCOUNTER0_LO, mmPA_SU_PERFCOUNTER0_HI},
    {mmPA_SU_PERFCOUNTER1_SELECT, 0, mmPA_SU_PERFCOUNTER1_LO, mmPA_SU_PERFCOUNTER1_HI},
    {mmPA_SU_PERFCOUNTER2_SELECT, 0, mmPA_SU_PERFCOUNTER2_LO, mmPA_SU_PERFCOUNTER2_HI},
    {mmPA_SU_PERFCOUNTER3_SELECT, 0, mmPA_SU_PERFCOUNTER3_LO, mmPA_SU_PERFCOUNTER3_HI}};

/*
 * PA_SC
 */
GpuCounterRegInfo AiPaScCounterRegAddr[] = {
    {mmPA_SC_PERFCOUNTER0_SELECT, 0, mmPA_SC_PERFCOUNTER0_LO, mmPA_SC_PERFCOUNTER0_HI},
    {mmPA_SC_PERFCOUNTER1_SELECT, 0, mmPA_SC_PERFCOUNTER1_LO, mmPA_SC_PERFCOUNTER1_HI},
    {mmPA_SC_PERFCOUNTER2_SELECT, 0, mmPA_SC_PERFCOUNTER2_LO, mmPA_SC_PERFCOUNTER2_HI},
    {mmPA_SC_PERFCOUNTER3_SELECT, 0, mmPA_SC_PERFCOUNTER3_LO, mmPA_SC_PERFCOUNTER3_HI}};

/*
 * SPI
 */
GpuCounterRegInfo AiSpiCounterRegAddr[] = {
    {mmSPI_PERFCOUNTER0_SELECT, 0, mmSPI_PERFCOUNTER0_LO, mmSPI_PERFCOUNTER0_HI},
    {mmSPI_PERFCOUNTER1_SELECT, 0, mmSPI_PERFCOUNTER1_LO, mmSPI_PERFCOUNTER1_HI},
    {mmSPI_PERFCOUNTER2_SELECT, 0, mmSPI_PERFCOUNTER2_LO, mmSPI_PERFCOUNTER2_HI},
    {mmSPI_PERFCOUNTER3_SELECT, 0, mmSPI_PERFCOUNTER3_LO, mmSPI_PERFCOUNTER3_HI},
    {mmSPI_PERFCOUNTER4_SELECT, 0, mmSPI_PERFCOUNTER4_LO, mmSPI_PERFCOUNTER4_HI},
    {mmSPI_PERFCOUNTER5_SELECT, 0, mmSPI_PERFCOUNTER5_LO, mmSPI_PERFCOUNTER5_HI}};

/*
 * TCA
 */
GpuCounterRegInfo AiTcaCounterRegAddr[] = {
    {mmTCA_PERFCOUNTER0_SELECT, 0, mmTCA_PERFCOUNTER0_LO, mmTCA_PERFCOUNTER0_HI},
    {mmTCA_PERFCOUNTER1_SELECT, 0, mmTCA_PERFCOUNTER1_LO, mmTCA_PERFCOUNTER1_HI},
    {mmTCA_PERFCOUNTER2_SELECT, 0, mmTCA_PERFCOUNTER2_LO, mmTCA_PERFCOUNTER2_HI},
    {mmTCA_PERFCOUNTER3_SELECT, 0, mmTCA_PERFCOUNTER3_LO, mmTCA_PERFCOUNTER3_HI}};

/*
 * TCC
 */
GpuCounterRegInfo AiTccCounterRegAddr[] = {
    {mmTCC_PERFCOUNTER0_SELECT, 0, mmTCC_PERFCOUNTER0_LO, mmTCC_PERFCOUNTER0_HI},
    {mmTCC_PERFCOUNTER1_SELECT, 0, mmTCC_PERFCOUNTER1_LO, mmTCC_PERFCOUNTER1_HI},
    {mmTCC_PERFCOUNTER2_SELECT, 0, mmTCC_PERFCOUNTER2_LO, mmTCC_PERFCOUNTER2_HI},
    {mmTCC_PERFCOUNTER3_SELECT, 0, mmTCC_PERFCOUNTER3_LO, mmTCC_PERFCOUNTER3_HI}};

/*
 * TCP
 */
GpuCounterRegInfo AiTcpCounterRegAddr[] = {
    {mmTCP_PERFCOUNTER0_SELECT, 0, mmTCP_PERFCOUNTER0_LO, mmTCP_PERFCOUNTER0_HI},
    {mmTCP_PERFCOUNTER1_SELECT, 0, mmTCP_PERFCOUNTER1_LO, mmTCP_PERFCOUNTER1_HI},
    {mmTCP_PERFCOUNTER2_SELECT, 0, mmTCP_PERFCOUNTER2_LO, mmTCP_PERFCOUNTER2_HI},
    {mmTCP_PERFCOUNTER3_SELECT, 0, mmTCP_PERFCOUNTER3_LO, mmTCP_PERFCOUNTER3_HI}};

/*
 * CB
 */
GpuCounterRegInfo AiCbCounterRegAddr[] = {
    {mmCB_PERFCOUNTER0_SELECT, 0, mmCB_PERFCOUNTER0_LO, mmCB_PERFCOUNTER0_HI},
    {mmCB_PERFCOUNTER1_SELECT, 0, mmCB_PERFCOUNTER1_LO, mmCB_PERFCOUNTER1_HI},
    {mmCB_PERFCOUNTER2_SELECT, 0, mmCB_PERFCOUNTER2_LO, mmCB_PERFCOUNTER2_HI},
    {mmCB_PERFCOUNTER3_SELECT, 0, mmCB_PERFCOUNTER3_LO, mmCB_PERFCOUNTER3_HI}};

/*
 * DB
 */
GpuCounterRegInfo AiDbCounterRegAddr[] = {
    {mmDB_PERFCOUNTER0_SELECT, 0, mmDB_PERFCOUNTER0_LO, mmDB_PERFCOUNTER0_HI},
    {mmDB_PERFCOUNTER1_SELECT, 0, mmDB_PERFCOUNTER1_LO, mmDB_PERFCOUNTER1_HI},
    {mmDB_PERFCOUNTER2_SELECT, 0, mmDB_PERFCOUNTER2_LO, mmDB_PERFCOUNTER2_HI},
    {mmDB_PERFCOUNTER3_SELECT, 0, mmDB_PERFCOUNTER3_LO, mmDB_PERFCOUNTER3_HI}};

/*
 * RLC
 */
GpuCounterRegInfo AiRlcCounterRegAddr[] = {
    {mmRLC_PERFCOUNTER0_SELECT, 0, mmRLC_PERFCOUNTER0_LO, mmRLC_PERFCOUNTER0_HI},
    {mmRLC_PERFCOUNTER1_SELECT, 0, mmRLC_PERFCOUNTER1_LO, mmRLC_PERFCOUNTER1_HI}};

/*
 * SC
 */
GpuCounterRegInfo AiScCounterRegAddr[] = {
    {mmPA_SC_PERFCOUNTER0_SELECT, 0, mmPA_SC_PERFCOUNTER0_LO, mmPA_SC_PERFCOUNTER0_HI},
    {mmPA_SC_PERFCOUNTER1_SELECT, 0, mmPA_SC_PERFCOUNTER1_LO, mmPA_SC_PERFCOUNTER1_HI},
    {mmPA_SC_PERFCOUNTER2_SELECT, 0, mmPA_SC_PERFCOUNTER2_LO, mmPA_SC_PERFCOUNTER2_HI},
    {mmPA_SC_PERFCOUNTER3_SELECT, 0, mmPA_SC_PERFCOUNTER3_LO, mmPA_SC_PERFCOUNTER3_HI},
    {mmPA_SC_PERFCOUNTER4_SELECT, 0, mmPA_SC_PERFCOUNTER4_LO, mmPA_SC_PERFCOUNTER4_HI},
    {mmPA_SC_PERFCOUNTER5_SELECT, 0, mmPA_SC_PERFCOUNTER5_LO, mmPA_SC_PERFCOUNTER5_HI},
    {mmPA_SC_PERFCOUNTER6_SELECT, 0, mmPA_SC_PERFCOUNTER6_LO, mmPA_SC_PERFCOUNTER6_HI},
    {mmPA_SC_PERFCOUNTER7_SELECT, 0, mmPA_SC_PERFCOUNTER7_LO, mmPA_SC_PERFCOUNTER7_HI}};

/*
 * SX
 */
GpuCounterRegInfo AiSxCounterRegAddr[] = {
    {mmSX_PERFCOUNTER0_SELECT, 0, mmSX_PERFCOUNTER0_LO, mmSX_PERFCOUNTER0_HI},
    {mmSX_PERFCOUNTER1_SELECT, 0, mmSX_PERFCOUNTER1_LO, mmSX_PERFCOUNTER1_HI},
    {mmSX_PERFCOUNTER2_SELECT, 0, mmSX_PERFCOUNTER2_LO, mmSX_PERFCOUNTER2_HI},
    {mmSX_PERFCOUNTER3_SELECT, 0, mmSX_PERFCOUNTER3_LO, mmSX_PERFCOUNTER3_HI}};

/*
 * TA
 */
GpuCounterRegInfo AiTaCounterRegAddr[] = {
    {mmTA_PERFCOUNTER0_SELECT, 0, mmTA_PERFCOUNTER0_LO, mmTA_PERFCOUNTER0_HI},
    {mmTA_PERFCOUNTER1_SELECT, 0, mmTA_PERFCOUNTER1_LO, mmTA_PERFCOUNTER1_HI}};

/*
 * TD
 */
GpuCounterRegInfo AiTdCounterRegAddr[] = {
    {mmTD_PERFCOUNTER0_SELECT, 0, mmTD_PERFCOUNTER0_LO, mmTD_PERFCOUNTER0_HI},
    {mmTD_PERFCOUNTER1_SELECT, 0, mmTD_PERFCOUNTER1_LO, mmTD_PERFCOUNTER1_HI}};

/*
 * GDS
 */
GpuCounterRegInfo AiGdsCounterRegAddr[] = {
    {mmGDS_PERFCOUNTER0_SELECT, 0, mmGDS_PERFCOUNTER0_LO, mmGDS_PERFCOUNTER0_HI},
    {mmGDS_PERFCOUNTER1_SELECT, 0, mmGDS_PERFCOUNTER1_LO, mmGDS_PERFCOUNTER1_HI},
    {mmGDS_PERFCOUNTER2_SELECT, 0, mmGDS_PERFCOUNTER2_LO, mmGDS_PERFCOUNTER2_HI},
    {mmGDS_PERFCOUNTER3_SELECT, 0, mmGDS_PERFCOUNTER3_LO, mmGDS_PERFCOUNTER3_HI}};

/*
 * VGT
 */
GpuCounterRegInfo AiVgtCounterRegAddr[] = {
    {mmVGT_PERFCOUNTER0_SELECT, 0, mmVGT_PERFCOUNTER0_LO, mmVGT_PERFCOUNTER0_HI},
    {mmVGT_PERFCOUNTER1_SELECT, 0, mmVGT_PERFCOUNTER1_LO, mmVGT_PERFCOUNTER1_HI},
    {mmVGT_PERFCOUNTER2_SELECT, 0, mmVGT_PERFCOUNTER2_LO, mmVGT_PERFCOUNTER2_HI},
    {mmVGT_PERFCOUNTER3_SELECT, 0, mmVGT_PERFCOUNTER3_LO, mmVGT_PERFCOUNTER3_HI}};

/*
 * IA
 */
GpuCounterRegInfo AiIaCounterRegAddr[] = {
    {mmIA_PERFCOUNTER0_SELECT, 0, mmIA_PERFCOUNTER0_LO, mmIA_PERFCOUNTER0_HI},
    {mmIA_PERFCOUNTER1_SELECT, 0, mmIA_PERFCOUNTER1_LO, mmIA_PERFCOUNTER1_HI},
    {mmIA_PERFCOUNTER2_SELECT, 0, mmIA_PERFCOUNTER2_LO, mmIA_PERFCOUNTER2_HI},
    {mmIA_PERFCOUNTER3_SELECT, 0, mmIA_PERFCOUNTER3_LO, mmIA_PERFCOUNTER3_HI}};

/*
 * MC
 */
GpuCounterRegInfo AiMcCounterRegAddr[] = {
    /*

    {mmMC_SEQ_PERF_SEQ_CTL__SI__VI, 0, mmMC_SEQ_PERF_SEQ_CNT_A_I0__VI,
     mmMC_SEQ_PERF_SEQ_CNT_A_I1__VI},
    {mmMC_SEQ_PERF_SEQ_CTL__SI__VI, 0, mmMC_SEQ_PERF_SEQ_CNT_B_I0__VI,
     mmMC_SEQ_PERF_SEQ_CNT_B_I1__VI},
    {mmMC_SEQ_PERF_SEQ_CTL__SI__VI, 0, mmMC_SEQ_PERF_SEQ_CNT_C_I0__VI,
     mmMC_SEQ_PERF_SEQ_CNT_C_I1__VI},
    {mmMC_SEQ_PERF_SEQ_CTL__SI__VI, 0, mmMC_SEQ_PERF_SEQ_CNT_D_I0__VI,
     mmMC_SEQ_PERF_SEQ_CNT_D_I1__VI}

     */
};

/*
 * SRBM
 */
GpuCounterRegInfo AiSrbmCounterRegAddr[] = {
    /*
    {mmSRBM_PERFCOUNTER0_SELECT, 0, mmSRBM_PERFCOUNTER0_LO,
     mmSRBM_PERFCOUNTER0_HI},
    {mmSRBM_PERFCOUNTER1_SELECT, 0, mmSRBM_PERFCOUNTER1_LO,
     mmSRBM_PERFCOUNTER1_HI}
     */
};

/*
 * WD
 */
GpuCounterRegInfo AiWdCounterRegAddr[] = {
    {mmWD_PERFCOUNTER0_SELECT, 0, mmWD_PERFCOUNTER0_LO, mmWD_PERFCOUNTER0_HI},
    {mmWD_PERFCOUNTER1_SELECT, 0, mmWD_PERFCOUNTER1_LO, mmWD_PERFCOUNTER1_HI},
    {mmWD_PERFCOUNTER2_SELECT, 0, mmWD_PERFCOUNTER2_LO, mmWD_PERFCOUNTER2_HI},
    {mmWD_PERFCOUNTER3_SELECT, 0, mmWD_PERFCOUNTER3_LO, mmWD_PERFCOUNTER3_HI}};

/*
 * CPG
 */
GpuCounterRegInfo AiCpgCounterRegAddr[] = {
    {mmCPG_PERFCOUNTER0_SELECT, 0, mmCPG_PERFCOUNTER0_LO, mmCPG_PERFCOUNTER0_HI},
    {mmCPG_PERFCOUNTER1_SELECT, 0, mmCPG_PERFCOUNTER1_LO, mmCPG_PERFCOUNTER1_HI}};

/*
 * CPC
 */
GpuCounterRegInfo AiCpcCounterRegAddr[] = {
    {mmCPC_PERFCOUNTER0_SELECT, 0, mmCPC_PERFCOUNTER0_LO, mmCPC_PERFCOUNTER0_HI},
    {mmCPC_PERFCOUNTER1_SELECT, 0, mmCPC_PERFCOUNTER1_LO, mmCPC_PERFCOUNTER1_HI}};

GpuPrivCounterBlockId AiBlockIdSq = {{0xb5c396b6, 0x47e4d310, 0xc35cfc86, 0x08f53a04}};
GpuPrivCounterBlockId AiBlockIdMc = {{0x13900b57, 0x4d984956, 0x5268d081, 0x9cf53719}};
GpuPrivCounterBlockId AiBlockIdIommuV2 = {{0x80969879, 0x4be6b0f6, 0x636af697, 0x1d10f500}};
GpuPrivCounterBlockId AiBlockIdKernelDriver = {{0xea9b5ae1, 0x44b36c3f, 0xf0da5489, 0x0aa96575}};

}  // pm4_profile
