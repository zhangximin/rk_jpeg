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
--  Description : Reference buffer control functions.
--
------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: refbuffer.c,v $
--  $Revision: 1.43 $
--  $Date: 2010/04/15 12:15:24 $
--
------------------------------------------------------------------------------*/

#include "vpu_type.h"
#include "regdrv.h"
#include "deccfg.h"
#include "refbuffer.h"

#include <stdio.h>

#define DEC_X170_MODE_H264  (0)
#define DEC_X170_MODE_MPEG4 (1)
#define DEC_X170_MODE_H263  (2)
#define DEC_X170_MODE_VC1   (4)
#define DEC_X170_MODE_MPEG2 (5)
#define DEC_X170_MODE_MPEG1 (6)
#define DEC_X170_MODE_VP6   (7)
#define DEC_X170_MODE_RV    (8)
#define DEC_X170_MODE_VP7   (9)
#define DEC_X170_MODE_VP8   (10)
#define DEC_X170_MODE_AVS   (11)

#define THR_ADJ_MAX (8)
#define THR_ADJ_MIN (1)

#define MAX_DIRECT_MVS      (3000)

#ifndef DEC_X170_REFBU_ADJUST_VALUE
#define DEC_X170_REFBU_ADJUST_VALUE 130
#endif

/* Read direct MV from output memory; indexes i0..i2 used to 
 * handle both big- and little-endian modes. */
#define DIR_MV_VER(p,i0,i1,i2) \
    ((((RK_U32)(p[(i0)])) << 3) | (((RK_U32)(p[(i1)])) >> 5) | (((RK_U32)(p[(i2)] & 0x3) ) << 11 ))
#define DIR_MV_BE_VER(p) \
    ((((RK_U32)(p[1])) << 3) | (((RK_U32)(p[0])) >> 5) | (((RK_U32)(p[2] & 0x3) ) << 11 ))
#define DIR_MV_LE_VER(p) \
    ((((RK_U32)(p[2])) << 3) | (((RK_U32)(p[3])) >> 5) | (((RK_U32)(p[1] & 0x3) ) << 11 ))
#define SIGN_EXTEND(value, bits) (value) = (((value)<<(32-bits))>>(32-bits))

/* Distribution ranges and zero point */
#define VER_DISTR_MIN           (-256)
#define VER_DISTR_MAX           (255)
#define VER_DISTR_RANGE         (512)
#define VER_DISTR_ZERO_POINT    (256)
#define VER_MV_RANGE            (16)
#define HOR_CALC_WIDTH          (32)

/* macro to get absolute value */
#define ABS(a) (((a) < 0) ? -(a) : (a))

static const memAccess_t memStatsPerFormat[] = {
    { 307, 36, 150 }, /* H.264 (upped 20%) */
    { 236, 29, 112 }, /* MPEG-4 */
    { 228, 25, 92  }, /* H.263 (based on MPEG-2) */
    {   0,  0,   0 }, /* JPEG */
    { 240, 29, 112 }, /* VC-1 */
    { 302, 25, 92 },  /* MPEG-2 */
    { 228, 25, 92  }, /* MPEG-1 */
    { 240, 29, 112 }, /* AVS (based on VC-1) */
    { 240, 29, 112 }, /* VP6 (based on VC-1) */
    { 240, 29, 112 }, /* RVx (based on VC-1) */
    { 240, 29, 112 }, /* VP7 (based on VC-1) */
    { 240, 29, 112 }, /* VP8 (based on VC-1) */
    { 240, 29, 112 }  /* AVS (based on VC-1) */
};

static const RK_S32 mbDataPerFormat[][2] = {
    { 734, 880 },   /* H.264 (upped 20%) */
    { 464, 535 },   /* MPEG-4 */
    { 435, 486 },   /* H.263 (same as MPEG-2 used) */
    { 0, 0 },       /* JPEG   */
    { 533, 644 },   /* VC-1   */
    { 435, 486 },   /* MPEG-2 */
    { 435, 486 },   /* MPEG-1 */
    { 533, 486 },   /* AVS (based now on VC-1) */
    { 533, 486 },   /* VP6 (based now on VC-1) */
    { 533, 486 },   /* RVx (based on VC-1) */
    { 533, 486 },   /* VP7 (based now on VC-1) */
    { 533, 486 },   /* VP8 (based now on VC-1) */
    { 533, 486 }    /* AVS (based now on VC-1) */
};


static RK_U32 GetSettings( refBuffer_t *pRefbu, RK_S32 *pX, RK_S32 *pY, RK_U32 isBpic,
                        RK_U32 isFieldPic );
static void IntraFrame( refBuffer_t *pRefbu );

