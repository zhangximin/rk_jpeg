
#define ALOG_TAG "AvcDecapi"
#include <utils/Log.h>

#include "dwl.h"

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
#include "h264hwd_container.h"
#include "h264decapi.h"
#include "pv_avcdec_api.h"
#include "regdrv.h"

#include "vpu.h"
#include "vpu_mem.h"

namespace android {

#define DEFAULT_STREAM_SIZE     1024*256


typedef enum CODEC_CMD {
    GET_STREAM_COUNT,
    GET_FRAME_COUNT,
    SET_DPB_FULL_OUTPUT,
    SET_MVC_DISABLE,
} CODEC_CMD;

On2_AvcDecoder::On2_AvcDecoder()
	: isFlashPlayer(true),
	  status(NO_INIT),
	  streamSize(0),
	  streamMem(NULL),
	  H264deccont(NULL) {
    //Mutex::Autolock autoLock(mLock);
	do {
        streamMem = (VPUMemLinear_t *)malloc(sizeof(VPUMemLinear_t));
        memset(streamMem,0,sizeof(VPUMemLinear_t));
        if (NULL == streamMem) {
            ALOGE("On2_AvcDecoder malloc streamMem failed");
            break;
        }
        H264deccont = (decContainer_t *)malloc(sizeof(decContainer_t));
        if (NULL == H264deccont) {
            ALOGE("On2_AvcDecoder malloc decContainer_t failed");
            break;
        }
        status = NO_ERROR;
	} while (0);

    if (status) {
        if (streamMem)   {free(streamMem);   streamMem   = NULL;}
        if (H264deccont) {free(H264deccont); H264deccont = NULL;}
    }
};

On2_AvcDecoder::~On2_AvcDecoder()
{
    //Mutex::Autolock autoLock(mLock);
    //ALOGE("free streamMem: 0x%.8x\n", streamMem);
    if (streamMem)   {free(streamMem);}//   streamMem   = NULL;}
    //ALOGE("free H264deccont: 0x%.8x\n", H264deccont);
    if (H264deccont) {free(H264deccont);}// H264deccont = NULL;}
    //status = NO_INIT;
};

int32_t On2_AvcDecoder::pv_on2avcdecoder_init(uint32_t ts_en)
{
    //Mutex::Autolock autoLock(mLock);
    H264DecRet ret;

    if (status) {
        ALOGE("pv_on2avcdecoder_init but status is %d\n", status);
        return status;
    }

//    DispbuffMalloc(640, 480);   //分配输出显示的buffer

    isFlashPlayer = 1;
#ifdef FLASH_DROP_FRAME
	dropThreshold = 0;
	curFrameNum = 0;
#endif

    ret = H264DecInit(H264deccont, 0);
    if (ret != H264DEC_OK)
    {
        ALOGE("H264DecInit failed ret %d\n", ret);
        pv_on2avcdecoder_deinit();
        return status = INVALID_OPERATION;
    }

#ifdef STREAM_DEBUG
    fpStream = fopen("/data/log/in.h264", "w+b");
	if(fpStream == NULL) {
		ALOGV("fopen rm.out fail\n");
	}
#endif
#ifdef OUTPUT_DEBUG
    fpOut = fopen("/data/tmp/out.yuv", "w+b");
    if (NULL == fpOut) {
        ALOGE("fpOut open /data/tmp/out.yuv failed\n");
    }
#endif
#ifdef PTS_DEBUG
    pts_in = fopen("/data/log/pts_in.txt", "w+b");
    if (NULL == pts_in) {
        ALOGE("pts_in  open failed\n");
    }
    pts_out = fopen("/data/log/pts_out.txt", "w+b");
    if (NULL == pts_out) {
        ALOGE("pts_out open failed\n");
    }
#endif

    SetDecRegister(H264deccont->h264Regs, HWIF_DEC_LATENCY,      DEC_X170_LATENCY_COMPENSATION);
    //clock_gating = DEC_X170_INTERNAL_CLOCK_GATING;  //控制IP上的clock, 0:clock始终打开; 1:由clock gating module控制时钟的开关,当解码器闲置的时候clock会被关掉.
    SetDecRegister(H264deccont->h264Regs, HWIF_DEC_CLK_GATE_E,   DEC_X170_INTERNAL_CLOCK_GATING);
    SetDecRegister(H264deccont->h264Regs, HWIF_DEC_OUT_TILED_E,  DEC_X170_OUTPUT_FORMAT);
    //output_picture_endian = 1;                      //1:little endian; 0:big endian
    SetDecRegister(H264deccont->h264Regs, HWIF_DEC_OUT_ENDIAN,   DEC_X170_OUTPUT_PICTURE_ENDIAN);
    //bus_burst_length = 16;                          //bus burst
    SetDecRegister(H264deccont->h264Regs, HWIF_DEC_MAX_BURST,    DEC_X170_BUS_BURST_LENGTH);
    SetDecRegister(H264deccont->h264Regs, HWIF_DEC_DATA_DISC_E,  DEC_X170_DATA_DISCARD_ENABLE);

	H264deccont->ts_en = ts_en;

    ALOGI("init done status %x\n", status);
    return NO_ERROR;
}

static struct timeval start, end;

int32_t On2_AvcDecoder::getOneFrame(uint8_t* aOutBuffer, uint32_t *aOutputLength, int64_t& InputTimestamp)
{
    H264DecPicture decPicture;

    if (status)
        return status;

    if (H264DecNextPicture(H264deccont, &decPicture) == H264DEC_PIC_RDY)
    {
#ifdef PTS_DEBUG
        if (pts_out) {
            fprintf(pts_out, "pts out : high %10d low %10d\n", decPicture.TimeHigh, decPicture.TimeLow);
            fflush(pts_out);
        }
        ALOGE("pts out : high %10d low %10d\n", decPicture.TimeHigh, decPicture.TimeLow);
#endif
        if (!isFlashPlayer)
        {
            VPU_FRAME frame;
            u32 hor_stride = decPicture.picWidth;
            u32 ver_stride = decPicture.picHeight;
            u32 width  = decPicture.cropParams.cropOutWidth;
            u32 height = decPicture.cropParams.cropOutHeight;
            frame.FrameBusAddr[0] = decPicture.outputPictureBusAddress;
            frame.FrameBusAddr[1] = decPicture.outputPictureBusAddress + hor_stride * ver_stride;
            frame.FrameWidth    = hor_stride;
            frame.FrameHeight   = ver_stride;
            frame.OutputWidth   = hor_stride;
            frame.OutputHeight  = ver_stride;
            frame.DisplayWidth  = width;
            frame.DisplayHeight = height;
            //ALOGE("width %d height %d h_stride %d v_stride %d", width, height, hor_stride, ver_stride);
            frame.FrameType     = decPicture.interlaced;
            frame.DecodeFrmNum  = decPicture.picId;
            frame.ShowTime.TimeLow = decPicture.TimeLow;
            frame.ShowTime.TimeHigh= decPicture.TimeHigh;
            frame.ErrorInfo     = decPicture.nbrOfErrMBs;
            InputTimestamp = (int64_t)decPicture.TimeHigh;
            InputTimestamp = (InputTimestamp << 32) + (int64_t)decPicture.TimeLow;
            frame.vpumem = decPicture.lineardata;
#ifdef OUTPUT_DEBUG
            if(fpOut) {
                VPUMemLink(&decPicture.lineardata);
                uint8_t *YAddr  = (uint8_t *)decPicture.lineardata.vir_addr;
                uint32_t YSize =  decPicture.picWidth* decPicture.picHeight;
                uint8_t *CAddr = YAddr + YSize;
                VPUMemInvalidate(&decPicture.lineardata);
                int count = fwrite(YAddr, 1, YSize*3/2, fpOut);
                fflush(fpOut);
                ALOGI("writing addr %p size %d count %d", YAddr, YSize, count);
                VPUMemDuplicate(&frame.vpumem, &decPicture.lineardata);
                VPUFreeLinear(&decPicture.lineardata);
            }
#endif
            memcpy(aOutBuffer,&frame,sizeof(VPU_FRAME));
            *aOutputLength = sizeof(VPU_FRAME);
            //ALOGE("frame.FrameBusAddr[0] = 0x%x\n",frame.FrameBusAddr[0]);
        }
        else
        {
//#define YUV_SEMIPLANAR
            VPUMemLink(&decPicture.lineardata);
            VPUMemInvalidate(&decPicture.lineardata);

            uint8_t *YAddr  = (uint8_t *)decPicture.lineardata.vir_addr;
            uint32_t YSize =  decPicture.picWidth* decPicture.picHeight;
            uint8_t *CAddr = YAddr + YSize;

#ifdef OUTPUT_DEBUG
            if(fpOut) {
                fwrite(YAddr, 1, YSize*3/2, fpOut);
                fflush(fpOut);
            }
#endif

#ifdef FLASH_DROP_FRAME

			if(dropThreshold == 0)
			{
				if(YSize > 640*480)
					dropThreshold = 4;
				else if(YSize > 480*320)
					dropThreshold = 5;
				else
					dropThreshold = 6;
				curFrameNum = 0;
			}

			if(curFrameNum != dropThreshold)
			{
#endif
            if (decPicture.cropParams.croppingFlag) {
                u32 cropLeftOffset = decPicture.cropParams.cropLeftOffset;
                u32 cropTopOffset  = decPicture.cropParams.cropTopOffset;
                u32 cropOutWidth   = decPicture.cropParams.cropOutWidth;
                u32 cropOutHeight  = decPicture.cropParams.cropOutHeight;
				u32 headOffset     = cropTopOffset*decPicture.picWidth;
				if(isFlashPlayer == 11)
					cropOutWidth = decPicture.picWidth;

                if (cropLeftOffset == 0 && cropOutWidth == decPicture.picWidth) {
                    uint32_t picLen = decPicture.picWidth*cropOutHeight;
                    memcpy(aOutBuffer,YAddr+headOffset ,picLen);
#ifdef YUV_SEMIPLANAR
                    memcpy(aOutBuffer+picLen,CAddr+headOffset/2,picLen/2);
#else
					uint32_t i;
					uint8_t *tmp = CAddr+headOffset/2;
					aOutBuffer+=picLen;
					for (i = 0; i < picLen/4; i++, tmp+=2) {
						*aOutBuffer++ = *tmp;
					}
					tmp = CAddr+headOffset/2+1;
					for (i = 0; i < picLen/4; i++, tmp+=2) {
						*aOutBuffer++ = *tmp;
					}
#endif
                } else {
                    uint8_t *start = YAddr+headOffset+cropLeftOffset;
					uint32_t y;
                    for (y = 0; y < cropOutHeight; y++,start+=decPicture.picWidth,aOutBuffer+=cropOutWidth) {
                        memcpy(aOutBuffer,start,cropOutWidth);
                    }
#ifdef YUV_SEMIPLANAR
                    start = CAddr+headOffset/2+cropLeftOffset;
                    for (y = 0; y < cropOutHeight/2; y++, start+=decPicture.picWidth,aOutBuffer+=cropOutWidth) {
                        memcpy(aOutBuffer,start,cropOutWidth);
                    }
#else
					uint32_t i, x;
					uint8_t *tmp = CAddr+headOffset/2+cropLeftOffset;
                    for (y = 0; y < cropOutHeight/2; y++, tmp+=decPicture.picWidth) {
						for (i=0; i<cropOutWidth/2;i++)
                        	*aOutBuffer++ = tmp[2*i];
                    }
					tmp = CAddr+headOffset/2+cropLeftOffset;
                    for (y = 0; y < cropOutHeight/2; y++, tmp+=decPicture.picWidth) {
						for (i=0; i<cropOutWidth/2;i++)
                        *aOutBuffer++ = tmp[2*i+1];
                    }
#endif
                }
            } else {
                memcpy(aOutBuffer,YAddr,YSize);
#ifdef YUV_SEMIPLANAR
                memcpy(aOutBuffer+YSize,CAddr,YSize/2);
#else
				uint32_t i;
				uint8_t *tmp = CAddr;
				aOutBuffer+=YSize;
				for (i = 0; i < YSize/4; i++, tmp+=2) {
					*aOutBuffer++ = *tmp;
				}
				tmp = CAddr+1;
				for (i = 0; i < YSize/4; i++, tmp+=2) {
					*aOutBuffer++ = *tmp;
				}
#endif
            }
#ifdef FLASH_DROP_FRAME
				curFrameNum++;
			}
			else
			{
				curFrameNum = 0;
				YSize = 0;
			}
#endif
            InputTimestamp = (int64_t)decPicture.TimeHigh;
            InputTimestamp = (InputTimestamp<<32) + decPicture.TimeLow;
            *aOutputLength = YSize;
            VPUFreeLinear(&decPicture.lineardata);
        }
        return 0;
    }
    return -1;
}

int32_t On2_AvcDecoder::prepareStream(uint8_t* aInputBuf, uint32_t aInBufSize)
{
    if (status)
        return status;

    if (NULL == streamMem) {
        ALOGE("streamMem is NULL\n");
        return status = NO_MEMORY;
    }

    if (0 == streamSize) {
        if (VPUMallocLinear(streamMem, DEFAULT_STREAM_SIZE))
        {
            ALOGE("VPUMallocLinear streamMem fail\n");
            return status = NO_MEMORY;
        }
        streamSize = DEFAULT_STREAM_SIZE;
    }

    if ((aInBufSize+1024) > streamSize) {
        VPUFreeLinear(streamMem);
        if(VPUMallocLinear(streamMem, (aInBufSize+1024))) {
            ALOGE("VPUMallocLinear realloc fail \n");
            return status = NO_MEMORY;
        }
        streamSize = aInBufSize;
    }

    memcpy((void *)streamMem->vir_addr, aInputBuf, aInBufSize);
	memset(((uint8_t *)streamMem->vir_addr) + aInBufSize, 0, 1024);
    if (VPUMemClean(streamMem)) {
        ALOGE("avc VPUMemClean error");
        return status = INVALID_OPERATION;
    }

#ifdef STREAM_DEBUG
    if(fpStream) {
        fwrite(streamMem->vir_addr, 1, aInBufSize, fpStream);
        fflush(fpStream);
    }
#endif

    return 0;
}

int32_t On2_AvcDecoder::pv_on2avcdecoder_oneframe(uint8_t* aOutBuffer, uint32_t *aOutputLength,
        uint8_t* aInputBuf, uint32_t* aInBufSize, int64_t& InputTimestamp)
{
    //Mutex::Autolock autoLock(mLock);
    H264DecRet ret;
    H264DecInput decInput;
    H264DecOutput decOutput;
    u32 error_count = 0;

	printf("pv_on2avcdecoder_oneframe pv_on2avcdecoder_oneframe %d , status = %d\n", *aInBufSize+8, status);

    if (status) {
        ALOGI("pv_on2avcdecoder_oneframe return status %d\n", status);
        return status;
    }

	isFlashPlayer = *aOutputLength;
    *aOutputLength = 0;

    if (NULL != H264deccont->storage.dpb)
    {
        if (!isFlashPlayer && H264deccont->storage.dpb->numOut)
        {
            //ALOGE("try getOneFrame\n");
            if (NO_ERROR == getOneFrame(aOutBuffer, aOutputLength, InputTimestamp)){
		PRINTFLINE
                return NO_ERROR;
	    }
            //ALOGE("try getOneFrame failed go on\n");
        }
    }

	//gettimeofday(&start, NULL);
    //ALOGE("pv_on2avcdecoder_oneframe start %lu ms", start.tv_sec * 1000 + start.tv_usec / 1000);

    if (prepareStream(aInputBuf, *aInBufSize)) {
        ALOGE("prepareStream failed ret %d\n", status);
        return -1;
    }

    PRINTFLINE

    decInput.skipNonReference   = 0;
    decInput.streamBusAddress   = (u32)streamMem->phy_addr;
	decInput.pStream            = (u8*)streamMem->vir_addr;
    decInput.dataLen            = *aInBufSize;
    decInput.TimeLow            = (uint32_t)(InputTimestamp);
    decInput.TimeHigh           = (uint32_t)(InputTimestamp>>32);
    decInput.picId              = 0;
    //decInput.isFlashPlayer      = isFlashPlayer;
#ifdef PTS_DEBUG
    if (pts_in) {
        fprintf(pts_in, "pts in  : high %10d low %10d\n", decInput.TimeHigh, decInput.TimeLow);
        fflush(pts_in);
    }
    ALOGE("pts in  : high %10d low %10d\n", decInput.TimeHigh, decInput.TimeLow);
#endif

#ifdef PTS_DEBUG

#endif

#define ERR_RET(err) do { ret = err; pv_on2avcdecoder_rest(); goto RET; } while (0)
#define ERR_CONTINUE(err) do { pv_on2avcdecoder_rest(); error_count++; } while (0)
    /* main decoding loop */
    do
    {
        /* Picture ID is the picture number in decoding order */
        /* call API function to perform decoding */
        ret = H264DecDecode(H264deccont, &decInput, &decOutput);

        printf("H264DecDecode decOutput.dataLeft %d ret %d\n", decOutput.dataLeft, ret);
        *aInBufSize = decOutput.dataLeft;
        printf("aInBufSize out : %d \n", *aInBufSize);
	PRINTFLINE
        switch (ret) {
        case H264DEC_PIC_DECODED:
        case H264DEC_PENDING_FLUSH: {
            getOneFrame(aOutBuffer, aOutputLength, InputTimestamp);
        } break;

        case H264DEC_STREAM_NOT_SUPPORTED: {
            ALOGE("ERROR: UNSUPPORTED STREAM!\n");
            //pv_on2avcdecoder_deinit();
            ERR_CONTINUE(-3);
        } break;

        case H264DEC_SIZE_TOO_LARGE: {
            ALOGE("ERROR: not support too large frame!\n");
            //ERR_RET(H264DEC_SIZE_TOO_LARGE);
            ERR_RET(H264DEC_PARAM_ERROR);
        } break;

        case H264DEC_HDRS_RDY: {
            H264DecInfo decInfo;
            /* Stream headers were successfully decoded
             * -> stream information is available for query now */

            if (H264DEC_OK != H264DecGetInfo(H264deccont, &decInfo))
            {
                ALOGE("ERROR in getting stream info!\n");
                //pv_on2avcdecoder_deinit();
                ERR_RET(H264DEC_PARAM_ERROR);
            }

            ALOGI("Width %d Height %d\n", decInfo.picWidth, decInfo.picHeight);
            ALOGI("videoRange %d, matrixCoefficients %d\n",
                         decInfo.videoRange, decInfo.matrixCoefficients);
        } break;

        case H264DEC_ADVANCED_TOOLS: {
            assert(decOutput.dataLeft);
            ALOGE("avc stream using advanced tools not supported\n");
            ERR_CONTINUE(-2);
            //ERR_RET(-1); //jmj ,wireless bitstream error,
        } break;

        case H264DEC_OK:
            /* nothing to do, just call again */
        case H264DEC_STRM_PROCESSED:
        case H264DEC_NONREF_PIC_SKIPPED:
        case H264DEC_STRM_ERROR: {
        } break;

        case H264DEC_NUMSLICE_ERROR: {
            error_count++;
        } break;

        case H264DEC_HW_TIMEOUT: {
            ALOGE("Timeout\n");
            ERR_RET(H264DEC_HW_TIMEOUT);
        } break;

        default: {
            ALOGE("\n  <%s>_%d  FATAL ERROR: %d\n", ret);
            ERR_RET(H264DEC_PARAM_ERROR);
        } break;
        }

        /* break out of do-while if maxNumPics reached (dataLen set to 0) */
        if ((decInput.dataLen == 0) || (error_count >= 2))
        {
            if (error_count)
                ALOGE("error_count %d\n", error_count);
            decInput.pStream += (decInput.dataLen - decOutput.dataLeft);
            decInput.streamBusAddress += (decInput.dataLen - decOutput.dataLeft);
            decInput.dataLen = decOutput.dataLeft;
            break;
        }

        if ((ret == H264DEC_HDRS_RDY) || decOutput.dataLeft)
        {
            decInput.pStream += (decInput.dataLen - decOutput.dataLeft);
            decInput.streamBusAddress += (decInput.dataLen - decOutput.dataLeft);
            decInput.dataLen = decOutput.dataLeft;
            ALOGV("(ret == H264DEC_HDRS_RDY) decInput.dataLen %d\n", decInput.dataLen);
        }

        /* keep decoding until if decoder only get its header */
    } while((ret == H264DEC_HDRS_RDY) || (ret == H264DEC_NUMSLICE_ERROR) || (decOutput.dataLeft >10));
RET:
	//gettimeofday(&end, NULL);
    //ALOGE("pv_on2avcdecoder_oneframe end %lu ms", end.tv_sec * 1000 + end.tv_usec / 1000);
    //ALOGE("pv_on2avcdecoder_oneframe %ld ms", (end.tv_sec - start.tv_sec) * 1000 +  (end.tv_usec - start.tv_usec) / 1000);
    *aInBufSize = 0;
    return ret;
}

int32_t On2_AvcDecoder::pv_on2avc_flushOneFrameInDpb(uint8_t* aOutBuffer, uint32_t *aOutputLength)
{
//    Mutex::Autolock autoLock(mLock);
    H264DecPicture decPicture;

    if ((aOutBuffer ==NULL) || (aOutputLength ==NULL)) {
        return -1;
    }
    if (status) {
        return -1;
    }

    H264DecReset(H264deccont);

    if (H264DecNextPicture(H264deccont, &decPicture) == H264DEC_PIC_RDY)
    {
        VPU_FRAME frame;
        u32 hor_stride = decPicture.picWidth;
        u32 ver_stride = decPicture.picHeight;
        u32 width  = decPicture.cropParams.cropOutWidth;
        u32 height = decPicture.cropParams.cropOutHeight;
        frame.FrameBusAddr[0] = decPicture.outputPictureBusAddress;
        frame.FrameBusAddr[1] = decPicture.outputPictureBusAddress + hor_stride * ver_stride;
        frame.FrameWidth    = hor_stride;
        frame.FrameHeight   = ver_stride;
        frame.OutputWidth   = hor_stride;
        frame.OutputHeight  = ver_stride;
        frame.DisplayWidth  = width;
        frame.DisplayHeight = height;
        frame.FrameType     = decPicture.interlaced;
        frame.DecodeFrmNum  = decPicture.picId;
        frame.ShowTime.TimeLow = decPicture.TimeLow;
        frame.ShowTime.TimeHigh= decPicture.TimeHigh;
        frame.ErrorInfo     = decPicture.nbrOfErrMBs;
        frame.vpumem = decPicture.lineardata;

        memcpy(aOutBuffer, &frame, sizeof(VPU_FRAME));
        *aOutputLength =sizeof(VPU_FRAME);
        return 0;
    } else {
        *aOutputLength =0;
        return -1;
    }

    return -1;
}

int32_t On2_AvcDecoder::pv_on2avcdecoder_deinit(void)
{
    //Mutex::Autolock autoLock(mLock);
#ifdef STREAM_DEBUG
    if (fpStream) {
        fclose(fpStream);
        fpStream = NULL;
    }
#endif
#ifdef OUTPUT_DEBUG
    if (fpOut) {
        fclose(fpOut);
        fpOut = NULL;
    }
#endif
#ifdef PTS_DEBUG
    if (pts_in) {
        fclose(pts_in);
        pts_in = NULL;
    }
    if (pts_out) {
        fclose(pts_out);
        pts_out = NULL;
    }
#endif

//    DispbuffFree(); //释放输出显示的buffer

    /* if output in display order is preferred, the decoder shall be forced
     * to output pictures remaining in decoded picture buffer. Use function
     * H264DecNextPicture() to obtain next picture in display order. Function
     * is called until no more images are ready for display. Second parameter
     * for the function is set to '1' to indicate that this is end of the
     * stream and all pictures shall be output */
    if(streamMem->vir_addr != NULL)
    {
        VPUFreeLinear(streamMem);
    }
    streamSize = 0;
    /* release decoder instance */
    H264DecRelease(H264deccont);

//    DispbuffFree();

    ALOGI("deinit DONE\n");

    return 0;
}

void On2_AvcDecoder::pv_on2avcdecoder_rest()
{
    //Mutex::Autolock autoLock(mLock);
    H264DecPicture decPicture;

    if (status)
        return ;

    H264DecReset(H264deccont);

    while(H264DecNextPicture(H264deccont, &decPicture) == H264DEC_PIC_RDY)
    {
        VPUMemLink(&decPicture.lineardata);
        VPUFreeLinear(&decPicture.lineardata);
    }
}
int32_t On2_AvcDecoder::pv_on2avcdecoder_perform(int32_t cmd, void *data)
{
    int32_t ret = 0;
    switch (cmd) {
    case SET_MVC_DISABLE : {
        H264deccont->disable_mvc = 1;
    } break;
    default : {
        ret = -EINVAL;
    } break;
    }
    return ret;
}
extern "C"
void *  get_class_On2AvcDecoder(void)
{
    return (void*)new On2_AvcDecoder();
}

extern "C"
void  destroy_class_On2AvcDecoder(void * AvcDecoder)
{
    delete (On2_AvcDecoder *)AvcDecoder;
}

extern "C"
int init_class_On2AvcDecoder(void * AvcDecoder,int ts_en)
{
	On2_AvcDecoder * Avcdec = (On2_AvcDecoder *)AvcDecoder;
	return Avcdec->pv_on2avcdecoder_init(ts_en);
}

extern "C"
int deinit_class_On2AvcDecoder(void * AvcDecoder)
{
	On2_AvcDecoder * Avcdec = (On2_AvcDecoder *)AvcDecoder;
	return Avcdec->pv_on2avcdecoder_deinit();
}

extern "C"
void reset_class_On2AvcDecoder(void * AvcDecoder)
{
	On2_AvcDecoder * Avcdec = (On2_AvcDecoder *)AvcDecoder;
	Avcdec->pv_on2avcdecoder_rest();
}
extern "C"
int perform_class_On2AvcDecoder(void * AvcDecoder, int cmd, void * data)
{
	On2_AvcDecoder * Avcdec = (On2_AvcDecoder *)AvcDecoder;
	return Avcdec->pv_on2avcdecoder_perform(cmd, data);
}

extern "C"
int dec_oneframe_class_On2AvcDecoder(void * AvcDecoder,uint8_t* aOutBuffer, uint32_t *aOutputLength,
        uint8_t* aInputBuf, uint32_t* aInBufSize,int64_t* InputTimestamp)
{
	int ret;
	printf("  <%s>_%d \n", __func__, __LINE__);
	int64_t myTimestamp = (* InputTimestamp);
	On2_AvcDecoder * Avcdec = (On2_AvcDecoder *)AvcDecoder;

	ret = Avcdec->pv_on2avcdecoder_oneframe(aOutBuffer,aOutputLength,
					aInputBuf,aInBufSize, myTimestamp);
	printf("  <%s>_%d    ret = %d \n", __func__, __LINE__, ret);
	(* InputTimestamp) = myTimestamp;

	return ret;
}

extern "C"
int flush_oneframe_in_dpb_class_On2AvcDecoder(void * AvcDecoder,uint8_t* aOutBuffer, uint32_t *aOutputLength)
{
    int ret;
    On2_AvcDecoder * Avcdec = (On2_AvcDecoder *)AvcDecoder;
    ret = Avcdec->pv_on2avc_flushOneFrameInDpb(aOutBuffer, aOutputLength);
    return ret;
}

}

