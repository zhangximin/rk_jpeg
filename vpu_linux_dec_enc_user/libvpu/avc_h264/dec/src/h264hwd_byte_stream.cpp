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
--  Abstract : Extract a NAL unit from byte stream
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264hwd_byte_stream.c,v $
--  $Date: 2008/09/11 08:32:04 $
--  $Revision: 1.2 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "h264hwd_byte_stream.h"
#include "h264hwd_util.h"
#include "deccfg.h"

namespace android {

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

#define BYTE_STREAM_ERROR  0xFFFFFFFF

#define	swap32bit(x) ((x>>24)|((x>>8)&0xff00)|((x<<8)&0xff0000)|(x<<24))

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Function name: ExtractNalUnit

        Functional description:
            Extracts one NAL unit from the byte stream buffer. Removes
            emulation prevention bytes if present. The original stream buffer
            is used directly and is therefore modified if emulation prevention
            bytes are present in the stream.

            Stream buffer is assumed to contain either exactly one NAL unit
            and nothing else, or one or more NAL units embedded in byte
            stream format described in the Annex B of the standard. Function
            detects which one is used based on the first bytes in the buffer.

        Inputs:
            pByteStream     pointer to byte stream buffer
            len             length of the stream buffer (in bytes)

        Outputs:
            pStrmData       stream information is stored here
            readBytes       number of bytes "consumed" from the stream buffer

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      error in byte stream

------------------------------------------------------------------------------*/

u32 h264bsdExtractNalUnit(const u8 * pByteStream, u32 len, strmData_t * pStrmData, u32 * readBytes)
{

/* Variables */

    u32 byteCount, initByteCount;
    u32 zeroCount;
    u8 byte;
    u32 invalidStream = HANTRO_FALSE;
    u32 hasEmulation = HANTRO_FALSE;
    const u8 *readPtr;

/* Code */

    ASSERT(pByteStream);
    ASSERT(len);
    ASSERT(len < BYTE_STREAM_ERROR);
    ASSERT(pStrmData);

    /* byte stream format if starts with 0x000001 or 0x000000 */
    if(len > 3 && pByteStream[0] == 0x00 && pByteStream[1] == 0x00 &&
       (pByteStream[2] & 0xFE) == 0x00)
    {
        DEBUG_PRINT(("BYTE STREAM detected\n"));
        /* search for NAL unit start point, i.e. point after first start code
         * prefix in the stream */
        zeroCount = byteCount = 2;
        readPtr = pByteStream + 2;
        /*lint -e(716) while(1) used consciously */
        while(1)
        {
            byte = *readPtr++;
            byteCount++;

            if(byteCount == len)
            {
                /* no start code prefix found -> error */
                *readBytes = len;

                ERROR_PRINT("NO START CODE PREFIX");
                return (HANTRO_NOK);
            }

            if(!byte)
                zeroCount++;
            else if((byte == 0x01) && (zeroCount >= 2))
                break;
            else
                zeroCount = 0;
        }

        initByteCount = byteCount;
        pStrmData->pStrmBuffStart = pByteStream + initByteCount;
        pStrmData->pStrmCurrPos = pStrmData->pStrmBuffStart;
        pStrmData->bitPosInWord = 0;
        pStrmData->strmBuffReadBits = 0;
        pStrmData->strmBuffSize = len - initByteCount;
        *readBytes = len;
        return (HANTRO_OK);
    }
    /* separate NAL units as input -> just set stream params */
    else
    {
        DEBUG_PRINT(("SINGLE NAL unit detected\n"));

        initByteCount = 0;
        zeroCount = 0;
        pStrmData->strmBuffSize = len;
        hasEmulation = HANTRO_TRUE;
    }

    pStrmData->pStrmBuffStart = pByteStream + initByteCount;
    pStrmData->pStrmCurrPos = pStrmData->pStrmBuffStart;
    pStrmData->bitPosInWord = 0;
    pStrmData->strmBuffReadBits = 0;

    /* return number of bytes "consumed" */
    *readBytes = pStrmData->strmBuffSize + initByteCount + zeroCount;

    if(invalidStream)
    {

        ERROR_PRINT("INVALID STREAM");
        return (HANTRO_NOK);
    }

	//find next nal unit when have 2 filed data.
	{
		zeroCount = byteCount = 0;
        readPtr = pByteStream + 0;
        /*lint -e(716) while(1) used consciously */
        while(1)
        {
            byte = *readPtr++;
            byteCount++;

            if(byteCount == len)
            {
                *readBytes = len;
                return (HANTRO_NOK);
            }

            if(!byte)
                zeroCount++;
            else if((byte == 0x01) && (zeroCount >= 2))
                break;
            else
                zeroCount = 0;
        }

        initByteCount = byteCount;
        pStrmData->pStrmBuffStart = pByteStream + initByteCount;
        pStrmData->pStrmCurrPos = pStrmData->pStrmBuffStart;
        pStrmData->bitPosInWord = 0;
        pStrmData->strmBuffReadBits = 0;
        pStrmData->strmBuffSize = len - initByteCount;
        *readBytes = len;
        return (HANTRO_OK);
	}
	
	#if 0
    /* remove emulation prevention bytes before rbsp processing */
    if(hasEmulation && pStrmData->removeEmul3Byte)
    {
        i32 i = pStrmData->strmBuffSize;
        u8 *writePtr = (u8 *) pStrmData->pStrmBuffStart;

        readPtr = pStrmData->pStrmBuffStart;

        zeroCount = 0;
        while(i--)
        {
            if((zeroCount == 2) && (*readPtr == 0x03))
            {
                /* emulation prevention byte shall be followed by one of the
                 * following bytes: 0x00, 0x01, 0x02, 0x03. This implies that
                 * emulation prevention 0x03 byte shall not be the last byte
                 * of the stream. */
                if((i == 0) || (*(readPtr + 1) > 0x03))
                    return (HANTRO_NOK);

                DEBUG_PRINT(("EMULATION PREVENTION 3 BYTE REMOVED\n"));

                /* do not write emulation prevention byte */
                readPtr++;
                zeroCount = 0;
            }
            else
            {
                /* NAL unit shall not contain byte sequences 0x000000,
                 * 0x000001 or 0x000002 */
                if((zeroCount == 2) && (*readPtr <= 0x02))
                    return (HANTRO_NOK);

                if(*readPtr == 0)
                    zeroCount++;
                else
                    zeroCount = 0;

                *writePtr++ = *readPtr++;
            }
        }

        /* (readPtr - writePtr) indicates number of "removed" emulation
         * prevention bytes -> subtract from stream buffer size */
        pStrmData->strmBuffSize -= (u32) (readPtr - writePtr);
    }
	#endif
	
    return (HANTRO_OK);
}

/*------------------------------------------------------------------------------
    Function name   : h264bsdFindNextStartCode
    Description     :
    Return type     : const u8 *
    Argument        : const u8 * pByteStream
    Argument        : u32 len
------------------------------------------------------------------------------*/
const u8 *h264bsdFindNextStartCode(const u8 * pByteStream, u32 len)
{
    u32 byteCount = 0;
    u32 zeroCount = 0;
    const u8 *start = NULL;

    /* determine size of the NAL unit. Search for next start code prefix
     * or end of stream  */

    /* start from second byte */
    pByteStream++;
    len--;

    while(byteCount++ < len)
    {
        u32 byte = *pByteStream++;

        if(byte == 0)
            zeroCount++;
        else
        {
            if((byte == 0x01) && (zeroCount >= 2))
            {
                start = pByteStream - MIN(zeroCount, 3) - 1;
                break;
            }

            zeroCount = 0;
        }
    }

    return start;

}

}
