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
--  Description : The API module C-functions of the PP
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: ppapi.c,v $
--  $Date: 2010/05/27 09:31:37 $
--  $Revision: 1.212 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "vpu_type.h"
#include "ppapi.h"
#include "ppinternal.h"
#include "regdrv.h"
#include "dwl.h"
#include "decppif.h"
#include "ppcfg.h"

#include "ppdebug.h"

/*------------------------------------------------------------------------------
       Version Information
------------------------------------------------------------------------------*/

#define PP_MAJOR_VERSION 1
#define PP_MINOR_VERSION 1

#define PP_BUILD_MAJOR 1
#define PP_BUILD_MINOR 128
#define PP_SW_BUILD ((PP_BUILD_MAJOR * 1000) + PP_BUILD_MINOR)

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

#ifdef PP_TRACE
#define PP_API_TRC(str)    PPTrace((str))
#else
#define PP_API_TRC(str)
#endif

#ifdef PP_H264DEC_PIPELINE_SUPPORT
#include "h264_pp_pipeline.h"
#endif

#ifdef PP_MPEG4DEC_PIPELINE_SUPPORT
#include "mpeg4_pp_pipeline.h"
#endif

#ifdef PP_JPEGDEC_PIPELINE_SUPPORT
#include "jpeg_pp_pipeline.h"
#endif

#ifdef PP_VC1DEC_PIPELINE_SUPPORT
#include "vc1hwd_pp_pipeline.h"
#endif

#ifdef PP_MPEG2DEC_PIPELINE_SUPPORT
#include "mpeg2hwd_pp_pipeline.h"
#endif

#ifdef PP_RVDEC_PIPELINE_SUPPORT
#include "rv_pp_pipeline.h"
#endif

#ifdef PP_VP6DEC_PIPELINE_SUPPORT
#include "vp6_pp_pipeline.h"
#endif

#ifdef PP_VP8DEC_PIPELINE_SUPPORT
#include "vp8hwd_pp_pipeline.h"
#endif

#if defined(PP_JPEGDEC_PIPELINE_SUPPORT) ||\
    defined(PP_MPEG4DEC_PIPELINE_SUPPORT) ||\
    defined(PP_H264DEC_PIPELINE_SUPPORT) ||\
    defined(PP_VC1DEC_PIPELINE_SUPPORT) ||\
    defined(PP_MPEG2DEC_PIPELINE_SUPPORT) ||\
    defined(PP_RVDEC_PIPELINE_SUPPORT) ||\
    defined(PP_AVSDEC_PIPELINE_SUPPORT) ||\
    defined(PP_VP6DEC_PIPELINE_SUPPORT) ||\
    defined(PP_VP8DEC_PIPELINE_SUPPORT)
#define PP_DEC_PIPELINE_SUPPORT
static void PPDecStartPp(const void *postPInst, const DecPpInterface *);
static void PPDecEndCallback(const void *postPInst);
static void PPDecConfigQueryFromDec(const void *postPInst, DecPpQuery *);
static void PPDecSetUpDeinterlace(PPContainer * ppC,
                                  const DecPpInterface * decpp);
static void PPDecSetOutBuffer(PPContainer * ppC, const DecPpInterface * decpp);
static void PPDecDisplayIndex(const void *postPInst, RK_U32 index);
static void PPDecBufferData(const void *postPInst, RK_U32 bufferIndex,
                            RK_U32 inputBusLuma, RK_U32 inputBusChroma);

#else
#undef PP_DEC_PIPELINE_SUPPORT
#endif

#define PP_IS_JOINED(ppC) (ppC->decInst != NULL ? 1 : 0)

/*------------------------------------------------------------------------------
    Function name   : PPInit
    Description     : initialize pp
    Return type     : PPResult
    Argument        : PPInst * postPInst
------------------------------------------------------------------------------*/
PPResult PPInit(PPInst * pPostPInst)
{
    const void *dwl;
    PPContainer *pPpCont;
    DWLInitParam_t dwlInit;
    RK_U32 hwId, product;

    PP_API_TRC("PPInit #");
    if(pPostPInst == NULL)
    {
        PP_API_TRC("PPInit# ERROR: PPInst == NULL");
        return (PP_PARAM_ERROR);
    }

    *pPostPInst = NULL; /* return NULL instance for any error */

    /* check that we have correct HW */
    hwId = 0x67311148;//VPUReadAsicIDDec();

    dwlInit.clientType = DWL_CLIENT_TYPE_PP;

    dwl = VPUClientInit(&dwlInit);

    if(dwl == NULL)
    {
        PP_API_TRC("PPInit# ERROR: DWL Init failed");
        return (PP_DWL_ERROR);
    }

    pPpCont = (PPContainer *) malloc(sizeof(PPContainer));

    if(pPpCont == NULL)
    {
        PP_API_TRC("PPInit# ERROR: Memory allocation failed");
        (void) VPUClientRelease(dwl);
        return (PP_MEMFAIL);
    }

    memset(pPpCont, 0, sizeof(PPContainer));

    pPpCont->dwl = dwl;

    PPInitDataStructures(pPpCont);

    PPInitHW(pPpCont);

    if(PPSelectOutputSize(pPpCont) != PP_OK)
    {
        PP_API_TRC("PPInit# ERROR: Ilegal output size");
        free(pPpCont);
        (void) VPUClientRelease(dwl);
        return (PP_DWL_ERROR);
    }

    pPpCont->decInst = NULL;
    pPpCont->pipeline = 0;
    pPpCont->hwId = 0;//product;    /* save product id */
    PPSetStatus(pPpCont, PP_STATUS_IDLE);

    *pPostPInst = pPpCont;

    return (PP_OK);
}

/*------------------------------------------------------------------------------
    Function name   : PPGetConfig
    Description     :
    Return type     : PPResult
    Argument        : PPInst postPInst
    Argument        : PPConfig * pPpConf
------------------------------------------------------------------------------*/
PPResult PPGetConfig(PPInst postPInst, PPConfig * pPpConf)
{
    PPContainer *ppC;

    PP_API_TRC("PPGetConfig #");

    if(postPInst == NULL || pPpConf == NULL)
    {
        return (PP_PARAM_ERROR);
    }

    ppC = (PPContainer *) postPInst;

    memcpy(pPpConf, &ppC->ppCfg, sizeof(PPConfig));

    return PP_OK;
}

