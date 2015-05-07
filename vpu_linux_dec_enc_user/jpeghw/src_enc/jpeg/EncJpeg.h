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
--  Abstract  : 
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: EncJpeg.h,v $
--  $Revision: 1.2 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of contents 
   
    1. Include headers
    2. Module defines
    3. Data types
    4. Function prototypes

------------------------------------------------------------------------------*/
#ifndef __ENC_JPEG_H__
#define __ENC_JPEG_H__

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "jpegencapi.h"

#include "vpu_type.h"
#include "enccommon.h"

#include "EncJpegPutBits.h"

/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/

#define MAX_NUMBER_OF_COMPONENTS 3

/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/

enum
{
    ENC_WHOLE_FRAME,
    ENC_PARTIAL_FRAME,
    ENC_420_MODE,
    ENC_422_MODE
};

typedef enum
{
    ENC_NO_UNITS = 0,
    ENC_DOTS_PER_INCH = 1,
    ENC_DOTS_PER_CM = 2
} EncAppUnitsType;

enum
{
    ENC_SINGLE_MARKER,
    ENC_MULTI_MARKER
};

typedef struct JpegEncQuantTables_t
{
    const RK_U8 *pQlumi;
    const RK_U8 *pQchromi;

} JpegEncQuantTables;

typedef struct JpegEncFrameHeader_t /* SOF0 */
{
    RK_U32 header;
    RK_U32 Lf;
    RK_U32 P;
    RK_U32 Y;
    RK_U32 X;
    RK_U32 Nf;
    RK_U32 Ci[MAX_NUMBER_OF_COMPONENTS];
    RK_U32 Hi[MAX_NUMBER_OF_COMPONENTS];
    RK_U32 Vi[MAX_NUMBER_OF_COMPONENTS];
    RK_U32 Tqi[MAX_NUMBER_OF_COMPONENTS];

} JpegEncFrameHeader;

typedef struct JpegEncCommentHeader_t   /* COM */
{
    RK_U32 comEnable;
    RK_U32 Lc;
    RK_U32 comLen;
    const RK_U8 *pComment;

} JpegEncCommentHeader;

typedef struct JpegEncRestart_t /* DRI */
{
    RK_U32 Lr;
    RK_U32 Ri;

} JpegEncRestart;

typedef struct JpegEncAppn_t    /* APP0 */
{
    RK_U32 Lp;
    RK_U32 ident1;
    RK_U32 ident2;
    RK_U32 ident3;
    RK_U32 version;
    RK_U32 units;
    RK_U32 Xdensity;
    RK_U32 Ydensity;
    /*RK_U32 XThumbnail;*/
    /*RK_U32 YThumbnail;*/
    /*RK_U32 thumbMode;*/
    /*RK_U32 rgb1;*/
    /*RK_U32 rgb2;*/
    RK_U32 thumbEnable;
    /*RK_U32 thumbSize;*/
    /*RK_U32 targetStart;*/
    /*RK_U32 targetEnd;*/
    /*RK_U8 *pStartOfOutput;*/
    /*RK_U8 *pHor;*/
    /*RK_U8 *pVer;*/
    /*RK_U32 appExtLp;*/
    /*RK_U32 appExtId1;*/
    /*RK_U32 appExtId2;*/
    /*RK_U32 appExtId3;*/
    /*RK_U32 extCode;*/
} JpegEncAppn_t;

typedef struct
{
    true_e header;
    JpegEncRestart restart; /* Restart Interval             */
    RK_S32 rstCount;
    JpegEncFrameHeader frame;   /* Frame Header Data            */
    JpegEncCommentHeader com;   /* COM Header Data              */
    JpegEncAppn_t appn; /* APPn Header Data             */
    JpegEncQuantTables qTable;
    RK_S32 markerType;
    RK_S32 codingType; /* Whole or slice */
    RK_S32 codingMode; /* 420 or 422 */
    RK_S32 sliceNum;   /* Number of current input slice */
    RK_S32 sliceRows;  /* Amount of MB rows in a slice */
    /*RK_S32 rotation;*/   /* Rotation 0/-90*+90 */
    RK_S32 width;
    RK_S32 height;
    RK_S32 mbNum;
    RK_S32 mbPerFrame;
    RK_S32 row;
    /*RK_S32 column;*/
    /*RK_S32 lastColumn;*/
    /*RK_S32 dcAbove[6];*/
    /*RK_S32 dcCurrent[6];*/
    /*RK_S32 dc[6];*/  /* Macroblock DC */
    /*RK_S32 rlcCount[6];*/    /* Block RLC count */
    /*const RK_S16 *rlc[6];*/  /* RLC data for each block */
    RK_U8 qTableLuma[64];
    RK_U8 qTableChroma[64];
    RK_U8 *streamStartAddress; /* output start address */
    JpegEncThumb thumbnail; /* thumbnail data */
} jpegData_s;

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

void EncJpegInit(jpegData_s * jpeg);

RK_U32 EncJpegHdr(stream_s * stream, jpegData_s * data);

/*void EncJpegImageEnd(stream_s * stream, jpegData_s * data);*/

/*void EncJpegImageEndReplaceRst(stream_s * stream, jpegData_s * data);*/

#endif
