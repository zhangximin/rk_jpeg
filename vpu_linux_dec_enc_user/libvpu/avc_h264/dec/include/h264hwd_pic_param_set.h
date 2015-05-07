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
--  Abstract : Decode picture parameter set information from the stream
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264hwd_pic_param_set.h,v $
--  $Date: 2008/03/13 12:48:06 $
--  $Revision: 1.1 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of contents

    1. Include headers
    2. Module defines
    3. Data types
    4. Function prototypes

------------------------------------------------------------------------------*/

#ifndef H264HWD_PIC_PARAM_SET_H
#define H264HWD_PIC_PARAM_SET_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "basetype.h"

#include "h264hwd_stream.h"

/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/

/* data structure to store PPS information decoded from the stream */
typedef struct
{
    u32 picParameterSetId;
    u32 seqParameterSetId;
    u32 picOrderPresentFlag;
    u32 numSliceGroups;
    u32 sliceGroupMapType;
    u32 runLength[256];
    u32 topLeft[256];
    u32 bottomRight[256];
    u32 sliceGroupChangeDirectionFlag;
    u32 sliceGroupChangeRate;
    u32 picSizeInMapUnits;
    u32 sliceGroupId[256];
    u32 numRefIdxL0Active;
    u32 numRefIdxL1Active;
    u32 picInitQp;
    i32 chromaQpIndexOffset;
    i32 chromaQpIndexOffset2;
    u32 deblockingFilterControlPresentFlag;
    u32 constrainedIntraPredFlag;
    u32 redundantPicCntPresentFlag;
    u32 entropyCodingModeFlag;
    u32 weightedPredFlag;
    u32 weightedBiPredIdc;
    u32 transform8x8Flag;
    u32 scalingMatrixPresentFlag;
    u32 scalingListPresent[8];
    u32 useDefaultScaling[8];
    u8 scalingList[8][64];
} picParamSet_t;

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

u32 h264bsdDecodePicParamSet(strmData_t *pStrmData,
    picParamSet_t *pPicParamSet);

#endif /* #ifdef H264HWD_PIC_PARAM_SET_H */