/*------------------------------------------------------------------------------
    Function name : UpdateMemModel
    Description   : Update memory model for reference buffer

    Return type   : 
    Argument      : 
------------------------------------------------------------------------------*/
void UpdateMemModel( refBuffer_t *pRefbu )
{

/* Variables */

    RK_S32 nonSeqPerFr;
    RK_S32 seqPerFr;
    RK_S32 widthInMbs;
    RK_S32 heightInMbs;
    RK_S32 latency;
    RK_S32 nonseq;
    RK_S32 seq;

    RK_S32 busWidth;
    RK_S32 tmp, tmp2;

/* Code */

#define DIV_CEIL(a,b) (((a)+(b)-1)/(b))

    widthInMbs  = pRefbu->picWidthInMbs;
    heightInMbs = pRefbu->picHeightInMbs;
    busWidth    = pRefbu->busWidthInBits;

    tmp = busWidth >> 2;    /* initially buffered mbs per row */
    tmp2 = busWidth >> 3;   /* n:o of mbs buffered at refresh */
    tmp = ( 1 + DIV_CEIL(widthInMbs - tmp, tmp2 ) ); /* latencies per mb row */
    tmp2 = 24 * heightInMbs; /* Number of rows to buffer in total */
    /* Latencies per frame */
    latency = 2 * tmp * heightInMbs;
    nonseq  = tmp2 * tmp;
    seq = ( DIV_CEIL( 16*widthInMbs, busWidth>>3 ) ) * tmp2 -
        nonseq;

    pRefbu->numCyclesForBuffering =
        latency * pRefbu->currMemModel.latency + 
        nonseq * ( 1 + pRefbu->currMemModel.nonseq ) +
        seq * ( 1 + pRefbu->currMemModel.seq );

    pRefbu->bufferPenalty = 
        pRefbu->memAccessStats.nonseq +
        pRefbu->memAccessStats.seq;
    if( busWidth == 32 )
        pRefbu->bufferPenalty >>= 1;

    pRefbu->avgCyclesPerMb =
        ( ( pRefbu->memAccessStats.latency * pRefbu->currMemModel.latency ) / 100 ) +
        pRefbu->memAccessStats.nonseq * ( 1 + pRefbu->currMemModel.nonseq ) +
        pRefbu->memAccessStats.seq * ( 1 + pRefbu->currMemModel.seq );

#ifdef REFBUFFER_TRACE
    printf("***** ref buffer mem model trace *****\n"); 
    printf("latency             = %7d clk\n", pRefbu->currMemModel.latency );
    printf("sequential          = %7d clk\n", pRefbu->currMemModel.nonseq );
    printf("non-sequential      = %7d clk\n", pRefbu->currMemModel.seq );

    printf("latency (mb)        = %7d\n", pRefbu->memAccessStats.latency );
    printf("sequential (mb)     = %7d\n", pRefbu->memAccessStats.nonseq );
    printf("non-sequential (mb) = %7d\n", pRefbu->memAccessStats.seq );

    printf("bus-width           = %7d\n", busWidth );

    printf("buffering cycles    = %7d\n", pRefbu->numCyclesForBuffering );
    printf("buffer penalty      = %7d\n", pRefbu->bufferPenalty );
    printf("avg cycles per mb   = %7d\n", pRefbu->avgCyclesPerMb );

    printf("***** ref buffer mem model trace *****\n");
#endif /* REFBUFFER_TRACE */

#undef DIV_CEIL

}

/*------------------------------------------------------------------------------
    Function name : IntraFrame
    Description   : Clear history buffers on intra frames

    Return type   : 
    Argument      : 
------------------------------------------------------------------------------*/
void IntraFrame( refBuffer_t *pRefbu )
{
    pRefbu->oy[0] = pRefbu->oy[1] = pRefbu->oy[2] = 0;
    pRefbu->numIntraBlk[0] = 
        pRefbu->numIntraBlk[1] = pRefbu->numIntraBlk[2] = (-1);
    pRefbu->coverage[0] = /*4 * tmp;*/
        pRefbu->coverage[1] = 
        pRefbu->coverage[2] = (-1); /* initial value */
}

/*------------------------------------------------------------------------------
    Function name : RefbuGetHitThreshold
    Description   : 

    Return type   : 
    Argument      : 
------------------------------------------------------------------------------*/
RK_S32 RefbuGetHitThreshold( refBuffer_t *pRefbu )
{

    RK_S32 requiredHitsClk = 0;
    RK_S32 requiredHitsData = 0;
    RK_S32 threshold;
    RK_S32 divisor;
    RK_S32 tmp;
    RK_S32 predMiss;

#ifdef REFBUFFER_TRACE
    printf("***** ref buffer threshold trace *****\n");
#endif /* REFBUFFER_TRACE */

    divisor = pRefbu->avgCyclesPerMb - pRefbu->bufferPenalty;

    if( divisor > 0 )
        requiredHitsClk = ( 4 * pRefbu->numCyclesForBuffering ) / divisor;

    divisor = pRefbu->mbWeight;
#ifdef REFBUFFER_TRACE
    printf("mb weight           = %7d\n", divisor );
#endif /* REFBUFFER_TRACE */

    if( divisor > 0 && pRefbu->dataExcessMaxPct >= 0 )
    {
        predMiss = 4 * pRefbu->frmSizeInMbs - pRefbu->predIntraBlk - 
            pRefbu->predCoverage;

        divisor = ( divisor * pRefbu->dataExcessMaxPct ) / 100;

#ifdef REFBUFFER_TRACE
        printf("predicted misses    = %7d\n", predMiss );
        printf("data excess %%       = %7d\n", pRefbu->dataExcessMaxPct );
        printf("divisor             = %7d\n", divisor );
#endif /* REFBUFFER_TRACE */

        /*tmp = (( DATA_EXCESS_MAX_PCT - 100 ) * pRefbu->mbWeight * predMiss ) / 400;*/
        tmp = 0;
        requiredHitsData = ( 4 * pRefbu->totalDataForBuffering - tmp);
        requiredHitsData /= divisor;
    }

    if(pRefbu->picHeightInMbs)
    {
        requiredHitsClk /= pRefbu->picHeightInMbs;
        requiredHitsData /= pRefbu->picHeightInMbs;
    }

#ifdef REFBUFFER_TRACE
    printf("target (clk)        = %7d\n", requiredHitsClk );
    printf("target (data)       = %7d\n", requiredHitsData );
    printf("***** ref buffer threshold trace *****\n");

#endif /* REFBUFFER_TRACE */


    return requiredHitsClk > requiredHitsData ? 
        requiredHitsClk : requiredHitsData;
}

