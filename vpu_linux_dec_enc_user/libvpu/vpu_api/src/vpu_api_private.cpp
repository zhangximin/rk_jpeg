#include "vpu_api_private.h"
#include "vpu_api.h"
//#include <cutils/properties.h>
#include "ctype.h"

#define  LOG_TAG "vpu_api_private"
#include <utils/Log.h>
#include <utils/List.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define _VPU_API_PRIVATE_DEBUG

#ifdef  _VPU_API_PRIVATE_DEBUG
#ifdef AVS40
#define API_PRIVATE_DEBUG           LOGD
#else
#define API_PRIVATE_DEBUG           ALOGD
#endif
#else
#define API_PRIVATE_DEBUG
#endif

#ifdef AVS40
#define API_PRIVATE_ERROR           LOGE
#else
#define API_PRIVATE_ERROR           ALOGE
#endif

static const char* MEDIA_RK_CFG_DEC_MPEG2   = "media.rk.cfg.dec.mpeg2";
static const char* MEDIA_RK_CFG_DEC_H263    = "media.rk.cfg.dec.h263";
static const char* MEDIA_RK_CFG_DEC_MPEG4   = "media.rk.cfg.dec.mpeg4";
static const char* MEDIA_RK_CFG_DEC_WMV     = "media.rk.cfg.dec.wmv";
static const char* MEDIA_RK_CFG_DEC_RV      = "media.rk.cfg.dec.rv";
static const char* MEDIA_RK_CFG_DEC_AVC     = "media.rk.cfg.dec.h264";
static const char* MEDIA_RK_CFG_DEC_MJPEG   = "media.rk.cfg.dec.mjpeg";
static const char* MEDIA_RK_CFG_DEC_VC1     = "media.rk.cfg.dec.vc1";
static const char* MEDIA_RK_CFG_DEC_FLV1    = "media.rk.cfg.dec.flv1";
static const char* MEDIA_RK_CFG_DEC_DIV3    = "media.rk.cfg.dec.div3";
static const char* MEDIA_RK_CFG_DEC_VP8     = "media.rk.cfg.dec.vp8";
static const char* MEDIA_RK_CFG_DEC_VP6     = "media.rk.cfg.dec.vp6";

static On2CodecConfigure_t on2_codec_cfg[] = {
    {CODEC_DECODER,     OMX_ON2_VIDEO_CodingMPEG2,      "M2V",          MEDIA_RK_CFG_DEC_MPEG2, CFG_VALUE_UNKNOW},
    {CODEC_DECODER,     OMX_ON2_VIDEO_CodingH263,       "M4vH263",      MEDIA_RK_CFG_DEC_H263,  CFG_VALUE_UNKNOW},
    {CODEC_DECODER,     OMX_ON2_VIDEO_CodingMPEG4,      "M4vH263",      MEDIA_RK_CFG_DEC_MPEG4, CFG_VALUE_UNKNOW},
    {CODEC_DECODER,     OMX_ON2_VIDEO_CodingWMV,        "VC1",          MEDIA_RK_CFG_DEC_WMV, CFG_VALUE_UNKNOW},
    {CODEC_DECODER,     OMX_ON2_VIDEO_CodingRV,         "RV",           MEDIA_RK_CFG_DEC_RV,    CFG_VALUE_UNKNOW},
    {CODEC_DECODER,     OMX_ON2_VIDEO_CodingAVC,        "AVC",          MEDIA_RK_CFG_DEC_AVC, CFG_VALUE_UNKNOW},
    {CODEC_DECODER,     OMX_ON2_VIDEO_CodingMJPEG,      "NULL",         MEDIA_RK_CFG_DEC_MJPEG, CFG_VALUE_UNKNOW},
    {CODEC_DECODER,     OMX_ON2_VIDEO_CodingVC1,        "VC1",          MEDIA_RK_CFG_DEC_VC1, CFG_VALUE_UNKNOW},
    {CODEC_DECODER,     OMX_ON2_VIDEO_CodingFLV1,       "FLV",          MEDIA_RK_CFG_DEC_FLV1,  CFG_VALUE_UNKNOW},
    {CODEC_DECODER,     OMX_ON2_VIDEO_CodingDIVX3,      "M4vH263",      MEDIA_RK_CFG_DEC_DIV3,  CFG_VALUE_UNKNOW},
    {CODEC_DECODER,     OMX_ON2_VIDEO_CodingVP8,        "VPX",          MEDIA_RK_CFG_DEC_VP8,   CFG_VALUE_UNKNOW},
    {CODEC_DECODER,     OMX_ON2_VIDEO_CodingVP6,        "VPX",          MEDIA_RK_CFG_DEC_VP6,   CFG_VALUE_UNKNOW},
};

