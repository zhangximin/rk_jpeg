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
--  Abstract : Top level control of the decoder
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264hwd_decoder.h,v $
--  $Date: 2010/05/14 10:45:43 $
--  $Revision: 1.5 $
--
------------------------------------------------------------------------------*/
#ifndef H264HWD_DECODER_H
#define H264HWD_DECODER_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "basetype.h"
#include "h264hwd_storage.h"
#include "h264hwd_container.h"
#include "h264hwd_dpb.h"

namespace android {

/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/

/* enumerated return values of the functions */
typedef enum RET_E {
    H264BSD_RDY,
    H264BSD_PIC_RDY,
    H264BSD_HDRS_RDY,
    H264BSD_ERROR,
    H264BSD_PARAM_SET_ERROR,
    H264BSD_NEW_ACCESS_UNIT,
    H264BSD_FMO,
    H264BSD_UNPAIRED_FIELD,
    H264BSD_NONREF_PIC_SKIPPED,
    H264BSD_MEMFAIL
} RET_E;

/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

void h264bsdInit(storage_t * pStorage);
u32 h264bsdDecode(decContainer_t * pDecCont, const u8 * byteStrm, u32 len,
                  u32 picId, u32 * readBytes);
void h264bsdShutdown(storage_t * pStorage);

dpbOutPicture_t *h264bsdNextOutputPicture(storage_t * pStorage);

u32 h264bsdPicWidth(storage_t * pStorage);
u32 h264bsdPicHeight(storage_t * pStorage);
u32 h264bsdVideoRange(storage_t * pStorage);
u32 h264bsdMatrixCoefficients(storage_t * pStorage);
u32 h264bsdIsMonoChrome(storage_t * pStorage);
void h264bsdCroppingParams(storage_t * pStorage, u32 * croppingFlag,
                           u32 * left, u32 * width, u32 * top, u32 * height);

u32 h264bsdCheckValidParamSets(storage_t * pStorage);

u32 h264bsdAspectRatioIdc(const storage_t * pStorage);
void h264bsdSarSize(const storage_t * pStorage, u32 * sar_width,
                    u32 * sar_height);
extern i32 GetPoc(dpbPicture_t *pic);

}

#endif /* #ifdef H264HWD_DECODER_H */
