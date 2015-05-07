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
--  $RCSfile: h264hwd_util.h,v $
--  $Date: 2008/09/03 05:56:16 $
--  $Revision: 1.2 $
--
------------------------------------------------------------------------------*/

#ifndef H264BSDDEC_UTIL_H
#define H264BSDDEC_UTIL_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "basetype.h"
#include "dwl.h"

#include "h264hwd_stream.h"
#include "h264hwd_debug.h"

/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/

#define HANTRO_OK   0
#define HANTRO_NOK  1

#define HANTRO_FALSE   (0U)
#define HANTRO_TRUE    (1U)

#define MEMORY_ALLOCATION_ERROR     0xFFFF
#define PARAM_SET_ERROR             0xFFF0

/* value to be returned by GetBits if stream buffer is empty */
#define END_OF_STREAM               0xFFFFFFFFU

#define EMPTY_RESIDUAL_INDICATOR    0xFFFFFF

/* macro to mark a residual block empty, i.e. contain zero coefficients */
#define MARK_RESIDUAL_EMPTY(residual) ((residual)[0] = EMPTY_RESIDUAL_INDICATOR)
/* macro to check if residual block is empty */
#define IS_RESIDUAL_EMPTY(residual) ((residual)[0] == EMPTY_RESIDUAL_INDICATOR)

#ifndef MIN
/* macro to get smaller of two values */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
/* macro to get greater of two values */
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef ABS
/* macro to get absolute value */
#define ABS(a) (((a) < 0) ? -(a) : (a))
#endif

/* macro to clip a value z, so that x <= z =< y */
#define CLIP3(x,y,z) (((z) < (x)) ? (x) : (((z) > (y)) ? (y) : (z)))

/* macro to clip a value z, so that 0 <= z =< 255 */
#define CLIP1(z) (((z) < 0) ? 0 : (((z) > 255) ? 255 : (z)))

/* macro to allocate memory */
/*#define ALLOCATE(ptr, count, type) \
{ \
    ptr = malloc((count) * sizeof(type)); \
}*/

/* macro to free allocated memory */
#define FREE(ptr) \
{ \
    if(ptr != NULL) {free(ptr); ptr = NULL;}\
}

extern const u32 h264bsdQpC[52];

/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/

typedef enum
{
    TOPFIELD = 0,
    BOTFIELD = 1,
    FRAME    = 2
} picStruct_e;

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

u32 h264bsdCountLeadingZeros(u32 value, u32 length);

u32 h264bsdRbspTrailingBits(strmData_t * strmData);

u32 h264bsdMoreRbspData(strmData_t * strmData);

u32 h264bsdNextMbAddress(u32 * pSliceGroupMap, u32 picSizeInMbs,
                         u32 currMbAddr);

u32 h264CheckCabacZeroWords( strmData_t *strmData );

#endif /* #ifdef H264BSDDEC_UTIL_H */
