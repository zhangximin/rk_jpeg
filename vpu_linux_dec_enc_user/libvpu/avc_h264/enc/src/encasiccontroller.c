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
--  $RCSfile: encasiccontroller.c,v $
--  $Revision: 1.6 $
--  $Date: 2008/04/02 12:44:15 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Include headers
------------------------------------------------------------------------------*/

#include "enccommon.h"
#include "encasiccontroller.h"
#include "encpreprocess.h"
#include "ewl.h"

#include <string.h>

/*------------------------------------------------------------------------------
    External compiler flags
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/

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

i32 ExpGolombSigned(i32 val);
void H264encFlushRegs(regValues_s * val);

/* MPEG-4 motion estimation parameters */
static const i32 mpeg4InterFavor[32] = { 0,
    0, 120, 140, 160, 200, 240, 280, 340, 400, 460, 520, 600, 680,
    760, 840, 920, 1000, 1080, 1160, 1240, 1320, 1400, 1480, 1560,
    1640, 1720, 1800, 1880, 1960, 2040, 2120
};

static const u32 mpeg4DiffMvPenalty[32] = { 0,
    4, 5, 6, 7, 8, 9, 10, 11, 14, 17, 20, 23, 27, 31, 35, 38, 41,
    44, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59
};

/* H.264 motion estimation parameters */
static const u32 h264PrevModeFavor[52] = {
    7, 7, 8, 8, 9, 9, 10, 10, 11, 12, 12, 13, 14, 15, 16, 17, 18,
    19, 20, 21, 22, 24, 25, 27, 29, 30, 32, 34, 36, 38, 41, 43, 46,
    49, 51, 55, 58, 61, 65, 69, 73, 78, 82, 87, 93, 98, 104, 110,
    117, 124, 132, 140
};

static const u32 h264DiffMvPenalty[52] =	//rk29
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 6, 6, 7, 8, 9, 10,
    11, 13, 14, 16, 18, 20, 23, 25, 29, 32, 36, 40, 45, 51, 57, 64,
    72, 81, 91
};
	
/* sqrt(2^((qp-12)/3))*8 */
static const u32 h264DiffMvPenalty_rk30[52] =	//rk30
    { 2, 2, 3, 3, 3, 4, 4, 4, 5, 6,
    6, 7, 8, 9, 10, 11, 13, 14, 16, 18,
    20, 23, 26, 29, 32, 36, 40, 45, 51, 57,
    64, 72, 81, 91, 102, 114, 128, 144, 161, 181,
    203, 228, 256, 287, 323, 362, 406, 456, 512, 575,
    645, 724
};

/* 31*sqrt(2^((qp-12)/3))/4 */
static const u32 h264DiffMvPenalty4p_rk30[52] =	//rk30
    { 2, 2, 2, 3, 3, 3, 4, 4, 5, 5,
    6, 7, 8, 9, 10, 11, 12, 14, 16, 17,
    20, 22, 25, 28, 31, 35, 39, 44, 49, 55,
    62, 70, 78, 88, 98, 110, 124, 139, 156, 175,
    197, 221, 248, 278, 312, 351, 394, 442, 496, 557,
    625, 701
};


/* JPEG QUANT table order */
static const u32 qpReorderTable[64] =
    { 0,  8, 16, 24,  1,  9, 17, 25, 32, 40, 48, 56, 33, 41, 49, 57,
      2, 10, 18, 26,  3, 11, 19, 27, 34, 42, 50, 58, 35, 43, 51, 59,
      4, 12, 20, 28,  5, 13, 21, 29, 36, 44, 52, 60, 37, 45, 53, 61,
      6, 14, 22, 30,  7, 15, 23, 31, 38, 46, 54, 62, 39, 47, 55, 63
};

extern u32 h264SkipSadPenalty_rk30[52];

