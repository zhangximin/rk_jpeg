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
--  Abstract  :   Encoder instance
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: H264Instance.h,v $
--  $Date: 2007/08/13 09:56:41 $
--  $Revision: 1.2 $
--
------------------------------------------------------------------------------*/

#ifndef __H264_INSTANCE_H__
#define __H264_INSTANCE_H__

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "enccommon.h"
#include "encpreprocess.h"
#include "encasiccontroller.h"

#include "H264SequenceParameterSet.h"
#include "H264PictureParameterSet.h"
#include "H264Slice.h"
#include "H264RateControl.h"
#include "H264Mad.h"

#ifdef VIDEOSTAB_ENABLED
#include "vidstabcommon.h"
#endif

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/
enum H264EncStatus
{
    H264ENCSTAT_INIT = 0xA1,
    H264ENCSTAT_START_STREAM,
    H264ENCSTAT_START_FRAME,
    H264ENCSTAT_ERROR
};

typedef struct
{
    u32 encStatus;
    u32 mbPerFrame;
    u32 mbPerRow;
    u32 mbPerCol;
    u32 frameCnt;
    u32 fillerNalSize;
    u32 testId;
    stream_s stream;
    preProcess_s preProcess;
    sps_s seqParameterSet;
    pps_s picParameterSet;
    slice_s slice;
    h264RateControl_s rateControl;
    madTable_s mad;
    asicData_s asic;
    const void *inst;
#ifdef VIDEOSTAB_ENABLED
    HWStabData vsHwData;
    SwStbData vsSwData;
#endif
} h264Instance_s;

#endif
