#include "stdio.h"
#include "vpu_global.h"
#include "utils/Log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vpu_api.h"

#undef LOG_TAG
#define LOG_TAG "vpu_api_demo"

static RK_U32 VPU_API_DEMO_DEBUG_DISABLE = 0;

#define VPU_API_DEMO_DEBUG 1

typedef char	yc_char;

#ifdef  VPU_API_DEMO_DEBUG
#ifdef AVS40
#define VPU_DEMO_LOG(...)   do { if (VPU_API_DEMO_DEBUG_DISABLE==0) LOGI(__VA_ARGS__); } while (0)
#else
#define VPU_DEMO_LOG(...)   do { if (VPU_API_DEMO_DEBUG_DISABLE==0) printf(__VA_ARGS__); } while (0)
#endif
#else
#define VPU_DEMO_LOG
#endif

#define BSWAP32(x) \
    ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) | \
      (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))


#define DEMO_ERR_RET(err) do { ret = err; goto DEMO_OUT; } while (0)
#define DECODE_ERR_RET(err) do { ret = err; goto DECODE_OUT; } while (0)
#define ENCODE_ERR_RET(err) do { ret = err; goto ENCODE_OUT; } while (0)


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

typedef struct VpuApiDemoCmdContext {
    RK_U32  width;
    RK_U32  height;
    CODEC_TYPE  codec_type;
    OMX_ON2_VIDEO_CODINGTYPE coding;
    RK_U8   input_file[200];
    RK_U8   output_file[200];
    RK_U8   have_input;
    RK_U8   have_output;
    RK_U8   disable_debug;
    RK_U32  record_frames;
    RK_S64  record_start_ms;
}VpuApiDemoCmdContext_t;

typedef struct VpuApiEncInput {
    EncInputStream_t stream;
    RK_U32 capability;
}VpuApiEncInput;


static void show_usage()
{
    VPU_DEMO_LOG("usage: vpu_apiDemo [options] input_file, \n\n");

    VPU_DEMO_LOG("Getting help:\n");
    VPU_DEMO_LOG("-help  --print options of vpu api demo\n");
}

static VpuApiCmd_t vpuApiCmd[] = {
    {"i",               "input_file",           "input bitstream file"},
    {"o",               "output_file",          "output bitstream file, "},
    {"w",               "width",                "the width of input bitstream"},
    {"h",               "height",               "the height of input bitstream"},
    {"t",               "codec_type",           "the codec type, dec: deoder, enc: encoder, default: decoder"},
    {"coding",          "coding_type",          "encoding type of the bitstream"},
    {"vframes",         "number",               "set the number of video frames to record"},
    {"ss",              "time_off",             "set the start time offset, use Ms as the unit."},
    {"d",               "disable",              "disable the debug output info."},
};

static RK_S32 show_help()
{
    VPU_DEMO_LOG("usage: vpu_apiDemo [options] input_file, \n\n");

    RK_S32 i =0;
    RK_U32 n = sizeof(vpuApiCmd)/sizeof(VpuApiCmd_t);
    for (i =0; i <n; i++) {
        VPU_DEMO_LOG("-%s  %s\t\t%s\n",
            vpuApiCmd[i].name, vpuApiCmd[i].argname, vpuApiCmd[i].help);
    }

    return 0;
}

static RK_S32 parse_options(int argc, char **argv, VpuApiDemoCmdContext_t* cmdCxt)
{
    char *opt;
    RK_U32 optindex, handleoptions = 1, ret =0;

    if ((argc <2) || (cmdCxt == NULL)) {
        VPU_DEMO_LOG("vpu api demo, input parameter invalid\n");
        show_usage();
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
                        VPU_DEMO_LOG("input file is invalid\n");
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
                        VPU_DEMO_LOG("out file is invalid\n");
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
                        VPU_DEMO_LOG("input width is invalid\n");
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
                        VPU_DEMO_LOG("input height is invalid\n");
                        ret = -1;
                        goto PARSE_OPINIONS_OUT;
                    }
                    break;
                case 't':
                    if (argv[optindex]) {
                        cmdCxt->codec_type = atoi(argv[optindex]);
                        break;
                    } else {
                        VPU_DEMO_LOG("input codec_type is invalid\n");
                        ret = -1;
                        goto PARSE_OPINIONS_OUT;
                    }

                default:
                    if ((*(opt+1) != '\0') && argv[optindex]) {
                        if (!strncmp(opt, "coding", 6)) {
                            VPU_DEMO_LOG("coding, argv[optindex]: %s",
                                    argv[optindex]);
                            cmdCxt->coding = atoi(argv[optindex]);
                        } else if (!strncmp(opt, "vframes", 7)) {
                            cmdCxt->record_frames = atoi(argv[optindex]);
                        } else if (!strncmp(opt, "ss", 2)) {
                            cmdCxt->record_start_ms = atoi(argv[optindex]);
                        } else {
                            ret = -1;
                            goto PARSE_OPINIONS_OUT;
                        }
                    } else {
                        ret = -1;
                        goto PARSE_OPINIONS_OUT;
                    }
                    break;
            }

            optindex += ret;
        }
    }

