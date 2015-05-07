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
--  Description : Hardware interface read/write
--
------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: refbuffer.h,v $
--  $Revision: 1.17 $
--  $Date: 2010/04/15 12:15:24 $
--
------------------------------------------------------------------------------*/
#ifndef __REFBUFFER_H__
#define __REFBUFFER_H__

#include "vpu_type.h"

typedef enum {
    REFBU_FRAME,
    REFBU_FIELD,
    REFBU_MBAFF
} refbuMode_e;

/* Feature support flags */
#define REFBU_SUPPORT_GENERIC       (1)
#define REFBU_SUPPORT_INTERLACED    (2)
#define REFBU_SUPPORT_DOUBLE        (4)
#define REFBU_SUPPORT_OFFSET        (8)

/* Buffering info */
#define REFBU_BUFFER_SINGLE_FIELD   (1)
#define REFBU_MULTIPLE_REF_FRAMES   (2)
#define REFBU_GOLDEN_FRAME_EXISTS   (4)

#ifndef HANTRO_TRUE
    #define HANTRO_TRUE     (1)
#endif /* HANTRO_TRUE */

#ifndef HANTRO_FALSE
    #define HANTRO_FALSE    (0)
#endif /* HANTRO_FALSE*/

/* macro to get smaller of two values */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

/* macro to get greater of two values */
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

typedef struct memAccess {
    RK_U32 latency;
    RK_U32 nonseq;
    RK_U32 seq;
} memAccess_t;

struct refBuffer;
typedef struct refBuffer {
#if 0
    RK_S32 ox[3];
#endif
    RK_S32 decModeMbWeights[2];
    RK_S32 mbWeight;
    RK_S32 oy[3];
    RK_S32 picWidthInMbs;
    RK_S32 picHeightInMbs;
    RK_S32 frmSizeInMbs;
    RK_S32 fldSizeInMbs;
    RK_S32 numIntraBlk[3];
    RK_S32 coverage[3];
    RK_S32 fldHitsP[3][2];
    RK_S32 fldHitsB[3][2];
    RK_S32 fldCnt;
    RK_S32 mvsPerMb;
    RK_S32 filterSize;
    /* Thresholds */
    RK_S32 predIntraBlk;
    RK_S32 predCoverage;
    RK_S32 checkpoint;
    RK_U32 decMode;
    RK_U32 dataExcessMaxPct;

    RK_S32 busWidthInBits;
    RK_S32 prevLatency;
    RK_S32 numCyclesForBuffering;
    RK_S32 totalDataForBuffering;
    RK_S32 bufferPenalty;
    RK_S32 avgCyclesPerMb;
    RK_U32 prevWasField;
    RK_U32 prevUsedDouble;
    RK_S32 thrAdj;
    memAccess_t currMemModel;   /* Clocks per operation, modifiable from 
                                 * testbench. */
    memAccess_t memAccessStats; /* Approximate counts for operations, set
                                 * based on format */
    RK_U32 memAccessStatsFlag;

    /* Support flags */
    RK_U32 interlacedSupport;
    RK_U32 doubleSupport;
    RK_U32 offsetSupport;

    /* Internal test mode */
    void (*testFunction)(struct refBuffer*,RK_U32*regBase,RK_U32 isIntra,RK_U32 mode);

} refBuffer_t;
void RefbuInit( refBuffer_t *pRefbu, RK_U32 decMode, RK_U32 picWidthInMbs, RK_U32 
                picHeightInMbs, RK_U32 supportFlags );

void RefbuMvStatistics( refBuffer_t *pRefbu, RK_U32 *regBase, 
                        RK_U32 *pMv, RK_U32 directMvsAvailable, 
                        RK_U32 isIntraPicture );

void RefbuMvStatisticsB( refBuffer_t *pRefbu, RK_U32 *regBase );

void RefbuSetup( refBuffer_t *pRefbu, RK_U32 *regBase,
                 refbuMode_e mode,
                 RK_U32 isIntraFrame, RK_U32 isBframe, 
                 RK_U32 refPicId0, RK_U32 refpicId1,
                 RK_U32 flags );


#endif /* __REFBUFFER_H__ */
