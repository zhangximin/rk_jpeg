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
--  Description : PP API's internal functions.
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: ppinternal.c,v $
--  $Date: 2010/04/07 11:20:37 $
--  $Revision: 1.57 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1.  Include headers
------------------------------------------------------------------------------*/

#include "vpu_type.h"
#include "ppapi.h"
#include "ppinternal.h"
#include "dwl.h"
#include "regdrv.h"
#include "ppdebug.h"
#include "ppcfg.h"
#include "decapicommon.h"

static void PPSetFrmBufferWriting(PPContainer * ppC);
static void PPSetRgbBitmask(PPContainer * ppC);
static void PPSetRgbTransformCoeffs(PPContainer * ppC);
static void PPSetDithering(PPContainer * ppC);

static RK_U32 PPIsInPixFmtOk(RK_U32 pix_fmt, const PPContainer * ppC);
static RK_U32 PPIsOutPixFmtOk(RK_U32 pix_fmt, const PPContainer * ppC);

static RK_S32 PPCheckAllWidthParams(PPConfig * ppCfg, RK_U32 blendEna);
static RK_S32 PPCheckAllHeightParams(PPConfig * ppCfg);

static RK_U32 PPFindFirstNonZeroBit(RK_U32 mask);
static void PPSetRgbBitmaskCustom(PPContainer * ppC, RK_U32 rgb16);

static RK_S32 PPContinuousCheck(RK_U32 value);
static RK_S32 PPCheckOverlapping(RK_U32 a, RK_U32 b, RK_U32 c, RK_U32 d);

static RK_U32 PPCountOnes(RK_U32 value);
static RK_U32 PPSelectDitheringValue(RK_U32 mask);

#if (PP_X170_DATA_BUS_WIDTH != 4) && (PP_X170_DATA_BUS_WIDTH != 8)
#error "Bad data bus width specified PP_X170_DATA_BUS_WIDTH"
#endif

/*------------------------------------------------------------------------------
    Function name   : PPGetStatus
    Description     :
    Return type     : RK_U32
    Argument        : PPContainer *ppC
------------------------------------------------------------------------------*/
RK_U32 PPGetStatus(const PPContainer * ppC)
{
    ASSERT(ppC != NULL);
    return ppC->status;
}

/*------------------------------------------------------------------------------
    Function name   : PPSetStatus
    Description     :
    Return type     : void
    Argument        : PPContainer *ppC
    Argument        : RK_U32 status
------------------------------------------------------------------------------*/
void PPSetStatus(PPContainer * ppC, RK_U32 status)
{
    ASSERT(ppC != NULL);
    ppC->status = status;
}

/*------------------------------------------------------------------------------
    Function name   : PPRefreshRegs
    Description     :
    Return type     : void
    Argument        : PPContainer * ppC
------------------------------------------------------------------------------*/
void PPRefreshRegs(PPContainer * ppC)
{
    RK_S32 i;
    RK_U32 offset = PP_X170_REG_START;

    RK_U32 *ppRegs = ppC->ppRegs;

    for(i = PP_X170_REGISTERS; i > 0; i--)
    {
        *ppRegs++ = DWLReadReg(ppC->dwl, offset);
        offset += 4;
    }
}

/*------------------------------------------------------------------------------
    Function name   : PPFlushRegs
    Description     :
    Return type     : void
    Argument        : PPContainer * ppC
------------------------------------------------------------------------------*/
void PPFlushRegs(PPContainer * ppC)
{
    RK_S32 i;
    RK_U32 offset = PP_X170_REG_START;
    RK_U32 *ppRegs = ppC->ppRegs;

    for(i = PP_X170_REGISTERS; i > 0; i--)
    {
        DWLWriteReg(ppC->dwl, offset, *ppRegs);
        ppRegs++;
        offset += 4;
    }
}

/*------------------------------------------------------------------------------
    Function name   : PPInitHW
    Description     :
    Return type     : void
    Argument        : PPContainer * ppC
------------------------------------------------------------------------------*/
void PPInitHW(PPContainer * ppC)
{
    RK_U32 *ppRegs = ppC->ppRegs;

    memset(ppRegs, 0, PP_X170_REGISTERS * sizeof(*ppRegs));

#if( PP_X170_USING_IRQ == 0 )
    SetPpRegister(ppRegs, HWIF_PP_IRQ_DIS, 1);
#endif

#if (PP_X170_OUTPUT_PICTURE_ENDIAN > 1)
#error "Bad value specified for PP_X170_OUTPUT_PICTURE_ENDIAN"
#endif

#if (PP_X170_INPUT_PICTURE_ENDIAN > 1)
#error "Bad value specified for PP_X170_INPUT_PICTURE_ENDIAN"
#endif

#if (PP_X170_BUS_BURST_LENGTH > 31)
#error "Bad value specified for PP_X170_BUS_BURST_LENGTH"
#endif

    SetPpRegister(ppRegs, HWIF_PP_IN_ENDIAN, PP_X170_INPUT_PICTURE_ENDIAN);
    SetPpRegister(ppRegs, HWIF_PP_OUT_ENDIAN, PP_X170_OUTPUT_PICTURE_ENDIAN);
    SetPpRegister(ppRegs, HWIF_PP_MAX_BURST, PP_X170_BUS_BURST_LENGTH);

#if ( PP_X170_DATA_DISCARD_ENABLE != 0 )
    SetPpRegister(ppRegs, HWIF_PP_DATA_DISC_E, 1);
#else
    SetPpRegister(ppRegs, HWIF_PP_DATA_DISC_E, 0);
#endif

#if ( PP_X170_SWAP_32_WORDS != 0 )
    SetPpRegister(ppRegs, HWIF_PP_OUT_SWAP32_E, 1);
#else
    SetPpRegister(ppRegs, HWIF_PP_OUT_SWAP32_E, 0);
#endif

#if ( PP_X170_SWAP_32_WORDS_INPUT != 0 )
    SetPpRegister(ppRegs, HWIF_PP_IN_SWAP32_E, 1);
    SetPpRegister(ppRegs, HWIF_PP_IN_A1_SWAP32, 1);
#else
    SetPpRegister(ppRegs, HWIF_PP_IN_SWAP32_E, 0);
    SetPpRegister(ppRegs, HWIF_PP_IN_A1_SWAP32, 0);
#endif

#if ( PP_X170_INTERNAL_CLOCK_GATING != 0 )
    SetPpRegister(ppRegs, HWIF_PP_CLK_GATE_E, 1);
#else
    SetPpRegister(ppRegs, HWIF_PP_CLK_GATE_E, 0);
#endif

    /* set AXI RW IDs */
    SetPpRegister(ppRegs, HWIF_PP_AXI_RD_ID, (PP_X170_AXI_ID_R & 0xFFU));
    SetPpRegister(ppRegs, HWIF_PP_AXI_WR_ID, (PP_X170_AXI_ID_W & 0xFFU));

    SetPpRegister(ppRegs, HWIF_PP_SCMD_DIS, PP_X170_SCMD_DISABLE);

    /* use alpha blend source 1 endian mode for alpha blend source 2 */
    SetPpRegister(ppC->ppRegs, HWIF_PP_IN_A2_ENDSEL, 1);

    return;
}

/*------------------------------------------------------------------------------
    Function name   : PPInitDataStructures
    Description     :
    Return type     : void
    Argument        : PPContainer * ppC
------------------------------------------------------------------------------*/
void PPInitDataStructures(PPContainer * ppC)
{
    PPOutImage *ppOutImg;
    PPInImage *ppInImg;

    PPOutRgb *ppOutRgb;

    ASSERT(ppC != NULL);

    memset(&ppC->ppCfg, 0, sizeof(PPConfig));

    ppOutImg = &ppC->ppCfg.ppOutImg;
    ppInImg = &ppC->ppCfg.ppInImg;

    ppOutRgb = &ppC->ppCfg.ppOutRgb;

    ppInImg->width = 720;
    ppInImg->height = 576;
    ppInImg->pixFormat = PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR;

    ppC->inFormat = PP_ASIC_IN_FORMAT_420_SEMIPLANAR;

    ppOutImg->width = 720;
    ppOutImg->height = 576;
    ppOutImg->pixFormat = PP_PIX_FMT_RGB32;

    ppC->outFormat = PP_ASIC_OUT_FORMAT_RGB;
    ppC->rgbDepth = 32;

    ppOutRgb->rgbTransform = PP_YCBCR2RGB_TRANSFORM_BT_601;

    ppOutRgb->rgbTransformCoeffs.a = 298;
    ppOutRgb->rgbTransformCoeffs.b = 409;
    ppOutRgb->rgbTransformCoeffs.c = 208;
    ppOutRgb->rgbTransformCoeffs.d = 100;
    ppOutRgb->rgbTransformCoeffs.e = 516;

    ppC->frmBufferLumaOrRgbOffset = 0;
    ppC->frmBufferChromaOffset = 0;

}