PARSE_OPINIONS_OUT:
    if (ret <0) {
        VPU_DEMO_LOG("vpu api demo, input parameter invalid\n");
        show_usage();
        return ERROR_INVALID_PARAM;
    }
    return ret;
}

static RK_S32 readBytesFromFile(RK_U8* buf, RK_S32 aBytes, FILE* file)
{
    if ((NULL ==buf) || (NULL ==file) || (0 ==aBytes)) {
        return -1;
    }
    RK_S32 ret = fread(buf, 1, aBytes, file);
    VPU_DEMO_LOG("  <%s>_%d   aBytes = %d, ret = %d \n", __func__, __LINE__, aBytes, ret);
	if(ret != aBytes)
	{
		VPU_DEMO_LOG("read %d bytes from file fail\n", aBytes);
        return -1;
	}

    return 0;
}

static RK_S32 vpu_encode_demo(VpuApiDemoCmdContext_t *cmd)
{
    if (cmd == NULL) {
    	printf("   <%s>_%d    cmd == NULL \n", __func__, __LINE__);
        return -1;
    }
#if 1
    FILE* pInFile = NULL;
    FILE* pOutFile = NULL;
    struct VpuCodecContext *ctx = NULL;
    RK_S32 nal = 0x00000001;
    RK_S32 fileSize, frame_count, ret, size;
    EncoderOut_t    enc_out_yuv;
    EncoderOut_t *enc_out = NULL;
    VpuApiEncInput enc_in_strm;
    VpuApiEncInput *api_enc_in = &enc_in_strm;
    EncInputStream_t *enc_in =NULL;
    EncParameter_t *enc_param = NULL;
    RK_S64 fakeTimeUs =0;
    int Format = VPU_H264ENC_YUV420_SEMIPLANAR;//VPU_H264ENC_YUV420_PLANAR;
    RK_U32 w_align = 0;
    RK_U32 h_align = 0;
    if ((cmd->have_input == NULL) || (cmd->width <=0) || (cmd->height <=0)
            || (cmd->coding <= OMX_ON2_VIDEO_CodingAutoDetect)) {
        VPU_DEMO_LOG("Warning: missing needed parameters for vpu api demo\n");
    }
    printf("   <%s>_%d  \n", __func__, __LINE__);
    if (cmd->have_input) {
        VPU_DEMO_LOG("input bitstream w: %d, h: %d, coding: %d(%s), path: %s\n",
            cmd->width, cmd->height, cmd->coding,
            cmd->codec_type == CODEC_DECODER ? "decode" : "encode",
            cmd->input_file);

        pInFile = fopen(cmd->input_file, "rb");
        if (pInFile == NULL) {
            VPU_DEMO_LOG("input file not exsist\n");
            ENCODE_ERR_RET(ERROR_INVALID_PARAM);
        }
    } else {
        VPU_DEMO_LOG("please set input bitstream file\n");
        ENCODE_ERR_RET(ERROR_INVALID_PARAM);
    }

    if (cmd->have_output) {
        VPU_DEMO_LOG("vpu api demo output file: %s\n",
            cmd->output_file);
        pOutFile = fopen(cmd->output_file, "wb");
        if (pOutFile == NULL) {
            VPU_DEMO_LOG("can not write output file\n");
            ENCODE_ERR_RET(ERROR_INVALID_PARAM);
        }
    }

    printf("   <%s>_%d  \n", __func__, __LINE__);
    fseek(pInFile, 0L, SEEK_END);
    fileSize = ftell(pInFile);
    fseek(pInFile, 0L, SEEK_SET);

    memset(&enc_in_strm, 0, sizeof(VpuApiEncInput));
    enc_in = &enc_in_strm.stream;
    enc_in->buf = NULL;

    memset(&enc_out_yuv, 0, sizeof(EncoderOut_t));
    enc_out = &enc_out_yuv;
    enc_out->data = (RK_U8*)malloc(cmd->width * cmd->height);
    if (enc_out->data == NULL) {
    	printf("   <%s>_%d  \n", __func__, __LINE__);
        ENCODE_ERR_RET(ERROR_MEMORY);
    }
    printf("   <%s>_%d  \n", __func__, __LINE__);

    ret = vpu_open_context(&ctx);
    printf("   <%s>_%d  \n", __func__, __LINE__);
    if (ret || (ctx ==NULL)) {
    	printf("   <%s>_%d  \n", __func__, __LINE__);
        ENCODE_ERR_RET(ERROR_MEMORY);
    }
    printf("   <%s>_%d  \n", __func__, __LINE__);

    /*
     ** now init vpu api context. codecType, codingType, width ,height
     ** are all needed before init.
    */
    ctx->codecType = cmd->codec_type;
    ctx->videoCoding = cmd->coding;
    ctx->width = cmd->width;
    ctx->height = cmd->height;
    ctx->no_thread = 1;

    ctx->private_data = malloc(sizeof(EncParameter_t));
    memset(ctx->private_data,0,sizeof(EncParameter_t));

    enc_param = (EncParameter_t*)ctx->private_data;
    enc_param->width = cmd->width;
    enc_param->height = cmd->height;
    enc_param->bitRate = 5*1024*1024;
    enc_param->framerate = 25;
    enc_param->enableCabac   = 0;
    enc_param->cabacInitIdc  = 0;
    enc_param->intraPicRate  = 25;

    if ((ret = ctx->init(ctx, NULL, 0)) !=0) {
       printf("   <%s>_%d  \n", __func__, __LINE__);
       VPU_DEMO_LOG("init vpu api context fail, ret: 0x%X", ret);
       ENCODE_ERR_RET(ERROR_INIT_VPU);
    }
    printf("   <%s>_%d  \n", __func__, __LINE__);

    /*
     ** init of VpuCodecContext while running encode, it returns
     ** sps and pps of encoder output, you need to save sps and pps
     ** after init.
    */
    VPU_DEMO_LOG("encode init ok, sps len: %d", ctx->extradata_size);
    printf("   <%s>_%d  \n", __func__, __LINE__);
    if(pOutFile && (ctx->extradata_size >0)) {
    printf("   <%s>_%d  \n", __func__, __LINE__);
        VPU_DEMO_LOG("dump %d bytes enc output stream to file",
            ctx->extradata_size);

        /* save sps and pps */
        fwrite(ctx->extradata, 1, ctx->extradata_size, pOutFile);
        fflush(pOutFile);
    }

    printf("   <%s>_%d  \n", __func__, __LINE__);
    ctx->control(ctx,VPU_API_ENC_SETFORMAT,&Format);  //set input format

    ctx->control(ctx,VPU_API_ENC_GETCFG,enc_param);  // open rate control
    enc_param->rc_mode = 1;
    ctx->control(ctx,VPU_API_ENC_SETCFG,enc_param);
    printf("   <%s>_%d  \n", __func__, __LINE__);

    /*
     ** vpu api encode process.
    */
    VPU_DEMO_LOG("init vpu api context ok, input yuv stream file size: %d", fileSize);
    w_align = ctx->width;
    h_align = ctx->height;
    size = w_align * h_align * 3/2;
    nal = BSWAP32(nal);
    ALOGE("read frm size %d",size);
    do {
        if (ftell(pInFile) >=fileSize) {
           VPU_DEMO_LOG("read end of file, complete");
           break;
        }

        if (enc_in && (enc_in->size ==0)) {
            if (enc_in->buf == NULL) {
                enc_in->buf = (RK_U8*)(malloc)(size);
                if (enc_in->buf == NULL) {
                    ENCODE_ERR_RET(ERROR_MEMORY);
                }
                api_enc_in->capability = size;
            }

            if (api_enc_in->capability <((RK_U32)size)) {
                enc_in->buf = (RK_U8*)(realloc)((void*)(enc_in->buf), size);
                if (enc_in->buf == NULL) {
                    ENCODE_ERR_RET(ERROR_MEMORY);
                }
                api_enc_in->capability = size;
            }

            if (readBytesFromFile(enc_in->buf, size, pInFile)) {
                break;
            } else {
                enc_in->size = size;
                enc_in->timeUs = fakeTimeUs;
                fakeTimeUs +=40000;
            }

            VPU_DEMO_LOG("read one frame, size: %d, timeUs: %lld, filePos: %ld",
                enc_in->size, enc_in->timeUs , ftell(pInFile));
        }

        if ((ret = ctx->encode(ctx, enc_in, enc_out)) !=0) {
           ENCODE_ERR_RET(ERROR_VPU_DECODE);
        } else {
            VPU_DEMO_LOG("vpu encode one frame, out len: %d, left size: %d",
                enc_out->size, enc_in->size);

            /*
             ** encoder output stream is raw bitstream, you need to add nal
             ** head by yourself.
            */
            if ((enc_out->size) && (enc_out->data)) {
                if(pOutFile) {
                    VPU_DEMO_LOG("dump %d bytes enc output stream to file",
                        enc_out->size);
                    fwrite((uint8_t*)&nal, 1, 4, pOutFile);
                    fwrite(enc_out->data, 1, enc_out->size, pOutFile);
                    fflush(pOutFile);
                }

                enc_out->size = 0;
            }
        }

        usleep(30);
    }while(1);

ENCODE_OUT:
    if (enc_in && enc_in->buf) {
        free(enc_in->buf);
        enc_in->buf = NULL;
    }
    if (enc_out && (enc_out->data)) {
        free(enc_out->data);
        enc_out->data = NULL;
    }
    if (ctx) {
        if (ctx->private_data) {
            free(ctx->private_data);
            ctx->private_data = NULL;
        }
        vpu_close_context(&ctx);
        ctx = NULL;
    }
    if (pInFile) {
        fclose(pInFile);
        pInFile = NULL;
    }
    if (pOutFile) {
        fclose(pOutFile);
        pOutFile = NULL;
    }

    if (ret) {
        VPU_DEMO_LOG("encode demo fail, err: %d", ret);
    } else {
        VPU_DEMO_LOG("encode demo complete OK.");
    }
    return ret;
#endif    

}

