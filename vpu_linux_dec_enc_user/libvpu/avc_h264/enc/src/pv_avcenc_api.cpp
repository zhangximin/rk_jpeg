#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include "framemanager.h"
#include "pv_avcenc_api.h"
#include "ewl.h"
#define LOG_TAG "pv_avcenc_api"
#include <utils/Log.h>

/* For command line structure */
#include "H264TestBench.h"

/* For parameter parsing */
#include "EncGetOption.h"

/* For accessing the EWL instance inside the encoder */
#include "H264Instance.h"

/* For compiler flags, test data, debug and tracing */
#include "enccommon.h"

//#include <cutils/properties.h>

#ifdef INTERNAL_TEST
#include "h264encapi_ext.h"
#endif

/* For memset, strcpy and strlen */
#include <string.h>

#include "vpu_mem.h"

#ifdef USE_EFENCE
#include "efence.h"
#endif

#define NALU_TABLE_SIZE     ((1920/16) + 3)
#define MAX_GOP_LEN 150

#define NO_INPUT_READ

static char *streamType[2] = { "BYTE_STREAM", "NAL_UNITS" };

/*------------------------------------------------------------------------------

    FreeRes

    Release all resources allcoated byt AllocRes()

------------------------------------------------------------------------------*/
void On2_AvcEncoder::FreeRes()
{
    if(pictureMem.vir_addr != NULL)
        VPUFreeLinear(&pictureMem);
    if(pictureStabMem.vir_addr != NULL)
        VPUFreeLinear(&pictureStabMem);
    if(outbufMem.vir_addr != NULL)
        VPUFreeLinear(&outbufMem);
}



i32 NextPic(i32 inputRateNumer, i32 inputRateDenom, i32 outputRateNumer,
            i32 outputRateDenom, i32 frameCnt, i32 firstPic)
{
    u32 sift;
    u32 skip;
    u32 numer;
    u32 denom;
    u32 next;

    numer = (u32) inputRateNumer *(u32) outputRateDenom;
    denom = (u32) inputRateDenom *(u32) outputRateNumer;

    if(numer >= denom)
    {
        sift = 9;
        do
        {
            sift--;
        }
        while(((numer << sift) >> sift) != numer);
    }
    else
    {
        sift = 17;
        do
        {
            sift--;
        }
        while(((numer << sift) >> sift) != numer);
    }
    skip = (numer << sift) / denom;
    next = (((u32) frameCnt * skip) >> sift) + (u32) firstPic;

    return (i32) next;
}


void PrintErrorValue(char *errorDesc, u32 retVal)
{
    char * str;

    switch (retVal)
    {
        case H264ENC_ERROR: str = "H264ENC_ERROR"; break;
        case H264ENC_NULL_ARGUMENT: str = "H264ENC_NULL_ARGUMENT"; break;
        case H264ENC_INVALID_ARGUMENT: str = "H264ENC_INVALID_ARGUMENT"; break;
        case H264ENC_MEMORY_ERROR: str = "H264ENC_MEMORY_ERROR"; break;
        case H264ENC_EWL_ERROR: str = "H264ENC_EWL_ERROR"; break;
        case H264ENC_EWL_MEMORY_ERROR: str = "H264ENC_EWL_MEMORY_ERROR"; break;
        case H264ENC_INVALID_STATUS: str = "H264ENC_INVALID_STATUS"; break;
        case H264ENC_OUTPUT_BUFFER_OVERFLOW: str = "H264ENC_OUTPUT_BUFFER_OVERFLOW"; break;
        case H264ENC_HW_BUS_ERROR: str = "H264ENC_HW_BUS_ERROR"; break;
        case H264ENC_HW_DATA_ERROR: str = "H264ENC_HW_DATA_ERROR"; break;
        case H264ENC_HW_TIMEOUT: str = "H264ENC_HW_TIMEOUT"; break;
        case H264ENC_HW_RESERVED: str = "H264ENC_HW_RESERVED"; break;
        case H264ENC_SYSTEM_ERROR: str = "H264ENC_SYSTEM_ERROR"; break;
        case H264ENC_INSTANCE_ERROR: str = "H264ENC_INSTANCE_ERROR"; break;
        case H264ENC_HRD_ERROR: str = "H264ENC_HRD_ERROR"; break;
        case H264ENC_HW_RESET: str = "H264ENC_HW_RESET"; break;
        default: str = "UNDEFINED";
    }

    ALOGV( "%s Return value: %s\n", errorDesc, str);
}

/*------------------------------------------------------------------------------
    PrintPSNR
        Calculate and print frame PSNR
------------------------------------------------------------------------------*/
u32 PrintPSNR(u8 *a, u8 *b, i32 scanline, i32 wdh, i32 hgt)
{
#ifdef PSNR
	float mse = 0.0;
	u32 tmp, i, j;

	for (j = 0 ; j < hgt; j++) {
		for (i = 0; i < wdh; i++) {
			tmp = a[i] - b[i];
			tmp *= tmp;
			mse += tmp;
		}
		a += scanline;
		b += wdh;
	}
	mse /= wdh * hgt;

	if (mse == 0.0) {
		ALOGV("--.-- | ");
	} else {
                mse = 10.0 * log10f(65025.0 / mse);
		ALOGV("%5.2f | ", mse);
	}

        return (u32)roundf(mse*100);
#else
	ALOGV("xx.xx | ");
        return 0;
#endif
}

