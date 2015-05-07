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
--  Abstract : Utility macros and functions
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264hwd_util.c,v $
--  $Date: 2008/10/01 12:30:48 $
--  $Revision: 1.3 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of contents

     1. Include headers
     2. External compiler flags
     3. Module defines
     4. Local function prototypes
     5. Functions
          h264bsdCountLeadingZeros
          h264bsdRbspTrailingBits
          h264bsdMoreRbspData
          h264bsdNextMbAddress

------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "h264hwd_util.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/* look-up table for expected values of stuffing bits */
static const u32 stuffingTable[8] = {0x1,0x2,0x4,0x8,0x10,0x20,0x40,0x80};

/* look-up table for chroma quantization parameter as a function of luma QP */
const u32 h264bsdQpC[52] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,
    20,21,22,23,24,25,26,27,28,29,29,30,31,32,32,33,34,34,35,35,36,36,37,37,37,
    38,38,38,39,39,39,39};

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

static u32 MoreRbspTrailingData(strmData_t *pStrmData);
/*------------------------------------------------------------------------------

   5.1  Function: h264bsdCountLeadingZeros

        Functional description:
            Count leading zeros in a code word. Code word is assumed to be
            right-aligned, last bit of the code word in the lsb of the value.

        Inputs:
            value   code word
            length  number of bits in the code word

        Outputs:
            none

        Returns:
            number of leading zeros in the code word

------------------------------------------------------------------------------*/

u32 h264bsdCountLeadingZeros(u32 value, u32 length)
{

/* Variables */

    u32 zeros = 0;
    u32 mask = 1 << (length - 1);

/* Code */

    ASSERT(length <= 32);

    while (mask && !(value & mask))
    {
        zeros++;
        mask >>= 1;
    }
    return(zeros);

}

/*------------------------------------------------------------------------------

   5.2  Function: h264bsdRbspTrailingBits

        Functional description:
            Check Raw Byte Stream Payload (RBSP) trailing bits, i.e. stuffing.
            Rest of the current byte (whole byte if allready byte aligned)
            in the stream buffer shall contain a '1' bit followed by zero or
            more '0' bits.

        Inputs:
            pStrmData   pointer to stream data structure

        Outputs:
            none

        Returns:
            HANTRO_OK      RBSP trailing bits found
            HANTRO_NOK     otherwise

------------------------------------------------------------------------------*/

u32 h264bsdRbspTrailingBits(strmData_t *pStrmData)
{

/* Variables */

    u32 stuffing;
    u32 stuffingLength;

/* Code */

    ASSERT(pStrmData);
    ASSERT(pStrmData->bitPosInWord < 8);

    stuffingLength = 8 - pStrmData->bitPosInWord;

    stuffing = h264bsdGetBits(pStrmData, stuffingLength);
    if (stuffing == END_OF_STREAM)
        return(HANTRO_NOK);

#ifdef HANTRO_PEDANTIC_MODE
    if (stuffing != stuffingTable[stuffingLength - 1])
        return(HANTRO_NOK);
    else
#endif
        return(HANTRO_OK);

}

/*------------------------------------------------------------------------------

   5.2  Function: h264bsdRbspSliceTrailingBits

        Functional description:
            Check slice trailing bits if CABAC is on.
            
        Inputs:
            pStrmData   pointer to stream data structure

        Outputs:
            none

        Returns:
            HANTRO_OK      RBSP slice trailing bits found
            HANTRO_NOK     otherwise

------------------------------------------------------------------------------*/
#ifdef H264_CABAC
u32 h264bsdRbspSliceTrailingBits(strmData_t *pStrmData)
{

/* Variables */

    u32 czw;
    u32 tmp;

/* Code */

    ASSERT(pStrmData);
    ASSERT(pStrmData->bitPosInWord < 8);

    if( pStrmData->bitPosInWord > 0 )
        h264bsdRbspTrailingBits( pStrmData );

    while(MoreRbspTrailingData( pStrmData ))
    {
        /* Read 16 bits */
        czw = h264bsdShowBits(pStrmData, 16 );
        tmp = h264bsdFlushBits(pStrmData, 16);
        if (tmp == END_OF_STREAM || czw)
            return(END_OF_STREAM);
    }

    return (HANTRO_OK);
}
#endif /* H264_CABAC */