using namespace android;

#ifdef AVC_TEST1
int main()
{
	int ret;
	FILE *fpin, *fpout;
	uint8_t *ptr = NULL;
    VPU_FRAME buffer_frame;
    uint8_t *buffer_stream;
	uint32_t aOutputLength = 0;
	uint32_t aInBufSize = 0;
	uint32_t tmp[2];
	int64_t  InputTimestamp = 0LL;
	On2_AvcDecoder *decoder = NULL;

	fpin = fopen("/sdcard/out.h264", "rb");
	if(fpin == NULL)
	{
		ALOGV("fpin file open failed!\n");
		return 0;
	}
	//fseek(fpin, 0x32f99b6-8, SEEK_CUR);	//

	fpout = fopen("/sdcard/fpout.yuv", "wb+");
	if(fpout == NULL)
	{
		ALOGV("fpin file open failed!\n");
		return 0;
	}

	//mem init
	memset(&buffer_frame, 0, sizeof(buffer_stream));
	buffer_stream = (uint8_t *)malloc(1024*1024*2);
	/*memset(&buffer_stream, 0, sizeof(buffer_stream));
	if (VPUMallocLinear(&buffer_stream, 1024*2048))
    {
        ALOGV("VPUMallocLinear buffer_stream fail\n");
        return 0;
    }*/


	decoder = new On2_AvcDecoder();
    if (NULL == decoder)
	{
        ALOGV("create On2_AvcDecoder failed\n");
        return -5;
    }

    ret = decoder->pv_on2avcdecoder_init(1);
    if (ret)
	{
        ALOGV("pv_on2avcdecoder_init failed ret %d\n", ret);
        return -6;
    }


    do {
		ret = fread(&tmp, 1, sizeof(tmp), fpin);
		if((ret < sizeof(tmp)) || (tmp[0] != 0x42564b52))
		{
			ALOGV("input.h264 file has end!=%d, tmp[0]=0x%x\n", ret, tmp[0]);
			break;
		}
		ret = fread(buffer_stream, 1, tmp[1], fpin);
		if(ret < tmp[1])
		{
			ALOGV("stream size has lost!\n", ret);
			break;
		}
		aInBufSize = tmp[1];

decoder_again:
		//memset(&buffer_frame, 0, sizeof(buffer_stream));
		aOutputLength = 0;
        ret = decoder->pv_on2avcdecoder_oneframe((uint8_t *)&buffer_frame, (uint32_t *)&aOutputLength,
            (uint8_t *)buffer_stream, (uint32_t *)&aInBufSize, InputTimestamp);
        if (ret < 0)
		{
            ALOGV("error %d\n", ret);
        }

		//ALOGV("ret %d aOutputLength: %d\n", ret, aOutputLength);

		if (aOutputLength)
		{
			ALOGV("aOutputLengthaOutputLengthaOutputLengthaOutputLength=%d\n");
			VPUMemLink(&buffer_frame.vpumem);

			#if	0
			{
				#define OUTPUT_WIDTH		640//160
				#define OUTPUT_HEIGHT		480//128
				uint8_t *dstmem = (uint8_t *)malloc(OUTPUT_WIDTH*OUTPUT_HEIGHT*2);
				uint8_t *src = (u8 *)buffer_frame.vpumem.vir_addr;
				uint8_t *dst = dstmem;
				uint32_t scalex = buffer_frame.FrameWidth/OUTPUT_WIDTH;
				uint32_t scaley = buffer_frame.FrameHeight/OUTPUT_HEIGHT;
				uint32_t i,j;

				VPUMemInvalidate(&buffer_frame.vpumem);

				for(i=0;i<OUTPUT_HEIGHT;i++)
					for(j=0;j<OUTPUT_WIDTH;j++)
					{
						dst[i*OUTPUT_WIDTH+j] = src[buffer_frame.FrameWidth*i*scaley+j*scalex];
					}

				src = src + buffer_frame.FrameWidth*buffer_frame.FrameHeight;
				dst = dst + OUTPUT_WIDTH*OUTPUT_HEIGHT;
				for(i=0;i<OUTPUT_HEIGHT/2;i++)
					for(j=0;j<OUTPUT_WIDTH/2;j++)
					{
						dst[i*OUTPUT_WIDTH+j*2] = src[buffer_frame.FrameWidth*i*scaley+j*scalex*2];
						dst[i*OUTPUT_WIDTH+j*2+1] = src[buffer_frame.FrameWidth*i*scaley+j*scalex*2+1];
					}

				fwrite(dstmem, 1, OUTPUT_WIDTH*OUTPUT_HEIGHT*3/2, fpout);
				fflush(fpout);
				free(dstmem);
			}
			#endif

			VPUFreeLinear(&buffer_frame.vpumem);
        }

		//ALOGV("aInBufSize=%d\n", aInBufSize);
		if(aInBufSize)
			goto decoder_again;
    } while (1);

	//if(buffer_stream.vir_addr)
    //	VPUFreeLinear(&buffer_stream);

    if (decoder)
	{
        decoder->pv_on2avcdecoder_rest();
        decoder->pv_on2avcdecoder_deinit();
        delete(decoder);
    }

	if(fpout)fclose(fpout);

	ALOGV("decoding end..!\n");

	return 0;
}

