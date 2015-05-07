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
--  $RCSfile: h264hwd_stream.c,v $
--  $Date: 2008/03/13 12:48:06 $
--  $Revision: 1.1 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "h264hwd_util.h"
#include "h264hwd_stream.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Function: h264bsdGetBits

        Functional description:
            Read and remove bits from the stream buffer.

        Input:
            pStrmData   pointer to stream data structure
            numBits     number of bits to read

        Output:
            none

        Returns:
            bits read from stream
            END_OF_STREAM if not enough bits left

------------------------------------------------------------------------------*/

u32 h264bsdGetBits(strmData_t * pStrmData, u32 numBits)
{

    u32 out;

    ASSERT(pStrmData);
    ASSERT(numBits < 32);

    out = h264bsdShowBits(pStrmData, 32) >> (32 - numBits);

    if(h264bsdFlushBits(pStrmData, numBits) == HANTRO_OK)
    {
        return (out);
    }
    else
    {
        return (END_OF_STREAM);
    }

}

/*------------------------------------------------------------------------------

   5.2  Function: HwShowBits

        Functional description:
          Read bits from input stream. Bits are located right
          aligned in the 32-bit output word. In case stream ends,
          function fills the word with zeros. For example, numBits = 18 and
          there are 7 bits left in the stream buffer -> return
              00000000000000xxxxxxx00000000000,
          where 'x's represent actual bits read from buffer.

        Input:

        Output:

------------------------------------------------------------------------------*/

u32 h264bsdShowBits(strmData_t * pStrmData, u32 numBits)
{

    i32 bits;
    u32 out, outBits;
    u32 tmpReadBits;
    const u8 *pStrm;

    ASSERT(pStrmData);
    ASSERT(pStrmData->pStrmCurrPos);
    ASSERT(pStrmData->bitPosInWord < 8);
    ASSERT(pStrmData->bitPosInWord == (pStrmData->strmBuffReadBits & 0x7));
    ASSERT(numBits <= 32);

    pStrm = pStrmData->pStrmCurrPos;

    /* bits left in the buffer */
    bits =
        (i32) pStrmData->strmBuffSize * 8 - (i32) pStrmData->strmBuffReadBits;

    if(/*!numBits || */!bits)   /* for the moment is always called  with numBits = 32 */
    {
        return (0);
    }

    if(!pStrmData->removeEmul3Byte)
    {

        out = outBits = 0;
        tmpReadBits = pStrmData->strmBuffReadBits;

        if(pStrmData->bitPosInWord)
        {
            out = pStrm[0] << (24 + pStrmData->bitPosInWord);
            pStrm++;
            outBits = 8 - pStrmData->bitPosInWord;
            bits -= outBits;
            tmpReadBits += outBits;
        }

        while(bits && outBits < numBits)
        {

            /* check emulation prevention byte */
            if(tmpReadBits >= 16 &&
               pStrm[-2] == 0x0 && pStrm[-1] == 0x0 && pStrm[0] == 0x3)
            {
                pStrm++;
                tmpReadBits += 8;
                bits -= 8;
                /* emulation prevention byte shall not be the last byte of the
                 * stream */
                if(bits <= 0)
                    break;
            }

            tmpReadBits += 8;

            if(outBits <= 24)
                out |= (u32) (*pStrm++) << (24 - outBits);
            else
                out |= (u32) (*pStrm++) >> (outBits - 24);

            outBits += 8;
            bits -= 8;
        }

        return (out >> (32 - numBits));

    }
    else
    {
        u32 shift;

        /* at least 32-bits in the buffer */
        if(bits >= 32)
        {
            u32 bitPosInWord = pStrmData->bitPosInWord;

            out = ((u32) pStrm[3]) | ((u32) pStrm[2] << 8) |
                ((u32) pStrm[1] << 16) | ((u32) pStrm[0] << 24);

            if(bitPosInWord)
            {
                out <<= bitPosInWord;
                out |= (u32) pStrm[4] >> (8 - bitPosInWord);
            }

            return (out >> (32 - numBits));
        }
        /* at least one bit in the buffer */
        else if(bits > 0)
        {
            shift = (i32) (24 + pStrmData->bitPosInWord);
            out = (u32) (*pStrm++) << shift;
            bits -= (i32) (8 - pStrmData->bitPosInWord);
            while(bits > 0)
            {
                shift -= 8;
                out |= (u32) (*pStrm++) << shift;
                bits -= 8;
            }
            return (out >> (32 - numBits));
        }
        else
            return (0);
    }
}

/*------------------------------------------------------------------------------

    Function: h264bsdFlushBits

        Functional description:
            Remove bits from the stream buffer

        Input:
            pStrmData       pointer to stream data structure
            numBits         number of bits to remove

        Output:
            none

        Returns:
            HANTRO_OK       success
            END_OF_STREAM   not enough bits left

------------------------------------------------------------------------------*/

