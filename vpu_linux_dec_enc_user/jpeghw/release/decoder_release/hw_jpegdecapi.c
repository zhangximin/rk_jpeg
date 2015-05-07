#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "dwl.h"
#include "jpegdeccontainer.h"

#include "deccfg.h"
#include "regdrv.h"

#include "hw_jpegdecapi.h"
#include "hw_jpegdecutils.h"

#define DEFAULT -1

#ifdef DEBUG_HW_JPEG
#define LocalPrint printf
#else
#define LocalPrint(...)
#endif

//#define LOGYC printf
#define LOGYC

static void PrintRetYC(JpegDecRet * pJpegRet)
{

    assert(pJpegRet);

    switch (*pJpegRet)
    {
    case JPEGDEC_FRAME_READY:
        printf("TB: jpeg API returned : JPEGDEC_FRAME_READY\n");
        break;
    case JPEGDEC_OK:
        printf("TB: jpeg API returned : JPEGDEC_OK\n");
        break;
    case JPEGDEC_ERROR:
        printf("TB: jpeg API returned : JPEGDEC_ERROR\n");
        break;
    case JPEGDEC_DWL_HW_TIMEOUT:
        printf( "TB: jpeg API returned : JPEGDEC_HW_TIMEOUT\n");
        break;
    case JPEGDEC_UNSUPPORTED:
        printf("TB: jpeg API returned : JPEGDEC_UNSUPPORTED\n");
        break;
    case JPEGDEC_PARAM_ERROR:
        printf("TB: jpeg API returned : JPEGDEC_PARAM_ERROR\n");
        break;
    case JPEGDEC_MEMFAIL:
        printf( "TB: jpeg API returned : JPEGDEC_MEMFAIL\n");
        break;
    case JPEGDEC_INITFAIL:
        printf( "TB: jpeg API returned : JPEGDEC_INITFAIL\n");
        break;
    case JPEGDEC_HW_BUS_ERROR:
        printf("TB: jpeg API returned : JPEGDEC_HW_BUS_ERROR\n");
        break;
    case JPEGDEC_SYSTEM_ERROR:
        printf("TB: jpeg API returned : JPEGDEC_SYSTEM_ERROR\n");
        break;
    case JPEGDEC_DWL_ERROR:
        printf( "TB: jpeg API returned : JPEGDEC_DWL_ERROR\n");
        break;
    case JPEGDEC_INVALID_STREAM_LENGTH:
        printf(
                "TB: jpeg API returned : JPEGDEC_INVALID_STREAM_LENGTH\n");
        break;
    case JPEGDEC_STRM_ERROR:
        printf( "TB: jpeg API returned : JPEGDEC_STRM_ERROR\n");
        break;
    case JPEGDEC_INVALID_INPUT_BUFFER_SIZE:
        printf(
                "TB: jpeg API returned : JPEGDEC_INVALID_INPUT_BUFFER_SIZE\n");
        break;
    case JPEGDEC_INCREASE_INPUT_BUFFER:
        printf(
                "TB: jpeg API returned : JPEGDEC_INCREASE_INPUT_BUFFER\n");
        break;
    case JPEGDEC_SLICE_MODE_UNSUPPORTED:
        printf(
                "TB: jpeg API returned : JPEGDEC_SLICE_MODE_UNSUPPORTED\n");
        break;
    default:
        printf( "TB: jpeg API returned unknown status\n");
        break;
    }
}
static unsigned int LocalGetSystemTime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned int) (tv.tv_sec * 1000 + tv.tv_usec / 1000 ); // microseconds to milliseconds
}

#ifdef DEBUG_HW_JPEG
#include <utils/Log.h>
#define LOG_TAG "HW_JPEG"


static unsigned int LocalGetSystemTime_(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned int) (tv.tv_sec * 1000 + tv.tv_usec / 1000 ); // microseconds to milliseconds
}

/*-----------------------------------------------------------------------------

Print JPEG api return value

-----------------------------------------------------------------------------*/
static void PrintRet(JpegDecRet * pJpegRet)
{

    assert(pJpegRet);

    switch (*pJpegRet)
    {
    case JPEGDEC_FRAME_READY:
        LocalPrint(LOG_DEBUG,"jpeghw",("TB: jpeg API returned : JPEGDEC_FRAME_READY\n"));
        break;
    case JPEGDEC_OK:
        LocalPrint(LOG_DEBUG,"jpeghw",("TB: jpeg API returned : JPEGDEC_OK\n"));
        break;
    case JPEGDEC_ERROR:
        LocalPrint(LOG_DEBUG,"jpeghw",("TB: jpeg API returned : JPEGDEC_ERROR\n"));
        break;
    case JPEGDEC_DWL_HW_TIMEOUT:
        LocalPrint(LOG_DEBUG,"jpeghw",( "TB: jpeg API returned : JPEGDEC_HW_TIMEOUT\n"));
        break;
    case JPEGDEC_UNSUPPORTED:
        LocalPrint(LOG_DEBUG,"jpeghw",("TB: jpeg API returned : JPEGDEC_UNSUPPORTED\n"));
        break;
    case JPEGDEC_PARAM_ERROR:
        LocalPrint(LOG_DEBUG,"jpeghw",("TB: jpeg API returned : JPEGDEC_PARAM_ERROR\n"));
        break;
    case JPEGDEC_MEMFAIL:
        LocalPrint(LOG_DEBUG,"jpeghw",( "TB: jpeg API returned : JPEGDEC_MEMFAIL\n"));
        break;
    case JPEGDEC_INITFAIL:
        LocalPrint(LOG_DEBUG,"jpeghw",( "TB: jpeg API returned : JPEGDEC_INITFAIL\n"));
        break;
    case JPEGDEC_HW_BUS_ERROR:
        LocalPrint(LOG_DEBUG,"jpeghw",("TB: jpeg API returned : JPEGDEC_HW_BUS_ERROR\n"));
        break;
    case JPEGDEC_SYSTEM_ERROR:
        LocalPrint(LOG_DEBUG,"jpeghw",("TB: jpeg API returned : JPEGDEC_SYSTEM_ERROR\n"));
        break;
    case JPEGDEC_DWL_ERROR:
        LocalPrint(LOG_DEBUG,"jpeghw",( "TB: jpeg API returned : JPEGDEC_DWL_ERROR\n"));
        break;
    case JPEGDEC_INVALID_STREAM_LENGTH:
        LocalPrint(LOG_DEBUG,"jpeghw",(
                "TB: jpeg API returned : JPEGDEC_INVALID_STREAM_LENGTH\n"));
        break;
    case JPEGDEC_STRM_ERROR:
        LocalPrint(LOG_DEBUG,"jpeghw",( "TB: jpeg API returned : JPEGDEC_STRM_ERROR\n"));
        break;
    case JPEGDEC_INVALID_INPUT_BUFFER_SIZE:
        LocalPrint(LOG_DEBUG,"jpeghw",(
                "TB: jpeg API returned : JPEGDEC_INVALID_INPUT_BUFFER_SIZE\n"));
        break;
    case JPEGDEC_INCREASE_INPUT_BUFFER:
        LocalPrint(LOG_DEBUG,"jpeghw",(
                "TB: jpeg API returned : JPEGDEC_INCREASE_INPUT_BUFFER\n"));
        break;
    case JPEGDEC_SLICE_MODE_UNSUPPORTED:
        LocalPrint(LOG_DEBUG,"jpeghw",(
                "TB: jpeg API returned : JPEGDEC_SLICE_MODE_UNSUPPORTED\n"));
        break;
    default:
        LocalPrint(LOG_DEBUG,"jpeghw",( "TB: jpeg API returned unknown status\n"));
        break;
    }
}
#endif

