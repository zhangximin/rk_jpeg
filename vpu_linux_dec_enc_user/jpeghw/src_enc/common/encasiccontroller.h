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
--  $RCSfile: encasiccontroller.h,v $
--  $Revision: 1.4 $
--
------------------------------------------------------------------------------*/
#ifndef __ENC_ASIC_CONTROLLER_H__
#define __ENC_ASIC_CONTROLLER_H__

#include "vpu_type.h"
#include "enccfg.h"
#include "ewl.h"


/* HW status register bits */
#define ASIC_STATUS_ALL                 0x1FD

#define ASIC_STATUS_IRQ_INTERVAL        0x100
#define ASIC_STATUS_TEST_IRQ2           0x080
#define ASIC_STATUS_TEST_IRQ1           0x040
#define ASIC_STATUS_BUFF_FULL           0x020
#define ASIC_STATUS_HW_RESET            0x010
#define ASIC_STATUS_ERROR               0x008
#define ASIC_STATUS_FRAME_READY         0x004

#define ASIC_IRQ_LINE                   0x001

#define ASIC_STATUS_ENABLE              0x001

#define ASIC_H264_BYTE_STREAM           0x00
#define ASIC_H264_NAL_UNIT              0x01

#define ASIC_INPUT_YUV420PLANAR         0x00
#define ASIC_INPUT_YUV420SEMIPLANAR     0x01
#define ASIC_INPUT_YUYV422INTERLEAVED   0x02
#define ASIC_INPUT_UYVY422INTERLEAVED   0x03
#define ASIC_INPUT_RGB565               0x04
#define ASIC_INPUT_RGB555               0x05
#define ASIC_INPUT_RGB444               0x06
#define ASIC_INPUT_RGB888               0x07
#define ASIC_INPUT_RGB101010            0x08

typedef enum
{
    IDLE = 0,   /* Initial state, both HW and SW disabled */
    HWON_SWOFF, /* HW processing, SW waiting for HW */
    HWON_SWON,  /* Both HW and SW processing */
    HWOFF_SWON, /* HW is paused or disabled, SW is processing */
    DONE
} bufferState_e;

typedef enum
{
    ASIC_MPEG4 = 0,
    ASIC_H263 = 1,
    ASIC_JPEG = 2,
    ASIC_H264 = 3
} asicCodingType_e;

typedef enum
{
    ASIC_P_16x16 = 0,
    ASIC_P_16x8 = 1,
    ASIC_P_8x16 = 2,
    ASIC_P_8x8 = 3,
    ASIC_I_4x4 = 4,
    ASIC_I_16x16 = 5
} asicMbType_e;

typedef enum
{
    ASIC_INTER = 0,
    ASIC_INTRA = 1
} asicFrameCodingType_e;