/*------------------------------------------------------------------------------
    Function name   : PPSetConfig
    Description     :
    Return type     : PPResult
    Argument        : PPInst postPInst
    Argument        : PPConfig * pPpConf
------------------------------------------------------------------------------*/
PPResult PPSetConfig(PPInst postPInst, PPConfig * pPpConf)
{

    PPContainer *ppC;

    PPOutImage *ppOutImg;
    PPInImage *ppInImg;
    PPOutRgb *ppOutRgb;
    PPRgbTransform *rgbT;

    RK_S32 tmp;

    PP_API_TRC("PPSetConfig #\n");
    if(postPInst == NULL || pPpConf == NULL)
    {
        return (PP_PARAM_ERROR);
    }

    ppC = (PPContainer *) postPInst;

    ppOutImg = &ppC->ppCfg.ppOutImg;
    ppInImg = &ppC->ppCfg.ppInImg;
    ppOutRgb = &ppC->ppCfg.ppOutRgb;
    rgbT = &ppOutRgb->rgbTransformCoeffs;

    if(ppC->multiBuffer)
    {
        /* we set these to make PPCheckConfig() happy */
        pPpConf->ppOutImg.bufferBusAddr =
            ppC->combinedModeBuffers.ppOutputBuffers[0].bufferBusAddr;
        pPpConf->ppOutImg.bufferChromaBusAddr =
            ppC->combinedModeBuffers.ppOutputBuffers[0].bufferChromaBusAddr;
    }
    else
    {
        /* If multibuffer mode is not used, then the amount of
         * buffers is not initialized. */
        if( ppC->combinedModeBuffers.nbrOfBuffers == 0 )
            ppC->combinedModeBuffers.nbrOfBuffers = 1;
    }

    tmp =
        PPCheckConfig(ppC, pPpConf, (ppC->decInst != NULL ? 1 : 0),
                      ppC->decType);

    if(tmp != (RK_S32) PP_OK)
    {
        return (PPResult) tmp;
    }

    /* store previous cfg in order to notice any changes that require
     * rerun of buffered pictures */
    memcpy(&ppC->prevCfg, &ppC->ppCfg, sizeof(PPConfig));

    memcpy(&ppC->ppCfg, pPpConf, sizeof(PPConfig));

    switch (ppOutRgb->rgbTransform)
    {
    case PP_YCBCR2RGB_TRANSFORM_CUSTOM:
        /* coeffs are coming from user */
        break;
    case PP_YCBCR2RGB_TRANSFORM_BT_601:
        /* Bt.601 */
        if(ppInImg->videoRange == 0)
        {
            rgbT->a = 298;
            rgbT->b = 409;
            rgbT->c = 208;
            rgbT->d = 100;
            rgbT->e = 516;
        }
        else
        {
            rgbT->a = 256;
            rgbT->b = 350;
            rgbT->c = 179;
            rgbT->d = 86;
            rgbT->e = 443;
        }
        break;
    case PP_YCBCR2RGB_TRANSFORM_BT_709:
        /* Bt.709 */
        if(ppInImg->videoRange == 0)
        {
            rgbT->a = 298;
            rgbT->b = 459;
            rgbT->c = 137;
            rgbT->d = 55;
            rgbT->e = 544;
        }
        else
        {
            rgbT->a = 256;
            rgbT->b = 403;
            rgbT->c = 120;
            rgbT->d = 48;
            rgbT->e = 475;
        }
        break;
    default:
        ASSERT(0);
    }

    /* initialize alpha blend source endianness to input picture endianness.
     * Cannot be done in Init() because output format is configured here and
     * PP_X170_IGNORE_ABLEND_ENDIANNESS flag affects only RGB32 */
    SetPpRegister(ppC->ppRegs, HWIF_PP_IN_A1_ENDIAN,
        PP_X170_INPUT_PICTURE_ENDIAN);

    switch (ppOutImg->pixFormat & 0xFF0000)
    {
    case PP_PIXEL_FORMAT_RGB_MASK:
        ppC->outFormat = PP_ASIC_OUT_FORMAT_RGB;
        if(ppOutImg->pixFormat & PP_PIXEL_FORMAT_RGB32_MASK)
        {
            ppC->rgbDepth = 32;
#if ( PP_X170_IGNORE_ABLEND_ENDIANNESS != 0 )
            /* if input endianness ignored for alpha blend sources -> set big
             * endian */
            SetPpRegister(ppC->ppRegs, HWIF_PP_IN_A1_ENDIAN,
                PP_X170_PICTURE_BIG_ENDIAN);
#endif
        }
        else
        {
            ppC->rgbDepth = 16;
        }
        break;
    case PP_PIXEL_FORMAT_YUV420_MASK:
        ppC->outFormat = PP_ASIC_OUT_FORMAT_420;
        break;
    case PP_PIXEL_FORMAT_YUV422_MASK:
        ppC->outFormat = PP_ASIC_OUT_FORMAT_422;
        ppC->outStartCh =
            (ppOutImg->pixFormat == PP_PIX_FMT_CBYCRY_4_2_2_INTERLEAVED ||
             ppOutImg->pixFormat == PP_PIX_FMT_CRYCBY_4_2_2_INTERLEAVED);
        ppC->outCrFirst =
            (ppOutImg->pixFormat == PP_PIX_FMT_YCRYCB_4_2_2_INTERLEAVED ||
             ppOutImg->pixFormat == PP_PIX_FMT_CRYCBY_4_2_2_INTERLEAVED);

        break;
    default:
        ASSERT(0);
    }

    switch (ppInImg->pixFormat)
    {
    case PP_PIX_FMT_YCBCR_4_2_2_INTERLEAVED:
        ppC->inFormat = PP_ASIC_IN_FORMAT_422;
        ppC->inStartCh = 0;
        ppC->inCrFirst = 0;
        break;
    case PP_PIX_FMT_YCRYCB_4_2_2_INTERLEAVED:
        ppC->inFormat = PP_ASIC_IN_FORMAT_422;
        ppC->inStartCh = 0;
        ppC->inCrFirst = 1;
        break;
    case PP_PIX_FMT_CBYCRY_4_2_2_INTERLEAVED:
        ppC->inFormat = PP_ASIC_IN_FORMAT_422;
        ppC->inStartCh = 1;
        ppC->inCrFirst = 0;
        break;
    case PP_PIX_FMT_CRYCBY_4_2_2_INTERLEAVED:
        ppC->inFormat = PP_ASIC_IN_FORMAT_422;
        ppC->inStartCh = 1;
        ppC->inCrFirst = 1;
        break;
    case PP_PIX_FMT_YCBCR_4_2_0_PLANAR:
        ppC->inFormat = PP_ASIC_IN_FORMAT_420_PLANAR;
        break;
    case PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR:
        ppC->inFormat = PP_ASIC_IN_FORMAT_420_SEMIPLANAR;
        break;
    case PP_PIX_FMT_YCBCR_4_0_0:
        ppC->inFormat = PP_ASIC_IN_FORMAT_400;
        break;
    case PP_PIX_FMT_YCBCR_4_2_2_SEMIPLANAR:
        ppC->inFormat = PP_ASIC_IN_FORMAT_422_SEMIPLANAR;
        break;
    case PP_PIX_FMT_YCBCR_4_2_0_TILED:
        ppC->inFormat = PP_ASIC_IN_FORMAT_420_TILED;
        break;
    case PP_PIX_FMT_YCBCR_4_4_0:
        ppC->inFormat = PP_ASIC_IN_FORMAT_440_SEMIPLANAR;
        break;
    case PP_PIX_FMT_YCBCR_4_1_1_SEMIPLANAR:
        ppC->inFormat = PP_ASIC_IN_FORMAT_411_SEMIPLANAR;
        break;
    case PP_PIX_FMT_YCBCR_4_4_4_SEMIPLANAR:
        ppC->inFormat = PP_ASIC_IN_FORMAT_444_SEMIPLANAR;
        break;
    default:
        ASSERT(0);
    }

    /* if config has changed in a way that messes up the multi-buffering,
     * mark previously decoded pics dirty */

    if(ppC->multiBuffer)
    {
        if(PPCheckSetupChanges(&ppC->prevCfg, &ppC->ppCfg))
        {
            ppC->currentSetupID++;
        }

    }

    PPSetupHW(ppC);

    return (PP_OK);
}

