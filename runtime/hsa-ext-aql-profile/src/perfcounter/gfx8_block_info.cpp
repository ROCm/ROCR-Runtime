#include "gfx8_block_info.h"
#include "gfxip/gfx8/si_ci_vi_merged_offset.h"

namespace pm4_profile {
/**
 * Table containing CounterGroups which represent VI hardware blocks
 * as defined by \ref GpuBlockInfo structure
 */
GpuBlockInfo Gfx8HwBlocks[] = {
    // Counter block CB
    {"VI_CB0", kHsaViCounterBlockIdCb0, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_CB,
     CntlMethodBySeAndInstance, 395, VI_COUNTER_NUM_PER_CB, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_CB1", kHsaViCounterBlockIdCb1, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_CB,
     CntlMethodBySeAndInstance, 395, VI_COUNTER_NUM_PER_CB, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_CB2", kHsaViCounterBlockIdCb2, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_CB,
     CntlMethodBySeAndInstance, 395, VI_COUNTER_NUM_PER_CB, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_CB3", kHsaViCounterBlockIdCb3, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_CB,
     CntlMethodBySeAndInstance, 395, VI_COUNTER_NUM_PER_CB, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block CPF
    {"VI_CPF", kHsaViCounterBlockIdCpf, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 19,
     VI_COUNTER_NUM_PER_CPF, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block DB
    {"VI_DB0", kHsaViCounterBlockIdDb0, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_DB,
     CntlMethodBySeAndInstance, 256, VI_COUNTER_NUM_PER_DB, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_DB1", kHsaViCounterBlockIdDb1, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_DB,
     CntlMethodBySeAndInstance, 256, VI_COUNTER_NUM_PER_DB, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_DB2", kHsaViCounterBlockIdDb2, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_DB,
     CntlMethodBySeAndInstance, 256, VI_COUNTER_NUM_PER_DB, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_DB3", kHsaViCounterBlockIdDb3, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_DB,
     CntlMethodBySeAndInstance, 256, VI_COUNTER_NUM_PER_DB, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block GRBM
    {"VI_GRBM", kHsaViCounterBlockIdGrbm, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 33,
     VI_COUNTER_NUM_PER_GRBM, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block GRBMSE
    {"VI_GRBMSE", kHsaViCounterBlockIdGrbmSe, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 14,
     VI_COUNTER_NUM_PER_GRBMSE, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block PA_SU
    {"VI_PA_SU", kHsaViCounterBlockIdPaSu, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 152,
     VI_COUNTER_NUM_PER_PA_SU, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block PA_SC
    {"VI_PA_SC", kHsaViCounterBlockIdPaSc, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 396,
     VI_COUNTER_NUM_PER_PA_SC, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block SPI
    {"VI_SPI", kHsaViCounterBlockIdSpi, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 196,
     VI_COUNTER_NUM_PER_SPI, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block SQ
    {"VI_SQ", kHsaViCounterBlockIdSq, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 298,
     VI_COUNTER_NUM_PER_SQ, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_SQ_ES", kHsaViCounterBlockIdSqEs, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 298,
     VI_COUNTER_NUM_PER_SQ, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_SQ_GS", kHsaViCounterBlockIdSqGs, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 298,
     VI_COUNTER_NUM_PER_SQ, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_SQ_VS", kHsaViCounterBlockIdSqVs, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 298,
     VI_COUNTER_NUM_PER_SQ, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_SQ_PS", kHsaViCounterBlockIdSqPs, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 298,
     VI_COUNTER_NUM_PER_SQ, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_SQ_LS", kHsaViCounterBlockIdSqLs, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 298,
     VI_COUNTER_NUM_PER_SQ, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_SQ_HS", kHsaViCounterBlockIdSqHs, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 298,
     VI_COUNTER_NUM_PER_SQ, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_SQ_CS", kHsaViCounterBlockIdSqCs, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 298,
     VI_COUNTER_NUM_PER_SQ, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block SX
    {"VI_SX", kHsaViCounterBlockIdSx, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 33,
     VI_COUNTER_NUM_PER_SX, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block TA
    {"VI_TA0", kHsaViCounterBlockIdTa0, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TA,
     CntlMethodBySeAndInstance, 118, VI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TA1", kHsaViCounterBlockIdTa1, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TA,
     CntlMethodBySeAndInstance, 118, VI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TA2", kHsaViCounterBlockIdTa2, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TA,
     CntlMethodBySeAndInstance, 118, VI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TA3", kHsaViCounterBlockIdTa3, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TA,
     CntlMethodBySeAndInstance, 118, VI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TA4", kHsaViCounterBlockIdTa4, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TA,
     CntlMethodBySeAndInstance, 118, VI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TA5", kHsaViCounterBlockIdTa5, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TA,
     CntlMethodBySeAndInstance, 118, VI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TA6", kHsaViCounterBlockIdTa6, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TA,
     CntlMethodBySeAndInstance, 118, VI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TA7", kHsaViCounterBlockIdTa7, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TA,
     CntlMethodBySeAndInstance, 118, VI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TA8", kHsaViCounterBlockIdTa8, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TA,
     CntlMethodBySeAndInstance, 118, VI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TA9", kHsaViCounterBlockIdTa9, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TA,
     CntlMethodBySeAndInstance, 118, VI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TA10", kHsaViCounterBlockIdTa10, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TA,
     CntlMethodBySeAndInstance, 118, VI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TA11", kHsaViCounterBlockIdTa11, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TA,
     CntlMethodBySeAndInstance, 118, VI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TA12", kHsaViCounterBlockIdTa12, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TA,
     CntlMethodBySeAndInstance, 118, VI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TA13", kHsaViCounterBlockIdTa13, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TA,
     CntlMethodBySeAndInstance, 118, VI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TA14", kHsaViCounterBlockIdTa14, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TA,
     CntlMethodBySeAndInstance, 118, VI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TA15", kHsaViCounterBlockIdTa15, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TA,
     CntlMethodBySeAndInstance, 118, VI_COUNTER_NUM_PER_TA, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block TCA
    {"VI_TCA0", kHsaViCounterBlockIdTca0, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCA,
     CntlMethodByInstance, 34, VI_COUNTER_NUM_PER_TCA, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCA1", kHsaViCounterBlockIdTca1, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCA,
     CntlMethodByInstance, 34, VI_COUNTER_NUM_PER_TCA, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block TCC
    {"VI_TCC0", kHsaViCounterBlockIdTcc0, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCC,
     CntlMethodByInstance, 191, VI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCC1", kHsaViCounterBlockIdTcc1, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCC,
     CntlMethodByInstance, 191, VI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCC2", kHsaViCounterBlockIdTcc2, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCC,
     CntlMethodByInstance, 191, VI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCC3", kHsaViCounterBlockIdTcc3, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCC,
     CntlMethodByInstance, 191, VI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCC4", kHsaViCounterBlockIdTcc4, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCC,
     CntlMethodByInstance, 191, VI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCC5", kHsaViCounterBlockIdTcc5, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCC,
     CntlMethodByInstance, 191, VI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCC6", kHsaViCounterBlockIdTcc6, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCC,
     CntlMethodByInstance, 191, VI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCC7", kHsaViCounterBlockIdTcc7, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCC,
     CntlMethodByInstance, 191, VI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCC8", kHsaViCounterBlockIdTcc8, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCC,
     CntlMethodByInstance, 191, VI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCC9", kHsaViCounterBlockIdTcc9, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCC,
     CntlMethodByInstance, 191, VI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCC10", kHsaViCounterBlockIdTcc10, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCC,
     CntlMethodByInstance, 191, VI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCC11", kHsaViCounterBlockIdTcc11, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCC,
     CntlMethodByInstance, 191, VI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCC12", kHsaViCounterBlockIdTcc12, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCC,
     CntlMethodByInstance, 191, VI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCC13", kHsaViCounterBlockIdTcc13, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCC,
     CntlMethodByInstance, 191, VI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCC14", kHsaViCounterBlockIdTcc14, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCC,
     CntlMethodByInstance, 191, VI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCC15", kHsaViCounterBlockIdTcc15, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCC,
     CntlMethodByInstance, 191, VI_COUNTER_NUM_PER_TCC, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block TD
    {"VI_TD0", kHsaViCounterBlockIdTd0, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TD,
     CntlMethodBySeAndInstance, 54, VI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TD1", kHsaViCounterBlockIdTd1, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TD,
     CntlMethodBySeAndInstance, 54, VI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TD2", kHsaViCounterBlockIdTd2, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TD,
     CntlMethodBySeAndInstance, 54, VI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TD3", kHsaViCounterBlockIdTd3, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TD,
     CntlMethodBySeAndInstance, 54, VI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TD4", kHsaViCounterBlockIdTd4, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TD,
     CntlMethodBySeAndInstance, 54, VI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TD5", kHsaViCounterBlockIdTd5, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TD,
     CntlMethodBySeAndInstance, 54, VI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TD6", kHsaViCounterBlockIdTd6, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TD,
     CntlMethodBySeAndInstance, 54, VI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TD7", kHsaViCounterBlockIdTd7, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TD,
     CntlMethodBySeAndInstance, 54, VI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TD8", kHsaViCounterBlockIdTd8, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TD,
     CntlMethodBySeAndInstance, 54, VI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TD9", kHsaViCounterBlockIdTd9, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TD,
     CntlMethodBySeAndInstance, 54, VI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TD10", kHsaViCounterBlockIdTd10, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TD,
     CntlMethodBySeAndInstance, 54, VI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TD11", kHsaViCounterBlockIdTd11, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TD,
     CntlMethodBySeAndInstance, 54, VI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TD12", kHsaViCounterBlockIdTd12, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TD,
     CntlMethodBySeAndInstance, 54, VI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TD13", kHsaViCounterBlockIdTd13, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TD,
     CntlMethodBySeAndInstance, 54, VI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TD14", kHsaViCounterBlockIdTd14, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TD,
     CntlMethodBySeAndInstance, 54, VI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TD15", kHsaViCounterBlockIdTd15, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TD,
     CntlMethodBySeAndInstance, 54, VI_COUNTER_NUM_PER_TD, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block TCP
    {"VI_TCP0", kHsaViCounterBlockIdTcp0, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, VI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCP1", kHsaViCounterBlockIdTcp1, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, VI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCP2", kHsaViCounterBlockIdTcp2, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, VI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCP3", kHsaViCounterBlockIdTcp3, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, VI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCP4", kHsaViCounterBlockIdTcp4, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, VI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCP5", kHsaViCounterBlockIdTcp5, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, VI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCP6", kHsaViCounterBlockIdTcp6, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, VI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCP7", kHsaViCounterBlockIdTcp7, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, VI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCP8", kHsaViCounterBlockIdTcp8, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, VI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCP9", kHsaViCounterBlockIdTcp9, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, VI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCP10", kHsaViCounterBlockIdTcp10, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, VI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCP11", kHsaViCounterBlockIdTcp11, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, VI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCP12", kHsaViCounterBlockIdTcp12, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, VI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCP13", kHsaViCounterBlockIdTcp13, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, VI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCP14", kHsaViCounterBlockIdTcp14, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, VI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},
    {"VI_TCP15", kHsaViCounterBlockIdTcp15, VI_MAX_NUM_SHADER_ENGINES, 2, VI_NUM_TCP,
     CntlMethodBySeAndInstance, 182, VI_COUNTER_NUM_PER_TCP, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block GDS
    {"VI_GDS", kHsaViCounterBlockIdGds, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 120,
     VI_COUNTER_NUM_PER_GDS, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block VGT
    {"VI_VGT", kHsaViCounterBlockIdVgt, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 145,
     VI_COUNTER_NUM_PER_VGT, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block IA
    {"VI_IA", kHsaViCounterBlockIdIa, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodBySe, 23,
     VI_COUNTER_NUM_PER_IA, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block MC
    {"VI_MC", kHsaViCounterBlockIdMc, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 22,
     VI_COUNTER_NUM_PER_MC, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block SRBM
    {"VI_SRBM", kHsaViCounterBlockIdSrbm, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 19,
     VI_COUNTER_NUM_PER_SRBM, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block WD
    {"VI_WD", kHsaViCounterBlockIdWd, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 36,
     VI_COUNTER_NUM_PER_WD, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block CPG
    {"VI_CPG", kHsaViCounterBlockIdCpg, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 48,
     VI_COUNTER_NUM_PER_CPG, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block CPC
    {"VI_CPC", kHsaViCounterBlockIdCpc, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 24,
     VI_COUNTER_NUM_PER_CPC, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block IOMMUV2
    {"VI_IOMMUV2", kHsaViCounterBlockIdIommuV2, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 25,
     8, 0, 0, true, 0, 0, false, 0, 0},

    // Counter block KernelDriver
    {"VI_KD", kHsaViCounterBlockIdKernelDriver, VI_MAX_NUM_SHADER_ENGINES, 2, 1, CntlMethodNone, 0,
     0, 0, 0, true, 0, 0, false, 0, 0},

    // Name of the last line should be empty to indicate end of all counter groups
    {"", kHsaViCounterBlockIdBlocksLast, 0, 0, 0, CntlMethodNone, 0, 0, 0, 0, false, 0, 0, false, 0,
     0}};

extern const uint32_t Gfx8HwBlockCount = sizeof(Gfx8HwBlocks) / sizeof(GpuBlockInfo);

/*
 * The following tables contain register addresses of the SQ counter registers
 */

/*
 * SQ
 */
GpuCounterRegInfo ViSqCounterRegAddr[] = {
    {mmSQ_PERFCOUNTER0_SELECT__CI__VI, mmSQ_PERFCOUNTER_CTRL__CI__VI, mmSQ_PERFCOUNTER0_LO__CI__VI,
     mmSQ_PERFCOUNTER0_HI__CI__VI},
    {mmSQ_PERFCOUNTER1_SELECT__CI__VI, mmSQ_PERFCOUNTER_CTRL__CI__VI, mmSQ_PERFCOUNTER1_LO__CI__VI,
     mmSQ_PERFCOUNTER1_HI__CI__VI},
    {mmSQ_PERFCOUNTER2_SELECT__CI__VI, mmSQ_PERFCOUNTER_CTRL__CI__VI, mmSQ_PERFCOUNTER2_LO__CI__VI,
     mmSQ_PERFCOUNTER2_HI__CI__VI},
    {mmSQ_PERFCOUNTER3_SELECT__CI__VI, mmSQ_PERFCOUNTER_CTRL__CI__VI, mmSQ_PERFCOUNTER3_LO__CI__VI,
     mmSQ_PERFCOUNTER3_HI__CI__VI},
    {mmSQ_PERFCOUNTER4_SELECT__CI__VI, mmSQ_PERFCOUNTER_CTRL__CI__VI, mmSQ_PERFCOUNTER4_LO__CI__VI,
     mmSQ_PERFCOUNTER4_HI__CI__VI},
    {mmSQ_PERFCOUNTER5_SELECT__CI__VI, mmSQ_PERFCOUNTER_CTRL__CI__VI, mmSQ_PERFCOUNTER5_LO__CI__VI,
     mmSQ_PERFCOUNTER5_HI__CI__VI},
    {mmSQ_PERFCOUNTER6_SELECT__CI__VI, mmSQ_PERFCOUNTER_CTRL__CI__VI, mmSQ_PERFCOUNTER6_LO__CI__VI,
     mmSQ_PERFCOUNTER6_HI__CI__VI},
    {mmSQ_PERFCOUNTER7_SELECT__CI__VI, mmSQ_PERFCOUNTER_CTRL__CI__VI, mmSQ_PERFCOUNTER7_LO__CI__VI,
     mmSQ_PERFCOUNTER7_HI__CI__VI},
    {mmSQ_PERFCOUNTER8_SELECT__CI__VI, mmSQ_PERFCOUNTER_CTRL__CI__VI, mmSQ_PERFCOUNTER8_LO__CI__VI,
     mmSQ_PERFCOUNTER8_HI__CI__VI},
    {mmSQ_PERFCOUNTER9_SELECT__CI__VI, mmSQ_PERFCOUNTER_CTRL__CI__VI, mmSQ_PERFCOUNTER9_LO__CI__VI,
     mmSQ_PERFCOUNTER9_HI__CI__VI},
    {mmSQ_PERFCOUNTER10_SELECT__CI__VI, mmSQ_PERFCOUNTER_CTRL__CI__VI,
     mmSQ_PERFCOUNTER10_LO__CI__VI, mmSQ_PERFCOUNTER10_HI__CI__VI},
    {mmSQ_PERFCOUNTER11_SELECT__CI__VI, mmSQ_PERFCOUNTER_CTRL__CI__VI,
     mmSQ_PERFCOUNTER11_LO__CI__VI, mmSQ_PERFCOUNTER11_HI__CI__VI},
    {mmSQ_PERFCOUNTER12_SELECT__CI__VI, mmSQ_PERFCOUNTER_CTRL__CI__VI,
     mmSQ_PERFCOUNTER12_LO__CI__VI, mmSQ_PERFCOUNTER12_HI__CI__VI},
    {mmSQ_PERFCOUNTER13_SELECT__CI__VI, mmSQ_PERFCOUNTER_CTRL__CI__VI,
     mmSQ_PERFCOUNTER13_LO__CI__VI, mmSQ_PERFCOUNTER13_HI__CI__VI},
    {mmSQ_PERFCOUNTER14_SELECT__CI__VI, mmSQ_PERFCOUNTER_CTRL__CI__VI,
     mmSQ_PERFCOUNTER14_LO__CI__VI, mmSQ_PERFCOUNTER14_HI__CI__VI},
    {mmSQ_PERFCOUNTER15_SELECT__CI__VI, mmSQ_PERFCOUNTER_CTRL__CI__VI,
     mmSQ_PERFCOUNTER15_LO__CI__VI, mmSQ_PERFCOUNTER15_HI__CI__VI}};

/*
 * DRMDMA
 */
GpuCounterRegInfo ViDrmdmaCounterRegAddr[] = {
    {mmSDMA0_PERFMON_CNTL__VI, 0, mmSDMA0_PERFCOUNTER0_RESULT__VI, 0},
    {mmSDMA0_PERFMON_CNTL__VI, 0, mmSDMA0_PERFCOUNTER1_RESULT__VI, 0},
    {mmSDMA1_PERFMON_CNTL__VI, 0, mmSDMA1_PERFCOUNTER0_RESULT__VI, 0},
    {mmSDMA1_PERFMON_CNTL__VI, 0, mmSDMA1_PERFCOUNTER1_RESULT__VI, 0},
};

/*
 * IH
 */
GpuCounterRegInfo ViIhCounterRegAddr[] = {
    {mmIH_PERFMON_CNTL__VI, 0, mmIH_PERFCOUNTER0_RESULT__VI, 0},
    {mmIH_PERFMON_CNTL__VI, 0, mmIH_PERFCOUNTER1_RESULT__VI, 0}};

/*
 * CPF
 */
GpuCounterRegInfo ViCpfCounterRegAddr[] = {
    {mmCPF_PERFCOUNTER0_SELECT__CI__VI, 0, mmCPF_PERFCOUNTER0_LO__CI__VI,
     mmCPF_PERFCOUNTER0_HI__CI__VI},
    {mmCPF_PERFCOUNTER1_SELECT__CI__VI, 0, mmCPF_PERFCOUNTER1_LO__CI__VI,
     mmCPF_PERFCOUNTER1_HI__CI__VI}};

/*
 * DRM
 */
GpuCounterRegInfo ViDrmCounterRegAddr[] = {
    {mmDRM_PERFCOUNTER1_SELECT, 0, mmDRM_PERFCOUNTER1_LO, mmDRM_PERFCOUNTER1_HI},
    {mmDRM_PERFCOUNTER2_SELECT, 0, mmDRM_PERFCOUNTER2_LO, mmDRM_PERFCOUNTER2_HI}};

/*
 * GRBM
 */
GpuCounterRegInfo ViGrbmCounterRegAddr[] = {
    {mmGRBM_PERFCOUNTER0_SELECT__CI__VI, 0, mmGRBM_PERFCOUNTER0_LO__CI__VI,
     mmGRBM_PERFCOUNTER0_HI__CI__VI},
    {mmGRBM_PERFCOUNTER1_SELECT__CI__VI, 0, mmGRBM_PERFCOUNTER1_LO__CI__VI,
     mmGRBM_PERFCOUNTER1_HI__CI__VI}};

/*
 * GRBM_SE
 */
GpuCounterRegInfo ViGrbmSeCounterRegAddr[] = {
    {mmGRBM_SE0_PERFCOUNTER_SELECT__CI__VI, 0, mmGRBM_SE0_PERFCOUNTER_LO__CI__VI,
     mmGRBM_SE0_PERFCOUNTER_HI__CI__VI},
    {mmGRBM_SE1_PERFCOUNTER_SELECT__CI__VI, 0, mmGRBM_SE1_PERFCOUNTER_LO__CI__VI,
     mmGRBM_SE1_PERFCOUNTER_HI__CI__VI},
    {mmGRBM_SE2_PERFCOUNTER_SELECT__CI__VI, 0, mmGRBM_SE2_PERFCOUNTER_LO__CI__VI,
     mmGRBM_SE2_PERFCOUNTER_HI__CI__VI},
    {mmGRBM_SE3_PERFCOUNTER_SELECT__CI__VI, 0, mmGRBM_SE3_PERFCOUNTER_LO__CI__VI,
     mmGRBM_SE3_PERFCOUNTER_HI__CI__VI}};

/*
 * PA_SU
 */
GpuCounterRegInfo ViPaSuCounterRegAddr[] = {
    {mmPA_SU_PERFCOUNTER0_SELECT__CI__VI, 0, mmPA_SU_PERFCOUNTER0_LO__CI__VI,
     mmPA_SU_PERFCOUNTER0_HI__CI__VI},
    {mmPA_SU_PERFCOUNTER1_SELECT__CI__VI, 0, mmPA_SU_PERFCOUNTER1_LO__CI__VI,
     mmPA_SU_PERFCOUNTER1_HI__CI__VI},
    {mmPA_SU_PERFCOUNTER2_SELECT__CI__VI, 0, mmPA_SU_PERFCOUNTER2_LO__CI__VI,
     mmPA_SU_PERFCOUNTER2_HI__CI__VI},
    {mmPA_SU_PERFCOUNTER3_SELECT__CI__VI, 0, mmPA_SU_PERFCOUNTER3_LO__CI__VI,
     mmPA_SU_PERFCOUNTER3_HI__CI__VI}};

/*
 * PA_SC
 */
GpuCounterRegInfo ViPaScCounterRegAddr[] = {
    {mmPA_SC_PERFCOUNTER0_SELECT__CI__VI, 0, mmPA_SC_PERFCOUNTER0_LO__CI__VI,
     mmPA_SC_PERFCOUNTER0_HI__CI__VI},
    {mmPA_SC_PERFCOUNTER1_SELECT__CI__VI, 0, mmPA_SC_PERFCOUNTER1_LO__CI__VI,
     mmPA_SC_PERFCOUNTER1_HI__CI__VI},
    {mmPA_SC_PERFCOUNTER2_SELECT__CI__VI, 0, mmPA_SC_PERFCOUNTER2_LO__CI__VI,
     mmPA_SC_PERFCOUNTER2_HI__CI__VI},
    {mmPA_SC_PERFCOUNTER3_SELECT__CI__VI, 0, mmPA_SC_PERFCOUNTER3_LO__CI__VI,
     mmPA_SC_PERFCOUNTER3_HI__CI__VI}};

/*
 * SPI
 */
GpuCounterRegInfo ViSpiCounterRegAddr[] = {
    {mmSPI_PERFCOUNTER0_SELECT__CI__VI, 0, mmSPI_PERFCOUNTER0_LO__CI__VI,
     mmSPI_PERFCOUNTER0_HI__CI__VI},
    {mmSPI_PERFCOUNTER1_SELECT__CI__VI, 0, mmSPI_PERFCOUNTER1_LO__CI__VI,
     mmSPI_PERFCOUNTER1_HI__CI__VI},
    {mmSPI_PERFCOUNTER2_SELECT__CI__VI, 0, mmSPI_PERFCOUNTER2_LO__CI__VI,
     mmSPI_PERFCOUNTER2_HI__CI__VI},
    {mmSPI_PERFCOUNTER3_SELECT__CI__VI, 0, mmSPI_PERFCOUNTER3_LO__CI__VI,
     mmSPI_PERFCOUNTER3_HI__CI__VI},
    {mmSPI_PERFCOUNTER4_SELECT__CI__VI, 0, mmSPI_PERFCOUNTER4_LO__CI__VI,
     mmSPI_PERFCOUNTER4_HI__CI__VI},
    {mmSPI_PERFCOUNTER5_SELECT__CI__VI, 0, mmSPI_PERFCOUNTER5_LO__CI__VI,
     mmSPI_PERFCOUNTER5_HI__CI__VI}};

/*
 * TCA
 */
GpuCounterRegInfo ViTcaCounterRegAddr[] = {
    {mmTCA_PERFCOUNTER0_SELECT__CI__VI, 0, mmTCA_PERFCOUNTER0_LO__CI__VI,
     mmTCA_PERFCOUNTER0_HI__CI__VI},
    {mmTCA_PERFCOUNTER1_SELECT__CI__VI, 0, mmTCA_PERFCOUNTER1_LO__CI__VI,
     mmTCA_PERFCOUNTER1_HI__CI__VI},
    {mmTCA_PERFCOUNTER2_SELECT__CI__VI, 0, mmTCA_PERFCOUNTER2_LO__CI__VI,
     mmTCA_PERFCOUNTER2_HI__CI__VI},
    {mmTCA_PERFCOUNTER3_SELECT__CI__VI, 0, mmTCA_PERFCOUNTER3_LO__CI__VI,
     mmTCA_PERFCOUNTER3_HI__CI__VI}};

/*
 * TCC
 */
GpuCounterRegInfo ViTccCounterRegAddr[] = {
    {mmTCC_PERFCOUNTER0_SELECT__CI__VI, 0, mmTCC_PERFCOUNTER0_LO__CI__VI,
     mmTCC_PERFCOUNTER0_HI__CI__VI},
    {mmTCC_PERFCOUNTER1_SELECT__CI__VI, 0, mmTCC_PERFCOUNTER1_LO__CI__VI,
     mmTCC_PERFCOUNTER1_HI__CI__VI},
    {mmTCC_PERFCOUNTER2_SELECT__CI__VI, 0, mmTCC_PERFCOUNTER2_LO__CI__VI,
     mmTCC_PERFCOUNTER2_HI__CI__VI},
    {mmTCC_PERFCOUNTER3_SELECT__CI__VI, 0, mmTCC_PERFCOUNTER3_LO__CI__VI,
     mmTCC_PERFCOUNTER3_HI__CI__VI}};

/*
 * TCP
 */
GpuCounterRegInfo ViTcpCounterRegAddr[] = {
    {mmTCP_PERFCOUNTER0_SELECT__CI__VI, 0, mmTCP_PERFCOUNTER0_LO__CI__VI,
     mmTCP_PERFCOUNTER0_HI__CI__VI},
    {mmTCP_PERFCOUNTER1_SELECT__CI__VI, 0, mmTCP_PERFCOUNTER1_LO__CI__VI,
     mmTCP_PERFCOUNTER1_HI__CI__VI},
    {mmTCP_PERFCOUNTER2_SELECT__CI__VI, 0, mmTCP_PERFCOUNTER2_LO__CI__VI,
     mmTCP_PERFCOUNTER2_HI__CI__VI},
    {mmTCP_PERFCOUNTER3_SELECT__CI__VI, 0, mmTCP_PERFCOUNTER3_LO__CI__VI,
     mmTCP_PERFCOUNTER3_HI__CI__VI}};

/*
 * CB
 */
GpuCounterRegInfo ViCbCounterRegAddr[] = {
    {mmCB_PERFCOUNTER0_SELECT__CI__VI, 0, mmCB_PERFCOUNTER0_LO__CI__VI,
     mmCB_PERFCOUNTER0_HI__CI__VI},
    {mmCB_PERFCOUNTER1_SELECT__CI__VI, 0, mmCB_PERFCOUNTER1_LO__CI__VI,
     mmCB_PERFCOUNTER1_HI__CI__VI},
    {mmCB_PERFCOUNTER2_SELECT__CI__VI, 0, mmCB_PERFCOUNTER2_LO__CI__VI,
     mmCB_PERFCOUNTER2_HI__CI__VI},
    {mmCB_PERFCOUNTER3_SELECT__CI__VI, 0, mmCB_PERFCOUNTER3_LO__CI__VI,
     mmCB_PERFCOUNTER3_HI__CI__VI}};

/*
 * DB
 */
GpuCounterRegInfo ViDbCounterRegAddr[] = {
    {mmDB_PERFCOUNTER0_SELECT__CI__VI, 0, mmDB_PERFCOUNTER0_LO__CI__VI,
     mmDB_PERFCOUNTER0_HI__CI__VI},
    {mmDB_PERFCOUNTER1_SELECT__CI__VI, 0, mmDB_PERFCOUNTER1_LO__CI__VI,
     mmDB_PERFCOUNTER1_HI__CI__VI},
    {mmDB_PERFCOUNTER2_SELECT__CI__VI, 0, mmDB_PERFCOUNTER2_LO__CI__VI,
     mmDB_PERFCOUNTER2_HI__CI__VI},
    {mmDB_PERFCOUNTER3_SELECT__CI__VI, 0, mmDB_PERFCOUNTER3_LO__CI__VI,
     mmDB_PERFCOUNTER3_HI__CI__VI}};

/*
 * RLC
 */
GpuCounterRegInfo ViRlcCounterRegAddr[] = {
    {mmRLC_PERFCOUNTER0_SELECT__CI__VI, 0, mmRLC_PERFCOUNTER0_LO__CI__VI,
     mmRLC_PERFCOUNTER0_HI__CI__VI},
    {mmRLC_PERFCOUNTER1_SELECT__CI__VI, 0, mmRLC_PERFCOUNTER1_LO__CI__VI,
     mmRLC_PERFCOUNTER1_HI__CI__VI}};

/*
 * SC
 */
GpuCounterRegInfo ViScCounterRegAddr[] = {
    {mmPA_SC_PERFCOUNTER0_SELECT__CI__VI, 0, mmPA_SC_PERFCOUNTER0_LO__CI__VI,
     mmPA_SC_PERFCOUNTER0_HI__CI__VI},
    {mmPA_SC_PERFCOUNTER1_SELECT__CI__VI, 0, mmPA_SC_PERFCOUNTER1_LO__CI__VI,
     mmPA_SC_PERFCOUNTER1_HI__CI__VI},
    {mmPA_SC_PERFCOUNTER2_SELECT__CI__VI, 0, mmPA_SC_PERFCOUNTER2_LO__CI__VI,
     mmPA_SC_PERFCOUNTER2_HI__CI__VI},
    {mmPA_SC_PERFCOUNTER3_SELECT__CI__VI, 0, mmPA_SC_PERFCOUNTER3_LO__CI__VI,
     mmPA_SC_PERFCOUNTER3_HI__CI__VI},
    {mmPA_SC_PERFCOUNTER4_SELECT__CI__VI, 0, mmPA_SC_PERFCOUNTER4_LO__CI__VI,
     mmPA_SC_PERFCOUNTER4_HI__CI__VI},
    {mmPA_SC_PERFCOUNTER5_SELECT__CI__VI, 0, mmPA_SC_PERFCOUNTER5_LO__CI__VI,
     mmPA_SC_PERFCOUNTER5_HI__CI__VI},
    {mmPA_SC_PERFCOUNTER6_SELECT__CI__VI, 0, mmPA_SC_PERFCOUNTER6_LO__CI__VI,
     mmPA_SC_PERFCOUNTER6_HI__CI__VI},
    {mmPA_SC_PERFCOUNTER7_SELECT__CI__VI, 0, mmPA_SC_PERFCOUNTER7_LO__CI__VI,
     mmPA_SC_PERFCOUNTER7_HI__CI__VI}};

/*
 * SX
 */
GpuCounterRegInfo ViSxCounterRegAddr[] = {
    {mmSX_PERFCOUNTER0_SELECT__CI__VI, 0, mmSX_PERFCOUNTER0_LO__CI__VI,
     mmSX_PERFCOUNTER0_HI__CI__VI},
    {mmSX_PERFCOUNTER1_SELECT__CI__VI, 0, mmSX_PERFCOUNTER1_LO__CI__VI,
     mmSX_PERFCOUNTER1_HI__CI__VI},
    {mmSX_PERFCOUNTER2_SELECT__CI__VI, 0, mmSX_PERFCOUNTER2_LO__CI__VI,
     mmSX_PERFCOUNTER2_HI__CI__VI},
    {mmSX_PERFCOUNTER3_SELECT__CI__VI, 0, mmSX_PERFCOUNTER3_LO__CI__VI,
     mmSX_PERFCOUNTER3_HI__CI__VI}};

/*
 * TA
 */
GpuCounterRegInfo ViTaCounterRegAddr[] = {
    {mmTA_PERFCOUNTER0_SELECT__CI__VI, 0, mmTA_PERFCOUNTER0_LO__CI__VI,
     mmTA_PERFCOUNTER0_HI__CI__VI},
    {mmTA_PERFCOUNTER1_SELECT__CI__VI, 0, mmTA_PERFCOUNTER1_LO__CI__VI,
     mmTA_PERFCOUNTER1_HI__CI__VI}};

/*
 * TD
 */
GpuCounterRegInfo ViTdCounterRegAddr[] = {
    {mmTD_PERFCOUNTER0_SELECT__CI__VI, 0, mmTD_PERFCOUNTER0_LO__CI__VI,
     mmTD_PERFCOUNTER0_HI__CI__VI},
    {mmTD_PERFCOUNTER1_SELECT__CI__VI, 0, mmTD_PERFCOUNTER1_LO__CI__VI,
     mmTD_PERFCOUNTER1_HI__CI__VI}};

/*
 * GDS
 */
GpuCounterRegInfo ViGdsCounterRegAddr[] = {
    {mmGDS_PERFCOUNTER0_SELECT__CI__VI, 0, mmGDS_PERFCOUNTER0_LO__CI__VI,
     mmGDS_PERFCOUNTER0_HI__CI__VI},
    {mmGDS_PERFCOUNTER1_SELECT__CI__VI, 0, mmGDS_PERFCOUNTER1_LO__CI__VI,
     mmGDS_PERFCOUNTER1_HI__CI__VI},
    {mmGDS_PERFCOUNTER2_SELECT__CI__VI, 0, mmGDS_PERFCOUNTER2_LO__CI__VI,
     mmGDS_PERFCOUNTER2_HI__CI__VI},
    {mmGDS_PERFCOUNTER3_SELECT__CI__VI, 0, mmGDS_PERFCOUNTER3_LO__CI__VI,
     mmGDS_PERFCOUNTER3_HI__CI__VI}};

/*
 * VGT
 */
GpuCounterRegInfo ViVgtCounterRegAddr[] = {
    {mmVGT_PERFCOUNTER0_SELECT__CI__VI, 0, mmVGT_PERFCOUNTER0_LO__CI__VI,
     mmVGT_PERFCOUNTER0_HI__CI__VI},
    {mmVGT_PERFCOUNTER1_SELECT__CI__VI, 0, mmVGT_PERFCOUNTER1_LO__CI__VI,
     mmVGT_PERFCOUNTER1_HI__CI__VI},
    {mmVGT_PERFCOUNTER2_SELECT__CI__VI, 0, mmVGT_PERFCOUNTER2_LO__CI__VI,
     mmVGT_PERFCOUNTER2_HI__CI__VI},
    {mmVGT_PERFCOUNTER3_SELECT__CI__VI, 0, mmVGT_PERFCOUNTER3_LO__CI__VI,
     mmVGT_PERFCOUNTER3_HI__CI__VI}};

/*
 * IA
 */
GpuCounterRegInfo ViIaCounterRegAddr[] = {
    {mmIA_PERFCOUNTER0_SELECT__CI__VI, 0, mmIA_PERFCOUNTER0_LO__CI__VI,
     mmIA_PERFCOUNTER0_HI__CI__VI},
    {mmIA_PERFCOUNTER1_SELECT__CI__VI, 0, mmIA_PERFCOUNTER1_LO__CI__VI,
     mmIA_PERFCOUNTER1_HI__CI__VI},
    {mmIA_PERFCOUNTER2_SELECT__CI__VI, 0, mmIA_PERFCOUNTER2_LO__CI__VI,
     mmIA_PERFCOUNTER2_HI__CI__VI},
    {mmIA_PERFCOUNTER3_SELECT__CI__VI, 0, mmIA_PERFCOUNTER3_LO__CI__VI,
     mmIA_PERFCOUNTER3_HI__CI__VI}};

/*
 * MC
 */
GpuCounterRegInfo ViMcCounterRegAddr[] = {
    {mmMC_SEQ_PERF_SEQ_CTL__SI__VI, 0, mmMC_SEQ_PERF_SEQ_CNT_A_I0__VI,
     mmMC_SEQ_PERF_SEQ_CNT_A_I1__VI},
    {mmMC_SEQ_PERF_SEQ_CTL__SI__VI, 0, mmMC_SEQ_PERF_SEQ_CNT_B_I0__VI,
     mmMC_SEQ_PERF_SEQ_CNT_B_I1__VI},
    {mmMC_SEQ_PERF_SEQ_CTL__SI__VI, 0, mmMC_SEQ_PERF_SEQ_CNT_C_I0__VI,
     mmMC_SEQ_PERF_SEQ_CNT_C_I1__VI},
    {mmMC_SEQ_PERF_SEQ_CTL__SI__VI, 0, mmMC_SEQ_PERF_SEQ_CNT_D_I0__VI,
     mmMC_SEQ_PERF_SEQ_CNT_D_I1__VI}};

/*
 * SRBM
 */
GpuCounterRegInfo ViSrbmCounterRegAddr[] = {
    {mmSRBM_PERFCOUNTER0_SELECT__VI, 0, mmSRBM_PERFCOUNTER0_LO__VI, mmSRBM_PERFCOUNTER0_HI__VI},
    {mmSRBM_PERFCOUNTER1_SELECT__VI, 0, mmSRBM_PERFCOUNTER1_LO__VI, mmSRBM_PERFCOUNTER1_HI__VI}};

/*
 * WD
 */
GpuCounterRegInfo ViWdCounterRegAddr[] = {
    {mmWD_PERFCOUNTER0_SELECT__CI__VI, 0, mmWD_PERFCOUNTER0_LO__CI__VI,
     mmWD_PERFCOUNTER0_HI__CI__VI},
    {mmWD_PERFCOUNTER1_SELECT__CI__VI, 0, mmWD_PERFCOUNTER1_LO__CI__VI,
     mmWD_PERFCOUNTER1_HI__CI__VI},
    {mmWD_PERFCOUNTER2_SELECT__CI__VI, 0, mmWD_PERFCOUNTER2_LO__CI__VI,
     mmWD_PERFCOUNTER2_HI__CI__VI},
    {mmWD_PERFCOUNTER3_SELECT__CI__VI, 0, mmWD_PERFCOUNTER3_LO__CI__VI,
     mmWD_PERFCOUNTER3_HI__CI__VI}};

/*
 * CPG
 */
GpuCounterRegInfo ViCpgCounterRegAddr[] = {
    {mmCPG_PERFCOUNTER0_SELECT__CI__VI, 0, mmCPG_PERFCOUNTER0_LO__CI__VI,
     mmCPG_PERFCOUNTER0_HI__CI__VI},
    {mmCPG_PERFCOUNTER1_SELECT__CI__VI, 0, mmCPG_PERFCOUNTER1_LO__CI__VI,
     mmCPG_PERFCOUNTER1_HI__CI__VI}};

/*
 * CPC
 */
GpuCounterRegInfo ViCpcCounterRegAddr[] = {
    {mmCPC_PERFCOUNTER0_SELECT__CI__VI, 0, mmCPC_PERFCOUNTER0_LO__CI__VI,
     mmCPC_PERFCOUNTER0_HI__CI__VI},
    {mmCPC_PERFCOUNTER1_SELECT__CI__VI, 0, mmCPC_PERFCOUNTER1_LO__CI__VI,
     mmCPC_PERFCOUNTER1_HI__CI__VI}};

GpuPrivCounterBlockId ViBlockIdSq = {{0xb5c396b6, 0x47e4d310, 0xc35cfc86, 0x08f53a04}};
GpuPrivCounterBlockId ViBlockIdMc = {{0x13900b57, 0x4d984956, 0x5268d081, 0x9cf53719}};
GpuPrivCounterBlockId ViBlockIdIommuV2 = {{0x80969879, 0x4be6b0f6, 0x636af697, 0x1d10f500}};
GpuPrivCounterBlockId ViBlockIdKernelDriver = {{0xea9b5ae1, 0x44b36c3f, 0xf0da5489, 0x0aa96575}};

}  // pm4_profile