int Parameter(commandLine_s * cml)
{
    i32 ret, i;
    char *optArg;
    argument_s argument;
    i32 status = 0;
    i32 bpsAdjustCount = 0;

    memset(cml, 0, sizeof(commandLine_s));

 //   cml->input = input;
 //   cml->output = output;
    cml->firstPic = 0;
    cml->lastPic = 0xffff;
    cml->lumWidthSrc = 1280;
    cml->lumHeightSrc = 720;

    cml->width = 1280;  //output width
    cml->height = 720;  //output height
    cml->horOffsetSrc = 0;  //Output image horizontal offset
    cml->verOffsetSrc = 0;  //Output image vertical offset

    cml->outputRateNumer = 30;
    cml->outputRateDenom = 1;
    cml->inputRateNumer = 30;
    cml->inputRateDenom = 1;

    cml->level = H264ENC_LEVEL_4_0;
    cml->byteStream = 1;    //nal stream:1; byte stream:0;

    cml->sei = 0;       //Enable/Disable SEI messages.[0]

    cml->rotation = 0;
    cml->inputFormat = H264ENC_YUV420_SEMIPLANAR;//H264ENC_BGR888;//H264ENC_YUV420_SEMIPLANAR;//H264ENC_BGR888;//H264ENC_RGB565;//H264ENC_YUV420_SEMIPLANAR;   //YUV420
    cml->colorConversion = 0;   //RGB to YCbCr color conversion type---BT.601
    cml->videoRange = 0;    //0..1 Video range.

    cml->videoStab = 0;    //Video stabilization. n > 0 enabled [0]

    cml->constIntraPred = 0;   //0=OFF, 1=ON Constrained intra pred flag [0]
    cml->disableDeblocking = 0;   //0..2 Value of disable_deblocking_filter_idc [0]
    cml->intraPicRate = 30;   //Intra picture rate. [0]
    cml->mbPerSlice = 0;   //Slice size in macroblocks. Should be a multiple of MBs per row. [0]


    cml->trans8x8 = 0;          //0=OFF, 1=Adaptive, 2=ON [0]\n"
    cml->enableCabac = 0;       //0=OFF, 1=ON [0]\n"
    cml->cabacInitIdc = 0;      //0..2\n");


    cml->bitPerSecond  = 4 * 1024 * 1024 / 2;         //Bitrate, 10000..levelMax [64000]\n"
    cml->picRc = 0;                      //0=OFF, 1=ON Picture rate control. [1]\n"
    cml->mbRc = 0;                       //0=OFF, 1=ON Mb rc (Check point rc). [1]\n"
    cml->hrdConformance =  0 ;        //0=OFF, 1=ON HRD conformance. [0]\n"
    cml->cpbSize       =   30000000 ;       //HRD Coded Picture Buffer size in bits. [0]\n"
    cml->gopLength     =   cml->intraPicRate;       //GOP length, 1..150 [intraPicRate]\n");


    cml->qpMin = 18;
    cml->qpHdr = 26;
    cml->qpMax = 30;

    cml->intraQpDelta = 4;//-3; //Intra QP delta
    cml->fixedIntraQp = 0;    //0..51, Fixed Intra QP, 0 = disabled. [0]
    cml->mbQpAdjustment = 4;    //-8..7, MAD based MB QP adjustment, 0 = disabled

    cml->userData  = NULL;        //SEI User data file name.\n"
//    cml->bpsAdjust = 0;        // Frame:bitrate for adjusting bitrate on the fly.\n"
    cml->psnr      = 0;        //Enables PSNR calculation for each frame.\n");


    cml->chromaQpOffset = 2;
    cml->filterOffsetA = 0;
    cml->filterOffsetB = 0;
    cml->burst = 16;        //burstSize
    cml->bursttype = 0;     //0=SIGLE, 1=INCR HW bus burst type. [0]
    cml->quarterPixelMv = 1;
    cml->testId = 0;
   // trigger_point = -1;      //Logic Analyzer trigger at picture <n>. [-1]

    cml->picSkip = 0;
  //  testParam = cml->testParam = 0;

    return 0;
}

void CloseEncoder(H264EncInst encoder)
{
    H264EncRet ret;
    h264Instance_s *pEnc264;
    pEnc264 = (h264Instance_s *)encoder;
    VPUClientRelease(pEnc264->asic.regs.socket);

    if((ret = H264EncRelease(encoder)) != H264ENC_OK)
    {
        PrintErrorValue("H264EncRelease() failed.", ret);
    }
}