#endif


#ifdef AVC_TEST

#include <utils/Vector.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <media/stagefright/foundation/ABitReader.h>

#define PRINT_SLICE_TYPE        0
#if PRINT_SLICE_TYPE
#define ALOGV                  printf
#else
#define PRINTF
#endif

#define SIZE_STREAM             (1024*1024)
#define SIZE_FRAME              (1928*1088)

#define ERR_UNSUPPORT_STREAM    -2
#define ERR_SLICE_IN_MIDDLE     -3

typedef struct NALPacket {
    uint8_t *nalStart;
    size_t nalSize;
} NALPacket;

void print_help()
{
    ALOGV("help: avc_dec [file]\n");
}

int find264NALFragment(uint8_t *data, size_t size, size_t *fragSize) {
    static const char kStartCode[4] = { 0x00, 0x00, 0x00, 0x01 };

    if (size < 4) {
        ALOGV("size %d < 4\n", size);
        return -1;
    }

    if (memcmp(kStartCode, data, 4)) {
        ALOGV("StartCode not found in %.2x %.2x %.2x %.2x\n", data[0], data[1], data[2], data[3]);
        return -2;
    }

    size_t offset = 4;
    while (offset + 3 < size && memcmp(kStartCode, &data[offset], 4)) {
        ++offset;
    }

    *fragSize = offset;

    return (int)(data[4] & 0x1f);
}

