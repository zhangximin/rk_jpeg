/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2006 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Abstract : Decode slice header information from the stream
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264hwd_slice_header.h,v $
--  $Date: 2010/04/22 06:54:56 $
--  $Revision: 1.4 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of contents

    1. Include headers
    2. Module defines
    3. Data types
    4. Function prototypes

------------------------------------------------------------------------------*/

#ifndef H264HWD_SLICE_HEADER_H
#define H264HWD_SLICE_HEADER_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "basetype.h"

#include "h264hwd_stream.h"
#include "h264hwd_cfg.h"
#include "h264hwd_seq_param_set.h"
#include "h264hwd_pic_param_set.h"
#include "h264hwd_nal_unit.h"

/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/

enum {
    P_SLICE = 0,
    B_SLICE = 1,
    I_SLICE = 2
};

enum {NO_LONG_TERM_FRAME_INDICES = 0xFFFF};

/* macro to determine if slice is an inter slice, sliceTypes 0 and 5 */
#define IS_P_SLICE(sliceType) (((sliceType) == P_SLICE) || \
    ((sliceType) == P_SLICE + 5))

/* macro to determine if slice is an intra slice, sliceTypes 2 and 7 */
#define IS_I_SLICE(sliceType) (((sliceType) == I_SLICE) || \
    ((sliceType) == I_SLICE + 5))

#define IS_B_SLICE(sliceType) (((sliceType) == B_SLICE) || \
    ((sliceType) == B_SLICE + 5))

/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/

/* structure to store data of one reference picture list reordering operation */
typedef struct
{
    u32 reorderingOfPicNumsIdc;
    u32 absDiffPicNum;
    u32 longTermPicNum;
    u32 absDiffViewIdx;
} refPicListReorderingOperation_t;

/* structure to store reference picture list reordering operations */
typedef struct
{
    u32 refPicListReorderingFlagL0;
    refPicListReorderingOperation_t command[MAX_NUM_REF_PICS+1];
} refPicListReordering_t;

/* structure to store data of one DPB memory management control operation */
typedef struct
{
    u32 memoryManagementControlOperation;
    u32 differenceOfPicNums;
    u32 longTermPicNum;
    u32 longTermFrameIdx;
    u32 maxLongTermFrameIdx;
} memoryManagementOperation_t;

/* worst case scenario: all MAX_NUM_REF_PICS pictures in the buffer are
 * short term pictures, each one of them is first marked as long term
 * reference picture which is then marked as unused for reference.
 * Additionally, max long-term frame index is set and current picture is
 * marked as long term reference picture. Last position reserved for
 * end memory_management_control_operation command */
#define MAX_NUM_MMC_OPERATIONS (2*MAX_NUM_REF_PICS+2+1)

/* structure to store decoded reference picture marking data */
typedef struct
{
    u32 strmLen;
    u32 noOutputOfPriorPicsFlag;
    u32 longTermReferenceFlag;
    u32 adaptiveRefPicMarkingModeFlag;
    memoryManagementOperation_t operation[MAX_NUM_MMC_OPERATIONS];
} decRefPicMarking_t;

/* structure to store slice header data decoded from the stream */
typedef struct
{
    u32 firstMbInSlice;
    u32 sliceType;
    u32 picParameterSetId;
    u32 frameNum;
    u32 idrPicId;
    u32 pocLength;
    u32 pocLengthHw;
    u32 picOrderCntLsb;
    i32 deltaPicOrderCntBottom;
    i32 deltaPicOrderCnt[2];
    u32 redundantPicCnt;
    u32 numRefIdxActiveOverrideFlag;
    u32 numRefIdxL0Active;
    u32 numRefIdxL1Active;
    i32 sliceQpDelta;
    u32 disableDeblockingFilterIdc;
    i32 sliceAlphaC0Offset;
    i32 sliceBetaOffset;
    u32 sliceGroupChangeCycle;
    refPicListReordering_t refPicListReordering;
    refPicListReordering_t refPicListReorderingL1;
    decRefPicMarking_t decRefPicMarking;
    u32 cabacInitIdc;
    u32 fieldPicFlag;
    u32 bottomFieldFlag;
    u32 directSpatialMvPredFlag;
} sliceHeader_t;

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

