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
-
-  Description : Video stabilization algorithms. Quarter pixel ME, Motion filter 
-
--------------------------------------------------------------------------------
-
-  Version control information, please leave untouched.
-
-  $RCSfile: vidstabalg.c,v $
-  $Revision: 1.1 $
-  $Date: 2007/07/16 09:28:37 $
-
------------------------------------------------------------------------------*/
#include "basetype.h"
#include "vidstabcommon.h"

#ifdef TEST_DATA
#include <stdio.h>
#endif

#define ABS(x)          ((x) < (0) ? -(x) : (x))
#define MAX(a, b)       ((a) > (b) ?  (a) : (b))
#define MIN(a, b)       ((a) < (b) ?  (a) : (b))

static void QuarterPixelMotion(SwStbData * data, i32 minX, i32 minY);
static void InterpolateMotion(i32(*data)[5], i32 * half_hor, i32 * half_ver);
static void FilterMotion(SwStbData * data, i32 * minX, i32 * minY);

#ifdef TEST_DATA
/*------------------------------------------------------------------------------
    Function name   : PrintTraceFile
    Description     :
    Return type     : void
    Argument        : x
    Argument        : y
------------------------------------------------------------------------------*/
void PrintTraceFile(i32 x, i32 y)
{
    static FILE *fTrc = NULL;

    if (fTrc == NULL)
    {
        fTrc = fopen("video_stab_result.trc", "w");
        if (fTrc)
            fprintf(fTrc, "offX, offY\n");
    }

    if (fTrc)
        fprintf(fTrc, "%4d, %4d\n", x, y);

}
#endif

/*------------------------------------------------------------------------------
    Function name   : FilterMotion
    Description     : 
    Return type     : void 
    Argument        : SwStbData * data
    Argument        : i32 * minX
    Argument        : i32 * minY
------------------------------------------------------------------------------*/
void FilterMotion(SwStbData * data, i32 * minX, i32 * minY)
{
    i32 meanX, meanY;
    i32 edgeX, edgeY;

    edgeX = (data->inputWidth - data->stabilizedWidth) / 2;
    edgeY = (data->inputHeight - data->stabilizedHeight) / 2;

    data->filterX += *minX;
    data->filterY += *minY;

    data->filterX += data->filterErrorX;
    data->filterY += data->filterErrorY;

    data->filterX -= data->filterErrorXp;
    data->filterY -= data->filterErrorYp;

    meanX = data->filterX / data->filterLengthX;
    meanY = data->filterY / data->filterLengthY;

    data->filterErrorXp = data->filterErrorX;
    data->filterErrorYp = data->filterErrorY;

    data->filterErrorX = data->filterX - meanX * data->filterLengthX;
    data->filterErrorY = data->filterY - meanY * data->filterLengthY;

    data->filterX -= meanX;
    data->filterY -= meanY;

    data->filterSumStabX += meanX;
    data->filterSumStabY += meanY;

    data->filterSumMotX += *minX;
    data->filterSumMotY += *minY;

    /* Compensate motion with filter */
    *minX -= meanX;
    *minY -= meanY;

    /* Adjust the filter length */
    if((ABS(data->filterSumMotX - data->filterSumStabX) >= 3 * edgeX / 4) &&
       data->filterLengthX > 4)
        data->filterLengthX -= 2;

    if((ABS(data->filterSumMotX - data->filterSumStabX) <= edgeX / 4) &&
       data->filterLengthX < 40)
        data->filterLengthX += 2;

    if((ABS(data->filterSumMotY - data->filterSumStabY) >= 3 * edgeY / 4) &&
       data->filterLengthY > 4)
        data->filterLengthY -= 2;

    if((ABS(data->filterSumMotY - data->filterSumStabY) <= edgeY / 4) &&
       data->filterLengthY < 40)
        data->filterLengthY += 2;

}