/*------------------------------------------------------------------------------
    Function name   : PPSetupHW
    Description     :
    Return type     : void
    Argument        : PPContainer * ppC
------------------------------------------------------------------------------*/
void PPSetupHW(PPContainer * ppC)
{

    PPOutMask1 *ppOutMask1;
    PPOutMask2 *ppOutMask2;
    PPOutImage *ppOutImg;
    PPInImage *ppInImg;
    PPInCropping *ppInCrop;
    PPOutDeinterlace *ppOutDeint;
    PPOutRgb *ppOutRgb;

    PPInRotation *ppInRot;

    RK_U32 *ppRegs;

    ASSERT(ppC != NULL);

    ppOutMask1 = &ppC->ppCfg.ppOutMask1;
    ppOutMask2 = &ppC->ppCfg.ppOutMask2;
    ppOutImg = &ppC->ppCfg.ppOutImg;
    ppInImg = &ppC->ppCfg.ppInImg;
    ppInCrop = &ppC->ppCfg.ppInCrop;
    ppOutDeint = &ppC->ppCfg.ppOutDeinterlace;

    ppInRot = &ppC->ppCfg.ppInRotation;
    ppOutRgb = &ppC->ppCfg.ppOutRgb;

    ppRegs = ppC->ppRegs;
    /* frame buffer setup */
    PPSetFrmBufferWriting(ppC);

    /* output buffer setup */
    SetPpRegister(ppRegs, HWIF_PP_OUT_LU_BASE,
                  (RK_U32) (ppOutImg->bufferBusAddr +
                         ppC->frmBufferLumaOrRgbOffset));

    /* chromas not needed for RGB and YUYV 422 out */
    if(ppOutImg->pixFormat == PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR)
    {
        SetPpRegister(ppRegs, HWIF_PP_OUT_CH_BASE,
                      (RK_U32) (ppOutImg->bufferChromaBusAddr +
                             ppC->frmBufferChromaOffset));
    }

    SetPpRegister(ppRegs, HWIF_PP_OUT_FORMAT, ppC->outFormat);
    if(ppC->outFormat == PP_ASIC_OUT_FORMAT_422)
    {
        SetPpRegister(ppRegs, HWIF_PP_OUT_START_CH, ppC->outStartCh);
        SetPpRegister(ppRegs, HWIF_PP_OUT_CR_FIRST, ppC->outCrFirst);
    }
    SetPpRegister(ppRegs, HWIF_PP_OUT_WIDTH, ppOutImg->width);
    SetPpRegister(ppRegs, HWIF_PP_OUT_HEIGHT, ppOutImg->height);

    /* deinterlacing parameters */
    SetPpRegister(ppRegs, HWIF_DEINT_E, ppOutDeint->enable);

    if(ppOutDeint->enable)
    {
        /* deinterlacing default parameters */
        SetPpRegister(ppRegs, HWIF_DEINT_BLEND_E, 0);
        SetPpRegister(ppRegs, HWIF_DEINT_THRESHOLD, 25);
        SetPpRegister(ppRegs, HWIF_DEINT_EDGE_DET, 25);
    }

    /* input setup */
    if(ppC->decInst == NULL)
    {
        SetPpRegister(ppRegs, HWIF_PP_IN_STRUCT, ppInImg->picStruct);

        if(ppInImg->picStruct != PP_PIC_BOT_FIELD &&
           ppInImg->picStruct != PP_PIC_BOT_FIELD_FRAME)
        {
            SetPpRegister(ppRegs, HWIF_PP_IN_LU_BASE, ppInImg->bufferBusAddr);
            SetPpRegister(ppRegs, HWIF_PP_IN_CB_BASE, ppInImg->bufferCbBusAddr);
        }

        if(ppInImg->picStruct != PP_PIC_FRAME_OR_TOP_FIELD &&
           ppInImg->picStruct != PP_PIC_TOP_FIELD_FRAME)
        {
            SetPpRegister(ppRegs, HWIF_PP_BOT_YIN_BASE,
                          ppInImg->bufferBusAddrBot);
            SetPpRegister(ppRegs, HWIF_PP_BOT_CIN_BASE,
                          ppInImg->bufferBusAddrChBot);
        }

        if(ppInImg->pixFormat == PP_PIX_FMT_YCBCR_4_2_0_PLANAR)
        {
            SetPpRegister(ppRegs, HWIF_PP_IN_CR_BASE, ppInImg->bufferCrBusAddr);
        }
    }

    SetPpRegister(ppRegs, HWIF_EXT_ORIG_WIDTH, (ppInImg->width + 15) / 16);
    if(ppC->inFormat < PP_ASIC_IN_FORMAT_EXTENSION)
        SetPpRegister(ppRegs, HWIF_PP_IN_FORMAT, ppC->inFormat);
    else
    {
        SetPpRegister(ppRegs, HWIF_PP_IN_FORMAT, PP_ASIC_IN_FORMAT_EXTENSION);
        SetPpRegister(ppRegs, HWIF_PP_IN_FORMAT_ES,
                      ppC->inFormat - PP_ASIC_IN_FORMAT_EXTENSION);
    }

    if(ppC->inFormat == PP_ASIC_IN_FORMAT_422)
    {
        SetPpRegister(ppRegs, HWIF_PP_IN_START_CH, ppC->inStartCh);
        SetPpRegister(ppRegs, HWIF_PP_IN_CR_FIRST, ppC->inCrFirst);
    }

    if(!ppInCrop->enable)
    {
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_W_EXT,
                      (((ppInImg->width / 16) & 0xE00) >> 9));
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_WIDTH,
                      ((ppInImg->width / 16) & 0x1FF));
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_H_EXT,
                      (((ppInImg->height / 16) & 0x700) >> 8));
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_HEIGHT,
                      ((ppInImg->height / 16) & 0x0FF));
        SetPpRegister(ppRegs, HWIF_CROP_STARTX_EXT, 0);
		SetPpRegister(ppRegs, HWIF_CROP_STARTX, 0);
        SetPpRegister(ppRegs, HWIF_CROP_STARTY_EXT, 0);
		SetPpRegister(ppRegs, HWIF_CROP_STARTY, 0);
        SetPpRegister(ppRegs, HWIF_PP_CROP8_R_E, 0);
        SetPpRegister(ppRegs, HWIF_PP_CROP8_D_E, 0);

        ppC->inWidth = ppInImg->width;
        ppC->inHeight = ppInImg->height;
    }
    else
    {
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_W_EXT,
                      ((((ppInCrop->width + 15) / 16) & 0xE00) >> 9));
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_WIDTH,
                      (((ppInCrop->width + 15) / 16) & 0x1FF));
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_H_EXT,
                      ((((ppInCrop->height + 15) / 16) & 0x700) >> 8));
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_HEIGHT,
                      (((ppInCrop->height + 15) / 16) & 0x0FF));
        SetPpRegister(ppRegs, HWIF_CROP_STARTX_EXT,
                      (((ppInCrop->originX / 16) & 0xE00) >> 9));
        SetPpRegister(ppRegs, HWIF_CROP_STARTX,
                      ((ppInCrop->originX / 16) & 0x1FF));
        SetPpRegister(ppRegs, HWIF_CROP_STARTY_EXT,
                      (((ppInCrop->originY / 16) & 0x700) >> 8));
        SetPpRegister(ppRegs, HWIF_CROP_STARTY,
                      ((ppInCrop->originY / 16) & 0x0FF));

        if(ppInCrop->width & 0x0F)
        {
            SetPpRegister(ppC->ppRegs, HWIF_PP_CROP8_R_E, 1);
        }
        else
        {
            SetPpRegister(ppC->ppRegs, HWIF_PP_CROP8_R_E, 0);
        }

        if(ppInCrop->height & 0x0F)
        {
            SetPpRegister(ppC->ppRegs, HWIF_PP_CROP8_D_E, 1);
        }
        else
        {
            SetPpRegister(ppC->ppRegs, HWIF_PP_CROP8_D_E, 0);
        }

        ppC->inWidth = ppInCrop->width;
        ppC->inHeight = ppInCrop->height;
    }

    /* setup scaling */
    PPSetupScaling(ppC, ppOutImg);

    /* YUV range */
    SetPpRegister(ppRegs, HWIF_YCBCR_RANGE, ppInImg->videoRange);

    if(ppOutImg->pixFormat & PP_PIXEL_FORMAT_RGB_MASK)
    {
        /* setup RGB conversion */
        PPSetRgbTransformCoeffs(ppC);

        if(ppOutRgb->ditheringEnable)
        {
            PPSetDithering(ppC);
        }
        /* setup RGB bitmasks */
        PPSetRgbBitmask(ppC);

    }

    if(ppC->decInst != NULL)
    {
        /* set up range expansion/mapping */
        if(ppInImg->vc1RangeRedFrm)
        {
            SetPpRegister(ppC->ppRegs, HWIF_RANGEMAP_Y_E, 1);
            SetPpRegister(ppC->ppRegs, HWIF_RANGEMAP_COEF_Y, 7 + 9);
            SetPpRegister(ppC->ppRegs, HWIF_RANGEMAP_C_E, 1);
            SetPpRegister(ppC->ppRegs, HWIF_RANGEMAP_COEF_C, 7 + 9);
        }
        else
        {
            SetPpRegister(ppC->ppRegs, HWIF_RANGEMAP_Y_E,
                          ppInImg->vc1RangeMapYEnable);
            SetPpRegister(ppC->ppRegs, HWIF_RANGEMAP_COEF_Y,
                          ppInImg->vc1RangeMapYCoeff + 9);
            SetPpRegister(ppC->ppRegs, HWIF_RANGEMAP_C_E,
                          ppInImg->vc1RangeMapCEnable);
            SetPpRegister(ppC->ppRegs, HWIF_RANGEMAP_COEF_C,
                          ppInImg->vc1RangeMapCCoeff + 9);
        }
        /* for pipeline, this is set up in PipelineStart */

    }

    /* setup rotation/flip */
    SetPpRegister(ppRegs, HWIF_ROTATION_MODE, ppInRot->rotation);

    /* setup masks */
    SetPpRegister(ppRegs, HWIF_MASK1_E, ppOutMask1->enable);

    /* Alpha blending mask 1 */
    if(ppOutMask1->enable && ppOutMask1->alphaBlendEna && ppC->blendEna)
    {
        SetPpRegister(ppRegs, HWIF_MASK1_ABLEND_E, 1);
        SetPpRegister(ppRegs, HWIF_ABLEND1_BASE,
                      ppOutMask1->blendComponentBase);
    }
    else
    {
        SetPpRegister(ppRegs, HWIF_MASK1_ABLEND_E, 0);
    }

    if(ppOutMask1->enable)
    {
        RK_U32 startX, startY;
        RK_S32 endX, endY;

        if(ppOutMask1->originX < 0)
        {
            startX = 0;
        }
        else if(ppOutMask1->originX > (RK_S32) ppOutImg->width)
        {
            startX = ppOutImg->width;
        }
        else
        {
            startX = (RK_U32) ppOutMask1->originX;
        }

        SetPpRegister(ppRegs, HWIF_MASK1_STARTX, startX);

        if(ppOutMask1->originY < 0)
        {
            startY = 0;
        }
        else if(ppOutMask1->originY > (RK_S32) ppOutImg->height)
        {
            startY = ppOutImg->height;
        }
        else
        {
            startY = (RK_U32) ppOutMask1->originY;
        }

        SetPpRegister(ppRegs, HWIF_MASK1_STARTY, startY);

        endX = ppOutMask1->originX + (RK_S32) ppOutMask1->width;
        if(endX > (RK_S32) ppOutImg->width)
        {
            endX = (RK_S32) ppOutImg->width;
        }
        else if(endX < 0)
        {
            endX = 0;
        }

        SetPpRegister(ppRegs, HWIF_MASK1_ENDX, (RK_U32) endX);

        endY = ppOutMask1->originY + (RK_S32) ppOutMask1->height;
        if(endY > (RK_S32) ppOutImg->height)
        {
            endY = (RK_S32) ppOutImg->height;
        }
        else if(endY < 0)
        {
            endY = 0;
        }

        SetPpRegister(ppRegs, HWIF_MASK1_ENDY, (RK_U32) endY);
    }

    SetPpRegister(ppRegs, HWIF_MASK2_E, ppOutMask2->enable);

    /* Alpha blending mask 2 */
    if(ppOutMask2->enable && ppOutMask2->alphaBlendEna && ppC->blendEna)
    {
        SetPpRegister(ppRegs, HWIF_MASK2_ABLEND_E, 1);
        SetPpRegister(ppRegs, HWIF_ABLEND2_BASE,
                      ppOutMask2->blendComponentBase);
    }
    else
    {
        SetPpRegister(ppRegs, HWIF_MASK2_ABLEND_E, 0);
    }

    if(ppOutMask2->enable)
    {
        RK_U32 startX, startY;
        RK_S32 endX, endY;

        if(ppOutMask2->originX < 0)
        {
            startX = 0;
        }
        else if(ppOutMask2->originX > (RK_S32) ppOutImg->width)
        {
            startX = ppOutImg->width;
        }
        else
        {
            startX = (RK_U32) ppOutMask2->originX;
        }

        SetPpRegister(ppRegs, HWIF_MASK2_STARTX, startX);

        if(ppOutMask2->originY < 0)
        {
            startY = 0;
        }
        else if(ppOutMask2->originY > (RK_S32) ppOutImg->height)
        {
            startY = ppOutImg->height;
        }
        else
        {
            startY = (RK_U32) ppOutMask2->originY;
        }

        SetPpRegister(ppRegs, HWIF_MASK2_STARTY, startY);

        endX = ppOutMask2->originX + (RK_S32) ppOutMask2->width;
        if(endX > (RK_S32) ppOutImg->width)
        {
            endX = (RK_S32) ppOutImg->width;
        }
        else if(endX < 0)
        {
            endX = 0;
        }

        SetPpRegister(ppRegs, HWIF_MASK2_ENDX, (RK_U32) endX);

        endY = ppOutMask2->originY + (RK_S32) ppOutMask2->height;
        if(endY > (RK_S32) ppOutImg->height)
        {
            endY = (RK_S32) ppOutImg->height;
        }
        else if(endY < 0)
        {
            endY = 0;
        }

        SetPpRegister(ppRegs, HWIF_MASK2_ENDY, (RK_U32) endY);
    }

}