/*------------------------------------------------------------------------------
    Function name   : PPRelease
    Description     :
    Return type     : void
    Argument        : PPInst postPInst
------------------------------------------------------------------------------*/
void PPRelease(PPInst postPInst)
{
    PPContainer *ppC;
    const void *dwl;

    if(postPInst == NULL)
    {
        return;
    }

    ppC = (PPContainer *) postPInst;

    if(ppC->decInst != NULL)
    {
        (void) PPDecCombinedModeDisable(ppC, ppC->decInst);
    }

    dwl = ppC->dwl;

    free(ppC);

    (void) VPUClientRelease((void *) dwl);
}

/*------------------------------------------------------------------------------
    Function name   : PPGetResult
    Description     :
    Return type     : PPResult
    Argument        : PPInst postPInst
------------------------------------------------------------------------------*/
PPResult PPGetResult(PPInst postPInst)
{
    PPContainer *ppC;
    PPResult ret = PP_OK;

    PP_API_TRC("PPGetResult #");
    if(postPInst == NULL)
    {
        return (PP_PARAM_ERROR);
    }

    ppC = (PPContainer *) postPInst;

    if(PPGetStatus(ppC) != PP_STATUS_IDLE)
    {
        return PP_BUSY;
    }

    if(PP_IS_JOINED(ppC))
    {
        ret = ppC->PPCombinedRet;
    }
    else
    {
        if(PPRun(ppC) != PP_OK)
        {
            return PP_BUSY;
        }

        ret = WaitForPp(ppC);
    }

    PP_API_TRC("PPGetResult # exit");
    return ret;
}

/*------------------------------------------------------------------------------
    Function name   : PPDecWaitResult
    Description     : Wait for PP, not started here i.e PPRun not called
    Return type     : PPResult
    Argument        : PPInst postPInst
------------------------------------------------------------------------------*/
PPResult PPDecWaitResult(PPInst postPInst)
{
    PPContainer *ppC;
    PPResult ret = PP_OK;

    PP_API_TRC("PPGetResult #");

    if(postPInst == NULL)
    {
        PP_API_TRC("PPGetResult paramerr");
        return (PP_PARAM_ERROR);
    }

    ppC = (PPContainer *) postPInst;

    ASSERT(ppC->pipeline == 0);

    if(ppC->decInst == NULL)
    {
        PP_API_TRC("PPGetResult paramerr");
        return (PP_PARAM_ERROR);
    }

    if(PPGetStatus(ppC) != PP_STATUS_RUNNING)
    {

        PP_API_TRC("PPGetResult pp_busy");
        return PP_BUSY;
    }

    ret = WaitForPp(ppC);
    PP_API_TRC("PPGetResult # exit");
    return ret;
}

/*------------------------------------------------------------------------------
    Function name   : PPGetAPIVersion
    Description     :
    Return type     : PPApiVersion
------------------------------------------------------------------------------*/
PPApiVersion PPGetAPIVersion()
{
    PPApiVersion ver;

    ver.major = PP_MAJOR_VERSION;
    ver.minor = PP_MINOR_VERSION;

    PP_API_TRC("PPGetAPIVersion");

    return ver;
}

/*------------------------------------------------------------------------------
    Function name   : PPGetBuild
    Description     :
    Return type     : PPBuild
    Argument        : void
------------------------------------------------------------------------------*/
PPBuild PPGetBuild(void)
{
    PPBuild ver;
    DWLHwConfig_t hwCfg;

    memset(&ver, 0, sizeof(ver));

    ver.swBuild = PP_SW_BUILD;
    ver.hwBuild = 0x67311148;//VPUReadAsicIDDec();
    DWLReadAsicConfig(&hwCfg);

    ver.hwConfig.maxDecPicWidth = hwCfg.maxDecPicWidth;
    ver.hwConfig.maxPpOutPicWidth = hwCfg.maxPpOutPicWidth;

    ver.hwConfig.h264Support = hwCfg.h264Support;
    ver.hwConfig.jpegSupport = hwCfg.jpegSupport;
    ver.hwConfig.mpeg2Support = hwCfg.mpeg2Support;
    ver.hwConfig.mpeg4Support = hwCfg.mpeg4Support;
    ver.hwConfig.vc1Support = hwCfg.vc1Support;
    ver.hwConfig.sorensonSparkSupport = hwCfg.sorensonSparkSupport;
    ver.hwConfig.ppSupport = hwCfg.ppSupport;
    ver.hwConfig.ppConfig = hwCfg.ppConfig;
    ver.hwConfig.vp6Support = hwCfg.vp6Support;
    ver.hwConfig.refBufSupport = hwCfg.refBufSupport;
    ver.hwConfig.avsSupport = hwCfg.avsSupport;
    ver.hwConfig.rvSupport = hwCfg.rvSupport;
    ver.hwConfig.customMpeg4Support = hwCfg.customMpeg4Support;

    PP_API_TRC("PPGetBuild#");

    return (ver);
}

/*------------------------------------------------------------------------------
    Function name   : PPDecStartPp
    Description     : set up and run pp based on information from the decoder
    Return type     : void
    Argument        : pp instance and control information struct
------------------------------------------------------------------------------*/

#ifdef PP_DEC_PIPELINE_SUPPORT