/*------------------------------------------------------------------------------
    Function name   : InterpolateMotion
    Description     : 
    Return type     : void 
    Argument        : i32 ** data
    Argument        : i32 * hor
    Argument        : i32 * ver
------------------------------------------------------------------------------*/
void InterpolateMotion(i32(*data)[5], i32 * hor, i32 * ver)
{
    i32 i, j, tmp;
    i32 min = 0;

    for(i = 0; i < 5; i += 2)
    {
        for(j = 1; j < 5; j += 2)
        {
            data[i][j] = (data[i][j - 1] + data[i][j + 1] + 1) / 2;
        }
    }

    for(i = 1; i < 5; i += 2)
    {
        for(j = 0; j < 5; j++)
        {
            data[i][j] = (data[i - 1][j] + data[i + 1][j] + 1) / 2;
        }
    }

    /* Find minimum */
    for(i = 1; i < 4; i++)
    {
        for(j = 1; j < 4; j++)
        {
            tmp = data[i - 1][j - 1]
                + data[i - 1][j]
                + data[i - 1][j + 1]
                + data[i][j - 1]
                + data[i][j]
                + data[i][j + 1]
                + data[i + 1][j - 1] + data[i + 1][j] + data[i + 1][j + 1];

            if((i == 1) && (j == 1))
            {
                min = tmp;
                *ver = i - 2;
                *hor = j - 2;
            }

            if(tmp < min)
            {
                min = tmp;
                *ver = i - 2;
                *hor = j - 2;
            }
        }
    }
}

/*------------------------------------------------------------------------------
    Function name   : QuarterPixelMotion
    Description     : 
    Return type     : void 
    Argument        : SwStbData * data
    Argument        : i32 minX
    Argument        : i32 minY
------------------------------------------------------------------------------*/
void QuarterPixelMotion(SwStbData * data, i32 minX, i32 minY)
{
    const u32 *motion = data->motion;

    i32(*half)[5] = data->half;
    i32(*quarter)[5] = data->quarter;
    i32 ver = 0;
    i32 hor = 0;
    i32 i, j;

    /* 1/4 pixel is not calculated for +-16 pixel GMV */
    if((ABS(minX - 16) < 16) && (ABS(minY - 16) < 16))
    {
        /* Get full pixel samples around minimum */
        for(i = 0; i < 3; i++)
        {
            for(j = 0; j < 3; j++)
            {
                half[2 * i][2 * j] = motion[i * 3 + j];
            }
        }

        /* Interpolate to half pixel */
        InterpolateMotion(half, &hor, &ver);

        data->qpMotionX += 2 * hor;
        data->qpMotionY += 2 * ver;

        /* Get half pixel samples around minimum */
        for(i = 0; i < 3; i++)
        {
            for(j = 0; j < 3; j++)
            {
                quarter[2 * i][2 * j] = half[1 + ver + i][1 + hor + j];
            }
        }

        /* Interpolate to quarter pixel */
        InterpolateMotion(quarter, &hor, &ver);

        data->qpMotionX += hor;
        data->qpMotionY += ver;
    }

}

/*------------------------------------------------------------------------------
    Function name   : VSAlgStabilize
    Description     : 
    Return type     : u32 
    Argument        : SwStbData * data
    Argument        : const HWStabData * hwStabData
------------------------------------------------------------------------------*/
u32 VSAlgStabilize(SwStbData * data, const HWStabData * hwStabData)
{
    i32 minX, minY;
    i32 edgeX, edgeY;
    u32 min, mean;

    /* inputImage = whole frame from the camera 
     * stabilizedImage = area to be encoded */

    /* Quarter pixel motion in [-2, 2] */
    if((data->qpMotionX < -2) || (data->qpMotionX > 2))
        data->qpMotionX = 0;

    if((data->qpMotionY < -2) || (data->qpMotionY > 2))
        data->qpMotionY = 0;

    edgeX = (data->inputWidth - data->stabilizedWidth) / 2;
    edgeY = (data->inputHeight - data->stabilizedHeight) / 2;

    mean = hwStabData->rMotionSum / 1089;
    min = hwStabData->rMotionMin;
    minX = hwStabData->rGmvX + 16;  /* alg is using [0, 32] range */
    minY = hwStabData->rGmvY + 16;  /* alg is using [0, 32] range */

    data->motion = hwStabData->rMatrixVal;

    /* scene change detection */
    if((min > (3*data->prevMin/2)) && (mean > (3*data->prevMean/2)))
    {
        /* Scene change works like a start of sequence
         * The internal variables will be zeroed for the next frame */

        data->prevMin = min;
        data->prevMean = mean;
        data->sceneChange = 1; /* signal scene change */

#ifdef TRACE_VIDEOSTAB_INTERNAL
        DEBUG_PRINT(("VS: Scene change detected\n"));
#endif
#ifdef TEST_DATA
        PrintTraceFile((data->inputWidth - data->stabilizedWidth) / 2,
                       (data->inputHeight - data->stabilizedHeight) / 2);
#endif
        return 1; /* reset stab also */
    }
    else
    {
        data->sceneChange = 0; /* no scene change */
        data->prevMin = min;
        data->prevMean = mean;
    }

    /* Threshold for not finding significant overall motion */
    if((mean == 0) || ((85 * mean / 100) < min))
    {
        /* The internal variables will be zeroed for the next frame */

#ifdef TRACE_VIDEOSTAB_INTERNAL
        DEBUG_PRINT(("VS: No motion detected\n"));
#endif
#ifdef TEST_DATA
        PrintTraceFile((data->inputWidth - data->stabilizedWidth) / 2,
                       (data->inputHeight - data->stabilizedHeight) / 2);
#endif
        return 1;
    }

    /* Half-pixel and quarter-pixel motion */
    QuarterPixelMotion(data, minX, minY);

    /* Add half pixel motion */
    if(data->qpMotionX > 2)
    {
        minX += 1;
        data->qpMotionX -= 4;
    }
    if(data->qpMotionX < -2)
    {
        minX -= 1;
        data->qpMotionX += 4;
    }
    if(data->qpMotionY > 2)
    {
        minY += 1;
        data->qpMotionY -= 4;
    }
    if(data->qpMotionY < -2)
    {
        minY -= 1;
        data->qpMotionY += 4;
    }

    /* Compensated motion */
    minX = 16 - minX;
    minY = 16 - minY;

    FilterMotion(data, &minX, &minY);

    /* Calculate the new stabilized picture offset */
    data->filterPosX = data->filterPosX + minX;
    data->filterPosY = data->filterPosY + minY;

    /* Saturate the stabilized window into input picture */
    data->stabOffsetX = MIN(MAX(data->filterPosX, 0), 2 * edgeX);
    data->stabOffsetY = MIN(MAX(data->filterPosY, 0), 2 * edgeY);

#ifdef TRACE_VIDEOSTAB_INTERNAL
    DEBUG_PRINT(("VS: New Result %d, %d\n", data->stabOffsetX,
                 data->stabOffsetY));
#endif
#ifdef TEST_DATA
    PrintTraceFile(data->stabOffsetX, data->stabOffsetY);
#endif

    return 0;
}