/*------------------------------------------------------------------------------

   5.6  Function: MoreRbspTrailingData

        Functional description:
          <++>
        Inputs:
          <++>
        Outputs:
          <++>

------------------------------------------------------------------------------*/

u32 MoreRbspTrailingData(strmData_t *pStrmData)
{

/* Variables */

    i32 bits;

/* Code */

    ASSERT(pStrmData);
    ASSERT(pStrmData->strmBuffReadBits <= 8 * pStrmData->strmBuffSize);

    bits = (i32)pStrmData->strmBuffSize*8 - (i32)pStrmData->strmBuffReadBits;
    if ( bits >= 8 )
        return(HANTRO_TRUE);
    else
        return(HANTRO_FALSE);

}

/*------------------------------------------------------------------------------

   5.3  Function: h264bsdMoreRbspData

        Functional description:
            Check if there is more data in the current RBSP. The standard
            defines this function so that there is more data if
                -more than 8 bits left or
                -last bits are not RBSP trailing bits

        Inputs:
            pStrmData   pointer to stream data structure

        Outputs:
            none

        Returns:
            HANTRO_TRUE    there is more data
            HANTRO_FALSE   no more data

------------------------------------------------------------------------------*/

u32 h264bsdMoreRbspData(strmData_t *pStrmData)
{

/* Variables */

    u32 bits;

/* Code */

    ASSERT(pStrmData);
    ASSERT(pStrmData->strmBuffReadBits <= 8 * pStrmData->strmBuffSize);

    bits = pStrmData->strmBuffSize * 8 - pStrmData->strmBuffReadBits;

    if (bits == 0)
        return(HANTRO_FALSE);

    if (bits > 8)
    {
        if (pStrmData->removeEmul3Byte)
            return(HANTRO_TRUE);

        bits &= 0x7;
        if (!bits) bits = 8;
        if (h264bsdShowBits(pStrmData, bits) != (1 << (bits-1)) ||
            (h264bsdShowBits(pStrmData, 23+bits) << 9))
            return(HANTRO_TRUE);
        else 
            return(HANTRO_FALSE);
    }
    else if (h264bsdShowBits(pStrmData,bits) != (1 << (bits-1)) )
        return(HANTRO_TRUE);
    else
        return(HANTRO_FALSE);

}

/*------------------------------------------------------------------------------

   5.4  Function: h264bsdNextMbAddress

        Functional description:
            Get address of the next macroblock in the current slice group.

        Inputs:
            pSliceGroupMap      slice group for each macroblock
            picSizeInMbs        size of the picture
            currMbAddr          where to start

        Outputs:
            none

        Returns:
            address of the next macroblock
            0   if none of the following macroblocks belong to same slice
                group as currMbAddr

------------------------------------------------------------------------------*/

u32 h264bsdNextMbAddress(u32 *pSliceGroupMap, u32 picSizeInMbs, u32 currMbAddr)
{

/* Variables */

    u32 i, sliceGroup;

/* Code */

    ASSERT(pSliceGroupMap);
    ASSERT(picSizeInMbs);
    ASSERT(currMbAddr < picSizeInMbs);

    sliceGroup = pSliceGroupMap[currMbAddr];

    i = currMbAddr + 1;

    while ((i < picSizeInMbs) && (pSliceGroupMap[i] != sliceGroup))
        i++;

    if (i == picSizeInMbs)
        i = 0;

    return(i);

}

/*------------------------------------------------------------------------------

   5.4  Function: h264CheckCabacZeroWords

        Functional description:

        Inputs:

        Outputs:

        Returns:

------------------------------------------------------------------------------*/

u32 h264CheckCabacZeroWords( strmData_t *strmData )
{

/* Variables */

    u32 tmp;

/* Code */

    ASSERT(strmData);

    while (MoreRbspTrailingData(strmData))
    {
        tmp = h264bsdGetBits(strmData, 16 );
        if( tmp == END_OF_STREAM )
            return HANTRO_OK;
        else if ( tmp )
            return HANTRO_NOK;
    }

    return HANTRO_OK;

}