/*------------------------------------------------------------------------------
    Function name   : PPCheckConfig
    Description     :
    Return type     : RK_S32
    Argument        : PPConfig * ppCfg
    Argument        : RK_U32 pipeline
    Argument        : RK_U32 decType
------------------------------------------------------------------------------*/
RK_S32 PPCheckConfig(PPContainer * ppC, PPConfig * ppCfg,
                  RK_U32 decLinked, RK_U32 decType)
{
    PPOutImage *ppOutImg;
    PPInImage *ppInImg;
    PPInCropping *ppInCrop;
    PPOutRgb *ppOutRgb;
    PPOutFrameBuffer *ppOutFrmBuffer;
    PPInRotation *ppInRotation;
    PPOutDeinterlace *ppOutDeint;

    PPOutMask1 *ppOutMask1;
    PPOutMask2 *ppOutMask2;

    const RK_U32 address_mask = (PP_X170_DATA_BUS_WIDTH - 1);

    ASSERT(ppCfg != NULL);

    ppOutImg = &ppCfg->ppOutImg;
    ppInImg = &ppCfg->ppInImg;
    ppInCrop = &ppCfg->ppInCrop;
    ppOutRgb = &ppCfg->ppOutRgb;
    ppOutFrmBuffer = &ppCfg->ppOutFrmBuffer;
    ppInRotation = &ppCfg->ppInRotation;
    ppOutDeint = &ppCfg->ppOutDeinterlace;

    ppOutMask1 = &ppCfg->ppOutMask1;
    ppOutMask2 = &ppCfg->ppOutMask2;

    /* PPInImage check */

    if(!PPIsInPixFmtOk(ppInImg->pixFormat, ppC))
    {
        return (RK_S32) PP_SET_IN_FORMAT_INVALID;
    }

    if(!decLinked)
    {
        if(ppInImg->picStruct != PP_PIC_BOT_FIELD &&
           ppInImg->picStruct != PP_PIC_BOT_FIELD_FRAME)
        {
            if((ppInImg->bufferBusAddr == 0) ||
               (ppInImg->bufferBusAddr & address_mask))
            {
                return (RK_S32) PP_SET_IN_ADDRESS_INVALID;
            }

            if(ppInImg->pixFormat & PP_PIXEL_FORMAT_YUV420_MASK)
            {
                if(ppInImg->bufferCbBusAddr == 0 ||
                   (ppInImg->bufferCbBusAddr & address_mask))
                    return (RK_S32) PP_SET_IN_ADDRESS_INVALID;
            }

            if(ppInImg->pixFormat == PP_PIX_FMT_YCBCR_4_2_0_PLANAR)
            {
                if(ppInImg->bufferCrBusAddr == 0 ||
                   (ppInImg->bufferCrBusAddr & address_mask))
                    return (RK_S32) PP_SET_IN_ADDRESS_INVALID;
            }
        }
        if(ppInImg->picStruct != PP_PIC_FRAME_OR_TOP_FIELD &&
           ppInImg->picStruct != PP_PIC_TOP_FIELD_FRAME)
        {
            if((ppInImg->bufferBusAddrBot == 0) ||
               (ppInImg->bufferBusAddrBot & address_mask))
            {
                return (RK_S32) PP_SET_IN_ADDRESS_INVALID;
            }

            if(ppInImg->pixFormat & PP_PIXEL_FORMAT_YUV420_MASK)
            {
                if(ppInImg->bufferBusAddrChBot == 0 ||
                   (ppInImg->bufferBusAddrChBot & address_mask))
                    return (RK_S32) PP_SET_IN_ADDRESS_INVALID;
            }
        }
    }

    if(ppC->hwId == 0x8170U)
    {
        if((ppInImg->width < PP_IN_MIN_WIDTH(decLinked)) ||
           (ppInImg->height < PP_IN_MIN_HEIGHT(decLinked)) ||
           (ppInImg->width > PP_IN_MAX_WIDTH(decLinked)) ||
           (ppInImg->height > PP_IN_MAX_HEIGHT(decLinked)) ||
           (ppInImg->width & PP_IN_DIVISIBILITY(decLinked)) ||
           (ppInImg->height & PP_IN_DIVISIBILITY(decLinked)))
        {
            return (RK_S32) PP_SET_IN_SIZE_INVALID;
        }
    }
    else
    {
        if((ppInImg->width < PP_IN_MIN_WIDTH(decLinked)) ||
           (ppInImg->height < PP_IN_MIN_HEIGHT(decLinked)) ||
           (ppInImg->width > PP_IN_MAX_WIDTH_EXT(decLinked)) ||
           (ppInImg->height > PP_IN_MAX_HEIGHT_EXT(decLinked)) ||
           (ppInImg->width & PP_IN_DIVISIBILITY(decLinked)) ||
           (ppInImg->height & PP_IN_DIVISIBILITY(decLinked)))
        {
            return (RK_S32) PP_SET_IN_SIZE_INVALID;
        }
    }

    if(ppInImg->picStruct > PP_PIC_BOT_FIELD_FRAME)
    {
        return (RK_S32) PP_SET_IN_STRUCT_INVALID;
    }
    else if(ppInImg->picStruct != PP_PIC_FRAME_OR_TOP_FIELD &&
            ppInImg->pixFormat != PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR &&
            ppInImg->pixFormat != PP_PIX_FMT_YCBCR_4_2_2_INTERLEAVED &&
            ppInImg->pixFormat != PP_PIX_FMT_YCRYCB_4_2_2_INTERLEAVED &&
            ppInImg->pixFormat != PP_PIX_FMT_CBYCRY_4_2_2_INTERLEAVED &&
            ppInImg->pixFormat != PP_PIX_FMT_CRYCBY_4_2_2_INTERLEAVED)
    {
        return (RK_S32) PP_SET_IN_STRUCT_INVALID;
    }

    /* cropping check */
    if(ppInCrop->enable != 0)
    {
        if((ppInCrop->width < PP_IN_MIN_WIDTH(decLinked)) ||
           (ppInCrop->height < PP_IN_MIN_HEIGHT(decLinked)) ||
           (ppInCrop->width > ppInImg->width) ||
           (ppInCrop->originX > ppInImg->width) ||
           (ppInCrop->height > ppInImg->height) ||
           (ppInCrop->originY > ppInImg->height) ||
           (ppInCrop->width & 0x07) ||
           (ppInCrop->height & 0x07) ||
           (ppInCrop->originX & 0x0F) || (ppInCrop->originY & 0x0F))
        {
            return (RK_S32) PP_SET_CROP_INVALID;
        }
#if 0
        /* when deinterlacing the cropped size has to be 16 multiple */
        if(ppCfg->ppOutDeinterlace.enable &&
           ((ppInCrop->width & 0x0F) || (ppInCrop->height & 0x0F)))
        {
            return (RK_S32) PP_SET_CROP_INVALID;
        }
#endif
    }
    /* check rotation */
    switch (ppInRotation->rotation)
    {
    case PP_ROTATION_NONE:
    case PP_ROTATION_RIGHT_90:
    case PP_ROTATION_LEFT_90:
    case PP_ROTATION_HOR_FLIP:
    case PP_ROTATION_VER_FLIP:
    case PP_ROTATION_180:
        break;
    default:
        return (RK_S32) PP_SET_ROTATION_INVALID;
    }

    /* jpeg dec linked, rotation not supported in 440, 422, 411 and 444 */
    if(decLinked != 0 && ppInRotation->rotation != PP_ROTATION_NONE &&
       (ppInImg->pixFormat == PP_PIX_FMT_YCBCR_4_4_0 ||
        ppInImg->pixFormat == PP_PIX_FMT_YCBCR_4_2_2_SEMIPLANAR ||
        ppInImg->pixFormat == PP_PIX_FMT_YCBCR_4_1_1_SEMIPLANAR ||
        ppInImg->pixFormat == PP_PIX_FMT_YCBCR_4_4_4_SEMIPLANAR))
    {
        return (RK_S32) PP_SET_ROTATION_INVALID;
    }

    /* rotation not supported in jpeg 400 but supported in h264 */
    if(decLinked != 0 && decType == PP_PIPELINED_DEC_TYPE_JPEG &&
       ppInImg->pixFormat == PP_PIX_FMT_YCBCR_4_0_0 &&
       ppInRotation->rotation != PP_ROTATION_NONE)
    {
        return (RK_S32) PP_SET_ROTATION_INVALID;
    }

    if(ppInImg->videoRange > 1)
    {
        return (RK_S32) PP_SET_IN_FORMAT_INVALID;
    }

    /* PPOutImage check */

    if(!PPIsOutPixFmtOk(ppOutImg->pixFormat, ppC))
    {
        return (RK_S32) PP_SET_OUT_FORMAT_INVALID;
    }

    if(ppOutImg->bufferBusAddr == 0 || ppOutImg->bufferBusAddr & address_mask)
    {
        return (RK_S32) PP_SET_OUT_ADDRESS_INVALID;
    }

    if(ppOutImg->pixFormat == PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR)
    {
        if(ppOutImg->bufferChromaBusAddr == 0 ||
           (ppOutImg->bufferChromaBusAddr & address_mask))
            return (RK_S32) PP_SET_OUT_ADDRESS_INVALID;
    }

    if(ppOutImg->width < PP_OUT_MIN_WIDTH ||
       ppOutImg->height < PP_OUT_MIN_HEIGHT ||
       ppOutImg->width > ppC->maxOutWidth ||
       ppOutImg->height > ppC->maxOutHeight)
    {
        return (RK_S32) PP_SET_OUT_SIZE_INVALID;
    }

    /* scale check */
    {
        RK_U32 w, h, multires = 0;

        w = ppInCrop->enable ? ppInCrop->width : ppInImg->width;
        h = ppInCrop->enable ? ppInCrop->height : ppInImg->height;

        if(decType == PP_PIPELINED_DEC_TYPE_VC1)
            multires = ppInImg->vc1MultiResEnable ? 1 : 0;

        /* swap width and height if input is rotated first */
        if(ppInRotation->rotation == PP_ROTATION_LEFT_90 ||
           ppInRotation->rotation == PP_ROTATION_RIGHT_90)
        {
            RK_U32 tmp = w;

            w = h;
            h = tmp;
        }

        if(!ppC->scalingEna)
        {
            if((w != ppOutImg->width) || (h != ppOutImg->height))
                return (RK_S32) PP_SET_SCALING_UNSUPPORTED;
        }

        if((ppOutImg->width > w) &&
           (ppOutImg->width > PP_OUT_MAX_WIDTH_UPSCALED(w, multires)))
        {
            return (RK_S32) PP_SET_OUT_SIZE_INVALID;
        }

        if(multires && ppOutImg->width != w)
            return (RK_S32) PP_SET_OUT_SIZE_INVALID;

        if((ppOutImg->height > h) &&
           (ppOutImg->height > PP_OUT_MAX_HEIGHT_UPSCALED(h, multires)))
        {
            return (RK_S32) PP_SET_OUT_SIZE_INVALID;
        }

        if(multires && ppOutImg->height != h)
            return (RK_S32) PP_SET_OUT_SIZE_INVALID;

        if(((ppOutImg->width > w) && (ppOutImg->height < h)) ||
           ((ppOutImg->width < w) && (ppOutImg->height > h)))
        {
            return (RK_S32) PP_SET_OUT_SIZE_INVALID;
        }
    }

    /* PPOutFrameBuffer */
    if(ppOutFrmBuffer->enable)
    {
        if((ppOutFrmBuffer->frameBufferWidth > PP_MAX_FRM_BUFF_WIDTH) ||
           (ppOutFrmBuffer->writeOriginX >=
            (RK_S32) ppOutFrmBuffer->frameBufferWidth) ||
           (ppOutFrmBuffer->writeOriginY >=
            (RK_S32) ppOutFrmBuffer->frameBufferHeight) ||
           (ppOutFrmBuffer->writeOriginX + (RK_S32) ppOutImg->width <= 0) ||
           (ppOutFrmBuffer->writeOriginY + (RK_S32) ppOutImg->height <= 0))
        {
            return (RK_S32) PP_SET_FRAMEBUFFER_INVALID;
        }
        /* Divisibility */
        if((ppOutFrmBuffer->writeOriginY & 1) &&
           (ppOutImg->pixFormat & PP_PIXEL_FORMAT_YUV420_MASK))
        {
            return (RK_S32) PP_SET_FRAMEBUFFER_INVALID;
        }

        if((ppOutFrmBuffer->frameBufferHeight & 1) &&
           (ppOutImg->pixFormat & PP_PIXEL_FORMAT_YUV420_MASK))
        {
            return (RK_S32) PP_SET_FRAMEBUFFER_INVALID;
        }

    }

    /* PPOutRgb */

    if((ppOutImg->pixFormat & PP_PIXEL_FORMAT_RGB_MASK))
    {
        /* Check support in HW */
        if(!ppC->ditherEna && ppOutRgb->ditheringEnable)
            return (RK_S32) PP_SET_DITHERING_UNSUPPORTED;

        if((ppOutRgb->rgbTransform != PP_YCBCR2RGB_TRANSFORM_CUSTOM) &&
           (ppOutRgb->rgbTransform != PP_YCBCR2RGB_TRANSFORM_BT_601) &&
           (ppOutRgb->rgbTransform != PP_YCBCR2RGB_TRANSFORM_BT_709))
        {
            return (RK_S32) PP_SET_VIDEO_ADJUST_INVALID;
        }

        if(ppOutRgb->brightness < -128 || ppOutRgb->brightness > 127)
        {
            return (RK_S32) PP_SET_VIDEO_ADJUST_INVALID;
        }

        if(ppOutRgb->saturation < -64 || ppOutRgb->saturation > 128)
        {
            return (RK_S32) PP_SET_VIDEO_ADJUST_INVALID;
        }

        if(ppOutRgb->contrast < -64 || ppOutRgb->contrast > 64)
        {
            return (RK_S32) PP_SET_VIDEO_ADJUST_INVALID;
        }

        if((ppOutImg->pixFormat & PP_PIXEL_FORMAT_RGB32_MASK))
        {
            if(ppOutRgb->alpha > 255)
            {
                return (RK_S32) PP_SET_VIDEO_ADJUST_INVALID;
            }
        }
        else /* 16 bits RGB */ if(ppOutRgb->transparency > 1)
        {
            return (RK_S32) PP_SET_VIDEO_ADJUST_INVALID;
        }

        if(ppOutImg->pixFormat == PP_PIX_FMT_RGB32_CUSTOM)
        {
            PPRgbBitmask *rgbbm = &ppOutRgb->rgbBitmask;

            if((rgbbm->maskR & rgbbm->maskG & rgbbm->maskB & rgbbm->
                maskAlpha) != 0)
            {
                return (RK_S32) PP_SET_RGB_BITMASK_INVALID;
            }
        }
        else if(ppOutImg->pixFormat == PP_PIX_FMT_RGB16_CUSTOM)
        {
            PPRgbBitmask *rgbbm = &ppOutRgb->rgbBitmask;

            if((rgbbm->maskR & rgbbm->maskG & rgbbm->maskB & rgbbm->
                maskAlpha) != 0 ||
               (rgbbm->maskR | rgbbm->maskG | rgbbm->maskB | rgbbm->
                maskAlpha) >= (1 << 16))
            {
                return (RK_S32) PP_SET_RGB_BITMASK_INVALID;
            }
        }
        if((ppOutImg->pixFormat == PP_PIX_FMT_RGB16_CUSTOM) ||
           (ppOutImg->pixFormat == PP_PIX_FMT_RGB32_CUSTOM))
        {

            PPRgbBitmask *rgbbm = &ppOutRgb->rgbBitmask;

            if(PPCheckOverlapping(rgbbm->maskR,
                                  rgbbm->maskG, rgbbm->maskB, rgbbm->maskAlpha))
                return (RK_S32) PP_SET_RGB_BITMASK_INVALID;

            if(PPContinuousCheck(rgbbm->maskR) ||
               PPContinuousCheck(rgbbm->maskG) ||
               PPContinuousCheck(rgbbm->maskB) ||
               PPContinuousCheck(rgbbm->maskAlpha))
                return (RK_S32) PP_SET_RGB_BITMASK_INVALID;

        }

    }

    if(ppOutMask1->enable && ppOutMask1->alphaBlendEna)
    {
        if(ppOutMask1->blendComponentBase & address_mask ||
           ppOutMask1->blendComponentBase == 0)
            return (RK_S32) PP_SET_MASK1_INVALID;
    }

    if(ppOutMask2->enable && ppOutMask2->alphaBlendEna)
    {
        if(ppOutMask2->blendComponentBase & address_mask ||
           ppOutMask2->blendComponentBase == 0)
            return (RK_S32) PP_SET_MASK2_INVALID;

    }

    {
        RK_S32 ret = PPCheckAllWidthParams(ppCfg, ppC->blendEna);

        if(ret != (RK_S32) PP_OK)
            return ret;
    }
    {
        RK_S32 ret = PPCheckAllHeightParams(ppCfg);

        if(ret != (RK_S32) PP_OK)
            return ret;
    }

    /* deinterlacing only for semiplanar & planar 4:2:0 */
    if(ppOutDeint->enable)
    {
        if(!ppC->deintEna)
            return (RK_S32) PP_SET_DEINTERLACING_UNSUPPORTED;

        if(ppInImg->pixFormat != PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR &&
           ppInImg->pixFormat != PP_PIX_FMT_YCBCR_4_2_0_PLANAR &&
           ppInImg->pixFormat != PP_PIX_FMT_YCBCR_4_0_0)
        {
            return (RK_S32) PP_SET_DEINTERLACE_INVALID;
        }
    }

    if(ppInImg->vc1RangeRedFrm &&
       (ppInImg->vc1RangeMapYEnable || ppInImg->vc1RangeMapCEnable))
        return (RK_S32) PP_SET_IN_RANGE_MAP_INVALID;
    else if(ppInImg->vc1RangeMapYCoeff > 7 || ppInImg->vc1RangeMapCCoeff > 7)
        return (RK_S32) PP_SET_IN_RANGE_MAP_INVALID;

    return 0;
}