int getSliceStartMBA(uint8_t *nalStart, size_t nalSize)
{
    uint32_t val = ((uint32_t)(nalStart[5] << 24)) |
                   ((uint32_t)(nalStart[6] << 16)) |
                   ((uint32_t)(nalStart[7] <<  8)) |
                   ((uint32_t)(nalStart[8] <<  0));
    uint32_t mask = 0x80000000;
    uint32_t numZeroes = 0;
    uint32_t mba;
    while ((val&mask) == 0) {
        ++numZeroes;
        val <<= 1;
    }

    if (0 == numZeroes) {
        mba = 0;
    } else {
        mba = (val >> (31 - numZeroes)) - 1;
    }

    //ALOGV("found slice mba %d numZeroes %d\n", mba, numZeroes);
    return mba;
}

int foundCompleteFrame(int type, int lastType, uint8_t *ptr, size_t size)
{
    static int firstMB = -1;
    int ret = 0;
    if (lastType < 0) {
        switch (type) {

        case NAL_CODED_SLICE :
        case NAL_CODED_SLICE_EXT :
        case NAL_CODED_SLICE_IDR : {
            firstMB = getSliceStartMBA(ptr, size);
            if (firstMB == 0) {
                goto FOUND_CONINUE;
            } else {
                firstMB = -1;
                ret =  ERR_SLICE_IN_MIDDLE;
                goto FOUND_ERR;
            }
        } break;

        case NAL_CODED_SLICE_DP_A :
        case NAL_CODED_SLICE_DP_B :
        case NAL_CODED_SLICE_DP_C : {
            ALOGV("do not support data partition\n");
            ret = ERR_UNSUPPORT_STREAM;
            goto FOUND_ERR;
        } break;

        case NAL_SEI : {
            ALOGV("found NAL: SEI\n");
            goto FOUND_SINGLE;
        } break;

        case NAL_SEQ_PARAM_SET :
        case NAL_SPS_EXT : {
            ALOGV("found NAL: SPS\n");
            goto FOUND_SINGLE;
        } break;

        case NAL_PIC_PARAM_SET :
        case NAL_SUBSET_SEQ_PARAM_SET : {
            ALOGV("found NAL: PPS\n");
            goto FOUND_SINGLE;
        } break;

        case NAL_ACCESS_UNIT_DELIMITER : {
            ALOGV("found NAL: ACCESS_UNIT_DELIMITER\n");
            goto FOUND_SINGLE;
        } break;

        case NAL_END_OF_SEQUENCE : {
            ALOGV("found NAL: NAL_END_OF_SEQUENCE\n");
            goto FOUND_SINGLE;
        } break;

        case NAL_END_OF_STREAM : {
            ALOGV("found NAL: NAL_END_OF_STREAM\n");
            goto FOUND_SINGLE;
        } break;

        case NAL_FILLER_DATA : {
            ALOGV("found NAL: NAL_FILLER_DATA\n");
            goto FOUND_SINGLE;
        } break;

        case NAL_CODED_SLICE_AUX : {
            ALOGV("found NAL: NAL_CODED_SLICE_AUX\n");
            goto FOUND_SINGLE;
        } break;

        default : {
            ALOGV("found NAL: unknown\n");
            ret = ERR_UNSUPPORT_STREAM;
            goto FOUND_ERR;
        } break;
        }
    } else {
        switch (type) {

        case NAL_CODED_SLICE :
        case NAL_CODED_SLICE_EXT :
        case NAL_CODED_SLICE_IDR : {
            int CurrMB = getSliceStartMBA(ptr, size);
            if (CurrMB == 0) {
                goto FOUND_LAST;
            } else {
                goto FOUND_CONINUE;
            }
        } break;

        case NAL_CODED_SLICE_DP_A :
        case NAL_CODED_SLICE_DP_B :
        case NAL_CODED_SLICE_DP_C : {
            ALOGV("do not support data partition\n");
            return ERR_UNSUPPORT_STREAM;
        } break;

        case NAL_SEI : {
            ALOGV("found NAL: SEI\n");
            goto FOUND_LAST;
        } break;

        case NAL_SEQ_PARAM_SET :
        case NAL_SPS_EXT : {
            ALOGV("found NAL: SPS\n");
            goto FOUND_LAST;
        } break;

        case NAL_PIC_PARAM_SET :
        case NAL_SUBSET_SEQ_PARAM_SET : {
            ALOGV("found NAL: PPS\n");
            goto FOUND_LAST;
        } break;

        case NAL_ACCESS_UNIT_DELIMITER : {
            ALOGV("found NAL: ACCESS_UNIT_DELIMITER\n");
            goto FOUND_LAST;
        } break;

        case NAL_END_OF_SEQUENCE : {
            ALOGV("found NAL: NAL_END_OF_SEQUENCE\n");
            goto FOUND_LAST;
        } break;

        case NAL_END_OF_STREAM : {
            ALOGV("found NAL: NAL_END_OF_STREAM\n");
            goto FOUND_LAST;
        } break;

        case NAL_FILLER_DATA : {
            ALOGV("found NAL: NAL_FILLER_DATA\n");
            goto FOUND_LAST;
        } break;

        case NAL_CODED_SLICE_AUX : {
            ALOGV("found NAL: NAL_CODED_SLICE_AUX\n");
            goto FOUND_LAST;
        } break;

        default : {
            ALOGV("found NAL: unknown\n");
            ret = ERR_UNSUPPORT_STREAM;
            goto FOUND_ERR;
        } break;
        }
    }

FOUND_SINGLE:
    firstMB = -1;
    return 1;

FOUND_CONINUE:
    return 0;

FOUND_LAST:
    firstMB = -1;
    return -1;

FOUND_ERR:
    firstMB = -1;
    return ret;
}