int OpenEncoder(commandLine_s * cml, h264Instance_s * pEnc)
{
    H264EncRet ret;
    H264EncConfig cfg;
    H264EncCodingCtrl codingCfg;
    H264EncRateCtrl rcCfg;
    H264EncPreProcessingCfg preProcCfg;

    H264EncInst encoder;
    h264Instance_s *pEnc264;
	u32 vpuid;

    /* Encoder initialization */
    if(cml->width == DEFAULT)
        cml->width = cml->lumWidthSrc;

    if(cml->height == DEFAULT)
        cml->height = cml->lumHeightSrc;

    /* outputRateNumer */
    if(cml->outputRateNumer == DEFAULT)
    {
        cml->outputRateNumer = cml->inputRateNumer;
    }

    /* outputRateDenom */
    if(cml->outputRateDenom == DEFAULT)
    {
        cml->outputRateDenom = cml->inputRateDenom;
    }

    if(cml->rotation)
    {
        cfg.width = cml->height;
        cfg.height = cml->width;
    }
    else
    {
        cfg.width = cml->width;
        cfg.height = cml->height;
    }

    cfg.frameRateDenom = cml->outputRateDenom;
    cfg.frameRateNum = cml->outputRateNumer;
    if(cml->byteStream)
        cfg.streamType = H264ENC_BYTE_STREAM;
    else
        cfg.streamType = H264ENC_NAL_UNIT_STREAM;

    cfg.level = H264ENC_LEVEL_3;

    if(cml->level != DEFAULT && cml->level != 0)
        cfg.level = (H264EncLevel)cml->level;

    ALOGV("Init config: size %dx%d   %d/%d fps  %s P&L %d\n",
         cfg.width, cfg.height, cfg.frameRateNum,
         cfg.frameRateDenom, streamType[cfg.streamType], cfg.level);

    if((ret = H264EncInit(&cfg, pEnc)) != H264ENC_OK)
    {
        PrintErrorValue("H264EncInit() failed.", ret);
        return -1;
    }

    encoder = (H264EncInst)pEnc;

    //get socket
    pEnc264 = (h264Instance_s *)encoder;
    pEnc264->asic.regs.socket = VPUClientInit(VPU_ENC);

	{
		VPUHwEncConfig_t hwCfg;
		memset(&hwCfg, 0, sizeof(VPUHwEncConfig_t));
		if (VPUClientGetHwCfg(pEnc264->asic.regs.socket, (RK_U32*)&hwCfg, sizeof(hwCfg)))
	    {
	        ALOGE("h264enc # Get HwCfg failed\n");
	        VPUClientRelease(pEnc264->asic.regs.socket);
	        return -1;
	    }

		if(hwCfg.reg_size == (164*4))
			vpuid = 0x30;
		else
			vpuid = 0x29;
	}

	//remark vpuid 30 or 29
	pEnc264->asic.regs.vpuid = vpuid;

    /* Encoder setup: rate control */
    if((ret = H264EncGetRateCtrl(encoder, &rcCfg)) != H264ENC_OK)
    {
        PrintErrorValue("H264EncGetRateCtrl() failed.", ret);
        CloseEncoder(encoder);
        return -1;
    }
    else
    {
        ALOGV("Get rate control: qp %2d [%2d, %2d]  %8d bps  "
               "pic %d mb %d skip %d  hrd %d\n  cpbSize %d gopLen %d "
               "intraQpDelta %2d\n",
               rcCfg.qpHdr, rcCfg.qpMin, rcCfg.qpMax, rcCfg.bitPerSecond,
               rcCfg.pictureRc, rcCfg.mbRc, rcCfg.pictureSkip, rcCfg.hrd,
               rcCfg.hrdCpbSize, rcCfg.gopLen, rcCfg.intraQpDelta);

        if(cml->qpHdr != DEFAULT)
            rcCfg.qpHdr = cml->qpHdr;
        if(cml->qpMin != DEFAULT)
            rcCfg.qpMin = cml->qpMin;
        if(cml->qpMax != DEFAULT)
            rcCfg.qpMax = cml->qpMax;
        if(cml->picSkip != DEFAULT)
            rcCfg.pictureSkip = cml->picSkip;
        if(cml->picRc != DEFAULT)
            rcCfg.pictureRc = cml->picRc;
        if(cml->mbRc != DEFAULT)
            rcCfg.mbRc = cml->mbRc != 0 ? 1 : 0;
        if(cml->bitPerSecond != DEFAULT)
            rcCfg.bitPerSecond = cml->bitPerSecond;

        if(cml->hrdConformance != DEFAULT)
            rcCfg.hrd = cml->hrdConformance;

        if(cml->cpbSize != DEFAULT)
            rcCfg.hrdCpbSize = cml->cpbSize;

        if(cml->intraPicRate != 0)
            rcCfg.gopLen = MIN(cml->intraPicRate, MAX_GOP_LEN);

        if(cml->gopLength != DEFAULT)
            rcCfg.gopLen = cml->gopLength;

        if(cml->intraQpDelta != DEFAULT)
            rcCfg.intraQpDelta = cml->intraQpDelta;

        rcCfg.fixedIntraQp = cml->fixedIntraQp;
        rcCfg.mbQpAdjustment = cml->mbQpAdjustment;

        ALOGV("Set rate control: qp %2d [%2d, %2d] %8d bps  "
               "pic %d mb %d skip %d  hrd %d\n"
               "  cpbSize %d gopLen %d intraQpDelta %2d "
               "fixedIntraQp %2d mbQpAdjustment %d\n",
               rcCfg.qpHdr, rcCfg.qpMin, rcCfg.qpMax, rcCfg.bitPerSecond,
               rcCfg.pictureRc, rcCfg.mbRc, rcCfg.pictureSkip, rcCfg.hrd,
               rcCfg.hrdCpbSize, rcCfg.gopLen, rcCfg.intraQpDelta,
               rcCfg.fixedIntraQp, rcCfg.mbQpAdjustment);

        if((ret = H264EncSetRateCtrl(encoder, &rcCfg)) != H264ENC_OK)
        {
            PrintErrorValue("H264EncSetRateCtrl() failed.", ret);
            CloseEncoder(encoder);
            return -1;
        }
    }

    /* Encoder setup: coding control */
    if((ret = H264EncGetCodingCtrl(encoder, &codingCfg)) != H264ENC_OK)
    {
        PrintErrorValue("H264EncGetCodingCtrl() failed.", ret);
        CloseEncoder(encoder);
        return -1;
    }
    else
    {
        if(cml->mbPerSlice != DEFAULT)
        {
            codingCfg.sliceSize = cml->mbPerSlice / (cfg.width / 16);
        }

        if(cml->constIntraPred != DEFAULT)
        {
            if(cml->constIntraPred != 0)
                codingCfg.constrainedIntraPrediction = 1;
            else
                codingCfg.constrainedIntraPrediction = 0;
        }

        if(cml->disableDeblocking != 0)
            codingCfg.disableDeblockingFilter = 1;
        else
            codingCfg.disableDeblockingFilter = 0;

        if((cml->disableDeblocking != 1) &&
           ((cml->filterOffsetA != 0) || (cml->filterOffsetB != 0)))
            codingCfg.disableDeblockingFilter = 1;

        if(cml->enableCabac != DEFAULT)
        {
            codingCfg.enableCabac = cml->enableCabac;
            if (cml->cabacInitIdc != DEFAULT)
                codingCfg.cabacInitIdc = cml->cabacInitIdc;
        }

        codingCfg.transform8x8Mode = cml->trans8x8;

        if(cml->videoRange != DEFAULT)
        {
            if(cml->videoRange != 0)
                codingCfg.videoFullRange = 1;
            else
                codingCfg.videoFullRange = 0;
        }

        if(cml->sei)
            codingCfg.seiMessages = 1;
        else
            codingCfg.seiMessages = 0;

        ALOGV
            ("Set coding control: SEI %d Slice %5d   deblocking %d "
             "constrained intra %d video range %d\n"
             "  cabac %d cabac initial idc %d Adaptive 8x8 transform %d\n",
             codingCfg.seiMessages, codingCfg.sliceSize,
             codingCfg.disableDeblockingFilter,
             codingCfg.constrainedIntraPrediction, codingCfg.videoFullRange, codingCfg.enableCabac,
             codingCfg.cabacInitIdc, codingCfg.transform8x8Mode );

        if((ret = H264EncSetCodingCtrl(encoder, &codingCfg)) != H264ENC_OK)
        {
            PrintErrorValue("H264EncSetCodingCtrl() failed.", ret);
            CloseEncoder(encoder);
            return -1;
        }

#ifdef INTERNAL_TEST
        /* Set some values outside the product API for internal
         * testing purposes */

        H264EncSetChromaQpIndexOffset(encoder, cml->chromaQpOffset);
        ALOGV("Set ChromaQpIndexOffset: %d\n", cml->chromaQpOffset);

        H264EncSetHwBurstSize(encoder, cml->burst);
        ALOGV("Set HW Burst Size: %d\n", cml->burst);
        H264EncSetHwBurstType(encoder, cml->bursttype);
        ALOGV("Set HW Burst Type: %d\n", cml->bursttype);
        if(codingCfg.disableDeblockingFilter == 1)
        {
            H264EncFilter advCoding;

            H264EncGetFilter(encoder, &advCoding);

            advCoding.disableDeblocking = cml->disableDeblocking;

            advCoding.filterOffsetA = cml->filterOffsetA * 2;
            advCoding.filterOffsetB = cml->filterOffsetB * 2;

            ALOGV
                ("Set filter params: disableDeblocking %d filterOffsetA = %i filterOffsetB = %i\n",
                 advCoding.disableDeblocking, advCoding.filterOffsetA,
                 advCoding.filterOffsetB);

            ret = H264EncSetFilter(encoder, &advCoding);
            if(ret != H264ENC_OK)
            {
                PrintErrorValue("H264EncSetFilter() failed.", ret);
                CloseEncoder(encoder);
                return -1;
            }
        }
        if (cml->quarterPixelMv != DEFAULT) {
            H264EncSetQuarterPixelMv(encoder, cml->quarterPixelMv);
            ALOGV("Set Quarter Pixel MV: %d\n", cml->quarterPixelMv);
        }
#endif

    }

    /* PreP setup */
    if((ret = H264EncGetPreProcessing(encoder, &preProcCfg)) != H264ENC_OK)
    {
        PrintErrorValue("H264EncGetPreProcessing() failed.", ret);
        CloseEncoder(encoder);
        return -1;
    }
    ALOGV
        ("Get PreP: input %4dx%d : offset %4dx%d : format %d : rotation %d "
           ": stab %d : cc %d\n",
         preProcCfg.origWidth, preProcCfg.origHeight, preProcCfg.xOffset,
         preProcCfg.yOffset, preProcCfg.inputType, preProcCfg.rotation,
         preProcCfg.videoStabilization, preProcCfg.colorConversion.type);

    preProcCfg.inputType = (H264EncPictureType)cml->inputFormat;
    preProcCfg.rotation = (H264EncPictureRotation)cml->rotation;

    preProcCfg.origWidth =
        cml->lumWidthSrc /*(cml->lumWidthSrc + 15) & (~0x0F) */ ;
    preProcCfg.origHeight = cml->lumHeightSrc;

    if(cml->horOffsetSrc != DEFAULT)
        preProcCfg.xOffset = cml->horOffsetSrc;
    if(cml->verOffsetSrc != DEFAULT)
        preProcCfg.yOffset = cml->verOffsetSrc;
    if(cml->videoStab != DEFAULT)
        preProcCfg.videoStabilization = cml->videoStab;
    if(cml->colorConversion != DEFAULT)
        preProcCfg.colorConversion.type =
                        (H264EncColorConversionType)cml->colorConversion;
    if(preProcCfg.colorConversion.type == H264ENC_RGBTOYUV_USER_DEFINED)
    {
        preProcCfg.colorConversion.coeffA = 20000;
        preProcCfg.colorConversion.coeffB = 44000;
        preProcCfg.colorConversion.coeffC = 5000;
        preProcCfg.colorConversion.coeffE = 35000;
        preProcCfg.colorConversion.coeffF = 38000;
    }

    ALOGV
        ("Set PreP: input %4dx%d : offset %4dx%d : format %d : rotation %d "
           ": stab %d : cc %d\n",
         preProcCfg.origWidth, preProcCfg.origHeight, preProcCfg.xOffset,
         preProcCfg.yOffset, preProcCfg.inputType, preProcCfg.rotation,
         preProcCfg.videoStabilization, preProcCfg.colorConversion.type);

    if((ret = H264EncSetPreProcessing(encoder, &preProcCfg)) != H264ENC_OK)
    {
        PrintErrorValue("H264EncSetPreProcessing() failed.", ret);
        CloseEncoder(encoder);
        return -1;
    }
    return 0;
}