/*------------------------------------------------------------------------------
    Function name   : PPRun
    Description     :
    Return type     : pp result
    Argument        : PPContainer * ppC
------------------------------------------------------------------------------*/
PPResult PPRun(PPContainer * ppC)
{
    PPSetStatus(ppC, PP_STATUS_RUNNING);

    PPDEBUG_PRINT(("pp status 2%x\n", PPGetStatus(ppC)));
#if 0
    if(DWLReserveHw(ppC->dwl) != DWL_OK)
    {
        return PP_BUSY;
    }
#endif

    if(ppC->pipeline)
    {
        ASSERT(ppC->ppCfg.ppInRotation.rotation == PP_ROTATION_NONE);
        /* Disable rotation for pipeline mode */
        ppC->ppCfg.ppInRotation.rotation = PP_ROTATION_NONE;
        SetPpRegister(ppC->ppRegs, HWIF_ROTATION_MODE, 0);
    }

    PPFlushRegs(ppC);

    if(!ppC->pipeline)
    {
        /* turn ASIC ON by setting high the enable bit */
        SetPpRegister(ppC->ppRegs, HWIF_PP_E, 1);
        //DWLEnableHW(ppC->dwl, PP_X170_REG_START, ppC->ppRegs[0]);
    }
    else
    {
        /* decoder turns PP ON in pipeline mode (leave enable bit low) */
        SetPpRegister(ppC->ppRegs, HWIF_PP_E, 0);
        //DWLEnableHW(ppC->dwl, PP_X170_REG_START, ppC->ppRegs[0]);
    }

    return PP_OK;
}

