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
--  Abstract : Decode NAL unit header
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264hwd_nal_unit.h,v $
--  $Date: 2010/04/22 06:54:56 $
--  $Revision: 1.2 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of contents
   
    1. Include headers
    2. Module defines
    3. Data types
    4. Function prototypes

------------------------------------------------------------------------------*/

#ifndef H264HWD_NAL_UNIT_H
#define H264HWD_NAL_UNIT_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "basetype.h"
#include "h264hwd_stream.h"

/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/

/* macro to determine if NAL unit pointed by pNalUnit contains an IDR slice */
#define IS_IDR_NAL_UNIT(pNalUnit) \
    ((pNalUnit)->nalUnitType == NAL_CODED_SLICE_IDR || \
     ((pNalUnit)->nalUnitType == NAL_CODED_SLICE_EXT && \
      (pNalUnit)->nonIdrFlag == 0))

/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/

typedef enum
{
    NAL_UNSPECIFIED = 0,
    NAL_CODED_SLICE = 1,
    NAL_CODED_SLICE_DP_A = 2,
    NAL_CODED_SLICE_DP_B = 3,
    NAL_CODED_SLICE_DP_C = 4,
    NAL_CODED_SLICE_IDR = 5,
    NAL_SEI = 6,
    NAL_SEQ_PARAM_SET = 7,
    NAL_PIC_PARAM_SET = 8,
    NAL_ACCESS_UNIT_DELIMITER = 9,
    NAL_END_OF_SEQUENCE = 10,
    NAL_END_OF_STREAM = 11,
    NAL_FILLER_DATA = 12,
    NAL_SPS_EXT = 13,
    NAL_PREFIX = 14,
    NAL_SUBSET_SEQ_PARAM_SET = 15,
    NAL_CODED_SLICE_AUX = 19,
    NAL_CODED_SLICE_EXT = 20,
    NAL_MAX_TYPE_VALUE = 31
} nalUnitType_e;

typedef struct
{
    nalUnitType_e nalUnitType;
    u32 nalRefIdc;
    u32 nonIdrFlag;
    u32 priorityId;
    u32 viewId;
    u32 temporalId;
    u32 anchorPicFlag;
    u32 interViewFlag;
} nalUnit_t;

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

u32 h264bsdDecodeNalUnit(strmData_t * pStrmData, nalUnit_t * pNalUnit);

#endif /* #ifdef H264HWD_NAL_UNIT_H */