On2_AvcEncoder:: On2_AvcEncoder()
{
	intraPeriodCnt = 0;
	codedFrameCnt = 0;
	next = 0;
	frameCnt = 0;
	streamSize = 0;
	bitrate = 0;
	psnrSum = 0;
	psnrCnt = 0;
	psnr = 0;
 #ifdef ENC_DEBUG
    fp1 = NULL;
    fp = NULL;
 #endif

	memset(&rc,0,sizeof(rc));
	memset(&cmdl,0,sizeof(cmdl));
	memset(&encIn,0,sizeof(encIn));
	memset(&encOut,0,sizeof(encOut));
	memset(&h264encInst,0,sizeof(h264encInst));

	memset(&pictureMem, 0, sizeof(pictureMem));
	memset(&pictureStabMem, 0, sizeof(pictureStabMem));
	memset(&outbufMem, 0, sizeof(outbufMem));
}

int On2_AvcEncoder::pv_on2avcencoder_init(AVCEncParams1 *aEncOption, uint8_t* aOutBuffer,uint32_t* aOutputLength)
{
 	commandLine_s *cml = &cmdl;
    H264EncRet ret;
    H264EncInst encoder = (H264EncInst)&h264encInst;
	u32 vpuid = 0x30;

    /* Parse command line parameters */
    if(Parameter(cml) != 0)
    {
        ALOGV( "Input parameter error\n");
        return -1;
    }
//add by csy
    cml->lumWidthSrc 		= aEncOption->width;
    cml->lumHeightSrc 		= aEncOption->height;
    cml->width 				= aEncOption->width;
    cml->height 			= aEncOption->height;
	cml->bitPerSecond  		= aEncOption->bitRate;                   //Bitrate, 10000..levelMax [64000]\n"
    cml->outputRateNumer 	= aEncOption->framerate;
    cml->inputRateNumer 	= aEncOption->framerate;
	cml->enableCabac 		= aEncOption->enableCabac;				//add cabac_flag
    cml->cabacInitIdc 		= aEncOption->cabacInitIdc;
	cml->framerateout		= aEncOption->framerateout;
    if(aEncOption->intraPicRate > 1){
        cml->intraPicRate = aEncOption->intraPicRate;
    }
	ALOGD("cml->cabacintIdc  %d cml->enablaCabac %d cml->intraPicRate %d",cml->cabacInitIdc ,cml->enableCabac ,cml->intraPicRate);
    /* Encoder initialization */
    cml->inputFormat = H264ENC_YUV420_SEMIPLANAR;//H264ENC_BGR888;//H264ENC_RGB565;//H264ENC_YUV420_SEMIPLANAR;   //YUV420
    if(aEncOption->format){
        cml->inputFormat = H264ENC_BGR888;
    }

    if(OpenEncoder(cml, &h264encInst) != 0)
    {
        ALOGV( "Open Encoder failure\n");
        return -1;
    }

    /* Set the test ID for internal testing,
     * the SW must be compiled with testing flags */
    H264EncSetTestId(encoder, cmdl.testId);

    /* Allocate input and output buffers */
    if(AllocRes() != 0)
    {
        ALOGV( "Failed to allocate the external resources!\n");
        FreeRes();
        CloseEncoder(encoder);
        return -1;
    }

    //h264encode init
    encIn.pNaluSizeBuf = NULL;
    encIn.naluSizeBufSize = 0;

    encIn.pOutBuf = outbufMem.vir_addr;
    encIn.busOutBuf = outbufMem.phy_addr;
    encIn.outBufSize = outbufMem.size;

    /* Allocate memory for NAL unit size buffer, optional */

    encIn.naluSizeBufSize = NALU_TABLE_SIZE * sizeof(u32);
    encIn.pNaluSizeBuf = (u32 *) malloc(encIn.naluSizeBufSize);

    VPUMemInvalidate(&outbufMem);

    if(!encIn.pNaluSizeBuf)
    {
        ALOGV("WARNING! Failed to allocate NAL unit size buffer.\n");
    }

    /* Source Image Size */
    if(cml->inputFormat <= H264ENC_YUV420_SEMIPLANAR)
    {
        src_img_size =
                cml->lumWidthSrc * cml->lumHeightSrc +
                2 * (((cml->lumWidthSrc + 1) >> 1) *
                ((cml->lumHeightSrc + 1) >> 1));
    }
    else if(cml->inputFormat <= H264ENC_BGR444)
    {
        /* 422 YUV or 16-bit RGB */
        src_img_size = cml->lumWidthSrc * cml->lumHeightSrc * 2;
    }
    else
    {
        /* 32-bit RGB */
        src_img_size = cml->lumWidthSrc * cml->lumHeightSrc * 4;
    }

	if(aEncOption->profileIdc==66 || aEncOption->profileIdc==77 || aEncOption->profileIdc==100)
		((h264Instance_s *)encoder)->seqParameterSet.profileIdc = aEncOption->profileIdc;
	if(aEncOption->levelIdc==41 || aEncOption->levelIdc==40 || aEncOption->levelIdc==32
		|| aEncOption->levelIdc==31 || aEncOption->levelIdc==30 || aEncOption->levelIdc==22
		|| aEncOption->levelIdc==21 || aEncOption->levelIdc==20 || aEncOption->levelIdc==13
		|| aEncOption->levelIdc==12 || aEncOption->levelIdc==11 || aEncOption->levelIdc==99
		|| aEncOption->levelIdc==10)
		((h264Instance_s *)encoder)->seqParameterSet.levelIdc = aEncOption->levelIdc;
    /* Start stream */
    ret = H264EncStrmStart(encoder, &encIn, &encOut);
    if(ret != H264ENC_OK)
    {
        PrintErrorValue("H264EncStrmStart() failed.", ret);
        return -1;
    }

//   fout = fopen(cml->output, "wb");

#ifdef ENC_DEBUG

    fp = fopen("/mnt/storage/test_dec.yuv", "rb");
    if(fp == NULL)
    {
        ALOGV( "Failed to create the output file.\n");
        return -1;
    }
/*    fp1 = fopen("/mnt/flash/H264enc.yuv", "wb+");
    if(fp1 == NULL)
    {
        ALOGV( "Failed to create the output file.\n");
        return -1;
    }*/

    #ifdef  VIDEOSTAB_ENABLED
    VPUMemInvalidate(&pictureMem);
    fread(pictureMem.vir_addr, 1, 1280*720*3/2, fp);
    VPUMemClean(&pictureMem);
    #endif

#endif

  // WriteStrm(fout, outbufMem.vir_addr, encOut.streamSize, 0);

    VPUMemClean(&outbufMem);
    VPUMemInvalidate(&outbufMem);
    memcpy(aOutBuffer,(uint8_t *)(outbufMem.vir_addr+1),(encOut.streamSize-4));
    *aOutputLength =  encOut.streamSize - 4;

#ifdef ENC_DEBUG
//    fwrite( outbufMem.vir_addr, 1, encOut.streamSize, fp);
#endif
/*    if(cml->byteStream == 0)
    {
        WriteNalSizesToFile(nal_sizes_file, encIn.pNaluSizeBuf,
                            encIn.naluSizeBufSize);
    }*/

    streamSize += encOut.streamSize;

    H264EncGetRateCtrl(encoder, &rc);

    /* Allocate a buffer for user data and read data from file */
 //   pUserData = ReadUserData(encoder, cml->userData);

    ALOGV("\n");
    ALOGV("Input | Pic | QP | Type |  BR avg  | ByteCnt (inst) |");
    if (cml->psnr)
        ALOGV(" PSNR  | NALU sizes\n");
    else
        ALOGV(" NALU sizes\n");

    ALOGV("------------------------------------------------------------------------\n");
    ALOGV("      |     | %2d | HDR  |          | %7i %6i | ",
            rc.qpHdr, streamSize, encOut.streamSize);
    if (cml->psnr)
        ALOGV("      | ");
//    PrintNalSizes(encIn.pNaluSizeBuf, (u8 *) outbufMem.vir_addr,
        //    encOut.streamSize, cml->byteStream);
    ALOGV("\n");

    /* Setup encoder input */
    {
        u32 w = (cml->lumWidthSrc + 15) & (~0x0f);

        encIn.busLuma = pictureMem.phy_addr;

        encIn.busChromaU = encIn.busLuma + (w * cml->lumHeightSrc);
        encIn.busChromaV = encIn.busChromaU +
            (((w + 1) >> 1) * ((cml->lumHeightSrc + 1) >> 1));
    }

    /* First frame is always intra with time increment = 0 */
    encIn.codingType = H264ENC_INTRA_FRAME;
    encIn.timeIncrement = 0;

    encIn.busLumaStab = pictureStabMem.phy_addr;

    intraPeriodCnt = cml->intraPicRate;
    ALOGD("init intraPeriodCnt %d",intraPeriodCnt);

	InframeCnt = 0;
	preframnum = -1;

    return 0;

}