/*------------------------------------------------------------------------------
    Function name   : VSAlgInit
    Description     : 
    Return type     : void 
    Argument        : SwStbData * data
    Argument        : u32 srcWidth
    Argument        : u32 srcHeight
    Argument        : u32 width
    Argument        : u32 height
------------------------------------------------------------------------------*/
void VSAlgInit(SwStbData * data, u32 srcWidth, u32 srcHeight, u32 width,
               u32 height)
{
    data->inputWidth = srcWidth;
    data->inputHeight = srcHeight;

    data->stabilizedWidth = width;
    data->stabilizedHeight = height;

    data->prevMean = ~0;
    data->prevMin = ~0;
    data->sceneChange = 0;

    VSAlgReset(data);
}

/*------------------------------------------------------------------------------
    Function name   : VSAlgGetResult
    Description     : 
    Return type     : void 
    Argument        : const SwStbData * data
    Argument        : u32 * xOff
    Argument        : u32 * yOff
------------------------------------------------------------------------------*/
void VSAlgGetResult(const SwStbData * data, u32 * xOff, u32 * yOff)
{
    *xOff = data->stabOffsetX;
    *yOff = data->stabOffsetY;

#ifdef TRACE_VIDEOSTAB_INTERNAL
    DEBUG_PRINT(("VS: Get Result %d, %d\n", data->stabOffsetX,
                 data->stabOffsetY));
#endif

}

/*------------------------------------------------------------------------------
    Function name   : VSAlgReset
    Description     : 
    Return type     : void 
    Argument        : SwStbData * data
------------------------------------------------------------------------------*/
void VSAlgReset(SwStbData * data)
{
    i32 edgeX, edgeY;

    edgeX = (data->inputWidth - data->stabilizedWidth) / 2;
    edgeY = (data->inputHeight - data->stabilizedHeight) / 2;

    data->qpMotionX = 0;
    data->qpMotionY = 0;
    data->stabOffsetX = edgeX;
    data->stabOffsetY = edgeY;
    data->filterPosX = edgeX;
    data->filterPosY = edgeY;
    data->filterX = 0;
    data->filterY = 0;
    data->filterErrorX = 0;
    data->filterErrorY = 0;
    data->filterErrorXp = 0;
    data->filterErrorYp = 0;
    data->filterSumMotX = 0;
    data->filterSumMotY = 0;
    data->filterSumStabX = 0;
    data->filterSumStabY = 0;

    data->filterLengthX = 16;
    data->filterLengthY = 16;
}