uint8_t VpuApiPrivate::mHaveParseCfg = 0;

VpuApiPrivate::VpuApiPrivate()
{
    mDebugFile = NULL;
    memset(&mCodecCfg, 0, sizeof(VpuApiCodecCfg_t));
    mCoding = OMX_ON2_VIDEO_CodingUnused;
 }

VpuApiPrivate::~VpuApiPrivate()
{
    if (mDebugFile) {
        fclose(mDebugFile);
        mDebugFile = NULL;
    }
}

int32_t VpuApiPrivate::on2_api_init(VPU_API *on2_api, uint32_t video_coding_type)
{
    if (on2_api == NULL) {
        API_PRIVATE_ERROR("on2_api_init fail, input parameter invalid\n");
        return -1;
    }

    memset(on2_api, 0, sizeof(VPU_API));

    switch (video_coding_type) {
    case OMX_ON2_VIDEO_CodingAVC:
        on2_api->get_class_On2Decoder                = get_class_On2AvcDecoder;
        on2_api->destroy_class_On2Decoder            = destroy_class_On2AvcDecoder;
        on2_api->init_class_On2Decoder_AVC           = init_class_On2AvcDecoder;
        on2_api->deinit_class_On2Decoder             = deinit_class_On2AvcDecoder;
        on2_api->dec_oneframe_class_On2Decoder_WithTimeStamp       = dec_oneframe_class_On2AvcDecoder;
        on2_api->reset_class_On2Decoder              = reset_class_On2AvcDecoder;
        on2_api->flush_oneframe_in_dpb_class_On2Decoder = flush_oneframe_in_dpb_class_On2AvcDecoder;

        on2_api->get_class_On2Encoder                = get_class_On2AvcEncoder;
        on2_api->destroy_class_On2Encoder            = destroy_class_On2AvcEncoder;
        on2_api->init_class_On2Encoder               = init_class_On2AvcEncoder;
        on2_api->deinit_class_On2Encoder             = deinit_class_On2AvcEncoder;
        on2_api->enc_oneframe_class_On2Encoder       = enc_oneframe_class_On2AvcEncoder;
        on2_api->enc_setconfig_class_On2Encoder      = set_config_class_On2AvcEncoder;
        on2_api->enc_getconfig_class_On2Encoder      = get_config_class_On2AvcEncoder;
        on2_api->enc_setInputFormat_class_On2Encoder = set_inputformat_class_On2AvcEncoder;
        on2_api->enc_setIdrframe_class_On2Encoder    = set_idrframe_class_On2AvcEncoder;
        break;
/*
    case OMX_ON2_VIDEO_CodingMPEG4:
    case OMX_ON2_VIDEO_CodingDIVX3:
        on2_api->get_class_On2Decoder                = get_class_On2M4vDecoder;
        on2_api->destroy_class_On2Decoder            = destroy_class_On2M4vDecoder;
        on2_api->reset_class_On2Decoder              = reset_class_On2M4vDecoder;
        on2_api->init_class_On2Decoder_M4VH263       = init_class_On2M4vDecoder;
        on2_api->deinit_class_On2Decoder             = deinit_class_On2M4vDecoder;
        on2_api->dec_oneframe_class_On2Decoder       = dec_oneframe_class_On2M4vDecoder;
        on2_api->flush_oneframe_in_dpb_class_On2Decoder = flush_oneframe_in_dpb_class_On2M4vDecoder;
        break;
    case OMX_ON2_VIDEO_CodingH263:
    case OMX_ON2_VIDEO_CodingFLV1:
        on2_api->get_class_On2Decoder                = get_class_On2H263Decoder;
        on2_api->destroy_class_On2Decoder            = destroy_class_On2H263Decoder;
        on2_api->reset_class_On2Decoder              = reset_class_On2H263Decoder;
        on2_api->init_class_On2Decoder_M4VH263       = init_class_On2H263Decoder;
        on2_api->deinit_class_On2Decoder             = deinit_class_On2H263Decoder;
        on2_api->dec_oneframe_class_On2Decoder       = dec_oneframe_class_On2H263Decoder;
        on2_api->flush_oneframe_in_dpb_class_On2Decoder = flush_oneframe_in_dpb_class_On2H263Decoder;
        break;
    case OMX_ON2_VIDEO_CodingMPEG2:
        on2_api->get_class_On2Decoder                = get_class_On2M2vDecoder;
        on2_api->destroy_class_On2Decoder            = destroy_class_On2M2vDecoder;
        on2_api->reset_class_On2Decoder              = reset_class_On2M2vDecoder;
        on2_api->init_class_On2Decoder               = init_class_On2M2vDecoder;
        on2_api->deinit_class_On2Decoder             = deinit_class_On2M2vDecoder;
        on2_api->dec_oneframe_class_On2Decoder       = dec_oneframe_class_On2M2vDecoder;
        on2_api->get_oneframe_class_On2Decoder       = get_oneframe_class_On2M2vDecoder;
        break;
    case OMX_ON2_VIDEO_CodingRV:
        on2_api->get_class_On2Decoder                = get_class_On2RvDecoder;
        on2_api->destroy_class_On2Decoder            = destroy_class_On2RvDecoder;
        on2_api->reset_class_On2Decoder              = reset_class_On2RvDecoder;
        on2_api->init_class_On2Decoder               = init_class_On2RvDecoder;
        on2_api->deinit_class_On2Decoder             = deinit_class_On2RvDecoder;
        on2_api->dec_oneframe_class_On2Decoder       = dec_oneframe_class_On2RvDecoder;
        on2_api->set_width_Height_class_On2Decoder_RV = set_width_Height_class_On2RvDecoder;
        break;
    case OMX_ON2_VIDEO_CodingVP8:
        on2_api->get_class_On2Decoder                = get_class_On2Vp8Decoder;
        on2_api->destroy_class_On2Decoder            = destroy_class_On2Vp8Decoder;
        on2_api->reset_class_On2Decoder              = reset_class_On2Vp8Decoder;
        on2_api->init_class_On2Decoder               = init_class_On2Vp8Decoder;
        on2_api->deinit_class_On2Decoder             = deinit_class_On2Vp8Decoder;
        on2_api->dec_oneframe_class_On2Decoder       = dec_oneframe_class_On2Vp8Decoder;

        on2_api->get_class_On2Encoder                = get_class_On2Vp8Encoder;
        on2_api->destroy_class_On2Encoder            = destroy_class_On2Vp8Encoder;
        on2_api->init_class_On2Encoder               = init_class_On2Vp8Encoder;
        on2_api->deinit_class_On2Encoder             = deinit_class_On2Vp8Encoder;
        on2_api->enc_oneframe_class_On2Encoder       = enc_oneframe_class_On2Vp8Encoder;
        break;
    case OMX_ON2_VIDEO_CodingWMV:
    case OMX_ON2_VIDEO_CodingVC1:
        on2_api->get_class_On2Decoder                = get_class_On2Vc1Decoder;
        on2_api->destroy_class_On2Decoder            = destroy_class_On2Vc1Decoder;
        on2_api->reset_class_On2Decoder              = reset_class_On2Vc1Decoder;
        on2_api->init_class_On2Decoder_VC1           = init_class_On2Vc1Decoder;
        on2_api->deinit_class_On2Decoder             = deinit_class_On2Vc1Decoder;
        on2_api->dec_oneframe_class_On2Decoder_WithTimeStamp       = dec_oneframe_class_On2Vc1Decoder;
        break;
    case OMX_ON2_VIDEO_CodingVP6:
        on2_api->get_class_On2Decoder                = get_class_On2Vp6Decoder;
        on2_api->destroy_class_On2Decoder            = destroy_class_On2Vp6Decoder;
        on2_api->reset_class_On2Decoder              = reset_class_On2Vp6Decoder;
        on2_api->init_class_On2Decoder_VP6           = init_class_On2Vp6Decoder;
        on2_api->deinit_class_On2Decoder             = deinit_class_On2Vp6Decoder;
        on2_api->dec_oneframe_class_On2Decoder       = dec_oneframe_class_On2Vp6Decoder;
        break;
*/
     case OMX_ON2_VIDEO_CodingMJPEG:
        on2_api->get_class_On2Decoder                = get_class_On2JpegDecoder;
        on2_api->destroy_class_On2Decoder            = destroy_class_On2JpegDecoder;
        on2_api->init_class_On2Decoder               = init_class_On2JpegDecoder;
        on2_api->deinit_class_On2Decoder             = deinit_class_On2JpegDecoder;
        on2_api->dec_oneframe_class_On2Decoder       = dec_oneframe_class_On2JpegDecoder;
        on2_api->reset_class_On2Decoder              = reset_class_On2JpegDecoder;
        on2_api->get_class_On2Encoder                = get_class_On2MjpegEncoder;
    	on2_api->destroy_class_On2Encoder            = destroy_class_On2MjpegEncoder;
    	on2_api->init_class_On2Encoder               = init_class_On2MjpegEncoder;
    	on2_api->deinit_class_On2Encoder             = deinit_class_On2MjpegEncoder;
    	on2_api->enc_oneframe_class_On2Encoder       = enc_oneframe_class_On2MjpegEncoder;
		break;		

    default:
        API_PRIVATE_ERROR("on2_api_init fail, unsupport coding type\n");
        return -1;
    }

    return 0;
}