int On2_AvcEncoder::pv_on2avcencoder_oneframe(uint8_t* aOutBuffer, uint32_t* aOutputLength,
        uint8_t *aInBuffer,uint32_t aInBuffPhy,uint32_t* aInBufSize,uint32_t* aOutTimeStamp,bool*  aSyncFlag )
{
    H264EncRet ret;
    commandLine_s *cml = &cmdl;
    int i;
    H264EncInst encoder = (H264EncInst)&h264encInst;

    /* Main encoding loop */
    if(1)
    {
    	if(h264encFramerateScale(cml->inputRateNumer, cml->framerateout))	//scale framerate
		{
			*aOutputLength = 0;	//skip curr frame
			ret = H264ENC_OK;
			return ret;
		}

        if(!aInBuffPhy){
            if(cml->inputFormat <= H264ENC_YUV420_SEMIPLANAR) {
                /* Input picture in planar YUV 4:2:0 format */
                memcpy((u8 *)pictureMem.vir_addr,
                        aInBuffer,
                        cml->lumWidthSrc * cml->lumHeightSrc * 3/2);
            } else if(cml->inputFormat <= H264ENC_BGR444) {
                /* Input picture in YUYV 4:2:2 or 16-bit RGB format */
                memcpy((u8 *)pictureMem.vir_addr,
                        aInBuffer,
                        cml->lumWidthSrc * cml->lumHeightSrc * 2);
            } else {
                /* Input picture in 32-bit RGB format */
                ALOGI("will copy w: %d, h: %d",
                    cml->lumWidthSrc, cml->lumHeightSrc);
                memcpy((u8 *)pictureMem.vir_addr,
                        aInBuffer,
                        cml->lumWidthSrc * cml->lumHeightSrc * 4);
            }
            VPUMemClean(&pictureMem);
        }
#ifdef  ENC_DEBUG
        VPUMemInvalidate(&pictureStabMem);
        fread(pictureStabMem.vir_addr, 1, 1280*720*3/2, fp);
        VPUMemClean(&pictureStabMem);
#endif

#ifdef  VIDEOSTAB_ENABLED
        if(frameCnt&1)
        {
           encIn.busLuma = pictureStabMem.phy_addr;//aInBuffPhy;
           encIn.busLumaStab = pictureMem.phy_addr;//aInBuffPhy;
        }else{
           encIn.busLuma = pictureMem.phy_addr;//aInBuffPhy;
           encIn.busLumaStab = pictureStabMem.phy_addr;//aInBuffPhy;
        }

        {
           u32 w = (cml->lumWidthSrc + 15) & (~0x0f);
           encIn.busChromaU = encIn.busLuma + (w * cml->lumHeightSrc);
           encIn.busChromaV = encIn.busChromaU +
               (((w + 1) >> 1) * ((cml->lumHeightSrc + 1) >> 1));
        }
#else
        {
           u32 w = (cml->lumWidthSrc + 15) & (~0x0f);
           if(!aInBuffPhy){
                encIn.busLuma = pictureMem.phy_addr;
           }else{
           encIn.busLuma = aInBuffPhy;
           }
           encIn.busChromaU = encIn.busLuma + (w * cml->lumHeightSrc);
           encIn.busChromaV = encIn.busChromaU +
               (((w + 1) >> 1) * ((cml->lumHeightSrc + 1) >> 1));
        }
#endif

        /* Select frame type */

        *aSyncFlag = 0;

        if((cml->intraPicRate != 0) && (intraPeriodCnt >= cml->intraPicRate))
        {
            encIn.codingType = H264ENC_INTRA_FRAME;
        }
        else
            encIn.codingType = H264ENC_PREDICTED_FRAME;


        if(encIn.codingType == H264ENC_INTRA_FRAME)
            intraPeriodCnt = 0;

        ret = H264EncStrmEncode(encoder, &encIn, &encOut);

        H264EncGetRateCtrl(encoder, &rc);

        streamSize += encOut.streamSize;

        switch (ret)
        {
        case H264ENC_FRAME_READY:

            ALOGV("%5i | %3i | %2i | %s | %8u | %7i %6i | ",
                next, frameCnt, rc.qpHdr,
                encOut.codingType == H264ENC_INTRA_FRAME ? " I  " :
                encOut.codingType == H264ENC_PREDICTED_FRAME ? " P  " : "skip",
                bitrate, streamSize, encOut.streamSize);
                if(encOut.codingType == H264ENC_INTRA_FRAME)
                {
                    *aSyncFlag = 1;
                }

            if (cml->psnr)
                psnr = PrintPSNR((u8 *)
                    (((h264Instance_s *)encoder)->asic.regs.inputLumBase +
                    ((h264Instance_s *)encoder)->asic.regs.inputLumaBaseOffset),
                    (u8 *)
                    (((h264Instance_s *)encoder)->asic.regs.internalImageLumBaseR),
                    cml->lumWidthSrc, cml->width, cml->height);
            if (psnr) {
                psnrSum += psnr;
                psnrCnt++;
            }
 //           PrintNalSizes(encIn.pNaluSizeBuf, (u8 *) outbufMem.vir_addr,
              //  encOut.streamSize, cml->byteStream);
            ALOGV("\n");
            //VPUMemClean(&outbufMem);

            ALOGV("outbufMem.vir_add = 0x%x outbufMem.phy_addr = 0x%x\n", outbufMem.vir_addr,outbufMem.phy_addr);
            VPUMemInvalidate(&outbufMem);
            memcpy(aOutBuffer,(uint8_t *)(outbufMem.vir_addr+1),(encOut.streamSize-4));
            *aOutputLength = encOut.streamSize - 4;
            ALOGV("encOut.streamSize = 0x%x\n",encOut.streamSize);

            break;

        case H264ENC_OUTPUT_BUFFER_OVERFLOW:
            ALOGV("%5i | %3i | %2i | %s | %8u | %7i %6i | \n",
                next, frameCnt, rc.qpHdr, "lost",
                bitrate, streamSize, encOut.streamSize);
            break;

        default:
            PrintErrorValue("H264EncStrmEncode() failed.", ret);
            /* For debugging, can be removed */
//            WriteStrm(fout, outbufMem.vir_addr, encOut.streamSize, 0);
            /* We try to continue encoding the next frame */
            break;
        }

        encIn.timeIncrement = cml->outputRateDenom;

        frameCnt++;

        if (encOut.codingType != H264ENC_NOTCODED_FRAME) {
            intraPeriodCnt++; codedFrameCnt++;
        }

    }else{
        return -10;
    }

    return ret;
}
void On2_AvcEncoder::H264encSetInputAddr(unsigned long input)
{
    commandLine_s *cml = &cmdl;
	u32 w = (cml->lumWidthSrc + 15) & (~0x0f);

	encIn.busLuma = input;
	encIn.busChromaU= input + (w * cml->lumHeightSrc);
	encIn.busChromaV = encIn.busChromaU +(((w + 1) >> 1) * ((cml->lumHeightSrc + 1) >> 1));
	return;
}
void On2_AvcEncoder::H264encSetintraPeriodCnt()
{
       intraPeriodCnt = cmdl.intraPicRate;
	return;
}
void On2_AvcEncoder::h264encSetmbperslice(int line_per_slice)
{
	h264Instance_s *pEncInst = &h264encInst;
	if(line_per_slice >= (cmdl.width>>4))
		return;

	pEncInst->slice.sliceSize = line_per_slice * pEncInst->mbPerRow;

	return;
}

