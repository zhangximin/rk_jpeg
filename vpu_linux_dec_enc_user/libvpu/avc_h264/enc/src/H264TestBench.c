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
--  Abstract : H264 Encoder testbench for linux
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: H264TestBench.c,v $
--  $Revision: 1.5 $
--  $Date: 2008/04/02 12:47:27 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

/* For SW/HW shared memory allocation */
#include "ewl.h"

/* For command line structure */
#include "H264TestBench.h"

/* For parameter parsing */
#include "EncGetOption.h"

/* For accessing the EWL instance inside the encoder */
#include "H264Instance.h"

/* For compiler flags, test data, debug and tracing */
#include "enccommon.h"

/* For Hantro H.264 encoder */
#include "h264encapi.h"

#ifdef INTERNAL_TEST
#include "h264encapi_ext.h"
#endif

/* For printing and file IO */
#include <stdio.h>

/* For dynamic memory allocation */
#include <stdlib.h>

/* For memset, strcpy and strlen */
#include <string.h>

#ifdef USE_EFENCE
#include "efence.h"
#endif

#include "vpu_mem.h"
#include <pthread.h>

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

NO_OUTPUT_WRITE: Output stream is not written to file. This should be used
                 when running performance simulations.
NO_INPUT_READ:   Input frames are not read from file. This should be used
                 when running performance simulations.
PSNR:            Enable PSNR calculation with --psnr option, only works with
                 system model

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

#define H264ERR_OUTPUT  //stdout
#define MAX_GOP_LEN 150
/* The amount of entries in the NAL unit size table, max one NALU / MB row */
#define NALU_TABLE_SIZE     ((1920/16) + 3)

#ifdef PSNR
	float log10f(float x);
	float roundf(float x);
#endif

/* Global variables */

i32 testParam;

static char input[] = "test.yuv";
static char output[] = "stream.h264";
static char nal_sizes_file[] = "nal_sizes.txt";

static char *streamType[2] = { "BYTE_STREAM", "NAL_UNITS" };

static option_s option[] = {
    {"help", 'H'},
    {"firstPic", 'a', 1},
    {"lastPic", 'b', 1},
    {"width", 'x', 1},
    {"height", 'y', 1},
    {"lumWidthSrc", 'w', 1},
    {"lumHeightSrc", 'h', 1},
    {"horOffsetSrc", 'X', 1},
    {"verOffsetSrc", 'Y', 1},
    {"outputRateNumer", 'f', 1},
    {"outputRateDenom", 'F', 1},
    {"inputRateNumer", 'j', 1},
    {"inputRateDenom", 'J', 1},
    {"inputFormat", 'l', 1},    /* Input image format */
    {"colorConversion", 'O', 1},    /* RGB to YCbCr conversion type */
    {"input", 'i', 1},          /* "input" must be after "inputFormat" */
    {"output", 'o', 1},
    {"videoRange", 'k', 1},
    {"rotation", 'r', 1},   /* Input image rotation */
    {"intraPicRate", 'I', 1},
    {"constIntraPred", 'T', 1},
    {"disableDeblocking", 'D', 1},
    {"filterOffsetA", 'W', 1},
    {"filterOffsetB", 'E', 1},

    {"trans8x8", '8', 1}, /* adaptive 4x4, 8x8 transform */
    {"enableCabac", 'K', 1},
    {"cabacInitIdc", 'p', 1},
    {"mbPerSlice", 'V', 1},

    {"bitPerSecond", 'B', 1},
    {"picRc", 'U', 1},
    {"mbRc", 'u', 1},
    {"picSkip", 's', 1},    /* Frame skiping */
    {"gopLength", 'g', 1}, /* group of pictures length */
    {"qpMin", 'n', 1},  /* Minimum frame header qp */
    {"qpMax", 'm', 1},  /* Maximum frame header qp */
    {"qpHdr", 'q', 1},  /* Defaul qp */
    {"chromaQpOffset", 'Q', 1}, /* Chroma qp index offset */
    {"hrdConformance", 'C', 1}, /* HDR Conformance (ANNEX C) */
    {"cpbSize", 'c', 1},    /* Coded Picture Buffer Size */
    {"intraQpDelta", 'A', 1},    /* QP adjustment for intra frames */
    {"fixedIntraQp", 'G', 1},    /* Fixed QP for all intra frames */

    {"userData", 'z', 1},  /* SEI User data file */
    {"level", 'L', 1},  /* Level * 10  (ANNEX A) */
    {"byteStream", 'R', 1},     /* Byte stream format (ANNEX B) */
    {"sei", 'S', 1},    /* SEI messages */
    {"videoStab", 'Z', 1},  /* video stabilization */
    {"bpsAdjust", '1', 1},  /* Setting bitrate on the fly */
    {"mbQpAdjustment", '2', 1},  /* MAD based MB QP adjustment */

    {"testId", 'e', 1},
    {"burstSize", 'N', 1},
    {"burstType", 't', 1},
    {"quarterPixelMv", 'M', 1},
    {"trigger", 'P', 1},
    {"psnr", 'd', 0},
    {"testParam", 'v', 1},
    {0, 0, 0}
};

/* SW/HW shared memories for input/output buffers */
static VPUMemLinear_t pictureMem;
static VPUMemLinear_t pictureStabMem;
static VPUMemLinear_t outbufMem;