u32 h264bsdDecodeSliceHeader(strmData_t *pStrmData,
  sliceHeader_t *pSliceHeader,
  seqParamSet_t *pSeqParamSet,
  picParamSet_t *pPicParamSet,
  nalUnit_t *pNalUnit,
  u32 numRefFrames);

u32 h264bsdCheckPpsId(strmData_t *pStrmData, u32 *ppsId);

u32 h264bsdCheckFrameNum(
  strmData_t *pStrmData,
  u32 maxFrameNum,
  u32 *frameNum);

u32 h264bsdCheckIdrPicId(
  strmData_t *pStrmData,
  u32 maxFrameNum,
  nalUnitType_e nalUnitType,
  u32 fieldPicFlag,
  u32 *idrPicId);

u32 h264bsdCheckPicOrderCntLsb(
  strmData_t *pStrmData,
  seqParamSet_t *pSeqParamSet,
  nalUnitType_e nalUnitType,
  u32 *picOrderCntLsb);

u32 h264bsdCheckDeltaPicOrderCntBottom(
  strmData_t *pStrmData,
  seqParamSet_t *pSeqParamSet,
  nalUnitType_e nalUnitType,
  i32 *deltaPicOrderCntBottom);

u32 h264bsdCheckDeltaPicOrderCnt(
  strmData_t *pStrmData,
  seqParamSet_t *pSeqParamSet,
  nalUnitType_e nalUnitType,
  u32 picOrderPresentFlag,
  i32 *deltaPicOrderCnt);

u32 h264bsdCheckRedundantPicCnt(
  const strmData_t *pStrmData,
  const seqParamSet_t *pSeqParamSet,
  const picParamSet_t *pPicParamSet,
  nalUnitType_e nalUnitType,
  u32 *redundantPicCnt);

u32 h264bsdCheckPriorPicsFlag(u32 * noOutputOfPriorPicsFlag,
                              const strmData_t * pStrmData,
                              const seqParamSet_t * pSeqParamSet,
                              const picParamSet_t * pPicParamSet,
                              nalUnitType_e nalUnitType);

u32 h264bsdFieldPicFlag(strmData_t * pStrmData,
                         u32 maxFrameNum,
                         nalUnitType_e nalUnitType,
                         u32 fieldPicFlagPresent,
                         u32 *fieldPicFlag);

u32 h264bsdBottomFieldFlag(strmData_t * pStrmData,
                         u32 maxFrameNum,
                         nalUnitType_e nalUnitType,
                         u32 fieldPicFlag,
                         u32 *bottomFieldFlag);

u32 h264bsdIsOppositeFieldPic(sliceHeader_t * pSliceCurr,
                              sliceHeader_t * pSlicePrev,
                              u32 *secondField, u32 prevRefFrameNum,
                              u32 newPicture);

u32 h264bsdCheckFieldPicFlag(strmData_t * pStrmData,
                         u32 maxFrameNum,
                         nalUnitType_e nalUnitType,
                         u32 fieldPicFlagPresent,
                         u32 *fieldPicFlag);

u32 h264bsdCheckBottomFieldFlag(strmData_t * pStrmData,
                         u32 maxFrameNum,
                         nalUnitType_e nalUnitType, u32 fieldPicFlag,
                         u32 * bottomFieldFlag);

u32 h264bsdCheckFirstMbInSlice(strmData_t * pStrmData,
                         nalUnitType_e nalUnitType,
                         u32 * firstMbInSlice);

#endif /* #ifdef H264HWD_SLICE_HEADER_H */
