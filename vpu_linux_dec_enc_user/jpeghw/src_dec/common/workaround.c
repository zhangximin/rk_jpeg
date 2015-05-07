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
--  $RCSfile: workaround.c,v $
--  $Date: 2009/10/20 08:51:45 $
--  $Revision: 1.3 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of context

     1. Include headers
     2. External identifiers
     3. Module defines
     4. Module identifiers
     5. Fuctions
        5.1     StrmDec_GetBits
        5.2     StrmDec_ShowBits
        5.3     StrmDec_ShowBitsAligned
        5.4     StrmDec_FlushBits
        5.5     StrmDec_NextStartCode
        5.6     StrmDec_FindSync
        5.7     StrmDec_GetStartCode
        5.8     StrmDec_NumBits
        5.9     StrmDec_UnFlushBits

------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "workaround.h"

/*------------------------------------------------------------------------------
    2. External identifiers
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

#define MAGIC_WORD_LENGTH   (8)
#define MB_OFFSET           (4)

static const RK_U8 magicWord[MAGIC_WORD_LENGTH] = "Rosebud\0";


/*------------------------------------------------------------------------------
    4. Module indentifiers
------------------------------------------------------------------------------*/

static RK_U32 GetMbOffset( RK_U32 mbNum, RK_U32 vopWidth, RK_U32 vopHeight );

/*------------------------------------------------------------------------------

   5.1  Function name: GetMbOffset

        Purpose:

        Input:

        Output:

------------------------------------------------------------------------------*/
RK_U32 GetMbOffset( RK_U32 mbNum, RK_U32 vopWidth, RK_U32 vopHeight )
{
    RK_U32 mbRow, mbCol;
    RK_U32 offset;

    mbRow = mbNum / vopWidth;
    mbCol = mbNum % vopWidth;
    offset = mbRow*16*16*vopWidth + mbCol*16;

    return offset;
}

/*------------------------------------------------------------------------------

   5.1  Function name: PrepareStuffingWorkaround

        Purpose:

        Input:

        Output:

------------------------------------------------------------------------------*/
void StuffMacroblock( RK_U32 mbNum, RK_U8 * pDecOut, RK_U8 *pRefPic, RK_U32 vopWidth,
                     RK_U32 vopHeight )
{

    RK_U32 pixWidth;
    RK_U32 mbRow, mbCol;
    RK_U32 offset;
    RK_U32 lumaSize;
    RK_U8 *pSrc;
    RK_U8 *pDst;
    RK_U32 x, y;

    pixWidth = 16*vopWidth;

    mbRow = mbNum / vopWidth;
    mbCol = mbNum % vopWidth;

    offset = mbRow*16*pixWidth + mbCol*16;
    lumaSize = 256*vopWidth*vopHeight;

    if(pRefPic)
    {

        pDst = pDecOut;
        pSrc = pRefPic;

        pDst += offset;
        pSrc += offset;
        /* Copy luma data */
        for( y = 0 ; y < 16 ; ++y )
        {
            for( x = 0 ; x < 16 ; ++x )
            {
                pDst[x] = pSrc[x];
            }
            pDst += pixWidth;
            pSrc += pixWidth;
        }

        /* Chroma data */
        offset = mbRow*8*pixWidth + mbCol*16;

        pDst = pDecOut;
        pSrc = pRefPic;

        pDst += lumaSize;
        pSrc += lumaSize;
        pDst += offset;
        pSrc += offset;

        for( y = 0 ; y < 8 ; ++y )
        {
            for( x = 0 ; x < 16 ; ++x )
            {
                pDst[x] = pSrc[x];
            }
            pDst += pixWidth;
            pSrc += pixWidth;
        }
    }
    else
    {
        pDst = pDecOut + offset;
        /* Copy luma data */
        for( y = 0 ; y < 16 ; ++y )
        {
            for( x = 0 ; x < 16 ; ++x )
            {
                RK_S32 tmp;
                if( mbCol )
                    tmp = pDst[x-pixWidth] + pDst[x-1] - pDst[x-pixWidth-1];
                else
                    tmp = pDst[x-pixWidth];
                if( tmp < 0 )           tmp = 0;
                else if ( tmp > 255 )   tmp = 255;
                pDst[x] = tmp;
            }
            pDst += pixWidth;
        }

        /* Chroma data */
        offset = mbRow*8*pixWidth + mbCol*16;

        pDst = pDecOut + lumaSize + offset;

        for( y = 0 ; y < 8 ; ++y )
        {
            for( x = 0 ; x < 16 ; ++x )
            {
                RK_S32 tmp;
                if( mbCol )
                    tmp = pDst[x-pixWidth] + pDst[x-2] - pDst[x-pixWidth-2];
                else
                    tmp = pDst[x-pixWidth];
                if( tmp < 0 )           tmp = 0;
                else if ( tmp > 255 )   tmp = 255;
                pDst[x] = tmp;
            }
            pDst += pixWidth;
        }
    }
}

/*------------------------------------------------------------------------------

   5.1  Function name: PrepareStuffingWorkaround

        Purpose:

        Input:

        Output:

------------------------------------------------------------------------------*/
void PrepareStuffingWorkaround( RK_U8 *pDecOut, RK_U32 vopWidth, RK_U32 vopHeight )
{

    RK_U32 i;
    RK_U8 * pBase;

    pBase = pDecOut + GetMbOffset(vopWidth*vopHeight - MB_OFFSET,
        vopWidth, vopHeight );

    for( i = 0 ; i < MAGIC_WORD_LENGTH ; ++i )
        pBase[i] = magicWord[i];

}