/*------------------------------------------------------------------------------
    Function name : InitMemAccess
    Description   :

    Return type   : 
    Argument      : 
------------------------------------------------------------------------------*/
void InitMemAccess( refBuffer_t *pRefbu, RK_U32 decMode, RK_U32 busWidth )
{
    /* Initialize stream format memory model */
    pRefbu->memAccessStats = memStatsPerFormat[decMode];
    pRefbu->memAccessStatsFlag = 0;
    if( busWidth == 64 )
    {
        pRefbu->memAccessStats.seq >>= 1;
        pRefbu->mbWeight = mbDataPerFormat[decMode][1];
    }
    else
    {
        pRefbu->mbWeight = mbDataPerFormat[decMode][0];
    }
}

/*------------------------------------------------------------------------------
    Function name : RefbuInit
    Description   :

    Return type   : 
    Argument      : 
------------------------------------------------------------------------------*/
void RefbuInit( refBuffer_t *pRefbu, RK_U32 decMode, RK_U32 picWidthInMbs, 
                RK_U32 picHeightInMbs, RK_U32 supportFlags )
{

/* Variables */

    RK_U32 tmp;
    RK_U32 i;

/* Code */

    /* Ignore init's if image size doesn't change. For example H264 SW may
     * call here when moving to RLC mode. */
    if( pRefbu->picWidthInMbs == picWidthInMbs &&
        pRefbu->picHeightInMbs == picHeightInMbs )
    {
        return;
    }

#ifdef REFBUFFER_TRACE
    printf("***** ref buffer initialized to %dx%d *****\n",
        picWidthInMbs, picHeightInMbs);
#endif /* REFBUFFER_TRACE */

    pRefbu->decMode = decMode;
    pRefbu->picWidthInMbs   = picWidthInMbs;
    pRefbu->picHeightInMbs  = picHeightInMbs;
    tmp                     = picWidthInMbs * picHeightInMbs;
    pRefbu->frmSizeInMbs    = tmp;

    pRefbu->totalDataForBuffering
                            = pRefbu->frmSizeInMbs*384;
    tmp                     = picWidthInMbs * ((picHeightInMbs + 1) / 2);
    pRefbu->fldSizeInMbs    = tmp;

    pRefbu->offsetSupport = (supportFlags & REFBU_SUPPORT_OFFSET) ? 1 : 0;
    pRefbu->interlacedSupport = (supportFlags & REFBU_SUPPORT_INTERLACED) ? 1 : 0;
    pRefbu->doubleSupport = (supportFlags & REFBU_SUPPORT_DOUBLE) ? 1 : 0;
    pRefbu->thrAdj = THR_ADJ_MAX;
    pRefbu->dataExcessMaxPct = DEC_X170_REFBU_ADJUST_VALUE;

    pRefbu->predCoverage = pRefbu->predIntraBlk = 0;
    IntraFrame( pRefbu );
    if( decMode == DEC_X170_MODE_H264 )
    {
        pRefbu->mvsPerMb = 16;
        pRefbu->filterSize = 3;
    }
    else if ( decMode == DEC_X170_MODE_VC1 )
    {
        pRefbu->mvsPerMb = 2;
        pRefbu->filterSize = 2;
    }
    else
    {
        pRefbu->mvsPerMb = 1;
        pRefbu->filterSize = 1;
    }

    /* Initialize buffer memory model */
    pRefbu->busWidthInBits          = DEC_X170_REFBU_WIDTH;
    pRefbu->currMemModel.latency    = DEC_X170_REFBU_LATENCY;
    pRefbu->currMemModel.nonseq     = DEC_X170_REFBU_NONSEQ;
    pRefbu->currMemModel.seq        = DEC_X170_REFBU_SEQ;
    pRefbu->prevLatency             = -1;
    pRefbu->numCyclesForBuffering   = 0;

    for ( i = 0 ; i < 3 ; ++i )
    {
        pRefbu->fldHitsP[i][0] =
            pRefbu->fldHitsP[i][1] =
            pRefbu->fldHitsB[i][0] =
            pRefbu->fldHitsB[i][1] = -1;
    }
    pRefbu->fldCnt = 0;

    /* Initialize stream format memory model */
    InitMemAccess( pRefbu, decMode, DEC_X170_REFBU_WIDTH );

    pRefbu->decModeMbWeights[0] = mbDataPerFormat[decMode][0];
    pRefbu->decModeMbWeights[1] = mbDataPerFormat[decMode][1];
}


