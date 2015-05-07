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
--  Description : ASIC low level controller
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: encasiccontroller_v2.c,v $
--  $Revision: 1.1 $
--  $Date: 2007/07/16 09:28:40 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Include headers
------------------------------------------------------------------------------*/
#include "encpreprocess.h"
#include "encasiccontroller.h"
#include "enccommon.h"
#include "ewl.h"
#include <utils/Log.h>

/*------------------------------------------------------------------------------
    External compiler flags
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/

/* Mask fields */
#define mask_2b         (RK_U32)0x00000003
#define mask_3b         (RK_U32)0x00000007
#define mask_4b         (RK_U32)0x0000000F
#define mask_5b         (RK_U32)0x0000001F
#define mask_6b         (RK_U32)0x0000003F
#define mask_11b        (RK_U32)0x000007FF
#define mask_14b        (RK_U32)0x00003FFF
#define mask_16b        (RK_U32)0x0000FFFF

#define HSWREG(n)       ((n)*4)

/*------------------------------------------------------------------------------
    Local function prototypes
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    EncAsicMemAlloc_V2

    Allocate HW/SW shared memory

    Input:
        asic        asicData structure
        width       width of encoded image, multiple of four
        height      height of encoded image
        type        ASIC_MPEG4 / ASIC_H263 / ASIC_JPEG

    Output:
        asic        base addresses point to allocated memory areas

    Return:
        ENCHW_OK        Success.
        ENCHW_NOK       Error: memory allocation failed, no memories allocated
                        and EWL instance released

------------------------------------------------------------------------------*/
RK_S32 EncAsicMemAlloc_V2(asicData_s * asic, RK_U32 width, RK_U32 height,
                       RK_U32 encodingType)
{
    RK_U32 mbTotal;
    regValues_s *regs;
    VPUMemLinear_t *buff = NULL;

    ASSERT(asic != NULL);
    ASSERT(width != 0);
    ASSERT(height != 0);
    ASSERT((height % 2) == 0);
    ASSERT((width % 4) == 0);

    regs = &asic->regs;

    regs->codingType = encodingType;

    width = (width + 15) / 16;
    height = (height + 15) / 16;

    mbTotal = width * height;

    if(regs->codingType != ASIC_JPEG)
    {
        /* The sizes of the memories */
        RK_U32 internalImageLumaSize = mbTotal * (16 * 16);

        RK_U32 internalImageChromaSize = mbTotal * (2 * 8 * 8);

        /* Allocate internal image, not needed for JPEG */
        if(VPUMallocLinear(&asic->internalImageLuma[0], internalImageLumaSize) != EWL_OK)
        {
            EncAsicMemFree_V2(asic);
            return ENCHW_NOK;
        }

        if(VPUMallocLinear(&asic->internalImageChroma[0], internalImageChromaSize) != EWL_OK)
        {
            EncAsicMemFree_V2(asic);
            return ENCHW_NOK;
        }

        /* Allocate internal image, not needed for JPEG */
        if(VPUMallocLinear(&asic->internalImageLuma[1], internalImageLumaSize) != EWL_OK)
        {
            EncAsicMemFree_V2(asic);
            return ENCHW_NOK;
        }

        if(VPUMallocLinear(&asic->internalImageChroma[1], internalImageChromaSize) != EWL_OK)
        {
            EncAsicMemFree_V2(asic);
            return ENCHW_NOK;
        }

        /* Set base addresses to the registers */
        regs->internalImageLumBaseW = asic->internalImageLuma[0].phy_addr;
        regs->internalImageChrBaseW = asic->internalImageChroma[0].phy_addr;
        regs->internalImageLumBaseR = asic->internalImageLuma[1].phy_addr;
        regs->internalImageChrBaseR = asic->internalImageChroma[1].phy_addr;

        /* NAL size table, table size must be 64-bit multiple,
         * space for zero at the end of table */
        if(regs->codingType == ASIC_H264)
        {
            /* Atleast 1 macroblock row in every slice */
            buff = &asic->sizeTbl.nal;
            asic->sizeTblSize = (sizeof(RK_U32) * (height+1) + 7) & (~7);
        }

        if(VPUMallocLinear(buff, asic->sizeTblSize) != EWL_OK)
        {
            EncAsicMemFree_V2(asic);
            return ENCHW_NOK;
        }

        /* CABAC context tables: all qps, intra+inter, 464 bytes/table  */
        if(VPUMallocLinear(&asic->cabacCtx, 52*2*464) != EWL_OK)
        {
            EncAsicMemFree_V2(asic);
            return ENCHW_NOK;
        }
        regs->cabacCtxBase = asic->cabacCtx.phy_addr;

        if(regs->riceEnable)
        {
            RK_U32 bytes = ((width+11)/12 * (height*2-1)) * 8;
            if(VPUMallocLinear(&asic->riceRead, bytes) != EWL_OK)
            {
                EncAsicMemFree_V2(asic);
                return ENCHW_NOK;
            }
            if(VPUMallocLinear(&asic->riceWrite, bytes) != EWL_OK)
            {
                EncAsicMemFree_V2(asic);
                return ENCHW_NOK;
            }
            regs->riceReadBase = asic->riceRead.phy_addr;
            regs->riceWriteBase = asic->riceWrite.phy_addr;
        }
    }

    return ENCHW_OK;
}

