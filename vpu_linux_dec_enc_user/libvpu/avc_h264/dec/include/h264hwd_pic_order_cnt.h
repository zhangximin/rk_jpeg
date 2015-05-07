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
--  Abstract : Compute Picture Order Count (POC) for a picture
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264hwd_pic_order_cnt.h,v $
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

#ifndef H264HWD_PIC_ORDER_CNT_H
#define H264HWD_PIC_ORDER_CNT_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "basetype.h"
#include "h264hwd_seq_param_set.h"
#include "h264hwd_slice_header.h"
#include "h264hwd_nal_unit.h"

/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/

/* structure to store information computed for previous picture, needed for
 * POC computation of a picture. Two first fields for POC type 0, last two
 * for types 1 and 2 */
typedef struct
{
    u32 prevPicOrderCntLsb;
    i32 prevPicOrderCntMsb;
    u32 prevFrameNum;
    u32 prevFrameNumOffset;
    u32 containsMmco5;
    i32 picOrderCnt[2];
    i32 minusPoc;
} pocStorage_t;

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

void h264bsdDecodePicOrderCnt(pocStorage_t *poc, const seqParamSet_t *sps,
     const sliceHeader_t *sliceHeader, const nalUnit_t *pNalUnit);

#endif /* #ifdef H264HWD_PIC_ORDER_CNT_H */