/*------------------------------------------------------------------------------
    Function name : GetSettings
    Description   : Determine whether or not to enable buffer, and calculate
                    buffer offset.   

    Return type   : RK_U32
    Argument      : 
------------------------------------------------------------------------------*/
RK_U32 GetSettings( refBuffer_t *pRefbu, RK_S32 *pX, RK_S32 *pY, RK_U32 isBpic,
                 RK_U32 isFieldPic)
{

/* Variables */

    RK_S32 tmp;
    RK_U32 enable = HANTRO_TRUE;
    RK_U32 frmSizeInMbs;
    RK_S32 cov;
    RK_S32 sign;
    RK_S32 numCyclesForBuffering;

/* Code */

    frmSizeInMbs = pRefbu->frmSizeInMbs;
    *pX = 0;
    *pY = 0;

    /* Disable automatically for pictures less than 16MB wide */
    if( pRefbu->picWidthInMbs <= 16 )
    {
        return HANTRO_FALSE;
    }

    numCyclesForBuffering = pRefbu->numCyclesForBuffering;
    if(isFieldPic)
        numCyclesForBuffering /= 2;

    if( pRefbu->prevUsedDouble )
    {
        cov = pRefbu->coverage[0];
        tmp = pRefbu->avgCyclesPerMb * cov / 4;
       
        /* Check whether it is viable to enable buffering */
        tmp = (2*numCyclesForBuffering < tmp);
        if( !tmp )
        {
            pRefbu->thrAdj -= 2;
            if ( pRefbu->thrAdj < THR_ADJ_MIN )
                pRefbu->thrAdj = THR_ADJ_MIN;
        }
        else
        {
            pRefbu->thrAdj++;
            if ( pRefbu->thrAdj > THR_ADJ_MAX )
                pRefbu->thrAdj = THR_ADJ_MAX;
        }
    }

    if( !isBpic )
    {
        if( pRefbu->coverage[1] != -1 )
        {
            cov = (5*pRefbu->coverage[0] - 1*pRefbu->coverage[1])/4;
            if( pRefbu->coverage[2] != -1 )
            {
                cov = cov + ( pRefbu->coverage[0] + pRefbu->coverage[1] + pRefbu->coverage[2] ) / 3;
                cov /= 2;
            }
            
        }
        else if ( pRefbu->coverage[0] != -1 ) 
        {
            cov = pRefbu->coverage[0];
        }
        else
        {
            cov = 4*frmSizeInMbs; 
        }
    }
    else
    {
        cov = pRefbu->coverage[0];
        if( cov == -1 )
        {
            cov = 4*frmSizeInMbs; 
        }
        /* MPEG-4 B-frames have no intra coding possibility, so extrapolate
         * hit rate to match it */
        else if( pRefbu->predIntraBlk < 4*frmSizeInMbs &&
            pRefbu->decMode == DEC_X170_MODE_MPEG4 )
        {
            cov *= (128*4*frmSizeInMbs) / (4*frmSizeInMbs-pRefbu->predIntraBlk) ;
            cov /= 128;
        }
        /* Assume that other formats have less intra MBs in B pictures */
        else
        {
            cov *= (128*4*frmSizeInMbs) / (4*frmSizeInMbs-pRefbu->predIntraBlk/2) ;
            cov /= 128;
        }
    }
    if( cov < 0 )   cov = 0;
    pRefbu->predCoverage = cov;

    /* Check whether it is viable to enable buffering */
    /* 1.criteria = cycles */
    tmp = pRefbu->avgCyclesPerMb * cov / 4;
    numCyclesForBuffering += pRefbu->bufferPenalty * cov / 4;   
    enable = (numCyclesForBuffering < tmp);
    /* 2.criteria = data */
    /*
    tmp = ( DATA_EXCESS_MAX_PCT * cov ) / 400;
    tmp = tmp * pRefbu->mbWeight;
    enable = enable && (pRefbu->totalDataForBuffering < tmp);
    */

#ifdef REFBUFFER_TRACE
    printf("***** ref buffer algorithm trace *****\n");
    printf("predicted coverage  = %7d\n", cov );
    printf("coverage history    = %7d%7d%7d\n",
        pRefbu->coverage[0],
        pRefbu->coverage[1],
        pRefbu->coverage[2] );
    printf("enable              = %d (%8d<%8d)\n", enable, numCyclesForBuffering, tmp );
#endif /* REFBUFFER_TRACE */

    /* If still enabled, calculate offsets */
    if( enable )
    {
        /* Round to nearest 16 multiple */
        tmp = (pRefbu->oy[0] + pRefbu->oy[1] + 1)/2;
        sign = ( tmp < 0 );
        if( pRefbu->prevWasField )
            tmp /= 2;
        tmp = ABS(tmp);
        tmp = tmp & ~15;
        if( sign )  tmp = -tmp;
        *pY = tmp;
    }

#ifdef REFBUFFER_TRACE
    printf("offset_x            = %7d\n", *pX );
    printf("offset_y            = %7d (%d %d)\n", *pY, pRefbu->oy[0], pRefbu->oy[1] );
    printf("***** ref buffer algorithm trace *****\n");
#endif /* REFBUFFER_TRACE */


    return enable;
}

/*------------------------------------------------------------------------------
    Function name : BuildDistribution
    Description   :

    Return type   : 
    Argument      : 
------------------------------------------------------------------------------*/
void BuildDistribution( RK_U32 *pDistrVer, 
                             RK_U32 *pMv, RK_S32 frmSizeInMbs, 
                             RK_U32 mvsPerMb, 
                             RK_U32 bigEndian,
                             RK_S32 *minY, RK_S32 *maxY )
{

/* Variables */

    RK_S32 mb;
    RK_S32 ver;
    RK_U8 * pMvTmp;
    RK_U32 mvs;
    RK_U32 skipMv = mvsPerMb*4;
    RK_U32 multiplier = 4;
    RK_U32 div = 2;

/* Code */

    mvs = frmSizeInMbs;
    /* Try to keep total n:o of mvs checked under MAX_DIRECT_MVS */
    if( mvs > MAX_DIRECT_MVS )
    {
        while( mvs/div > MAX_DIRECT_MVS )
            div++;

        mvs /= div;
        skipMv *= div;
        multiplier *= div;
    }

    pMvTmp = (RK_U8*)pMv;
    if( bigEndian )
    {
        for( mb = 0 ; mb < mvs ; ++mb )
        {
            {
                ver         = DIR_MV_BE_VER(pMvTmp);
                SIGN_EXTEND(ver, 13);
                /* Cut fraction and saturate */
                /*lint -save -e702 */
                ver >>= 2;
                /*lint -restore */
                if( ver >= VER_DISTR_MIN && ver <= VER_DISTR_MAX )
                {
                    pDistrVer[ver] += multiplier;
                    if( ver < *minY )    *minY = ver;
                    if( ver > *maxY )    *maxY = ver;
                }
            }
            pMvTmp += skipMv; /* Skip all other blocks for macroblock */
        }
    }
    else
    {
        for( mb = 0 ; mb < mvs ; ++mb )
        {
            {
                ver         = DIR_MV_LE_VER(pMvTmp);
                SIGN_EXTEND(ver, 13);
                /* Cut fraction and saturate */
                /*lint -save -e702 */
                ver >>= 2;
                /*lint -restore */
                if( ver >= VER_DISTR_MIN && ver <= VER_DISTR_MAX )
                {
                    pDistrVer[ver] += multiplier;
                    if( ver < *minY )    *minY = ver;
                    if( ver > *maxY )    *maxY = ver;
                }
            }
            pMvTmp += skipMv; /* Skip all other blocks for macroblock */
        }
    }
}