/*------------------------------------------------------------------------------
    Function name   : PPSetFrmBufferWriting
    Description     :
    Return type     : void
    Argument        : PPContainer * ppC
------------------------------------------------------------------------------*/
void PPSetFrmBufferWriting(PPContainer * ppC)
{
    PPOutImage *ppOutImg;
    PPOutFrameBuffer *ppOutFrmBuffer;
    RK_U32 *ppRegs;

    ASSERT(ppC != NULL);

    ppOutImg = &ppC->ppCfg.ppOutImg;
    ppOutFrmBuffer = &ppC->ppCfg.ppOutFrmBuffer;

    ppRegs = ppC->ppRegs;

    if(ppOutFrmBuffer->enable)
    {

        RK_S32 up, down, right, left, scanline;

        up = ppOutFrmBuffer->writeOriginY;
        left = ppOutFrmBuffer->writeOriginX;
        down =
            ((RK_S32) ppOutFrmBuffer->frameBufferHeight - up) -
            (RK_S32) ppOutImg->height;
        right =
            ((RK_S32) ppOutFrmBuffer->frameBufferWidth - left) -
            (RK_S32) ppOutImg->width;

        scanline = (RK_S32) ppOutFrmBuffer->frameBufferWidth;

        if(left < 0)
        {
            SetPpRegister(ppRegs, HWIF_LEFT_CROSS, (RK_U32) (-left));
            SetPpRegister(ppRegs, HWIF_LEFT_CROSS_E, 1);
        }
        else
        {
            SetPpRegister(ppRegs, HWIF_LEFT_CROSS_E, 0);
        }
        if(right < 0)
        {
            SetPpRegister(ppRegs, HWIF_RIGHT_CROSS, (RK_U32) (-right));
            SetPpRegister(ppRegs, HWIF_RIGHT_CROSS_E, 1);
        }
        else
        {
            SetPpRegister(ppRegs, HWIF_RIGHT_CROSS_E, 0);
        }

        if(up < 0)
        {
            SetPpRegister(ppRegs, HWIF_UP_CROSS, (RK_U32) (-up));
            SetPpRegister(ppRegs, HWIF_UP_CROSS_E, 1);
        }
        else
        {
            SetPpRegister(ppRegs, HWIF_UP_CROSS_E, 0);
        }

        if(down < 0)
        {
            SetPpRegister(ppRegs, HWIF_DOWN_CROSS, (RK_U32) (-down));
            SetPpRegister(ppRegs, HWIF_DOWN_CROSS_E, 1);
        }
        else
        {
            SetPpRegister(ppRegs, HWIF_DOWN_CROSS_E, 0);
        }

        SetPpRegister(ppRegs, HWIF_DISPLAY_WIDTH,
                      ppOutFrmBuffer->frameBufferWidth);

        if(ppOutImg->pixFormat & PP_PIXEL_FORMAT_RGB_MASK)
        {
            ppC->frmBufferLumaOrRgbOffset =
                (scanline * up + left) * ((RK_S32) ppC->rgbDepth / 8);
        }
        else if(ppOutImg->pixFormat == PP_PIX_FMT_YCBCR_4_2_2_INTERLEAVED ||
                ppOutImg->pixFormat == PP_PIX_FMT_YCRYCB_4_2_2_INTERLEAVED ||
                ppOutImg->pixFormat == PP_PIX_FMT_CBYCRY_4_2_2_INTERLEAVED ||
                ppOutImg->pixFormat == PP_PIX_FMT_CRYCBY_4_2_2_INTERLEAVED)

        {
            ppC->frmBufferLumaOrRgbOffset = (scanline * up + left) * 2;
        }
        else    /* PP_PIX_FMT_YCBCR_4_2_0_CH_INTERLEAVED */
        {
            ppC->frmBufferLumaOrRgbOffset = (scanline * up + left);

            ppC->frmBufferChromaOffset = (scanline * up) / 2 + left;
        }

    }
    else
    {
        SetPpRegister(ppRegs, HWIF_DOWN_CROSS_E, 0);
        SetPpRegister(ppRegs, HWIF_LEFT_CROSS_E, 0);
        SetPpRegister(ppRegs, HWIF_RIGHT_CROSS_E, 0);
        SetPpRegister(ppRegs, HWIF_UP_CROSS_E, 0);

        SetPpRegister(ppRegs, HWIF_DISPLAY_WIDTH, ppOutImg->width);

        ppC->frmBufferLumaOrRgbOffset = 0;
        ppC->frmBufferChromaOffset = 0;
    }
}

/*------------------------------------------------------------------------------
    Function name   : PPSetRgbBitmaskCustom
    Description     :
    Return type     : void
    Argument        : PPContainer * ppC
    Argument        : RK_U32 rgb16
------------------------------------------------------------------------------*/
void PPSetRgbBitmaskCustom(PPContainer * ppC, RK_U32 rgb16)
{
    RK_U32 *ppRegs;
    RK_U32 mask, pad, alpha;
    PPRgbBitmask *rgbMask;

    ASSERT(ppC != NULL);
    rgbMask = &ppC->ppCfg.ppOutRgb.rgbBitmask;
    ppRegs = ppC->ppRegs;

    alpha = rgbMask->maskAlpha;

    if(rgb16)
    {
        alpha |= alpha << 16;
    }

    /* setup R */
    mask = rgbMask->maskR;

    if(rgb16)
    {
        mask |= mask << 16; /* duplicate mask for 16 bits RGB */
    }

    pad = PPFindFirstNonZeroBit(mask);
    SetPpRegister(ppRegs, HWIF_RGB_R_PADD, pad);
    SetPpRegister(ppRegs, HWIF_R_MASK, mask | alpha);

    /* setup G */
    mask = rgbMask->maskG;

    if(rgb16)
    {
        mask |= mask << 16; /* duplicate mask for 16 bits RGB */
    }

    pad = PPFindFirstNonZeroBit(mask);
    SetPpRegister(ppRegs, HWIF_RGB_G_PADD, pad);
    SetPpRegister(ppRegs, HWIF_G_MASK, mask | alpha);

    /* setup B */
    mask = rgbMask->maskB;

    if(rgb16)
    {
        mask |= mask << 16; /* duplicate mask for 16 bits RGB */
    }

    pad = PPFindFirstNonZeroBit(mask);
    SetPpRegister(ppRegs, HWIF_RGB_B_PADD, pad);
    SetPpRegister(ppRegs, HWIF_B_MASK, mask | alpha);
}

/*------------------------------------------------------------------------------
    Function name   : PPSetRgbBitmask
    Description     :
    Return type     : void
    Argument        : PPContainer * ppC
------------------------------------------------------------------------------*/
void PPSetRgbBitmask(PPContainer * ppC)
{
    PPOutImage *ppOutImg;
    RK_U32 *ppRegs;

    ASSERT(ppC != NULL);

    ppOutImg = &ppC->ppCfg.ppOutImg;
    ppRegs = ppC->ppRegs;

    switch (ppOutImg->pixFormat)
    {
    case PP_PIX_FMT_BGR32:
        SetPpRegister(ppRegs, HWIF_B_MASK,
                      0x00FF0000 | (ppC->ppCfg.ppOutRgb.alpha << 24));
        SetPpRegister(ppRegs, HWIF_G_MASK,
                      0x0000FF00 | (ppC->ppCfg.ppOutRgb.alpha << 24));
        SetPpRegister(ppRegs, HWIF_R_MASK,
                      0x000000FF | (ppC->ppCfg.ppOutRgb.alpha << 24));
        SetPpRegister(ppRegs, HWIF_RGB_B_PADD, 8);
        SetPpRegister(ppRegs, HWIF_RGB_G_PADD, 16);
        SetPpRegister(ppRegs, HWIF_RGB_R_PADD, 24);
        break;
    case PP_PIX_FMT_RGB32:
        SetPpRegister(ppRegs, HWIF_R_MASK,
                      0x00FF0000 | (ppC->ppCfg.ppOutRgb.alpha << 24));
        SetPpRegister(ppRegs, HWIF_G_MASK,
                      0x0000FF00 | (ppC->ppCfg.ppOutRgb.alpha << 24));
        SetPpRegister(ppRegs, HWIF_B_MASK,
                      0x000000FF | (ppC->ppCfg.ppOutRgb.alpha << 24));
        SetPpRegister(ppRegs, HWIF_RGB_R_PADD, 8);
        SetPpRegister(ppRegs, HWIF_RGB_G_PADD, 16);
        SetPpRegister(ppRegs, HWIF_RGB_B_PADD, 24);
        break;

    case PP_PIX_FMT_RGB16_5_5_5:
        {
            RK_U32 mask;

            mask = 0x7C00 | (ppC->ppCfg.ppOutRgb.transparency << 15);
            SetPpRegister(ppRegs, HWIF_R_MASK, mask | (mask << 16));
            mask = 0x03E0 | (ppC->ppCfg.ppOutRgb.transparency << 15);
            SetPpRegister(ppRegs, HWIF_G_MASK, mask | (mask << 16));
            mask = 0x001F | (ppC->ppCfg.ppOutRgb.transparency << 15);
            SetPpRegister(ppRegs, HWIF_B_MASK, mask | (mask << 16));
            SetPpRegister(ppRegs, HWIF_RGB_R_PADD, 1);
            SetPpRegister(ppRegs, HWIF_RGB_G_PADD, 6);
            SetPpRegister(ppRegs, HWIF_RGB_B_PADD, 11);

        }
        break;
    case PP_PIX_FMT_BGR16_5_5_5:
        {
            RK_U32 mask;

            mask = 0x7C00 | (ppC->ppCfg.ppOutRgb.transparency << 15);
            SetPpRegister(ppRegs, HWIF_B_MASK, mask | (mask << 16));
            mask = 0x03E0 | (ppC->ppCfg.ppOutRgb.transparency << 15);
            SetPpRegister(ppRegs, HWIF_G_MASK, mask | (mask << 16));
            mask = 0x001F | (ppC->ppCfg.ppOutRgb.transparency << 15);
            SetPpRegister(ppRegs, HWIF_R_MASK, mask | (mask << 16));
            SetPpRegister(ppRegs, HWIF_RGB_B_PADD, 1);
            SetPpRegister(ppRegs, HWIF_RGB_G_PADD, 6);
            SetPpRegister(ppRegs, HWIF_RGB_R_PADD, 11);

        }

        break;

    case PP_PIX_FMT_RGB16_5_6_5:
        SetPpRegister(ppRegs, HWIF_R_MASK, 0xF800F800);
        SetPpRegister(ppRegs, HWIF_G_MASK, 0x07E007E0);
        SetPpRegister(ppRegs, HWIF_B_MASK, 0x001F001F);
        SetPpRegister(ppRegs, HWIF_RGB_R_PADD, 0);
        SetPpRegister(ppRegs, HWIF_RGB_G_PADD, 5);
        SetPpRegister(ppRegs, HWIF_RGB_B_PADD, 11);
        break;

    case PP_PIX_FMT_BGR16_5_6_5:
        SetPpRegister(ppRegs, HWIF_B_MASK, 0xF800F800);
        SetPpRegister(ppRegs, HWIF_G_MASK, 0x07E007E0);
        SetPpRegister(ppRegs, HWIF_R_MASK, 0x001F001F);
        SetPpRegister(ppRegs, HWIF_RGB_B_PADD, 0);
        SetPpRegister(ppRegs, HWIF_RGB_G_PADD, 5);
        SetPpRegister(ppRegs, HWIF_RGB_R_PADD, 11);
        break;

    case PP_PIX_FMT_RGB16_CUSTOM:
        PPSetRgbBitmaskCustom(ppC, 1);
        break;

    case PP_PIX_FMT_RGB32_CUSTOM:
        PPSetRgbBitmaskCustom(ppC, 0);
        break;
    default:
        ASSERT(0);  /* should never get here */
        break;
    }
}

