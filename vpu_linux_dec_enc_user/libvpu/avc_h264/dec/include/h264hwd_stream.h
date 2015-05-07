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
--  Abstract : Stream buffer handling
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264hwd_stream.h,v $
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

#ifndef H264HWD_STREAM_H
#define H264HWD_STREAM_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "basetype.h"

/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/

typedef struct
{
    const u8 *pStrmBuffStart;   /* pointer to start of stream buffer */
    const u8 *pStrmCurrPos;  /* current read address in stream buffer */
    u32 bitPosInWord;        /* bit position in stream buffer byte */
    u32 strmBuffSize;        /* size of stream buffer (bytes) */
    u32 strmBuffReadBits;    /* number of bits read from stream buffer */
    u32 removeEmul3Byte;     /* signal the pre-removal of emulation prevention 3 bytes */
    u32 emulByteCount;       /* counter incremented for each removed byte */
} strmData_t;

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

u32 h264bsdGetBits(strmData_t * pStrmData, u32 numBits);

u32 h264bsdShowBits(strmData_t * pStrmData, u32 numBits);

u32 h264bsdFlushBits(strmData_t * pStrmData, u32 numBits);

u32 h264bsdIsByteAligned(strmData_t *);

#endif /* #ifdef H264HWD_STREAM_H */