/*------------------------------------------------------------------------------

    EncAsicMemFree_V2

    Free HW/SW shared memory

------------------------------------------------------------------------------*/
void EncAsicMemFree_V2(asicData_s * asic)
{
    ASSERT(asic != NULL);

    if(asic->internalImageLuma[0].vir_addr != NULL)
        VPUFreeLinear(&asic->internalImageLuma[0]);

    if(asic->internalImageChroma[0].vir_addr != NULL)
        VPUFreeLinear(&asic->internalImageChroma[0]);

    if(asic->internalImageLuma[1].vir_addr != NULL)
        VPUFreeLinear(&asic->internalImageLuma[1]);

    if(asic->internalImageChroma[1].vir_addr != NULL)
        VPUFreeLinear(&asic->internalImageChroma[1]);

    if(asic->sizeTbl.nal.vir_addr != NULL)
        VPUFreeLinear(&asic->sizeTbl.nal);

    if(asic->cabacCtx.vir_addr != NULL)
        VPUFreeLinear(&asic->cabacCtx);

    if(asic->riceRead.vir_addr != NULL)
        VPUFreeLinear(&asic->riceRead);

    if(asic->riceWrite.vir_addr != NULL)
        VPUFreeLinear(&asic->riceWrite);

    asic->internalImageLuma[0].vir_addr = NULL;
    asic->internalImageChroma[0].vir_addr = NULL;
    asic->internalImageLuma[1].vir_addr = NULL;
    asic->internalImageChroma[1].vir_addr = NULL;
    asic->sizeTbl.nal.vir_addr = NULL;
    asic->cabacCtx.vir_addr = NULL;
    asic->riceRead.vir_addr = NULL;
    asic->riceWrite.vir_addr = NULL;
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
RK_S32 EncAsicCheckStatus_V2(asicData_s * asic)
{
    RK_S32 ret;
    RK_U32 status;

    status = EncAsicGetStatus(&asic->regs);
	//printf("EncAsic: IRQstatus VALUE 0x%08x\n",status
    //								);
    ALOGD("EncAsic: IRQstatus VALUE 0x%08x\n",status);
    if(status & ASIC_STATUS_ERROR)
    {
        ret = ASIC_STATUS_ERROR;
    }
    else if(status & ASIC_STATUS_HW_RESET)
    {
        ret = ASIC_STATUS_HW_RESET;
    }
    else if(status & ASIC_STATUS_FRAME_READY)
    {
        regValues_s *regs = &asic->regs;

        EncAsicGetRegisters(asic->ewl, regs);

        ret = ASIC_STATUS_FRAME_READY;
    }
    else if(status & ASIC_STATUS_TEST_IRQ1){
		ret = ASIC_STATUS_ERROR;
        //regValues_s *regs = &asic->regs;

        //EncAsicGetRegisters(asic->ewl, regs);

        //ret = ASIC_STATUS_FRAME_READY;
		
		ALOGD("warnning:TEST_IRQ1,please check it");
	}else {
		ALOGD("warnning:buffer oveflow,please check it");
        ret = ASIC_STATUS_BUFF_FULL;
        /* we do not support recovery from buffer full situation so */
        /* ASIC has to be stopped                                   */
        //EncAsicStop(asic->ewl);
    }

//    EWLReleaseHw(asic->ewl);
    return ret;
}
