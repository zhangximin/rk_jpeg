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
--  Abstract : Stream decoding utilities
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: bqueue.c,v $
--  $Date: 2010/04/15 12:15:24 $
--  $Revision: 1.2 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of context

     1. Include headers
     2. External identifiers
     3. Module defines
     4. Module identifiers
     5. Fuctions

------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "bqueue.h"
#ifndef HANTRO_OK
    #define HANTRO_OK   (0)
#endif /* HANTRO_TRUE */

#ifndef HANTRO_NOK
    #define HANTRO_NOK  (1)
#endif /* HANTRO_FALSE*/

/*------------------------------------------------------------------------------
    2. External identifiers
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Module indentifiers
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    BqueueInit
        Initialize buffer queue
------------------------------------------------------------------------------*/
RK_U32 BqueueInit( bufferQueue_t *bq, RK_U32 numBuffers )
{
    RK_U32 i;
    bq->picI = (RK_U32*)malloc( sizeof(RK_U32)*numBuffers);
    if( bq->picI == NULL )
    {
        return HANTRO_NOK;
    }
    for( i = 0 ; i < numBuffers ; ++i )
    {
        bq->picI[i] = 0;
    }
    bq->queueSize = numBuffers;
    bq->ctr = 1;

    return HANTRO_OK;
}

/*------------------------------------------------------------------------------
    BqueueRelease
------------------------------------------------------------------------------*/
void BqueueRelease( bufferQueue_t *bq )
{
    if(bq->picI)
    {
        free(bq->picI);
        bq->picI = NULL;
    }
    bq->prevAnchorSlot  = 0;
    bq->queueSize       = 0;
}

/*------------------------------------------------------------------------------
    BqueueNext
        Return "oldest" available buffer.
------------------------------------------------------------------------------*/
RK_U32 BqueueNext( bufferQueue_t *bq, RK_U32 ref0, RK_U32 ref1, RK_U32 ref2, RK_U32 bPic )
{
    RK_U32 minPicI = 1<<30;
    RK_U32 nextOut = (RK_U32)0xFFFFFFFFU;
    RK_U32 i;
    /* Find available buffer with smallest index number  */
    i = 0;

    while( i < bq->queueSize )
    {
        if(i == ref0 || i == ref1 || i == ref2) /* Skip reserved anchor pictures */
        {
            i++;
            continue;
        }
        if( bq->picI[i] < minPicI )
        {
            minPicI = bq->picI[i];
            nextOut = i;
        }
        i++;
    }

    if( nextOut == (RK_U32)0xFFFFFFFFU)
    {
        return 0; /* No buffers available, shouldn't happen */
    }

    /* Update queue state */
    if( bPic )
    {
        bq->picI[nextOut] = bq->ctr-1;
        bq->picI[bq->prevAnchorSlot]++;
    }
    else
    {
        bq->picI[nextOut] = bq->ctr;
    }
    bq->ctr++;
    if( !bPic )
    {
        bq->prevAnchorSlot = nextOut;
    }

    return nextOut;
}

/*------------------------------------------------------------------------------
    BqueueDiscard
        "Discard" output buffer, e.g. if error concealment used and buffer
        at issue is never going out.
------------------------------------------------------------------------------*/
void BqueueDiscard( bufferQueue_t *bq, RK_U32 buffer )
{
    bq->picI[buffer] = 0;
}
