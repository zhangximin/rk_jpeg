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
--  Description : Jpeg decoder header decoding source code
--
------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: jpegdechdrs.c,v $
--  $Revision: 1.10 $
--  $Date: 2010/05/12 11:58:33 $
--

------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------

    Table of contents

    1. Include headers
    2. External compiler flags
    3. Module defines
    4. Local function prototypes
    5. Functions
        - JpegDecDecodeFrame
        - JpegDecDecodeQuantTables
        - JpegDecDecodeHuffmanTables

------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "jpegdechdrs.h"
#include "jpegdecutils.h"
#include "jpegdecmarkers.h"
#include "jpegdecinternal.h"
#include "jpegdecscan.h"
#include "jpegdecapi.h"
#include "dwl.h"

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
    5. Functions
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

        Function name: JpegDecDecodeFrame

        Functional description:
          Decodes frame headers

        Inputs:
          JpegDecContainer *pDecData      Pointer to JpegDecContainer structure

        Outputs:
          OK/NOK

------------------------------------------------------------------------------*/
JpegDecRet JpegDecDecodeFrameHdr(JpegDecContainer * pDecData)
{
    RK_U32 i;
    RK_U32 width, height;
    RK_U32 tmp1, tmp2;
    RK_U32 Hmax = 0;
    RK_U32 Vmax = 0;
    RK_U32 size = 0;
    JpegDecRet retCode;

    retCode = JPEGDEC_OK;

    /* frame header length */
    pDecData->frame.Lf = JpegDecGet2Bytes(&(pDecData->stream));

    /* check if there is enough data */
    if(((pDecData->stream.readBits / 8) + pDecData->frame.Lf) >
       pDecData->stream.streamLength)
        return (JPEGDEC_STRM_ERROR);

    /* Sample precision */
    pDecData->frame.P = JpegDecGetByte(&(pDecData->stream));
    if(pDecData->frame.P != 8)
    {
        JPEGDEC_TRACE_INTERNAL(("if ( pDecData->frame.P != 8)\n"));
        return (JPEGDEC_UNSUPPORTED);
    }
    /* Number of Lines */
    pDecData->frame.Y = JpegDecGet2Bytes(&(pDecData->stream));
    if(pDecData->frame.Y < 1)
    {
        return (JPEGDEC_UNSUPPORTED);
    }
    pDecData->frame.hwY = pDecData->frame.Y;

    /* round up to next multiple-of-16 */
    pDecData->frame.hwY += 0xf;
    pDecData->frame.hwY &= ~(0xf);

    /* Number of samples per line */
    pDecData->frame.X = JpegDecGet2Bytes(&(pDecData->stream));
    if(pDecData->frame.X < 1)
    {
        return (JPEGDEC_UNSUPPORTED);
    }
    pDecData->frame.hwX = pDecData->frame.X;

    /* round up to next multiple-of-16 */
    pDecData->frame.hwX += 0xf;
    pDecData->frame.hwX &= ~(0xf);

    /* for internal() */
    pDecData->info.X = pDecData->frame.hwX;
    pDecData->info.Y = pDecData->frame.hwY;

    /* check for minimum and maximum dimensions */
    if(pDecData->frame.hwX < pDecData->minSupportedWidth ||
       pDecData->frame.hwY < pDecData->minSupportedHeight ||
       pDecData->frame.hwX > pDecData->maxSupportedWidth ||
       pDecData->frame.hwY > pDecData->maxSupportedHeight ||
       (pDecData->frame.hwX * pDecData->frame.hwY) >
       pDecData->maxSupportedPixelAmount)
    {
        JPEGDEC_TRACE_INTERNAL(("FRAME: Unsupported size\n"));
        return (JPEGDEC_UNSUPPORTED);
    }

    /* Number of components */
    pDecData->frame.Nf = JpegDecGetByte(&(pDecData->stream));
    if((pDecData->frame.Nf != 3) && (pDecData->frame.Nf != 1))
    {
        JPEGDEC_TRACE_INTERNAL(("pDecData->frame.Nf != 3 && pDecData->frame.Nf != 1\n"));
        return (JPEGDEC_UNSUPPORTED);
    }

    /* save component specific data */
    /* Nf == number of components */
    for(i = 0; i < pDecData->frame.Nf; i++)
    {
        pDecData->frame.component[i].C = JpegDecGetByte(&(pDecData->stream));
        if(i == 0)  /* for the first component */
        {
            /* if first component id is something else than 1 (jfif) */
            pDecData->scan.index = pDecData->frame.component[i].C;
        }
        else
        {
            /* if component ids 'jumps' */
            if((pDecData->frame.component[i - 1].C + 1) !=
               pDecData->frame.component[i].C)
            {
                JPEGDEC_TRACE_INTERNAL(("component ids 'jumps'\n"));
                return (JPEGDEC_UNSUPPORTED);
            }
        }
        tmp1 = JpegDecGetByte(&(pDecData->stream));
        pDecData->frame.component[i].H = tmp1 >> 4;
        if(pDecData->frame.component[i].H > Hmax)
        {
            Hmax = pDecData->frame.component[i].H;
        }
        pDecData->frame.component[i].V = tmp1 & 0xF;
        if(pDecData->frame.component[i].V > Vmax)
        {
            Vmax = pDecData->frame.component[i].V;
        }

        pDecData->frame.component[i].Tq = JpegDecGetByte(&(pDecData->stream));
    }

    if(pDecData->frame.Nf == 1)
    {
        Hmax = Vmax = 1;
        pDecData->frame.component[0].H = 1;
        pDecData->frame.component[0].V = 1;
    }
    else if(Hmax == 0 || Vmax == 0)
    {
        JPEGDEC_TRACE_INTERNAL(("Hmax == 0 || Vmax == 0 \n"));
        return (JPEGDEC_UNSUPPORTED);
    }

    /* JPEG_YCBCR411 horizontal size has to be multiple of 32 pels */
    if(Hmax == 4 && (pDecData->frame.hwX & 0x1F))
    {
        /* round up to next multiple-of-32 */
        pDecData->frame.hwX += 16;
        pDecData->info.X = pDecData->frame.hwX;

        /* check for minimum and maximum dimensions */
        if(pDecData->frame.hwX > pDecData->maxSupportedWidth ||
           (pDecData->frame.hwX * pDecData->frame.hwY) >
           pDecData->maxSupportedPixelAmount)
        {
            JPEGDEC_TRACE_INTERNAL(("FRAME: Unsupported size\n"));
            return (JPEGDEC_UNSUPPORTED);
        }
    }

    /* set image pointers, calculate pixelPerRow for each component */
    width = ((pDecData->frame.hwX + Hmax * 8 - 1) / (Hmax * 8)) * Hmax * 8;
    height = ((pDecData->frame.hwY + Vmax * 8 - 1) / (Vmax * 8)) * Vmax * 8;

    /* calculate numMcuInRow and numMcuInFrame */
    ASSERT(Hmax != 0);
    ASSERT(Vmax != 0);
    pDecData->frame.numMcuInRow = width / (8 * Hmax);
    pDecData->frame.numMcuInFrame = pDecData->frame.numMcuInRow *
        (height / (8 * Vmax));

    /* reset mcuNumbers */
    pDecData->frame.mcuNumber = 0;
    pDecData->frame.row = pDecData->frame.col = 0;

    for(i = 0; i < pDecData->frame.Nf; i++)
    {

        ASSERT(i <= 2);

        tmp1 = (width * pDecData->frame.component[i].H + Hmax - 1) / Hmax;
        tmp2 = (height * pDecData->frame.component[i].V + Vmax - 1) / Vmax;
        size += tmp1 * tmp2;
        /* pixels per row */

        pDecData->image.pixelsPerRow[i] = tmp1;
        pDecData->image.columns[i] = tmp2;
        pDecData->frame.numBlocks[i] =
            (((pDecData->frame.hwX * pDecData->frame.component[i].H) / Hmax +
              7) >> 3) * (((pDecData->frame.hwY *
                            pDecData->frame.component[i].V) / Vmax + 7) >> 3);

        if(i == 0)
        {
            pDecData->image.sizeLuma = size;
        }
    }

    pDecData->image.size = size;
    pDecData->image.sizeChroma = size - pDecData->image.sizeLuma;

    /* set YUV mode & calculate rlc tmp size */
    retCode = JpegDecMode(pDecData);
    if(retCode != JPEGDEC_OK)
    {
        return (retCode);
    }

    return (JPEGDEC_OK);
}