static FILE *yuvFile = NULL;
static u32 file_size;

i32 trigger_point = -1;      /* Logic Analyzer trigger point */

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
static int AllocRes(commandLine_s * cmdl, H264EncInst enc);
static void FreeRes(H264EncInst enc);
static int OpenEncoder(commandLine_s * cml, h264Instance_s * pEnc);
static i32 Encode(H264EncInst inst, commandLine_s * cml);
static void CloseEncoder(H264EncInst encoder);
static i32 NextPic(i32 inputRateNumer, i32 inputRateDenom, i32 outputRateNumer,
                   i32 outputRateDenom, i32 frameCnt, i32 firstPic);
static int ReadPic(u8 * image, i32 size, i32 nro, char *name, i32 width,
                   i32 height, i32 format);
static u8* ReadUserData(H264EncInst encoder, char *name);
static int Parameter(commandLine_s * ep);
static void Help(void);
static void WriteStrm(FILE * fout, u32 * outbuf, u32 size, u32 endian);
static int ChangeInput(i32 argc, char **argv, char **name, option_s * option);
static void PrintNalSizes(const u32 *pNaluSizeBuf, const u8 *pOutBuf,
        u32 strmSize, i32 byteStream);

static void WriteNalSizesToFile(const char *file, const u32 * pNaluSizeBuf,
                                u32 buffSize);
static void PrintErrorValue(const char *errorDesc, u32 retVal);
static u32 PrintPSNR(u8 *a, u8 *b, i32 scanline, i32 wdh, i32 hgt);

H264EncInst encoder;
commandLine_s cmdl;
H264EncIn encIn;
H264EncOut encOut;
H264EncRateCtrl rc;
int intraPeriodCnt = 0, codedFrameCnt = 0, next = 0, src_img_size;
u32 frameCnt = 0;
u32 streamSize = 0;
u32 bitrate = 0;
u32 psnrSum = 0;
u32 psnrCnt = 0;
u32 psnr = 0;
i32 i;
u8 *pUserData;
FILE *fout = NULL;

/*------------------------------------------------------------------------------

    main

------------------------------------------------------------------------------*/
int h264encode(void)
{
    i32 ret;

    //enc initial
    if(H264encodeInit() != 0)
        ALOGV("H264encodeInit fail!!");

    //enc main loop
    do{
        ret = H264encodeOneFrame(encoder, &cmdl);
//        getchar();
    }while(ret == H264ENC_FRAME_READY);

    if(ret == -10)
        ALOGV("file has read end!");
    if(ret == -11)
        ALOGV("file read error!");

    //enc end and exit, release some resource!
    H264encodeExit(encoder, &cmdl);

    FreeRes(encoder);

    CloseEncoder(encoder);

    return ret;
}