/*
static RK_U32 FindImageInfoEnd(RK_U8 * pStream, RK_U32 streamLength, RK_U32 * pOffset)
{
    RK_U32 i;

    for(i = 0; i < streamLength; ++i)
    {
        if(0xFF == pStream[i])
        {
            if(((i + 32) < streamLength) && 0xDA == pStream[i + 1])
            {
                *pOffset = i;
                return 0;
            }
        }
    }
    return 1;
}*/

int SetPostProcessor(unsigned int * reg,VPUMemLinear_t *dst,int inWidth,int inHeigth,
								int outWidth,int outHeight,int inColor, PostProcessInfo *ppInfo)//,int outColor,HW_BOOL dither)
{
	int outColor = ppInfo->outFomart;
	HW_BOOL dither = ppInfo->shouldDither;

	SetDecRegister(reg, HWIF_PP_AXI_RD_ID, 0);
	SetDecRegister(reg, HWIF_PP_AXI_WR_ID, 0);
	SetDecRegister(reg, HWIF_PP_AHB_HLOCK_E, 1);
	SetDecRegister(reg, HWIF_PP_SCMD_DIS, 1);
	SetDecRegister(reg, HWIF_PP_IN_A2_ENDSEL, 1);
	SetDecRegister(reg, HWIF_PP_IN_A1_SWAP32, 1);
	SetDecRegister(reg, HWIF_PP_IN_A1_ENDIAN, 1);
	SetDecRegister(reg, HWIF_PP_IN_SWAP32_E, 1);
	SetDecRegister(reg, HWIF_PP_DATA_DISC_E, 1);
	SetDecRegister(reg, HWIF_PP_CLK_GATE_E, 0);
	SetDecRegister(reg, HWIF_PP_IN_ENDIAN, 1);
	SetDecRegister(reg, HWIF_PP_OUT_ENDIAN, 1);
	SetDecRegister(reg, HWIF_PP_OUT_SWAP32_E, 1);
	SetDecRegister(reg, HWIF_PP_MAX_BURST, 16);

	SetDecRegister(reg, HWIF_PP_IN_LU_BASE, 0);
	
	SetDecRegister(reg, HWIF_EXT_ORIG_WIDTH, inWidth>>4);

	if(ppInfo->cropW <= 0)
	{
		SetDecRegister(reg, HWIF_PP_IN_W_EXT,
					(((inWidth / 16) & 0xE00) >> 9));
		SetDecRegister(reg, HWIF_PP_IN_WIDTH,
					((inWidth / 16) & 0x1FF));
		SetDecRegister(reg, HWIF_PP_IN_H_EXT,
					(((inHeigth / 16) & 0x700) >> 8));
		SetDecRegister(reg, HWIF_PP_IN_HEIGHT,
					((inHeigth / 16) & 0x0FF));
	}else{
		SetDecRegister(reg, HWIF_PP_IN_W_EXT,
					(((ppInfo->cropW / 16) & 0xE00) >> 9));
		SetDecRegister(reg, HWIF_PP_IN_WIDTH,
					((ppInfo->cropW / 16) & 0x1FF));
		SetDecRegister(reg, HWIF_PP_IN_H_EXT,
					(((ppInfo->cropH / 16) & 0x700) >> 8));
		SetDecRegister(reg, HWIF_PP_IN_HEIGHT,
					((ppInfo->cropH / 16) & 0x0FF));
		SetDecRegister(reg, HWIF_CROP_STARTX_EXT,
					(((ppInfo->cropX / 16) & 0xE00) >> 9));
		SetDecRegister(reg, HWIF_CROP_STARTX,
					((ppInfo->cropX / 16) & 0x1FF));
		SetDecRegister(reg, HWIF_CROP_STARTY_EXT,
					(((ppInfo->cropY / 16) & 0x700) >> 8));
		SetDecRegister(reg, HWIF_CROP_STARTY,
					((ppInfo->cropY / 16) & 0x0FF));
		if(ppInfo->cropW & 0x0F)
		{
			SetDecRegister(reg, HWIF_PP_CROP8_R_E, 1);
		}else{
			SetDecRegister(reg, HWIF_PP_CROP8_R_E, 0);
		}
		if(ppInfo->cropH & 0x0F)
		{
			SetDecRegister(reg, HWIF_PP_CROP8_D_E, 1);
		}else{
			SetDecRegister(reg, HWIF_PP_CROP8_D_E, 0);
		}
		inWidth = ppInfo->cropW;
		inHeigth= ppInfo->cropH;
	}
	
	SetDecRegister(reg, HWIF_DISPLAY_WIDTH, outWidth);
	SetDecRegister(reg, HWIF_PP_OUT_WIDTH, outWidth);
	SetDecRegister(reg, HWIF_PP_OUT_HEIGHT, outHeight);
	SetDecRegister(reg, HWIF_PP_OUT_LU_BASE, dst->phy_addr);

	switch (inColor)
	{
		case	PP_IN_FORMAT_YUV422INTERLAVE:
			SetDecRegister(reg, HWIF_PP_IN_FORMAT, 0);
			break;
		case	PP_IN_FORMAT_YUV420SEMI:
			SetDecRegister(reg, HWIF_PP_IN_FORMAT, 1);
			break;
		case	PP_IN_FORMAT_YUV420PLANAR:
			SetDecRegister(reg, HWIF_PP_IN_FORMAT, 2);
			break;
		case	PP_IN_FORMAT_YUV400:
			SetDecRegister(reg, HWIF_PP_IN_FORMAT, 3);
			break;
		case	PP_IN_FORMAT_YUV422SEMI:
			SetDecRegister(reg, HWIF_PP_IN_FORMAT, 4);
			break;
		case	PP_IN_FORMAT_YUV420SEMITIELED:
			SetDecRegister(reg, HWIF_PP_IN_FORMAT, 5);
			break;
		case	PP_IN_FORMAT_YUV440SEMI:
			SetDecRegister(reg, HWIF_PP_IN_FORMAT, 6);
			break;
		case	PP_IN_FORMAT_YUV444_SEMI:
			SetDecRegister(reg, HWIF_PP_IN_FORMAT, 7);
			SetDecRegister(reg, HWIF_PP_IN_FORMAT_ES, 0);
			break;
		case	PP_IN_FORMAT_YUV411_SEMI:
			SetDecRegister(reg, HWIF_PP_IN_FORMAT, 7);
			SetDecRegister(reg, HWIF_PP_IN_FORMAT_ES, 1);
			break;
		default:
			return -1;
	}
#define VIDEORANGE 1	//0 or 1
	int videoRange = VIDEORANGE;	
	SetDecRegister(reg, HWIF_RANGEMAP_COEF_Y, 9);
	SetDecRegister(reg, HWIF_RANGEMAP_COEF_C, 9);

	/*  brightness */
	SetDecRegister(reg, HWIF_COLOR_COEFFF, BRIGHTNESS);

	if (outColor <= PP_OUT_FORMAT_ARGB)
	{
		/*Bt.601*/
		unsigned int	a = 298;
		unsigned int	b = 409;
		unsigned int	c = 208;
		unsigned int	d = 100;
		unsigned int	e = 516;
		
		/*Bt.709
		unsigned int	a = 298;
		unsigned int	b = 459;
		unsigned int	c = 137;
		unsigned int	d = 55;
		unsigned int	e = 544;*/

		int	satur = 0, tmp;
		if(videoRange != 0){
			/*Bt.601*/
			a = 256;
			b = 350;
			c = 179;
			d = 86;
			e = 443;
			/*Bt.709
			a = 256;
			b = 403;
			c = 120;
			d = 48;
			e = 475;*/
			SetDecRegister(reg, HWIF_YCBCR_RANGE, videoRange);
		}
		int contrast = CONTRAST;
		if(contrast != 0)
		{
			int thr1y, thr2y, off1, off2, thr1, thr2, a1, a2;
			if(videoRange == 0){
				int tmp1, tmp2;
				/* Contrast */
                thr1 = (219 * (contrast + 128)) / 512;
                thr1y = (219 - 2 * thr1) / 2;
                thr2 = 219 - thr1;
                thr2y = 219 - thr1y;

                tmp1 = (thr1y * 256) / thr1;
                tmp2 = ((thr2y - thr1y) * 256) / (thr2 - thr1);
                off1 = ((thr1y - ((tmp2 * thr1) / 256)) * a) / 256;
                off2 = ((thr2y - ((tmp1 * thr2) / 256)) * a) / 256;

                tmp1 = (64 * (contrast + 128)) / 128;
                tmp2 = 256 * (128 - tmp1);
                a1 = (tmp2 + off2) / thr1;
                a2 = a1 + (256 * (off2 - 1)) / (thr2 - thr1);
			}else{
				/* Contrast */
                thr1 = (64 * (contrast + 128)) / 128;
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

            SetDecRegister(reg, HWIF_CONTRAST_THR1, thr1);
            SetDecRegister(reg, HWIF_CONTRAST_THR2, thr2);

            SetDecRegister(reg, HWIF_CONTRAST_OFF1, off1);
            SetDecRegister(reg, HWIF_CONTRAST_OFF2, off2);

            SetDecRegister(reg, HWIF_COLOR_COEFFA1, a1);
            SetDecRegister(reg, HWIF_COLOR_COEFFA2, a2);
		}else{
			SetDecRegister(reg, HWIF_CONTRAST_THR1, 55);
			SetDecRegister(reg, HWIF_CONTRAST_THR2, 165);
			SetDecRegister(reg, HWIF_CONTRAST_OFF1, 0);
			SetDecRegister(reg, HWIF_CONTRAST_OFF2, 0);
			tmp = a;
			if(tmp > 1023)
				tmp = 1023;
			else if(tmp < 0)
				tmp = 0;
			SetDecRegister(reg, HWIF_COLOR_COEFFA1, tmp);
			SetDecRegister(reg, HWIF_COLOR_COEFFA2, tmp);
		}

		SetDecRegister(reg, HWIF_PP_OUT_ENDIAN, 0);

		/* saturation */
		satur = 64 + SATURATION;

		tmp = (satur * (int) b) / 64;
		if(tmp > 1023)
			tmp = 1023;
		else if(tmp < 0)
			tmp = 0;
		SetDecRegister(reg, HWIF_COLOR_COEFFB, (unsigned int) tmp);

		tmp = (satur * (int) c) / 64;
		if(tmp > 1023)
			tmp = 1023;
		else if(tmp < 0)
			tmp = 0;
		SetDecRegister(reg, HWIF_COLOR_COEFFC, (unsigned int) tmp);

		tmp = (satur * (int) d) / 64;
		if(tmp > 1023)
			tmp = 1023;
		else if(tmp < 0)
			tmp = 0;
		SetDecRegister(reg, HWIF_COLOR_COEFFD, (unsigned int) tmp);

		tmp = (satur * (int) e) / 64;
		if(tmp > 1023)
			tmp = 1023;
		else if(tmp < 0)
			tmp = 0;

		SetDecRegister(reg, HWIF_COLOR_COEFFE, (unsigned int) tmp);
	}
	
	switch (outColor)
	{
		case	PP_OUT_FORMAT_RGB565:
			SetDecRegister(reg, HWIF_R_MASK, 0xF800F800);
			SetDecRegister(reg, HWIF_G_MASK, 0x07E007E0);
			SetDecRegister(reg, HWIF_B_MASK, 0x001F001F);

			SetDecRegister(reg, HWIF_RGB_R_PADD, 0);
			SetDecRegister(reg, HWIF_RGB_G_PADD, 5);
			SetDecRegister(reg, HWIF_RGB_B_PADD, 11);
			if(dither)//always do dither
			{
				LOGYC("we do dither.");
				SetDecRegister(reg, HWIF_DITHER_SELECT_R, 2);
				SetDecRegister(reg, HWIF_DITHER_SELECT_G, 3);
				SetDecRegister(reg, HWIF_DITHER_SELECT_B, 2);
			}else{
				LOGYC("we do not dither.");
			}
			SetDecRegister(reg, HWIF_RGB_PIX_IN32, 1);
			SetDecRegister(reg, HWIF_PP_OUT_SWAP16_E,1);
			SetDecRegister(reg, HWIF_PP_OUT_FORMAT, 0);
			break;
		case	PP_OUT_FORMAT_ARGB:
			SetDecRegister(reg, HWIF_R_MASK, 0x000000FF | (0xff << 24));
			SetDecRegister(reg, HWIF_G_MASK, 0x0000FF00 | (0xff << 24));
			SetDecRegister(reg, HWIF_B_MASK, 0x00FF0000 | (0xff << 24));
			SetDecRegister(reg, HWIF_RGB_R_PADD, 24);
			SetDecRegister(reg, HWIF_RGB_G_PADD, 16);
			SetDecRegister(reg, HWIF_RGB_B_PADD, 8);
		
			SetDecRegister(reg, HWIF_RGB_PIX_IN32, 0);
			SetDecRegister(reg, HWIF_PP_OUT_FORMAT, 0);
			break;
		case	PP_OUT_FORMAT_YUV422INTERLAVE:
			SetDecRegister(reg, HWIF_PP_OUT_FORMAT, 3);
			break;
		case	PP_OUT_FORMAT_YUV420INTERLAVE:
			SetDecRegister(reg, HWIF_PP_OUT_CH_BASE, dst->phy_addr+outWidth*outHeight);
			SetDecRegister(reg, HWIF_PP_OUT_FORMAT, 5);
			break;
		default:
			return -1;
	}

	SetDecRegister(reg, HWIF_ROTATION_MODE, 0);


	{
		unsigned int		inw,inh;
		unsigned int		outw,outh;

		inw = inWidth -1;
		inh = inHeigth -1;
		outw = outWidth -1;
		outh = outHeight -1;


		if (inw < outw)
		{
			SetDecRegister(reg, HWIF_HOR_SCALE_MODE, 1);
			SetDecRegister(reg, HWIF_SCALE_WRATIO, (outw<<16)/inw);
			SetDecRegister(reg, HWIF_WSCALE_INVRA, (inw<<16)/outw);
		}
		else if (inw > outw)
		{
			SetDecRegister(reg, HWIF_HOR_SCALE_MODE, 2);
			SetDecRegister(reg, HWIF_WSCALE_INVRA, ((outw+1)<<16)/(inw+1));
		}
		else
			SetDecRegister(reg, HWIF_HOR_SCALE_MODE, 0);

		if (inh < outh)
		{
			SetDecRegister(reg, HWIF_VER_SCALE_MODE, 1);
			SetDecRegister(reg, HWIF_SCALE_HRATIO, (outh<<16)/inh);
			SetDecRegister(reg, HWIF_HSCALE_INVRA, (inh<<16)/outh);
		}
		else if (inh > outh)
		{
			SetDecRegister(reg, HWIF_VER_SCALE_MODE, 2);
			SetDecRegister(reg, HWIF_HSCALE_INVRA, ((outh+1)<<16)/(inh+1)+1);
		}
		else
			SetDecRegister(reg, HWIF_VER_SCALE_MODE, 0);
	}

	SetDecRegister(reg, HWIF_PP_PIPELINE_E, 1);
	return 0;
}

void resetImageInfo(JpegDecImageInfo * imageInfo){
	/* reset imageInfo */
    imageInfo->displayWidth = 0;
    imageInfo->displayHeight = 0;
    imageInfo->outputWidth = 0;
    imageInfo->outputHeight = 0;
    imageInfo->version = 0;
    imageInfo->units = 0;
    imageInfo->xDensity = 0;
    imageInfo->yDensity = 0;
    imageInfo->outputFormat = 0;
    imageInfo->thumbnailType = 0;
    imageInfo->displayWidthThumb = 0;
    imageInfo->displayHeightThumb = 0;
    imageInfo->outputWidthThumb = 0;
    imageInfo->outputHeightThumb = 0;
    imageInfo->outputFormatThumb = 0;
}

int hw_jpeg_decode(HwJpegInputInfo *inInfo, HwJpegOutputInfo *outInfo, char *reuseBitmap, int bm_w, int bm_h){
	JpegDecInst jpegInst = NULL;
	JpegDecRet jpegRet;
	JpegDecImageInfo imageInfo;
	JpegDecInput jpegIn;
	JpegDecOutput jpegOut;
	JpegDecContainer *jpegC;
	VPUMemLinear_t streamMem;
	VPUMemLinear_t vpuMem;
	VPUMemLinear_t ppOutMem;
	struct hw_jpeg_source_mgr *src = inInfo->streamCtl.inStream;
	RK_U32 infoRet = 0;
#ifdef DEBUG_HW_JPEG
	JpegDecApiVersion decVer;
    JpegDecBuild decBuild;
#endif
	int ppInputFomart = -1;
	RK_U32 ppScaleW = 96, ppScaleH = 96;
	int sampleSize = inInfo->ppInfo.scale_denom;
	HW_BOOL isVpuMemSrc = src->isVpuMem;
	int hwUseThumb = 0;
	RK_U32 bufsize = 0;
	int getDataLength = -1;
	RK_U32 mcuSizeDivider = 0,amountOfMCUs = 0, mcuInRow = 0;
	HW_BOOL retnBforWriteMei = 0;
#define DEBUG_HW_JPEG_TIME
#ifdef DEBUG_HW_JPEG_TIME
	unsigned int startTime,stopTime;
#endif
	LOGYC(" <%s>_%d \n", __func__, __LINE__);	
	CODEBEGIN
	streamMem.vir_addr = NULL;
	streamMem.phy_addr = 0;
	ppOutMem.vir_addr = NULL;
	ppOutMem.phy_addr = 0;
	jpegIn.streamBuffer.pVirtualAddress = NULL;
    jpegIn.streamBuffer.busAddress = 0;
    jpegIn.streamLength = 0;
    jpegIn.pictureBufferY.pVirtualAddress = NULL;
    jpegIn.pictureBufferY.busAddress = 0;
    jpegIn.pictureBufferCbCr.pVirtualAddress = NULL;
    jpegIn.pictureBufferCbCr.busAddress = 0;

    jpegIn.sliceMbSet = 0;
	jpegIn.streamLength = inInfo->streamCtl.wholeStreamLength;
    jpegIn.bufferSize = 0;

    /* reset output */
    jpegOut.outputPictureY.pVirtualAddress = NULL;
    jpegOut.outputPictureY.busAddress = 0;
    jpegOut.outputPictureCbCr.pVirtualAddress = NULL;
    jpegOut.outputPictureCbCr.busAddress = 0;
    jpegOut.outputPictureCr.pVirtualAddress = NULL;
    jpegOut.outputPictureCr.busAddress = 0;

	outInfo->decoderHandle = NULL;
	outInfo->outAddr = NULL;
	outInfo->ppscalew = outInfo->outWidth = -1;
	outInfo->ppscaleh = outInfo->outHeight = -1;
	outInfo->shouldScale = 0;
    
	resetImageInfo(&imageInfo);
#ifdef DEBUG_HW_JPEG
    /* Print API and build version numbers */
     //LocalPrint(("Ready to JpegGetAPIVersion!!!\n"));

    decVer = JpegGetAPIVersion();
    decBuild = JpegDecGetBuild();
	
    /* Version */
    //LocalPrint("X170 JPEG Decoder API v%d.%d - SW build: %d - HW build: %x\n",
    //        (int)decVer.major, decVer.minor, decBuild.swBuild, decBuild.hwBuild);
#endif

	LOGYC(" <%s>_%d \n", __func__, __LINE__);	
	//init stream
	(*src->init_source)(inInfo);
	//imageinfo
	jpegRet = JpegDecGetSimpleImageInfo(inInfo, &jpegIn, &imageInfo, &infoRet);
	LOGYC(" <%s>_%d    jpegRet = %d \n", __func__, __LINE__, jpegRet);	
	if(jpegRet != JPEGDEC_OK)
	{
		LOGYC(" <%s>_%d \n", __func__, __LINE__);	
		retnBforWriteMei = 1;
		LOGYC("JpegDecGetImageInfo fail error code is  %d\n", (int)jpegRet);
		/* Handle here the error situation */
		break;
	}
	LOGYC("Base Image W,H: %d,%d \n",imageInfo.displayWidth, imageInfo.displayHeight);
	LOGYC("Base Image W,H: %d,%d, coding type: %d, progressive mode is %d \n", imageInfo.displayWidth, imageInfo.displayHeight, imageInfo.codingMode, JPEGDEC_PROGRESSIVE);
	LOGYC("imageInfo.out: %d,%d(align by 16) \n",imageInfo.outputWidth,imageInfo.outputHeight);
	LOGYC(" <%s>_%d    sampleSize = %d\n", __func__, __LINE__, sampleSize);	
#ifdef USE_DSTWH_DOSCALE
	if(sampleSize==0 && inInfo->ppInfo.dstWidth>0 && inInfo->ppInfo.dstHeight>0){
		ppScaleW = inInfo->ppInfo.dstWidth;
		ppScaleH = inInfo->ppInfo.dstHeight;
	}else 
#endif
	if(sampleSize>0){
		ppScaleW = (imageInfo.displayWidth + sampleSize - 1)/sampleSize;//set the same value to this in libjpeg
		ppScaleH = (imageInfo.displayHeight + sampleSize - 1)/sampleSize;
	}else{
		retnBforWriteMei = 1;
                jpegRet = JPEGDEC_UNSUPPORTED;
                break;
	}
	outInfo->outWidth = ppScaleW;
	outInfo->outHeight = ppScaleH;
/*	if(ppScaleW * sampleSize < imageInfo.displayWidth){
		outInfo->outWidth = (ppScaleW + 1) & (~1);//even for webview to show jpeg right
	}
	if(ppScaleH * sampleSize < imageInfo.displayHeight){
		outInfo->outHeight = (ppScaleH + 1) & (~1);
	}
*/
	ppScaleW = (ppScaleW + 15) & (~15); // pp dest width must be dividable by 16 not 8; //fix 1000*800 jpeg error. keng die de datasheet
	ppScaleH = (ppScaleH + 15) & (~15); // must be dividable by 2.in pp downscaling ,the output lines always equal (desire lines - 1);
	if(imageInfo.displayWidth > 4048 && ppScaleW > 1920){//practical value
		LOGYC("image width greater than 4048 and out width greater than 1920, displaywidth: %d, ppScaleW: %d \n", imageInfo.displayWidth, ppScaleW);
		retnBforWriteMei = 1;
		jpegRet = JPEGDEC_INVALID_INPUT_BUFFER_SIZE;
		break;
	} else if(ppScaleW < JPEGDEC_MIN_WIDTH || ppScaleH < JPEGDEC_MIN_HEIGHT){
		LOGYC("OUT W,H less than 48: %d,%d \n", ppScaleW, ppScaleH);
		retnBforWriteMei = 1;
		jpegRet = JPEGDEC_UNSUPPORTED;
		break;
	}
#ifdef USE_DSTWH_DOSCALE
	else if((ppScaleW>(imageInfo.displayWidth*3-1)) || (ppScaleH>(imageInfo.displayHeight*3-1))){
		LOGYC("OUT W,H three times larger than initwh, out: %d,%d, init:%d,%d\n", ppScaleW, ppScaleH, imageInfo.displayWidth, imageInfo.displayHeight);
                retnBforWriteMei = 1;
                jpegRet = JPEGDEC_UNSUPPORTED;
                break;
	}
#endif
#if 0
	if(ppScaleH > 1920){
		LOGYC("image outh is greater than 1280: %d, this limit value may be no need", ppScaleH);
		retnBforWriteMei = 1;
		jpegRet = JPEGDEC_INVALID_INPUT_BUFFER_SIZE;
		break;	
	}
	if(ppScaleW > imageInfo.displayWidth || ppScaleH > imageInfo.displayHeight
		|| ppScaleW > 1920 || ppScaleH > 1920){
		ppScaleW = (ppScaleW + 15) & (~15);
		ppScaleH = (ppScaleH + 15) & (~15);
	}
#endif
	outInfo->ppscalew = ppScaleW;
	outInfo->ppscaleh = ppScaleH;

	LOGYC("out w,h: %d,%d, has thumb: %d\n", outInfo->outWidth, outInfo->outHeight,
		imageInfo.thumbnailType == JPEGDEC_THUMBNAIL_JPEG);
	if(sampleSize>0 && sampleSize!=1 && imageInfo.thumbnailType==JPEGDEC_THUMBNAIL_JPEG && imageInfo.displayWidthThumb<320/*for the cts*/){
		LOGYC("thumb w,h: %d,%d\n",imageInfo.displayWidthThumb, imageInfo.displayHeightThumb);
		if(outInfo->outWidth <= imageInfo.displayWidthThumb
			&& outInfo->outHeight <= imageInfo.displayHeightThumb){
			inInfo->streamCtl.useThumb = 1;
			if(imageInfo.displayWidthThumb == outInfo->outWidth
				&& imageInfo.displayHeightThumb == outInfo->outHeight){
				inInfo->ppInfo.scale_denom = 1;
				LOGYC("new sample size is : 1 \n");
			} else {
				//recompute sample size
				unsigned int a = imageInfo.displayWidthThumb / outInfo->outWidth;
				unsigned int b = imageInfo.displayHeightThumb / outInfo->outHeight;
				if(a > b){
					a = b;
				}
				a = a >> 1;
				b = 1;
				while(a>0){//If has more effective way, FIX here
					b = b << 1;
					a = a >> 1;
				}
				inInfo->ppInfo.scale_denom = b;
				outInfo->shouldScale = 1;
				LOGYC("new sample size is : %d \n", b);
			}
if(*reuseBitmap == 1){
	if(outInfo->outWidth != bm_w || outInfo->outHeight != bm_h){
		LOGYC("thumb reuse wh do not corresponding \n");
		*reuseBitmap = -1;
	}
}
			//directly go soft decoder
			LOGYC("has thumb nail, then just go softdecoding.\n");
			retnBforWriteMei = 1;
			if(inInfo->justcaloutwh == 0){
				jpegRet = JPEGDEC_UNSUPPORTED;
			}
			break;	
#if 0
			if(imageInfo.codingModeThumb != JPEGDEC_PROGRESSIVE && infoRet == JPEGDEC_THUMB_SUPPORTED){
				hwUseThumb = 1;
			} else {
				LOGYC("hw do not support this thumbnail image. infoRet : 0x%x, imageInfo.codingModeThumb: %d", infoRet, imageInfo.codingModeThumb);
				retnBforWriteMei = 1;
				jpegRet = JPEGDEC_UNSUPPORTED;
				break;				
			}
#endif
		
		}
	}
	
	if(ppScaleW < imageInfo.displayWidth){
	 outInfo->outWidth = ppScaleW - (imageInfo.outputWidth-imageInfo.displayWidth)*ppScaleW/imageInfo.outputWidth;
	 
	}
	if(ppScaleH < imageInfo.displayHeight){
	 outInfo->outHeight = ppScaleH - (imageInfo.outputHeight-imageInfo.displayHeight)*ppScaleH/imageInfo.outputHeight;
	}

	if(hwUseThumb){
		bufsize = (inInfo->streamCtl.thumbLength + 255) & (~255);
	} else {
LOGYC("BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB, %d, JPEGDEC_HAVE_DRI:%d \n", infoRet, JPEGDEC_HAVE_DRI);
		//do not need thumb
		if(imageInfo.codingMode == JPEGDEC_PROGRESSIVE 
			|| (infoRet & JPEGDEC_UNSUPPORTEDSIZE) != 0
			|| (infoRet & JPEGDEC_YUV_UNSUPPORTED) != 0
			|| (infoRet & JPEGDEC_HAVE_DRI) != 0
			|| (imageInfo.displayWidth <= 160 && imageInfo.displayHeight <= 128)){//thumbnail ususlly is 160_120, if less than the size we take soft	
			retnBforWriteMei = 1;
LOGYC("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA \n");
			jpegRet = JPEGDEC_UNSUPPORTED;
			break;
		}
		bufsize = (inInfo->streamCtl.wholeStreamLength+255)&(~255); /* input stream buffer size in bytes */
	}
	if(inInfo->justcaloutwh == 1){
		//if just decode bound return at once
		LOGYC("just cal out wh, return.\n");
		break;
	}
	if(bufsize<JPEG_INPUT_BUFFER) {
		bufsize = JPEG_INPUT_BUFFER;
	}
	LOGYC(" <%s>_%d \n", __func__, __LINE__);	
	/* Jpeg initialization */
	jpegRet = JpegDecInit(&jpegInst);
	LOGYC(" <%s>_%d    jpegRet = %d \n", __func__, __LINE__, jpegRet);	
	if(jpegRet != JPEGDEC_OK)
	{
		LOGYC("JpegDecInit Fail!!!\n");
		retnBforWriteMei = 1;
		break;
	}
	outInfo->decoderHandle = jpegInst;

	LOGYC(" <%s>_%d \n", __func__, __LINE__);	
	if(!(*src->resync_to_restart)(inInfo)){
		//reset pos
		jpegRet = JPEGDEC_STRM_ERROR;
		retnBforWriteMei = 1;
		break;
	}
	LOGYC("\n  <%s>_%d  hwUseThumb = %d    isVpuMemSrc = %d \n", __func__, __LINE__, hwUseThumb, isVpuMemSrc);
	if(hwUseThumb || !isVpuMemSrc)
	{
		jpegRet = VPUMallocLinear(&streamMem,bufsize);
		if(jpegRet != DWL_OK)
    		{
			LOGYC("UNABLE TO ALLOCATE STREAM BUFFER MEMORY,ppScaleW = %d,ppScaleH = %d\n",ppScaleW,ppScaleH);
			break;
		}
	}
	LOGYC(" <%s>_%d \n", __func__, __LINE__);	
	if(hwUseThumb){
		getDataLength = (*src->fill_thumb)(inInfo, streamMem.vir_addr);
		if(getDataLength <= 0){
			jpegRet = JPEGDEC_STRM_ERROR;
			retnBforWriteMei = 1;
			break;
		}
		if(getDataLength <= inInfo->streamCtl.thumbLength)
    		{
			LOGYC("jpeghw","FillInputBuf Success,getDataLength = %d,wholeStreamLength = %d\n", getDataLength,inInfo->streamCtl.thumbLength);
			jpegIn.streamLength = inInfo->streamCtl.thumbLength; /* JPEG length in bytes */
			jpegIn.bufferSize = 0; /* input buffering */
    		}else{
			jpegIn.streamLength = inInfo->streamCtl.thumbLength; /* JPEG length in bytes */
			jpegIn.bufferSize = bufsize; /* input buffering */
    		}
//jpegRet = -1;break;//test fail
	} else {
		//if(isVpuMemSrc){
			//set value directly
		//	(*src->get_vpumemInst)(inInfo,&streamMem);
		//	getDataLength = inInfo->streamCtl.wholeStreamLength;
		//} else {
			if(isVpuMemSrc){
				(*src->get_vpumemInst)(inInfo,&streamMem);
			}
			//VPUMemLinear_t vpuMem;
			vpuMem.vir_addr = NULL;
			LOGYC("before fill_buffer, imageInfo.displayWidth = %d, imageInfo.displayHeight = %d \n", imageInfo.displayWidth, imageInfo.displayHeight);
			getDataLength = (*src->fill_buffer)(inInfo, streamMem.vir_addr, &vpuMem, imageInfo.displayWidth, imageInfo.displayHeight);
			LOGYC("    <%s>_%d   getDataLength = %d   streamMem.vir_addr = %p vpuMem.vir_addr = %p\n", __func__, __LINE__, getDataLength, streamMem.vir_addr, vpuMem.vir_addr);
			if(getDataLength <= 0){
				jpegRet = JPEGDEC_STRM_ERROR;
				//retnBforWriteMei = 1;
				break;
			}
#if 0
			FILE * testf = fopen("./testf.jpg", "wb");
			char * testd = (char *)streamMem.vir_addr;
			fwrite(testd, 1, getDataLength, testf);
			fclose(testf);
#endif
			if(vpuMem.vir_addr != NULL){
				if(!isVpuMemSrc && streamMem.vir_addr != NULL){
					VPUFreeLinear(&streamMem);
				}
				streamMem = vpuMem;
			}
		//}
		if(getDataLength <= inInfo->streamCtl.wholeStreamLength)
    		{
			LOGYC("FillInputBuf Success,getDataLength = %d,wholeStreamLength = %d\n", getDataLength,inInfo->streamCtl.wholeStreamLength);
			jpegIn.streamLength = inInfo->streamCtl.wholeStreamLength; /* JPEG length in bytes */
			jpegIn.bufferSize = 0; /* input buffering */
    		}else{
			jpegIn.streamLength = inInfo->streamCtl.wholeStreamLength; /* JPEG length in bytes */
			jpegIn.bufferSize = bufsize; /* input buffering */
    		}
	}
	char * tmp_data = (char *)streamMem.vir_addr;
	LOGYC(" <%s>_%d  <%x, %x, %x, %x>\n", __func__, __LINE__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);	
	VPUMemClean(&streamMem);
	LOGYC(" <%s>_%d  <%x, %x, %x, %x>\n", __func__, __LINE__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);	

	/* Pointer to the input JPEG */
	jpegIn.streamBuffer.pVirtualAddress = (RK_U32 *) streamMem.vir_addr;
	jpegIn.streamBuffer.busAddress = streamMem.phy_addr;
	/* Get image information of the JPEG image */
	resetImageInfo(&imageInfo);
	jpegRet = JpegDecGetImageInfo(jpegInst, &jpegIn, &imageInfo);//may be can ignore this
	LOGYC(" <%s>_%d     jpegRet = %d \n", __func__, __LINE__, jpegRet);
	if(jpegRet != JPEGDEC_OK){
		LOGYC("Dec getImageInfo fail. \n");
		break;
	}
	LOGYC(" <%s>_%d  *reuseBitmap = %d  <%d, %d>  <%d, %d>\n", 
		__func__, __LINE__, *reuseBitmap, outInfo->outWidth, outInfo->outHeight, bm_w, bm_h);
	if(*reuseBitmap == 1){
		if(outInfo->outWidth != bm_w || outInfo->outHeight != bm_h){
			LOGYC("reuse wh do not corresponding");
			*reuseBitmap = -1;
			break;
		}
	}
	jpegRet = VPUMallocLinear(&ppOutMem, ppScaleW * ppScaleH*2*(inInfo->ppInfo.outFomart+1));
	LOGYC(" <%s>_%d     jpegRet = %d \n", __func__, __LINE__, jpegRet);
	if(jpegRet != DWL_OK){
		LOGYC("UNABLE TO ALLOCATE STREAM BUFFER MEMORY,ppScaleW = %d,ppScaleH = %d\n",ppScaleW,ppScaleH);
		break;
	}
	//VPUMemFlush(&ppOutMem);

	jpegC = (JpegDecContainer*)jpegInst;
	jpegC->ppOutMem = ppOutMem;

	if(imageInfo.outputFormat)
    {
        switch (imageInfo.outputFormat)
        {
        case JPEGDEC_YCbCr400:
            LOGYC("\t\t-JPEG: FULL RESOLUTION OUTPUT: JPEGDEC_YCbCr400\n");
			ppInputFomart = PP_IN_FORMAT_YUV400;
            break;
        case JPEGDEC_YCbCr420_SEMIPLANAR:
                    LOGYC("\t\t-JPEG: FULL RESOLUTION OUTPUT: JPEGDEC_YCbCr420_SEMIPLANAR\n");
			ppInputFomart = PP_IN_FORMAT_YUV420SEMI;
            break;
        case JPEGDEC_YCbCr422_SEMIPLANAR:
                    LOGYC("\t\t-JPEG: FULL RESOLUTION OUTPUT: JPEGDEC_YCbCr422_SEMIPLANAR\n");
			ppInputFomart = PP_IN_FORMAT_YUV422SEMI;
            break;
        case JPEGDEC_YCbCr440:
                    LOGYC("\t\t-JPEG: FULL RESOLUTION OUTPUT: JPEGDEC_YCbCr440\n");
			ppInputFomart = PP_IN_FORMAT_YUV440SEMI;
            break;
        case JPEGDEC_YCbCr411_SEMIPLANAR:
                    LOGYC("\t\t-JPEG: FULL RESOLUTION OUTPUT: JPEGDEC_YCbCr411_SEMIPLANAR\n");
			ppInputFomart = PP_IN_FORMAT_YUV411_SEMI;
            break;
        case JPEGDEC_YCbCr444_SEMIPLANAR:
                    LOGYC("\t\t-JPEG: FULL RESOLUTION OUTPUT: JPEGDEC_YCbCr444_SEMIPLANAR\n");
			ppInputFomart = PP_IN_FORMAT_YUV444_SEMI;
            break;
		default:
			LOGYC("\t\t-JPEG: FULL RESOLUTION OUTPUT: %d\n",imageInfo.outputFormat);
        }
    }
	LOGYC(" <%s>_%d ppOutMem.vir_addr = %x\n", __func__, __LINE__, ppOutMem.vir_addr);
	SetPostProcessor(jpegC->jpegRegs, &ppOutMem, imageInfo.outputWidth, imageInfo.outputHeight,/*imageInfo.displayWidth, imageInfo.displayHeight,*/
					ppScaleW, ppScaleH, ppInputFomart, &inInfo->ppInfo);
	LOGYC(" ppScaleW is %d, ppScaleH is %d. inInfo->ppInfo.outFomart is %d.\n", ppScaleW, ppScaleH, inInfo->ppInfo.outFomart);
	jpegC->ppInstance = 1;//- - lazy way
	/* Slice mode */
	jpegIn.sliceMbSet = 0;

	/* calculate MCU's */
	if(imageInfo.outputFormat == JPEGDEC_YCbCr400 ||
	   imageInfo.outputFormat == JPEGDEC_YCbCr444_SEMIPLANAR)
	{
		amountOfMCUs =
			((imageInfo.outputWidth * imageInfo.outputHeight) / 64);
		mcuInRow = (imageInfo.outputWidth / 8);
	}
	else if(imageInfo.outputFormat == JPEGDEC_YCbCr420_SEMIPLANAR)
	{
		/* 265 is the amount of luma samples in MB for 4:2:0 */
		amountOfMCUs =
			((imageInfo.outputWidth * imageInfo.outputHeight) / 256);
		mcuInRow = (imageInfo.outputWidth / 16);
	}
	else if(imageInfo.outputFormat == JPEGDEC_YCbCr422_SEMIPLANAR)
	{
		/* 128 is the amount of luma samples in MB for 4:2:2 */
		amountOfMCUs =
			((imageInfo.outputWidth * imageInfo.outputHeight) / 128);
		mcuInRow = (imageInfo.outputWidth / 16);
	}
	else if(imageInfo.outputFormat == JPEGDEC_YCbCr440)
	{
		/* 128 is the amount of luma samples in MB for 4:4:0 */
		amountOfMCUs =
			((imageInfo.outputWidth * imageInfo.outputHeight) / 128);
		mcuInRow = (imageInfo.outputWidth / 8);
	}
	else if(imageInfo.outputFormat == JPEGDEC_YCbCr411_SEMIPLANAR)
	{
		amountOfMCUs =
			((imageInfo.outputWidth * imageInfo.outputHeight) / 256);
		mcuInRow = (imageInfo.outputWidth / 32);
	}
	
	/* set mcuSizeDivider for slice size count */
	if(imageInfo.outputFormat == JPEGDEC_YCbCr400 ||
	   imageInfo.outputFormat == JPEGDEC_YCbCr440 ||
	   imageInfo.outputFormat == JPEGDEC_YCbCr444_SEMIPLANAR)
		mcuSizeDivider = 2;
	else
		mcuSizeDivider = 1;

	/* 9190 and over 16M ==> force to slice mode */
	if((jpegIn.sliceMbSet == 0) &&
	   ((imageInfo.outputWidth * imageInfo.outputHeight) >
		JPEGDEC_MAX_PIXEL_AMOUNT))
	{
		do
		{
			jpegIn.sliceMbSet++;
		}
		while(((jpegIn.sliceMbSet * (mcuInRow / mcuSizeDivider)) +
			   (mcuInRow / mcuSizeDivider)) <
			  JPEGDEC_MAX_SLICE_SIZE_8190);
		LocalPrint(LOG_DEBUG,"jpeghw","Force to slice mode (over 16M) ==> Decoder Slice MB Set %d\n",(int)jpegIn.sliceMbSet);
	}

	jpegIn.decImageType = 0; // USE FULL MODEs
	printf("\n <%s>_%d\n", __func__, __LINE__);	
#ifdef DEBUG_HW_JPEG_TIME
	startTime = LocalGetSystemTime();
#endif
	/* Decode JPEG */
	do
	{
		LOGYC("\n <%s>_%d\n", __func__, __LINE__);	
		jpegRet = JpegDecDecode(jpegInst, &jpegIn, &jpegOut);
		LOGYC("\n <%s>_%d     jpegRet = %d \n", __func__, __LINE__, jpegRet);	
#if 0//DEBUG_HW_JPEG
		printDecPPRegs(((JpegDecContainer*)jpegInst)->jpegRegs);
#endif
		if( jpegRet == JPEGDEC_FRAME_READY)
		{
			LOGYC("\n <%s>_%d\n", __func__, __LINE__);	
			//VPUMemFlush(&ppOutMem);
#ifdef DEBUG_HW_JPEG_TIME
			stopTime = LocalGetSystemTime();
			LOGYC("just Jpeg Hardware Consumed Time : %d MS.\n", stopTime - startTime);
                //LOG(LOG_ERROR,"jpeghw","Don't contain memcpy, just Jpeg Hardware Consumed Time : %d MS.\n", stopTime - startTime);
#endif
			LOGYC("<%s>_%d outInfo->outAddr=%p ppOutMem.vir_addr=%p  ppOutMem.size = %d\n", __func__, __LINE__, outInfo->outAddr, ppOutMem.vir_addr, ppOutMem.size);
			VPUMemInvalidate(&ppOutMem);
			outInfo->outAddr = (char*)ppOutMem.vir_addr;
			//jpegInst = NULL;
			LOGYC("frame ready out:%d,%d  outInfo->outAddr = %p\n",ppScaleW,ppScaleH, outInfo->outAddr);
#if 0
                        FILE * testfs = fopen("./testfs.yuv", "wb");
                        char * testds = (char *)ppOutMem.vir_addr;
                        fwrite(testds, 1, ppOutMem.size, testfs);
                        fclose(testfs);
#endif
		}
		else if( jpegRet == JPEGDEC_SLICE_READY)
		{
			LOGYC("   <%s>_%d   JPEGDEC_SLICE_READY\n", __func__, __LINE__);
			/* move the decoded slice to be processed e.g. to the display
			controller */
			//processSlice(jpegOut.outputPictureY, jpegOut.outputPictureCbCr,	luma_size, chroma_size);
		}
		else if( jpegRet == JPEGDEC_STRM_PROCESSED)
		{
			LOGYC("   <%s>_%d   JPEGDEC_STRM_PROCESSED\n", __func__, __LINE__);
		//	VPUMemFlush(&streamMem);
		}
		else
		{
			LOGYC("DECODE ERROR, jpegRet: %d\n", jpegRet);
			/* Handle here the error situation */
			hw_jpeg_release(jpegInst);
			jpegInst = NULL;
			outInfo->decoderHandle = NULL;
		}
	}
	while(jpegRet != JPEGDEC_FRAME_READY && jpegInst != NULL);
#ifdef DEBUG_HW_JPEG_TIME
                        LOGYC("Jpeg Hardware Consumed Time(include VPUMemInvalidate) : %d MS.\n", LocalGetSystemTime() - startTime);
#endif
	CODEEND

	LOGYC("  <%s>_%d    jpegRet = %d\n", __func__, __LINE__, jpegRet);
	if(jpegRet < 0){
		if(inInfo->streamCtl.useThumb){
			outInfo->thumbPmem->thumbpmem = streamMem;
			if(getDataLength > 0){
				outInfo->thumbPmem->reuse = 1;
				streamMem.vir_addr = NULL;
			}
		}else if(retnBforWriteMei && isVpuMemSrc){
			(*src->get_vpumemInst)(inInfo,&streamMem);
			vpuMem.vir_addr = NULL;
			(*src->fill_buffer)(inInfo, streamMem.vir_addr, &vpuMem, imageInfo.displayWidth, imageInfo.displayHeight);
			//(*src->fill_buffer)(inInfo, streamMem.vir_addr);
			//VPUMemClean(&streamMem);
			streamMem.vir_addr = NULL;
		}
	}
	//JpegDecRelease(jpegInst);
	if(!isVpuMemSrc && streamMem.vir_addr){
		LOGYC("  <%s>_%d streamMem.vir_addr = %x\n", __func__, __LINE__, streamMem.vir_addr);
		//vpuMem release outside
		jpegIn.streamBuffer.pVirtualAddress = 0;
		jpegIn.streamBuffer.busAddress = 0;
		VPUFreeLinear(&streamMem);
	}
	LOGYC("\n <%s>_%d,   jpegRet = %d \n", __func__, __LINE__, jpegRet);	
	PrintRetYC(&jpegRet);
#ifdef DEBUG_HW_JPEG
	PrintRet(&jpegRet);
#endif
	return jpegRet;
}

int hw_jpeg_release(void *decInst)
{
	LOGYC("\n <%s>_%d \n", __func__, __LINE__);	
	JpegDecContainer *jpegC;
	/* Release JPEG Decoder , Auto Release pp output memory,too*/
	if(!decInst)
		return 0;
	jpegC = (JpegDecContainer*)decInst;
	
	if(jpegC->ppOutMem.vir_addr)
	{
		//LocalPrint(("ppOutMem Free.\n"));
		VPUFreeLinear(&(jpegC->ppOutMem));
	}
	JpegDecRelease((JpegDecInst)decInst);
	return 0;
}

int hw_jpeg_VPUMallocLinear(VPUMemLinear_t *p, int size){
	//VPUMemLinear_t* mem = (VPUMemLinear_t *)malloc(sizeof(VPUMemLinear_t));
	//if(mem == NULL){
	//	return -1;
	//}
	//int ret = 
	//if(ret){
	//	*p = mem;
	//} else {
	//	free(mem);
	//}
	return VPUMallocLinear(p, size) == DWL_OK;
}

int hw_jpeg_VPUFreeLinear(VPUMemLinear_t *p){
	int ret = VPUFreeLinear(p);
	p->vir_addr = NULL;
	//free(p);
	return ret;
}

//void hw_jpeg_VPUMemFlush(VPUMemLinear_t *p){
//	VPUMemClean(p);
//}

#ifdef DEBUG_HW_JPEG
int printDecPPRegs(RK_U32* regs){
	int i = 0;
	FILE* file = fopen("/home/ubuntu/dec_pp_regs.txt","w");
	if(file == NULL)
		return -1;
	for(; i < DEC_X170_REGISTERS + 41; i++){
		LOG("jpeghw","reg [ %d ]:  %08x\n",i,regs[i]);
		fprintf(file,"reg [ %d ]: 0x %08X\n",i,regs[i]);
	}
	fclose(file);
	return 0;
}
#endif
