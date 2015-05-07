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
--  Description :
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: regdrv.c,v $
--  $Revision: 1.6 $
--  $Date: 2008/04/24 11:49:29 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Include headers
------------------------------------------------------------------------------*/

#include "regdrv.h"

/*------------------------------------------------------------------------------
    External compiler flags
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/

#ifdef _ASSERT_USED
#ifndef ASSERT
#include <assert.h>
#define ASSERT(expr) assert(expr)
#endif
#else
#define ASSERT(expr)
#endif

static const u32 regMask[33] = { 0x00000000,
    0x00000001, 0x00000003, 0x00000007, 0x0000000F,
    0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF,
    0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF,
    0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF,
    0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF,
    0x001FFFFF, 0x003FFFFF, 0x007FFFFF, 0x00FFFFFF,
    0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF,
    0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF
};

/* { SWREG, BITS, POSITION } */
static const u32 hwDecRegSpec[HWIF_LAST_REG + 1][3] = {
/* include script-generated part */
#include "8170table.h"
/* HWIF_DEC_IRQ_STAT */ {1, 7, 12},
/* HWIF_PP_IRQ_STAT */ {60, 2, 12},
/* dummy entry */ {0, 0, 0}
};

//extern DispLinearMem   DisplayBuff[3];

/*------------------------------------------------------------------------------
    Local function prototypes
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Functions
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Function name: SetDecRegister

        Functional description:

        Inputs:

        Outputs:

        Returns:

------------------------------------------------------------------------------*/
void SetDecRegister(u32 * regBase, u32 id, u32 value)
{

    u32 tmp;

    ASSERT(id < HWIF_LAST_REG);

#if 0
    ALOGV("set:reg[%.2d] bit[%.2d] len[%.2d] val: %8d\n",
        hwDecRegSpec[id][0],
        hwDecRegSpec[id][2],
        hwDecRegSpec[id][1],
        value);
#endif

    tmp = regBase[hwDecRegSpec[id][0]];
    tmp &= ~(regMask[hwDecRegSpec[id][1]] << hwDecRegSpec[id][2]);
    tmp |= (value & regMask[hwDecRegSpec[id][1]]) << hwDecRegSpec[id][2];
    regBase[hwDecRegSpec[id][0]] = tmp;

}

/*------------------------------------------------------------------------------

    Function name: GetDecRegister

        Functional description:

        Inputs:

        Outputs:

        Returns:

------------------------------------------------------------------------------*/
u32 GetDecRegister(const u32 * regBase, u32 id)
{

    u32 tmp;

    ASSERT(id < HWIF_LAST_REG);

    tmp = regBase[hwDecRegSpec[id][0]];
    tmp = tmp >> hwDecRegSpec[id][2];
    tmp &= regMask[hwDecRegSpec[id][1]];

#if 0
    ALOGV("get:reg[%.2d] bit[%.2d] len[%.2d] val: %8d\n",
        hwDecRegSpec[id][0],
        hwDecRegSpec[id][2],
        hwDecRegSpec[id][1],
        tmp);
#endif

    return (tmp);

}



