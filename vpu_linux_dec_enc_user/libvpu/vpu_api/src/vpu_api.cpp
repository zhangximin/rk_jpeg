
#define  LOG_TAG "vpu_api"
#include <utils/Log.h>
//#include <utils/List.h>

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <errno.h>
//#include "OMX_Core.h"

//#include "framemanager.h"

#include "vpu.h"
#include "vpu_mem.h"
#include <pthread.h>
#include "rk_list.h"

#include "vpu_api_private.h"
#include "vpu_api.h"
//#include "post_process/deinterlace/include/deinter.h"
#include "svn_info.h"

#define OMX_BUFFERFLAG_EOS 0x00000001


#undef LOG_TAG
#define LOG_TAG "vpu_api"

#define _VPU_API_DEBUG

#ifdef  _VPU_API_DEBUG
#ifdef AVS40
#define VPU_API_DEBUG           printf
#else
#define VPU_API_DEBUG           printf
#endif
#else
#define VPU_API_DEBUG
#endif

#ifdef AVS40
#define VPU_API_ERROR           printf
#else
#define VPU_API_ERROR           printf
#endif

#define VPU_API_DEBUG	printf

#define VPU_API_WRITE_DATA_DEBUG 0

static char kStartCode[4] = { 0x00, 0x00, 0x00, 0x01};

static
void addCodecSpecificData(uint8_t *pSpecData,uint8_t *data, int32_t size, uint32_t *len) {
    if ((NULL ==pSpecData) || (NULL ==data) ||
            (NULL ==len) || (size ==0)) {
        return;
    }

    memcpy(pSpecData + *len, kStartCode, 4);
    memcpy(pSpecData + *len + 4, data, size);
    *len += size + 4;
}

static
uint16_t U16_AT(const uint8_t *ptr) {
    return ptr[0] << 8 | ptr[1];
}

static
uint32_t U32_AT(const uint8_t *ptr) {
    return ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3];
}

static
int32_t GetNAL_Config(uint8_t** bitstream, int32_t* size)
{
    int i = 0;
    int j;
    uint8_t* nal_unit = *bitstream;
    int count = 0;

    if (NULL ==nal_unit) {
        return -1;
    }

    while (nal_unit[i++] == 0 && i < *size)
    {
    }

    if (nal_unit[i-1] == 1)
    {
        *bitstream = nal_unit + i;
    }
    else
    {
        j = *size;
        *size = 0;
        return j;  // no SC at the beginning, not supposed to happen
    }

    j = i;
    while (i < *size)
    {
        if (count >= 2 && nal_unit[i] == 0x01)
        {
            i -= 2;
            break;
        }
        if (nal_unit[i])
            count = 0;
        else
            count++;
        i++;
    }

    *size -= i;
    return (i -j);
}

static
void avc_extradata_proc(uint8_t *pSpecData, uint32_t *len, uint8_t *extra_data,
                        int32_t extra_size,int32_t *nal_length) {
    if ((NULL ==pSpecData) || (NULL == len) ||
            (NULL == extra_data) || (NULL == nal_length)) {
        return;
    }

    uint8_t* ptr = extra_data;
    int32_t size = extra_size;
    uint8_t* tp;
    if(size < 7){
       return;
    }
    tp = ptr;
    if (tp[0] == 0 && tp[1] == 0) {
        uint8_t* tmp_ptr = tp;
        uint8_t* buffer_begin = tp;
        int16_t length = 0;
        int initbufsize = size;
        int tConfigSize = 0;
        *nal_length = 0;
        do
        {
            tmp_ptr += length;
            length = GetNAL_Config(&tmp_ptr, &initbufsize);
            addCodecSpecificData(pSpecData, tmp_ptr, length, len);
        }
        while (initbufsize > 0);
    } else {
       uint8_t profile = ptr[1];
       uint8_t level = ptr[3];
       int32_t lengthSize = 1 + (ptr[4] & 3);
       int32_t numSeqParameterSets = ptr[5] & 31;
       int32_t numPictureParameterSets;
       int32_t i;

       *nal_length = lengthSize;

       if(ptr[0] != 1){
           return;
       }

       // There is decodable content out there that fails the following
       // assertion, let's be lenient for now...
       // CHECK((ptr[4] >> 2) == 0x3f);  // reserved

       // commented out check below as H264_QVGA_500_NO_AUDIO.3gp
       // violates it...
       // CHECK((ptr[5] >> 5) == 7);  // reserved

       ptr += 6;
       size -= 6;

       for (i=0; i<numSeqParameterSets; ++i) {
           size_t length;
           length = U16_AT(ptr);
           ptr += 2;
           size -= 2;
           addCodecSpecificData(pSpecData, ptr, length,len);
           ptr += length;
           size -= length;
       }
       numPictureParameterSets = *ptr;
       ++ptr;
       --size;
       for (i = 0; i<numPictureParameterSets; ++i) {
           int32_t length;
           length = U16_AT(ptr);
           ptr += 2;
           size -= 2;
           addCodecSpecificData(pSpecData, ptr, length,len);
           ptr += length;
           size -= length;
       }
    }
}

static
void rv_extradata_proc(uint8_t *destData,uint32_t *len, uint8_t *extra_data,int32_t extra_size){
    if ((NULL ==destData) || (NULL ==len) || (extra_data ==NULL) || (extra_size ==0)) {
        return;
    }

    Stream_describe *strd;
    strd = (Stream_describe*)destData;
    strd->flag[0] = 'R';
    strd->flag[1] = 'K';
    strd->flag[2] = 'V';
    strd->flag[3] = 'B';
    strd->retFlag = 0;

    memcpy(destData + sizeof(Stream_describe), extra_data, extra_size);

    strd->size = extra_size;
    strd->slices = 0;
    strd->timel = 0;
    strd->timeh = 0;
    strd->type = 0x10;
    *len = strd->size + sizeof(Stream_describe);
}

static
void vc1_extradata_proc(VpuCodecContext *ctx, uint8_t* dstData, uint32_t* len)
{
    /* advance(VC-1) rcv info */
    /*
     ** NumFrames(3 bytes)
     ** version(1 bytes)
     ** height(4 bytes)
     ** width(4 bytes)
     ** the first 4 bytes of video extra data (4 bytes).
     */

    /* wmv3 rcv info */
    /*
     ** NumFrames(3 bytes)
     ** version(1 bytes)
     ** extraDataLen(4 bytes)
     ** extradata
     ** height(4 bytes)
     ** width(4bytes)
     */

    if ((NULL ==ctx) || (NULL ==dstData) || (NULL ==len)) {
        return;
    }

    if ((ctx->videoCoding != OMX_ON2_VIDEO_CodingWMV) &&
            (ctx->videoCoding != OMX_ON2_VIDEO_CodingVC1)) {
        return;
    }

    VPU_API_DEBUG("vc1_extradata_proc, extraDataSize size: %d", ctx->extradata_size);

    uint32_t iRcvHeadLen = 0;
    if (ctx->videoCoding == OMX_ON2_VIDEO_CodingWMV) {
        iRcvHeadLen = 4 + 4 + ctx->extradata_size + 4 + 4;
    } else {
        iRcvHeadLen = 4 + 4 + 4 + 4;
    }

    uint8_t* buffer = dstData;
    uint32_t gNumFrame = 0xFFFF;
    *buffer++ = (gNumFrame & 0xff);
    *buffer++ = ((gNumFrame >> 8) & 0xff);
    *buffer++ = ((gNumFrame>> 16) & 0xff);

    if (ctx->videoCoding == OMX_ON2_VIDEO_CodingWMV) {
        *buffer++ = 0x85;
        if (ctx->extradata && (ctx->extradata_size >=0)) {
            memcpy(buffer, &ctx->extradata_size, 4);
            buffer +=4;
            memcpy(buffer, ctx->extradata,ctx->extradata_size);
            buffer +=ctx->extradata_size;
        } else {
            int32_t tmpSize =0;
            memcpy(buffer, &tmpSize, 4);
            buffer +=4;
        }
    } else {
        *buffer++ = 0x88;
    }

    memcpy(buffer, &ctx->height, 4);
    buffer +=4;

    memcpy(buffer, &ctx->width, 4);
    buffer +=4;

    if (ctx->extradata && (ctx->videoCoding == OMX_ON2_VIDEO_CodingVC1)) {
        memcpy(buffer, ctx->extradata, 4);
        buffer +=4;
    }

    *len = iRcvHeadLen;
    return;
}

class VpuDeinter {
public:
    VpuDeinter();
    ~VpuDeinter();

    int32_t init();
    int32_t startDeinterThread();
    int32_t stopDeinterThread();
    void    flush();
    void    flush_deinter();

    int32_t deint_perform(uint32_t cmd, uint32_t *data);
    int32_t pushPreDeinterFrame(uint8_t* buf,  uint32_t size, int64_t timeUs);
    int32_t getPreDeinterFrame(VPU_FRAME* frame);

    int32_t pushDeinterlaceFrame(VPU_FRAME* frame);
    int32_t getDeinterlaceFrame(uint8_t* buf,  uint32_t* size, int64_t &timestamp);

    void    deinterlacePoll();
    int32_t deinterlaceProc();

    static int32_t frame_destroy(VPU_FRAME* frame);

private:
    android::deinterlace_dev* deint_dev;
    pthread_t thread;
	int32_t thread_running;
	int32_t thread_reset;
    int32_t deint_status;
    int32_t deint_flag;
    pthread_cond_t  pre_deint_cond;
    pthread_mutex_t pre_deint_mutex;
    rk_list *preDeintFrm;       /* pre deinterlace process frame list */
    rk_list *deintFrm;          /* deinterlace processed frame list */
    DeinterContext_t mDeintCtx;

    pthread_cond_t  reset_cond;
    pthread_mutex_t reset_mutex;

    static void* deinter_thread(void *me);
};

VpuDeinter::VpuDeinter()
    :deint_dev(NULL),
     thread_running(0),
     thread_reset(0),
     deint_status(DEINTER_NOT_INIT),
     deint_flag(0),
     preDeintFrm(NULL),
     deintFrm(NULL)
{
}

VpuDeinter::~VpuDeinter()
{
    VPU_API_DEBUG("~~VpuDeinter in");
    stopDeinterThread();
    VPU_API_DEBUG("~~VpuDeinter pos 0");
    pthread_cond_destroy(&pre_deint_cond);
    pthread_mutex_destroy(&pre_deint_mutex);
    pthread_cond_destroy(&reset_cond);
    pthread_mutex_destroy(&reset_mutex);

    if (deint_dev && mDeintCtx.poll_flag) {
        deint_dev->sync();
    }
    if (preDeintFrm) {
        delete preDeintFrm;
        preDeintFrm = NULL;
    }
    if (deintFrm) {
        delete deintFrm;
        deintFrm = NULL;
    }
    if (deint_dev) {
        delete deint_dev;
        deint_dev = NULL;
    }
}