int32_t VpuApiPrivate::video_fill_hdr(uint8_t *dst,uint8_t *src, uint32_t size,
            int64_t time, uint32_t type, uint32_t num)
{
    if ((NULL ==dst) || (NULL ==src)) {
        API_PRIVATE_ERROR("video_fill_hdr fail, input parameter invalid\n");
        return -1;
    }

    VPU_BITSTREAM h;
    uint32_t TimeLow = 0;
    if (mCoding == OMX_ON2_VIDEO_CodingH263) {
        TimeLow = (uint32_t)(time);
    } else {
        TimeLow = (uint32_t)(time/1000);
    }
    h.StartCode = BSWAP(VPU_BITSTREAM_START_CODE);
    h.SliceLength= BSWAP(size);
    h.SliceTime.TimeLow = BSWAP(TimeLow);
    h.SliceTime.TimeHigh= 0;
    h.SliceType= BSWAP(type);
    h.SliceNum= BSWAP(num);
    h.Res[0] = 0;
    h.Res[1] = 0;
    memcpy(dst, &h, sizeof(VPU_BITSTREAM));
    memcpy((uint8_t*)(dst + sizeof(VPU_BITSTREAM)),src, size);
    return 0;
}

int32_t VpuApiPrivate::get_line(VpuApiCodecCfg_t *s, uint8_t *buf, uint32_t maxlen)
{
    if ((s == NULL) || (buf == NULL)) {
        return 0;
    }

    int32_t i = 0;
    uint8_t c;

    do {
        c = *(s->buf + s->cfg_file.read_pos++);
        if (c && i < maxlen-1)
            buf[i++] = c;
    } while (c != '\n' && c);

    buf[i] = 0;
    return i;
}