/*------------------------------------------------------------------------------
    Local function prototypes
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Initialize empty structure with default values.
------------------------------------------------------------------------------*/
i32 EncAsicControllerInit(asicData_s * asic)
{
    ASSERT(asic != NULL);

    /* Initialize default values from defined configuration */
    asic->regs.irqDisable = ENC8270_IRQ_DISABLE;

    asic->regs.asicCfgReg =
        ((ENC8270_AXI_READ_ID & (255)) << 24) |
        ((ENC8270_AXI_WRITE_ID & (255)) << 16) |
        ((ENC8270_OUTPUT_SWAP_16 & (1)) << 15) |
        ((ENC8270_BURST_LENGTH & (63)) << 8) |
        ((ENC8270_BURST_INCR_TYPE_ENABLED & (1)) << 6) |
        ((ENC8270_BURST_DATA_DISCARD_ENABLED & (1)) << 5) |
        ((ENC8270_ASIC_CLOCK_GATING_ENABLED & (1)) << 4) |
        ((ENC8270_OUTPUT_SWAP_32 & (1)) << 3) |
        ((ENC8270_OUTPUT_SWAP_8 & (1)) << 1);

    /* Initialize default values */
    asic->regs.roundingCtrl = 0;
    asic->regs.cpDistanceMbs = 0;
    asic->regs.riceEnable = 0;

    /* User must set these */
    asic->regs.inputLumBase = 0;
    asic->regs.inputCbBase = 0;
    asic->regs.inputCrBase = 0;

    asic->internalImageLuma[0].vir_addr = NULL;
    asic->internalImageChroma[0].vir_addr = NULL;
    asic->internalImageLuma[1].vir_addr = NULL;
    asic->internalImageChroma[1].vir_addr = NULL;
    asic->cabacCtx.vir_addr = NULL;
    asic->riceRead.vir_addr = NULL;
    asic->riceWrite.vir_addr = NULL;

#ifdef ASIC_WAVE_TRACE_TRIGGER
    asic->regs.vop_count = 0;
#endif

    /* get ASIC ID value */
    asic->regs.asicHwId = EncAsicGetId(asic->ewl);

/* we do NOT reset hardware at this point because */
/* of the multi-instance support                  */

    return ENCHW_OK;
}