int32_t VpuDeinter::init()
{
    if (deint_status !=DEINTER_NOT_INIT) {
        return 0;
    }
    printf("   <%s>_%d \n", __func__, __LINE__);
    pthread_mutex_init(&pre_deint_mutex, NULL);
    pthread_cond_init(&pre_deint_cond, NULL);
    pthread_mutex_init(&reset_mutex, NULL);
    pthread_cond_init(&reset_cond, NULL);
    memset(&mDeintCtx, 0, sizeof(DeinterContext_t));

    deint_dev = new android::deinterlace_dev();
    if (deint_dev ==NULL) {
        deint_status = DEINTER_ERR_OPEN_DEV;
        goto VPU_DEINT_INIT_FAIL;
    }
    if (deint_dev->test()) {
        deint_status = DEINTER_ERR_TEST_DEV;
        goto VPU_DEINT_INIT_FAIL;
    }

    preDeintFrm = new rk_list((node_destructor)frame_destroy);
    if (NULL == preDeintFrm) {
        deint_status = DEINTER_ERR_ERR_LIST_STREAM;
        VPU_API_ERROR("found fatal error when creating stream list");
        goto VPU_DEINT_INIT_FAIL;
    }

    deintFrm = new rk_list((node_destructor)frame_destroy);
    if (NULL == deintFrm) {
        deint_status = DEINTER_ERR_ERR_LIST_STREAM;
        VPU_API_ERROR("found fatal error when creating frame list");
        goto VPU_DEINT_INIT_FAIL;
    }

    deint_status = DEINTER_OK;
    startDeinterThread();
    if (deint_status) {
        goto VPU_DEINT_INIT_FAIL;
    }

    return 0;

VPU_DEINT_INIT_FAIL:
    VPU_API_ERROR("init fail for vpu deinterlace, deint_status: %d", deint_status);
    return deint_status;
}

int32_t VpuDeinter::startDeinterThread()
{
    VPU_API_DEBUG("startDeinterThread");
    if (!thread_running) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        if (pthread_create(&thread, &attr, deinter_thread, (void*)this)) {
            deint_status == VPU_API_ERR_FATAL_THREAD;
        }
        pthread_attr_destroy(&attr);
    }
    return 0;
}

int32_t VpuDeinter::stopDeinterThread()
{
    VPU_API_DEBUG("thread_stop");
    if (thread_running) {
        pthread_mutex_lock(&pre_deint_mutex);
        thread_running = 0;
        pthread_cond_signal(&pre_deint_cond);
        pthread_mutex_unlock(&pre_deint_mutex);
        void *dummy;
        pthread_join(thread, &dummy);
    }
    return 0;
}

void VpuDeinter::flush()
{
    if (deint_status) {
        return;
    }

    pthread_mutex_lock(&reset_mutex);

    pthread_mutex_lock(&pre_deint_mutex);
    thread_reset = 1;
    pthread_cond_signal(&pre_deint_cond);
    pthread_mutex_unlock(&pre_deint_mutex);

    pthread_cond_wait(&reset_cond, &reset_mutex);
    pthread_mutex_unlock(&reset_mutex);

    return;
}

void VpuDeinter::flush_deinter()
{
    if (deint_status) {
        return;
    }

    if (deint_dev && mDeintCtx.poll_flag) {
        deint_dev->sync();
    }
    if (preDeintFrm) {
        preDeintFrm->flush();
    }
    if (deintFrm) {
        deintFrm->flush();
    }
    mDeintCtx.poll_flag = 0;

    return;
}

int32_t VpuDeinter::deint_perform(uint32_t cmd, uint32_t *data)
{
    if (deint_status) return deint_status;
    int32_t ret = 0;
    switch (cmd) {
    case DEINT_GET_PRE_PROCESS_COUNT : {
        int32_t* count = (int32_t*)data;
        *count = preDeintFrm->list_size();
    } break;
    case DEINT_GET_FRAME_COUNT : {
        int32_t* count = (int32_t*)data;
        *count = deintFrm->list_size();
    } break;
    case DEINT_SET_INPUT_EOS_REACH: {
        deint_flag |=DEINTER_INPUT_EOS_REACHED;
    } break;

    default : {
        VPU_API_ERROR("invalid command %d", cmd);
        ret = -EINVAL;
    } break;
    }
    return ret;
}

int32_t VpuDeinter::pushPreDeinterFrame(uint8_t* buf,  uint32_t size, int64_t timeUs)
{
    if (deint_status) {
        return deint_status;
    }
    if ((buf ==NULL) || (size ==0)) {
        return 0;
    }

    // do not push too many stream buffer
    //if (preDeintFrm->list_size() > 4) return -1;

    pthread_mutex_lock(&pre_deint_mutex);
    int32_t ret = preDeintFrm->add_at_head(buf, sizeof(VPU_FRAME));
    if (ret) {
        pthread_mutex_unlock(&pre_deint_mutex);
        VPU_API_ERROR("failed to add node to pre deinter list");
        deint_status = DEINTER_ERR_LIST_PRE_DEINT;
        return deint_status;
    } else {
        pthread_cond_signal(&pre_deint_cond);
        pthread_mutex_unlock(&pre_deint_mutex);
    }
    return 0;
}

int32_t VpuDeinter::getPreDeinterFrame(VPU_FRAME* frame)
{
    int32_t ret = -EINVAL;
    if (preDeintFrm->list_size()) {
        ret = preDeintFrm->del_at_tail(frame, sizeof(VPU_FRAME));
    }

    return ret;
}

int32_t VpuDeinter::pushDeinterlaceFrame(VPU_FRAME* frame)
{
    if (deint_status) return deint_status;

    int32_t ret = deintFrm->add_at_head(frame, sizeof(VPU_FRAME));
    return ret;
}

int32_t VpuDeinter::getDeinterlaceFrame(uint8_t* buf,  uint32_t* size, int64_t &timestamp)
{
    if(deint_status ==DEINTER_NOT_INIT){
        *size = 0;
        return 0;
    }
    if (deint_status) return deint_status;

    if(!deintFrm->list_size()){
        *size = 0;
        if (deint_flag & DEINTER_INPUT_EOS_REACHED) {
            return -1;
        } else {
            return 0;
        }
    }

    if (deintFrm->del_at_tail(buf, sizeof(VPU_FRAME))) {
        *size = 0;
    } else {
        VPU_FRAME *p = (VPU_FRAME*)buf;
        *size = sizeof(VPU_FRAME);
        timestamp = p->ShowTime.TimeHigh;
        timestamp = (timestamp << 32) + p->ShowTime.TimeLow;
    }
    return 0;
}

void VpuDeinter::deinterlacePoll()
{
    int64_t timeUs = 0;
    VPU_FRAME* frame =NULL;

    if (mDeintCtx.poll_flag ==0) {
        return;
    }
    if (deint_dev->sync()) {
        return;
    }

    frame = &mDeintCtx.originBuf;
	if (frame->vpumem.phy_addr) {
		VPUMemLink(&frame->vpumem);
		VPUFreeLinear(&frame->vpumem);
	}

    frame = &mDeintCtx.deintBuf;
	pushDeinterlaceFrame(frame);
    mDeintCtx.poll_flag = 0;
    return;
}

int32_t VpuDeinter::deinterlaceProc()
{
    if (!preDeintFrm->list_size()) return 0;

    int32_t ret =0;
    ret = getPreDeinterFrame(&mDeintCtx.deintBuf);
    if (ret) {
        VPU_API_DEBUG("get pre deinterlace frame fail in deinterlaceProc");
        return ret;
    }

    VPU_FRAME *frame = (VPU_FRAME*)(&mDeintCtx.deintBuf);
    VPU_FRAME* orig = &mDeintCtx.originBuf;
    memcpy(orig, frame, sizeof(VPU_FRAME));
    ret = deint_dev->perform(frame, 0);
    if (ret) {
        VPU_API_ERROR("perform deinterlace failed");
    	if (frame->vpumem.phy_addr) {
    		VPUMemLink(&frame->vpumem);
    		VPUFreeLinear(&frame->vpumem);
    	}
        memset(orig, 0, sizeof(VPU_FRAME));
        return -1;
    } else {
        mDeintCtx.poll_flag = 1;
    }

    return 0;
}

int32_t VpuDeinter::frame_destroy(VPU_FRAME *frame)
{
    if (frame) {
        VPUMemLink(&frame->vpumem);
        VPUFreeLinear(&frame->vpumem);
    }
    return 0;
}

void* VpuDeinter::deinter_thread(void *aVpuDeint)
{
    if (aVpuDeint ==NULL) {
        return NULL;
    }

    VpuDeinter *p = (VpuDeinter*)aVpuDeint;

    prctl(PR_SET_NAME, (unsigned long)"Deinterlace", 0, 0, 0);
    p->thread_running = true;
    bool deInterRet = true;
    uint32_t frame_count = 0;
    VPU_API_DEBUG("deinterlace %p work thread created", p);

    while ((p->thread_running) && (!p->deint_status)) {
        pthread_mutex_lock(&p->reset_mutex);
        if (p->thread_reset) {
            p->flush_deinter();
            p->thread_reset = 0;
            pthread_cond_signal(&p->reset_cond);
        }
        pthread_mutex_unlock(&p->reset_mutex);

        pthread_mutex_lock(&p->pre_deint_mutex);
        int32_t can_deinter = 0;
        if (!p->thread_running) {
            pthread_mutex_unlock(&p->pre_deint_mutex);
            break;
        }

        frame_count = 0;
        if (0 == p->deint_perform(DEINT_GET_FRAME_COUNT, &frame_count)) {
            //if (frame_count < DEFAULT_FRAME_BUFFER_DEPTH) {
                can_deinter = 1;
            //}
        }
        pthread_mutex_unlock(&p->pre_deint_mutex);

        if (can_deinter) {
            if (p->deinterlaceProc() <0) {
                p->deint_status = DEINTER_ERR_UNKNOW;
                p->thread_running =0;
                break;
            }

            if (p->mDeintCtx.poll_flag) {
                p->deinterlacePoll();
            } else {
                usleep(1000);
            }
        }else{
            usleep(1000);
        }
    }

    VPU_API_DEBUG("codec %p work thread end", aVpuDeint);
    return NULL;
}

class VpuApi {
public:
    VpuApi();
    ~VpuApi();

    int32_t init(VpuCodecContext *ctx, uint8_t *extraData, uint32_t extra_size);
    int32_t needOpenThread(VpuCodecContext *ctx);
    int32_t startThreadIfNecessary(VpuCodecContext *ctx);
    int32_t flush(VpuCodecContext *ctx);
    int32_t on2ResetWrapper(VpuCodecContext *ctx);
    int32_t decode(VpuCodecContext *ctx, VideoPacket_t *pkt, DecoderOut_t *aDecOut);
    int32_t decode_sendstream(VpuCodecContext *ctx, VideoPacket_t *pkt);
    int32_t decode_getoutframe(VpuCodecContext *ctx,DecoderOut_t *aDecOut);
    int32_t preProcessPacket(VpuCodecContext *ctx, VideoPacket_t *pkt);

    int32_t encode(VpuCodecContext *ctx, EncInputStream_t *aEncInStrm, EncoderOut_t *aEncOut);

    int32_t encoder_sendframe(VpuCodecContext *ctx,  EncInputStream_t *aEncInStrm);
    int32_t encoder_getstream(VpuCodecContext *ctx, EncoderOut_t *aEncOut);

    int32_t send_stream(uint8_t* buf,  uint32_t size, int64_t timestamp, int32_t usePts);
    int32_t get_frame(uint8_t* buf,  uint32_t* size, int64_t &timestamp);

    int32_t perform(uint32_t cmd, uint32_t *data);
    int32_t control(VpuCodecContext *ctx,VPU_API_CMD cmd,void *param);

    int32_t check();
private:

    pthread_cond_t  stream_cond;
    pthread_mutex_t stream_mutex;
    rk_list *stream;

    rk_list *frame;
    int32_t initFlag;
    int32_t mNALLengthSize;

    pthread_t thread;
	int32_t thread_running;

    int codec_status;

    int reset_delaydone;
    pthread_cond_t  reset_cond;
    pthread_mutex_t reset_mutex;
	int32_t thread_reset;

    VPU_API *mOn2Api;
    VpuApiPrivate *mPrivate;
    void *mOn2Handle;
    On2DecExt_t mOn2Extion;
    rk_list *time_list;
    On2M2vTimeAmend_t       mAmendTime;
    int32_t mEncoderFlag;
    int32_t mEosSet;
	int32_t mApiFlags;

