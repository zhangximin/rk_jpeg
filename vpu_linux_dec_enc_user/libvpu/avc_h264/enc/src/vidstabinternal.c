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
-  Description : Video stabilization standalone control
-
--------------------------------------------------------------------------------
-
-  Version control information, please leave untouched.
-
-  $RCSfile: vidstabinternal.c,v $
-  $Revision: 1.1 $
-  $Date: 2007/07/16 09:28:37 $
-
------------------------------------------------------------------------------*/
#include "basetype.h"
#include "vidstabcommon.h"
#include "vidstabinternal.h"
#include "vidstabcfg.h"
#include "ewl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ASIC_WAVE_TRACE_TRIGGER
extern i32 trigger_point;    /* picture which will be traced */
#endif

/* Mask fields */
#define mask_2b         (u32)0x00000003
#define mask_3b         (u32)0x00000007
#define mask_4b         (u32)0x0000000F
#define mask_5b         (u32)0x0000001F
#define mask_6b         (u32)0x0000003F
#define mask_11b        (u32)0x000007FF
#define mask_14b        (u32)0x00003FFF
#define mask_16b        (u32)0x0000FFFF

#define HSWREG(n)       ((n)*4)


/*------------------------------------------------------------------------------
    Function name   : VSCheckInput
    Description     : 
    Return type     : i32 
    Argument        : const VideoStbParam * param
------------------------------------------------------------------------------*/
i32 VSCheckInput(const VideoStbParam * param)
{
    /* Input picture minimum dimensions */
    if((param->inputWidth < 104) || (param->inputHeight < 104))
        return 1;

    /* Stabilized picture minimum  values */
    if((param->stabilizedWidth < 96) || (param->stabilizedHeight < 96))
        return 1;

    /* Stabilized dimensions multiple of 4 */
    if(((param->stabilizedWidth & 3) != 0) ||
       ((param->stabilizedHeight & 3) != 0))
        return 1;

    /* Edge >= 4 pixels */
    if((param->inputWidth < (param->stabilizedWidth + 8)) ||
       (param->inputHeight < (param->stabilizedHeight + 8)))
        return 1;

    /* stride 8 multiple */
    if((param->stride < param->inputWidth) || (param->stride & 7) != 0)
        return 1;

    /* input format */
    if(param->format > VIDEOSTB_BGR101010)
    {
        return 1;
    }

    return 0;
}

/*------------------------------------------------------------------------------
    Function name   : VSInitAsicCtrl
    Description     : 
    Return type     : void 
    Argument        : VideoStb * pVidStab
------------------------------------------------------------------------------*/
void VSInitAsicCtrl(VideoStb * pVidStab)
{
    RegValues *val = &pVidStab->regval;
    u32 *regMirror = pVidStab->regMirror;

    ASSERT(pVidStab != NULL);

    /* Initialize default values from defined configuration */
    val->asicCfgReg = 
        ((VS8270_AXI_READ_ID & (255)) << 24) |
        ((VS8270_AXI_WRITE_ID & (255)) << 16) |
        ((VS8270_BURST_LENGTH & (63)) << 8) |
        ((VS8270_BURST_INCR_TYPE_ENABLED & (1)) << 6) |
        ((VS8270_BURST_DATA_DISCARD_ENABLED & (1)) << 5) |
        ((VS8270_ASIC_CLOCK_GATING_ENABLED & (1)) << 4);

    val->irqDisable = VS8270_IRQ_DISABLE;
    val->rwStabMode = ASIC_VS_MODE_ALONE;

    memset(regMirror, 0, sizeof(regMirror));
}
#if 0
void H264encFlushRegs(RegValues * val)
{
	printf("   <%s>_%d   ycDaniel  \n", __func__, __LINE__);

    prinf("H264encFlushRegs\n");
        u32 registerlen = 0;

        if(val->vpuid == 0x30)
                registerlen = 164;
        else
                registerlen = ENC8270_REGISTERS;

    if (VPUClientSendReg(val->socket, val->regMirror, /*ENC8270_REGISTERS*/registerlen))
        prinf("H264encFlushRegs fail\n");
    else
        prinf("H264encFlushRegs success\n");
}
#endif