int find264FrameStream(uint8_t *data, size_t size, NALPacket *p) {
    uint8_t *ptr = data;
    size_t tmpSize = 0;
    int frameStart = 0;
    int lastType = -1;

    p->nalStart = data;
    p->nalSize = 0;

    do {
        int type = find264NALFragment(ptr, size, &tmpSize);
        if (type <= 0) {
            ALOGV("find264FrameStream found error while looking for NALFragment\n");
            return -1;
        } else {
            int found = foundCompleteFrame(type, lastType, ptr, size);
            if (found > 0) /* found a complete packet */ {
                p->nalSize = tmpSize;
                goto FRAME_COMPLETE;
            } else if (found == 0) /* not a complete packet continue */ {
                p->nalSize += tmpSize;
                ptr += tmpSize;
                lastType = type;
            } else if (found == -1) /* last packet for a packet */{
                goto FRAME_COMPLETE;
            } else {
                ptr += tmpSize;
                size -= tmpSize;
                p->nalStart = ptr;
                p->nalSize = 0;
                ALOGV("foundCompleteFrame error ret %d skip size %d reseek at 0x%.8x size %d\n", found, tmpSize, (u32)ptr, size);
                continue;
            }
        }
    } while (size > 100);

FRAME_COMPLETE:
    return 0;
}

