/*
***************************************************************************************************
*
*  Trade secret of Advanced Micro Devices, Inc.
*  Copyright (c) 2010 Advanced Micro Devices, Inc. (unpublished)
*
*  All rights reserved.  This notice is intended as a precaution against inadvertent publication and
*  does not imply publication or any waiver of confidentiality.  The year included in the foregoing
*  notice is the year of creation of the work.
*
***************************************************************************************************
*/

#ifndef _GFX9_PM4DEFS_H_
#define _GFX9_PM4DEFS_H_

/******************************************************************************
*
*  gfx9_pm4defs.h
*
*  GFX9 PM4 definitions, typedefs, and enumerations.
*
******************************************************************************/

#define COPY_DATA_SEL_REG 0                   ///< Mem-mapped register
#define COPY_DATA_SEL_SRC_SYS_PERF_COUNTER 4  ///< Privileged memory performance counter
#define COPY_DATA_SEL_COUNT_1DW 0             ///< Copy 1 word (32 bits)
#define COPY_DATA_SEL_COUNT_2DW 1             ///< Copy 2 words (64 bits)

#endif  // _GFX9_PM4DEFS_H_