static void PPDecStartPp(const void *postPInst, const DecPpInterface * decpp)
{
    PPContainer *ppC;
    PPInImage *ppInImg;
    PPOutImage *ppOutImg;
    PPOutDeinterlace *ppOutDeint;
    RK_U32 tmp = 0;

    PP_API_TRC("PPDecStartPp #");
    ppC = (PPContainer *) postPInst;

    ASSERT(ppC != NULL);
    ASSERT(decpp != NULL);

    if(ppC == NULL)
    {
        return;
    }

    if(decpp == NULL)
    {
        ppC->PPCombinedRet = PP_PARAM_ERROR;
        return;
    }

    ppC->PPCombinedRet = PP_OK;

    ppOutDeint = &ppC->ppCfg.ppOutDeinterlace;

    ppInImg = &ppC->ppCfg.ppInImg;
    ppOutImg = &ppC->ppCfg.ppOutImg;

    /* Set VC-1 specific parameters */
    if(ppC->decType == PP_PIPELINED_DEC_TYPE_VC1)
    {

        ppC->inWidth = decpp->croppedW;
        ppC->inHeight = decpp->croppedH;

        PPSetupScaling(ppC, ppOutImg);

        /* HW must discard 8 pixels from input picture.
         * Notice that decpp->inwidth is always multiple of 16 and
         * decpp->croppedW is always multiple of 8. Same for height. */
        if(decpp->inwidth != decpp->croppedW)
            SetPpRegister(ppC->ppRegs, HWIF_PP_CROP8_R_E, 1);
        else
            SetPpRegister(ppC->ppRegs, HWIF_PP_CROP8_R_E, 0);

        if(decpp->inheight != decpp->croppedH)
            SetPpRegister(ppC->ppRegs, HWIF_PP_CROP8_D_E, 1);

        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_WIDTH, decpp->inwidth >> 4);
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_HEIGHT, decpp->inheight >> 4);

        if(decpp->rangeRed)
        {
            SetPpRegister(ppC->ppRegs, HWIF_RANGEMAP_Y_E, 1);
            SetPpRegister(ppC->ppRegs, HWIF_RANGEMAP_COEF_Y, 7 + 9);
            SetPpRegister(ppC->ppRegs, HWIF_RANGEMAP_C_E, 1);
            SetPpRegister(ppC->ppRegs, HWIF_RANGEMAP_COEF_C, 7 + 9);
        }
        else
        {
            SetPpRegister(ppC->ppRegs,
                          HWIF_RANGEMAP_Y_E, decpp->rangeMapYEnable);
            SetPpRegister(ppC->ppRegs,
                          HWIF_RANGEMAP_COEF_Y, decpp->rangeMapYCoeff + 9);
            SetPpRegister(ppC->ppRegs,
                          HWIF_RANGEMAP_C_E, decpp->rangeMapCEnable);
            SetPpRegister(ppC->ppRegs,
                          HWIF_RANGEMAP_COEF_C, decpp->rangeMapCCoeff + 9);
        }
    }
    else if(ppC->decType == PP_PIPELINED_DEC_TYPE_MPEG4)
    {
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_HEIGHT, decpp->inheight >> 4);
        /* crop extra block if height is odd and fields */
        if(decpp->croppedH != decpp->inheight)
        {
            SetPpRegister(ppC->ppRegs, HWIF_PP_CROP8_D_E, 1);
        }
    }

    /* update output buffer when multibuffering used */
    if((ppC->multiBuffer != 0) &&
       (ppC->decType == PP_PIPELINED_DEC_TYPE_MPEG4 ||
        ppC->decType == PP_PIPELINED_DEC_TYPE_MPEG2 ||
        ppC->decType == PP_PIPELINED_DEC_TYPE_H264 ||
	    ppC->decType == PP_PIPELINED_DEC_TYPE_VC1 ||
        ppC->decType == PP_PIPELINED_DEC_TYPE_RV ||
        ppC->decType == PP_PIPELINED_DEC_TYPE_AVS))
    {
        if(decpp->bufferIndex >= ppC->combinedModeBuffers.nbrOfBuffers)
        {
            PP_API_TRC
                ("PPDecStartPp # exit runtime error (out-of-range buffer index)");
            ppC->PPCombinedRet = PP_PARAM_ERROR;
            return;
        }

        PPDecSetOutBuffer(ppC, decpp);
    }

    /* check that input size is same as set in PPSetConfig.
     * ppInImg->width is rounded up to next multiple of 8 as well as croppedW */
    if(decpp->croppedW != ppInImg->width)
    {
        PP_API_TRC("PPDecStartPp # exit runtime error");
        ppC->PPCombinedRet = PP_DEC_RUNTIME_ERROR;
        return;
    }

    if(decpp->croppedH != ppInImg->height)
    {
        PP_API_TRC("PPDecStartPp # exit runtime error");
        ppC->PPCombinedRet = PP_DEC_RUNTIME_ERROR;
        return;
    }

    /* Set PP input image dimensions. Cropping dimensions are used
     * if Zooming is enabled in pp configuration */
    if(ppC->ppCfg.ppInCrop.enable)
    {
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_W_EXT,
                      ((((ppC->ppCfg.ppInCrop.width + 15) >> 4) & 0xE00) >> 9));
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_WIDTH,
                      (((ppC->ppCfg.ppInCrop.width + 15) >> 4) & 0x1FF));
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_H_EXT,
                      ((((ppC->ppCfg.ppInCrop.height +
                          15) >> 4) & 0x700) >> 8));
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_HEIGHT,
                      (((ppC->ppCfg.ppInCrop.height + 15) >> 4) & 0x0FF));
    }

    if(decpp->usePipeline)
    {
        PP_API_TRC("pipelined #\n");
        ppC->pipeline = 1;
        SetPpRegister(ppC->ppRegs, HWIF_PP_PIPELINE_E, 1);
    }
    else
    {
        /* check for valid picStruct for deinterlacing */
        if(ppOutDeint->enable &&
           (decpp->picStruct == DECPP_PIC_TOP_AND_BOT_FIELD_FRAME))
        {
            PPDecSetUpDeinterlace(ppC, decpp);
        }
        else
        {
            /* disable deinterlacing for any other picStruct */
            SetPpRegister(ppC->ppRegs, HWIF_DEINT_E, 0);
            PPDEBUG_PRINT(("Deinterlacing DISABLED because not valid dec input!\n"));

            SetPpRegister(ppC->ppRegs, HWIF_PP_IN_STRUCT, decpp->picStruct);
#if 0
            if(decpp->picStruct == DECPP_PIC_TOP_FIELD_FRAME ||
               decpp->picStruct == DECPP_PIC_FRAME_OR_TOP_FIELD)
            {

                SetPpRegister(ppC->ppRegs, HWIF_PP_IN_LU_BASE,
                              decpp->inputBusLuma);
                SetPpRegister(ppC->ppRegs, HWIF_PP_IN_CB_BASE,
                              decpp->inputBusChroma);

            }
            else
            {
                SetPpRegister(ppC->ppRegs, HWIF_PP_BOT_YIN_BASE,
                              decpp->bottomBusLuma);
                SetPpRegister(ppC->ppRegs, HWIF_PP_BOT_CIN_BASE,
                              decpp->bottomBusChroma);
            }
#endif
            /* set all phy_base addresses */
            /* based on HWIF_PP_IN_STRUCT, HW will use the relevant ones only */
            SetPpRegister(ppC->ppRegs, HWIF_PP_IN_LU_BASE, decpp->inputBusLuma);

            if(ppInImg->pixFormat == PP_PIX_FMT_YCBCR_4_0_0)
            {
                /* this is workaround for standalone 4:0:0 in HW */
                SetPpRegister(ppC->ppRegs, HWIF_PP_IN_CB_BASE,
                              decpp->inputBusLuma);
                SetPpRegister(ppC->ppRegs, HWIF_PP_BOT_YIN_BASE,
                              decpp->bottomBusLuma);
                SetPpRegister(ppC->ppRegs, HWIF_PP_BOT_CIN_BASE,
                              decpp->bottomBusLuma);
            }
            else
            {
                SetPpRegister(ppC->ppRegs, HWIF_PP_IN_CB_BASE,
                              decpp->inputBusChroma);
                SetPpRegister(ppC->ppRegs, HWIF_PP_BOT_YIN_BASE,
                              decpp->bottomBusLuma);
                SetPpRegister(ppC->ppRegs, HWIF_PP_BOT_CIN_BASE,
                              decpp->bottomBusChroma);
            }
        }
        PP_API_TRC("non pipelined #\n");
        ppC->pipeline = 0;
        SetPpRegister(ppC->ppRegs, HWIF_PP_PIPELINE_E, 0);

        tmp = decpp->littleEndian ?
            PP_X170_PICTURE_LITTLE_ENDIAN : PP_X170_PICTURE_BIG_ENDIAN;

        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_ENDIAN, tmp);

        tmp = decpp->wordSwap ? 1 : 0;
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_SWAP32_E, tmp);
    }

    if(PPRun(ppC) != PP_OK)
    {
        PP_API_TRC("PPDecStartPp # exit PPRun failed");
        ppC->PPCombinedRet = PP_BUSY;
        return;
    }

    ppC->PPCombinedRet = PP_OK;
    PP_API_TRC("PPDecStartPp # exit");
    return;

}