static int fd = -1;
static int fb_flag = 0;
static struct fb_var_screeninfo var;
int display_init()
{
    ALOGV("we will open /dev/graphics/fb1 now\n");
    fd = open("/dev/graphics/fb1", O_RDWR, 0);
    if (fd < 0)
    {
        ALOGV("open fb1 fail!\n");
        return -1;
    }

    ALOGV("we will get FBIOGET_VSCREENINFO now\n");
    if (ioctl(fd, FBIOGET_VSCREENINFO, &var) == -1) {
        ALOGV("ioctl fb1 FBIOGET_VSCREENINFO fail!\n");
        return -1;
    }

    ALOGV("we just set 320 * 240 as the test width and height\n");
    var.xres_virtual = 320;    //win0 memery x size
    var.yres_virtual = 240;    //win0 memery y size
    var.xoffset = 0;   //win0 start x in memery
    var.yoffset = 0;   //win0 start y in memery
    var.xres = 320;    //win0 show x size
    var.yres = 240;    //win0 show y size
    var.nonstd = ((0<<20)&0xfff00000) + ((0<<8)&0xfff00) + 2; //win0 ypos & xpos & format (ypos<<20 + xpos<<8 + format)    // 2 uvuv  1 uuuuvvvv
    var.grayscale = ((768<<20)&0xfff00000) + ((1024<<8)&0xfff00) + 0;   //win0 xsize & ysize
    var.bits_per_pixel = 16;

    return 0;
}