    rk_list *enc_outstream;

    const char *mVersion;
    VpuDeinter *mVpuDeinter;

    static int32_t stream_destroy(stream_packet* stream);
    static int32_t frame_destroy(VPU_FRAME *frame);

    static int32_t outstream_destroy(EncoderOut_t *aEncOut);

	static void* codec_thread(void *me);

    int32_t On2CodecInit(VpuCodecContext *ctx);
    bool    On2Decode(VpuCodecContext *ctx, On2DecInput_t* on2DecInput, On2DecOutput_t* on2DecOutput);
    int32_t On2FlushFrameInDpb(VpuCodecContext *ctx);
    int32_t parseNALSize(const uint8_t *data) const;
    int32_t AvcDataParser(uint8_t **inputBuffer, uint8_t *tmpBuffer, uint32_t *inputLen);
    int32_t RvDataParser(uint8_t **inputBuffer,uint8_t *tmpBuffer,uint32_t *inputLen,int64_t timeUs);
    int32_t Vc1DataParser(VpuCodecContext *ctx, uint8_t **inputBuffer,uint8_t *tmpBuffer,uint32_t *inputLen);
    int64_t amendMpeg2TsIfNecessary(int64_t curFrmtimeUs);

    int32_t get_stream(stream_packet *packet);
    int32_t put_frame(VPU_FRAME* p);
    int32_t thread_start(VpuCodecContext *ctx);
    int32_t thread_stop();

    void    enableDeinterlace();
};

VpuApi::VpuApi()
    : stream(NULL),
      frame(NULL),
      initFlag(false),
      mNALLengthSize(0),
      thread_running(0),
      codec_status(VPU_API_OK),
      reset_delaydone(1),
      thread_reset(0),
      mOn2Api(NULL),
      mPrivate(NULL),
      mOn2Handle(NULL),
      time_list(NULL),
      mEncoderFlag(0),
      mEosSet(0),
      mApiFlags(0),
      enc_outstream(NULL),
      mVpuDeinter(NULL)
{
    VPU_API_DEBUG("VpuApi construct in");
    memset(&mOn2Extion, 0, sizeof(mOn2Extion));
    memset(&mAmendTime, 0, sizeof(On2M2vTimeAmend_t));
    mAmendTime.needReset == true;
    mPrivate = new VpuApiPrivate();

#if VPU_API_WRITE_DATA_DEBUG
    if (mPrivate) {
        mPrivate->openDebugFile("/data/video/vc1.bin");
    }
#endif
}

VpuApi::~VpuApi()
{
    VPU_API_DEBUG("~VpuApi in");
    thread_stop();

    if (mOn2Api) {
        if(mOn2Handle){
            if(!mEncoderFlag){
                mOn2Api->deinit_class_On2Decoder(mOn2Handle);
                mOn2Api->destroy_class_On2Decoder(mOn2Handle);
            }else{
                mOn2Api->deinit_class_On2Encoder(mOn2Handle);
                mOn2Api->destroy_class_On2Encoder(mOn2Handle);
            }
            mOn2Handle = NULL;
        }
        free(mOn2Api);
        mOn2Api = NULL;
    }

    if (mPrivate) {
        delete mPrivate;
        mPrivate = NULL;
    }

    if (mVpuDeinter) {
        delete mVpuDeinter;
        mVpuDeinter = NULL;
    }

    if (stream) delete stream;
    pthread_cond_destroy(&stream_cond);
    pthread_mutex_destroy(&stream_mutex);

    pthread_cond_destroy(&reset_cond);
    pthread_mutex_destroy(&reset_mutex);

    if (frame)  delete frame;
    if (time_list) delete time_list;

    if (enc_outstream) delete enc_outstream;

    VPU_API_DEBUG("VpuApi deinit done\n");
}

int32_t VpuApi::init(VpuCodecContext *ctx, uint8_t *extraData, uint32_t extra_size)
{
    VPU_API_DEBUG("VpuApi init in, extra_size: %d", extra_size);

    if ((NULL == ctx) || (NULL ==mPrivate)) {
        return VPU_API_ERR_UNKNOW;
    }

    mVersion = SF_COMPILE_INFO;

    if (initFlag == true) {
        return 0;
    }

    mOn2Api = (VPU_API*)malloc(sizeof(VPU_API));
    if (NULL == mOn2Api) {
        codec_status = VPU_API_ERR_INIT;
        VPU_API_ERROR("found fatal error when creating on2 api handle");
    }
    memset(mOn2Api, 0, sizeof(VPU_API));

    switch (ctx->codecType) {
        case CODEC_DECODER:
        {
            if (extraData && extra_size) {
                ctx->extradata = (uint8_t*)malloc(extra_size);
                if (NULL ==ctx->extradata) {
                    return VPU_API_ERR_UNKNOW;
                }
                memset(ctx->extradata, 0, extra_size);
                memcpy(ctx->extradata, extraData, extra_size);
                ctx->extradata_size = extra_size;
            }
            break;
        }
        case CODEC_ENCODER:
            mEncoderFlag = 1;
            break;

        default:
            codec_status = VPU_API_ERR_INIT;
            return codec_status;
    }

    int32_t ret =0;

    if(ctx->codecType == CODEC_ENCODER){
        switch (ctx->videoCoding) {
            case OMX_ON2_VIDEO_CodingAVC:
            case OMX_ON2_VIDEO_CodingMJPEG:
            case OMX_ON2_VIDEO_CodingVP8:
                ret = mPrivate->on2_api_init(mOn2Api, ctx->videoCoding);
            break;

            default:
                codec_status = VPU_API_ERR_INIT;
                return codec_status;
            }

        if (ret) {
            codec_status = VPU_API_ERR_INIT;
            VPU_API_ERROR("vpu api init fail");
            return ret;
        }

        ret = On2CodecInit(ctx);
        enc_outstream = new rk_list((node_destructor)outstream_destroy);
        if (NULL == enc_outstream) {
           codec_status = VPU_API_ERR_LIST_STREAM;
           VPU_API_ERROR("found fatal error when creating enc_outstream list");
        }
        initFlag = true;
        VPU_API_DEBUG("enc init ready ret %d", ret);
        return ret;

    }
    switch (ctx->videoCoding) {
        case OMX_ON2_VIDEO_CodingAVC:
        case OMX_ON2_VIDEO_CodingMJPEG:
        case OMX_ON2_VIDEO_CodingVP8:
        case OMX_ON2_VIDEO_CodingMPEG2:
        case OMX_ON2_VIDEO_CodingH263:
        case OMX_ON2_VIDEO_CodingMPEG4:
        case OMX_ON2_VIDEO_CodingDIVX3:
        case OMX_ON2_VIDEO_CodingWMV:
        case OMX_ON2_VIDEO_CodingVC1:
        case OMX_ON2_VIDEO_CodingRV:
        case OMX_ON2_VIDEO_CodingVP6:
        case OMX_ON2_VIDEO_CodingFLV1:
            if (mPrivate->isSupportUnderCfg(ctx->videoCoding)) {
                ret = mPrivate->on2_api_init(mOn2Api, ctx->videoCoding);
                mPrivate->setVideoCoding(ctx->videoCoding);
                if (ctx->videoCoding ==OMX_ON2_VIDEO_CodingMJPEG) {
                    ctx->enableparsing =0;
                }
            } else {
                VPU_API_ERROR("coding: %d, not support under user config", ctx->videoCoding);
                ret = VPU_API_ERR_INIT;
            }
            break;

        default:
            codec_status = VPU_API_ERR_INIT;
            return codec_status;
    }

    if (ret) {
        codec_status = VPU_API_ERR_INIT;
        VPU_API_ERROR("vpu api init fail");
        return ret;
    }

    ret = On2CodecInit(ctx);
    if (ret) {
        codec_status = VPU_API_ERR_ON2_CODEC_INIT;
        VPU_API_DEBUG("on2 codec init error while in vpu api init \n");
        return ret;
    }

    stream = new rk_list((node_destructor)stream_destroy);
    if (NULL == stream) {
        codec_status = VPU_API_ERR_LIST_STREAM;
        VPU_API_ERROR("found fatal error when creating stream list \n");
    }

    frame  = new rk_list((node_destructor)frame_destroy);
    if (NULL == frame) {
        codec_status = VPU_API_ERR_LIST_STREAM;
        VPU_API_ERROR("found fatal error when creating frame list \n");
    }

    time_list  = new rk_list((node_destructor)NULL);
    if (NULL == time_list) {
        codec_status = VPU_API_ERR_LIST_STREAM;
        VPU_API_ERROR("found fatal error when creating time list \n");
    }

    if ((ret = startThreadIfNecessary(ctx)) !=0) {
        codec_status = VPU_API_ERR_INIT;
        VPU_API_DEBUG("init error while in vpu api init \n");
    }

    ret = check();
    initFlag = true;

    VPU_API_DEBUG("dec data is ready, init complete check %d \n", ret);
    return ret;
}

int32_t VpuApi::needOpenThread(VpuCodecContext *ctx)
{
    if(ctx->no_thread){
        return 0;
    }

    return 1;
}

int32_t VpuApi::startThreadIfNecessary(VpuCodecContext *ctx)
{
    if (ctx ==NULL) {
        return -1;
    }

    pthread_mutex_init(&stream_mutex, NULL);
    pthread_cond_init(&stream_cond, NULL);

    if (!needOpenThread(ctx)) {
        return 0;
    }

    int32_t ret =0;

    pthread_mutex_init(&reset_mutex, NULL);
    pthread_cond_init(&reset_cond, NULL);

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutexattr_destroy(&attr);

    thread_start(ctx);

    ret = check();
    return ret;
}

int32_t VpuApi::check()
{
    int32_t ret = 0;
    if (codec_status) {
        ret = codec_status;
        VPU_API_ERROR("check fail, codec_status: 0x%X", codec_status);

        if (stream) {
            delete stream;
            stream = NULL;
        }
        if (frame) {
            delete frame;
            frame = NULL;
        }
        if (time_list) {
            delete time_list;
            time_list = NULL;
        }

        thread_stop();
    }
    return ret;
}

int32_t VpuApi::send_stream(uint8_t* buf,  uint32_t size, int64_t timestamp, int32_t usePts)
{
    if (check()) return codec_status;

    // do not push too many stream buffer
    if (stream->list_size() > 4){
        usleep(1000);
        return -1;
    }

    stream_packet node;
    memset(&node, 0, sizeof(stream_packet));

    node.buf = (uint8_t*)malloc(size);
    if (node.buf == NULL) {
        // failed to malloc vpu_mem, just return and back again
        return -2;
    }

    node.size = size;
    memcpy((void *)node.buf, buf, size);
    node.timestamp = timestamp;
    node.usePts = usePts;

    pthread_mutex_lock(&stream_mutex);
    int32_t ret = stream->add_at_head(&node, sizeof(node));
    if (ret) {
        pthread_mutex_unlock(&stream_mutex);
        VPU_API_ERROR("failed to add node to stream list");
        if (node.buf) {
            free(node.buf);
            node.buf = NULL;
        }
        codec_status = VPU_API_ERR_LIST_STREAM;
        return VPU_API_ERR_LIST_STREAM;
    } else {
        pthread_cond_signal(&stream_cond);
        pthread_mutex_unlock(&stream_mutex);
        /*VPU_API_DEBUG("send_stream phy: 0x%.8x size %d timestamp %lld",
            node.mem.phy_addr, node.mem.size, timestamp);*/
    }
    return 0;
}