int32_t VpuApiPrivate::read_chomp_line(VpuApiCodecCfg_t* s, uint8_t *buf, uint32_t maxlen)
{
    int len = get_line(s, buf, maxlen);
    while (len > 0 && isspace(buf[len - 1]))
        buf[--len] = '\0';
    return len;
}

int32_t VpuApiPrivate::parseStagefrightCfgFile(VpuApiCodecCfg_t* cfg)
{
    if (cfg == NULL) {
        API_PRIVATE_ERROR("parseStagefrightCfgFile fail, input parameter invalid\n");
        return -1;
    }

    uint8_t line[512], tmp[100];
    uint32_t size =0, ret =0, n =0, k =0;
    uint32_t tag_read =0;
    VpuApiCodecCfg_t* p = cfg;
    FILE* file = p->cfg_file.file;
    On2CodecConfigure_t* on2_cfg = NULL;
    char *c = NULL, *s = NULL;
    char quote = '"';

    fseek(file, 0, SEEK_END);
    p->cfg_file.file_size = size = ftell(file);
    fseek(file, 0, SEEK_SET);
    API_PRIVATE_DEBUG("stagefright cfg file size: %d",
        p->cfg_file.file_size);
    if (size >=1024*1024) {
        API_PRIVATE_ERROR("parseStagefrightCfgFile fail, config file "
                "size: %d too large, max support 1M\n", size);
        return -1;
    }
    if (p->buf) {
        free(p->buf);
        p->buf = NULL;
    }
    p->buf = (uint8_t*)malloc(size);
    if (p->buf == NULL) {
        API_PRIVATE_ERROR("alloc memory fail while parse cfg file\n");
        ret = -1;
        goto PARSE_CFG_OUT;
    }
    memset(p->buf, 0, size);

    if ((n = fread(p->buf, 1, size, file)) !=size) {
        API_PRIVATE_ERROR("read bytes from parse cfg file fail\n");
        ret = -1;
        goto PARSE_CFG_OUT;
    }

    size =0;
    while (size <p->cfg_file.file_size) {
        if ((n = read_chomp_line(p, line, sizeof(line))) <0) {
            break;
        }

        API_PRIVATE_DEBUG("read one line, size: %d, : %s\n", n, line);
        if (n ==0) {
            if (p->cfg_file.read_pos >=p->cfg_file.file_size) {
                break;
            }

            continue;
        }
        if (!tag_read) {
            if (strstr((char*)line, "rockchip_stagefright_config")) {
                continue;
            }
            if (!strstr((char*)line, "<Decoders>")) {
                API_PRIVATE_ERROR("parseStagefrightCfgFile fail, invalid tag\n");
                ret = -1;
                break;
            }

            tag_read = 1;
            continue;
        }
        if (!(s =strstr((char*)line, "media.rk.cfg.dec."))) {
            if (tag_read && (strstr((char*)line, "</Decoders>") ||
                                strstr((char*)line, "</rockchip_stagefright_config>"))) {
                break;
            }
            API_PRIVATE_ERROR("parseStagefrightCfgFile fail, invalid line: %s\n", line);
            ret = -1;
            break;
        } else  {
            for (k =0; k <=strlen(s); k++) {
                if (s[k] ==quote) {
                    break;
                }
            }
            if (k <sizeof(tmp)) {
                memset(tmp, 0, sizeof(tmp));
                strncpy((char*)tmp, s, k);
            }

            if (on2_cfg = getStagefrightCfgByName(CODEC_DECODER, tmp)) {
                if (strstr((char*)line, "support=\"yes\"")) {
                    on2_cfg->value = CFG_VALUE_SUPPORT;
                } else {
                    on2_cfg->value = CFG_VALUE_NOT_SUPPORT;
                }
            }
        }

        size +=n;
    }

PARSE_CFG_OUT:
    if (p->buf) {
        free(p->buf);
        p->buf = NULL;
    }

    return ret;
}