/*------------------------------------------------------------------------------
    Function name : DirectMvStatistics
    Description   :

    Return type   : 
    Argument      : 
------------------------------------------------------------------------------*/
void DirectMvStatistics( refBuffer_t *pRefbu, RK_U32 *pMv, RK_S32 numIntraBlk,
                         RK_U32 bigEndian )
{

/* Variables */

    RK_S32 * pTmp;
    RK_U8 *pMvTmp;
    RK_S32 mb;
    RK_U32 tmp;
    RK_S32 frmSizeInMbs;
    RK_U32 width;
    RK_S32 i;
    RK_S32 ver;
    RK_S32 oy = 0;
    RK_S32 best = 0;
    RK_S32 sum;
    RK_S32 mvsPerMb;
    RK_S32 minY = VER_DISTR_MAX, maxY = VER_DISTR_MIN;
    
    /* Mv distributions per component*/
    RK_U32  distrVer[VER_DISTR_RANGE] = { 0 };
    RK_U32 *pDistrVer = distrVer + VER_DISTR_ZERO_POINT;

/* Code */

    mvsPerMb = pRefbu->mvsPerMb;
    width = pRefbu->picWidthInMbs;

    if( pRefbu->prevWasField )
        frmSizeInMbs = pRefbu->fldSizeInMbs;
    else
        frmSizeInMbs = pRefbu->frmSizeInMbs;

    if( numIntraBlk < 4*frmSizeInMbs )
    {
        BuildDistribution( pDistrVer,
                           pMv,
                           frmSizeInMbs,
                           mvsPerMb,
                           bigEndian,
                           &minY, &maxY );

        /* Fix Intra occurences */
        pDistrVer[0] -= numIntraBlk;

#if 0 /* Use median for MV calculation */
        /* Find median */
        {
            tmp = (frmSizeInMbs - numIntraMbs) / 2;
            sum = 0;
            i = VER_DISTR_MIN;
            for( i = VER_DISTR_MIN ; sum < tmp ; i++ )
                sum += pDistrVer[i];
            oy = i-1;

            /* Calculate coverage percent */
            best = 0;
            i = MAX( VER_DISTR_MIN, oy-VER_MV_RANGE );
            limit = MIN( VER_DISTR_MAX, oy+VER_MV_RANGE );
            for( ; i < limit ; ++i )
                best += pDistrVer[i];
        }
        else
#endif
        {
            RK_S32 y;
            RK_S32 penalty;

            /* Initial sum */
            sum = 0;
            minY += VER_DISTR_ZERO_POINT;
            maxY += VER_DISTR_ZERO_POINT;

            for( i = 0 ; i < 2*VER_MV_RANGE ; ++i ) 
            {
                sum += distrVer[i];
            }
            best = 0;
            /* Other sums */
            maxY -= 2*VER_MV_RANGE;
            for( i = 0 ; i < VER_DISTR_RANGE-2*VER_MV_RANGE-1 ; ++i )
            {
                sum -= distrVer[i];
                sum += distrVer[2*VER_MV_RANGE+i];
                y = VER_DISTR_MIN+VER_MV_RANGE+i+1;
                if ( ABS(y) > 8 )
                {
                    penalty = ABS(y)-8;
                    penalty = (frmSizeInMbs*penalty)/16;
                }
                else
                {
                    penalty = 0;
                }
                /*if( (ABS(y) & 15) == 0 )*/
                {
                    if( sum - penalty > best )
                    {
                        best = sum - penalty;
                        oy = y;
                    }
                    else if ( sum - penalty == best )
                    {
                        if( ABS(y) < ABS(oy) )  oy = y;
                    }
                }
            }
        }

        if(pRefbu->prevWasField)
            best *= 2;
        pRefbu->coverage[0] = best; 

        /* Store updated offsets */
        pTmp = pRefbu->oy;
        pTmp[2] = pTmp[1];
        pTmp[1] = pTmp[0];
        pTmp[0] = oy;

    }
    else
    {
        pTmp = pRefbu->oy;
        pTmp[2] = pTmp[1];
        pTmp[1] = pTmp[0];
        pTmp[0] = 0;
    }

}

/*------------------------------------------------------------------------------
    Function name : RefbuMvStatisticsB
    Description   :

    Return type   : 
    Argument      : 
------------------------------------------------------------------------------*/
void RefbuMvStatisticsB( refBuffer_t *pRefbu, RK_U32 *regBase )
{

/* Variables */

    RK_S32 topFldCnt;
    RK_S32 botFldCnt;

/* Code */

    topFldCnt = GetDecRegister( regBase, HWIF_REFBU_TOP_SUM );
    botFldCnt = GetDecRegister( regBase, HWIF_REFBU_BOT_SUM );

    if( pRefbu->fldCnt >= 2 &&
        GetDecRegister( regBase, HWIF_FIELDPIC_FLAG_E ) &&
        (topFldCnt || botFldCnt))
    {
        pRefbu->fldHitsB[2][0] = pRefbu->fldHitsB[1][0];    pRefbu->fldHitsB[2][1] = pRefbu->fldHitsB[1][1];
        pRefbu->fldHitsB[1][0] = pRefbu->fldHitsB[0][0];    pRefbu->fldHitsB[1][1] = pRefbu->fldHitsB[0][1];
        if( GetDecRegister( regBase, HWIF_PIC_TOPFIELD_E ) ) {
            pRefbu->fldHitsB[0][0] = topFldCnt;         pRefbu->fldHitsB[0][1] = botFldCnt;
        } else {
            pRefbu->fldHitsB[0][0] = botFldCnt;         pRefbu->fldHitsB[0][1] = topFldCnt;
        }
    }
    if( GetDecRegister( regBase, HWIF_FIELDPIC_FLAG_E ) )
        pRefbu->fldCnt++;
}