/*------------------------------------------------------------------------------

        Function name: JpegDecDecodeQuantTables

        Functional description:
          Decodes quantisation tables from the stream

        Inputs:
          JpegDecContainer *pDecData      Pointer to JpegDecContainer structure

        Outputs:
          OK    (0)
          NOK   (-1)

------------------------------------------------------------------------------*/

JpegDecRet JpegDecDecodeQuantTables(JpegDecContainer * pDecData)
{
    RK_U32 t, tmp, i;
    StreamStorage *pStream = &(pDecData->stream);

    pDecData->quant.Lq = JpegDecGet2Bytes(pStream);

    /* check if there is enough data for tables */
    if(((pStream->readBits / 8) + pDecData->quant.Lq) > pStream->streamLength)
        return (JPEGDEC_STRM_ERROR);

    t = 4;

    while(t < pDecData->quant.Lq)
    {
        /* Tq value selects what table the components use */

        /* read tables and write to decData->quant */
        tmp = JpegDecGetByte(pStream);
        t++;
        /* supporting only 8 bits / sample */
        if((tmp >> 4) != 0)
        {
            return (JPEGDEC_UNSUPPORTED);
        }
        tmp &= 0xF;
        /* set the quantisation table pointer */

        if(tmp == 0)
        {
            JPEGDEC_TRACE_INTERNAL(("qtable0\n"));
            pDecData->quant.table = pDecData->quant.table0;
        }
        else if(tmp == 1)
        {
            JPEGDEC_TRACE_INTERNAL(("qtable1\n"));
            pDecData->quant.table = pDecData->quant.table1;
        }
        else if(tmp == 2)
        {
            JPEGDEC_TRACE_INTERNAL(("qtable2\n"));
            pDecData->quant.table = pDecData->quant.table2;
        }
        else if(tmp == 3)
        {
            JPEGDEC_TRACE_INTERNAL(("qtable3\n"));
            pDecData->quant.table = pDecData->quant.table3;
        }
        else
        {
            return (JPEGDEC_UNSUPPORTED);
        }
        for(i = 0; i < 64; i++)
        {
            pDecData->quant.table[i] = JpegDecGetByte(pStream);
            t++;
        }

    }

    return (JPEGDEC_OK);
}