int display_picture(VPU_FRAME *frame)
{
    if(frame)
    {
        if (!fb_flag)
        {
            fb_flag = 1;

            var.xres_virtual = frame->DisplayWidth;    //win0 memery x size
            var.yres_virtual = frame->DisplayHeight;    //win0 memery y size
            var.xres = frame->DisplayWidth;    //win0 show x size
            var.yres = frame->DisplayHeight;    //win0 show y size
            ALOGV("frame->picWidth = 0x%x, frame->picHeight = 0x%x\n",frame->DisplayWidth, frame->DisplayHeight);
            ALOGV("frame->visible_addr = 0x%x\n",frame->FrameBusAddr[0]);
            var.reserved[2] = 1;
            var.reserved[3] = (unsigned long)frame->FrameBusAddr[0];
            ALOGV("frame->visible_addr = 0x%x\n",frame->FrameBusAddr[0]);
            var.reserved[4] = (unsigned long)frame->FrameBusAddr[1];
            //ALOGV("Get one frame: pic->data[0] = %x, w: %d, h: %d >>>>>>>>>>>! \n", (unsigned int)pic.data[0], (unsigned int)pic.width, (unsigned int)pic.height);
            ALOGV(" start displayer \n");
            if (ioctl(fd, FBIOPUT_VSCREENINFO, &var) == -1)
            {
                ALOGV("ioctl fb1 FBIOPUT_VSCREENINFO fail!\n");
                return -1;
            }
        }
        else
        {
            int data[2];
            data[0] = (int)frame->FrameBusAddr[0];
            data[1] = (int)frame->FrameBusAddr[1];
            //ALOGV("Get one frame: pic->data[0] = %x, y: %x, uv: %x >>>>>>>>>>>! \n", (unsigned int)pic.data[0], (unsigned int)data[0], (unsigned int)data[1]);
            //ALOGV("ts=%d >>>>>>>>>>>! \n", pic.pts.low_part);
            if (ioctl(fd, 0x5002, data) == -1)
            {
                ALOGV("ioctl fb1 FBIOPUT_VSCREENINFO fail!\n");
                return -1;
            }
        }
    }
    else
    {
        ALOGV("frame is null\n");
    }

    return 0;
}
Vector<VPU_FRAME> frames;