int On2_AvcEncoder::H264encSetInputFormat(H264EncPictureType inputFormat)
{
	H264EncPreProcessingCfg preProcCfg;
	H264EncRet ret;
	H264EncInst encoder = (H264EncInst)&h264encInst;

	if((ret = H264EncGetPreProcessing(encoder, &preProcCfg)) != H264ENC_OK)
	{
		PrintErrorValue("H264EncGetPreProcessing() failed.", ret);
		return -1;
	}
	if(inputFormat!=preProcCfg.inputType) {
	    intraPeriodCnt = cmdl.intraPicRate;
	}

    commandLine_s *cml = &cmdl;
    if((inputFormat !=preProcCfg.inputType) &&
            (inputFormat >=H264ENC_YUV420_PLANAR) &&
            (inputFormat <=H264ENC_BGR101010)) {
	    intraPeriodCnt = cmdl.intraPicRate;
        if(pictureMem.vir_addr != NULL) {
            VPUFreeLinear(&pictureMem);
        }

        pictureMem.vir_addr = NULL;
        pictureStabMem.vir_addr = NULL;
        int32_t pic_size = 0;
        if(inputFormat <= H264ENC_YUV420_SEMIPLANAR) {
            /* Input picture in planar YUV 4:2:0 format */
            pic_size =
                ((cml->lumWidthSrc + 15) & (~15)) * cml->lumHeightSrc * 3 / 2;
        } else if(inputFormat <= H264ENC_BGR444) {
            /* Input picture in YUYV 4:2:2 or 16-bit RGB format */
            pic_size =
                ((cml->lumWidthSrc + 15) & (~15)) * cml->lumHeightSrc * 2;
        } else {
            /* Input picture in 32-bit RGB format */
            pic_size =
                ((cml->lumWidthSrc + 15) & (~15)) * cml->lumHeightSrc * 4;
        }

        if (VPUMallocLinear(&pictureMem, pic_size) != EWL_OK)
        {
            ALOGV( "Failed to allocate input picture!\n");
            pictureMem.vir_addr = NULL;
            return -1;
        }
        src_img_size = pic_size;
        cml->inputFormat = inputFormat;
	}
	preProcCfg.inputType = (H264EncPictureType)inputFormat;
	if((ret = H264EncSetPreProcessing(encoder, &preProcCfg)) != H264ENC_OK)
	{
		PrintErrorValue("H264EncSetPreProcessing() failed.", ret);
		return -1;
	}



	return 0;
}
void On2_AvcEncoder::pv_on2avcencoder_getconfig(AVCEncParams1 *vpug)
{
	h264Instance_s *pEncInst = &h264encInst;
	H264EncCodingCtrl pCodeParams;
	H264EncGetCodingCtrl(&h264encInst,  & pCodeParams);
    H264EncGetRateCtrl(&h264encInst, &rc);
	vpug->width 	= cmdl.lumWidthSrc;
	vpug->height	= cmdl.lumHeightSrc;
	vpug->bitRate 	= rc.bitPerSecond;                   //Bitrate, 10000..levelMax [64000]\n"
    vpug->rc_mode 	= rc.pictureRc | rc.mbRc;                           //0=OFF, 1=ON Picture rate control. [1]\n"
    vpug->framerate = pEncInst->rateControl.outRateNum;
	vpug->qp 		= rc.qpHdr;
	vpug->enableCabac	= pCodeParams.enableCabac;
	vpug->cabacInitIdc 	= pCodeParams.cabacInitIdc;
	vpug->intraPicRate  = cmdl.intraPicRate;
	memset(vpug->reserved, 0 ,sizeof(vpug->reserved));
ALOGV("pv_on2avcencoder_getconfig bitrate w %d h %d %d rc %d fps %d cabac_flag %d idc %d",
		vpug->width,vpug->height,vpug->bitRate,vpug->rc_mode,vpug->framerate,vpug->enableCabac,vpug->cabacInitIdc);
}
void On2_AvcEncoder::pv_on2avcencoder_setconfig(AVCEncParams1 *vpug)
{
    h264Instance_s *pEncInst = &h264encInst;
    u32 testid = 0;

   // ALOGE("pv_on2avcencoder_setconfig");


    //cmdl.hrdConformance = vpug->rc_mode;


    H264EncGetRateCtrl(&h264encInst, &rc);
	// ALOGE("rc bitrate w %d h %d bitrate %d rc %d fps %d  qphdr %d qpmax %d qpmin %d intraqp %d fixintraqp %d",

//	 vpug->width,vpug->height,vpug->bitRate,vpug->rc_mode,vpug->framerate,rc.qpHdr,rc.qpMin,rc.qpMax,rc.intraQpDelta ,
	//	 rc.fixedIntraQp);

	rc.hrdCpbSize = cmdl.cpbSize;
	rc.bitPerSecond = vpug->bitRate;
	rc.hrd = 0;	//not use hrd when set bitrate
	rc.mbRc = vpug->rc_mode;
	rc.pictureRc = vpug->rc_mode;
    rc.qpHdr = vpug->qp;
	pEncInst->rateControl.outRateNum = vpug->framerate;

    int ret;
	h264VirtualBuffer_s *vb = &pEncInst->rateControl.virtualBuffer;


    {
        ret = H264EncSetRateCtrl(&h264encInst, &rc);
		vb->virtualBitCnt = 0;
		vb->realBitCnt = 0;
		vb->picTimeInc = 0;
		vb->timeScale = vpug->framerate;
		encIn.timeIncrement = 0;
    }


ALOGV("pv_on2avcencoder_setconfig bitrate w %d h %d bitrate %d rc %d fps %d  qphdr %d qpmax %d qpmin %d intraqp %d ret %d fixintraqp %d testid %d vpug->enablaCabac %d",
	vpug->width,vpug->height,vpug->bitRate,vpug->rc_mode,vpug->framerate,cmdl.qpHdr,cmdl.qpMin,cmdl.qpMax,cmdl.intraQpDelta ,ret,
    cmdl.fixedIntraQp,testid,vpug->enableCabac);
}