On2CodecConfigure_t* VpuApiPrivate::getStagefrightCfgByCoding(
            CODEC_TYPE codec_type,
            uint32_t video_coding_type)
{
    uint32_t i =0, cfgCnt = 0;
    uint32_t coding = video_coding_type;

    cfgCnt = sizeof(on2_codec_cfg) / sizeof(On2CodecConfigure_t);
    for (; i <=(cfgCnt); i++) {
        if ((coding == on2_codec_cfg[i].videoCoding) &&
                (codec_type == on2_codec_cfg[i].codecType)) {
            return &on2_codec_cfg[i];
        }
    }

    return NULL;
}

On2CodecConfigure_t* VpuApiPrivate::getStagefrightCfgByName(
        CODEC_TYPE codec_type, uint8_t* name)
{
    uint32_t i =0, cfgCnt = 0;
    cfgCnt = sizeof(on2_codec_cfg) / sizeof(On2CodecConfigure_t);
    for (; i <=(cfgCnt); i++) {
        if ((!strncasecmp(on2_codec_cfg[i].cfg,
                (char*)name, strlen(on2_codec_cfg[i].cfg))) &&
                (codec_type == on2_codec_cfg[i].codecType)) {
            return &on2_codec_cfg[i];
        }
    }

    return NULL;
}