u32 h264bsdFlushBits(strmData_t * pStrmData, u32 numBits)
{

    u32 bytesLeft;
    const u8 *pStrm;

    ASSERT(pStrmData);
    ASSERT(pStrmData->pStrmBuffStart);
    ASSERT(pStrmData->pStrmCurrPos);
    ASSERT(pStrmData->bitPosInWord < 8);
    ASSERT(pStrmData->bitPosInWord == (pStrmData->strmBuffReadBits & 0x7));
    if(!pStrmData->removeEmul3Byte)
    {

        if((pStrmData->strmBuffReadBits + numBits) >
           (8 * pStrmData->strmBuffSize))
        {
            pStrmData->strmBuffReadBits = 8 * pStrmData->strmBuffSize;
            pStrmData->bitPosInWord = 0;
            pStrmData->pStrmCurrPos =
                pStrmData->pStrmBuffStart + pStrmData->strmBuffSize;
            return (END_OF_STREAM);
        }
        else
        {
            bytesLeft =
                (8 * pStrmData->strmBuffSize - pStrmData->strmBuffReadBits) / 8;
            pStrm = pStrmData->pStrmCurrPos;
            if(pStrmData->bitPosInWord)
            {
                if(numBits < 8 - pStrmData->bitPosInWord)
                {
                    pStrmData->strmBuffReadBits += numBits;
                    pStrmData->bitPosInWord += numBits;
                    return (HANTRO_OK);
                }
                numBits -= 8 - pStrmData->bitPosInWord;
                pStrmData->strmBuffReadBits += 8 - pStrmData->bitPosInWord;
                pStrmData->bitPosInWord = 0;
                pStrm++;

                if(pStrmData->strmBuffReadBits >= 16 && bytesLeft &&
                   pStrm[-2] == 0x0 && pStrm[-1] == 0x0 && pStrm[0] == 0x3)
                {
                    pStrm++;
                    pStrmData->strmBuffReadBits += 8;
                    bytesLeft--;
                    pStrmData->emulByteCount++;
                }

            }

            while(numBits >= 8 && bytesLeft)
            {
                if(bytesLeft > 2 && pStrm[0] == 0 && pStrm[1] == 0 &&
                   pStrm[2] <= 1)
                {
                    /* trying to flush part of start code prefix -> error */
                    return (HANTRO_NOK);
                }

                pStrm++;
                pStrmData->strmBuffReadBits += 8;
                bytesLeft--;

                /* check emulation prevention byte */
                if(pStrmData->strmBuffReadBits >= 16 && bytesLeft &&
                   pStrm[-2] == 0x0 && pStrm[-1] == 0x0 && pStrm[0] == 0x3)
                {
                    pStrm++;
                    pStrmData->strmBuffReadBits += 8;
                    bytesLeft--;
                    pStrmData->emulByteCount++;
                }
                numBits -= 8;
            }

            if(numBits && bytesLeft)
            {
                if(bytesLeft > 2 && pStrm[0] == 0 && pStrm[1] == 0 &&
                   pStrm[2] <= 1)
                {
                    /* trying to flush part of start code prefix -> error */
                    return (HANTRO_NOK);
                }

                pStrmData->strmBuffReadBits += numBits;
                pStrmData->bitPosInWord = numBits;
                numBits = 0;
            }

            pStrmData->pStrmCurrPos = pStrm;

            if(numBits)
                return (END_OF_STREAM);
            else
                return (HANTRO_OK);
        }
    }
    else
    {
        pStrmData->strmBuffReadBits += numBits;
        pStrmData->bitPosInWord = pStrmData->strmBuffReadBits & 0x7;
        if((pStrmData->strmBuffReadBits) <= (8 * pStrmData->strmBuffSize))
        {
            pStrmData->pStrmCurrPos = pStrmData->pStrmBuffStart +
                (pStrmData->strmBuffReadBits >> 3);
            return (HANTRO_OK);
        }
        else
            return (END_OF_STREAM);
    }
}

/*------------------------------------------------------------------------------

    Function: h264bsdIsByteAligned

        Functional description:
            Check if current stream position is byte aligned.

        Inputs:
            pStrmData   pointer to stream data structure

        Outputs:
            none

        Returns:
            HANTRO_TRUE        stream is byte aligned
            HANTRO_FALSE       stream is not byte aligned

------------------------------------------------------------------------------*/

u32 h264bsdIsByteAligned(strmData_t * pStrmData)
{

/* Variables */

/* Code */

    if(!pStrmData->bitPosInWord)
        return (HANTRO_TRUE);
    else
        return (HANTRO_FALSE);

}