int On2_AvcEncoder::h264encFramerateScale(int inframerate, int outframerate)
{
	int curframenum, ret=1;
	//ALOGV("inframerate=%d, outframerate=%d\n", inframerate, outframerate);
	if(outframerate<=0 || outframerate>30)	//check outframerate的值是否非法
		return 0;

	if(inframerate <= outframerate)	//只处理输入帧率大于输出帧率的case
		return 0;

	curframenum = ((InframeCnt*(outframerate<<8))/inframerate)>>8;

	InframeCnt++;

	if(curframenum != preframnum)
	{
		ret = 0;
		preframnum = curframenum;
	}

	return ret;
}

int On2_AvcEncoder::AllocRes( )
{
    i32 ret;
    u32 pictureSize;
    u32 outbufSize;
    commandLine_s * cml = &cmdl;

    if(cml->inputFormat <= 1)
    {
        /* Input picture in planar YUV 4:2:0 format */
        pictureSize =
            ((cml->lumWidthSrc + 15) & (~15)) * cml->lumHeightSrc * 3 / 2;
    }
    else if(cml->inputFormat <= 9)
    {
        /* Input picture in YUYV 4:2:2 or 16-bit RGB format */
        pictureSize =
            ((cml->lumWidthSrc + 15) & (~15)) * cml->lumHeightSrc * 2;
    }
    else
    {
        /* Input picture in 32-bit RGB format */
        pictureSize =
            ((cml->lumWidthSrc + 15) & (~15)) * cml->lumHeightSrc * 4;
    }

    ALOGV("Input %dx%d encoding at %dx%d\n", cml->lumWidthSrc,
           cml->lumHeightSrc, cml->width, cml->height);

    pictureMem.vir_addr = NULL;
    outbufMem.vir_addr = NULL;
    pictureStabMem.vir_addr = NULL;

    /* Here we use the EWL instance directly from the encoder
     * because it is the easiest way to allocate the linear memories */
    ret = VPUMallocLinear(&pictureMem, pictureSize);
    if (ret != EWL_OK)
    {
        ALOGV( "Failed to allocate input picture!\n");
        pictureMem.vir_addr = NULL;
        return 1;
    }

    if(cml->videoStab > 0)
    {
        ret = VPUMallocLinear(&pictureStabMem, pictureSize);
        if (ret != EWL_OK)
        {
            ALOGV( "Failed to allocate stab input picture!\n");
            pictureStabMem.vir_addr = NULL;
            return 1;
        }
    }

    outbufSize =  (1024 * 1024 * 2);

    ret = VPUMallocLinear(&outbufMem, outbufSize);
    if (ret != EWL_OK)
    {
        ALOGV( "Failed to allocate output buffer!\n");
        outbufMem.vir_addr = NULL;
        return 1;
    }

    ALOGV("Input buffer size:          %d bytes\n", pictureMem.size);
    ALOGV("Input buffer bus address:   0x%08x\n", pictureMem.phy_addr);
    ALOGV("Input buffer user address:  0x%08x\n", (u32) pictureMem.vir_addr);
    ALOGV("Output buffer size:         %d bytes\n", outbufMem.size);
    ALOGV("Output buffer bus address:  0x%08x\n", outbufMem.phy_addr);
    ALOGV("Output buffer user address: 0x%08x\n", (u32) outbufMem.vir_addr);

    return 0;
}