/*------------------------------------------------------------------------------
    Function name : RefbuMvStatistics
    Description   :

    Return type   : 
    Argument      : 
------------------------------------------------------------------------------*/
void RefbuMvStatistics( refBuffer_t *pRefbu, RK_U32 *regBase, 
                        RK_U32 *pMv, RK_U32 directMvsAvailable, RK_U32 isIntraPicture )
{

/* Variables */

    RK_S32 * pTmp;
    RK_S32 tmp;
    RK_S32 numIntraBlk;
    RK_S32 topFldCnt;
    RK_S32 botFldCnt;
    RK_S32 currTopField;
    RK_U32 bigEndian;

/* Code */

    if( isIntraPicture )
    {
        /*IntraFrame( pRefbu );*/
        return; /* Clear buffers etc. ? */
    }

    if(pRefbu->prevWasField && !pRefbu->interlacedSupport)
        return;

#ifdef ASIC_TRACE_SUPPORT
    /* big endian */
    bigEndian = 1;
#else
    /* Determine HW endianness; this affects how we read motion vector
     * map from HW. Map endianness is same as decoder output endianness */
    if( GetDecRegister( regBase, HWIF_DEC_OUT_ENDIAN ) == 0 )
    {
        /* big endian */
        bigEndian = 1;
    }
    else
    {
        /* little endian */
        bigEndian = 0;
    }
#endif

    if(!pRefbu->offsetSupport)
    {
        directMvsAvailable = 0;
    }

    numIntraBlk = GetDecRegister( regBase, HWIF_REFBU_INTRA_SUM );
    topFldCnt = GetDecRegister( regBase, HWIF_REFBU_TOP_SUM );
    botFldCnt = GetDecRegister( regBase, HWIF_REFBU_BOT_SUM );

#ifdef REFBUFFER_TRACE
    printf("***** ref buffer mv statistics trace *****\n");
    printf("intra blocks        = %7d\n", numIntraBlk );
    printf("top fields          = %7d\n", topFldCnt );
    printf("bottom fields       = %7d\n", botFldCnt );
#endif /* REFBUFFER_TRACE */

    if( pRefbu->fldCnt > 0 &&
        GetDecRegister( regBase, HWIF_FIELDPIC_FLAG_E ) &&
        (topFldCnt || botFldCnt))
    {
        pRefbu->fldHitsP[2][0] = pRefbu->fldHitsP[1][0];    pRefbu->fldHitsP[2][1] = pRefbu->fldHitsP[1][1];
        pRefbu->fldHitsP[1][0] = pRefbu->fldHitsP[0][0];    pRefbu->fldHitsP[1][1] = pRefbu->fldHitsP[0][1];
        if( GetDecRegister( regBase, HWIF_PIC_TOPFIELD_E ) ) {
            pRefbu->fldHitsP[0][0] = topFldCnt;         pRefbu->fldHitsP[0][1] = botFldCnt;
        } else {
            pRefbu->fldHitsP[0][0] = botFldCnt;         pRefbu->fldHitsP[0][1] = topFldCnt;
        }
    }
    if( GetDecRegister( regBase, HWIF_FIELDPIC_FLAG_E ) )
        pRefbu->fldCnt++;
            
    pRefbu->coverage[2] = pRefbu->coverage[1];
    pRefbu->coverage[1] = pRefbu->coverage[0];
    if(directMvsAvailable)
    {
        DirectMvStatistics( pRefbu, pMv, numIntraBlk, bigEndian );
    }
    else if(pRefbu->offsetSupport)
    {
        RK_S32 interMvs;
        RK_S32 sum;
        sum = GetDecRegister( regBase, HWIF_REFBU_Y_MV_SUM );
        SIGN_EXTEND( sum, 22 );
        interMvs = (4*pRefbu->frmSizeInMbs - numIntraBlk)/4;
        if( pRefbu->prevWasField )
            interMvs *= 2;
        if( interMvs == 0 ) 
            interMvs = 1;   
        /* Require at least 50% mvs present to calculate reliable avg offset */
        if( interMvs * 50 >= pRefbu->frmSizeInMbs )
        {
            /* Store updated offsets */
            pTmp = pRefbu->oy;
            pTmp[2] = pTmp[1];
            pTmp[1] = pTmp[0];
            pTmp[0] = sum/interMvs;
        }
    }

    /* Read buffer hits from previous frame. If number of hits < threshold
     * for the frame, then we know that HW switched buffering off. */
    tmp = GetDecRegister( regBase, HWIF_REFBU_HIT_SUM );
    if ( tmp >= pRefbu->checkpoint && pRefbu->checkpoint )
    {
        if(pRefbu->prevWasField)
            tmp *= 2;
        pRefbu->coverage[0] = tmp;

#ifdef REFBUFFER_TRACE
        printf("actual coverage     = %7d\n", tmp );
#endif /* REFBUFFER_TRACE */

    }
    else if(!directMvsAvailable)
    {
        /* Buffering was disabled for previous frame, no direct mv 
         * data available either, so assume we got a bit more hits than 
         * the frame before that */
        if( pRefbu->coverage[1] != -1 )
        {
            pRefbu->coverage[0] = ( 4 * 
                pRefbu->picWidthInMbs * 
                pRefbu->picHeightInMbs + 5 * pRefbu->coverage[1] ) / 6;
        }
        else
            pRefbu->coverage[0] = pRefbu->frmSizeInMbs * 4;

#ifdef REFBUFFER_TRACE
        printf("calculated coverage = %7d\n", pRefbu->coverage[0] );
#endif /* REFBUFFER_TRACE */

    }
    else
    {

#ifdef REFBUFFER_TRACE
        printf("estimated coverage  = %7d\n", pRefbu->coverage[0] );
#endif /* REFBUFFER_TRACE */

    }

    /* Store intra counts for rate prediction */
    pTmp = pRefbu->numIntraBlk;
    pTmp[2] = pTmp[1];
    pTmp[1] = pTmp[0];
    pTmp[0] = numIntraBlk;

    /* Predict number of intra mbs for next frame */
    if(pTmp[2] != -1)           tmp = (pTmp[0] + pTmp[1] + pTmp[2])/3;
    else if( pTmp[1] != -1 )    tmp = (pTmp[0] + pTmp[1])/2;
    else                        tmp = pTmp[0];
    pRefbu->predIntraBlk = MIN( pTmp[0], tmp ); 

#ifdef REFBUFFER_TRACE
    printf("predicted intra blk = %7d\n", pRefbu->predIntraBlk );
    printf("***** ref buffer mv statistics trace *****\n");
#endif /* REFBUFFER_TRACE */


}