/*------------------------------------------------------------------------------
    Function name   : PPSetRgbTransformCoeffs
    Description     :
    Return type     : void
    Argument        : PPContainer * ppC
------------------------------------------------------------------------------*/
void PPSetRgbTransformCoeffs(PPContainer * ppC)
{
    PPOutImage *ppOutImg;
    PPInImage *ppInImg;
    PPOutRgb *ppOutRgb;
    RK_U32 *ppRegs;

    ASSERT(ppC != NULL);

    ppOutImg = &ppC->ppCfg.ppOutImg;
    ppInImg = &ppC->ppCfg.ppInImg;
    ppOutRgb = &ppC->ppCfg.ppOutRgb;

    ppRegs = ppC->ppRegs;

    if(ppOutImg->pixFormat & PP_PIXEL_FORMAT_RGB_MASK)
    {
        RK_S32 satur = 0, tmp;
        PPRgbTransform *rgbT = &ppOutRgb->rgbTransformCoeffs;

        if(ppC->rgbDepth == 32)
            SetPpRegister(ppRegs, HWIF_RGB_PIX_IN32, 0);
        else
            SetPpRegister(ppRegs, HWIF_RGB_PIX_IN32, 1);

        /*  Contrast */
        if(ppOutRgb->contrast != 0)
        {
            RK_S32 thr1y, thr2y, off1, off2, thr1, thr2, a1, a2;

            if(ppInImg->videoRange == 0)
            {
                RK_S32 tmp1, tmp2;

                /* Contrast */
                thr1 = (219 * (ppOutRgb->contrast + 128)) / 512;
                thr1y = (219 - 2 * thr1) / 2;
                thr2 = 219 - thr1;
                thr2y = 219 - thr1y;

                tmp1 = (thr1y * 256) / thr1;
                tmp2 = ((thr2y - thr1y) * 256) / (thr2 - thr1);
                off1 = ((thr1y - ((tmp2 * thr1) / 256)) * (RK_S32) rgbT->a) / 256;
                off2 = ((thr2y - ((tmp1 * thr2) / 256)) * (RK_S32) rgbT->a) / 256;

                tmp1 = (64 * (ppOutRgb->contrast + 128)) / 128;
                tmp2 = 256 * (128 - tmp1);
                a1 = (tmp2 + off2) / thr1;
                a2 = a1 + (256 * (off2 - 1)) / (thr2 - thr1);
            }
            else
            {
                /* Contrast */
                thr1 = (64 * (ppOutRgb->contrast + 128)) / 128;
                thr1y = 128 - thr1;
                thr2 = 256 - thr1;
                thr2y = 256 - thr1y;
                a1 = (thr1y * 256) / thr1;
                a2 = ((thr2y - thr1y) * 256) / (thr2 - thr1);
                off1 = thr1y - (a2 * thr1) / 256;
                off2 = thr2y - (a1 * thr2) / 256;
            }

            if(a1 > 1023)
                a1 = 1023;
            else if(a1 < 0)
                a1 = 0;

            if(a2 > 1023)
                a2 = 1023;
            else if(a2 < 0)
                a2 = 0;

            if(thr1 > 255)
                thr1 = 255;
            else if(thr1 < 0)
                thr1 = 0;

            if(thr2 > 255)
                thr2 = 255;
            else if(thr2 < 0)
                thr2 = 0;

            if(off1 > 511)
                off1 = 511;
            else if(off1 < -512)
                off1 = -512;

            if(off2 > 511)
                off2 = 511;
            else if(off2 < -512)
                off2 = -512;

            SetPpRegister(ppRegs, HWIF_CONTRAST_THR1, (RK_U32) thr1);
            SetPpRegister(ppRegs, HWIF_CONTRAST_THR2, (RK_U32) thr2);

            SetPpRegister(ppRegs, HWIF_CONTRAST_OFF1, off1);
            SetPpRegister(ppRegs, HWIF_CONTRAST_OFF2, off2);

            SetPpRegister(ppRegs, HWIF_COLOR_COEFFA1, (RK_U32) a1);
            SetPpRegister(ppRegs, HWIF_COLOR_COEFFA2, (RK_U32) a2);

        }
        else
        {
            SetPpRegister(ppRegs, HWIF_CONTRAST_THR1, 55);
            SetPpRegister(ppRegs, HWIF_CONTRAST_THR2, 165);

            SetPpRegister(ppRegs, HWIF_CONTRAST_OFF1, 0);
            SetPpRegister(ppRegs, HWIF_CONTRAST_OFF2, 0);

            tmp = rgbT->a;

            if(tmp > 1023)
                tmp = 1023;
            else if(tmp < 0)
                tmp = 0;

            SetPpRegister(ppRegs, HWIF_COLOR_COEFFA1, tmp);
            SetPpRegister(ppRegs, HWIF_COLOR_COEFFA2, tmp);
        }

        /*  brightness */
        SetPpRegister(ppRegs, HWIF_COLOR_COEFFF, ppOutRgb->brightness);

        /* saturation */
        satur = 64 + ppOutRgb->saturation;

        tmp = (satur * (RK_S32) rgbT->b) / 64;
        if(tmp > 1023)
            tmp = 1023;
        else if(tmp < 0)
            tmp = 0;
        SetPpRegister(ppRegs, HWIF_COLOR_COEFFB, (RK_U32) tmp);

        tmp = (satur * (RK_S32) rgbT->c) / 64;
        if(tmp > 1023)
            tmp = 1023;
        else if(tmp < 0)
            tmp = 0;
        SetPpRegister(ppRegs, HWIF_COLOR_COEFFC, (RK_U32) tmp);

        tmp = (satur * (RK_S32) rgbT->d) / 64;
        if(tmp > 1023)
            tmp = 1023;
        else if(tmp < 0)
            tmp = 0;
        SetPpRegister(ppRegs, HWIF_COLOR_COEFFD, (RK_U32) tmp);

        tmp = (satur * (RK_S32) rgbT->e) / 64;
        if(tmp > 1023)
            tmp = 1023;
        else if(tmp < 0)
            tmp = 0;

        SetPpRegister(ppRegs, HWIF_COLOR_COEFFE, (RK_U32) tmp);
    }
}

/*------------------------------------------------------------------------------
    Function name   : PPFindFirstNonZeroBit
    Description     :
    Return type     : RK_U32
    Argument        : RK_U32 mask
------------------------------------------------------------------------------*/
RK_U32 PPFindFirstNonZeroBit(RK_U32 mask)
{
    RK_U32 offset = 0;

    while(!(mask & 0x80000000) && (offset < 32))
    {
        mask <<= 1;
        offset++;
    }

    return offset & 0x1F;
}

/*------------------------------------------------------------------------------
    Function name   : PPIsInPixFmtOk
    Description     :
    Return type     : RK_U32
    Argument        : RK_U32 pix_fmt
    Argument        : const PPContainer * ppC
------------------------------------------------------------------------------*/
RK_U32 PPIsInPixFmtOk(RK_U32 pix_fmt, const PPContainer * ppC)
{
    RK_U32 ret = 1;
    const RK_S32 decLinked = ppC->decInst == NULL ? 0 : 1;

    switch (pix_fmt)
    {
    case PP_PIX_FMT_YCBCR_4_2_0_TILED:
        if(ppC->decType == PP_PIPELINED_DEC_TYPE_JPEG)
            ret = 0;
        break;
    case PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR:
        break;
    case PP_PIX_FMT_YCBCR_4_2_0_PLANAR:
    case PP_PIX_FMT_YCBCR_4_2_2_INTERLEAVED:
        /* these are not supported in pipeline */
        if(decLinked != 0)
            ret = 0;
        break;
    case PP_PIX_FMT_YCBCR_4_0_0:
        /* this supported just in H264 and JPEG pipeline mode */
        if(decLinked == 0 ||
           (ppC->decType != PP_PIPELINED_DEC_TYPE_JPEG &&
            ppC->decType != PP_PIPELINED_DEC_TYPE_H264))
            ret = 0;
        /* H264 monochrome not supported in 8170 */
        if((ppC->hwId == 0x8170U) &&
           (ppC->decType == PP_PIPELINED_DEC_TYPE_H264))
            ret = 0;
        break;
    case PP_PIX_FMT_YCBCR_4_2_2_SEMIPLANAR:
    case PP_PIX_FMT_YCBCR_4_4_0:
    case PP_PIX_FMT_YCBCR_4_1_1_SEMIPLANAR:
    case PP_PIX_FMT_YCBCR_4_4_4_SEMIPLANAR:
        /* these supported just in JPEG pipeline mode */
        if(decLinked == 0 || ppC->decType != PP_PIPELINED_DEC_TYPE_JPEG)
            ret = 0;
        break;
    case PP_PIX_FMT_YCRYCB_4_2_2_INTERLEAVED:
    case PP_PIX_FMT_CBYCRY_4_2_2_INTERLEAVED:
    case PP_PIX_FMT_CRYCBY_4_2_2_INTERLEAVED:
        /* these are not supported in pipeline and in 8170 */
        if(decLinked != 0 || (ppC->hwId == 0x8170U))
            ret = 0;
        break;
    default:
        ret = 0;
    }

    return ret;
}

/*------------------------------------------------------------------------------
    Function name   : PPIsOutPixFmtOk
    Description     :
    Return type     : RK_U32
    Argument        : RK_U32 pix_fmt
    Argument        : const PPContainer * ppC
------------------------------------------------------------------------------*/
RK_U32 PPIsOutPixFmtOk(RK_U32 pix_fmt, const PPContainer * ppC)
{
    switch (pix_fmt)
    {
    case PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR:
    case PP_PIX_FMT_YCBCR_4_2_2_INTERLEAVED:
    case PP_PIX_FMT_RGB16_CUSTOM:
    case PP_PIX_FMT_RGB16_5_5_5:
    case PP_PIX_FMT_RGB16_5_6_5:
    case PP_PIX_FMT_BGR16_5_5_5:
    case PP_PIX_FMT_BGR16_5_6_5:
    case PP_PIX_FMT_RGB32_CUSTOM:
    case PP_PIX_FMT_RGB32:
    case PP_PIX_FMT_BGR32:
        return 1;
    case PP_PIX_FMT_YCRYCB_4_2_2_INTERLEAVED:
    case PP_PIX_FMT_CBYCRY_4_2_2_INTERLEAVED:
    case PP_PIX_FMT_CRYCBY_4_2_2_INTERLEAVED:
        if(ppC->hwId == 0x8170U)
            return 0;
        else
            return 1;
    default:
        return 0;
    }
}

/*------------------------------------------------------------------------------
    Function name   : PPIsOutPixFmtBlendOk
    Description     :
    Return type     : RK_U32
    Argument        : RK_U32 pix_fmt
------------------------------------------------------------------------------*/
RK_U32 PPIsOutPixFmtBlendOk(RK_U32 pix_fmt)
{
    switch (pix_fmt)
    {
    case PP_PIX_FMT_YCBCR_4_2_2_INTERLEAVED:
    case PP_PIX_FMT_YCRYCB_4_2_2_INTERLEAVED:
    case PP_PIX_FMT_CBYCRY_4_2_2_INTERLEAVED:
    case PP_PIX_FMT_CRYCBY_4_2_2_INTERLEAVED:
    case PP_PIX_FMT_RGB16_CUSTOM:
    case PP_PIX_FMT_RGB16_5_5_5:
    case PP_PIX_FMT_RGB16_5_6_5:
    case PP_PIX_FMT_BGR16_5_5_5:
    case PP_PIX_FMT_BGR16_5_6_5:
    case PP_PIX_FMT_RGB32_CUSTOM:
    case PP_PIX_FMT_RGB32:
    case PP_PIX_FMT_BGR32:
        return 1;
    default:
        return 0;
    }
}