/*------------------------------------------------------------------------------
    Function name   : PPDecConfigQueryFromDec
                        Used by decoder to ask about pp setup
    Description     : if pipeline proposed, evaluate reasons who not
    Return type     : void
    Argument        : const void *postPInst
------------------------------------------------------------------------------*/
static void PPDecConfigQueryFromDec(const void *postPInst,
                                    DecPpQuery * decPpQuery)
{
    PPContainer *ppC;

    ASSERT(postPInst != NULL);
    if(postPInst == NULL)
    {
        return;
    }
    ASSERT(decPpQuery != NULL);
    if(decPpQuery == NULL)
    {
        return;
    }

    ppC = (PPContainer *) postPInst;

    decPpQuery->pipelineAccepted = 1;
    decPpQuery->ppConfigChanged = 0;

    decPpQuery->deinterlace = ppC->ppCfg.ppOutDeinterlace.enable ? 1 : 0;
    decPpQuery->multiBuffer = ppC->multiBuffer;

    if(decPpQuery->deinterlace)
    {
        decPpQuery->pipelineAccepted = 0;
    }

    if(ppC->ppCfg.ppInRotation.rotation != PP_ROTATION_NONE)
    {
        decPpQuery->pipelineAccepted = 0;
    }

    if(ppC->ppCfg.ppInCrop.enable != 0 &&
       ppC->decType != PP_PIPELINED_DEC_TYPE_JPEG)
    {
        decPpQuery->pipelineAccepted = 0;
    }

    if(ppC->ppCfg.ppInRotation.rotation == PP_ROTATION_NONE &&
       ppC->prevCfg.ppInRotation.rotation != PP_ROTATION_NONE)
    {
        decPpQuery->ppConfigChanged = 1;
    }

    /* Some formats' pipelined block size limits max vertical upscaling ratio
     * to 2.5x */
    if(ppC->decType == PP_PIPELINED_DEC_TYPE_RV ||
       ppC->decType == PP_PIPELINED_DEC_TYPE_VP6 ||
       ppC->decType == PP_PIPELINED_DEC_TYPE_VP8 )
    {
        if( (ppC->ppCfg.ppOutImg.height > ppC->ppCfg.ppInImg.height) &&
            (2*ppC->ppCfg.ppOutImg.height >
                (5*ppC->ppCfg.ppInImg.height-2)))
        {
            decPpQuery->pipelineAccepted = 0;
        }
    }

    return;
}

/*------------------------------------------------------------------------------
    Function name   : PPDecEndCallback
    Description     : Decoder/pp cooperation ending
    Return type     : void
    Argument        : const void *postPInst
------------------------------------------------------------------------------*/
void PPDecEndCallback(const void *postPInst)
{
    PPContainer *ppC;
    PPResult ppRet;

    PP_API_TRC("PPDecEndCallback #");
    ppC = (PPContainer *) postPInst;

    if(ppC->PPCombinedRet == PP_OK)
    {
        if(ppC->pipeline)
        {
            PPRefreshRegs(ppC);

            /* turn pipeline OFF so it doesn't interfer with other instances */
            ppC->pipeline = 0;
            SetPpRegister(ppC->ppRegs, HWIF_PP_PIPELINE_E, 0);

            //DWLDisableHW(ppC->dwl, PP_X170_REG_START, ppC->ppRegs[0]);

            //DWLReleaseHw(ppC->dwl);

            PPSetStatus(ppC, PP_STATUS_IDLE);

        }
        else
        {
            ppRet = PPDecWaitResult(postPInst);
            ppC->PPCombinedRet = ppRet;
        }
    }
    PP_API_TRC("PPDecEndCallback # exit");
}

/*------------------------------------------------------------------------------
    Function name   : PPDecSetUpDeinterlace
    Description     : Decoder/pp cooperation, set up reg for deinterlace
    Return type     : void
    Argument        : void *postPInst, DecPpInterface
------------------------------------------------------------------------------*/
static void PPDecSetUpDeinterlace(PPContainer * ppC,
                                  const DecPpInterface * decpp)
{

    ASSERT(decpp->picStruct == DECPP_PIC_TOP_AND_BOT_FIELD_FRAME);
    SetPpRegister(ppC->ppRegs, HWIF_PP_IN_STRUCT,
                  DECPP_PIC_TOP_AND_BOT_FIELD_FRAME);

    if(ppC->ppCfg.ppInImg.pixFormat == PP_PIX_FMT_YCBCR_4_0_0)
    {
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_LU_BASE, decpp->inputBusLuma);
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_CB_BASE, decpp->inputBusLuma);

        SetPpRegister(ppC->ppRegs, HWIF_PP_BOT_YIN_BASE, decpp->bottomBusLuma);
        SetPpRegister(ppC->ppRegs, HWIF_PP_BOT_CIN_BASE, decpp->bottomBusLuma);
    }
    else
    {
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_LU_BASE, decpp->inputBusLuma);
        SetPpRegister(ppC->ppRegs, HWIF_PP_IN_CB_BASE, decpp->inputBusChroma);

        SetPpRegister(ppC->ppRegs, HWIF_PP_BOT_YIN_BASE, decpp->bottomBusLuma);
        SetPpRegister(ppC->ppRegs, HWIF_PP_BOT_CIN_BASE,
                      decpp->bottomBusChroma);
    }

}