int32_t VpuApi::On2CodecInit(VpuCodecContext *ctx)
{
    VPU_API_DEBUG("\n On2CodecInit in \n");

    if (NULL ==ctx) {
        return VPU_API_ERR_UNKNOW;
    }

    if (check()) return codec_status;

    if ((NULL == ctx) || (NULL == mOn2Api)) {
        codec_status = VPU_API_ERR_INIT;
        return VPU_API_ERR_INIT;
    }

    switch (ctx->codecType) {
        case CODEC_DECODER:
        case CODEC_ENCODER:
            break;

        default:
            return VPU_API_ERR_INIT;
    }

    int ret =0;
    if(ctx->codecType == CODEC_ENCODER){
        mOn2Handle = mOn2Api->get_class_On2Encoder();
        if (NULL ==mOn2Handle) {
            VPU_API_ERROR("can not get on2 encoder handle");
            codec_status = VPU_API_ERR_INIT;
            return VPU_API_ERR_INIT;
        }
        if(!ctx->extradata){ //malloc data for sps pps
	    printf("   <%s>_%d \n", __func__, __LINE__);
            ctx->extradata = (uint8_t*)malloc(4*1024);
            if(NULL == ctx->extradata){
                VPU_API_ERROR("can not get on2 encoder handle");
                codec_status = VPU_API_ERR_INIT;
                return VPU_API_ERR_INIT;
            }
            memcpy(ctx->extradata, "\x00\x00\x00\x01", 4);
            ret = mOn2Api->init_class_On2Encoder(mOn2Handle,(EncParameter_t *)ctx->private_data,
                                                 ctx->extradata + 4,(RK_U32*)&ctx->extradata_size);
	    printf("   <%s>_%d  ret = %d \n", __func__, __LINE__, ret);
            if(!ret){
                ctx->extradata_size += 4;
            }
            return ret;
        }
        return 0;
   }

    /* decoder init process */
    VPU_GENERIC vpug;
    vpug.CodecType = 0;
    vpug.ImgWidth = ctx->width;
    vpug.ImgHeight = ctx->height;
    uint8_t *rcvdata = NULL;
    uint32_t rcvlen = 0;

    /* check width and height */
    if ((vpug.ImgWidth ==0) || (vpug.ImgHeight ==0)) {
        VPU_API_ERROR("\n video width(%d) * height(%d) is invalid \n",
            vpug.ImgWidth, vpug.ImgHeight);
    }

    mOn2Handle = mOn2Api->get_class_On2Decoder();
    if (NULL ==mOn2Handle) {
        VPU_API_ERROR("\n can not get on2 decoder handle\n");
        codec_status = VPU_API_ERR_INIT;
        return VPU_API_ERR_INIT;
    }

    VPU_API_DEBUG("\n On2CodecInit, videoCoding: 0x%X, width: %d, height: %d\n",
        ctx->videoCoding, ctx->width, ctx->height);

    switch(ctx->videoCoding) {
        case OMX_ON2_VIDEO_CodingMPEG4:
            vpug.CodecType = VPU_CODEC_DEC_MPEG4;
            ret = mOn2Api->init_class_On2Decoder_M4VH263(mOn2Handle, &vpug);
            break;

        case OMX_ON2_VIDEO_CodingDIVX3:
            vpug.CodecType = VPU_CODEC_DEC_DIVX3;
            vpug.ImgWidth = (ctx->width + 15)&(~15);
            vpug.ImgHeight = (ctx->height + 15)&(~15);
            ret = mOn2Api->init_class_On2Decoder_M4VH263(mOn2Handle, &vpug);
            break;

        case OMX_ON2_VIDEO_CodingWMV:
        case OMX_ON2_VIDEO_CodingVC1:
            if(ctx->enableparsing){
                rcvdata = (uint8_t*)malloc(ctx->extradata_size + 1024);
                if (NULL ==rcvdata) {
                    codec_status = VPU_API_ERR_INIT;
                    return VPU_API_ERR_INIT;
                }
                memset(rcvdata, 0, sizeof(ctx->extradata_size + 1024));

                vc1_extradata_proc(ctx, rcvdata, &rcvlen);

                ret = mOn2Api->init_class_On2Decoder_VC1(
                        mOn2Handle, rcvdata, rcvlen, ctx->extradata_size);
                free(rcvdata);
            }else{
                ret = mOn2Api->init_class_On2Decoder_VC1(
                        mOn2Handle, ctx->extradata, ctx->extradata_size, ctx->extra_cfg.vc1extra_size);
            }
            break;

        case OMX_ON2_VIDEO_CodingAVC:
            printf("\n   <%s>_%d \n", __func__, __LINE__);
            ret = mOn2Api->init_class_On2Decoder_AVC(mOn2Handle, ctx->extra_cfg.tsformat);
            break;

        case OMX_ON2_VIDEO_CodingVP6:
            ret = mOn2Api->init_class_On2Decoder_VP6(mOn2Handle, ctx->extra_cfg.vp6codeid);
            break;

        case OMX_ON2_VIDEO_CodingFLV1:
            vpug.CodecType = VPU_CODEC_DEC_SORESONSPARKLE;
            ret = mOn2Api->init_class_On2Decoder_M4VH263(mOn2Handle, &vpug);
            break;

        case OMX_ON2_VIDEO_CodingH263:
            vpug.CodecType = VPU_CODEC_DEC_H263;
            ret = mOn2Api->init_class_On2Decoder_M4VH263(mOn2Handle, &vpug);
            break;

        case OMX_ON2_VIDEO_CodingMJPEG:
            ctx->enableparsing =0;
            /* continue */
        default:
            if (mOn2Api->init_class_On2Decoder) {
                ret = mOn2Api->init_class_On2Decoder(mOn2Handle);
            } else {
                ret = VPU_API_ERR_INIT;
            }
            break;
    }
    printf("\n   <%s>_%d    ret = %d \n", __func__, __LINE__, ret);
    if(ret) {
        codec_status = VPU_API_ERR_INIT;
        return VPU_API_ERR_INIT;
    }

    if (ctx->videoCoding == OMX_ON2_VIDEO_CodingAVC && ctx->extradata_size) {
        uint32_t SpsLen = 0;
        uint32_t outLen = 0;
        int64_t outputTimeUs = 0;
        uint8_t *tmpSpsdata = (uint8_t*)malloc(ctx->extradata_size + 1024);
	printf("\n   <%s>_%d    ctx->extradata_size = %d \n", __func__, __LINE__, ctx->extradata_size);
        if (NULL ==tmpSpsdata) {
            codec_status = VPU_API_ERR_INIT;
            return VPU_API_ERR_INIT;
        }
        memset(tmpSpsdata, 0, ctx->extradata_size + 1024);

        avc_extradata_proc(tmpSpsdata, &SpsLen,
                ctx->extradata, ctx->extradata_size, &mNALLengthSize);

        if(SpsLen){
            printf("\n   <%s>_%d \n", __func__, __LINE__);
            if (mOn2Api->dec_oneframe_class_On2Decoder_WithTimeStamp(
                        mOn2Handle, tmpSpsdata, &outLen, tmpSpsdata, &SpsLen, &outputTimeUs) == -1) {
		printf("\n   <%s>_%d \n", __func__, __LINE__);
                codec_status = VPU_API_ERR_INIT;
		free(tmpSpsdata);
                return VPU_API_ERR_INIT;
            }
        }
        free(tmpSpsdata);
    } else if ((ctx->videoCoding == OMX_ON2_VIDEO_CodingRV) && (ctx->extradata_size)) {
        if(ctx->enableparsing){
            uint32_t inputlen = 0;
            uint32_t outputlen = 0;
            uint8_t *extradata = (uint8_t*)malloc(ctx->extradata_size+1024);
            if (NULL ==extradata) {
                codec_status = VPU_API_ERR_INIT;
                return VPU_API_ERR_INIT;
            }
            memset(extradata, 0, ctx->extradata_size + 1024);

            rv_extradata_proc(extradata, &inputlen, ctx->extradata, ctx->extradata_size);

            mOn2Api->set_width_Height_class_On2Decoder_RV(mOn2Handle, &vpug.ImgWidth, &vpug.ImgHeight);
            if(inputlen){
                if (mOn2Api->dec_oneframe_class_On2Decoder(
                        mOn2Handle, extradata, &outputlen, extradata, &inputlen ) == -1) {
                    codec_status = VPU_API_ERR_INIT;
                    return VPU_API_ERR_INIT;
                }
            }
            free(extradata);
        }
    }

    VPU_API_DEBUG("On2CodecInit OK");
    return 0;
}

