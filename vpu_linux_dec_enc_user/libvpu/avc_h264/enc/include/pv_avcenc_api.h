#ifndef _PV_AVCENC_API_H_
#define _PV_AVCENC_API_H_

#include "H264TestBench.h"
#include "H264Instance.h"
#include "vpu_mem.h"
//#define ENC_DEBUG

typedef int     int32_t;
typedef unsigned int uint32_t;
typedef int status_t;
typedef unsigned char uint8_t;

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
   int  format;
   int	intraPicRate;
   int  framerateout;
   int  profileIdc;
   int  levelIdc;
   int	reserved[3];
}AVCEncParams1;


class On2_AvcEncoder
{
    public:
         On2_AvcEncoder();
        ~On2_AvcEncoder() { };
        int pv_on2avcencoder_init(AVCEncParams1 *aEncOption, uint8_t* aOutBuffer, uint32_t* aOutputLength);
        int pv_on2avcencoder_oneframe(uint8_t* aOutBuffer, uint32_t*   aOutputLength,
        uint8_t *aInBuffer,uint32_t aInBuffPhy,uint32_t*   aInBufSize,uint32_t* aOutTimeStamp, bool*  aSyncFlag );
        void pv_on2avcencoder_setconfig(AVCEncParams1* vpug);
        void pv_on2avcencoder_getconfig(AVCEncParams1* vpug);
	int H264encSetInputFormat(H264EncPictureType inputFormat);
	void H264encSetintraPeriodCnt();
	void H264encSetInputAddr(unsigned long input);
        int pv_on2avcencoder_deinit();
	void h264encSetmbperslice(int line_per_slice);
	int h264encFramerateScale(int inframerate, int outframerate);
    private:
		int AllocRes( );
        void FreeRes();
    private:

        VPUMemLinear_t pictureMem ;
        VPUMemLinear_t pictureStabMem;
        VPUMemLinear_t outbufMem;
//      H264EncInst encoder;
        commandLine_s cmdl;
        H264EncIn encIn;
        H264EncOut encOut;
        H264EncRateCtrl rc;
        h264Instance_s h264encInst;
        int intraPeriodCnt, codedFrameCnt , next , src_img_size;
        u32 frameCnt;
        i32 InframeCnt;
		i32	preframnum;
        u32 streamSize;
        u32 bitrate;
        u32 psnrSum;
        u32 psnrCnt;
        u32 psnr;
#ifdef ENC_DEBUG
        FILE *fp;
        FILE *fp1;
#endif
};
#endif