int On2_AvcEncoder::pv_on2avcencoder_deinit()
{
	H264EncRet ret;
    commandLine_s * cml = &cmdl;
    H264EncInst encoder = (H264EncInst)&h264encInst;
    /* End stream */
#ifdef ENC_DEBUG
    if(fp)
    {
        fclose(fp);
        fp = NULL;
    }
    if(fp1)
    {
        fclose(fp1);
        fp1 = NULL;
    }
#endif

    ret = H264EncStrmEnd(encoder, &encIn, &encOut);
    CloseEncoder(encoder);
    if(ret != H264ENC_OK)
    {
        PrintErrorValue("H264EncStrmEnd() failed.", ret);
    }
    else
    {
        streamSize += encOut.streamSize;
        ALOGV("      |     |    | END  |          | %7i %6i | ",
                streamSize, encOut.streamSize);
    }
    FreeRes();
    return 0;

}
extern "C"
void *  get_class_On2AvcEncoder(void)
{
    return (void*)new On2_AvcEncoder();
}

extern "C"
void  destroy_class_On2AvcEncoder(void * AvcEncoder)
{
    delete (On2_AvcEncoder *)AvcEncoder;
    AvcEncoder = NULL;
}

typedef struct
{
   int width;
   int height;
   int rc_mode;
   int bitRate;
   int framerate;
	int	qp;
   int	enableCabac;
   int	cabacInitIdc;
   int format;
   int	intraPicRate;
   int  framerateout;
   int	reserved[5];
}EncParams1;

extern "C"
int init_class_On2AvcEncoder(void * AvcEncoder, EncParams1 *aEncOption, uint8_t* aOutBuffer,uint32_t* aOutputLength)
{
	On2_AvcEncoder * Avcenc = (On2_AvcEncoder *)AvcEncoder;
	return Avcenc->pv_on2avcencoder_init((AVCEncParams1 *)aEncOption, aOutBuffer, aOutputLength);
}

extern "C"
int deinit_class_On2AvcEncoder(void * AvcEncoder)
{
	On2_AvcEncoder * Avcenc = (On2_AvcEncoder *)AvcEncoder;
	return Avcenc->pv_on2avcencoder_deinit();
}

extern "C"
int enc_oneframe_class_On2AvcEncoder(void * AvcEncoder, uint8_t* aOutBuffer, uint32_t* aOutputLength,
                                     uint8_t *aInBuffer,uint32_t aInBuffPhy,uint32_t* aInBufSize,uint32_t* aOutTimeStamp,bool*  aSyncFlag)
{
	int ret;

	On2_AvcEncoder * Avcenc = (On2_AvcEncoder *)AvcEncoder;

	ret = Avcenc->pv_on2avcencoder_oneframe(aOutBuffer, aOutputLength,aInBuffer,
                                            aInBuffPhy, aInBufSize, aOutTimeStamp, aSyncFlag);
	return ret;
}

extern "C"

int set_idrframe_class_On2AvcEncoder(void * AvcEncoder)
{

	On2_AvcEncoder * Avcenc = (On2_AvcEncoder *)AvcEncoder;

	Avcenc->H264encSetintraPeriodCnt();
	return 0;
}
extern "C"

int set_config_class_On2AvcEncoder(void * AvcEncoder,AVCEncParams1 *vpug)
{

	On2_AvcEncoder * Avcenc = (On2_AvcEncoder *)AvcEncoder;
	Avcenc->pv_on2avcencoder_setconfig(vpug);
	return 0;
}

extern "C"

int get_config_class_On2AvcEncoder(void * AvcEncoder,AVCEncParams1 *vpug)
{

	On2_AvcEncoder * Avcenc = (On2_AvcEncoder *)AvcEncoder;

	Avcenc->pv_on2avcencoder_getconfig(vpug);
	return 0;
}

extern "C"

int set_inputformat_class_On2AvcEncoder(void * AvcEncoder,H264EncPictureType inputFormat)
{

	On2_AvcEncoder * Avcenc = (On2_AvcEncoder *)AvcEncoder;
	Avcenc->H264encSetInputFormat(inputFormat);
	return 0;
}

extern "C"

int setmbperslice_config_enc_oneframe(void * AvcEncoder, int line_per_slice)
{
	On2_AvcEncoder * Avcenc = (On2_AvcEncoder *)AvcEncoder;

	Avcenc->h264encSetmbperslice(line_per_slice);

	return 0;
}