/*------------------------------------------------------------------------------
    Function name   : VSSetupAsicAll
    Description     : 
    Return type     : void 
    Argument        : VideoStb * pVidStab
------------------------------------------------------------------------------*/
void VSSetupAsicAll(VideoStb * pVidStab)
{

    const void *ewl = pVidStab->ewl;
    RegValues *val = &pVidStab->regval;
    u32 *regMirror = pVidStab->regMirror;

    regMirror[1] = ((val->irqDisable & 1) << 1);    /* clear IRQ status */

    /* system configuration */
    if (val->inputImageFormat < ASIC_INPUT_RGB565)      /* YUV input */
        regMirror[2] = val->asicCfgReg |
            ((VS8270_INPUT_SWAP_16_YUV & (1)) << 14) |
            ((VS8270_INPUT_SWAP_32_YUV & (1)) << 2) |
            (VS8270_INPUT_SWAP_8_YUV & (1));
    else if (val->inputImageFormat < ASIC_INPUT_RGB888) /* 16-bit RGB input */
        regMirror[2] = val->asicCfgReg |
            ((VS8270_INPUT_SWAP_16_RGB16 & (1)) << 14) |
            ((VS8270_INPUT_SWAP_32_RGB16 & (1)) << 2) |
            (VS8270_INPUT_SWAP_8_RGB16 & (1));
    else                                                /* 32-bit RGB input */
        regMirror[2] = val->asicCfgReg |
            ((VS8270_INPUT_SWAP_16_RGB32 & (1)) << 14) |
            ((VS8270_INPUT_SWAP_32_RGB32 & (1)) << 2) |
            (VS8270_INPUT_SWAP_8_RGB32 & (1));

    /* Input picture buffers */
    regMirror[11] = val->inputLumBase;

    /* Common control register, use INTRA mode */
    regMirror[14] = (val->mbsInRow << 19) | (val->mbsInCol << 10) | (1 << 3);

    /* PreP control */
    regMirror[15] =
        (val->inputLumaBaseOffset << 26) |
        (val->pixelsOnRow << 12) |
        (val->xFill << 10) | (val->yFill << 6) | (val->inputImageFormat << 2);

    regMirror[39] = val->rwNextLumaBase;
    regMirror[40] = val->rwStabMode << 30;

    regMirror[53] = ((val->colorConversionCoeffB & mask_16b) << 16) |
                     (val->colorConversionCoeffA & mask_16b);

    regMirror[54] = ((val->colorConversionCoeffE & mask_16b) << 16) |
                     (val->colorConversionCoeffC & mask_16b);

    regMirror[55] = ((val->bMaskMsb & mask_5b) << 26) |
                    ((val->gMaskMsb & mask_5b) << 21) |
                    ((val->rMaskMsb & mask_5b) << 16) |
                     (val->colorConversionCoeffF & mask_16b);

    /*{
        i32 i;

        for(i = 1; i <= 55; i++)
        {
            if(i != 26)
                EWLWriteReg(ewl, HSWREG(i), regMirror[i]);
        }
    }*/

    /* enable bit is written last */
    regMirror[14] |= ASIC_STATUS_ENABLE;

//    EWLEnableHW(ewl, HSWREG(14), regMirror[14]);
    H264encFlushRegs(val);
}

/*------------------------------------------------------------------------------
    Function name   : CheckAsicStatus
    Description     : 
    Return type     : i32 
    Argument        : u32 status
------------------------------------------------------------------------------*/
i32 CheckAsicStatus(u32 status)
{
    i32 ret;

    if(status & ASIC_STATUS_HW_RESET)
    {
        ret = VIDEOSTB_HW_RESET;
    }
    else if(status & ASIC_STATUS_FRAME_READY)
    {
        ret = VIDEOSTB_OK;
    }
    else
    {
        ret = VIDEOSTB_HW_BUS_ERROR;
    }

    return ret;
}

#if 0
/*------------------------------------------------------------------------------
    Function name   : VSWaitAsicReady
    Description     : 
    Return type     : i32 
    Argument        : VideoStb * pVidStab
------------------------------------------------------------------------------*/
i32 VSWaitAsicReady(VideoStb * pVidStab)
{
    const void *ewl = pVidStab->ewl;
    u32 *regMirror = pVidStab->regMirror;
    i32 ret;

    /* Wait for IRQ */
    ret = EWLWaitHwRdy(ewl);

    if(ret != EWL_OK)
    {
        if(ret == EWL_ERROR)
        {
            /* IRQ error => Stop and release HW */
            ret = VIDEOSTB_SYSTEM_ERROR;
        }
        else    /*if(ewl_ret == EWL_HW_WAIT_TIMEOUT) */
        {
            /* IRQ Timeout => Stop and release HW */
            ret = VIDEOSTB_HW_TIMEOUT;
        }

//        EWLEnableHW(ewl, HSWREG(14), 0);    /* make sure ASIC is OFF */
    }
    else
    {
        i32 i;

        regMirror[1] = EWLReadReg(ewl, HSWREG(1));  /* IRQ status */
        for(i = 40; i <= 50; i++)
        {
            regMirror[i] = EWLReadReg(ewl, HSWREG(i));  /* VS results */
        }

        ret = CheckAsicStatus(regMirror[1]);
    }

    return ret;
}
#endif

