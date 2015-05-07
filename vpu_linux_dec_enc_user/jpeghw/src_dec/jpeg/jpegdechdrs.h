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
--  Description : Jpeg decoder header decoding header file
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: jpegdechdrs.h,v $
--  $Revision: 1.1 $
--  $Date: 2007/03/30 05:44:50 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of contents 
   
    1. Include headers
    2. Module defines
    3. Data types
    4. Function prototypes

------------------------------------------------------------------------------*/

#ifndef JPEGDECHDRS_H
#define JPEGDECHDRS_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "vpu_type.h"
#include "jpegdeccontainer.h"

/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

JpegDecRet JpegDecDecodeFrameHdr(JpegDecContainer * pDecData);
JpegDecRet JpegDecDecodeQuantTables(JpegDecContainer * pDecData);
JpegDecRet JpegDecDecodeHuffmanTables(JpegDecContainer * pDecData);
JpegDecRet JpegDecMode(JpegDecContainer * pDecData);
JpegDecRet JpegDecMode(JpegDecContainer *);

#endif /* #ifdef MODULE_H */
