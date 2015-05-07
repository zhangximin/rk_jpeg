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
--  Abstract : Header file for stream decoding utilities
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: bqueue.h,v $
--  $Date: 2010/02/25 12:18:56 $
--  $Revision: 1.1 $
--
------------------------------------------------------------------------------*/

#ifndef BQUEUE_H_DEFINED
#define BQUEUE_H_DEFINED

#include "vpu_type.h"
#include "dwl.h"

typedef struct
{
    RK_U32 *picI;
    RK_U32 ctr;
    RK_U32 queueSize;
    RK_U32 prevAnchorSlot;
} bufferQueue_t;

#define BQUEUE_UNUSED (RK_U32)(0xffffffff)

RK_U32  BqueueInit( bufferQueue_t *bq, RK_U32 numBuffers );
void BqueueRelease( bufferQueue_t *bq );
RK_U32  BqueueNext( bufferQueue_t *bq, RK_U32 ref0, RK_U32 ref1, RK_U32 ref2, RK_U32 bPic );
void BqueueDiscard( bufferQueue_t *bq, RK_U32 buffer );

#endif /* BQUEUE_H_DEFINED */