FILE *output = NULL;
int displaying = 0;

#define SLEEP_GAP   20000

void *thread_display(void *args)
{
    u32 width, height;
    u32 display_en = 1;
    if (display_init()) {
        display_en = 0;
    }

    while (displaying) {
        if (frames.size()> 10) {
            VPU_FRAME frame = frames.editItemAt(0);
            frames.removeAt(0);
            width  = frame.DisplayWidth;
            height = frame.DisplayHeight;
            VPUMemLink(&frame.vpumem);

            if (output) {
                static int once = 1;
                VPUMemInvalidate(&frame.vpumem);
                if (once) {
                    ALOGV("width %d height %d\n", width, height);
                    once = 0;
                }
                fwrite(frame.vpumem.vir_addr, 1, width*height*3/2, output);
            }

            if (display_en) {
                display_picture(&frame);
            }
            usleep(SLEEP_GAP);

            VPUFreeLinear(&frame.vpumem);
        } else {
            usleep(SLEEP_GAP*2);
        }
    }

    return NULL;
}

int main(int argc, char **argv) {
    int fd = -1;
    uint8_t *ptr = NULL;
    uint8_t *buffer_frame = NULL;
    uint8_t *buffer_stream = NULL;
    On2_AvcDecoder *decoder = NULL;
    struct stat stat_data;
    size_t filesize = 0;
    size_t leftSize = 0;
    int ret = 0;
    int file_end = 0;
    int index_w, index_r = 8;
    pthread_t thread;

    if (argc < 2) {
        print_help();
        return 0;
    }

    displaying = 1;

    {   // create display thread
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&thread, &attr, thread_display, NULL);
    pthread_attr_destroy(&attr);
    }

    do {
    if (argc > 2) {
        output = fopen("/flash/output.yuv", "w+b");
        if (NULL == output) {
            ALOGV("can not write output file\n");
            ret = -1;
            break;
        }
    }
    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        ALOGV("input file not exsist\n");
        ret = -2;
        break;
    }
    if (fstat(fd,&stat_data) < 0)
    {
        ALOGV("fstat fail\n");
        ret = -3;
        break;
    }

    buffer_stream = (uint8_t *)mmap(NULL, stat_data.st_size, PROT_READ,MAP_SHARED, fd, 0);
    if (MAP_FAILED == buffer_stream)
    {
        ALOGV("mmap fail\n");
        ret = -4;
        break;
    }

    ptr = buffer_stream;
    filesize = leftSize = stat_data.st_size;
    ALOGV("file size: %d\n", filesize);

    decoder = new On2_AvcDecoder();
    if (NULL == decoder) {
        ALOGV("create On2_AvcDecoder failed\n");
        ret = -5;
        break;
    }

    ret = decoder->pv_on2avcdecoder_init(0);
    if (ret) {
        ALOGV("pv_on2avcdecoder_init failed ret %d\n", ret);
        ret = -6;
        break;
    }

    buffer_frame = (uint8_t *)malloc(SIZE_FRAME);
    if (NULL == buffer_frame) {
        ALOGV("create buffer_frame failed\n");
        ret = -7;
        break;
    }

    do {
        uint32_t aOutputLength = 0;
        uint32_t aInBufSize = 0;
        int64_t  InputTimestamp = 0;
        NALPacket nalPacket;

        ret = find264FrameStream(ptr, leftSize, &nalPacket);
        if (ret) {
            ALOGV("find264FrameStream found not nal\n");
            ret = -8;
            break;
        }

        ALOGV("nal type %d size %d\n", nalPacket.nalStart[4]&0x1f, nalPacket.nalSize);

        leftSize -= (nalPacket.nalStart - ptr) + nalPacket.nalSize;
        aInBufSize = nalPacket.nalSize;

        ret = decoder->pv_on2avcdecoder_oneframe(
            buffer_frame, (uint32_t *)&aOutputLength,
            ptr, (uint32_t *)&aInBufSize, InputTimestamp);
        if (ret < 0) {
            //ALOGV("pv_on2avcdecoder_oneframe failed ret %d aOutputLength %d\n", ret, aOutputLength);
            ALOGV("error %d\n", ret);
        }

        ALOGV("ret %d aOutputLength: %d\n", ret, aOutputLength);
        if (aOutputLength) {
            VPU_FRAME *frame = (VPU_FRAME *)buffer_frame;
            frames.push(*frame);
            usleep(SLEEP_GAP);
        }

        ptr = nalPacket.nalStart + nalPacket.nalSize;
        //ALOGV("ptr 0x%.8x\n", ptr);
    } while (leftSize > 100);

    if (ret < 0) {
        ALOGV("(ret < 0)\n");
        break;
    }

    if (decoder) {
        decoder->pv_on2avcdecoder_rest();
        decoder->pv_on2avcdecoder_deinit();
        delete(decoder);
    }
    if (buffer_stream) {
        munmap(buffer_stream, filesize);
    }
    if (fd > 0) {
        close(fd);
    }
    if (output) {
        fclose(output);
    }
    ALOGV("decode done\n");

    {
    void *ptr;
    displaying = 0;
    pthread_join(thread, &ptr);
    }

    return 0;
    } while (0);

    if (decoder) {
        decoder->pv_on2avcdecoder_rest();
        decoder->pv_on2avcdecoder_deinit();
        delete(decoder);
    }
    if (buffer_stream) {
        munmap(buffer_stream, filesize);
    }
    if (fd > 0) {
        close(fd);
    }
    if (output) {
        fclose(output);
    }

    ALOGV("decode fail\n");

    {
    void *ptr;
    displaying = 0;
    pthread_join(thread, &ptr);
    }

    return ret;
}
#endif