bool VpuApi::On2Decode(VpuCodecContext *ctx, On2DecInput_t* on2DecInput, On2DecOutput_t* on2DecOutput)
{
    if ((ctx == NULL) || (on2DecInput == NULL) || (on2DecOutput == NULL) ||
            (on2DecInput->pStream == NULL)) {
        VPU_API_DEBUG("On2Decode, input parameter is invalid");
        return false;
    }

    if (mOn2Api == NULL) {
        return false;
    }

    uint32_t outLen =0;
    uint8_t* inputBuffer = on2DecInput->pStream;
    VPU_FRAME outVpuFrame;
    VPU_FRAME *pframe = &outVpuFrame;
    uint8_t *vpu_bitstream = NULL;
    bool skipErrFrame = false;
    int64_t outputTime = on2DecInput->timeUs;
	On2DecExt_t* pOn2Ext = &mOn2Extion;

    memset(pframe, 0, sizeof(VPU_FRAME));

    while(on2DecInput->dataLen > 3){
        memset(pframe, 0, sizeof(VPU_FRAME));
        outputTime = on2DecInput->timeUs;

        if ((ctx->videoCoding == OMX_ON2_VIDEO_CodingAVC) ||
                (ctx->videoCoding == OMX_ON2_VIDEO_CodingWMV) ||
                (ctx->videoCoding == OMX_ON2_VIDEO_CodingVC1)) {
            if (mOn2Api->dec_oneframe_class_On2Decoder_WithTimeStamp(mOn2Handle, (uint8_t*)pframe, &outLen,
                                                       inputBuffer, &on2DecInput->dataLen, &outputTime) == -1) {
                /*
                 ** check whether we have meet hardware decoder error.
                 ** if hardware decoder can not decode successfully,
                 ** try to use software decoder.
                */
                if (ctx->videoCoding == OMX_ON2_VIDEO_CodingAVC) {
                    if (pframe->ErrorInfo) {
                        if ((pOn2Ext->h264HwStatus.errFrmNum++) >=20) {
                            skipErrFrame = false;
                        } else {
                            skipErrFrame = true;
                        }
                        goto Hwdec_FAIL;
                    } else {
                        pOn2Ext->h264HwStatus.errFrmNum = 0;
                    }
                } else {
                    goto Hwdec_FAIL;
                }
            }
            if(!reset_delaydone){
                mOn2Api->reset_class_On2Decoder(mOn2Handle);
                reset_delaydone = 1;
                /*
                 ** skip this output frame when we need to reset while do seek.
                */
                skipErrFrame = true;
                goto Hwdec_FAIL;
            }
         } else if (ctx->videoCoding == OMX_ON2_VIDEO_CodingVP6 || ctx->videoCoding == OMX_ON2_VIDEO_CodingRV){
            if (mOn2Api->dec_oneframe_class_On2Decoder(mOn2Handle, (uint8_t*)pframe, &outLen,
                inputBuffer,&on2DecInput->dataLen) == -1) {
                goto Hwdec_FAIL;
            }
            on2DecInput->dataLen = 0;
         }else{
            if(ctx->enableparsing){
                uint32_t bitsLen = sizeof(VPU_BITSTREAM) + on2DecInput->dataLen;
                vpu_bitstream = (uint8_t *)malloc(sizeof(VPU_BITSTREAM) + on2DecInput->dataLen);
                if (vpu_bitstream == NULL) {
                    goto Hwdec_FAIL;
                }

                memset(vpu_bitstream, 0, sizeof(VPU_BITSTREAM) + on2DecInput->dataLen);

                if(ctx->videoCoding == OMX_ON2_VIDEO_CodingMPEG2) {
                    outputTime = amendMpeg2TsIfNecessary(outputTime);
                    outputTime = outputTime/1000;
                    if (mPrivate) {
                        mPrivate->m2v_fill_hdr(vpu_bitstream, inputBuffer, on2DecInput->dataLen,outputTime , 0, 0,0);
                    } else {
                        VPU_API_DEBUG("vpu private is invalid, do m2v filled header fail");
                        goto Hwdec_FAIL;
                    }
               }else{
                    if (mPrivate) {
                        mPrivate->video_fill_hdr(vpu_bitstream, inputBuffer, on2DecInput->dataLen,outputTime , 0, 0);
                    } else {
                        VPU_API_DEBUG("vpu private is invalid, do filled header fail");
                        goto Hwdec_FAIL;
                    }
               }

               if (mOn2Api->dec_oneframe_class_On2Decoder(mOn2Handle,(uint8_t*)pframe,&outLen,
                                                                       vpu_bitstream, &bitsLen) == -1) {
                    goto Hwdec_FAIL;
               }
            }else{
                if (mOn2Api->dec_oneframe_class_On2Decoder(mOn2Handle,(uint8_t*)pframe,&outLen,
                                                                       inputBuffer, &on2DecInput->dataLen) == -1) {
                    goto Hwdec_FAIL;
                }

            }

            if (vpu_bitstream != NULL) {
                free(vpu_bitstream);
                vpu_bitstream = NULL;
            }

            on2DecInput->dataLen = 0;
        }

        if (ctx->videoCoding == OMX_ON2_VIDEO_CodingMPEG2) {
            outLen = 0;
            mOn2Api->get_oneframe_class_On2Decoder(mOn2Handle, (uint8_t*)pframe, &outLen);
        }

        if (outLen) {
            /*
             ** check whether we have meet hardware decoder error.
             ** if hardware decoder can not decode successfully,
             ** try to use software decoder.
            */
            if (ctx->videoCoding == OMX_ON2_VIDEO_CodingMPEG2) {
                if (pframe->ErrorInfo) {
                    if ((pOn2Ext->m2vHwStatus.errFrmNum++) >=20) {
                        skipErrFrame = false;
                    } else {
                        skipErrFrame = true;
                    }
                    goto Hwdec_FAIL;
                } else {
                    pOn2Ext->m2vHwStatus.errFrmNum = 0;
                }
            }
			if (ctx->videoCoding == OMX_ON2_VIDEO_CodingAVC) {
				pOn2Ext->h264HwStatus.errFrmNum = 0;
			}
            if((on2DecInput->usePts ==0) && time_list && time_list->list_size() &&
                        (ctx->videoCoding == OMX_ON2_VIDEO_CodingAVC)){

                if (time_list->del_at_tail(&outputTime, sizeof(int64_t))) {
                    VPU_API_DEBUG("get time stamp from list fail");
                    outputTime = 0;
                }
            } else if((ctx->videoCoding == OMX_ON2_VIDEO_CodingMPEG2) ||
                        (ctx->videoCoding == OMX_ON2_VIDEO_CodingRV) ||
                        (ctx->videoCoding == OMX_ON2_VIDEO_CodingH263) ||
                        (ctx->videoCoding == OMX_ON2_VIDEO_CodingMPEG4) ||
                        (ctx->videoCoding == OMX_ON2_VIDEO_CodingDIVX3)){
                /* special set frame width and height for div3 */
                if (ctx->videoCoding == OMX_ON2_VIDEO_CodingDIVX3) {
                    pframe->DisplayWidth = ctx->width;
                    pframe->DisplayHeight = ctx->height;
                }
                outputTime = ((int64_t)(pframe->ShowTime.TimeHigh) <<32) | \
                        ((int64_t)(pframe->ShowTime.TimeLow));
                if (ctx->videoCoding !=OMX_ON2_VIDEO_CodingH263) {
                    outputTime *=1000;
                }
            }else if((ctx->videoCoding == OMX_ON2_VIDEO_CodingWMV) ||
                        (ctx->videoCoding == OMX_ON2_VIDEO_CodingVC1)){
                outputTime = ((int64_t)(pframe->ShowTime.TimeHigh) <<32) | \
                        ((int64_t)(pframe->ShowTime.TimeLow));
            }
            /*VPU_API_DEBUG("On2Decode decoded one frame ok, phyAddr: 0x%x, outputTime = %lld",
                pframe->vpumem.phy_addr, outputTime);*/
            pframe->ShowTime.TimeHigh = ((uint32_t)(outputTime >>32));
            pframe->ShowTime.TimeLow= ((uint32_t)(outputTime & 0xFFFFFFFF));

            /*
             ** currently only support interlaced filed mode deinterlace
             ** support after codec_thread has opened.
            */
            if (needOpenThread(ctx)) {
                if ((mApiFlags & ENABLE_DEINTERLACE_SUPPORT) &&
                        pframe->FrameType && (mVpuDeinter==NULL)) {
                    mVpuDeinter = new VpuDeinter();
                    if (mVpuDeinter && mVpuDeinter->init()) {
                        VPU_API_ERROR("init deinterlace processor fail");
                        goto Hwdec_FAIL;
                    }
                }
                if (pframe->FrameType && mVpuDeinter) {
                    if (mVpuDeinter->pushPreDeinterFrame((uint8_t*)pframe, sizeof(VPU_FRAME), outputTime)) {
                        VPU_API_DEBUG("pushPreDeinterFrame fail");
                        goto Hwdec_FAIL;
                    }
                } else {
                    put_frame(pframe);
                }
            } else {
                put_frame(pframe);
            }
            outLen = 0;
        }
    }

    return true;

Hwdec_FAIL:
    if (pframe) {
        if (pframe->vpumem.phy_addr) {
            VPUMemLink(&pframe->vpumem);
            VPUFreeLinear(&pframe->vpumem);
            pframe->vpumem.phy_addr = 0;
        }
    }
    if (vpu_bitstream) {
        free(vpu_bitstream);
        vpu_bitstream = NULL;
    }
    return skipErrFrame;
}

int32_t VpuApi::On2FlushFrameInDpb(VpuCodecContext *ctx)
{
    if ((ctx == NULL) || (mOn2Api ==NULL)) {
        VPU_API_DEBUG("On2FlushFrameInDpb, input parameter is invalid");
        return -1;
    }

    if (mOn2Api->flush_oneframe_in_dpb_class_On2Decoder ==NULL) {
        VPU_API_DEBUG("not impletement this method.");
        return 0;
    }

    VPU_FRAME outVpuFrame;
    VPU_FRAME *pframe = &outVpuFrame;
    uint32_t outLength =0;

    while (1) {
        memset(pframe, 0, sizeof(VPU_FRAME));
        outLength =0;
        if (mOn2Api->flush_oneframe_in_dpb_class_On2Decoder(
                    mOn2Handle, (uint8_t*)pframe, &outLength)) {
            break;
        } else {
            VPU_API_DEBUG("flush one frame frome dpb, times_low: %d, time_high: %d, dis_w: %d, dis_h: %d",
                pframe->ShowTime.TimeLow, pframe->ShowTime.TimeHigh,
                pframe->DisplayWidth, pframe->DisplayHeight);

            put_frame(pframe);
        }
    }

    return 0;
}

int32_t VpuApi::get_stream(stream_packet *packet)
{
    int32_t ret = -EINVAL;
    if (stream->list_size()) {
        ret = stream->del_at_tail(packet, sizeof(stream_packet));
    }
    return ret;
}

int32_t VpuApi::put_frame(VPU_FRAME* p)
{
    if (check()) return codec_status;

    int64_t timestamp = (int64_t)p->ShowTime.TimeHigh;
    timestamp = (timestamp << 32) + (int64_t)p->ShowTime.TimeLow;
    int32_t ret = frame->add_at_head(p, sizeof(VPU_FRAME));
    //VPU_API_DEBUG("put_frame timestamp %lld ret %d", timestamp, ret);
    return ret;
}

int32_t VpuApi::get_frame(uint8_t* buf,  uint32_t* size, int64_t &timestamp)
{
    if(!initFlag){
        *size = 0;
        return 0;
    }
    if (check()) return codec_status;

    if(!frame->list_size()){
        if(mEosSet){
            VPU_API_DEBUG("get frame return eos");
            return -1;
        }
    }
    if (frame->del_at_tail(buf, sizeof(VPU_FRAME))) {
        *size = 0;
    } else {
        VPU_FRAME *p = (VPU_FRAME*)buf;
        *size = sizeof(VPU_FRAME);
        timestamp = p->ShowTime.TimeHigh;
        timestamp = (timestamp << 32) + p->ShowTime.TimeLow;
    }
    return 0;
}

int32_t VpuApi::perform(uint32_t cmd, uint32_t *data)
{
    if (check()) return codec_status;
    int32_t ret = 0;
    switch (cmd) {
    case VPU_GET_STREAM_COUNT : {
        int32_t* count = (int32_t*)data;
        *count = stream->list_size();
    } break;
    case VPU_GET_FRAME_COUNT : {
        int32_t* count = (int32_t*)data;
        if (mVpuDeinter) {
            ret = mVpuDeinter->deint_perform(cmd, data);
        } else {
            *count = frame->list_size();
        }
    } break;
    default : {
        VPU_API_ERROR("invalid command %d", cmd);
        ret = -EINVAL;
    } break;
    }
    return ret;
}

int32_t VpuApi::stream_destroy(stream_packet* stream)
{
    if (stream ==NULL) {
        return 0;
    }
    if (stream->buf) {
        free(stream->buf);
        stream->buf = NULL;
    }

    return 0;
}
int32_t VpuApi::frame_destroy(VPU_FRAME *frame)
{
    VPUMemLink(&frame->vpumem);
    VPUFreeLinear(&frame->vpumem);
    return 0;
}

int32_t VpuApi::outstream_destroy(EncoderOut_t *aEncOut)
{
   if(aEncOut->data){
        free(aEncOut->data);
        aEncOut->data = NULL;
   }
    return 0;
}


int32_t VpuApi::thread_start(VpuCodecContext *ctx)
{
    VPU_API_DEBUG("thread_start");
    if (!thread_running) {
    	pthread_attr_t attr;
    	pthread_attr_init(&attr);
    	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        if (pthread_create(&thread, &attr, codec_thread, (void*)ctx)) {
            codec_status |= VPU_API_ERR_FATAL_THREAD;
        }
    	pthread_attr_destroy(&attr);
    }
    return 0;
}

int32_t VpuApi::thread_stop()
{
    VPU_API_DEBUG("thread_stop");
    if (thread_running) {
        pthread_mutex_lock(&stream_mutex);
        thread_running = 0;
        pthread_cond_signal(&stream_cond);
        pthread_mutex_unlock(&stream_mutex);
        void *dummy;
        pthread_join(thread, &dummy);
    }
    return 0;
}

void VpuApi::enableDeinterlace()
{
    mApiFlags |=ENABLE_DEINTERLACE_SUPPORT;
}

