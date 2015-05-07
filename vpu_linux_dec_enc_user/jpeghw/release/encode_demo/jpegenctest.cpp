#include <stdio.h>
#include <stdlib.h>
#include "vpu_mem.h"
#include "enccommon.h"
#include <utils/Log.h>
#include "hw_jpegenc.h"
//#include "SkImageDecoder.h"
#define TEST_SCALE 0
//#if TEST_SCALE
#include "swscale.h"
//#endif

VPUMemLinear_t *gMem = NULL;
typedef struct VpuApiDemoCmdContext {
    RK_U32  width;
    RK_U32  height;
    RK_U8   input_file[200];
    RK_U8   output_file[200];
    RK_U8   have_input;
    RK_U8   have_output;
    RK_U8   disable_debug;
    RK_U32  record_frames;
    RK_S64  record_start_ms;
}VpuApiDemoCmdContext_t;

typedef enum VPU_API_DEMO_RET {
    VPU_DEMO_OK = 0,
    VPU_DEMO_PARSE_HELP_OK  = 1,

    VPU_DEMO_ERROR_BASE     = -100,
    ERROR_INVALID_PARAM     = VPU_DEMO_ERROR_BASE - 1,
    ERROR_INVALID_STREAM    = VPU_DEMO_ERROR_BASE - 2,
    ERROR_IO                = VPU_DEMO_ERROR_BASE - 3,
    ERROR_MEMORY            = VPU_DEMO_ERROR_BASE - 4,
    ERROR_INIT_VPU          = VPU_DEMO_ERROR_BASE - 5,

    ERROR_VPU_DECODE        = VPU_DEMO_ERROR_BASE - 90,
} VPU_API_DEMO_RET;

typedef struct VpuApiCmd {
    RK_U8* name;
    RK_U8* argname;
    RK_U8* help;
}VpuApiCmd_t;

static VpuApiCmd_t vpuApiCmd[] = {
    {"i",               "input_file",           "input bitstream file"},
    {"o",               "output_file",          "output bitstream file, "},
    {"w",               "width",                "the width of input bitstream"},
    {"h",               "height",               "the height of input bitstream"},
    {"vframes",         "number",               "set the number of video frames to record"},
    {"ss",              "time_off",             "set the start time offset, use Ms as the unit."},
    {"d",               "disable",              "disable the debug output info."},
};

static RK_S32 show_help()
{
    printf("usage: vpu_apiDemo [options] input_file, \n\n");

    RK_S32 i =0;
    RK_U32 n = sizeof(vpuApiCmd)/sizeof(VpuApiCmd_t);
    for (i =0; i <n; i++) {
        printf("-%s  %s\t\t%s\n",
            vpuApiCmd[i].name, vpuApiCmd[i].argname, vpuApiCmd[i].help);
    }

    return 0;
}

static RK_S32 parse_options(int argc, char **argv, VpuApiDemoCmdContext_t* cmdCxt)
{
    char *opt;
    RK_U32 optindex, handleoptions = 1, ret =0;

    if ((argc <2)) {
        printf("vpu api demo, input parameter invalid\n");
        show_help();
        return ERROR_INVALID_PARAM;
    }

    /* parse options */
    optindex = 1;
    while (optindex < argc) {
        opt = argv[optindex++];

        if (handleoptions && opt[0] == '-' && opt[1] != '\0') {
            if (opt[1] == '-') {
                if (opt[2] != '\0') {
                    opt++;
                } else {
                     handleoptions = 0;
                    continue;
                }
            }

            opt++;

            switch (*opt) {
                case 'i':
                    if (argv[optindex]) {
                        memcpy(cmdCxt->input_file, argv[optindex], strlen(argv[optindex]));
                        cmdCxt->input_file[strlen(argv[optindex])] = '\0';
                        cmdCxt->have_input = 1;
                    } else {
                        printf("input file is invalid\n");
                        ret = -1;
                        goto PARSE_OPINIONS_OUT;
                    }
                    break;
                case 'o':
                    if (argv[optindex]) {
                        memcpy(cmdCxt->output_file, argv[optindex], strlen(argv[optindex]));
                        cmdCxt->output_file[strlen(argv[optindex])] = '\0';
                        cmdCxt->have_output = 1;
                        break;
                    } else {
                        printf("out file is invalid\n");
                        ret = -1;
                        goto PARSE_OPINIONS_OUT;
                    }
                case 'd':
                    cmdCxt->disable_debug = 1;
                    break;
                case 'w':
                    if (argv[optindex]) {
                        cmdCxt->width = atoi(argv[optindex]);
                        break;
                    } else {
                        printf("input width is invalid\n");
                        ret = -1;
                        goto PARSE_OPINIONS_OUT;
                    }
                case 'h':
                    if ((*(opt+1) != '\0') && !strncmp(opt, "help", 4)) {
                        show_help();
                        ret = VPU_DEMO_PARSE_HELP_OK;
                        goto PARSE_OPINIONS_OUT;
                    } else if (argv[optindex]) {
                        cmdCxt->height = atoi(argv[optindex]);
                    } else {
                        printf("input height is invalid\n");
                        ret = -1;
                        goto PARSE_OPINIONS_OUT;
                    }
                    break;
                default:
                        ret = -1;
                        goto PARSE_OPINIONS_OUT;
                    break;
            }

            optindex += ret;
        }
    }

PARSE_OPINIONS_OUT:
    if (ret <0) {
        printf("vpu api demo, input parameter invalid\n");
	show_help();
        return ERROR_INVALID_PARAM;
    }
    return ret;
}