/*------------------------------------------------------------------------------

   5.1  Function name: ProcessStuffingWorkaround

        Purpose: Check bytes written in PrepareStuffingWorkaround(). If bytes
                 match magic word, then error happened earlier on in the picture.
                 If bytes mismatch, then HW got to end of picture and error
                 interrupt is most likely because of faulty stuffing. In this
                 case we either conceal tail end of the frame or copy it from
                 previous frame.

        Input:

        Output:
            HANTRO_TRUE
            HANTRO_FALSE

------------------------------------------------------------------------------*/
RK_U32  ProcessStuffingWorkaround( RK_U8 * pDecOut, RK_U8 * pRefPic, RK_U32 vopWidth,
                                RK_U32 vopHeight )
{

    RK_U32 i;
    RK_U8 * pBase;
    RK_U32 numMbs;
    RK_U32 match = HANTRO_TRUE;

    numMbs = vopWidth*vopHeight;

    pBase = pDecOut + GetMbOffset(numMbs - MB_OFFSET, vopWidth, vopHeight );

    for( i = 0 ; i < MAGIC_WORD_LENGTH && match ; ++i )
        if( pBase[i] != magicWord[i] )
            match = HANTRO_FALSE;

    /* If 4th last macroblock is overwritten, then assume it's a stuffing
     * error. Copy remaining three macroblocks from previous ref frame. */
    if( !match )
    {
        for ( i = 1+numMbs - MB_OFFSET ; i < numMbs ; ++i )
        {
            StuffMacroblock( i, pDecOut, pRefPic, vopWidth, vopHeight );
        }
    }

    return match ? HANTRO_FALSE : HANTRO_TRUE;

}

/*------------------------------------------------------------------------------

   5.1  Function name: ProcessStuffingWorkaround

        Purpose:

        Input:

        Output:

------------------------------------------------------------------------------*/
void InitWorkarounds(RK_U32 decMode, workaround_t *pWorkarounds)
{
    RK_U32 asicId = 0x67311148;//VPUReadAsicIDDec();
    RK_U32 asicVer = asicId >> 16;
    RK_U32 asicBuild = asicId & 0xFFFF;

    /* set all workarounds off by default */
    pWorkarounds->stuffing = HANTRO_FALSE;
    pWorkarounds->startCode = HANTRO_FALSE;

    /* 8170 decoder does not support bad stuffing bytes. */
    if( asicVer == 0x8170U)
    {
        pWorkarounds->stuffing = HANTRO_TRUE;
    }
    else if( asicVer == 0x8190U )
    {
        switch(decMode)
        {
            case 1: /* MPEG4 */
                if( asicBuild < 0x2570 )
                    pWorkarounds->stuffing = HANTRO_TRUE;
                break;
            case 2: /* H263 */
                /* No HW tag supports this */
                pWorkarounds->stuffing = HANTRO_TRUE;
                break;
            case 4: /* VC1 */
                /* No HW tag supports this */
                pWorkarounds->stuffing = HANTRO_TRUE;
                break;
            case 5: /* MPEG2 */
            case 6: /* MPEG1 */
                if( asicBuild < 0x2470 )
                    pWorkarounds->stuffing = HANTRO_TRUE;
                break;
        }
    }
    if (decMode == 5 /*MPEG2*/)
        pWorkarounds->startCode = HANTRO_TRUE;

}

/*------------------------------------------------------------------------------

   5.1  Function name: PrepareStartCodeWorkaround

        Purpose: Prepare for start code workaround checking; write magic word
            to last 8 bytes of the picture (frame or field)

        Input:

        Output:

------------------------------------------------------------------------------*/
void PrepareStartCodeWorkaround( RK_U8 *pDecOut, RK_U32 vopWidth, RK_U32 vopHeight,
    RK_U32 topField )
{

    RK_U32 i;
    RK_U8 * pBase;

    pBase = pDecOut + vopWidth*vopHeight*256 - 8;
    if (topField)
        pBase -= 16*vopWidth;

    for( i = 0 ; i < MAGIC_WORD_LENGTH ; ++i )
        pBase[i] = magicWord[i];

}

/*------------------------------------------------------------------------------

   5.1  Function name: ProcessStartCodeWorkaround

        Purpose: Check bytes written in PrepareStartCodeWorkaround(). If bytes
                 match magic word, then error happened earlier on in the picture.
                 If bytes mismatch, then HW got to end of picture and timeout
                 interrupt is most likely because of corrupted startcode. In
                 this case we just ignore timeout.

                 Note: in addition to ignoring timeout, SW needs to find
                 next start code as HW does not update stream end pointer
                 properly. Methods of searching next startcode are mode
                 specific and cannot be done here.

        Input:

        Output:
            HANTRO_TRUE
            HANTRO_FALSE

------------------------------------------------------------------------------*/
RK_U32  ProcessStartCodeWorkaround( RK_U8 * pDecOut, RK_U32 vopWidth, RK_U32 vopHeight,
    RK_U32 topField )
{

    RK_U32 i;
    RK_U8 * pBase;
    RK_U32 numMbs;
    RK_U32 match = HANTRO_TRUE;

    pBase = pDecOut + vopWidth*vopHeight*256 - 8;
    if (topField)
        pBase -= 16*vopWidth;

    for( i = 0 ; i < MAGIC_WORD_LENGTH && match ; ++i )
        if( pBase[i] != magicWord[i] )
            match = HANTRO_FALSE;

    return match ? HANTRO_FALSE : HANTRO_TRUE;

}
