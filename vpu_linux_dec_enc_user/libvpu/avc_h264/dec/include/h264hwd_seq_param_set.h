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
--  Abstract : Decode seq parameter set information from the stream
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264hwd_seq_param_set.h,v $
--  $Date: 2010/04/22 06:54:56 $
--  $Revision: 1.3 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of contents

    1. Include headers
    2. Module defines
    3. Data types
    4. Function prototypes

------------------------------------------------------------------------------*/

#ifndef H264HWD_SEQ_PARAM_SET_H
#define H264HWD_SEQ_PARAM_SET_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "basetype.h"

#include "h264hwd_cfg.h"
#include "h264hwd_stream.h"
#include "h264hwd_vui.h"

/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/

/* structure to store sequence parameter set information decoded from the
 * stream */
typedef struct
{
    u32 profileIdc;
    u32 levelIdc;
    u8 constrained_set0_flag;
    u8 constrained_set1_flag;
    u8 constrained_set2_flag;
    u8 constrained_set3_flag;
    u32 seqParameterSetId;
    u32 maxFrameNum;
    u32 picOrderCntType;
    u32 maxPicOrderCntLsb;
    u32 deltaPicOrderAlwaysZeroFlag;
    i32 offsetForNonRefPic;
    i32 offsetForTopToBottomField;
    u32 numRefFramesInPicOrderCntCycle;
    i32 offsetForRefFrame[32];
    u32 numRefFrames;
    u32 gapsInFrameNumValueAllowedFlag;
    u32 picWidthInMbs;
    u32 picHeightInMbs;
    u32 frameCroppingFlag;
    u32 frameCropLeftOffset;
    u32 frameCropRightOffset;
    u32 frameCropTopOffset;
    u32 frameCropBottomOffset;
    u32 vuiParametersPresentFlag;
    vuiParameters_t *vuiParameters;
    u32 maxDpbSize;
    u32 frameMbsOnlyFlag;
    u32 mbAdaptiveFrameFieldFlag;
    u32 direct8x8InferenceFlag;
    u32 chromaFormatIdc;
    u32 monoChrome;
    u32 scalingMatrixPresentFlag;
    u32 scalingListPresent[8];
    u32 useDefaultScaling[8];
    u8 scalingList[8][64];

    /* mvc extension */
    struct {
        u32 numViews;
        u32 viewId[MAX_NUM_VIEWS];
    } mvc;
} seqParamSet_t;

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

u32 h264bsdDecodeSeqParamSet(strmData_t *pStrmData,
    seqParamSet_t *pSeqParamSet, u32 mvcFlag);

u32 h264bsdCompareSeqParamSets(seqParamSet_t *pSps1, seqParamSet_t *pSps2);

#endif /* #ifdef H264HWD_SEQ_PARAM_SET_H */
