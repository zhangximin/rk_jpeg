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
--  $RCSfile: workaround.h,v $
--  $Date: 2009/10/20 08:51:45 $
--  $Revision: 1.2 $
--
------------------------------------------------------------------------------*/

#ifndef WORKAROUND_H_DEFINED
#define WORKAROUND_H_DEFINED

#include "vpu_type.h"

typedef struct workaround_s 
{
    RK_U32 stuffing;
    RK_U32 startCode;
} workaround_t;

#ifndef HANTRO_TRUE
    #define HANTRO_TRUE     (1)
#endif /* HANTRO_TRUE */

#ifndef HANTRO_FALSE
    #define HANTRO_FALSE    (0)
#endif /* HANTRO_FALSE*/

void InitWorkarounds(RK_U32 decMode, workaround_t *pWorkarounds );
void PrepareStuffingWorkaround( RK_U8 *pDecOut, RK_U32 vopWidth, RK_U32 vopHeight );
RK_U32  ProcessStuffingWorkaround( RK_U8 * pDecOut, RK_U8 * pRefPic, RK_U32 vopWidth, 
                                RK_U32 vopHeight );
void PrepareStartCodeWorkaround( RK_U8 *pDecOut, RK_U32 vopWidth, RK_U32 vopHeight,
    RK_U32 topField );
RK_U32  ProcessStartCodeWorkaround( RK_U8 *pDecOut, RK_U32 vopWidth, RK_U32 vopHeight,
    RK_U32 topField );

#endif /* WORKAROUND_H_DEFINED */
