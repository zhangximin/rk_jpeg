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
--  Abstract  : 
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: EncJpegInit.c,v $
--  $Revision: 1.2 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of contents

    1. Include headers
    2. External compiler flags
    3. Module defines
    4. Local function prototypes
    5. Functions

------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "EncJpegInit.h"

#include "EncJpegCommon.h"
#include "ewl.h"
#include <utils/Log.h>

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
static void SetQTable(RK_U8 *dst, const RK_U8 *src);

/*------------------------------------------------------------------------------

    JpegInit

------------------------------------------------------------------------------*/
int enc_register_size = 0;//global variable

#define APITRACE ALOGD

JpegEncRet JpegInit(const JpegEncCfg * pEncCfg, jpegInstance_s ** instAddr)
{
    jpegInstance_s *inst = NULL;
    const void *ewl = NULL;

    JpegEncRet ret = JPEGENC_OK;
    EWLInitParam_t param;

    ASSERT(pEncCfg);
    ASSERT(instAddr);

    *instAddr = NULL;
	VPUHwEncConfig_t enc_config;

    param.clientType = EWL_CLIENT_TYPE_JPEG_ENC;

    /* Init EWL */
    //if((ewl = EWLInit(&param)) == NULL)
    //    return JPEGENC_EWL_ERROR;

    /* Encoder instance */
    inst = (jpegInstance_s *) calloc(1, sizeof(jpegInstance_s));

    if(inst == NULL)
    {
        ret = JPEGENC_MEMORY_ERROR;
        goto err;
    }

    /* Default values */
    EncJpegInit(&inst->jpeg);

    /* Set parameters depending on user config */
    /* Choose quantization tables */
    inst->jpeg.qTable.pQlumi = QuantLuminance[pEncCfg->qLevel];
    inst->jpeg.qTable.pQchromi = QuantChrominance[pEncCfg->qLevel];

    /* If user specified quantization tables, use them */
    if (pEncCfg->qTableLuma)
    {
        SetQTable(inst->jpeg.qTableLuma, pEncCfg->qTableLuma);
        inst->jpeg.qTable.pQlumi = inst->jpeg.qTableLuma;
    }

    if (pEncCfg->qTableChroma)
    {
        SetQTable(inst->jpeg.qTableChroma, pEncCfg->qTableChroma);
        inst->jpeg.qTable.pQchromi = inst->jpeg.qTableChroma;
    }

    /* Comment header data */
    if(pEncCfg->comLength > 0 && pEncCfg->pCom != NULL)
    {
        inst->jpeg.com.comLen = pEncCfg->comLength;
        inst->jpeg.com.pComment = pEncCfg->pCom;
        inst->jpeg.com.comEnable = 1;
    }

    /* Units type */
    if(pEncCfg->unitsType == JPEGENC_NO_UNITS)
    {
        inst->jpeg.appn.units = ENC_NO_UNITS;
        inst->jpeg.appn.Xdensity = 1;
        inst->jpeg.appn.Ydensity = 1;
    }
    else
    {
        inst->jpeg.appn.units = pEncCfg->unitsType;
        inst->jpeg.appn.Xdensity = pEncCfg->xDensity;
        inst->jpeg.appn.Ydensity = pEncCfg->yDensity;
    }

    /* Marker type */
    if(pEncCfg->markerType == JPEGENC_SINGLE_MARKER)
    {
        inst->jpeg.markerType = ENC_SINGLE_MARKER;
    }
    else
    {
        inst->jpeg.markerType = pEncCfg->markerType;
    }

#if 0
    /* Rotation type */
    if(pEncCfg->rotation == JPEGENC_ROTATE_90R)
    {
        inst->jpeg.rotation = ROTATE_90R;
    }
    else if(pEncCfg->rotation == JPEGENC_ROTATE_90L)
    {
        inst->jpeg.rotation = ROTATE_90L;
    }
    else
    {
        inst->jpeg.rotation = ROTATE_0;
    }
#endif

    /* Copy quantization tables to ASIC internal memories */
    EncAsicSetQuantTable(&inst->asic, inst->jpeg.qTable.pQlumi,
                         inst->jpeg.qTable.pQchromi);

    /* Initialize ASIC */
    inst->asic.ewl = ewl;
	inst->asic.regs.socket = VPUClientInit(VPU_ENC);
	if(enc_register_size == 0){
		memset(&enc_config, 0, sizeof(VPUHwEncConfig_t));
		if (!VPUClientGetHwCfg(inst->asic.regs.socket, (RK_U32*)&enc_config, sizeof(enc_config))){
			enc_register_size = enc_config.reg_size/4;
			APITRACE("Jpeg Get Hw Cfg, register size: %d", enc_register_size);
		}
	}
	if(!enc_register_size){
		APITRACE("JpegEncInit: ERROR, 0 register size is not allowed");
		ret = JPEGENC_ERROR;
		goto err;
	}
	
    (void) EncAsicControllerInit(&inst->asic);

    *instAddr = inst;

    return ret;

  err:
    if(inst != NULL)
        free(inst);
    //if(ewl != NULL)
       // (void) EWLRelease(ewl);

    return ret;
}

/*------------------------------------------------------------------------------

    JpegShutdown

    Function frees the encoder instance.

    Input   instance_s *    Pointer to the encoder instance to be freed.
                            After this the pointer is no longer valid.

------------------------------------------------------------------------------*/
void JpegShutdown(jpegInstance_s * data)
{
    const void *ewl;

    ASSERT(data);

    ewl = data->asic.ewl;

    EncAsicMemFree_V2(&data->asic);

    free(data);
	VPUClientRelease(data->asic.regs.socket);

   // (void) EWLRelease(ewl);
}

/*------------------------------------------------------------------------------
    SetQTable
------------------------------------------------------------------------------*/
void SetQTable(RK_U8 *dst, const RK_U8 *src)
{
    RK_S32 i;

    for (i = 0; i < 64; i++)
    {
        RK_U8 qp = src[i];

        /* Round qp to value that can be handled by ASIC */
        if (qp > 128)
            qp = qp/8*8;
        else if (qp > 64)
            qp = qp/4*4;
        else if (qp > 32)
            qp = qp/2*2;

        dst[i] = qp;
    }
}