/*------------------------------------------------------------------------------
    Function name   : PPSetupScaling
    Description     :
    Return type     : void
    Argument        : PPContainer * ppC
    Argument        : const PPOutImage *ppOutImg
------------------------------------------------------------------------------*/
void PPSetupScaling(PPContainer * ppC, const PPOutImage * ppOutImg)
{
    RK_U32 *ppRegs = ppC->ppRegs;
    PPInCropping *ppInCrop;
    RK_U32 inWidth, inHeight;

    ppInCrop = &ppC->ppCfg.ppInCrop;

    /* swap width and height if input is rotated first */
    if(ppC->ppCfg.ppInRotation.rotation == PP_ROTATION_LEFT_90 ||
       ppC->ppCfg.ppInRotation.rotation == PP_ROTATION_RIGHT_90)
    {
        if(ppInCrop->enable)
        {
            inWidth = ppInCrop->height;
            inHeight = ppInCrop->width;
        }
        else
        {
            inWidth = ppC->inHeight;
            inHeight = ppC->inWidth;
        }
    }
    else
    {
        if(ppInCrop->enable)
        {
            inWidth = ppInCrop->width;
            inHeight = ppInCrop->height;
        }
        else
        {
            inWidth = ppC->inWidth;
            inHeight = ppC->inHeight;
        }
    }

    if(inWidth < ppOutImg->width)
    {
        /* upscale */
        RK_U32 W, invW;

        SetPpRegister(ppRegs, HWIF_HOR_SCALE_MODE, 1);

        W = FDIVI(TOFIX((ppOutImg->width - 1), 16), (inWidth - 1));

        SetPpRegister(ppRegs, HWIF_SCALE_WRATIO, W);

        invW = FDIVI(TOFIX((inWidth - 1), 16), (ppOutImg->width - 1));

        SetPpRegister(ppRegs, HWIF_WSCALE_INVRA, invW);
    }
    else if(inWidth > ppOutImg->width)
    {
        /* downscale */
        RK_U32 Ch;

        SetPpRegister(ppRegs, HWIF_HOR_SCALE_MODE, 2);

        Ch = FDIVI(TOFIX((ppOutImg->width), 16), inWidth);

        SetPpRegister(ppRegs, HWIF_WSCALE_INVRA, Ch);
    }
    else
    {
        SetPpRegister(ppRegs, HWIF_HOR_SCALE_MODE, 0);
    }

    if(inHeight < ppOutImg->height)
    {
        /* upscale */
        RK_U32 H, invH;

        SetPpRegister(ppRegs, HWIF_VER_SCALE_MODE, 1);

        H = FDIVI(TOFIX((ppOutImg->height - 1), 16), (inHeight - 1));

        SetPpRegister(ppRegs, HWIF_SCALE_HRATIO, H);

        invH = FDIVI(TOFIX((inHeight - 1), 16), (ppOutImg->height - 1));

        SetPpRegister(ppRegs, HWIF_HSCALE_INVRA, invH);
    }
    else if(inHeight > ppOutImg->height)
    {
        /* downscale */
        RK_U32 Cv;

        SetPpRegister(ppRegs, HWIF_VER_SCALE_MODE, 2);

        Cv = FDIVI(TOFIX((ppOutImg->height), 16), inHeight) + 1;

        SetPpRegister(ppRegs, HWIF_HSCALE_INVRA, Cv);
    }
    else
    {
        SetPpRegister(ppRegs, HWIF_VER_SCALE_MODE, 0);
    }
}

/*------------------------------------------------------------------------------
    Function name   : PPCheckAllXParams
    Description     :
    Return type     : RK_U32
    Argument        : PPConfig * ppCfg
------------------------------------------------------------------------------*/
static RK_S32 PPCheckAllWidthParams(PPConfig * ppCfg, RK_U32 blendEna)
{
    PPOutMask1 *ppOutMask1;
    PPOutMask2 *ppOutMask2;
    PPOutImage *ppOutImg;
    PPOutFrameBuffer *ppOutFrmBuffer;

    RK_S32 ret = (RK_S32) PP_OK;
    RK_U32 multiple;

    ASSERT(ppCfg != NULL);
    ppOutMask1 = &ppCfg->ppOutMask1;
    ppOutMask2 = &ppCfg->ppOutMask2;

    ppOutImg = &ppCfg->ppOutImg;
    ppOutFrmBuffer = &ppCfg->ppOutFrmBuffer;

    multiple = PP_X170_DATA_BUS_WIDTH;

    if(ppOutImg->pixFormat & PP_PIXEL_FORMAT_RGB_MASK)
    {
        if(ppOutImg->pixFormat & PP_PIXEL_FORMAT_RGB32_MASK)
        {
            multiple = multiple / 4;    /* 4 bytes per pixel */
        }
        else
        {
            multiple = multiple / 2;    /* 2 bytes per pixel */
        }
    }
    else if(ppOutImg->pixFormat == PP_PIX_FMT_YCBCR_4_2_2_INTERLEAVED ||
            ppOutImg->pixFormat == PP_PIX_FMT_YCRYCB_4_2_2_INTERLEAVED ||
            ppOutImg->pixFormat == PP_PIX_FMT_CBYCRY_4_2_2_INTERLEAVED ||
            ppOutImg->pixFormat == PP_PIX_FMT_CRYCBY_4_2_2_INTERLEAVED)
    {
        multiple = multiple / 2;    /* 2 bytes per pixel */
    }

    if(ppOutImg->width & (WIDTH_MULTIPLE - 1))
        ret = (RK_S32) PP_SET_OUT_SIZE_INVALID;
    if(ppOutMask1->enable && (ppOutMask1->width & (multiple - 1)))
        ret = (RK_S32) PP_SET_MASK1_INVALID;
    if(ppOutMask1->enable && (ppOutMask1->originX & (multiple - 1)))
        ret = (RK_S32) PP_SET_MASK1_INVALID;
    if(ppOutMask2->enable && (ppOutMask2->width & (multiple - 1)))
        ret = (RK_S32) PP_SET_MASK2_INVALID;
    if(ppOutMask2->enable && (ppOutMask2->originX & (multiple - 1)))
        ret = (RK_S32) PP_SET_MASK2_INVALID;

    /* Check if blending is enabled in HW */
    if(ppOutMask1->alphaBlendEna && !blendEna)
    {
        ret = (RK_S32) PP_SET_ABLEND_UNSUPPORTED;
    }

    if(ppOutMask2->alphaBlendEna && !blendEna)
    {
        ret = (RK_S32) PP_SET_ABLEND_UNSUPPORTED;
    }

    if(ppOutMask1->enable && ppOutMask1->alphaBlendEna)
    {
        /* Blending masks only for 422 and rbg */
        if(!PPIsOutPixFmtBlendOk(ppOutImg->pixFormat))
        {
            ret = (RK_S32) PP_SET_MASK1_INVALID;
        }

        if(ppOutMask1->width + ppOutMask1->originX > ppOutImg->width)
            ret = (RK_S32) PP_SET_MASK1_INVALID;

        if(ppOutMask1->originY < 0)
            ret = (RK_S32) PP_SET_MASK1_INVALID;

        if(ppOutMask1->height + ppOutMask1->originY > ppOutImg->height)
            ret = (RK_S32) PP_SET_MASK1_INVALID;

        if(ppOutMask1->originX < 0)
            ret = (RK_S32) PP_SET_MASK1_INVALID;

    }

    if(ppOutMask2->enable && ppOutMask2->alphaBlendEna)
    {
        /* Blending masks only for 422 and rbg */
        if(!PPIsOutPixFmtBlendOk(ppOutImg->pixFormat))
        {
            ret = (RK_S32) PP_SET_MASK2_INVALID;
        }

        if(ppOutMask2->width + ppOutMask2->originX > ppOutImg->width)
            ret = (RK_S32) PP_SET_MASK2_INVALID;

        if(ppOutMask2->originY < 0)
            ret = (RK_S32) PP_SET_MASK2_INVALID;

        if(ppOutMask2->height + ppOutMask2->originY > ppOutImg->height)
            ret = (RK_S32) PP_SET_MASK2_INVALID;

        if(ppOutMask2->originX < 0)
            ret = (RK_S32) PP_SET_MASK2_INVALID;
    }

    if(ppOutFrmBuffer->enable)
    {
        RK_S32 tmp;

        if((ppOutFrmBuffer->frameBufferWidth & (multiple - 1)))
            ret = (RK_S32) PP_SET_FRAMEBUFFER_INVALID;
        if((ppOutFrmBuffer->writeOriginX & (multiple - 1)))
            ret = (RK_S32) PP_SET_FRAMEBUFFER_INVALID;

        tmp = ppOutFrmBuffer->writeOriginX;
        tmp = tmp > 0 ? tmp : (-1) * tmp;

        if(((RK_U32) tmp & (multiple - 1)))
            ret = (RK_S32) PP_SET_FRAMEBUFFER_INVALID;
    }

    return ret;
}

/*------------------------------------------------------------------------------
    Function name   : PPCheckAllHeightParams
    Description     :
    Return type     : RK_U32
    Argument        : PPConfig * ppCfg
------------------------------------------------------------------------------*/
RK_S32 PPCheckAllHeightParams(PPConfig * ppCfg)
{
    PPOutMask1 *ppOutMask1;
    PPOutMask2 *ppOutMask2;
    PPOutImage *ppOutImg;

    RK_S32 ret = (RK_S32) PP_OK;
    RK_U32 multiple;

    ASSERT(ppCfg != NULL);

    ppOutMask1 = &ppCfg->ppOutMask1;
    ppOutMask2 = &ppCfg->ppOutMask2;

    ppOutImg = &ppCfg->ppOutImg;

    multiple = PP_X170_DATA_BUS_WIDTH;

    if(ppOutImg->pixFormat & PP_PIXEL_FORMAT_RGB_MASK)
    {
        if(ppOutImg->pixFormat & PP_PIXEL_FORMAT_RGB32_MASK)
        {
            multiple = multiple / 4;    /* 4 bytes per pixel */
        }
        else
        {
            multiple = multiple / 2;    /* 2 bytes per pixel */
        }
    }
    else if(ppOutImg->pixFormat == PP_PIX_FMT_YCBCR_4_2_2_INTERLEAVED ||
            ppOutImg->pixFormat == PP_PIX_FMT_YCRYCB_4_2_2_INTERLEAVED ||
            ppOutImg->pixFormat == PP_PIX_FMT_CBYCRY_4_2_2_INTERLEAVED ||
            ppOutImg->pixFormat == PP_PIX_FMT_CRYCBY_4_2_2_INTERLEAVED)
    {
        multiple = multiple / 2;    /* 2 bytes per pixel */
    }

    if(ppOutImg->height & (HEIGHT_MULTIPLE - 1))
        return (RK_S32) PP_SET_OUT_SIZE_INVALID;

    if(ppOutMask1->enable &&
       (ppOutImg->pixFormat & PP_PIXEL_FORMAT_YUV420_MASK) &&
       (ppOutMask1->originY & 1))
        ret = (RK_S32) PP_SET_MASK1_INVALID;

    if(ppOutMask1->enable &&
       (ppOutImg->pixFormat & PP_PIXEL_FORMAT_YUV420_MASK) &&
       (ppOutMask1->height & 1))
        ret = (RK_S32) PP_SET_MASK1_INVALID;

    if(ppOutMask2->enable &&
       (ppOutImg->pixFormat & PP_PIXEL_FORMAT_YUV420_MASK) &&
       (ppOutMask2->originY & 1))
        ret = (RK_S32) PP_SET_MASK2_INVALID;

    if(ppOutMask2->enable &&
       (ppOutImg->pixFormat & PP_PIXEL_FORMAT_YUV420_MASK) &&
       (ppOutMask2->height & 1))
        ret = (RK_S32) PP_SET_MASK2_INVALID;

    return ret;
}