#endif /* PP_DEC_PIPELINE_SUPPORT */

/*------------------------------------------------------------------------------
    Function name   : PPDecCombinedModeEnable
    Description     :
    Return type     : PPResult
    Argument        : PPInst postPInst
    Argument        : const void *pDecInst
    Argument        : RK_U32 decType
------------------------------------------------------------------------------*/
PPResult PPDecCombinedModeEnable(PPInst postPInst, const void *pDecInst,
                                 RK_U32 decType)
{
    PPContainer *ppC;
    RK_S32 ret = ~0;

    ppC = (PPContainer *) postPInst;
    PP_API_TRC("Pipeline enable #");
    PP_API_TRC("Pipeline enable #");

    if(ppC == NULL)
    {
        PP_API_TRC("PPDecCombinedModeEnable# ERROR: NULL pp instance");
        return PP_PARAM_ERROR;
    }

    if(pDecInst == NULL)
    {
        PP_API_TRC("PPDecCombinedModeEnable# ERROR: NULL decoder instance");
        return (PP_PARAM_ERROR);
    }

    if(decType == 0 || decType > 11)
    {
        PP_API_TRC
            ("PPDecCombinedModeEnable# ERROR: decoder type not defined correctly");
        return (PP_PARAM_ERROR);
    }

    if(PPGetStatus(ppC) != PP_STATUS_IDLE)
    {
        PP_API_TRC("PPDecCombinedModeEnable# ERROR: pp status not idle");
        return PP_BUSY;
    }

    if(ppC->decInst != NULL)
    {
        PP_API_TRC
            ("PPDecCombinedModeEnable# ERROR: PP already connected to a decoder");
        return PP_DEC_COMBINED_MODE_ERROR;
    }

    switch (decType)
    {

#ifdef PP_H264DEC_PIPELINE_SUPPORT
    case PP_PIPELINED_DEC_TYPE_H264:
        PPDEBUG_PRINT(("Pipeline enable h264 #\n"));
        ret = h264RegisterPP(pDecInst, ppC, PPDecStartPp,
                             PPDecEndCallback, PPDecConfigQueryFromDec,
                             PPDecDisplayIndex);
        break;
#endif

#ifdef PP_MPEG4DEC_PIPELINE_SUPPORT
    case PP_PIPELINED_DEC_TYPE_MPEG4:
        PPDEBUG_PRINT(("Pipeline enable mpeg-4 #\n"));
        ret = mpeg4RegisterPP(pDecInst, ppC, PPDecStartPp,
                              PPDecEndCallback, PPDecConfigQueryFromDec,
                              PPDecDisplayIndex, PPDecBufferData);
        break;
#endif

#ifdef PP_JPEGDEC_PIPELINE_SUPPORT
        PPDEBUG_PRINT(("Pipeline enable JPEG #\n"));
    case PP_PIPELINED_DEC_TYPE_JPEG:
        ret = jpegRegisterPP(pDecInst, ppC, PPDecStartPp,
                             PPDecEndCallback, PPDecConfigQueryFromDec);

        break;
#endif

#ifdef PP_VC1DEC_PIPELINE_SUPPORT
    case PP_PIPELINED_DEC_TYPE_VC1:
        PPDEBUG_PRINT(("Pipeline enable vc-1 #\n"));
        ret = vc1RegisterPP(pDecInst, ppC, PPDecStartPp,
                            PPDecEndCallback, PPDecConfigQueryFromDec,
                            PPDecDisplayIndex, PPDecBufferData);
        break;
#endif

#ifdef PP_MPEG2DEC_PIPELINE_SUPPORT
    case PP_PIPELINED_DEC_TYPE_MPEG2:
        PPDEBUG_PRINT(("Pipeline enable mpeg-2 #\n"));
        ret = mpeg2RegisterPP(pDecInst, ppC, PPDecStartPp,
                              PPDecEndCallback, PPDecConfigQueryFromDec,
                              PPDecDisplayIndex, PPDecBufferData);
        break;
#endif

#ifdef PP_VP6DEC_PIPELINE_SUPPORT
    case PP_PIPELINED_DEC_TYPE_VP6:
        PPDEBUG_PRINT(("Pipeline enable VP6 #\n"));
        ret = vp6RegisterPP(pDecInst, ppC, PPDecStartPp,
                            PPDecEndCallback, PPDecConfigQueryFromDec);
        break;
#endif

#ifdef PP_VP8DEC_PIPELINE_SUPPORT
    case PP_PIPELINED_DEC_TYPE_VP8:
        PPDEBUG_PRINT(("Pipeline enable VP8 #\n"));
        ret = vp8RegisterPP(pDecInst, ppC, PPDecStartPp,
                            PPDecEndCallback, PPDecConfigQueryFromDec);
        break;
#endif

#ifdef PP_RVDEC_PIPELINE_SUPPORT
    case PP_PIPELINED_DEC_TYPE_RV:
        PPDEBUG_PRINT(("Pipeline enable real #\n"));
        ret = rvRegisterPP(pDecInst, ppC, PPDecStartPp,
                              PPDecEndCallback, PPDecConfigQueryFromDec,
                              PPDecDisplayIndex, PPDecBufferData);
        break;
#endif

#ifdef PP_AVSDEC_PIPELINE_SUPPORT
    case PP_PIPELINED_DEC_TYPE_AVS:
         PPDEBUG_PRINT(("Pipeline enable AVS #\n"));
         ret = avsRegisterPP(pDecInst, ppC, PPDecStartPp,
                             PPDecEndCallback, PPDecConfigQueryFromDec,
                             PPDecDisplayIndex, PPDecBufferData);
         break;
#endif
    default:
        PP_API_TRC("PPDecCombinedModeEnable# unknown decoder type \n");
        return PP_PARAM_ERROR;
    }

    /*lint -e(527)   */
    if(ret != 0)
    {
        return PP_DEC_COMBINED_MODE_ERROR;
    }

    ppC->decType = decType;
    ppC->decInst = pDecInst;

    PP_API_TRC("PPDecCombinedModeEnable# OK \n");
    return PP_OK;
}