int32_t VpuApiPrivate::isSupportUnderCfg(uint32_t video_coding_type)
{
    //char value[PROPERTY_VALUE_MAX];
    //char substring[PROPERTY_VALUE_MAX];
    On2CodecConfigure_t* cfg = NULL;
    int32_t support =0;
    API_PRIVATE_DEBUG("isSupportUnderCfg in");

    if (mHaveParseCfg ==0) {
        if (read_user_config()) {
            API_PRIVATE_DEBUG("not read user's config, go old config process");
            cfg = getStagefrightCfgByCoding(CODEC_DECODER, video_coding_type);
            goto OLD_CFG;
        }
    }

    if ((cfg = getStagefrightCfgByCoding(CODEC_DECODER, video_coding_type)) !=NULL) {
        if (cfg->value == CFG_VALUE_SUPPORT) {
            support =1;
        } else if (cfg->value == CFG_VALUE_NOT_SUPPORT) {
            return 0;
        }
    }

OLD_CFG:

    API_PRIVATE_DEBUG("go old config process, support: %d", support);
    if ((support ==0) && cfg) {
        /*
        //if (property_get("media.decoder.cfg", value, NULL)) 
        {
            API_PRIVATE_DEBUG("value: %s, old_cfg: %s", value, (char*)(cfg->old_cfg));
            if (strstr(value, (char*)(cfg->old_cfg))) {
                support = 1;
            }
        } 
        //else {
        //    // user not set any codec config, default support it. 
        //    support = 1;
        //}
        */
        support = 1;
    }

    API_PRIVATE_DEBUG("isSupportUnderCfg out, support: %d", support);
    return support;
}

int32_t VpuApiPrivate::read_user_config()
{
    VpuApiCodecCfg_t* codecCfg = &mCodecCfg;
    if (mHaveParseCfg ==1) {
        return 0;
    }

    FILE *file = fopen("/system/etc/media_stagefright.cfg", "r");
    if (file == NULL) {
        API_PRIVATE_DEBUG("unable to open media codecs configuration file.");
        return -1;
    }

    int32_t ret =0;
    codecCfg->cfg_file.file = file;
    if ((ret = parseStagefrightCfgFile(codecCfg)) <0) {
        API_PRIVATE_ERROR("parse media_stagefright.cfg fail");
    }

    if (file) {
        fclose(file);
        file = NULL;
    }

    mHaveParseCfg = 1;
    return ret;
}

int32_t VpuApiPrivate::m2v_fill_hdr(uint8_t *dst, uint8_t *src, uint32_t size,
            int64_t time, uint32_t type, uint32_t num, uint32_t retFlag)
{
    if ((NULL ==dst) || (NULL ==src) || (size ==0)) {
        API_PRIVATE_ERROR("m2v_fill_hdr fail, input parameter invalid\n");
        return -1;
    }

    VPU_BITSTREAM h;
    uint32_t TimeLow = (uint32_t)(time);
    h.StartCode = BSWAP(VPU_BITSTREAM_START_CODE);
    h.SliceLength = size;
    h.SliceTime.TimeLow  = (uint32_t)time;
    h.SliceTime.TimeHigh = 0;
    h.SliceType = type;
    h.SliceNum = num;

    memcpy(dst, &h, sizeof(VPU_BITSTREAM));
    memcpy((uint8_t*)(dst + sizeof(VPU_BITSTREAM)),src, size);
    return 0;
}

int32_t VpuApiPrivate::openDebugFile(const char *path)
{
    if (mDebugFile) {
        fclose(mDebugFile);
        mDebugFile = NULL;
    }

    mDebugFile = fopen(path, "wb");
    return mDebugFile == NULL ? -1 : 0;
}

int32_t VpuApiPrivate::writeToDebugFile(uint8_t* data, uint32_t size)
{
    if ((data == NULL) || (size ==0)) {
        return -1;
    }

    if (mDebugFile) {
        fwrite(data, 1, size, mDebugFile);
        fflush(mDebugFile);
    }

    return 0;
}

void VpuApiPrivate::setVideoCoding(uint32_t coding)
{
    mCoding = coding;
}
/* for rkffplayer */
extern "C"
void vpu_api_init(VPU_API *vpu_api, OMX_ON2_VIDEO_CODINGTYPE video_coding_type)
{
    int32_t ret = VpuApiPrivate::on2_api_init(vpu_api, video_coding_type);
}