int flush(int buf_type, int offset, int len){
	VPUMemFlush(gMem);
return 0;
}
/*放大：
	重视速度：fast_bilinear, point
	重视质量：cubic, spline, lanczos
  缩小：
    重视速度：fast_bilinear, point
	重视质量：gauss, bilinear
	重视锐度：cubic,spline,lanczos
*/
int main(int argc, char **argv){
	printf("  <%s>_%d \n", __func__, __LINE__);
	VpuApiDemoCmdContext_t demoCmdCtx;
	VpuApiDemoCmdContext_t* cmd = &demoCmdCtx;
	RK_S32 ret =0;
	if ((ret = parse_options(argc, argv, cmd)) !=0) {
        	if (ret == VPU_DEMO_PARSE_HELP_OK) {
        		return 0;
        	}
		printf("  <%s>_%d    error argv ..... \n", __func__, __LINE__);
		return 0;
	}
#if TEST_SCALE
	VPUMemLinear_t inMem;
	//VPUMemLinear_t outMem;
	int inputw = 2592;
	int inputh = 1944;
	int outw = 160;
	int outh = 128;
	FILE *yuvFile = NULL;
	int in_size = inputw*inputh*3;
	VPUMallocLinear(&inMem, in_size);
	yuvFile = fopen("/flash/yuv2592_1944.yuv","rb");
	fread(inMem.vir_addr,1,in_size,yuvFile);
	VPUMemFlush(&inMem);
	fclose(yuvFile);
	{
		int w = inputw;
		int h = inputh;
		uint8_t * src = (uint8_t*)inMem.vir_addr;
		uint8_t * yuvsrc[4] = {src, src+w*h, NULL, NULL};
		int yuvsrcstride[4] = {inputw, inputw, 0, 0};
		uint8_t * dst = (uint8_t*)malloc(outw*outh*3/2);
		uint8_t * yuvdst[4] = {dst, dst+outw*outh, NULL, NULL};
		int yuvdststride[4] = {outw, outw, 0, 0};
		struct SwsContext *sws = sws_getContext(w,h, PIX_FMT_NV12, outw, outh, PIX_FMT_NV12,
			SWS_FAST_BILINEAR, NULL, NULL, NULL);
		ALOGE("before sws_scale.");
		sws_scale(sws, yuvsrc, yuvsrcstride, 0, inputh, yuvdst, yuvdststride);
		ALOGE("after sws_scale.");
		sws_freeContext(sws);
		yuvFile = fopen("/flash/yuv160_128.yuv", "wb");
		fwrite(dst, 1, outw*outh*3/2, yuvFile);
		fclose(yuvFile);
		free(dst);
	}
	VPUFreeLinear(&inMem);	
#else
	int loopCounts = 0;
	JpegEncInInfo JpegInInfo;
	RkExifInfo exifInfo;
	RkGPSInfo gpsInfo;
	char* maker = "rockchip sb";
	char* model = "rockchip camera";
	JpegEncOutInfo JpegOutInfo;
	char *time = "2011:07:05 17:53:53";
	char* gpsprocessmethod = "GPS chuli ge mao a.";
	int jpegw = cmd->width;//1680;//2592;
	int jpegh = cmd->height;//1050;//1944;
	//int jpegw = 160;
	//int jpegh = 120;
	VPUMemLinear_t inMem;
	VPUMemLinear_t outMem;
	printf("  <%s>_%d   <%d, %d>\n", __func__, __LINE__, jpegw, jpegh);
VPUMemLinear_t tmp180Mem;
//#define RGB_DEBUG

#ifndef RGB_DEBUG
	int in_size = jpegw*jpegh*3/2+897;//yuv420
#else
	int in_size;
	int pixelBytes = 2;
	int in_rgb_size = in_size = jpegw*jpegh*pixelBytes;//xrgb888 rgb565
	int in_yuv420_size = jpegw*jpegh*3/2;//yuv420
#endif
	printf("  <%s>_%d \n", __func__, __LINE__);
	int out_size = jpegw*jpegh + 160 * 128*16;
	//void* thumbBuf = NULL;
	//FILE *tFile = NULL;
	int thumbfilelen = -1;
FILE *file = NULL;
FILE *yuvFile = NULL;
#ifdef RGB_DEBUG
char* yuvdata = (char*)malloc(in_yuv420_size);
	printf("  <%s>_%d \n", __func__, __LINE__);
if(yuvdata == NULL){
	ALOGE("fuck 1.");
}
	printf("  <%s>_%d \n", __func__, __LINE__);
VPUMallocLinear(&inMem, in_rgb_size);
#else
	printf("  <%s>_%d \n", __func__, __LINE__);
VPUMallocLinear(&inMem, in_size);
	printf("  <%s>_%d \n", __func__, __LINE__);
#endif
	VPUMallocLinear(&outMem, out_size);
	gMem = &outMem;

//yuvFile = fopen("/sdcard/1680_1050_yuv420sp.yuv","rb");
yuvFile = fopen(cmd->input_file,"rb");
//yuvFile = fopen("/flash/160_120_yuv420sp.yuv","rb");
#ifdef RGB_DEBUG//rgb
fread(yuvdata, 1, in_yuv420_size, yuvFile);
//memset(yuvdata, 0xf0, in_yuv420_size);
#if 1
{
	int w = jpegw;
        int h = jpegh;
        uint8_t * dst = (uint8_t*)inMem.vir_addr;
        uint8_t * yuvdst[4] = {dst, NULL, NULL, NULL};
        int yuvdststride[4] = {pixelBytes*w, 0, 0, 0};
        uint8_t * src = (uint8_t*)yuvdata;
        uint8_t * yuvsrc[4] = {src, src+w*h, NULL, NULL};
        int yuvsrcstride[4] = {w, w, 0, 0};
        struct SwsContext *sws = sws_getContext(w,h, PIX_FMT_NV12, w, h, PIX_FMT_RGB565,//PIX_FMT_RGB32
                  SWS_FAST_BILINEAR, NULL, NULL, NULL);
        ALOGE("before sws_scale1.");
        sws_scale(sws, yuvsrc, yuvsrcstride, 0, h, yuvdst, yuvdststride);
        ALOGE("after sws_scale1.");
        sws_freeContext(sws);
        //yuvFile = fopen("/flash/yuv160_128.yuv", "wb");
        //fwrite(dst, 1, outw*outh*3/2, yuvFile);
        //fclose(yuvFile);
        //free(dst);
}
free(yuvdata);
#else
free(yuvdata);
{
int i = 0;
unsigned char* st = (unsigned char*)inMem.vir_addr;
unsigned int c = 0xff;
int j = 0;
for(;i<jpegh;i++){
	for(j = 0; j<jpegw; j++){
		*st++ = 0;
		*st++ = 0;
		*st++ = 0xff;
		*st++ = 0;
	}
	//c += 0xff;
}
printf("color: 0x%x\n", *(unsigned int*)inMem.vir_addr);
}
#endif
#else
fread(inMem.vir_addr,1,in_size,yuvFile);
#endif
VPUMemFlush(&inMem);
fclose(yuvFile);

	exifInfo.maker = maker;
	exifInfo.makerchars = 12;
	exifInfo.modelstr = model;
	exifInfo.modelchars = 16;
	exifInfo.Orientation = 1;
	memcpy(exifInfo.DateTime, time, 20);
	exifInfo.ExposureTime.num = 400;
	exifInfo.ExposureTime.denom = 1;
	exifInfo.ApertureFNumber.num = 0x118;
	exifInfo.ApertureFNumber.denom = 0x64;
	exifInfo.ISOSpeedRatings = 0x59;
	exifInfo.CompressedBitsPerPixel.num = 0x4;
	exifInfo.CompressedBitsPerPixel.denom = 0x1;
	exifInfo.ShutterSpeedValue.num = 0x452;
	exifInfo.ShutterSpeedValue.denom = 0x100;
	exifInfo.ApertureValue.num = 0x2f8;
	exifInfo.ApertureValue.denom = 0x100;
	exifInfo.ExposureBiasValue.num = 0;
	exifInfo.ExposureBiasValue.denom = 0x100;
	exifInfo.MaxApertureValue.num = 0x02f8;
	exifInfo.MaxApertureValue.denom = 0x100;
	exifInfo.MeteringMode = 02;
	exifInfo.Flash = 0x10;
	exifInfo.FocalLength.num = 0x4;
	exifInfo.FocalLength.denom = 0x1;
	exifInfo.FocalPlaneXResolution.num = 0x8383;
	exifInfo.FocalPlaneXResolution.denom = 0x67;
	exifInfo.FocalPlaneYResolution.num = 0x7878;
	exifInfo.FocalPlaneYResolution.denom = 0x76;
	exifInfo.SensingMethod = 2;
	exifInfo.FileSource = 3;
	exifInfo.CustomRendered = 1;
	exifInfo.ExposureMode = 0;
	exifInfo.WhiteBalance = 0;
	exifInfo.DigitalZoomRatio.num = jpegw;
	exifInfo.DigitalZoomRatio.denom = jpegw;
	exifInfo.SceneCaptureType = 0x01;

	gpsInfo.GPSLatitudeRef[0] = 'N';
	gpsInfo.GPSLatitudeRef[1] = '\0';
	gpsInfo.GPSLatitude[0].num = 0x77;
	gpsInfo.GPSLatitude[0].denom = 1;
	gpsInfo.GPSLatitude[1].num = 0x66;
	gpsInfo.GPSLatitude[1].denom = 1;
	gpsInfo.GPSLatitude[2].num = 0x55;
	gpsInfo.GPSLatitude[2].denom = 1;
	gpsInfo.GPSLongitudeRef[0] = 'E';
	gpsInfo.GPSLongitudeRef[1] = '\0';
	gpsInfo.GPSLongitude[0].num = 0x44;
	gpsInfo.GPSLongitude[0].denom = 1;
	gpsInfo.GPSLongitude[1].num = 0x33;
	gpsInfo.GPSLongitude[1].denom = 1;
	gpsInfo.GPSLongitude[2].num = 0x22;
	gpsInfo.GPSLongitude[2].denom = 1;
	gpsInfo.GPSAltitudeRef = 1;
	gpsInfo.GPSAltitude.num = 0x88;
	gpsInfo.GPSAltitude.denom = 0x1;
	gpsInfo.GpsTimeStamp[0].num = 18;
	gpsInfo.GpsTimeStamp[0].denom = 1;
	gpsInfo.GpsTimeStamp[1].num = 36;
	gpsInfo.GpsTimeStamp[1].denom = 1;
	gpsInfo.GpsTimeStamp[2].num = 48;
	gpsInfo.GpsTimeStamp[2].denom = 1;
	memcpy(gpsInfo.GpsDateStamp,"2011:07:05",11);//"YYYY:MM:DD\0"
	gpsInfo.GPSProcessingMethod = gpsprocessmethod;
	gpsInfo.GpsProcessingMethodchars = 20;//length of GpsProcessingMethod

	JpegInInfo.frameHeader = 1;
	JpegInInfo.rotateDegree = DEGREE_0;
	JpegInInfo.y_rgb_addr = inMem.phy_addr;
#ifndef RGB_DEBUG
	JpegInInfo.uv_addr = inMem.phy_addr + (jpegw*jpegh);
#else
	JpegInInfo.uv_addr = 0;
#endif
	JpegInInfo.y_vir_addr = (unsigned char*)inMem.vir_addr;
#ifndef RGB_DEBUG
	JpegInInfo.uv_vir_addr = ((unsigned char*)inMem.vir_addr + jpegw*jpegh);
#else
	JpegInInfo.uv_vir_addr = 0;
#endif
	if(JpegInInfo.rotateDegree == DEGREE_180){
		VPUMallocLinear(&tmp180Mem, in_size);
		JpegInInfo.yuvaddrfor180 = tmp180Mem.phy_addr;
	}else{
		JpegInInfo.yuvaddrfor180 = NULL;
	}
	JpegInInfo.inputW = jpegw;
	JpegInInfo.inputH = jpegh;
#ifdef RGB_DEBUG
	JpegInInfo.type = HWJPEGENC_RGB565;//HWJPEGENC_RGB888;
#else
	JpegInInfo.type = JPEGENC_YUV420_SP;
#endif
	JpegInInfo.qLvl = 9;
//tFile = fopen("/flash/thumbtest.jpg","rb");
//fseek(tFile, SEEK_SET, SEEK_END);
//thumbfilelen = ftell(tFile);//bu chao guo 2G
//thumbBuf = malloc(thumbfilelen);
//fseek(tFile, SEEK_SET, SEEK_SET);
//fread(thumbBuf,1, thumbfilelen, tFile);
//fclose(tFile);
#ifdef RGB_DEBUG
	JpegInInfo.doThumbNail = 1;
#else
	JpegInInfo.doThumbNail = 0;//insert thumbnail at APP0 extension if motionjpeg, else at APP1 extension
#endif
	JpegInInfo.thumbData = NULL;//if thumbData is NULL, do scale, the type above can not be 420_P or 422_UYVY
	JpegInInfo.thumbDataLen = -1;
	JpegInInfo.thumbW = 160;
	JpegInInfo.thumbH = 128;//128;
	JpegInInfo.thumbqLvl = 5;
	JpegInInfo.exifInfo = &exifInfo;
	JpegInInfo.gpsInfo = &gpsInfo;

	JpegOutInfo.outBufPhyAddr = outMem.phy_addr;
	JpegOutInfo.outBufVirAddr = (unsigned char*)outMem.vir_addr;
	JpegOutInfo.outBuflen = out_size;
	JpegOutInfo.jpegFileLen = 0;
	JpegOutInfo.cacheflush = flush;
ALOGE("outbuflen: %d", out_size);
static char str[32] = {'\0'};
for(loopCounts = 0; loopCounts < 1; loopCounts++){
	if(hw_jpeg_encode(&JpegInInfo, &JpegOutInfo) < 0 || JpegOutInfo.jpegFileLen <= 0){
		ALOGE("hw jpeg encode fail.");
	}else{
ALOGD("final file offset: %d, size: %d", JpegOutInfo.finalOffset, JpegOutInfo.jpegFileLen);
		//VPUMemFlush(&outMem);
		//SkBitmap bitmap;
		//SkImageDecoder::DecodeMemory(JpegOutInfo.outBufVirAddr + JpegOutInfo.finalOffset, JpegOutInfo.jpegFileLen, &bitmap,
                //             SkBitmap::kARGB_8888_Config, SkImageDecoder::kDecodePixels_Mode);
		//ALOGE("decode complete. %p", bitmap.getPixels());
		sprintf(str, cmd->output_file, loopCounts);
		file = fopen(str,"wb");
		fwrite(JpegOutInfo.outBufVirAddr + JpegOutInfo.finalOffset, 1, JpegOutInfo.jpegFileLen, file);
		fclose(file);
		//ALOGE();
	}
}
	//free(thumbBuf);
	VPUFreeLinear(&inMem);
	VPUFreeLinear(&outMem);
	if(JpegInInfo.rotateDegree == DEGREE_180){
		VPUFreeLinear(&tmp180Mem);
	}
#endif
	return 0;
}