/*------------------------------------------------------------------------------
    Function name   : PPDecCombinedModeDisable
    Description     : decouple pp and dec
    Return type     : PPResult
    Argument        : PPInst postPInst
    Argument        : const void *pDecInst
------------------------------------------------------------------------------*/
PPResult PPDecCombinedModeDisable(PPInst postPInst, const void *pDecInst)
{
    PPContainer *ppC;

    ppC = (PPContainer *) postPInst;

    PP_API_TRC("PPDecCombinedModeDisable \n");
    ASSERT(ppC != NULL && pDecInst != NULL);

    if(ppC == NULL || pDecInst == NULL)
        return PP_PARAM_ERROR;

    if(ppC->decInst != pDecInst)
        return PP_DEC_COMBINED_MODE_ERROR;

    if(PPGetStatus(ppC) != PP_STATUS_IDLE)
        return PP_BUSY;

    switch (ppC->decType)
    {
#ifdef PP_H264DEC_PIPELINE_SUPPORT
    case PP_PIPELINED_DEC_TYPE_H264:
        PPDEBUG_PRINT(("Pipeline disable h264 #\n"));
        (void) h264UnregisterPP(ppC->decInst, ppC);
        ppC->decInst = NULL;
        break;
#endif

#ifdef PP_MPEG4DEC_PIPELINE_SUPPORT
    case PP_PIPELINED_DEC_TYPE_MPEG4:
        PPDEBUG_PRINT(("Pipeline disable mpeg4 #\n"));
        (void) mpeg4UnregisterPP(ppC->decInst, ppC);
        ppC->decInst = NULL;
        break;
#endif

#ifdef PP_JPEGDEC_PIPELINE_SUPPORT
    case PP_PIPELINED_DEC_TYPE_JPEG:
        (void) jpegUnregisterPP(ppC->decInst, ppC);
        ppC->decInst = NULL;
        break;
#endif

#ifdef PP_VC1DEC_PIPELINE_SUPPORT
    case PP_PIPELINED_DEC_TYPE_VC1:
        PPDEBUG_PRINT(("Pipeline disable vc1 #\n"));
        (void) vc1UnregisterPP(ppC->decInst, ppC);
        ppC->decInst = NULL;
        break;
#endif

#ifdef PP_MPEG2DEC_PIPELINE_SUPPORT
    case PP_PIPELINED_DEC_TYPE_MPEG2:
        PPDEBUG_PRINT(("Pipeline disable mpeg2 #\n"));
        (void) mpeg2UnregisterPP(ppC->decInst, ppC);
        ppC->decInst = NULL;
        break;
#endif

#ifdef PP_RVDEC_PIPELINE_SUPPORT
    case PP_PIPELINED_DEC_TYPE_RV:
        PPDEBUG_PRINT(("Pipeline disable real #\n"));
        (void) rvUnregisterPP(ppC->decInst, ppC);
        ppC->decInst = NULL;
        break;
#endif

#ifdef PP_VP6DEC_PIPELINE_SUPPORT
    case PP_PIPELINED_DEC_TYPE_VP6:
        PPDEBUG_PRINT(("Pipeline disable VP6 #\n"));
        (void) vp6UnregisterPP(ppC->decInst, ppC);
        ppC->decInst = NULL;
        break;
#endif

#ifdef PP_VP8DEC_PIPELINE_SUPPORT
    case PP_PIPELINED_DEC_TYPE_VP8:
        PPDEBUG_PRINT(("Pipeline disable VP8 #\n"));
        (void) vp8UnregisterPP(ppC->decInst, ppC);
        ppC->decInst = NULL;
        break;
#endif

#ifdef PP_AVSDEC_PIPELINE_SUPPORT
    case PP_PIPELINED_DEC_TYPE_AVS:
        PPDEBUG_PRINT(("Pipeline disable avs #\n"));
        (void) avsUnregisterPP(ppC->decInst, ppC);
        ppC->decInst = NULL;
        break;
#endif

    default:
        ASSERT(0);  /* should never get here */
        break;
    }

    SetPpRegister(ppC->ppRegs, HWIF_PP_PIPELINE_E, 0);

    ppC->pipeline = 0;
    ppC->decInst = NULL;

    PP_API_TRC("PPDecCombinedModeDisable OK \n");
    return PP_OK;
}

/*------------------------------------------------------------------------------
    Function name   : PPDecSetMultipleOutput
    Description     :
    Return type     :
    Argument        :
    Argument        :
------------------------------------------------------------------------------*/
PPResult PPDecSetMultipleOutput(PPInst postPInst,
                                const PPOutputBuffers * pBuffers)
{
    PPContainer *ppC;
    RK_U32 i = 0;

    ppC = (PPContainer *) postPInst;

    PP_API_TRC("PPDecSetMultipleOutput\n");

    if(ppC->hwId == 0x8170U)
    {
        PP_API_TRC("PPDecSetMultipleOutput: Feature NOT suppported!\n");
        return PP_PARAM_ERROR;
    }

    if(ppC == NULL || pBuffers == NULL)
        return PP_PARAM_ERROR;

    if(ppC->decInst == NULL)
        return PP_PARAM_ERROR;

#ifdef PP_H264DEC_PIPELINE_SUPPORT
    /* multibuffer cannot be used if PP in combined mode with H264 decoder
     * and decoder was initialized to use display smoothing */
    if (ppC->decType == PP_PIPELINED_DEC_TYPE_H264 &&
        h264UseDisplaySmoothing(ppC->decInst))
    {
        return PP_PARAM_ERROR;
    }
#endif

    if(pBuffers->nbrOfBuffers == 0 ||
       pBuffers->nbrOfBuffers > PP_MAX_MULTIBUFFER)
        return PP_PARAM_ERROR;

    for(i = 0; i < pBuffers->nbrOfBuffers; i++)
        if(pBuffers->ppOutputBuffers[i].bufferBusAddr == 0)
            return PP_PARAM_ERROR;

    for(i = 0; i < pBuffers->nbrOfBuffers; i++)
        ppC->bufferData[i].setupID = ppC->currentSetupID;

    /* mark previously decoded pics dirty */
    ppC->currentSetupID++;

    memcpy(&ppC->combinedModeBuffers, pBuffers,
                     sizeof(PPOutputBuffers));
    ppC->multiBuffer = 1;

    PP_API_TRC("PPDecSetMultipleOutput OK \n");
    return PP_OK;

}