int H264encodeInit(void)
{
    commandLine_s *cml = &cmdl;
    H264EncRet ret;

    {
        pthread_t tid;
        ALOGV("vpu_service starting\n");
        pthread_create(&tid, NULL, (void *)vpu_service, NULL);
    }

    /* Parse command line parameters */
    if(Parameter(&cmdl) != 0)
    {
        ALOGV( "Input parameter error\n");
        return -1;
    }

    /* Encoder initialization */
    if(OpenEncoder(&cmdl, &encoder) != 0)
    {
        ALOGV( "Open Encoder failure\n");
        return -1;
    }

    /* Set the test ID for internal testing,
     * the SW must be compiled with testing flags */
    H264EncSetTestId(encoder, cmdl.testId);

    /* Allocate input and output buffers */
    if(AllocRes(&cmdl, encoder) != 0)
    {
        ALOGV( "Failed to allocate the external resources!\n");
        FreeRes(encoder);
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

    if(!encIn.pNaluSizeBuf)
    {
        ALOGV("WARNING! Failed to allocate NAL unit size buffer.\n");
    }

    /* Source Image Size */
    if(cml->inputFormat <= 1)
    {
        src_img_size = cml->lumWidthSrc * cml->lumHeightSrc +
            2 * (((cml->lumWidthSrc + 1) >> 1) *
                 ((cml->lumHeightSrc + 1) >> 1));
    }
    else if(cml->inputFormat <= 9)
    {
        /* 422 YUV or 16-bit RGB */
        src_img_size = cml->lumWidthSrc * cml->lumHeightSrc * 2;
    }
    else
    {
        /* 32-bit RGB */
        src_img_size = cml->lumWidthSrc * cml->lumHeightSrc * 4;
    }

    ALOGV("Reading input from file <%s>, frame size %d bytes.\n",
            cml->input, src_img_size);


    /* Start stream */
    ret = H264EncStrmStart(encoder, &encIn, &encOut);
    if(ret != H264ENC_OK)
    {
        PrintErrorValue("H264EncStrmStart() failed.", ret);
        return -1;
    }

    fout = fopen(cml->output, "wb");
    if(fout == NULL)
    {
        ALOGV( "Failed to create the output file.\n");
        return -1;
    }

    WriteStrm(fout, outbufMem.vir_addr, encOut.streamSize, 0);
    if(cml->byteStream == 0)
    {
        WriteNalSizesToFile(nal_sizes_file, encIn.pNaluSizeBuf,
                            encIn.naluSizeBufSize);
    }

    streamSize += encOut.streamSize;

    H264EncGetRateCtrl(encoder, &rc);

    /* Allocate a buffer for user data and read data from file */
    pUserData = ReadUserData(encoder, cml->userData);

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
    PrintNalSizes(encIn.pNaluSizeBuf, (u8 *) outbufMem.vir_addr,
            encOut.streamSize, cml->byteStream);
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

    return 0;
}

int H264encodeExit(H264EncInst encoder, commandLine_s * cml)
{
    H264EncRet ret;

    /* End stream */
    ret = H264EncStrmEnd(encoder, &encIn, &encOut);
    if(ret != H264ENC_OK)
    {
        PrintErrorValue("H264EncStrmEnd() failed.", ret);
    }
    else
    {
        streamSize += encOut.streamSize;
        ALOGV("      |     |    | END  |          | %7i %6i | ",
                streamSize, encOut.streamSize);
        if (cml->psnr)
            ALOGV("      | ");
        PrintNalSizes(encIn.pNaluSizeBuf, (u8 *) outbufMem.vir_addr,
                encOut.streamSize, cml->byteStream);
        ALOGV("\n");

//        WriteStrm(fout, outbufMem.vir_addr, encOut.streamSize, 0);

        if(cml->byteStream == 0)
        {
            WriteNalSizesToFile(nal_sizes_file, encIn.pNaluSizeBuf,
                                encIn.naluSizeBufSize);
        }
    }

    ALOGV("\nBitrate target %d bps, actual %d bps (%d%%).\n",
            rc.bitPerSecond, bitrate,
            (rc.bitPerSecond) ? bitrate*100/rc.bitPerSecond : 0);
    ALOGV("Total of %d frames processed, %d frames encoded, %d bytes.\n",
            frameCnt, codedFrameCnt, streamSize);

    if (psnrCnt)
        ALOGV("Average PSNR %d.%02d\n",
            (psnrSum/psnrCnt)/100, (psnrSum/psnrCnt)%100);

    return 0;
}

/*------------------------------------------------------------------------------

    H264encodeOneFrame

    Do the encoding.

    Params:
        argc    - number of arguments to the application
        argv    - argument list as provided to the application
        encoder - encoder instance
        cml     - processed comand line options

    Return:
        0   - for success
        -1  - error

------------------------------------------------------------------------------*/
int H264encodeOneFrame(H264EncInst encoder, commandLine_s * cml)
{
    H264EncRet ret;

    /* Main encoding loop */
    if((next = NextPic(cml->inputRateNumer, cml->inputRateDenom,
                          cml->outputRateNumer, cml->outputRateDenom, frameCnt,
                          cml->firstPic)) <= cml->lastPic)
    {
#ifdef EVALUATION_LIMIT
    if(frameCnt >= EVALUATION_LIMIT)
        break;
#endif

#ifndef NO_INPUT_READ
        /* Read next frame */
        if(ReadPic((u8 *) pictureMem.vir_addr,
                    src_img_size, next, cml->input,
                   cml->lumWidthSrc, cml->lumHeightSrc, cml->inputFormat) != 0)
            return -11;

        if(cml->videoStab > 0)
        {
            /* Stabilize the frame after current frame */
            i32 nextStab = NextPic(cml->inputRateNumer, cml->inputRateDenom,
                          cml->outputRateNumer, cml->outputRateDenom, frameCnt+1,
                          cml->firstPic);

            if(ReadPic((u8 *) pictureStabMem.vir_addr,
                        src_img_size, nextStab, cml->input,
                       cml->lumWidthSrc, cml->lumHeightSrc,
                       cml->inputFormat) != 0)
                return -11;
        }
#endif

        for (i = 0; i < MAX_BPS_ADJUST; i++)
            if (cml->bpsAdjustFrame[i] &&
                (codedFrameCnt == cml->bpsAdjustFrame[i]))
            {
                rc.bitPerSecond = cml->bpsAdjustBitrate[i];
                ALOGV("Adjusting bitrate target: %d\n", rc.bitPerSecond);
                if((ret = H264EncSetRateCtrl(encoder, &rc)) != H264ENC_OK)
                {
                    PrintErrorValue("H264EncSetRateCtrl() failed.", ret);
                }
            }


        /* Select frame type */
        if((cml->intraPicRate != 0) && (intraPeriodCnt >= cml->intraPicRate))
            encIn.codingType = H264ENC_INTRA_FRAME;
        else
            encIn.codingType = H264ENC_PREDICTED_FRAME;

        if(encIn.codingType == H264ENC_INTRA_FRAME)
            intraPeriodCnt = 0;

        ret = H264EncStrmEncode(encoder, &encIn, &encOut);

        H264EncGetRateCtrl(encoder, &rc);

        streamSize += encOut.streamSize;

        /* Note: This will overflow if large output rate numerator is used */
        if((frameCnt+1) && cml->outputRateDenom)
            bitrate = 8 * ((streamSize/(frameCnt+1)) *
                    (u32) cml->outputRateNumer / (u32) cml->outputRateDenom);

        switch (ret)
        {
        case H264ENC_FRAME_READY:

            ALOGV("%5i | %3i | %2i | %s | %8u | %7i %6i | ",
                next, frameCnt, rc.qpHdr,
                encOut.codingType == H264ENC_INTRA_FRAME ? " I  " :
                encOut.codingType == H264ENC_PREDICTED_FRAME ? " P  " : "skip",
                bitrate, streamSize, encOut.streamSize);
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
            PrintNalSizes(encIn.pNaluSizeBuf, (u8 *) outbufMem.vir_addr,
                encOut.streamSize, cml->byteStream);
            ALOGV("\n");

            WriteStrm(fout, outbufMem.vir_addr, encOut.streamSize, 0);

            if(cml->byteStream == 0)
            {
                WriteNalSizesToFile(nal_sizes_file, encIn.pNaluSizeBuf,
                                    encIn.naluSizeBufSize);
            }

            if (pUserData)
            {
                /* We want the user data to be written only once so
                 * we disable the user data and free the memory after
                 * first frame has been encoded. */
                H264EncSetSeiUserData(encoder, NULL, 0);
                free(pUserData);
                pUserData = NULL;
            }

            break;

        case H264ENC_OUTPUT_BUFFER_OVERFLOW:
            ALOGV("%5i | %3i | %2i | %s | %8u | %7i %6i | \n",
                next, frameCnt, rc.qpHdr, "lost",
                bitrate, streamSize, encOut.streamSize);
            break;

        default:
            PrintErrorValue("H264EncStrmEncode() failed.", ret);
            /* For debugging, can be removed */
            WriteStrm(fout, outbufMem.vir_addr, encOut.streamSize, 0);
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

/*------------------------------------------------------------------------------

    AllocRes

    Allocation of the physical memories used by both SW and HW:
    the input pictures and the output stream buffer.

    NOTE! The implementation uses the EWL instance from the encoder
          for OS independence. This is not recommended in final environment
          because the encoder will release the EWL instance in case of error.
          Instead, the memories should be allocated from the OS the same way
          as inside EWLMallocLinear().

------------------------------------------------------------------------------*/
int AllocRes(commandLine_s * cmdl, H264EncInst enc)
{
    i32 ret;
    u32 pictureSize;
    u32 outbufSize;


    if(cmdl->inputFormat <= 1)
    {
        /* Input picture in planar YUV 4:2:0 format */
        pictureSize =
            ((cmdl->lumWidthSrc + 15) & (~15)) * cmdl->lumHeightSrc * 3 / 2;
    }
    else if(cmdl->inputFormat <= 9)
    {
        /* Input picture in YUYV 4:2:2 or 16-bit RGB format */
        pictureSize =
            ((cmdl->lumWidthSrc + 15) & (~15)) * cmdl->lumHeightSrc * 2;
    }
    else
    {
        /* Input picture in 32-bit RGB format */
        pictureSize =
            ((cmdl->lumWidthSrc + 15) & (~15)) * cmdl->lumHeightSrc * 4;
    }

    ALOGV("Input %dx%d encoding at %dx%d\n", cmdl->lumWidthSrc,
           cmdl->lumHeightSrc, cmdl->width, cmdl->height);

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

    if(cmdl->videoStab > 0)
    {
        ret = VPUMallocLinear(&pictureStabMem, pictureSize);
        if (ret != EWL_OK)
        {
            ALOGV( "Failed to allocate stab input picture!\n");
            pictureStabMem.vir_addr = NULL;
            return 1;
        }
    }

    outbufSize = 4 * pictureMem.size < (1024 * 1024 * 3 / 2) ?
        4 * pictureMem.size : (1024 * 1024 * 3 / 2);

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

/*------------------------------------------------------------------------------

    FreeRes

    Release all resources allcoated byt AllocRes()

------------------------------------------------------------------------------*/
void FreeRes(H264EncInst enc)
{
    if(pictureMem.vir_addr != NULL)
        VPUFreeLinear(&pictureMem);
    if(pictureStabMem.vir_addr != NULL)
        VPUFreeLinear(&pictureStabMem);
    if(outbufMem.vir_addr != NULL)
        VPUFreeLinear(&outbufMem);
}

/*------------------------------------------------------------------------------

    OpenEncoder
        Create and configure an encoder instance.

    Params:
        cml     - processed comand line options
        pEnc    - place where to save the new encoder instance
    Return:
        0   - for success
        -1  - error

------------------------------------------------------------------------------*/
int OpenEncoder(commandLine_s * cml, H264EncInst * pEnc)
{
    H264EncRet ret;
    H264EncConfig cfg;
    H264EncCodingCtrl codingCfg;
    H264EncRateCtrl rcCfg;
    H264EncPreProcessingCfg preProcCfg;

    H264EncInst encoder;
    h264Instance_s *pEnc264;

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

    encoder = *pEnc;

    //get socket
    pEnc264 = (h264Instance_s *)encoder;
    pEnc264->asic.regs.socket = VPUClientInit(VPU_ENC);

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

/*------------------------------------------------------------------------------

    CloseEncoder
       Release an encoder insatnce.

   Params:
        encoder - the instance to be released
------------------------------------------------------------------------------*/
void CloseEncoder(H264EncInst encoder)
{
    H264EncRet ret;

    if((ret = H264EncRelease(encoder)) != H264ENC_OK)
    {
        PrintErrorValue("H264EncRelease() failed.", ret);
    }
}

/*------------------------------------------------------------------------------

    Parameter
        Process the testbench calling arguments.

    Params:
        argc    - number of arguments to the application
        argv    - argument list as provided to the application
        cml     - processed comand line options
    Return:
        0   - for success
        -1  - error

------------------------------------------------------------------------------*/
int Parameter(commandLine_s * cml)
{
    i32 ret, i;
    char *optArg;
    argument_s argument;
    i32 status = 0;
    i32 bpsAdjustCount = 0;

    memset(cml, 0, sizeof(commandLine_s));

    cml->input = input;
    cml->output = output;
    cml->firstPic = 0;
    cml->lastPic = 100;
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

    cml->level = 40;
    cml->byteStream = 1;    //nal stream:1; byte stream:0;

    cml->sei = 0;       //Enable/Disable SEI messages.[0]

    cml->rotation = 0;
    cml->inputFormat = 0;   //YUV420
    cml->colorConversion = 0;   //RGB to YCbCr color conversion type---BT.601
    cml->videoRange = 0;    //0..1 Video range.

    cml->videoStab = 0;    //Video stabilization. n > 0 enabled [0]

    cml->constIntraPred = 0;   //0=OFF, 1=ON Constrained intra pred flag [0]
    cml->disableDeblocking = 0;   //0..2 Value of disable_deblocking_filter_idc [0]
    cml->intraPicRate = 0;   //Intra picture rate. [0]
    cml->mbPerSlice = 0;   //Slice size in macroblocks. Should be a multiple of MBs per row. [0]


    cml->trans8x8 = 0;          //0=OFF, 1=Adaptive, 2=ON [0]\n"
    cml->enableCabac = 0;       //0=OFF, 1=ON [0]\n"
    cml->cabacInitIdc = 0;      //0..2\n");


    cml->bitPerSecond  = 64000;         //Bitrate, 10000..levelMax [64000]\n"
    cml->picRc = 1;                      //0=OFF, 1=ON Picture rate control. [1]\n"
    cml->mbRc = 1;                       //0=OFF, 1=ON Mb rc (Check point rc). [1]\n"
    cml->hrdConformance =  0 ;        //0=OFF, 1=ON HRD conformance. [0]\n"
    cml->cpbSize       =   0 ;       //HRD Coded Picture Buffer size in bits. [0]\n"
    cml->gopLength     =   1 ;       //GOP length, 1..150 [intraPicRate]\n");


    cml->qpHdr = 26;
    cml->qpMin = 10;
    cml->qpMax = 51;
    cml->intraQpDelta = -3; //Intra QP delta
    cml->fixedIntraQp = 0;    //0..51, Fixed Intra QP, 0 = disabled. [0]
    cml->mbQpAdjustment = 0;    //-8..7, MAD based MB QP adjustment, 0 = disabled

    cml->userData  = NULL;        //SEI User data file name.\n"
//    cml->bpsAdjust = 0;        // Frame:bitrate for adjusting bitrate on the fly.\n"
    cml->psnr      = 0;        //Enables PSNR calculation for each frame.\n");


    cml->chromaQpOffset = 2;
    cml->filterOffsetA = 0;
    cml->filterOffsetB = 0;
    cml->burst = 16;        //burstSize
    cml->bursttype = 0;     //0=SIGLE, 1=INCR HW bus burst type. [0]
    cml->quarterPixelMv = 0;
    cml->testId = 0;
    trigger_point = -1;      //Logic Analyzer trigger at picture <n>. [-1]

    cml->picSkip = 0;
    testParam = cml->testParam = 0;

    return 0;
}

/*------------------------------------------------------------------------------

    ReadPic

    Read raw YUV 4:2:0 or 4:2:2 image data from file

    Params:
        image   - buffer where the image will be saved
        size    - amount of image data to be read
        nro     - picture number to be read
        name    - name of the file to read
        width   - image width in pixels
        height  - image height in pixels
        format  - image format (YUV 420/422/RGB16/RGB32)

    Returns:
        0 - for success
        non-zero error code
------------------------------------------------------------------------------*/
int ReadPic(u8 * image, i32 size, i32 nro, char *name, i32 width, i32 height,
            i32 format)
{
    int ret = 0;

    if(yuvFile == NULL)
    {
        yuvFile = fopen(name, "rb");

        if(yuvFile == NULL)
        {
            ALOGV( "\nUnable to open YUV file: %s\n", name);
            ret = -1;
            goto end;
        }

        fseek(yuvFile, 0, SEEK_END);
        file_size = ftell(yuvFile);
    }

    /* Stop if over last frame of the file */
    if((u32)size * (nro + 1) > file_size)
    {
        ALOGV("\nCan't read frame, EOF\n");
        ret = -1;
        goto end;
    }

    if(fseek(yuvFile, (u32)size * nro, SEEK_SET) != 0)
    {
        ALOGV( "\nI can't seek frame no: %i from file: %s\n",
                nro, name);
        ret = -1;
        goto end;
    }

    if((width & 0x0f) == 0)
        fread(image, 1, size, yuvFile);
    else
    {
        i32 i;
        u8 *buf = image;
        i32 scan = (width + 15) & (~0x0f);

        /* Read the frame so that scan (=stride) is multiple of 16 pixels */

        if(format == 0) /* YUV 4:2:0 planar */
        {
            /* Y */
            for(i = 0; i < height; i++)
            {
                fread(buf, 1, width, yuvFile);
                buf += scan;
            }
            /* Cb */
            for(i = 0; i < (height / 2); i++)
            {
                fread(buf, 1, width / 2, yuvFile);
                buf += scan / 2;
            }
            /* Cr */
            for(i = 0; i < (height / 2); i++)
            {
                fread(buf, 1, width / 2, yuvFile);
                buf += scan / 2;
            }
        }
        else if(format == 1)    /* YUV 4:2:0 semiplanar */
        {
            /* Y */
            for(i = 0; i < height; i++)
            {
                fread(buf, 1, width, yuvFile);
                buf += scan;
            }
            /* CbCr */
            for(i = 0; i < (height / 2); i++)
            {
                fread(buf, 1, width, yuvFile);
                buf += scan;
            }
        }
        else if(format <= 9)   /* YUV 4:2:2 interleaved or 16-bit RGB */
        {
            for(i = 0; i < height; i++)
            {
                fread(buf, 1, width * 2, yuvFile);
                buf += scan * 2;
            }
        }
        else    /* 32-bit RGB */
        {
            for(i = 0; i < height; i++)
            {
                fread(buf, 1, width * 4, yuvFile);
                buf += scan * 4;
            }
        }

    }

  end:

    return ret;
}

/*------------------------------------------------------------------------------

    ReadUserData
        Read user data from file and pass to encoder

    Params:
        name - name of file in which user data is located

    Returns:
        NULL - when user data reading failed
        pointer - allocated buffer containing user data

------------------------------------------------------------------------------*/
u8* ReadUserData(H264EncInst encoder, char *name)
{
    FILE *file = NULL;
    i32 byteCnt;
    u8 *data;

    if(name == NULL)
    {
        return NULL;
    }

    /* Get user data length from file */
    file = fopen(name, "rb");
    if(file == NULL)
    {
        ALOGV( "Unable to open User Data file: %s\n", name);
        return NULL;
    }
    fseek(file, 0L, SEEK_END);
    byteCnt = ftell(file);
    rewind(file);

    /* Minimum size of user data */
    if (byteCnt < 16)
        byteCnt = 16;

    /* Maximum size of user data */
    if (byteCnt > 2048)
        byteCnt = 2048;

    /* Allocate memory for user data */
    if((data = (u8 *) malloc(sizeof(u8) * byteCnt)) == NULL)
    {
        fclose(file);
        ALOGV( "Unable to alloc User Data memory\n");
        return NULL;
    }

    /* Read user data from file */
    fread(data, sizeof(u8), byteCnt, file);
    fclose(file);

    ALOGV("User data: %d bytes [%d %d %d %d ...]\n",
        byteCnt, data[0], data[1], data[2], data[3]);

    /* Pass the data buffer to encoder
     * The encoder reads the buffer during following H264EncStrmEncode() calls.
     * User data writing must be disabled (with SetSeiUserData(enc, 0, 0)) */
    H264EncSetSeiUserData(encoder, data, byteCnt);

    return data;
}

/*------------------------------------------------------------------------------

    Help
        Print out some instructions about usage.
------------------------------------------------------------------------------*/
void Help(void)
{
    ALOGV( "Usage:  %s [options] -i inputfile\n\n", "h264_testenc");

    ALOGV(
            "  -i[s] --input             Read input from file. [input.yuv]\n"
            "  -o[s] --output            Write output to file. [stream.h264]\n"
            "  -a[n] --firstPic          First picture of input file. [0]\n"
            "  -b[n] --lastPic           Last picture of input file. [100]\n"
            "  -w[n] --lumWidthSrc       Width of source image. [176]\n"
            "  -h[n] --lumHeightSrc      Height of source image. [144]\n");
    ALOGV(
            "  -x[n] --width             Width of output image. [--lumWidthSrc]\n"
            "  -y[n] --height            Height of output image. [--lumHeightSrc]\n"
            "  -X[n] --horOffsetSrc      Output image horizontal offset. [0]\n"
            "  -Y[n] --verOffsetSrc      Output image vertical offset. [0]\n");
    ALOGV(
            "  -f[n] --outputRateNumer   1..65535 Output picture rate numerator. [30]\n"
            "  -F[n] --outputRateDenom   1..65535 Output picture rate denominator. [1]\n"
            "  -j[n] --inputRateNumer    1..65535 Input picture rate numerator. [30]\n"
            "  -J[n] --inputRateDenom    1..65535 Input picture rate denominator. [1]\n");

    ALOGV( "\n  -L[n] --level             10..40, H264 Level. [30]\n");
    ALOGV( "\n  -R[n] --byteStream        Stream type. [1]\n"
            "                            1 - byte stream according to Annex B.\n"
            "                            0 - NAL units. Nal sizes returned in <nal_sizes.txt>\n");
    ALOGV(
            "\n  -S[n] --sei               Enable/Disable SEI messages.[0]\n");

    ALOGV(
            "\n  -r[n] --rotation          Rotate input image. [0]\n"
            "                                0 - disabled\n"
            "                                1 - 90 degrees right\n"
            "                                2 - 90 degrees right\n"
            "  -l[n] --inputFormat       Input YUV format. [0]\n"
            "                                0 - YUV420\n"
            "                                1 - YUV420 semiplanar\n"
            "                                2 - YUYV422\n"
            "                                3 - UYVY422\n"
            "                                4 - RGB565\n"
            "                                5 - BGR565\n"
            "                                6 - RGB555\n"
            "                                7 - BGR555\n"
            "                                8 - RGB444\n"
            "                                9 - BGR444\n"
            "                                10 - RGB888\n"
            "                                11 - BGR888\n"
            "                                12 - RGB101010\n"
            "                                13 - BGR101010\n"
            "  -O[n] --colorConversion   RGB to YCbCr color conversion type. [0]\n"
            "                                0 - BT.601\n"
            "                                1 - BT.709\n"
            "                                2 - User defined\n"
            "  -k[n] --videoRange        0..1 Video range. [0]\n\n");

    ALOGV(
            "  -Z[n] --videoStab         Video stabilization. n > 0 enabled [0]\n\n");

    ALOGV(
            "  -T[n] --constIntraPred    0=OFF, 1=ON Constrained intra pred flag [0]\n"
            "  -D[n] --disableDeblocking 0..2 Value of disable_deblocking_filter_idc [0]\n"
            "  -I[n] --intraPicRate      Intra picture rate. [0]\n"
            "  -V[n] --mbPerSlice        Slice size in macroblocks. Should be a\n"
            "                            multiple of MBs per row. [0]\n\n");
    ALOGV(
            "  -8[n] --trans8x8          0=OFF, 1=Adaptive, 2=ON [0]\n"
            "  -K[n] --enableCabac       0=OFF, 1=ON [0]\n"
            "  -p[n] --cabacInitIdc      0..2\n");
    ALOGV(
            "\n  -B[n] --bitPerSecond      Bitrate, 10000..levelMax [64000]\n"
            "  -U[n] --picRc             0=OFF, 1=ON Picture rate control. [1]\n"
            "  -u[n] --mbRc              0=OFF, 1=ON Mb rc (Check point rc). [1]\n"
            "  -C[n] --hrdConformance    0=OFF, 1=ON HRD conformance. [0]\n"
            "  -c[n] --cpbSize           HRD Coded Picture Buffer size in bits. [0]\n"
            "  -g[n] --gopLength         GOP length, 1..150 [intraPicRate]\n");

    ALOGV(
            "\n  -s[n] --picSkip           0=OFF, 1=ON Picture skip rate control. [0]\n"
            "  -q[n] --qpHdr             -1..51, Default frame header qp. [26]\n"
            "                             -1=Encoder calculates initial QP\n"
            "  -n[n] --qpMin             0..51, Minimum frame header qp. [10]\n"
            "  -m[n] --qpMax             0..51, Maximum frame header qp. [51]\n"
            "  -A[n] --intraQpDelta      -12..12, Intra QP delta. [-3]\n"
            "  -G[n] --fixedIntraQp      0..51, Fixed Intra QP, 0 = disabled. [0]\n"
            "  -2[n] --mbQpAdjustment    -8..7, MAD based MB QP adjustment, 0 = disabled. [0]\n");

    ALOGV(
            "\n  -z[n] --userData          SEI User data file name.\n"
            "  -1[n]:[n] --bpsAdjust     Frame:bitrate for adjusting bitrate on the fly.\n"
            "  -d    --psnr              Enables PSNR calculation for each frame.\n");

    ALOGV(
            "\nTesting parameters that are not supported for end-user:\n"
            "  -Q[n] --chromaQpOffset    -12..12 Chroma QP offset. [2]\n"
            "  -W[n] --filterOffsetA     -6..6 Deblockiong filter offset A. [0]\n"
            "  -E[n] --filterOffsetB     -6..6 Deblockiong filter offset B. [0]\n"
            "  -N[n] --burstSize          0..63 HW bus burst size. [16]\n"
            "  -t[n] --burstType          0=SIGLE, 1=INCR HW bus burst type. [0]\n"
            "  -M[n] --quarterPixelMv     0=Disable, 1=Enable.\n"
            "  -e[n] --testId            Internal test ID. [0]\n"
            "  -P[n] --trigger           Logic Analyzer trigger at picture <n>. [-1]\n"
            "\n");
    ;
}

/*------------------------------------------------------------------------------

    WriteStrm
        Write encoded stream to file

    Params:
        fout    - file to write
        strbuf  - data to be written
        size    - amount of data to write
        endian  - data endianess, big or little

------------------------------------------------------------------------------*/
void WriteStrm(FILE * fout, u32 * strmbuf, u32 size, u32 endian)
{

#ifdef NO_OUTPUT_WRITE
    return;
#endif

    /* Swap the stream endianess before writing to file if needed */
    if(endian == 1)
    {
        u32 i = 0, words = (size + 3) / 4;

        while(words)
        {
            u32 val = strmbuf[i];
            u32 tmp = 0;

            tmp |= (val & 0xFF) << 24;
            tmp |= (val & 0xFF00) << 8;
            tmp |= (val & 0xFF0000) >> 8;
            tmp |= (val & 0xFF000000) >> 24;
            strmbuf[i] = tmp;
            words--;
            i++;
        }

    }

    /* Write the stream to file */
    fwrite(strmbuf, 1, size, fout);
}

/*------------------------------------------------------------------------------

    NextPic

    Function calculates next input picture depending input and output frame
    rates.

    Input   inputRateNumer  (input.yuv) frame rate numerator.
            inputRateDenom  (input.yuv) frame rate denominator
            outputRateNumer (stream.mpeg4) frame rate numerator.
            outputRateDenom (stream.mpeg4) frame rate denominator.
            frameCnt        Frame counter.
            firstPic        The first picture of input.yuv sequence.

    Return  next    The next picture of input.yuv sequence.

------------------------------------------------------------------------------*/
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

/*------------------------------------------------------------------------------

    ChangeInput
        Change input file.
    Params:
        argc    - number of arguments to the application
        argv    - argument list as provided to the application
        option  - list of accepted cmdline options

    Returns:
        0 - for success
------------------------------------------------------------------------------*/
int ChangeInput(i32 argc, char **argv, char **name, option_s * option)
{
    i32 ret;
    argument_s argument;
    i32 enable = 0;

    argument.optCnt = 1;
    while((ret = EncGetOption(argc, argv, option, &argument)) != -1)
    {
        if((ret == 1) && (enable))
        {
            *name = argument.optArg;
            ALOGV("\nNext file: %s\n", *name);
            return 1;
        }
        if(argument.optArg == *name)
        {
            enable = 1;
        }
    }

    return 0;
}

/*------------------------------------------------------------------------------

    API tracing
        TRacing as defined by the API.
    Params:
        msg - null terminated tracing message
------------------------------------------------------------------------------*/
void H264EncTrace(const char *msg)
{
    static FILE *fp = NULL;

    if(fp == NULL)
        fp = fopen("api.trc", "wt");

    if(fp)
        fprintf(fp, "%s\n", msg);
}

/*------------------------------------------------------------------------------
    Get out pure NAL units from byte stream format (one picture data)
    This is an example!

    Params:
        pNaluSizeBuf - buffer where the individual NAL size are (return by API)
        pStream- buffre containign  the whole picture data
------------------------------------------------------------------------------*/
void NalUnitsFromByteStream(const u32 * pNaluSizeBuf, const u8 * pStream)
{
    u32 nalSize, nal;
    const u8 *pNalBase;

    nal = 0;
    pNalBase = pStream + 4; /* skip the 4-byte startcode */

    while(pNaluSizeBuf[nal] != 0)   /* after the last NAL unit size we have a zero */
    {
        nalSize = pNaluSizeBuf[nal] - 4;

        /* now we have the pure NAL unit, do something with it */
        /* DoSomethingWithThisNAL(pNalBase, nalSize); */

        pNalBase += pNaluSizeBuf[nal];  /* next NAL data base address */
        nal++;  /* next NAL unit */
    }
}

/*------------------------------------------------------------------------------

------------------------------------------------------------------------------*/
void PrintNalSizes(const u32 *pNaluSizeBuf, const u8 *pOutBuf, u32 strmSize,
        i32 byteStream)
{
    u32 nalu = 0, naluSum = 0;

    /* Step through the NALU size buffer */
    while(pNaluSizeBuf && pNaluSizeBuf[nalu] != 0)
    {
#ifdef INTERNAL_TEST
        /* In byte stream format each NAL must start with
         * start code: any number of leading zeros + 0x000001 */
        if (byteStream) {
            int zero_count = 0;
            const u8 *pTmp = pOutBuf + naluSum;

            /* count zeros, shall be at least 2 */
            while(*pTmp++ == 0x00)
                zero_count++;

            if(zero_count < 2 || pTmp[-1] != 0x01)
                ALOGV
                    ("Error: NAL unit %d at %d doesn't start with '00 00 01'  ",
                     nalu, naluSum);
        }
#endif

        naluSum += pNaluSizeBuf[nalu];
        ALOGV("%d  ", pNaluSizeBuf[nalu++]);
    }

#ifdef INTERNAL_TEST
    /* NAL sum must be the same as the whole frame stream size */
    if (naluSum && naluSum != strmSize)
        ALOGV("Error: Sum of NAL size (%d) does not match stream size\n",
                naluSum);
#endif
}

/*------------------------------------------------------------------------------
    WriteNalSizesToFile
        Dump NAL size to a file

    Params:
        file         - file name where toi dump
        pNaluSizeBuf - buffer where the individual NAL size are (return by API)
        buffSize     - size in bytes of the above buffer
------------------------------------------------------------------------------*/
void WriteNalSizesToFile(const char *file, const u32 * pNaluSizeBuf,
                         u32 buffSize)
{
    FILE *fo;
    u32 offset = 0;

    fo = fopen(file, "at");

    if(fo == NULL)
    {
        ALOGV("FAILED to open NAL size tracing file <%s>\n", file);
        return;
    }

    while(offset < buffSize && *pNaluSizeBuf != 0)
    {
        fALOGV(fo, "%d\n", *pNaluSizeBuf++);
        offset += sizeof(u32);
    }

    fclose(fo);
}

/*------------------------------------------------------------------------------
    PrintErrorValue
        Print return error value
------------------------------------------------------------------------------*/
void PrintErrorValue(const char *errorDesc, u32 retVal)
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

    fALOGV( "%s Return value: %s\n", errorDesc, str);
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