/*------------------------------------------------------------------------------

        Function name: JpegDecDecodeHuffmanTables

        Functional description:
          Decodes huffman tables from the stream

        Inputs:
          DecData *JpegDecContainer      Pointer to JpegDecContainer structure

        Outputs:
          OK    (0)
          NOK   (-1)

------------------------------------------------------------------------------*/
JpegDecRet JpegDecDecodeHuffmanTables(JpegDecContainer * pDecData)
{
    RK_U32 i, len, Tc, Th, tmp;
    RK_S32 j;
    StreamStorage *pStream = &(pDecData->stream);

    pDecData->vlc.Lh = JpegDecGet2Bytes(pStream);

    /* check if there is enough data for tables */
    if(((pStream->readBits / 8) + pDecData->vlc.Lh) > pStream->streamLength)
        return (JPEGDEC_STRM_ERROR);

    /* four bytes already read in */
    len = 4;

    while(len < pDecData->vlc.Lh)
    {
        tmp = JpegDecGetByte(pStream);
        len++;
        Tc = tmp >> 4;  /* Table class */
        if(Tc != 0 && Tc != 1)
        {
            return (JPEGDEC_UNSUPPORTED);
        }
        Th = tmp & 0xF; /* Huffman table identifier */
        /* only two tables in baseline allowed */
        if((pDecData->frame.codingType == SOF0) && (Th > 1))
        {
            return (JPEGDEC_UNSUPPORTED);
        }
        /* four tables in progressive allowed */
        if((pDecData->frame.codingType == SOF2) && (Th > 3))
        {
            return (JPEGDEC_UNSUPPORTED);
        }
        /* set the table pointer */
        if(Tc)
        {
            /* Ac table */
            switch (Th)
            {
            case 0:
                JPEGDEC_TRACE_INTERNAL(("ac0\n"));
                pDecData->vlc.table = &(pDecData->vlc.acTable0);
                break;
            case 1:
                JPEGDEC_TRACE_INTERNAL(("ac1\n"));
                pDecData->vlc.table = &(pDecData->vlc.acTable1);
                break;
            case 2:
                JPEGDEC_TRACE_INTERNAL(("ac2\n"));
                pDecData->vlc.table = &(pDecData->vlc.acTable2);
                break;
            case 3:
                JPEGDEC_TRACE_INTERNAL(("ac3\n"));
                pDecData->vlc.table = &(pDecData->vlc.acTable3);
                break;
            default:
                return (JPEGDEC_UNSUPPORTED);
            }
        }
        else
        {
            /* Dc table */
            switch (Th)
            {
            case 0:
                JPEGDEC_TRACE_INTERNAL(("dc0\n"));
                pDecData->vlc.table = &(pDecData->vlc.dcTable0);
                break;
            case 1:
                JPEGDEC_TRACE_INTERNAL(("dc1\n"));
                pDecData->vlc.table = &(pDecData->vlc.dcTable1);
                break;
            case 2:
                JPEGDEC_TRACE_INTERNAL(("dc2\n"));
                pDecData->vlc.table = &(pDecData->vlc.dcTable2);
                break;
            case 3:
                JPEGDEC_TRACE_INTERNAL(("dc3\n"));
                pDecData->vlc.table = &(pDecData->vlc.dcTable3);
                break;
            default:
                return (JPEGDEC_UNSUPPORTED);
            }
        }

        tmp = 0;
        /* read in the values of list BITS */
        for(i = 0; i < 16; i++)
        {
            tmp += pDecData->vlc.table->bits[i] = JpegDecGetByte(pStream);
            len++;
        }
        /* allocate memory for HUFFVALs */
        if(pDecData->vlc.table->vals != NULL)
        {
            /* free previously reserved table */
            DWLfree(pDecData->vlc.table->vals);
        }

        pDecData->vlc.table->vals = (RK_U32 *) DWLmalloc(sizeof(RK_U32) * tmp);

        /* set the table length */
        pDecData->vlc.table->tableLength = tmp;
        /* read in the HUFFVALs */
        for(i = 0; i < tmp; i++)
        {
            pDecData->vlc.table->vals[i] = JpegDecGetByte(pStream);
            len++;
        }
        /* first and last lengths */
        for(i = 0; i < 16; i++)
        {
            if(pDecData->vlc.table->bits[i] != 0)
            {
                pDecData->vlc.table->start = i;
                break;
            }
        }
        for(j = 15; j >= 0; j--)
        {
            if(pDecData->vlc.table->bits[j] != 0)
            {
                pDecData->vlc.table->last = ((RK_U32) j + 1);
                break;
            }
        }
    }

    return (JPEGDEC_OK);
}