int32_t VpuApi::on2ResetWrapper(VpuCodecContext *ctx)
{
    VPU_API_DEBUG("on2ResetWrapper in");
    if (ctx ==NULL) {
        return VPU_API_ERR_UNKNOW;
    }

    if (check()) {
        VPU_API_DEBUG("on2ResetWrapper: invalid codec status 0x%x", codec_status);
        return 0;
    }

    stream->flush();
    frame->flush();

    if (mOn2Api) {
        if (time_list) {
            time_list->flush();
        }

        mApiFlags =0;
        mEosSet = 0;

        if ((ctx->videoCoding != OMX_ON2_VIDEO_CodingWMV) &&
            (ctx->videoCoding != OMX_ON2_VIDEO_CodingVC1)){
            VPU_API_DEBUG("no reset_delay reset");
            mOn2Api->reset_class_On2Decoder(mOn2Handle);
        } else {
            reset_delaydone = 0;
        }
        /*
         ** may be we are in seek, so reset mpeg2-amend time
         ** infomation if necessary.
        */
        memset(&mOn2Extion, 0, sizeof(mOn2Extion));
        mAmendTime.count =0;
        mAmendTime.originTimeUs =0;
        mAmendTime.needReset = true;
    }

    if (mVpuDeinter) {
        mVpuDeinter->flush();
    }

    return 0;
}

int32_t VpuApi::flush(VpuCodecContext *ctx)
{
    VPU_API_DEBUG("flush in");
    if (ctx ==NULL) {
        return VPU_API_ERR_UNKNOW;
    }

    if ((ctx->decoder_err) ||
            ((needOpenThread(ctx)) && (!thread_running))) {
        return 0;
    }

    if(!initFlag){
        return 0;
    }

    if (check()) {
        VPU_API_DEBUG("flush: invalid codec status 0x%x", codec_status);
        return 0;
    }

    if (needOpenThread(ctx)) {
        pthread_mutex_lock(&reset_mutex);

        pthread_mutex_lock(&stream_mutex);
        thread_reset = 1;
        pthread_cond_signal(&stream_cond);
        pthread_mutex_unlock(&stream_mutex);

        pthread_cond_wait(&reset_cond, &reset_mutex);
        pthread_mutex_unlock(&reset_mutex);
    } else {
        on2ResetWrapper(ctx);
    }
    return 0;
}

int32_t VpuApi::preProcessPacket(VpuCodecContext *ctx, VideoPacket_t *pkt)
{
    if ((ctx == NULL) || (pkt == NULL) || (pkt->data == NULL)) {
        VPU_API_DEBUG("decode, input parameter is invalid");
        return VPU_API_ERR_UNKNOW;
    }

    uint32_t stream_count = 0;

    if (0 == perform((uint32_t)VPU_GET_STREAM_COUNT, &stream_count) && stream_count < 4) {
        uint32_t inputLen = pkt->size;
        uint8_t *inputBuffer = pkt->data;
        uint8_t *tmpBuffer = NULL;
        int64_t inputTime = 0;
        On2DecExt_t* pOn2Ext = &mOn2Extion;

        if(pkt->pts != VPU_API_NOPTS_VALUE) {
            inputTime = pkt->pts;
        }else{
            inputTime = pkt->dts;
            if (time_list && time_list->add_at_head(&inputTime, sizeof(int64_t))) {
                VPU_API_DEBUG("push timestamp to list fail");
            }
        }
        if(ctx->videoCoding == OMX_ON2_VIDEO_CodingAVC){
            if(ctx->enableparsing){
                tmpBuffer = (uint8_t *)malloc(pkt->size + 1024);
                if (tmpBuffer == NULL) {
                    VPU_API_DEBUG("decode, malloc memory fail for tmp buffer");
                    return VPU_API_ERR_UNKNOW;
                }

                memset(tmpBuffer, 0, pkt->size + 1024);
                AvcDataParser(&inputBuffer,tmpBuffer,&inputLen);
            }
        }else if(ctx->videoCoding == OMX_ON2_VIDEO_CodingRV){

            if(ctx->enableparsing){
                uint32_t *p = (uint32_t *)inputBuffer;
                if(0x42564b52 == p[0]){ //check rkvb
                    ctx->enableparsing = 0;
                }else{
	                tmpBuffer = (uint8_t *)malloc(pkt->size + 1024);
	                if (tmpBuffer == NULL) {
	                    VPU_API_DEBUG("decode, malloc memory fail for tmp buffer");
	                    return VPU_API_ERR_UNKNOW;
	                }

	                memset(tmpBuffer, 0, pkt->size + 1024);
	                RvDataParser(&inputBuffer,tmpBuffer,&inputLen,inputTime);
                }
            }
        }else if((ctx->videoCoding == OMX_ON2_VIDEO_CodingVC1) && (pOn2Ext->headSend ==0)){
            if(ctx->enableparsing) {
                tmpBuffer = (uint8_t *)malloc(pkt->size + 1024);
                if (tmpBuffer == NULL) {
                    VPU_API_DEBUG("decode, malloc memory fail for tmp buffer");
                    return VPU_API_ERR_UNKNOW;
                }

                memset(tmpBuffer, 0, pkt->size + 1024);
                Vc1DataParser(ctx, &inputBuffer,tmpBuffer,&inputLen);
            } else {
                pOn2Ext->headSend =1;
            }
        }else if((pOn2Ext->headSend ==0) &&
                    ((ctx->videoCoding == OMX_ON2_VIDEO_CodingMPEG4) ||
                    (ctx->videoCoding == OMX_ON2_VIDEO_CodingDIVX3))){
            uint8_t *inBuf = inputBuffer;

            tmpBuffer = (uint8_t *)malloc(pkt->size + 1024);
            if (tmpBuffer == NULL) {
                VPU_API_DEBUG("decode, malloc memory fail for tmp buffer");
                return VPU_API_ERR_UNKNOW;
            }

            memset(tmpBuffer, 0, pkt->size + 1024);

            if(ctx->extradata_size > 1024){
                free(tmpBuffer);
                tmpBuffer = (uint8_t *)malloc(pkt->size + 1024 + ctx->extradata_size);
                if (tmpBuffer == NULL) {
                    VPU_API_DEBUG("decode, malloc memory fail for tmp buffer");
                    return VPU_API_ERR_UNKNOW;
                }

                memset(tmpBuffer, 0, pkt->size + 1024 + ctx->extradata_size);
            }

            if (ctx->extradata) {
                memcpy(tmpBuffer, ctx->extradata,ctx->extradata_size);
                memcpy(tmpBuffer + ctx->extradata_size, inBuf, inputLen);
                inputBuffer = tmpBuffer;
                inputLen += ctx->extradata_size;
            }

            pOn2Ext->headSend = 1;

        }

        int32_t usePts = 0;
        if (pkt->pts != VPU_API_NOPTS_VALUE) {
            usePts = 1;
        }

        if (0 == send_stream(inputBuffer, inputLen, inputTime, usePts)) {
            pkt->size = 0;
        }

        if (tmpBuffer) {
            free(tmpBuffer);
            tmpBuffer = NULL;
        }
    }else{
        usleep(1000);
    }

    return 0;
}

int32_t VpuApi::decode(VpuCodecContext *ctx, VideoPacket_t *pkt, DecoderOut_t *aDecOut)
{
    if ((ctx == NULL) || (pkt == NULL) || (aDecOut == NULL) ||
            (pkt->data == NULL) || (aDecOut->data == NULL)) {
        VPU_API_DEBUG("decode, input parameter is invalid");
        return VPU_API_ERR_UNKNOW;
    }
    if(ctx->decoder_err){
        return VPU_API_ERR_STREAM;
    }

    preProcessPacket(ctx, pkt);

    if (needOpenThread(ctx)) {
        if (mVpuDeinter) {
            return mVpuDeinter->getDeinterlaceFrame(aDecOut->data, &(aDecOut->size), aDecOut->timeUs);
        } else {
            return get_frame(aDecOut->data, &(aDecOut->size), aDecOut->timeUs);
        }
    } else {
        if (frame->list_size()) {
            return get_frame(aDecOut->data, &(aDecOut->size), aDecOut->timeUs);
        }

        stream_packet input;
        int decRet = 0;
        memset(&input, 0, sizeof(stream_packet));
        if (stream && stream->list_size()) {
            if (0 == get_stream(&input)) {
                // has a stream to decode
                On2DecInput_t decInput;
                On2DecOutput_t decOutput;
                memset(&decInput, 0, sizeof(On2DecInput_t));
                memset(&decOutput, 0, sizeof(On2DecOutput_t));
                decInput.pStream            = (uint8_t*)input.buf;
                decInput.dataLen            = input.size;
                decInput.timeUs             = input.timestamp;
                decInput.usePts             = input.usePts;
                decInput.picId              = 0;
                decInput.skipNonReference   = 0;
                //VPU_API_DEBUG("get a stream, phy 0x%.8x size %d", input.mem.phy_addr, input.mem.size);
                do {
                    decRet = On2Decode(ctx, &decInput, &decOutput);
                    if (decRet == false) {
                        ctx->decoder_err = 1;
                        break;
                    }
                } while (decInput.dataLen >= 2);
				if (input.buf) {
		            free(input.buf);
		            input.buf = NULL;
		        }
            } else {
                usleep(1000);
                VPU_API_ERROR("failed to get a stream with list_size %d", stream->list_size());
            }
        }

        return get_frame(aDecOut->data, &(aDecOut->size), aDecOut->timeUs);
    }

    return 0;
}

int32_t VpuApi:: decode_sendstream(VpuCodecContext *ctx, VideoPacket_t *pkt){
    if ((ctx == NULL) || (pkt == NULL)) {
        VPU_API_DEBUG("decode, input parameter is invalid");
        return VPU_API_ERR_UNKNOW;
    }

    if (check()) return codec_status;

    if(!initFlag){
        return VPU_API_ERR_INIT;
    }
    if(ctx->decoder_err){
        return VPU_API_ERR_STREAM;
    }
    if(pkt->nFlags & OMX_BUFFERFLAG_EOS){
        VPU_API_DEBUG("send eos ture");
        mApiFlags |=NEED_FLUSH_ON2_DPB;
        if (mVpuDeinter) {
            mVpuDeinter->deint_perform(DEINT_SET_INPUT_EOS_REACH, NULL);
        }
        pthread_cond_signal(&stream_cond);
    }
    if((pkt->data ==NULL) || (!pkt->size)){
        return 0;
    }

    preProcessPacket(ctx, pkt);

    return 0;
}

int32_t VpuApi:: decode_getoutframe(VpuCodecContext *ctx,DecoderOut_t *aDecOut){
    if ((ctx == NULL)|| (aDecOut == NULL) ||(aDecOut->data == NULL)) {
        VPU_API_DEBUG("decode, input parameter is invalid");
        return VPU_API_ERR_UNKNOW;
    }

    if (check()) return codec_status;

    if(!initFlag){
        aDecOut->size =0;
        return 0;
    }
    if (needOpenThread(ctx)) {
        if (mVpuDeinter) {
            return mVpuDeinter->getDeinterlaceFrame(aDecOut->data, &(aDecOut->size), aDecOut->timeUs);
        } else {
            return get_frame(aDecOut->data, &(aDecOut->size), aDecOut->timeUs);
        }
    } else {
        stream_packet input;
        int decRet = 0;
        memset(&input, 0, sizeof(stream_packet));
        if (stream && stream->list_size()) {
            if (0 == get_stream(&input)) {
                // has a stream to decode
                On2DecInput_t decInput;
                On2DecOutput_t decOutput;
                memset(&decInput, 0, sizeof(On2DecInput_t));
                memset(&decOutput, 0, sizeof(On2DecOutput_t));
                decInput.pStream            = input.buf;
                decInput.dataLen            = input.size;
                decInput.timeUs             = input.timestamp;
                decInput.usePts             = input.usePts;
                decInput.picId              = 0;
                decInput.skipNonReference   = 0;
                //VPU_API_DEBUG("get a stream, phy 0x%.8x size %d", input.mem.phy_addr, input.mem.size);
                do {
                    decRet = On2Decode(ctx, &decInput, &decOutput);
                    if (decRet == false) {
                        ctx->decoder_err = 1;
                        break;
                    }
                } while (decInput.dataLen >= 2);
				if (input.buf) {
		            free(input.buf);
		            input.buf = NULL;
		        }
            } else {
                usleep(1000);
                VPU_API_ERROR("failed to get a stream with list_size %d", stream->list_size());
            }
        }
        if (mApiFlags & NEED_FLUSH_ON2_DPB) {
            VPU_API_DEBUG("here will flush one frame from dpb");
            /*
             ** flush frames of decoders that in dpb to output frame list.
            */
            On2FlushFrameInDpb(ctx);
            mApiFlags &= ~NEED_FLUSH_ON2_DPB;
            mEosSet = 1;
        }

        return get_frame(aDecOut->data, &(aDecOut->size), aDecOut->timeUs);
    }
    return 0;
}