/*------------------------------------------------------------------------------
    Function name : DecideParityMode
    Description   : Setup reference buffer for next frame.

    Return type   : 
    Argument      : pRefbu              Reference buffer descriptor
                    isBframe            Is frame B-coded
------------------------------------------------------------------------------*/
RK_U32 DecideParityMode( refBuffer_t *pRefbu, RK_U32 isBframe )
{

/* Variables */

    RK_S32 same, opp;

/* Code */

    if( pRefbu->decMode != DEC_X170_MODE_H264 )
    {
        /* Don't use parity mode for other formats than H264
         * for now */
        return 0;
    }


    /* Read history */
    if( isBframe )
    {
        same = pRefbu->fldHitsB[0][0];
        /*
        if( pRefbu->fldHitsB[1][0] >= 0 )
            same += pRefbu->fldHitsB[1][0];
        if( pRefbu->fldHitsB[2][0] >= 0 )
            same += pRefbu->fldHitsB[2][0];
            */
        opp  = pRefbu->fldHitsB[0][1];
        /*
        if( pRefbu->fldHitsB[1][1] >= 0 )
            opp += pRefbu->fldHitsB[1][1];
        if( pRefbu->fldHitsB[2][0] >= 0 )
            opp += pRefbu->fldHitsB[2][1];*/
    }
    else
    {
        same = pRefbu->fldHitsP[0][0];
        /*
        if( pRefbu->fldHitsP[1][0] >= 0 )
            same += pRefbu->fldHitsP[1][0];
        if( pRefbu->fldHitsP[2][0] >= 0 )
            same += pRefbu->fldHitsP[2][0];
            */
        opp  = pRefbu->fldHitsP[0][1];
        /*
        if( pRefbu->fldHitsP[1][1] >= 0 )
            opp += pRefbu->fldHitsP[1][1];
        if( pRefbu->fldHitsP[2][0] >= 0 )
            opp += pRefbu->fldHitsP[2][1];
            */
    }

    /* If not enough data yet, bail out */
    if( same == -1 || opp == -1 )
        return 0;

    if( opp*2 <= same )
        return 1;

    return 0;

}