/*------------------------------------------------------------------------------

    EncAsicSetQuantTable

    Set new jpeg quantization table to be used by ASIC

------------------------------------------------------------------------------*/
void EncAsicSetQuantTable(asicData_s * asic,
                          const u8 * lumTable, const u8 * chTable)
{
    i32 i;

    ASSERT(lumTable);
    ASSERT(chTable);

    for(i = 0; i < 64; i++)
    {
        asic->regs.quantTable[i] = lumTable[qpReorderTable[i]];
    }
    for(i = 0; i < 64; i++)
    {
        asic->regs.quantTable[64 + i] = chTable[qpReorderTable[i]];
    }
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
u32 EncAsicGetId(const void *ewl)
{
    return 0x82701110;
}

/*------------------------------------------------------------------------------
    When the frame is successfully encoded the internal image is recycled
    so that during the next frame the current internal image is read and
    the new reconstructed image is written two macroblock rows earlier.
------------------------------------------------------------------------------*/
void EncAsicRecycleInternalImage(regValues_s * val)
{
    /* The next encoded frame will use current reconstructed frame as */
    /* the reference */
    u32 tmp;

    tmp = val->internalImageLumBaseW;
    val->internalImageLumBaseW = val->internalImageLumBaseR;
    val->internalImageLumBaseR = tmp;

    tmp = val->internalImageChrBaseW;
    val->internalImageChrBaseW = val->internalImageChrBaseR;
    val->internalImageChrBaseR = tmp;

    tmp = val->riceReadBase;
    val->riceReadBase = val->riceWriteBase;
    val->riceWriteBase = tmp;
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
void CheckRegisterValues(regValues_s * val)
{
    u32 i;

    ASSERT(val->irqDisable <= 1);
    ASSERT(val->rlcLimitSpace / 2 < (1 << 20));
    ASSERT(val->mbsInCol <= 511);
    ASSERT(val->mbsInRow <= 511);
    ASSERT(val->filterDisable <= 2);
    ASSERT(val->madThreshold <= 63);
    ASSERT(val->madQpDelta >= -8 && val->madQpDelta <= 7);
    ASSERT(val->qp <= 51);
    ASSERT(val->constrainedIntraPrediction <= 1);
    ASSERT(val->roundingCtrl <= 1);
    ASSERT(val->frameCodingType <= 1);
    ASSERT(val->codingType <= 3);
    ASSERT(val->pixelsOnRow >= 16 && val->pixelsOnRow <= 8192); /* max input for cropping */
    ASSERT(val->xFill <= 3);
    ASSERT(val->yFill <= 14 && ((val->yFill & 0x01) == 0));
    ASSERT(val->sliceAlphaOffset >= -6 && val->sliceAlphaOffset <= 6);
    ASSERT(val->sliceBetaOffset >= -6 && val->sliceBetaOffset <= 6);
    ASSERT(val->chromaQpIndexOffset >= -12 && val->chromaQpIndexOffset <= 12);
    ASSERT(val->sliceSizeMbRows <= 127);
    ASSERT(val->inputImageFormat <= ASIC_INPUT_RGB101010);
    ASSERT(val->inputImageRotation <= 2);
    ASSERT(val->cpDistanceMbs >= 0 && val->cpDistanceMbs <= 2047);
    ASSERT(val->vpMbBits <= 15);
    ASSERT(val->vpSize <= 65535);

    if(val->codingType != ASIC_JPEG && val->cpTarget != NULL)
    {
        ASSERT(val->cpTargetResults != NULL);

        for(i = 0; i < 10; i++)
        {
            ASSERT(*val->cpTarget < (1 << 16));
        }

        ASSERT(val->targetError != NULL);

        for(i = 0; i < 7; i++)
        {
            ASSERT((*val->targetError) >= -32768 &&
                   (*val->targetError) < 32768);
        }

        ASSERT(val->deltaQp != NULL);

        for(i = 0; i < 7; i++)
        {
            ASSERT((*val->deltaQp) >= -8 && (*val->deltaQp) < 8);
        }
    }

    (void) val;
}

/*------------------------------------------------------------------------------
    Function name   : EncAsicFrameStart
    Description     : 
    Return type     : void 
    Argument        : const void *ewl
    Argument        : regValues_s * val
------------------------------------------------------------------------------*/
void EncAsicFrameStart(const void *ewl, regValues_s * val)
{
    u32 interFavor = 0, diffMvPenalty = 0, prevModeFavor = 0;
	
    /* Set the interrupt interval in macroblock rows (JPEG) or
     * in macroblocks (video) */
    if(val->codingType != ASIC_JPEG)
    {
        switch (val->codingType)
        {
        case ASIC_MPEG4:
        case ASIC_H263:
            interFavor = mpeg4InterFavor[val->qp];
            diffMvPenalty = mpeg4DiffMvPenalty[val->qp];
            break;
        case ASIC_H264:
            prevModeFavor = h264PrevModeFavor[val->qp];
            diffMvPenalty = h264DiffMvPenalty[val->qp];
            break;
        default:
            break;
        }
    }

    CheckRegisterValues(val);

    memset(val->regMirror, 0, sizeof(val->regMirror));

    /* encoder interrupt */
    val->regMirror[1] = (val->irqDisable << 1);

    /* system configuration */
    if (val->inputImageFormat < ASIC_INPUT_RGB565)      /* YUV input */
        val->regMirror[2] = val->asicCfgReg |
            ((ENC8270_INPUT_SWAP_16_YUV & (1)) << 14) |
            ((ENC8270_INPUT_SWAP_32_YUV & (1)) << 2) |
            (ENC8270_INPUT_SWAP_8_YUV & (1));
    else if (val->inputImageFormat < ASIC_INPUT_RGB888) /* 16-bit RGB input */
        val->regMirror[2] = val->asicCfgReg |
            ((ENC8270_INPUT_SWAP_16_RGB16 & (1)) << 14) |
            ((ENC8270_INPUT_SWAP_32_RGB16 & (1)) << 2) |
            (ENC8270_INPUT_SWAP_8_RGB16 & (1));
    else    /* 32-bit RGB input */
        val->regMirror[2] = val->asicCfgReg |
            ((ENC8270_INPUT_SWAP_16_RGB32 & (1)) << 14) |
            ((ENC8270_INPUT_SWAP_32_RGB32 & (1)) << 2) |
            (ENC8270_INPUT_SWAP_8_RGB32 & (1));

    /* output stream buffer */
    {
        val->regMirror[5] = val->outputStrmBase;

        if(val->codingType == ASIC_H264)
        {
            /* NAL size buffer or MB control buffer */
            val->regMirror[6] = val->sizeTblBase.nal;
            if(val->sizeTblBase.nal)
                val->sizeTblPresent = 1;
        }
        else if(val->codingType == ASIC_H263)
        {
            /* GOB size buffer or MB control buffer */
            val->regMirror[6] = val->sizeTblBase.gob;
            if(val->sizeTblBase.gob)
                val->sizeTblPresent = 1;
        }
        else
        {
            /* VP size buffer or MB control buffer */
            val->regMirror[6] = val->sizeTblBase.vp;
            if(val->sizeTblBase.vp)
                val->sizeTblPresent = 1;
        }
    }

    /* Video encoding reference picture buffers */
    if(val->codingType != ASIC_JPEG)
    {
        val->regMirror[7] = val->internalImageLumBaseR;
        val->regMirror[8] = val->internalImageChrBaseR;
        val->regMirror[9] = val->internalImageLumBaseW;
        val->regMirror[10] = val->internalImageChrBaseW;
    }

    /* Input picture buffers */
    val->regMirror[11] = val->inputLumBase;
    val->regMirror[12] = val->inputCbBase;
    val->regMirror[13] = val->inputCrBase;

    /* Common control register */
    val->regMirror[14] = (ENC8270_TIMEOUT_INTERRUPT << 31) |
        (val->riceEnable << 30) |
        (val->sizeTblPresent << 29) |
        (val->mbsInRow << 19) |
        (val->mbsInCol << 10) |
        (val->frameCodingType << 3) | (val->codingType << 1);

    /* PreP control */
    val->regMirror[15] =
        (val->inputChromaBaseOffset << 29) |
        (val->inputLumaBaseOffset << 26) |
        (val->pixelsOnRow << 12) |
        (val->xFill << 10) |
        (val->yFill << 6) |
        (val->inputImageFormat << 2) | (val->inputImageRotation);

    /* H.264 control */
    val->regMirror[16] =
        (val->picInitQp << 26) |
        ((val->sliceAlphaOffset & mask_4b) << 22) |
        ((val->sliceBetaOffset & mask_4b) << 18) |
        ((val->chromaQpIndexOffset & mask_5b) << 13) |
        (val->filterDisable << 5) |
        (val->idrPicId << 1) | (val->constrainedIntraPrediction);

	if(val->vpuid == 0x30)
	{
		val->regMirror[17] =
			(val->ppsId << 24) | (prevModeFavor << 16) | (val->frameNum);
		
		val->regMirror[18] = (val->sliceSizeMbRows << 23) |
			(val->disableQuarterPixelMv << 22) |
			(val->transform8x8Mode << 21) | (val->cabacInitIdc << 19) |
			(val->enableCabac << 18) | (val->h264Inter4x4Disabled << 17) |
			(val->h264StrmMode << 16) | (val->intra16Favor);

    	//val->regMirror[19] = 0;
    	
		/* If favor has not been set earlier by testId use default */
		{
			i32 scaler = MAX(1, 200/(val->mbsInRow+val->mbsInCol));
			val->skipPenalty = MIN(255, h264SkipSadPenalty_rk30[val->qp]*scaler);
		}

		val->splitMvMode = 1;
		val->diffMvPenalty[0] = h264DiffMvPenalty4p_rk30[val->qp];
		val->diffMvPenalty[1] = h264DiffMvPenalty_rk30[val->qp];
		val->diffMvPenalty[2] = h264DiffMvPenalty_rk30[val->qp];
		val->splitPenalty[0] = 0;
		val->splitPenalty[1] = 0;
		val->splitPenalty[2] = 0;
		val->splitPenalty[3] = 0;
		
		val->zeroMvFavorDiv2 = 10;

		val->regMirror[19] = (val->splitMvMode<<30) | val->diffMvPenalty[1] | 
							(val->diffMvPenalty[0]<<10) | (val->diffMvPenalty[2]<<20);
			
		val->regMirror[62] = val->zeroMvFavorDiv2<<28;

    	val->regMirror[20] = 0;

		val->regMirror[21] = (val->skipPenalty << 24) |	val->interFavor;

	}else{
		val->regMirror[17] =
			(val->ppsId << 24) | (prevModeFavor << 16) | (val->intra16Favor);
			
		val->regMirror[18] = (val->sliceSizeMbRows << 23) |
			(val->disableQuarterPixelMv << 22) |
			(val->transform8x8Mode << 21) | (val->cabacInitIdc << 19) |
			(val->enableCabac << 18) | (val->h264Inter4x4Disabled << 17) |
			(val->h264StrmMode << 16) | val->frameNum;
		
		val->regMirror[19] = val->riceReadBase;

		val->regMirror[20] = 0;
		
		/* Motion Estimation control */
		val->regMirror[21] = (val->skipPenalty << 24) |
							 (diffMvPenalty << 16) | val->interFavor;
	}


    /* stream buffer limits */
    {
        val->regMirror[22] = val->strmStartMSB;
        val->regMirror[23] = val->strmStartLSB;
        val->regMirror[24] = val->outputStrmSize;
    }

    val->regMirror[25] = ((val->madQpDelta & 0xF) << 28) |
                         (val->madThreshold << 22);

    /* video encoding rate control */
    if(val->codingType != ASIC_JPEG)
    {
        val->regMirror[27] = (val->qp << 26) | (val->qpMax << 20) |
            (val->qpMin << 14) | (val->cpDistanceMbs);

        if(val->cpTarget != NULL)
        {
            val->regMirror[28] = (val->cpTarget[0] << 16) | (val->cpTarget[1]);
            val->regMirror[29] = (val->cpTarget[2] << 16) | (val->cpTarget[3]);
            val->regMirror[30] = (val->cpTarget[4] << 16) | (val->cpTarget[5]);
            val->regMirror[31] = (val->cpTarget[6] << 16) | (val->cpTarget[7]);
            val->regMirror[32] = (val->cpTarget[8] << 16) | (val->cpTarget[9]);

            val->regMirror[33] = ((val->targetError[0] & mask_16b) << 16) |
                (val->targetError[1] & mask_16b);
            val->regMirror[34] = ((val->targetError[2] & mask_16b) << 16) |
                (val->targetError[3] & mask_16b);
            val->regMirror[35] = ((val->targetError[4] & mask_16b) << 16) |
                (val->targetError[5] & mask_16b);

            val->regMirror[36] = ((val->deltaQp[0] & mask_4b) << 24) |
                ((val->deltaQp[1] & mask_4b) << 20) |
                ((val->deltaQp[2] & mask_4b) << 16) |
                ((val->deltaQp[3] & mask_4b) << 12) |
                ((val->deltaQp[4] & mask_4b) << 8) |
                ((val->deltaQp[5] & mask_4b) << 4) |
                (val->deltaQp[6] & mask_4b);
        }
    }

    /* Stream start offset */
	if(val->vpuid == 0x30)
    	val->regMirror[37] = (val->firstFreeBit << 23);
	else
    	val->regMirror[37] = (val->firstFreeBit << 22);

    if(val->codingType != ASIC_JPEG)
    {
        val->regMirror[39] = val->vsNextLumaBase;
        val->regMirror[40] = val->vsMode << 30;
    }

    val->regMirror[51] = val->cabacCtxBase;

    val->regMirror[52] = val->riceWriteBase;

    val->regMirror[53] = ((val->colorConversionCoeffB & mask_16b) << 16) |
                          (val->colorConversionCoeffA & mask_16b);

    val->regMirror[54] = ((val->colorConversionCoeffE & mask_16b) << 16) |
                          (val->colorConversionCoeffC & mask_16b);

    val->regMirror[55] = ((val->bMaskMsb & mask_5b) << 26) |
                         ((val->gMaskMsb & mask_5b) << 21) |
                         ((val->rMaskMsb & mask_5b) << 16) |
                          (val->colorConversionCoeffF & mask_16b);

	if(val->vpuid == 0x30)
	{
		i32 i = 0;
		//intra area
		val->regMirror[56] = (val->intraAreaLeft&0xff)<<24;
		val->regMirror[56] |= (val->intraAreaRight&0xff)<<16;
		val->regMirror[56] |= (val->intraAreaTop&0xff)<<8;
		val->regMirror[56] |= (val->intraAreaBottom&0xff);
		//roi1 area
		val->regMirror[60] = (val->roi1Left&0xff)<<24;
		val->regMirror[60] |= (val->roi1Right&0xff)<<16;
		val->regMirror[60] |= (val->roi1Top&0xff)<<8;
		val->regMirror[60] |= (val->roi1Bottom&0xff);
		//roi2 area
		val->regMirror[61] = (val->roi2Left&0xff)<<24;
		val->regMirror[61] |= (val->roi2Right&0xff)<<16;
		val->regMirror[61] |= (val->roi2Top&0xff)<<8;
		val->regMirror[61] |= (val->roi2Bottom&0xff);		
		
		/* DMV penalty tables */
		for (i = 0; i < ASIC_PENALTY_TABLE_SIZE; i++) {
			val->dmvPenalty[i] = i;
			val->dmvQpelPenalty[i] = MIN(255,ExpGolombSigned(i));
		}
		
		/* Write DMV penalty tables to regs */
		for(i = 0; i < 128; i += 4)
		{
			/* swreg[96]=0x180 to swreg[127]=0x1FC */
			val->regMirror[96 + (i/4)] = 
						((val->dmvPenalty[i   ] << 24) |
						(val->dmvPenalty[i + 1] << 16) |
						(val->dmvPenalty[i + 2] << 8) |
						(val->dmvPenalty[i + 3]));
		}
		for(i = 0; i < 128; i += 4)
		{
			/* swreg[128]=0x200 to swreg[159]=0x27C */
			val->regMirror[128 + (i/4)] = 
						((val->dmvQpelPenalty[i   ] << 24) |
						(val->dmvQpelPenalty[i + 1] << 16) |
						(val->dmvQpelPenalty[i + 2] << 8) |
						(val->dmvQpelPenalty[i + 3]));
		}
	}

    /* Register with enable bit is written last */
    val->regMirror[14] |= ASIC_STATUS_ENABLE;

//    EWLEnableHW(ewl, HSWREG(14), val->regMirror[14]);
    H264encFlushRegs(val);

}

void H264encFlushRegs(regValues_s * val)
{
    VPU_DEBUG("H264encFlushRegs\n");
	u32 registerlen = 0;
	
	/*{
		int i;
		for(i=0;i<164;i++)
		{
			printf("val->regMirror[%d]=0x%x\n", i, val->regMirror[i]);
		}
	}*/

	if(val->vpuid == 0x30)
		registerlen = 164;
	else
		registerlen = ENC8270_REGISTERS;
	
    if (VPUClientSendReg(val->socket, val->regMirror, /*ENC8270_REGISTERS*/registerlen))
        VPU_DEBUG("H264encFlushRegs fail\n");
    else
        VPU_DEBUG("H264encFlushRegs success\n");
}

#if 0
/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
void EncAsicFrameContinue(const void *ewl, regValues_s * val)
{
    /* clear status bits, clear IRQ => HW restart */
    u32 status = val->regMirror[1];

    status &= (~ASIC_STATUS_ALL);
    status &= ~ASIC_IRQ_LINE;

    val->regMirror[1] = status;

    /*CheckRegisterValues(val); */

    /* Write only registers which may be updated mid frame */
    EWLWriteReg(ewl, HSWREG(24), (val->rlcLimitSpace / 2));

    val->regMirror[5] = val->rlcBase;
    EWLWriteReg(ewl, HSWREG(5), val->regMirror[5]);

    /* Register with status bits is written last */
    EWLEnableHW(ewl, HSWREG(1), val->regMirror[1]);

#ifdef TRACE_REGS
    EncTraceRegs(ewl, 0);
#endif

}
#endif

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
void EncAsicGetRegisters(const void *ewl, regValues_s * val)
{
    u32 word;

    val->outputStrmSize = val->regMirror[24]/8;//EWLReadReg(ewl, HSWREG(24)) / 8;  /* bytes */

    if(val->codingType != ASIC_JPEG && val->cpTarget != NULL)
    {
        /* video coding with MB rate control ON */
        u32 i, reg = 28;
        u32 cpt_prev = 0;
        u32 overflow = 0;

        for(i = 0; i < 10; i += 2, reg += 1)
        {
            u32 cpt;

            word = val->regMirror[reg];//EWLReadReg(ewl, reg);    /* 2 results per reg */

            /* first result from reg */
            cpt = (word >> 16) * 4;

            /* detect any overflow */
            if(cpt < cpt_prev)
                overflow += (1 << 18);
            cpt_prev = cpt;

            val->cpTargetResults[i] = cpt + overflow;

            /* second result from same reg */
            cpt = (word & mask_16b) * 4;

            /* detect any overflow */
            if(cpt < cpt_prev)
                overflow += (1 << 18);
            cpt_prev = cpt;

            val->cpTargetResults[i + 1] = cpt + overflow;
        }
    }

    /* QP sum div2, 18 bits */
    val->qpSum = (val->regMirror[25] & 0x3FFFF) * 2;//(EWLReadReg(ewl, HSWREG(25)) & 0x3FFFF) * 2;

    /* MAD MB count, 13 bits */
    val->madCount = (val->regMirror[38] & 0x3FFE000) >> 13;

    /* Non-zero coefficient count, 22-bits from swreg37 */
    val->rlcCount = val->regMirror[37] & 0x3FFFFF;

    /* get stabilization results if needed */
    /*if(val->vsMode != 0)
    {
        i32 i;

        for(i = 40; i <= 50; i++)
        {
            val->regMirror[i] = EWLReadReg(ewl, HSWREG(i));
        }
    }*/
}

void EncAsicGetRegisters_new(const void *ewl, regValues_s * val)
{

    /* HW output stream size, bits to bytes */
    val->outputStrmSize =
            val->regMirror[24]/8;//EncAsicGetRegisterValue(ewl, val->regMirror, HEncStrmBufLimit) / 8;

    if(val->codingType != ASIC_JPEG && val->cpTarget != NULL)
    {
        /* video coding with MB rate control ON */
        u32 i,reg=28;
        u32 cpt_prev = 0;
        u32 overflow = 0;

        for(i = 0; i < 10; i++)
        {
            u32 cpt;

            /* Checkpoint result div32 */
            cpt = ((val->regMirror[reg]>>(16-16*(i&1)))&0xffff)*32;//EncAsicGetRegisterValue(ewl, val->regMirror,
                                    //(regName)(HEncCP1WordTarget+i)) * 32;

            /* detect any overflow, overflow div32 is 65536 => overflow at 21 bits */
            if(cpt < cpt_prev)
                overflow += (1 << 21);
            cpt_prev = cpt;

            val->cpTargetResults[i] = cpt + overflow;
			reg = reg + (i&1);
        }
    }

    /* QP sum div2 */
    val->qpSum = (val->regMirror[25] & 0x001fffff)*2;//EncAsicGetRegisterValue(ewl, val->regMirror, HEncQpSum) * 2;

    /* MAD MB count*/
    val->madCount = (val->regMirror[38] & 0xffff0000) >> 16;//EncAsicGetRegisterValue(ewl, val->regMirror, HEncMadCount);

    /* Non-zero coefficient count*/
    val->rlcCount = (val->regMirror[37] & 0x007fffff)*4;//EncAsicGetRegisterValue(ewl, val->regMirror, HEncRlcSum) * 4;

    /* get stabilization results if needed */
    if(val->vsMode != 0)
    {
        i32 i;
		#if 0
        for(i = 40; i <= 50; i++)
        {
            val->regMirror[i] = EWLReadReg(ewl, HSWREG(i));
        }
		#endif
    }

#ifdef TRACE_REGS
    EncTraceRegs(ewl, 1, EncAsicGetRegisterValue(ewl, val->regMirror, HEncMbCount));
#endif

}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
void EncAsicStop(const void *ewl)
{
//    EWLDisableHW(ewl, HSWREG(14), 0);
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
u32 EncAsicGetStatus(regValues_s *val)
{
    return val->regMirror[1];
}

/*------------------------------------------------------------------------------

        ExpGolombSigned

------------------------------------------------------------------------------*/
i32 ExpGolombSigned(i32 val)
{
        i32 tmp = 0;

        if (val > 0) {
                val = 2*val;
        } else {
                val = -2*val + 1;
        }

        while (val >> ++tmp);

        /*
        H264NalBits(stream, val, tmp*2-1);
        */

        return tmp*2-1;
}