int32_t VpuApi::encode(VpuCodecContext *ctx, EncInputStream_t *aEncInStrm, EncoderOut_t *aEncOut)
{
    if ((ctx ==NULL) || (aEncInStrm == NULL) || (aEncOut == NULL)) {
        VPU_API_ERROR("encode frame fail, input param invalid");
        return -1;
    }
    int ret = 0;
    ret = mOn2Api->enc_oneframe_class_On2Encoder(mOn2Handle, aEncOut->data, (RK_U32*)&aEncOut->size,aEncInStrm->buf,
        aEncInStrm->bufPhyAddr,(RK_U32*)&aEncInStrm->size,(RK_U32*)&aEncOut->timeUs, &aEncOut->keyFrame);

    if (ret <0) {
        VPU_API_ERROR("encode frame fail, ret: %d", ret);
        return ret;
    }

    aEncInStrm->size =0;
    return 0;
}

int32_t VpuApi::encoder_sendframe(VpuCodecContext *ctx, EncInputStream_t *aEncInStrm){
    int ret = 0;
    EncoderOut_t aEncOut;
    if ((ctx == NULL) || (aEncInStrm == NULL)) {
        VPU_API_DEBUG("encoder, input parameter is invalid");
        return VPU_API_ERR_UNKNOW;
    }
    if(!initFlag){
        return VPU_API_ERR_INIT;
    }

    if(aEncInStrm->nFlags){
        VPU_API_DEBUG("send eos ture");
        mEosSet = 1;
    }
    if(!aEncInStrm->size){
        return 0;
    }
    memset(&aEncOut,0,sizeof(EncoderOut_t));
    aEncOut.data = (uint8_t*)malloc(MAX_STREAM_LENGHT);
    if(aEncOut.data ==NULL){
        ALOGE("malloc for encoder output stream error");
        return VPU_API_ERR_STREAM;
    }
  //  ALOGE("send encoder address 0x%x", aEncInStrm->bufPhyAddr);
    ret = mOn2Api->enc_oneframe_class_On2Encoder(mOn2Handle, aEncOut.data, (RK_U32*)&aEncOut.size,aEncInStrm->buf,
        aEncInStrm->bufPhyAddr,(RK_U32*)&aEncInStrm->size,(RK_U32*)&aEncOut.timeUs, &aEncOut.keyFrame);

 //   ALOGE("send encoder out size %d", aEncOut.size);
    if(aEncOut.size > 0){
        aEncOut.timeUs = aEncInStrm->timeUs;
        if(enc_outstream != NULL){
            enc_outstream->add_at_head(&aEncOut,sizeof(EncoderOut_t));
        }else{
            ALOGE("enc_outstream push list fail");
        }
    }else{
        free(aEncOut.data);
    }

    return 0;
}

int32_t VpuApi::encoder_getstream(VpuCodecContext *ctx,EncoderOut_t *aEncOut){

    if ((ctx == NULL)|| (aEncOut == NULL)) {
        VPU_API_DEBUG("encoder, input parameter is invalid");
        return VPU_API_ERR_UNKNOW;
    }

    if(!initFlag){
        return 0;
    }

    if (check()) return codec_status;

    memset(aEncOut,0,sizeof(aEncOut));
    if(enc_outstream == NULL){
        ALOGE("enc_outstream no malloc");
    }
    if(!enc_outstream->list_size()){
        if(mEosSet){
            VPU_API_DEBUG("get frame return eos");
            return -1;
        }
    }
   // ALOGE("enc_outstream->list_size() = %d",enc_outstream->list_size());
    if(enc_outstream->list_size()){
    enc_outstream->del_at_tail(aEncOut, sizeof(EncoderOut_t));
    }
    return 0;
}


void *VpuApi::codec_thread(void *aCtx)
{
    if (aCtx ==NULL) {
        return NULL;
    }

    VpuCodecContext* ctx = (VpuCodecContext*)aCtx;
    VpuApi *p = (VpuApi*)ctx->vpuApiObj;
    if (p ==NULL) {
        return NULL;
    }

    stream_packet input;
    p->thread_running = true;
    bool decRet = true;
    uint32_t frame_count = 0;
    memset(&input, 0, sizeof(stream_packet));
    VPU_API_DEBUG("codec %p work thread created", aCtx);


    while ((p->thread_running) && (!ctx->decoder_err)) {
        //VPU_API_DEBUG("stream list size %d", p->stream->list_size());
        pthread_mutex_lock(&p->reset_mutex);
        if (p->thread_reset) {
            p->on2ResetWrapper(ctx);
            p->thread_reset = 0;
            pthread_cond_signal(&p->reset_cond);
        }

        /*
         ** fix for mediastress async multiple flush, cause reset_condition and
         ** stream_condition dead lock.
        */
        pthread_mutex_lock(&p->stream_mutex);
        pthread_mutex_unlock(&p->reset_mutex);

        int32_t can_decode = 0;
        if (!p->thread_running) {
            pthread_mutex_unlock(&p->stream_mutex);
            break;
        }

        frame_count = 0;
        if (0 == p->perform(VPU_GET_FRAME_COUNT, &frame_count)) {
            if (frame_count < DEFAULT_FRAME_BUFFER_DEPTH) {
                if (0 == p->get_stream(&input)) {
                    can_decode = 1;
                } else {
                    if(p->mApiFlags & NEED_FLUSH_ON2_DPB){
                        /*
                         ** flush frames of decoders that in dpb to output frame list.
                        */
                        usleep(40000);
                        p->On2FlushFrameInDpb(ctx);
                        p->mApiFlags &= ~NEED_FLUSH_ON2_DPB;
                        p->mEosSet= 1;
                    }
                    pthread_cond_wait(&p->stream_cond, &p->stream_mutex);
                }
            }
        }
        pthread_mutex_unlock(&p->stream_mutex);

        if (can_decode) {
            // has a stream to decode
            On2DecInput_t decInput;
            On2DecOutput_t decOutput;
            memset(&decInput, 0, sizeof(On2DecInput_t));
            memset(&decOutput, 0, sizeof(On2DecOutput_t));
            decInput.pStream            = input.buf;
            decInput.dataLen            = input.size;
            decInput.timeUs             = input.timestamp;
            decInput.usePts             = input.usePts;
            decInput.picId              = 0;
            decInput.skipNonReference   = 0;
            //VPU_API_DEBUG("get a stream, phy 0x%.8x size %d", input.mem.phy_addr, input.mem.size);
            do {
                decRet = p->On2Decode(ctx, &decInput, &decOutput);
                if (decRet == false) {
                    ctx->decoder_err = 1;
                    p->thread_running =0;
                    p->codec_status = VPU_API_ERR_UNKNOW;
                    VPU_API_DEBUG("on2 decoder one frame fail, decRet: 0x%X", decRet);
                    break;
                }
            } while (decInput.dataLen >= 2);
			if (input.buf) {
	            free(input.buf);
	            input.buf = NULL;
        	}
        }else{
            usleep(1000);
        }
    }

    VPU_API_DEBUG("codec %p work thread end", aCtx);

    return NULL;
}

int32_t VpuApi::parseNALSize(const uint8_t *data) const {
    switch (mNALLengthSize) {
        case 1:
            return *data;
        case 2:
            return U16_AT(data);
        case 3:
            return ((size_t)data[0] << 16) | U16_AT(&data[1]);
        case 4:
            return U32_AT(data);
    }
    return 0;
}

int32_t VpuApi::AvcDataParser(uint8_t **inputBuffer, uint8_t *tmpBuffer, uint32_t *inputLen)
{
    if ((inputBuffer == NULL) || (tmpBuffer == NULL) || (inputLen == NULL)) {
        VPU_API_DEBUG("AvcDataParser, input parameter is invalid");
        return -1;
    }

    uint8_t *inBuffer = *inputBuffer;
    if (mNALLengthSize >0) {
        uint8_t *dstData = tmpBuffer;
        size_t srcOffset = 0;
        size_t dstOffset = 0;
        while (srcOffset < *inputLen) {
            bool isMalFormed = (srcOffset + mNALLengthSize > *inputLen);
            size_t nalLength = 0;
            if (!isMalFormed) {
                nalLength = parseNALSize(&inBuffer[srcOffset]);
                srcOffset += mNALLengthSize;
                isMalFormed = nalLength > *inputLen;
            }

            if (isMalFormed) {
                VPU_API_DEBUG("Video is malformed");
                break;
            }

            if (nalLength == 0) {
                break;
            }
            dstData[dstOffset++] = 0;
            dstData[dstOffset++] = 0;
            dstData[dstOffset++] = 0;
            dstData[dstOffset++] = 1;
            memcpy(&dstData[dstOffset], &inBuffer[srcOffset], nalLength);
            srcOffset += nalLength;
            dstOffset += nalLength;
       }
       *inputBuffer = tmpBuffer;
       *inputLen = dstOffset;
    } else if (inBuffer[0] == 0x00 && inBuffer[1] == 0x00) {
        size_t srcOffset = 0;
        size_t dstOffset = 0;
        uint8_t* tmp_ptr = *inputBuffer;
        uint8_t* buffer_begin = tmpBuffer;
        int32_t length = 0;
        int initbufsize = *inputLen;
        int tConfigSize = 0;
        do {
            tmp_ptr += length;
            length = GetNAL_Config(&tmp_ptr, &initbufsize);
            uint32_t NalTag = 0x00000001;
            if ((tmp_ptr[0] & 0x1f) != 6) {
                buffer_begin[0] = (NalTag >> 24) & 0xFF;
                buffer_begin[1] = (NalTag >> 16) & 0xFF;
                buffer_begin[2] = (NalTag >> 8) & 0xFF;
                buffer_begin[3] = (NalTag ) & 0xFF;
                memcpy(buffer_begin + 4, tmp_ptr, length);
                buffer_begin += (length + 4);
                tConfigSize += (length + 4);
            }
        }
        while (initbufsize > 0);
        *inputBuffer = tmpBuffer;
        *inputLen = tConfigSize;
    }

    return 0;
}

