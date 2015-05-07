//============================================================================
// Name        : rk_jpeg.cpp
// Author      : Simon Cheung
// Version     :
// Copyright   : zhangximin@gmail.com 2014~2016
// Description :
//============================================================================

#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <SkStream.h>
//#include "hw_jpegdecapi.h"
#include "SkHwJpegUtility.h"
//#include "sxun_hwjpeg_decode.h"
#include <ion.h>
#include <rockchip_ion.h>

#ifdef __cplusplus
extern "C"
{
#endif

int hwjpeg_decoder(char* data,char * data_out, int size, int loff,int toff,int width,int height) {
	int out_size;

	printf("   <%s>_%d \n", __func__, __LINE__);
	SkMemoryStream stream(data, size, false);
	//SkAutoTUnref<SkMemoryStream> stream(new SkMemoryStream(data,size,false));
	HwJpegInputInfo hwInfo;
	//memset(hwInfo,0,sizeof(hwInfo));
	sk_hw_jpeg_source_mgr sk_hw_stream(&stream, &hwInfo, false);

	hwInfo.justcaloutwh = 0;
	hwInfo.streamCtl.inStream = &sk_hw_stream;
	hwInfo.streamCtl.wholeStreamLength = stream.getLength(); // > 64M? break;
	hwInfo.streamCtl.thumbOffset = -1;
	hwInfo.streamCtl.thumbLength = -1;
	hwInfo.streamCtl.useThumb = 0;

	ReusePmem thumbPmem;
	thumbPmem.reuse = 0;

	HwJpegOutputInfo outInfo;
	//memset(hwInfo,0,sizeof(outInfo));
	outInfo.thumbPmem = &thumbPmem;
	outInfo.outAddr = NULL;

	PostProcessInfo * ppInfo = &hwInfo.ppInfo;
	ppInfo->outFomart = 1;
	ppInfo->shouldDither = 0;
	ppInfo->scale_denom = 1;
	ppInfo->cropX = 0;
	ppInfo->cropY = 0;
	ppInfo->cropW = -1;
	ppInfo->cropH = -1;

	int ret = -1;
	char reuseBitmap = 0;
	printf("   <%s>_%d \n", __func__, __LINE__);
//    printf("PSurface HWjpeg00_decode start width:%d, height:%d, stride:%d ", width, height, stride);
	if ((ret = hw_jpeg_decode(&hwInfo, &outInfo, &reuseBitmap, width, height))
			>= 0) {
#if 1
		FILE * testfs = fopen("./testfffffs.yuv", "wb");
		char * testds = (char *)outInfo.outAddr;
		fwrite(testds, 1, width * height * 4, testfs);
		fclose(testfs);
#endif

		///////////////////////////////////////////////////////////////////////////
		memcpy(data_out, (char *) outInfo.outAddr, width * height * 4);

		//   printf("hw_jpeg_decode is ok\n");
		hw_jpeg_release(outInfo.decoderHandle);
		outInfo.decoderHandle = NULL;
		//stream = NULL;
		printf("%p\n", outInfo.outAddr);
		usleep(100);
		return true;
	}
	hw_jpeg_release(outInfo.decoderHandle);
	outInfo.decoderHandle = NULL;
	//stream = NULL;

	//  printf("hw_jpeg_decode is error error\n");
	// printf("hw_jpeg_decode error outInfo.outAddr=================%p\n",outInfo.outAddr);

	return false;
}

#ifdef __cplusplus
}
#endif
