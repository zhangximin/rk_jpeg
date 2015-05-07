/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2007 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Description : API for the 8190 RV Decoder
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: on2rvdecapi.h,v $
--  $Date: 2010/02/24 08:25:12 $
--  $Revision: 1.3 $
--
------------------------------------------------------------------------------*/

#ifndef __ON2RVDECAPI_H__
#define __ON2RVDECAPI_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "basetype.h"

/* return values */
#define MAKE_RESULT(sev,fac,code) \
    (((u32)sev << 31) | ((u32)4 << 16) | ((fac<<6) | (code)))

#define ON2RVDEC_OK                     MAKE_RESULT(0,0,0x0000)
#define ON2RVDEC_OUTOFMEMORY            MAKE_RESULT(1,7,0x000e)
#define ON2RVDEC_INVALID_PARAMETER      MAKE_RESULT(1,7,0x0057)
#define ON2RVDEC_NOTIMPL                MAKE_RESULT(1,0,0x4001)
#define ON2RVDEC_POINTER                MAKE_RESULT(1,0,0x4003)
#define ON2RVDEC_FAIL                   MAKE_RESULT(1,0,0x4005)

typedef u32 On2RvDecRet;

/* custom message handling */
#define ON2RV_MSG_ID_Set_RVDecoder_RPR_Sizes 36

typedef u32 On2RvCustomMessage_ID;

typedef struct
{
    On2RvCustomMessage_ID message_id;
    u32 num_sizes;
    u32 *sizes;
} On2RvMsgSetDecoderRprSizes;

/* input and output flag definitions */
#define ON2RV_DECODE_MORE_FRAMES    0x00000001
#define ON2RV_DECODE_DONT_DRAW      0x00000002
#define ON2RV_DECODE_KEY_FRAME      0x00000004
#define ON2RV_DECODE_B_FRAME        0x00000008
#define ON2RV_DECODE_LAST_FRAME     0x00000200

/* input and output structures */
typedef struct
{
    i32 bIsValid;
    u32 ulSegmentOffset;
} codecSegmentInfo;

typedef struct
{
    u32 dataLength;
    i32 bInterpolateImage;
    u32 numDataSegments;
    codecSegmentInfo *pDataSegments;
    u32 flags;
    u32 timestamp;
    u32 streamBusAddr;
} On2DecoderInParams;

typedef struct
{
    u32 numFrames;
    u32 notes;
    u32 timestamp;
    u32 width;
    u32 height;
    u8 *pOutFrame;
} On2DecoderOutParams;

/* decoder initialization structure */
typedef struct
{
    u16 outtype;
    u16 pels;
    u16 lines;
    u16 nPadWidth;
    u16 nPadHeight;
    u16 pad_to_32;
    u32 ulInvariants;
    i32 packetization;
    u32 ulStreamVersion;
} On2DecoderInit;

/* decoding function */
On2RvDecRet On2RvDecDecode(u8 *pRV10Packets,
    u8   *pDecodedFrameBuffer, /* unused */
    void *pInputParams,
    void *pOutputParams,
    void *decInst);

/* initialization function */
On2RvDecRet On2RvDecInit(void *pRV10Init,
    void **pDecInst);

/* release function */
On2RvDecRet On2RvDecFree(void *decInst);

/* custom message handling function. Only Set_RPR_Sizes message implemented */
On2RvDecRet On2RvDecCustomMessage(void *msg_id, void *decInst);

/* unused, always returns DEC_NOTIMPL */
On2RvDecRet On2RvDecHiveMessage(void *msg, void *decInst);

/* function to obtain last decoded picture out from the decoder */
On2RvDecRet On2RvDecPeek(void *pOutputParams, void *decInst);

#ifdef __cplusplus
}
#endif

#endif  /* __ON2RVDECAPI_H__ */
