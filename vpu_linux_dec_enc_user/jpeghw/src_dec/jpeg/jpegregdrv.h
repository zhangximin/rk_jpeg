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
--  Description :  X170 JPEG Decoder HW register access functions interface
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: jpegregdrv.h,v $
--  $Date: 2007/06/05 09:37:57 $
--  $Revision: 1.2 $
--
------------------------------------------------------------------------------*/

#ifndef _JPG_HWREGDRV_H_
#define _JPG_HWREGDRV_H_

#include "vpu_type.h"
#include "deccfg.h"
#include "regdrv.h"

#define JPEG_X170_MODE_JPEG       3

#define JPEGDEC_X170_IRQ_DEC_RDY        0x01
#define JPEGDEC_X170_IRQ_BUS_ERROR      0x02
#define JPEGDEC_X170_IRQ_BUFFER_EMPTY   0x04
#define JPEGDEC_X170_IRQ_ASO            0x08
#define JPEGDEC_X170_IRQ_STREAM_ERROR   0x10
#define JPEGDEC_X170_IRQ_SLICE_READY    0x00
#define JPEGDEC_X170_IRQ_TIMEOUT        0x40

#endif /* #define _JPG_HWREGDRV_H_ */