static RK_S32 vpu_decode_demo(VpuApiDemoCmdContext_t *cmd)
{
    if (cmd == NULL) {
        return -1;
    }

    FILE* pInFile = NULL;
    FILE* pOutFile = NULL;
    struct VpuCodecContext* ctx = NULL;
    RK_S32 fileSize =0, pkt_size =0;
    RK_S32 ret = 0, frame_count = 0;
    DecoderOut_t    decOut;
    VideoPacket_t demoPkt;
    VideoPacket_t* pkt =NULL;
    DecoderOut_t *pOut = NULL;
    VPU_FRAME *frame = NULL;
    RK_S64 fakeTimeUs =0;
    RK_U8* pExtra = NULL;
    RK_U32 extraSize = 0;

    if ((cmd->have_input == NULL) || (cmd->width <=0) || (cmd->height <=0)
            || (cmd->coding <= OMX_ON2_VIDEO_CodingAutoDetect)) {
        VPU_DEMO_LOG("Warning: missing needed parameters for vpu api demo\n");
    }

    if (cmd->have_input) {
        VPU_DEMO_LOG("input bitstream w: %d, h: %d, coding: %d(%s), path: %s\n",
            cmd->width, cmd->height, cmd->coding,
            cmd->codec_type == CODEC_DECODER ? "decode" : "encode",
            cmd->input_file);

        pInFile = fopen(cmd->input_file, "rb");
        if (pInFile == NULL) {
            VPU_DEMO_LOG("input file not exsist\n");
            DECODE_ERR_RET(ERROR_INVALID_PARAM);
        }
    } else {
        VPU_DEMO_LOG("please set input bitstream file\n");
        DECODE_ERR_RET(ERROR_INVALID_PARAM);
    }

    if (cmd->have_output) {
        VPU_DEMO_LOG("vpu api demo output file: %s\n",
            cmd->output_file);
        pOutFile = fopen(cmd->output_file, "wb");
        if (pOutFile == NULL) {
            VPU_DEMO_LOG("can not write output file\n");
            DECODE_ERR_RET(ERROR_INVALID_PARAM);
        }
        if (cmd->record_frames ==0)
            cmd->record_frames = 5;
    }

    fseek(pInFile, 0L, SEEK_END);
    fileSize = ftell(pInFile);
    fseek(pInFile, 0L, SEEK_SET);
    VPU_DEMO_LOG("     fileSize = %d \n", fileSize);
    memset(&demoPkt, 0, sizeof(VideoPacket_t));
    pkt = &demoPkt;
    pkt->data = NULL;
    pkt->pts = VPU_API_NOPTS_VALUE;
    pkt->dts = VPU_API_NOPTS_VALUE;

    memset(&decOut, 0, sizeof(DecoderOut_t));
    pOut = &decOut;
    pOut->data = (RK_U8*)(malloc)(sizeof(VPU_FRAME));
    if (pOut->data ==NULL) {
        DECODE_ERR_RET(ERROR_MEMORY);
    }
    memset(pOut->data, 0, sizeof(VPU_FRAME));

    ret = vpu_open_context(&ctx);
    if (ret || (ctx ==NULL)) {
        DECODE_ERR_RET(ERROR_MEMORY);
    }

    /*
     ** read codec extra data from input stream file.
    */
    if (readBytesFromFile((RK_U8*)(&extraSize), 4, pInFile)) {
        DECODE_ERR_RET(ERROR_IO);
    }
//    extraSize = fileSize;
    VPU_DEMO_LOG("codec extra data size: %d", extraSize);

    pExtra = (RK_U8*)(malloc)(extraSize);
    if (pExtra ==NULL) {
        DECODE_ERR_RET(ERROR_MEMORY);
    }
    memset(pExtra, 0, extraSize);

    if (readBytesFromFile(pExtra, extraSize, pInFile)) {
        DECODE_ERR_RET(ERROR_IO);
    }

    /*
     ** now init vpu api context. codecType, codingType, width ,height
     ** are all needed before init.
    */
    ctx->codecType = cmd->codec_type;
    ctx->videoCoding = cmd->coding;
    ctx->width = cmd->width;
    ctx->height = cmd->height;
    ctx->no_thread = 1;

    if ((ret = ctx->init(ctx, pExtra, extraSize)) !=0) {
       VPU_DEMO_LOG("init vpu api context fail, ret: 0x%X", ret);
       DECODE_ERR_RET(ERROR_INIT_VPU);
    }

    /*
     ** vpu api decoder process.
    */
    VPU_DEMO_LOG("init vpu api context ok, fileSize: %d", fileSize);

    do {
        if (ftell(pInFile) >=fileSize) {
           VPU_DEMO_LOG("read end of file, complete");
           break;
        }

        if (pkt && (pkt->size ==0)) {
            if (readBytesFromFile((RK_U8*)(&pkt_size), 4, pInFile)) {
                break;
            }

            if (pkt->data ==NULL) {
                pkt->data = (RK_U8*)(malloc)(pkt_size);
                if (pkt->data ==NULL) {
                    DECODE_ERR_RET(ERROR_MEMORY);
                }
                pkt->capability = pkt_size;
            }

            if (pkt->capability <((RK_U32)pkt_size)) {
                pkt->data = (RK_U8*)(realloc)((void*)(pkt->data), pkt_size);
                if (pkt->data ==NULL) {
                    DECODE_ERR_RET(ERROR_MEMORY);
                }
                pkt->capability = pkt_size;
            }

            if (readBytesFromFile(pkt->data, pkt_size, pInFile)) {
                break;
            } else {
                pkt->size = pkt_size;
                pkt->pts = fakeTimeUs;
                fakeTimeUs +=40000;
            }

            VPU_DEMO_LOG("read one packet, size: %d, pts: %lld, filePos: %ld",
                pkt->size, pkt->pts, ftell(pInFile));
        }

        /* note: must set out put size to 0 before do decoder. */
        pOut->size = 0;

        if ((ret = ctx->decode(ctx, pkt, pOut)) !=0) {
           DECODE_ERR_RET(ERROR_VPU_DECODE);
        } else {
            VPU_DEMO_LOG("vpu decode one frame, out len: %d, left size: %d",
                pOut->size, pkt->size);

            /*
             ** both virtual and physical address of the decoded frame are contained
             ** in structure named VPU_FRAME, if you want to use virtual address, make
             ** sure you have done VPUMemLink before.
            */
            if ((pOut->size) && (pOut->data)) {
                VPU_FRAME *frame = (VPU_FRAME *)(pOut->data);
                VPUMemLink(&frame->vpumem);
                RK_U32 wAlign16 = ((frame->DisplayWidth+ 15) & (~15));
                RK_U32 hAlign16 = ((frame->DisplayHeight + 15) & (~15));
                RK_U32 frameSize = wAlign16*hAlign16*3/2;

                if(pOutFile && (frame_count++ <cmd->record_frames)) {
                    VPU_DEMO_LOG("write %d frame(yuv420sp) data, %d bytes to file",
                        frame_count, frameSize);

                    fwrite((RK_U8*)(frame->vpumem.vir_addr), 1, frameSize, pOutFile);
                    fflush(pOutFile);
                }

                /*
                 ** remember use VPUFreeLinear to free, other wise memory leak will
                 ** give you a surprise.
                */
                VPUFreeLinear(&frame->vpumem);
                pOut->size = 0;
            }
        }

        usleep(30);
    }while(!(ctx->decoder_err));

DECODE_OUT:
    if (pkt && pkt->data) {
        free(pkt->data);
        pkt->data = NULL;
    }
    if (pOut && (pOut->data)) {
        free(pOut->data);
        pOut->data = NULL;
    }
    if (pExtra) {
        free(pExtra);
        pExtra = NULL;
    }
    if (ctx) {
        vpu_close_context(&ctx);
        ctx = NULL;
    }
    if (pInFile) {
        fclose(pInFile);
        pInFile = NULL;
    }
    if (pOutFile) {
        fclose(pOutFile);
        pOutFile = NULL;
    }

    if (ret) {
        VPU_DEMO_LOG("decode demo fail, err: %d", ret);
    } else {
        VPU_DEMO_LOG("encode demo complete OK.");
    }
    return ret;
}


