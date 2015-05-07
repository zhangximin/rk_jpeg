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
--  Abstract : Definition of decContainer_t data structure
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264hwd_container.h,v $
--  $Date: 2010/05/14 10:45:43 $
--  $Revision: 1.12 $
--
------------------------------------------------------------------------------*/

#ifndef H264HWD_CONTAINER_H
#define H264HWD_CONTAINER_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "h264hwd_storage.h"
#include "h264hwd_util.h"
#include "refbuffer.h"
#include "deccfg.h"
#include "decppif.h"

namespace android {

/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/
typedef enum H264DEC_STATUS {
    H264DEC_UNINITIALIZED,
    H264DEC_INITIALIZED,
    H264DEC_BUFFER_EMPTY,
    H264DEC_NEW_HEADERS,
    H264DEC_SYNC_STREAM,
} H264DEC_STATUS;

/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/

/* stream interface */
typedef struct decStream {
    const u8 *pHwStreamStart;
    u32 hwStreamStartBus;
    u32 hwBitPos;
    u32 hwLength;
    u32 streamPosUpdated;
} decStream_t;

/* asic interface */
typedef struct DecAsicBuffers
{
    VPUMemLinear_t cabacInit;
    u32 outPhyAddr;
    u32 outVirAddr;
    u32 refPicList[16];
    u32 maxRefFrm;
    u32 filterDisable;
    i32 chromaQpIndexOffset;
    i32 chromaQpIndexOffset2;
    //u32 disableOutWriting;
    u32 enableDmvAndPoc;
} DecAsicBuffers_t;

typedef struct decContainer
{
    const void *checksum;
    H264DEC_STATUS decStat;
    int socket;                 /* VPU client handle or socket */

    const u8 *pHwStreamStart;
    u32 hwStreamStartBus;
    u32 hwBitPos;
    u32 hwLength;
    u32 streamPosUpdated;

    u32 gapsCheckedForThis;
    u32 packetDecoded;

    storage_t storage;       /* h264bsd storage */
    DecAsicBuffers_t asicBuff[1];
    u32 refBufSupport;
    u32 h264ProfileSupport;
    u32 maxDecPicWidth;
    refBuffer_t refBufferCtrl;

    u32 h264Regs[DEC_X170_REGISTERS];
	
	i32 h264hwStatus[2];	//for pic conceal
	i32 h264hwdecLen[2];
	i32 h264hwFrameLen[2];

	u32 dommco5;
	u32 ts_en;
    u32 disable_mvc;        // default 0
} decContainer_t;

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

}

#endif /* #ifdef H264HWD_CONTAINER_H */
