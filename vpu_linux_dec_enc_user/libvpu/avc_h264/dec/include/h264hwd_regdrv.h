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
--  Description : Hardware interface read/write
--
------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264hwd_regdrv.h,v $
--  $Revision: 1.1 $
--  $Date: 2008/03/13 12:48:06 $
--
------------------------------------------------------------------------------*/
#ifndef __H264HWD_REGDRV_H__
#define __H264HWD_REGDRV_H__

#include "basetype.h"
#include "regdrv.h"

#define DEC_X170_ALIGN_MASK         0x07

#define DEC_X170_MODE_H264          0x00

#define DEC_X170_IRQ_DEC_RDY        0x01
#define DEC_X170_IRQ_BUS_ERROR      0x02
#define DEC_X170_IRQ_BUFFER_EMPTY   0x04
#define DEC_X170_IRQ_ASO            0x08
#define DEC_X170_IRQ_STREAM_ERROR   0x10
#define DEC_X170_IRQ_TIMEOUT        0x40

#endif  /* __H264HWD_REGDRV_H__ */