int main(int argc, char **argv)
{
    VPU_DEMO_LOG("/*******  vpu api demo in *******/ \n");

    VpuApiDemoCmdContext_t demoCmdCtx;
    RK_S32 ret =0;
    VPU_API_DEMO_DEBUG_DISABLE = 0;

    if (argc == 1) {
        show_usage();
        VPU_DEMO_LOG("vpu api demo complete directly\n");
        return 0;
    }

    VpuApiDemoCmdContext_t* cmd = &demoCmdCtx;
    memset (cmd, 0, sizeof(VpuApiDemoCmdContext_t));
    cmd->codec_type = CODEC_DECODER;
    if ((ret = parse_options(argc, argv, cmd)) !=0) {
        if (ret == VPU_DEMO_PARSE_HELP_OK) {
            return 0;
        }

        VPU_DEMO_LOG("parse_options fail\n\n");
        show_usage();
        DEMO_ERR_RET(ERROR_INVALID_PARAM);
    }

    if (cmd->disable_debug) {
        VPU_API_DEMO_DEBUG_DISABLE = 1;
    }
    printf("   <%s>_%d  \n", __func__, __LINE__);

    switch (cmd->codec_type) {
    case CODEC_DECODER:
    	printf("   <%s>_%d  \n", __func__, __LINE__);
        ret = vpu_decode_demo(cmd);
        break;
    case CODEC_ENCODER:
    	printf("   <%s>_%d  \n", __func__, __LINE__);
        ret = vpu_encode_demo(cmd);
        break;

    default:
    	printf("   <%s>_%d  \n", __func__, __LINE__);
        ret = ERROR_INVALID_PARAM;
        break;
    }

DEMO_OUT:
    if (ret) {
        VPU_DEMO_LOG("vpu api demo fail, err: %d", ret);
    } else {
        VPU_DEMO_LOG("vpu api demo complete OK.");
    }
    return ret;
}