int32_t VpuApi::RvDataParser(uint8_t **inputBuffer,uint8_t *tmpBuffer,uint32_t *inputLen,int64_t timeUs){
    if ((inputBuffer == NULL) || (tmpBuffer == NULL) || (inputLen == NULL)) {
        VPU_API_DEBUG("RvDataParser, input parameter is invalid");
        return -1;
    }

    uint8_t *inBuffer = *inputBuffer;
    Stream_describe *strd = (Stream_describe *)tmpBuffer;
    int slices = inBuffer[0] + 1;
    int i;
    uint8_t *dstData;

    for (i=1; i < slices; i++) {
        int s0 = 8 * i - 3;
        int s1 = 8 * (i+1) - 3;
        uint32_t* tmp1 = (uint32_t*)&inBuffer[s0];
        uint32_t* tmp2 = (uint32_t*)&inBuffer[s1];
        //VPU_API_DEBUG("slice length 0x%x",*tmp2 - *tmp1 );
        strd->slicesize[i-1] = BSWAP(*tmp2 - *tmp1);
    }
    int s =  8 * (slices) - 3;
    uint32_t* tmp1 = (uint32_t*)&inBuffer[s];
    strd->slicesize[slices-1] =  *inputLen - slices * 8 - 1 - *tmp1;
    //VPU_API_DEBUG("slice length 0x%x",strd->slicesize[slices-1]);

    strd->flag[0] = 'R';
    strd->flag[1] = 'K';
    strd->flag[2] = 'V';
    strd->flag[3] = 'B';
    strd->retFlag = 0;

    strd->size   = *inputLen - slices * 8 - 1;
    strd->slices = slices;
    strd->timel  = (uint32_t)(timeUs/1000);
    strd->timeh  =  0;
    strd->type   = 0x20;

    dstData = inBuffer + 1 + slices * 8;

    memcpy(tmpBuffer+sizeof(Stream_describe),dstData,strd->size);
    *inputBuffer = tmpBuffer;
    *inputLen = sizeof(Stream_describe)+strd->size;

    return 0;
}

int32_t VpuApi::Vc1DataParser(VpuCodecContext *ctx, uint8_t **inputBuffer,uint8_t *tmpBuffer,uint32_t *inputLen) {
    if ((inputBuffer == NULL) || (tmpBuffer == NULL) || (inputLen == NULL)) {
        VPU_API_DEBUG("Vc1DataParser, input parameter is invalid");
        return -1;
    }

    uint8_t *inBuf = *inputBuffer;
    On2DecExt_t* pOn2Ext = &mOn2Extion;

    if ((pOn2Ext->headSend ==0) && ctx && ctx->extradata) {
        memcpy(tmpBuffer, ctx->extradata, ctx->extradata_size);
        memcpy(tmpBuffer + ctx->extradata_size, inBuf, *inputLen);
        *inputBuffer = tmpBuffer;
        *inputLen += ctx->extradata_size;
    }

    pOn2Ext->headSend = 1;
    return 0;
}

int64_t VpuApi::amendMpeg2TsIfNecessary(int64_t curFrmtimeUs) {
    int64_t pts = curFrmtimeUs;

    if (curFrmtimeUs <0) {
       curFrmtimeUs = mAmendTime.lastTimeUs;
    }

    if (abs(mAmendTime.lastTimeUs - pts) >2000000) {
        if (mAmendTime.count++ ==0) {
            if (mAmendTime.needReset ==true) {
                mAmendTime.originTimeUs = pts;
                mAmendTime.needReset = false;
            } else {
                mAmendTime.originTimeUs =mAmendTime.lastTimeUs;
            }

            if (mAmendTime.frameDelayUs ==0) {
                mAmendTime.frameDelayUs = 33000;
            }
        }

        if (mAmendTime.count <=5) {
            /*
             ** use orgin time as current pts.
            */
            pts = mAmendTime.originTimeUs;
        } else {
            mAmendTime.count =0;
            mAmendTime.originTimeUs =0;
            mAmendTime.needReset = true;
        }
    } else {
        mAmendTime.count =0;
        mAmendTime.originTimeUs =0;
    }

    mAmendTime.lastTimeUs = pts;
    mAmendTime.needReset = false;

    return pts;
}
int32_t VpuApi::control(VpuCodecContext *ctx,VPU_API_CMD cmd,void *param){
    if (NULL == ctx) {
        return VPU_API_ERR_UNKNOW;
    }

    if (check()) return codec_status;

    switch(cmd){
        case VPU_API_ENC_SETCFG:
        {
            mOn2Api->enc_setconfig_class_On2Encoder(mOn2Handle,(EncParameter_t*)param);
            break;
        }

        case VPU_API_ENC_GETCFG:
        {
            mOn2Api->enc_getconfig_class_On2Encoder(mOn2Handle,(EncParameter_t*)param);
            break;
        }

        case VPU_API_ENC_SETFORMAT:
        {
            mOn2Api->enc_setInputFormat_class_On2Encoder(mOn2Handle,*((H264EncPictureType *)param));
            break;
        }

        case VPU_API_ENC_SETIDRFRAME:
        {
            mOn2Api->enc_setIdrframe_class_On2Encoder(mOn2Handle);
            break;
        }

        case VPU_API_ENABLE_DEINTERLACE:
        {
            if (ctx->codecType ==CODEC_DECODER) {
                enableDeinterlace();
            }
            break;
        }
        default:
            break;
    }

    return 0;
}

static int32_t vpu_api_init(VpuCodecContext *ctx, uint8_t *extraData, uint32_t extra_size)
{
    VPU_API_DEBUG("vpu_api_init in, extra_size: %d", extra_size);

    if (ctx == NULL) {
        VPU_API_DEBUG("vpu_api_init fail, input invalid");
        return VPU_API_ERR_UNKNOW;
    }

    VpuApi* api = (VpuApi*)(ctx->vpuApiObj);
    if (api == NULL) {
        VPU_API_DEBUG("vpu_api_init fail, vpu api invalid");
        return VPU_API_ERR_UNKNOW;
    }

    return api->init(ctx, extraData, extra_size);
}

static int32_t vpu_api_decode(VpuCodecContext *ctx, VideoPacket_t *pkt, DecoderOut_t *aDecOut)
{
    if (ctx == NULL) {
        VPU_API_DEBUG("vpu_api_decode fail, input invalid");
        return VPU_API_ERR_UNKNOW;
    }

    VpuApi* api = (VpuApi*)(ctx->vpuApiObj);
    if (api == NULL) {
        VPU_API_DEBUG("vpu_api_decode fail, vpu api invalid");
        return VPU_API_ERR_UNKNOW;
    }

    return api->decode(ctx, pkt, aDecOut);
}
static int32_t vpu_api_sendstream(VpuCodecContext *ctx, VideoPacket_t *pkt)
{
    if (ctx == NULL) {
        VPU_API_DEBUG("vpu_api_decode fail, input invalid");
        return VPU_API_ERR_UNKNOW;
    }

    VpuApi* api = (VpuApi*)(ctx->vpuApiObj);
    if (api == NULL) {
        VPU_API_DEBUG("vpu_api_sendstream fail, vpu api invalid");
        return VPU_API_ERR_UNKNOW;
    }

    return api->decode_sendstream(ctx, pkt);
}

static int32_t vpu_api_getframe(VpuCodecContext *ctx, DecoderOut_t *aDecOut)
{
    if (ctx == NULL) {
        VPU_API_DEBUG("vpu_api_decode fail, input invalid");
        return VPU_API_ERR_UNKNOW;
    }

    VpuApi* api = (VpuApi*)(ctx->vpuApiObj);
    if (api == NULL) {
        VPU_API_DEBUG("vpu_api_getframe fail, vpu api invalid");
        return VPU_API_ERR_UNKNOW;
    }

    return api->decode_getoutframe(ctx,aDecOut);
}

static int32_t vpu_api_sendframe(VpuCodecContext *ctx, EncInputStream_t *aEncInStrm)
{
    if (ctx == NULL) {
        VPU_API_DEBUG("vpu_api_decode fail, input invalid");
        return VPU_API_ERR_UNKNOW;
    }

    VpuApi* api = (VpuApi*)(ctx->vpuApiObj);
    if (api == NULL) {
        VPU_API_DEBUG("vpu_api_sendframe fail, vpu api invalid");
        return VPU_API_ERR_UNKNOW;
    }

    return api->encoder_sendframe(ctx, aEncInStrm);
}

static int32_t vpu_api_getstream(VpuCodecContext *ctx, EncoderOut_t *aEncOut)
{
    if (ctx == NULL) {
        VPU_API_DEBUG("vpu_api_decode fail, input invalid");
        return VPU_API_ERR_UNKNOW;
    }

    VpuApi* api = (VpuApi*)(ctx->vpuApiObj);
    if (api == NULL) {
        VPU_API_DEBUG("vpu_api_getframe fail, vpu api invalid");
        return VPU_API_ERR_UNKNOW;
    }

    return api->encoder_getstream(ctx,aEncOut);
}



static int32_t vpu_api_encode(VpuCodecContext *ctx, EncInputStream_t *aEncInStrm, EncoderOut_t *aEncOut)
{
    if (ctx == NULL) {
        VPU_API_DEBUG("vpu_api_encode fail, input invalid");
        return VPU_API_ERR_UNKNOW;
    }

    VpuApi* api = (VpuApi*)(ctx->vpuApiObj);
    if (api == NULL) {
        VPU_API_DEBUG("vpu_api_encode fail, vpu api invalid");
        return VPU_API_ERR_UNKNOW;
    }

    return api->encode(ctx, aEncInStrm, aEncOut);
}

static int32_t vpu_api_flush(VpuCodecContext *ctx)
{
    if (ctx == NULL) {
        VPU_API_DEBUG("vpu_api_encode fail, input invalid");
        return VPU_API_ERR_UNKNOW;
    }

    VpuApi* api = (VpuApi*)(ctx->vpuApiObj);
    if (api == NULL) {
        VPU_API_DEBUG("vpu_api_flush fail, vpu api invalid");
        return VPU_API_ERR_UNKNOW;
    }

    return api->flush(ctx);
}

static int32_t vpu_api_control(VpuCodecContext *ctx, VPU_API_CMD cmdType,void *param)
{
    if (ctx == NULL) {
        VPU_API_DEBUG("vpu_api_decode fail, input invalid");
        return VPU_API_ERR_UNKNOW;
    }

    VpuApi* api = (VpuApi*)(ctx->vpuApiObj);
    if (api == NULL) {
        VPU_API_DEBUG("vpu_api_decode fail, vpu api invalid");
        return VPU_API_ERR_UNKNOW;
    }

    return api->control(ctx, cmdType, param);
}
extern "C"
int32_t vpu_open_context(VpuCodecContext **ctx)
{
    VPU_API_DEBUG("vpu_open_context in");
    VpuCodecContext *s = *ctx;

    if (!s) {
        s = (VpuCodecContext*)malloc(sizeof(VpuCodecContext));
        if (!s) {
            VPU_API_ERROR("Input context has not been properly allocated");
            return -1;
        }
        memset(s, 0, sizeof(VpuCodecContext));
        s->enableparsing = 1;

        VpuApi* api = new VpuApi();
        if (api == NULL) {
            VPU_API_ERROR("Vpu api object has not been properly allocated");
            return -1;
        }

        s->vpuApiObj = (void*)api;
        s->init = vpu_api_init;
        s->decode = vpu_api_decode;
        s->encode = vpu_api_encode;
        s->flush = vpu_api_flush;
        s->control = vpu_api_control;
        s->decode_sendstream = vpu_api_sendstream;
        s->decode_getframe = vpu_api_getframe;
        s->encoder_sendframe = vpu_api_sendframe;
        s->encoder_getstream = vpu_api_getstream;

        *ctx = s;
        return 0;
    }

    if (!s->vpuApiObj){
        VPU_API_ERROR("Input context has not been "
                "properly allocated and is not NULL either");
        return -1;
    }
    return 0;
}

extern "C"
int32_t vpu_close_context(VpuCodecContext **ctx)
{
    VPU_API_DEBUG("vpu_close_context in");
    VpuCodecContext *s = *ctx;

    if (s) {
        VpuApi* api = (VpuApi*)(s->vpuApiObj);
        if (s->vpuApiObj) {
            delete api;
            s->vpuApiObj = NULL;
        }
        if (s->extradata) {
            free(s->extradata);
            s->extradata = NULL;
        }
        if(s->private_data){
            free(s->private_data);
            s->private_data = NULL;
        }
        free(s);
        s = NULL;
        *ctx = s;

        VPU_API_DEBUG("vpu_close_context ok");
        return 0;
    }
    return 0;
}