/*------------------------------------------------------------------------------
    Function name   : PPGetNextOutput
    Description     : API function for acquiring pp output location
    Return type     : PPResult
    Argument        : PPInst postPInst
    Argument        :
------------------------------------------------------------------------------*/
PPResult PPGetNextOutput(PPInst postPInst, PPOutput * pOut)
{
    PPContainer *ppC;
    PPResult ret = PP_OK;

    ppC = (PPContainer *) postPInst;

    PP_API_TRC("PPGetNextOutput\n");

    if(ppC->hwId == 0x8170U)
    {
        PP_API_TRC("PPGetNextOutput: Feature NOT suppported!\n");
        return PP_PARAM_ERROR;
    }

    if(ppC == NULL || pOut == NULL)
        return PP_PARAM_ERROR;

    if(PPGetStatus(ppC) != PP_STATUS_IDLE)
    {
        return PP_BUSY;
    }

    if(ppC->multiBuffer)
    {
        const PPOutput *out;
        RK_U32 displayIndex = ppC->displayIndex;

        if(displayIndex >= ppC->combinedModeBuffers.nbrOfBuffers)
        {
            PP_API_TRC("PPGetNextOutput: Output index out of range!\n");
            return PP_DEC_RUNTIME_ERROR;
        }
        out = &ppC->combinedModeBuffers.ppOutputBuffers[displayIndex];

        pOut->bufferBusAddr =
            ppC->combinedModeBuffers.ppOutputBuffers[displayIndex].
            bufferBusAddr;
        pOut->bufferChromaBusAddr =
            ppC->combinedModeBuffers.ppOutputBuffers[displayIndex].
            bufferChromaBusAddr;

        if(ppC->currentSetupID != ppC->bufferData[displayIndex].setupID)
        {
            /* the config has changed after processing this picture */
            /* The input source information was stored here previously, so rerun */
            SetPpRegister(ppC->ppRegs, HWIF_PP_IN_LU_BASE,
                          ppC->bufferData[displayIndex].inputBusLuma);
            SetPpRegister(ppC->ppRegs, HWIF_PP_IN_CB_BASE,
                          ppC->bufferData[displayIndex].inputBusChroma);

            SetPpRegister(ppC->ppRegs, HWIF_PP_OUT_LU_BASE,
                          ppC->combinedModeBuffers.ppOutputBuffers[displayIndex].
                          bufferBusAddr);
            SetPpRegister(ppC->ppRegs, HWIF_PP_OUT_CH_BASE,
                          ppC->combinedModeBuffers.ppOutputBuffers[displayIndex].
                          bufferChromaBusAddr);

            /* rerun picture because the configuration has changed */
            PP_API_TRC("PPGetNextOutput: Config changed... rerun\n");
            if(PPRun(ppC) != PP_OK)
            {
                return PP_BUSY;
            }
            ret = WaitForPp(ppC);
        }
    }
    else
    {
        pOut->bufferBusAddr = ppC->ppCfg.ppOutImg.bufferBusAddr;
        pOut->bufferChromaBusAddr = ppC->ppCfg.ppOutImg.bufferChromaBusAddr;
    }

    PP_API_TRC("PPGetNextOutput end\n");
    return ret;
}

#ifdef PP_DEC_PIPELINE_SUPPORT
/*------------------------------------------------------------------------------
    Function name   : PPDecSetOutBuffer
    Description     : set up multibuffer
    Return type     : void
    Argument        : container
------------------------------------------------------------------------------*/

#ifdef ASIC_TRACE_SUPPORT
extern RK_U8 *pDisplayY;
extern RK_U8 *pDisplayC;
#endif

static void PPDecSetOutBuffer(PPContainer * ppC, const DecPpInterface * decpp)
{

    PPOutImage *ppOutImg;

    ppOutImg = &ppC->ppCfg.ppOutImg;

    ASSERT(decpp->bufferIndex < ppC->combinedModeBuffers.nbrOfBuffers);

    ppOutImg->bufferBusAddr =
        ppC->combinedModeBuffers.ppOutputBuffers[decpp->bufferIndex].
        bufferBusAddr;
    ppOutImg->bufferChromaBusAddr =
        ppC->combinedModeBuffers.ppOutputBuffers[decpp->bufferIndex].
        bufferChromaBusAddr;

    ppC->displayIndex = decpp->displayIndex;

#ifdef ASIC_TRACE_SUPPORT
    pDisplayY = ppOutImg->bufferBusAddr;
    pDisplayC = ppOutImg->bufferChromaBusAddr;
#endif

    /* output buffer setup */
    SetPpRegister(ppC->ppRegs, HWIF_PP_OUT_LU_BASE,
                  (RK_U32) (ppOutImg->bufferBusAddr +
                         ppC->frmBufferLumaOrRgbOffset));

    /* chromas not needed for RGB and YUYV 422 out */
    if(ppOutImg->pixFormat == PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR)
    {
        SetPpRegister(ppC->ppRegs, HWIF_PP_OUT_CH_BASE,
                      (RK_U32) (ppOutImg->bufferChromaBusAddr +
                             ppC->frmBufferChromaOffset));
    }

    /* store some info if the config is changed and we need to rerun the pic */
    ppC->bufferData[decpp->bufferIndex].setupID = ppC->currentSetupID;

    /* Code below is required for H264 multibuffer support, but must not
     * be used on any formats as they have different handling of PP input
     * buffers. */
#ifdef PP_H264DEC_PIPELINE_SUPPORT
    ppC->bufferData[decpp->bufferIndex].inputBusLuma = decpp->inputBusLuma;
    ppC->bufferData[decpp->bufferIndex].inputBusChroma = decpp->inputBusChroma;
#endif /* PP_H264DEC_PIPELINE_SUPPORT */

}

/*------------------------------------------------------------------------------
    Function name   : PPDecDisplayIndex
    Description     : set next output in display order
    Return type     : void
    Argument        : container
------------------------------------------------------------------------------*/
void PPDecDisplayIndex(const void *postPInst, RK_U32 index)
{
    PPContainer *ppC;

    PP_API_TRC("PPDecDisplayIndex #");
    ppC = (PPContainer *) postPInst;

    ASSERT(index < ppC->combinedModeBuffers.nbrOfBuffers);

    ppC->displayIndex = index;

    return;
}

/*------------------------------------------------------------------------------
    Function name   : PPDecBufferData
    Description     : Store some info if the config is changed
    Return type     : void
    Argument        : container
------------------------------------------------------------------------------*/
void PPDecBufferData(const void *postPInst, RK_U32 bufferIndex,
                     RK_U32 inputBusLuma, RK_U32 inputBusChroma)
{
    PPContainer *ppC;

    PP_API_TRC("PPDecBufferData #");
    ppC = (PPContainer *) postPInst;

    ASSERT(bufferIndex < ppC->combinedModeBuffers.nbrOfBuffers);

    /* store some info if the config is changed and we need to rerun the pic */
    ppC->bufferData[bufferIndex].setupID = ppC->currentSetupID;
    ppC->bufferData[bufferIndex].inputBusLuma = inputBusLuma;
    ppC->bufferData[bufferIndex].inputBusChroma = inputBusChroma;

    return;
}

#endif /*PP_DEC_PIPELINE_SUPPORT */