/*------------------------------------------------------------------------------
    Function name   : PPContinuosCheck
    Description     : Check are the ones only one-after-another
    Return type     : RK_S32
    Argument        : RK_U32
------------------------------------------------------------------------------*/
RK_S32 PPContinuousCheck(RK_U32 value)
{

    RK_S32 ret = (RK_S32) PP_OK;
    RK_U32 first = 0;
    RK_U32 tmp = 0;

    if(value)
    {
        do
        {
            tmp = value & 1;
            if(tmp)
                ret = (RK_S32) PP_OK;
            else
                ret = (RK_S32) PP_PARAM_ERROR;

            first |= tmp;

            value = value >> 1;
            if(!tmp && !tmp && first)
                break;

        }
        while(value);
    }

    return ret;
}

/*------------------------------------------------------------------------------
    Function name   : PPCheckOverlapping
    Description     : Check if values overlap
    Return type     : RK_S32
    Argument        : RK_U32 a b c d
------------------------------------------------------------------------------*/

RK_S32 PPCheckOverlapping(RK_U32 a, RK_U32 b, RK_U32 c, RK_U32 d)
{

    if((a & b) || (a & c) || (a & d) || (b & c) || (b & d) || (c & d))
    {
        return (RK_S32) PP_PARAM_ERROR;
    }
    else
    {
        return (RK_S32) PP_OK;
    }

}

/*------------------------------------------------------------------------------
    Function name   : PPSelectOutputSize
    Description     : Select output size  based on the HW version info
    Return type     : RK_S32
    Argument        : pp container *
------------------------------------------------------------------------------*/

RK_S32 PPSelectOutputSize(PPContainer * ppC)
{
    RK_U32 id = 0;
    RK_U32 product = 0;
    DWLHwConfig_t hwConfig;

    ASSERT(ppC != NULL);

    ppC->altRegs = 1;

    DWLReadAsicConfig(&hwConfig);
    ppC->maxOutWidth = hwConfig.maxPpOutPicWidth;
    ppC->maxOutHeight = 1920;

    ppC->blendEna = hwConfig.ppConfig & PP_ALPHA_BLENDING ? 1 : 0;
    ppC->deintEna = hwConfig.ppConfig & PP_DEINTERLACING ? 1 : 0;
    ppC->ditherEna = hwConfig.ppConfig & PP_DITHERING ? 1 : 0;
    ppC->scalingEna = hwConfig.ppConfig & PP_SCALING ? 1 : 0;

    PPDEBUG_PRINT(("Alt regs, output size %d\n", ppC->maxOutHeight));
    return (RK_S32) PP_OK;
}

/*------------------------------------------------------------------------------
    Function name   : PPSetDithering
    Description     : Set up dithering
    Return type     :
    Argument        : pp container *
------------------------------------------------------------------------------*/
static void PPSetDithering(PPContainer * ppC)
{
    PPOutImage *ppOutImg;
    PPRgbBitmask *rgbbm;
    RK_U32 *ppRegs;
    RK_U32 tmp = 0;

    ASSERT(ppC != NULL);

    ppOutImg = &ppC->ppCfg.ppOutImg;
    ppRegs = ppC->ppRegs;

    switch (ppOutImg->pixFormat)
    {
    case PP_PIX_FMT_RGB16_5_5_5:
    case PP_PIX_FMT_BGR16_5_5_5:
        SetPpRegister(ppRegs, HWIF_DITHER_SELECT_R, 2);
        SetPpRegister(ppRegs, HWIF_DITHER_SELECT_G, 2);
        SetPpRegister(ppRegs, HWIF_DITHER_SELECT_B, 2);
        break;
    case PP_PIX_FMT_RGB16_5_6_5:
    case PP_PIX_FMT_BGR16_5_6_5:
        SetPpRegister(ppRegs, HWIF_DITHER_SELECT_R, 2);
        SetPpRegister(ppRegs, HWIF_DITHER_SELECT_G, 3);
        SetPpRegister(ppRegs, HWIF_DITHER_SELECT_B, 2);
        break;
    case PP_PIX_FMT_RGB16_CUSTOM:
    case PP_PIX_FMT_RGB32_CUSTOM:

        rgbbm = &ppC->ppCfg.ppOutRgb.rgbBitmask;

        tmp = PPSelectDitheringValue(rgbbm->maskR);
        SetPpRegister(ppRegs, HWIF_DITHER_SELECT_R, tmp);

        tmp = PPSelectDitheringValue(rgbbm->maskG);
        SetPpRegister(ppRegs, HWIF_DITHER_SELECT_G, tmp);

        tmp = PPSelectDitheringValue(rgbbm->maskB);
        SetPpRegister(ppRegs, HWIF_DITHER_SELECT_B, tmp);

        break;

    default:
        break;
    }

}

/*------------------------------------------------------------------------------
    Function name   : PPCountOnes
    Description     : one ones in value
    Return type     : number of ones
    Argument        : RK_U32 value
------------------------------------------------------------------------------*/
static RK_U32 PPCountOnes(RK_U32 value)
{
    RK_U32 ones = 0;

    if(value)
    {
        do
        {
            if(value & 1)
                ones++;

            value = value >> 1;
        }
        while(value);
    }
    return ones;
}

/*------------------------------------------------------------------------------
    Function name   : PPSelectDitheringValue
    Description     : select dithering matrix for color depth set in mask
    Return type     : RK_U32, dithering value which is set to HW
    Argument        : RK_U32 mask, mask for selecting color depth
------------------------------------------------------------------------------*/
static RK_U32 PPSelectDitheringValue(RK_U32 mask)
{

    RK_U32 ones = 0;
    RK_U32 ret = 0;

    ones = PPCountOnes(mask);

    switch (ones)
    {
    case 4:
        ret = 1;
        break;
    case 5:
        ret = 2;
        break;
    case 6:
        ret = 3;
        break;
    default:
        ret = 0;
        break;
    }

    return ret;

}

/*------------------------------------------------------------------------------
    Function name   : WaitForPp
    Description     : Wait PP HW to finish
    Return type     : PPResult
    Argument        : PPContainer *
------------------------------------------------------------------------------*/
PPResult WaitForPp(PPContainer * ppC)
{
    const void *dwl;
    RK_S32 dwlret = 0;
    RK_U32 irq_stat;
    PPResult ret = PP_OK;

    dwl = ppC->dwl;

    dwlret = VPUWaitHwReady(dwl, (RK_U32) (-1));

    if(dwlret == DWL_HW_WAIT_TIMEOUT)
    {
        ret = PP_HW_TIMEOUT;
    }
    else if(dwlret == DWL_HW_WAIT_ERROR)
    {
        ret = PP_SYSTEM_ERROR;
    }

    PPRefreshRegs(ppC);

    irq_stat = GetPpRegister(ppC->ppRegs, HWIF_PP_IRQ_STAT);

    /* make sure ASIC is OFF */
    SetPpRegister(ppC->ppRegs, HWIF_PP_E, 0);
    SetPpRegister(ppC->ppRegs, HWIF_PP_IRQ, 0);
    SetPpRegister(ppC->ppRegs, HWIF_PP_IRQ_STAT, 0);
    /* also disable pipeline bit! */
    SetPpRegister(ppC->ppRegs, HWIF_PP_PIPELINE_E, 0);

    //DWLDisableHW(ppC->dwl, PP_X170_REG_START, ppC->ppRegs[0]);
    //DWLReleaseHw(ppC->dwl);

    PPSetStatus(ppC, PP_STATUS_IDLE);

    if(irq_stat & DEC_8170_IRQ_BUS)
        ret = PP_HW_BUS_ERROR;

    return ret;
}

/*------------------------------------------------------------------------------
    Function name   : PPCheckSetupChanges
    Description     : Check changes in PP config
    Return type     : PPResult
    Argument        : PPContainer *
------------------------------------------------------------------------------*/
RK_U32 PPCheckSetupChanges(PPConfig * prevCfg, PPConfig * newCfg)
{

    RK_U32 changes = 0;

    PPOutImage *prevOutImg, *newOutImg;
    PPInCropping *prevInCropping, *newInCropping;
    PPOutMask1 *prevMask1, *newMask1;
    PPOutMask2 *prevMask2, *newMask2;
    PPOutFrameBuffer *prevFrameBuffer, *newFrameBuffer;
    PPInRotation *prevRotation, *newRotation;

    prevOutImg = &prevCfg->ppOutImg;
    prevInCropping = &prevCfg->ppInCrop;
    prevMask1 = &prevCfg->ppOutMask1;
    prevMask2 = &prevCfg->ppOutMask2;
    prevFrameBuffer = &prevCfg->ppOutFrmBuffer;
    prevRotation = &prevCfg->ppInRotation;

    newOutImg = &newCfg->ppOutImg;
    newInCropping = &newCfg->ppInCrop;
    newMask1 = &newCfg->ppOutMask1;
    newMask2 = &newCfg->ppOutMask2;
    newFrameBuffer = &newCfg->ppOutFrmBuffer;
    newRotation = &newCfg->ppInRotation;

    /* output picture parameters */
    if(prevOutImg->width != newOutImg->width ||
       prevOutImg->height != newOutImg->height ||
       prevOutImg->pixFormat != newOutImg->pixFormat)
        changes++;

    /* checked bacause changes pipeline status */
    if(prevInCropping->enable != newInCropping->enable)
        changes++;

    /* checked bacause changes pipeline status */
    if(prevRotation->rotation != newRotation->rotation)
        changes++;

    if(prevMask1->enable != newMask1->enable ||
       prevMask1->originX != newMask1->originX ||
       prevMask1->originY != newMask1->originY ||
       prevMask1->height != newMask1->height ||
       prevMask1->width != newMask1->width)
        changes++;

    if(prevMask2->enable != newMask2->enable ||
       prevMask2->originX != newMask2->originX ||
       prevMask2->originY != newMask2->originY ||
       prevMask2->height != newMask2->height ||
       prevMask2->width != newMask2->width)
        changes++;

    if(prevFrameBuffer->enable != newFrameBuffer->enable ||
       prevFrameBuffer->writeOriginX != newFrameBuffer->writeOriginX ||
       prevFrameBuffer->writeOriginY != newFrameBuffer->writeOriginY ||
       prevFrameBuffer->frameBufferWidth != newFrameBuffer->frameBufferWidth ||
       prevFrameBuffer->frameBufferHeight != newFrameBuffer->frameBufferHeight)
        changes++;

    return changes;

}