/*------------------------------------------------------------------------------
    Function name : RefbuSetup
    Description   : Setup reference buffer for next frame.

    Return type   : 
    Argument      : pRefbu              Reference buffer descriptor
                    regBase             Pointer to SW/HW control registers
                    isInterlacedField   
                    isIntraFrame        Is frame intra-coded (or IDR for H.264)
                    isBframe            Is frame B-coded
                    refPicId            pic_id for reference picture, if 
                                        applicable. For H.264 pic_id for 
                                        nearest reference picture of L0.
------------------------------------------------------------------------------*/
void RefbuSetup( refBuffer_t *pRefbu, RK_U32 *regBase,
                 refbuMode_e mode,
                 RK_U32 isIntraFrame, 
                 RK_U32 isBframe, 
                 RK_U32 refPicId0, RK_U32 refPicId1, RK_U32 flags )
{

/* Variables */

    RK_S32 ox, oy;
    RK_S32 enable;
    RK_S32 tmp;
    RK_S32 thr2 = 0;
    RK_U32 featureDisable = 0;
    RK_U32 useAdaptiveMode = 0;
    RK_U32 useDoubleMode = 0;
    RK_U32 goldenFrameExists = 0;
    RK_U32 multipleRefFrames = 1;
    RK_U32 multipleRefFields = 1;
    RK_U32 singleRefField = 0;
    RK_U32 pic0 = 0, pic1 = 0;

/* Code */

    SetDecRegister(regBase, HWIF_REFBU_THR, 0 );
    SetDecRegister(regBase, HWIF_REFBU2_THR, 0 );
    SetDecRegister(regBase, HWIF_REFBU_PICID, 0 );
    SetDecRegister(regBase, HWIF_REFBU_Y_OFFSET, 0 );

    multipleRefFrames = ( flags & REFBU_MULTIPLE_REF_FRAMES ) ? 1 : 0;
    goldenFrameExists = ( flags & REFBU_GOLDEN_FRAME_EXISTS ) ? 1 : 0;

    pRefbu->prevWasField = (mode == REFBU_FIELD && !isBframe);
    
    /* Check supported features */
    if(mode != REFBU_FRAME && !pRefbu->interlacedSupport)
        featureDisable = 1;

    if(!isIntraFrame && !featureDisable)
    {
        if(pRefbu->prevLatency != pRefbu->currMemModel.latency)
        {
            UpdateMemModel( pRefbu );
            pRefbu->prevLatency = pRefbu->currMemModel.latency;
        }

        enable = GetSettings( pRefbu, &ox, &oy, 
                              isBframe, mode == REFBU_FIELD);

        tmp = RefbuGetHitThreshold( pRefbu );
        pRefbu->checkpoint = tmp;

        if( mode == REFBU_FIELD )
        {
            tmp = DecideParityMode( pRefbu, isBframe );
            SetDecRegister( regBase, HWIF_REFBU_FPARMOD_E, tmp );
            if( !tmp )
            {
                pRefbu->thrAdj = 1;
            }
        }
        else
        {
            pRefbu->thrAdj = 1;
        }

        SetDecRegister(regBase, HWIF_REFBU_E, enable );
        if( enable )
        {
            /* Figure out which features to enable */
            if( pRefbu->doubleSupport )
            {
                if( !isBframe ) /* P field/frame */
                {
                    if( mode == REFBU_FIELD )
                    {
                        if( singleRefField )
                        {
                            /* Buffer only reference field given in refPicId0 */
                        }
                        else if (multipleRefFields )
                        {
                            /* Let's not try to guess */
                            useDoubleMode = 1;
                            useAdaptiveMode = 1;
                            pRefbu->checkpoint /= pRefbu->thrAdj;
                            thr2 = pRefbu->checkpoint ;
                        }
                        else
                        {
                            /* Buffer both reference fields explicitly */
                            useDoubleMode = 1;
                            pRefbu->checkpoint /= pRefbu->thrAdj;
                            thr2 = pRefbu->checkpoint;
                        }
                    }
                    else if( multipleRefFrames )
                    {
                        /*
                        useAdaptiveMode = 1;
                        useDoubleMode = 1;
                        */
                        pRefbu->checkpoint /= pRefbu->thrAdj;
                        thr2 = pRefbu->checkpoint;
                    }
                    else if ( goldenFrameExists )
                    {
                        useDoubleMode = 1;
                        pRefbu->checkpoint /= pRefbu->thrAdj;
                        thr2 = pRefbu->checkpoint;
                    }
                    else
                    {
                        /* Content to buffer just one ref pic */
                    }
                }
                else /* B field/frame */
                {
                    if( mode == REFBU_FIELD )
                    {
                        /* Let's not try to guess */
                        useAdaptiveMode = 1;
                        useDoubleMode = 1;
                        pRefbu->checkpoint /= pRefbu->thrAdj;
                        /*pRefbu->checkpoint /= 2;*/
                        thr2 = pRefbu->checkpoint;
                    }
                    else if (!multipleRefFrames )
                    {
                        /* Buffer forward and backward pictures as given in
                         * refPicId0 and refPicId1 */
                        useDoubleMode = 1;
                        pRefbu->checkpoint /= pRefbu->thrAdj;
                        thr2 = pRefbu->checkpoint;
                    }
                    else
                    {
                        /* Let's not try to guess */
                        useDoubleMode = 1;
                        useAdaptiveMode = 1;
                        pRefbu->checkpoint /= pRefbu->thrAdj;
                        thr2 = pRefbu->checkpoint;
                    }
                }
            }
            else /* Just single buffering supported */
            {
                if( !isBframe ) /* P field/frame */
                {
                    if( mode == REFBU_FIELD )
                    {
                        useAdaptiveMode = 1;
                    }
                    else if (multipleRefFrames)
                    {
                        /*useAdaptiveMode = 1;*/
                    }
                    else if (goldenFrameExists)
                    {
                        /* what here */
                    }
                    else
                    {
                    }
                }
                else /* B field/frame */
                {
                    useAdaptiveMode = 1;
                }
            }

            if(!useAdaptiveMode)
            {
                pic0 = refPicId0;
                if( useDoubleMode )
                {
                    pic1 = refPicId1;
                }
            }

            SetDecRegister(regBase, HWIF_REFBU_EVAL_E, useAdaptiveMode );            
            
            /* Calculate amount of hits required for first mb row */

            if ( mode == REFBU_MBAFF )
            {
                pRefbu->checkpoint *= 2;
                thr2 *= 2;
            }

            if( useDoubleMode )
            {
                oy = 0; /* Disable offset */
            }

            /* Limit offset */
            {
                RK_S32 limit, height;
                height = pRefbu->picHeightInMbs;
                if( mode == REFBU_FIELD )
                    height /= 2; /* adjust to field height */
                
                if( mode == REFBU_MBAFF )
                    limit = 64; /* 4 macroblock rows */
                else
                    limit = 48; /* 3 macroblock rows */

                if( (RK_S32)(oy+limit) > (RK_S32)(height*16) )
                    oy = height*16-limit;
                if( (RK_S32)((-oy)+limit) > (RK_S32)(height*16) )
                    oy = -(height*16-limit);
            }

            /* Disable offset just to make sure */
            if(!pRefbu->offsetSupport || 1) /* NOTE: always disabled for now */
                oy = 0; 

            SetDecRegister(regBase, HWIF_REFBU_THR, pRefbu->checkpoint );
            SetDecRegister(regBase, HWIF_REFBU_PICID, pic0 );
            SetDecRegister(regBase, HWIF_REFBU_Y_OFFSET, oy );

            if(pRefbu->doubleSupport)
            {
                /* Very much TODO */
                SetDecRegister(regBase, HWIF_REFBU2_BUF_E, useDoubleMode );
                SetDecRegister(regBase, HWIF_REFBU2_THR, thr2 );
                SetDecRegister(regBase, HWIF_REFBU2_PICID, pic1 );
                pRefbu->prevUsedDouble = useDoubleMode;
            }
        }
        pRefbu->prevWasField = (mode == REFBU_FIELD && !isBframe);
    }
    else
    {
        pRefbu->checkpoint = 0;
        SetDecRegister(regBase, HWIF_REFBU_E, HANTRO_FALSE );
    }

    if(pRefbu->testFunction)
    {
        pRefbu->testFunction(pRefbu, regBase, isIntraFrame, mode );
    }

}