/*------------------------------------------------------------------------------
    Function name   : VSSetCropping
    Description     : 
    Return type     : void 
    Argument        : VideoStb * pVidStab
    Argument        : u32 currentPictBus
    Argument        : u32 nextPictBus
------------------------------------------------------------------------------*/
void VSSetCropping(VideoStb * pVidStab, u32 currentPictBus, u32 nextPictBus)
{
    u32 byteOffsetCurrent;
    u32 width, height;
    RegValues *regs;

    ASSERT(pVidStab != NULL && currentPictBus != 0 && nextPictBus != 0);

    regs = &pVidStab->regval;

    regs->inputLumBase = currentPictBus;
    regs->rwNextLumaBase = nextPictBus;

    /* RGB conversion coefficients for RGB input */
    if (pVidStab->yuvFormat >= 4) {
        regs->colorConversionCoeffA = 19589;
        regs->colorConversionCoeffB = 38443;
        regs->colorConversionCoeffC = 7504;
        regs->colorConversionCoeffE = 37008;
        regs->colorConversionCoeffF = 46740;
    }

    /* Setup masks to separate R, G and B from RGB */
    switch ((i32)pVidStab->yuvFormat)
    {
        case 4: /* RGB565 */
            regs->rMaskMsb = 15;
            regs->gMaskMsb = 10;
            regs->bMaskMsb = 4;
            break;
        case 5: /* BGR565 */
            regs->bMaskMsb = 15;
            regs->gMaskMsb = 10;
            regs->rMaskMsb = 4;
            break;
        case 6: /* RGB555 */
            regs->rMaskMsb = 14;
            regs->gMaskMsb = 9;
            regs->bMaskMsb = 4;
            break;
        case 7: /* BGR555 */
            regs->bMaskMsb = 14;
            regs->gMaskMsb = 9;
            regs->rMaskMsb = 4;
            break;
        case 8: /* RGB444 */
            regs->rMaskMsb = 11;
            regs->gMaskMsb = 7;
            regs->bMaskMsb = 3;
            break;
        case 9: /* BGR444 */
            regs->bMaskMsb = 11;
            regs->gMaskMsb = 7;
            regs->rMaskMsb = 3;
            break;
        case 10: /* RGB888 */
            regs->rMaskMsb = 23;
            regs->gMaskMsb = 15;
            regs->bMaskMsb = 7;
            break;
        case 11: /* BGR888 */
            regs->bMaskMsb = 23;
            regs->gMaskMsb = 15;
            regs->rMaskMsb = 7;
            break;
        case 12: /* RGB101010 */
            regs->rMaskMsb = 29;
            regs->gMaskMsb = 19;
            regs->bMaskMsb = 9;
            break;
        case 13: /* BGR101010 */
            regs->bMaskMsb = 29;
            regs->gMaskMsb = 19;
            regs->rMaskMsb = 9;
            break;
        default:
            /* No masks needed for YUV format */
            regs->rMaskMsb = regs->gMaskMsb = regs->bMaskMsb = 0;
    }

    if (pVidStab->yuvFormat <= 3)
        regs->inputImageFormat = pVidStab->yuvFormat;       /* YUV */
    else if (pVidStab->yuvFormat <= 5)
        regs->inputImageFormat = ASIC_INPUT_RGB565;         /* 16-bit RGB */
    else if (pVidStab->yuvFormat <= 7)
        regs->inputImageFormat = ASIC_INPUT_RGB555;         /* 15-bit RGB */
    else if (pVidStab->yuvFormat <= 9)
        regs->inputImageFormat = ASIC_INPUT_RGB444;         /* 12-bit RGB */
    else if (pVidStab->yuvFormat <= 11)
        regs->inputImageFormat = ASIC_INPUT_RGB888;         /* 24-bit RGB */
    else
        regs->inputImageFormat = ASIC_INPUT_RGB101010;      /* 30-bit RGB */


    regs->pixelsOnRow = pVidStab->stride;

    /* cropping */

    /* Current image position */
    byteOffsetCurrent = pVidStab->data.stabOffsetY;
    byteOffsetCurrent *= pVidStab->stride;
    byteOffsetCurrent += pVidStab->data.stabOffsetX;

    if(pVidStab->yuvFormat >=2 && pVidStab->yuvFormat <= 9)    /* YUV 422 / RGB 16bpp */
    {
        byteOffsetCurrent *= 2;
    }
    else if(pVidStab->yuvFormat > 9)    /* RGB 32bpp */
    {
        byteOffsetCurrent *= 4;
    }

    regs->inputLumBase += (byteOffsetCurrent & (~7));
    if(pVidStab->yuvFormat >= 10)
    {
        /* Note: HW does the cropping AFTER RGB to YUYV conversion
         * so the offset is calculated using 16bpp */
        regs->inputLumaBaseOffset = (byteOffsetCurrent & 7)/2;
    }
    else
    {
        regs->inputLumaBaseOffset = (byteOffsetCurrent & 7);
    }

    /* next picture's offset same as above */
    regs->rwNextLumaBase += (byteOffsetCurrent & (~7));

    /* source image setup, size and fill */
    width = pVidStab->data.stabilizedWidth;
    height = pVidStab->data.stabilizedHeight;

    /* Set stabilized picture dimensions */
    regs->mbsInRow = (width + 15) / 16;
    regs->mbsInCol = (height + 15) / 16;

    /* Set the overfill values */
    if(width & 0x0F)
        regs->xFill = (16 - (width & 0x0F)) / 4;
    else
        regs->xFill = 0;

    if(height & 0x0F)
        regs->yFill = 16 - (height & 0x0F);
    else
        regs->yFill = 0;

    return;
}