/*------------------------------------------------------------------------------

        Function name: JpegDecMode

        Functional description:
          check YCBCR mode

        Inputs:
          JpegDecContainer *pDecData      Pointer to JpegDecContainer structure

        Outputs:
          OK/NOK

------------------------------------------------------------------------------*/
JpegDecRet JpegDecMode(JpegDecContainer * pDecData)
{
    /*  check input format */
    if(pDecData->frame.Nf == 3)
    {
        /* JPEG_YCBCR420 */
        if(pDecData->frame.component[0].H == 2 &&
           pDecData->frame.component[0].V == 2 &&
           pDecData->frame.component[1].H == 1 &&
           pDecData->frame.component[1].V == 1 &&
           pDecData->frame.component[2].H == 1 &&
           pDecData->frame.component[2].V == 1)
        {
            pDecData->info.yCbCrMode = JPEGDEC_YUV420;
            pDecData->info.X = pDecData->frame.hwX;
            pDecData->info.Y = pDecData->frame.hwY;

            /* calculate new output size if slice mode used */
            if(pDecData->info.sliceMbSetValue)
            {
                /* Y */
                pDecData->image.sizeLuma = (pDecData->info.X *
                                            (pDecData->info.sliceMbSetValue *
                                             16));

                /* CbCr */
                pDecData->image.sizeChroma = pDecData->image.sizeLuma / 2;

            }
        }
        /* JPEG_YCBCR422 */
        else if(pDecData->frame.component[0].H == 2 &&
                pDecData->frame.component[0].V == 1 &&
                pDecData->frame.component[1].H == 1 &&
                pDecData->frame.component[1].V == 1 &&
                pDecData->frame.component[2].H == 1 &&
                pDecData->frame.component[2].V == 1)
        {
            pDecData->info.yCbCrMode = JPEGDEC_YUV422;
            pDecData->info.X = (pDecData->frame.hwX);
            pDecData->info.Y = (pDecData->frame.hwY);

            /* check if fill needed */
            if((pDecData->frame.Y & 0xF) && (pDecData->frame.Y & 0xF) <= 8)
                pDecData->info.fillBottom = 1;

            /* calculate new output size if slice mode used */
            if(pDecData->info.sliceMbSetValue)
            {
                /* Y */
                pDecData->image.sizeLuma = (pDecData->info.X *
                                            (pDecData->info.sliceMbSetValue *
                                             16));

                /* CbCr */
                pDecData->image.sizeChroma = pDecData->image.sizeLuma;
            }
        }
        /* JPEG_YCBCR440 */
        else if(pDecData->frame.component[0].H == 1 &&
                pDecData->frame.component[0].V == 2 &&
                pDecData->frame.component[1].H == 1 &&
                pDecData->frame.component[1].V == 1 &&
                pDecData->frame.component[2].H == 1 &&
                pDecData->frame.component[2].V == 1)
        {
            pDecData->info.yCbCrMode = JPEGDEC_YUV440;
            pDecData->info.X = (pDecData->frame.hwX);
            pDecData->info.Y = (pDecData->frame.hwY);

            /* check if fill needed */
            if((pDecData->frame.X & 0xF) && (pDecData->frame.X & 0xF) <= 8)
                pDecData->info.fillRight = 1;

            /* calculate new output size if slice mode used */
            if(pDecData->info.sliceMbSetValue)
            {
                /* Y */
                pDecData->image.sizeLuma = (pDecData->info.X *
                                            (pDecData->info.sliceMbSetValue *
                                             16));

                /* CbCr */
                pDecData->image.sizeChroma = pDecData->image.sizeLuma;
            }
        }
        /* JPEG_YCBCR444 : NOT SUPPORTED */
        else if(pDecData->frame.component[0].H == 1 &&
                pDecData->frame.component[0].V == 1 &&
                pDecData->frame.component[1].H == 1 &&
                pDecData->frame.component[1].V == 1 &&
                pDecData->frame.component[2].H == 1 &&
                pDecData->frame.component[2].V == 1)
        {
            pDecData->info.yCbCrMode = JPEGDEC_YUV444;
            pDecData->info.X = pDecData->frame.hwX;
            pDecData->info.Y = pDecData->frame.hwY;

            /* check if fill needed */
            if((pDecData->frame.X & 0xF) && (pDecData->frame.X & 0xF) <= 8)
                pDecData->info.fillRight = 1;

            if((pDecData->frame.Y & 0xF) && (pDecData->frame.Y & 0xF) <= 8)
                pDecData->info.fillBottom = 1;

            /* calculate new output size if slice mode used */
            if(pDecData->info.sliceMbSetValue)
            {
                /* Y */
                pDecData->image.sizeLuma = (pDecData->info.X *
                                            (pDecData->info.sliceMbSetValue *
                                             16));

                /* CbCr */
                pDecData->image.sizeChroma = pDecData->image.sizeLuma * 2;
            }
        }
        /* JPEG_YCBCR411 */
        else if(pDecData->frame.component[0].H == 4 &&
                pDecData->frame.component[0].V == 1 &&
                pDecData->frame.component[1].H == 1 &&
                pDecData->frame.component[1].V == 1 &&
                pDecData->frame.component[2].H == 1 &&
                pDecData->frame.component[2].V == 1)
        {
            pDecData->info.yCbCrMode = JPEGDEC_YUV411;
            pDecData->info.X = (pDecData->frame.hwX);
            pDecData->info.Y = (pDecData->frame.hwY);

            /* check if fill needed */
            if((pDecData->frame.Y & 0xF) && (pDecData->frame.Y & 0xF) <= 8)
                pDecData->info.fillBottom = 1;

            /* calculate new output size if slice mode used */
            if(pDecData->info.sliceMbSetValue)
            {
                /* Y */
                pDecData->image.sizeLuma = (pDecData->info.X *
                                            (pDecData->info.sliceMbSetValue *
                                             16));

                /* CbCr */
                pDecData->image.sizeChroma = pDecData->image.sizeLuma / 2;
            }
        }
        else
        {
            return (JPEGDEC_UNSUPPORTED);
        }
    }
    else if(pDecData->frame.Nf == 1)
    {
        /* 4:0:0 */
        if((pDecData->frame.component[0].V == 1) ||
           (pDecData->frame.component[0].H == 1))
        {
            pDecData->info.yCbCrMode = JPEGDEC_YUV400;
            pDecData->info.X = (pDecData->frame.hwX);
            pDecData->info.Y = (pDecData->frame.hwY);

            /* check if fill needed */
            if((pDecData->frame.X & 0xF) && (pDecData->frame.X & 0xF) <= 8)
                pDecData->info.fillRight = 1;

            if((pDecData->frame.Y & 0xF) && (pDecData->frame.Y & 0xF) <= 8)
                pDecData->info.fillBottom = 1;

            /* calculate new output size if slice mode used */
            if(pDecData->info.sliceMbSetValue)
            {
                /* Y */
                pDecData->image.sizeLuma =
                    ((((pDecData->info.X +
                        15) / 16) * 16) * (pDecData->info.sliceMbSetValue *
                                           16));

                /* CbCr */
                pDecData->image.sizeChroma = 0;
            }
        }
        else
        {
            return (JPEGDEC_UNSUPPORTED);
        }
    }
    else
    {
        return (JPEGDEC_UNSUPPORTED);
    }

#ifdef JPEGDEC_ERROR_RESILIENCE
    if(pDecData->info.fillBottom)
    {
        pDecData->info.Y -= 16;
        pDecData->frame.hwY -= 16;
    }
#endif /* JPEGDEC_ERROR_RESILIENCE */

    /* save the original sampling format for progressive use */
    pDecData->info.yCbCrModeOrig = pDecData->info.yCbCrMode;

    return (JPEGDEC_OK);
}