typedef struct
{
    RK_U32 irqDisable;
    RK_U32 irqInterval;
    RK_U32 mbsInCol;
    RK_U32 mbsInRow;
    RK_U32 qp;
    RK_U32 qpMin;
    RK_U32 qpMax;
    RK_U32 constrainedIntraPrediction;
    RK_U32 roundingCtrl;
    RK_U32 frameCodingType;
    RK_U32 codingType;
    RK_U32 pixelsOnRow;
    RK_U32 xFill;
    RK_U32 yFill;
    RK_U32 ppsId;
    RK_U32 idrPicId;
    RK_U32 frameNum;
    RK_U32 picInitQp;
    RK_S32 sliceAlphaOffset;
    RK_S32 sliceBetaOffset;
    RK_U32 filterDisable;
    RK_U32 transform8x8Mode;
    RK_U32 enableCabac;
    RK_U32 cabacInitIdc;
    RK_S32 chromaQpIndexOffset;
    RK_U32 sliceSizeMbRows;
    RK_U32 inputImageFormat;
    RK_U32 inputImageRotation;
    RK_U32 outputStrmBase;
    RK_U32 outputStrmSize;
    RK_U32 firstFreeBit;
    RK_U32 strmStartMSB;
    RK_U32 strmStartLSB;
    RK_U32 rlcBase;
    RK_U32 rlcLimitSpace;
    RK_U32 socket;
    
    union
    {
        RK_U32 nal;
        RK_U32 vp;
        RK_U32 gob;
    } sizeTblBase;
    RK_U32 internalImageLumBaseW;
    RK_U32 internalImageChrBaseW;
    RK_U32 internalImageLumBaseR;
    RK_U32 internalImageChrBaseR;
    RK_U32 inputLumBase;
    RK_U32 inputCbBase;
    RK_U32 inputCrBase;
    RK_U32 cpDistanceMbs;
    RK_U32 *cpTargetResults;
    const RK_U32 *cpTarget;
    const RK_S32 *targetError;
    const RK_S32 *deltaQp;
    RK_U32 rlcCount;
    RK_U32 qpSum;
    RK_U32 h264StrmMode;   /* 0 - byte stream, 1 - NAL units */
    RK_U32 sizeTblPresent;
    RK_U32 gobHeaderMask;
    RK_U32 gobFrameId;
    RK_U8 quantTable[8 * 8 * 2];
    RK_U32 jpegMode;
    RK_U32 jpegSliceEnable;
    RK_U32 jpegRestartInterval;
    RK_U32 jpegRestartMarker;
    RK_U32 regMirror[164];//[96];//[164];//rk30 change to 164, we set to max
    RK_U32 inputLumaBaseOffset;
    RK_U32 inputChromaBaseOffset;
    RK_U32 h264Inter4x4Disabled;
    RK_U32 disableQuarterPixelMv;
    RK_U32 vsNextLumaBase;
    RK_U32 vsMode;
    RK_U32 vpSize;
    RK_U32 vpMbBits;
    RK_U32 hec;
    RK_U32 moduloTimeBase;
    RK_U32 intraDcVlcThr;
    RK_U32 vopFcode;
    RK_U32 timeInc;
    RK_U32 timeIncBits;
    RK_U32 asicCfgReg;
    RK_U32 asicHwId;
    RK_U32 intra16Favor;
    RK_U32 interFavor;
    RK_U32 skipPenalty;
    RK_S32 madQpDelta;
    RK_U32 madThreshold;
    RK_U32 madCount;
    RK_U32 riceEnable;
    RK_U32 riceReadBase;
    RK_U32 riceWriteBase;
    RK_U32 cabacCtxBase;
    RK_U32 colorConversionCoeffA;
    RK_U32 colorConversionCoeffB;
    RK_U32 colorConversionCoeffC;
    RK_U32 colorConversionCoeffE;
    RK_U32 colorConversionCoeffF;
    RK_U32 rMaskMsb;
    RK_U32 gMaskMsb;
    RK_U32 bMaskMsb;
#ifdef ASIC_WAVE_TRACE_TRIGGER
    RK_U32 vop_count;
#endif
} regValues_s;

typedef struct
{
    const void *ewl;
    regValues_s regs;
    VPUMemLinear_t internalImageLuma[2];
    VPUMemLinear_t internalImageChroma[2];
    VPUMemLinear_t cabacCtx;
    VPUMemLinear_t riceRead;
    VPUMemLinear_t riceWrite;
    RK_U32 sizeTblSize;
    union
    {
        VPUMemLinear_t nal;
        VPUMemLinear_t vp;
        VPUMemLinear_t gob;
    } sizeTbl;
} asicData_s;

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/
RK_S32 EncAsicControllerInit(asicData_s * asic);

RK_S32 EncAsicMemAlloc_V2(asicData_s * asic, RK_U32 width, RK_U32 height,
                       RK_U32 encodingType);
void EncAsicMemFree_V2(asicData_s * asic);

/* Functions for controlling ASIC */
void EncAsicSetQuantTable(asicData_s * asic,
                          const RK_U8 * lumTable, const RK_U8 * chTable);

void EncAsicGetRegisters(const void *ewl, regValues_s * val);
RK_U32 EncAsicGetStatus(regValues_s *val);

RK_U32 EncAsicGetId(const void *ewl);

void EncAsicFrameStart(const void *ewl, regValues_s * val);

void EncAsicStop(const void *ewl);

void EncAsicRecycleInternalImage(regValues_s * val);

RK_S32 EncAsicCheckStatus_V2(asicData_s * asic);

#ifdef MPEG4_HW_RLC_MODE_ENABLED

RK_S32 EncAsicMemAlloc(asicData_s * asic, RK_U32 width, RK_U32 height, RK_U32 rlcBufSize);
void EncAsicMemFree(asicData_s * asic);

/* Functions for parsing data from ASIC output tables */
asicMbType_e EncAsicMbType(const RK_U32 * control);
RK_S32 EncAsicQp(const RK_U32 * control);
void EncAsicMv(const RK_U32 * control, RK_S8 mv[4], RK_S32 xy);
void EncAsicDc(RK_S32 * mbDc, const RK_U32 * control);
RK_S32 EncAsicRlcCount(const RK_U32 * mbRlc[6], RK_S32 mbRlcCount[6],
                    const RK_U32 * rlcData, const RK_U32 * control);

void EncAsicFrameContinue(const void *ewl, regValues_s * val);

RK_S32 EncAsicCheckStatus(asicData_s * asic);

#endif /* MPEG4_HW_RLC_MODE_ENABLED */

#endif
