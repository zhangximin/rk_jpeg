#include <utils/Log.h>
#include <utils/List.h>

#include <stdlib.h>
//#include <cutils/properties.h> // for property_get
#include "deinter.h"
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <dlfcn.h>  // for dlopen/dlclose
#include "ppOp.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>


#undef LOG_TAG
#define LOG_TAG "post_deinterlace"

//#define _VPU_POST_DEINTER_DEBUG

#ifdef  _VPU_POST_DEINTER_DEBUG
#ifdef AVS40
#define DEINTER_LOG           LOGD
#else
#define DEINTER_LOG           ALOGD
#endif
#else
#define DEINTER_LOG
#endif

#ifdef AVS40
#define DEINTER_ERR           LOGE
#else
#define DEINTER_ERR           ALOGE
#endif

namespace android {

deinterlace_dev::deinterlace_dev()
    :dev_status(USING_NULL),dev_fd(-1),priv_data(NULL)
{
    DEINTER_LOG("deinterlace_dev construct in");
    dev_fd = open("/dev/rk29-ipp", O_RDWR, 0);
    if (dev_fd > 0) {
        dev_status = USING_IPP;
        DEINTER_LOG("using ipp device");
    } else {
        if (access("/dev/iep", 06) == 0) {
            iep_lib_handle = dlopen("/system/lib/libiep.so", RTLD_LAZY);
            if (iep_lib_handle == NULL) {
                DEINTER_ERR("dlopen iep library failure\n");
                return;
            }

            DEINTER_LOG("using ipp device");
            ops.claim = (void* (*)())dlsym(iep_lib_handle, "iep_ops_claim");
            ops.reclaim = (void* (*)(void *iep_obj))dlsym(iep_lib_handle, "iep_ops_reclaim");
            ops.init_discrete = (int (*)(void *iep_obj,
                          int src_act_w, int src_act_h,
                          int src_x_off, int src_y_off,
                          int src_vir_w, int src_vir_h,
                          int src_format,
                          int src_mem_addr, int src_uv_addr, int src_v_addr,
                          int dst_act_w, int dst_act_h,
                          int dst_x_off, int dst_y_off,
                          int dst_vir_w, int dst_vir_h,
                          int dst_format,
                          int dst_mem_addr, int dst_uv_addr, int dst_v_addr))dlsym(iep_lib_handle, "iep_ops_init_discrete");
            ops.config_yuv_deinterlace = (int (*)(void *iep_obj))dlsym(iep_lib_handle, "iep_ops_config_yuv_deinterlace");
            ops.run_async_ncb = (int (*)(void *iep_obj))dlsym(iep_lib_handle, "iep_ops_run_async_ncb");
            ops.poll = (int (*)(void *iep_obj))dlsym(iep_lib_handle, "iep_ops_poll");
            if (ops.claim == NULL || ops.reclaim == NULL || ops.init_discrete == NULL
                || ops.config_yuv_deinterlace == NULL || ops.run_async_ncb == NULL || ops.poll == NULL) {
                DEINTER_ERR("dlsym iep library failure\n");
                dlclose(iep_lib_handle);
                return;
            }

            api = (void*)ops.claim();
            if (api == NULL) {
                DEINTER_ERR("iep api claim failure\n");
                dlclose(iep_lib_handle);
                return;
            }

            dev_status = USING_IEP;
            DEINTER_LOG("Deinterlace Using IEP\n");
        } else {
            //char prop_value[PROPERTY_VALUE_MAX];
            //if (property_get("sys.sf.pp_deinterlace", prop_value, NULL) && atoi(prop_value) > 0) 
            {
                dev_fd = VPUClientInit(VPU_PP);
                if (dev_fd > 0) {
                    dev_status = USING_PP;
                    DEINTER_LOG("try to use PP ok");
                } else {
                    DEINTER_LOG("found no hardware for deinterlace just skip!!");
                }
            } 
            //else {
            //    DEINTER_LOG("no ipp to do deinterlace but pp is disabled");
            //    dev_fd = -1;
            //}
        }
    }
}

deinterlace_dev::~deinterlace_dev()
{
    switch (dev_status) {
    case USING_IEP : {
        ops.reclaim(api);
        dlclose(iep_lib_handle);
    } break;
    case USING_IPP : {
        close(dev_fd);
    } break;
    case USING_PP : {
        if (NULL != priv_data) {
            PP_OP_HANDLE hnd = (PP_OP_HANDLE)priv_data;
            ppOpRelease(hnd);
            priv_data = NULL;
        }
        VPUClientRelease(dev_fd);
    } break;
    default : {
    } break;
    }
}

status_t deinterlace_dev::perform(VPU_FRAME *frm, uint32_t bypass)
{
    status_t ret = NO_INIT;
    VPUMemLinear_t deInterlaceFrame;
    ret = VPUMallocLinear(&deInterlaceFrame, frm->FrameHeight*frm->FrameWidth*3/2);
    if (!ret) {
        uint32_t width    = frm->FrameWidth;
        uint32_t height   = frm->FrameHeight;
        uint32_t srcYAddr = frm->FrameBusAddr[0];
        uint32_t srcCAddr = frm->FrameBusAddr[1];
        uint32_t dstYAddr = deInterlaceFrame.phy_addr;
        uint32_t dstCAddr = deInterlaceFrame.phy_addr + width*height;
        frm->FrameBusAddr[0] = dstYAddr;
        frm->FrameBusAddr[1] = dstCAddr;
        switch (dev_status) {
        case USING_IPP : {
            struct rk29_ipp_req ipp_req;
            memset(&ipp_req,0,sizeof(rk29_ipp_req));
            ipp_req.src0.YrgbMst = srcYAddr;
            ipp_req.src0.CbrMst  = srcCAddr;
            ipp_req.src0.w = width;
            ipp_req.src0.h = height;
            ipp_req.src0.fmt = IPP_Y_CBCR_H2V2;
            ipp_req.dst0.w = width;
            ipp_req.dst0.h = height;
            ipp_req.src_vir_w = width;
            ipp_req.dst_vir_w = width;
            ipp_req.timeout = 100;
            ipp_req.flag = IPP_ROT_0;
            ipp_req.deinterlace_enable = bypass ? 0 : 1;
            ipp_req.deinterlace_para0 = 8;
            ipp_req.deinterlace_para1 = 16;
            ipp_req.deinterlace_para2 = 8;
            ipp_req.dst0.YrgbMst = dstYAddr;
            ipp_req.dst0.CbrMst  = dstCAddr;
            ret = ioctl(dev_fd, IPP_BLIT_ASYNC, &ipp_req);
        } break;
        case USING_IEP : {
            /**
            IEP_FORMAT_ARGB_8888    = 0x0,
            IEP_FORMAT_ABGR_8888    = 0x1,
            IEP_FORMAT_RGBA_8888    = 0x2,
            IEP_FORMAT_BGRA_8888    = 0x3,
            IEP_FORMAT_RGB_565      = 0x4,
            IEP_FORMAT_BGR_565      = 0x5,

            IEP_FORMAT_YCbCr_422_SP = 0x10,
            IEP_FORMAT_YCbCr_422_P  = 0x11,
            IEP_FORMAT_YCbCr_420_SP = 0x12,
            IEP_FORMAT_YCbCr_420_P  = 0x13,
            IEP_FORMAT_YCrCb_422_SP = 0x14,
            IEP_FORMAT_YCrCb_422_P  = 0x15,
            IEP_FORMAT_YCrCb_420_SP = 0x16,
            IEP_FORMAT_YCrCb_420_P  = 0x17
            */

            ops.init_discrete(api, width, height, 0, 0, width, height, 0x12, srcYAddr, srcCAddr, 0,
                                  width, height, 0, 0, width, height, 0x12, dstYAddr, dstCAddr, 0);

            if (!bypass) {
                if (0 > ops.config_yuv_deinterlace(api)) {
                    DEINTER_ERR("Failure to Configure YUV DEINTERLACE\n");
                }
            }

            ops.run_async_ncb(api);

        } break;
        case USING_PP : {
            if (NULL == priv_data) {
                PP_OPERATION opt;
                memset(&opt, 0, sizeof(opt));
                opt.srcAddr     = srcYAddr;
                opt.srcFormat   = PP_IN_FORMAT_YUV420SEMI;
                opt.srcWidth    = opt.srcHStride = width;
                opt.srcHeight   = opt.srcVStride = height;

                opt.dstAddr     = dstYAddr;
                opt.dstFormat   = PP_OUT_FORMAT_YUV420INTERLAVE;
                opt.dstWidth    = opt.dstHStride = width;
                opt.dstHeight   = opt.dstVStride = height;
                opt.deinterlace = 1;
                opt.vpuFd       = dev_fd;
                ret = ppOpInit(&priv_data, &opt);
                if (ret) {
                    DEINTER_ERR("ppOpInit failed");
                    priv_data = NULL;
                }
            }

            if (NULL != priv_data) {
                PP_OP_HANDLE hnd = (PP_OP_HANDLE)priv_data;
                ppOpSet(hnd, PP_SET_SRC_ADDR, srcYAddr);
                ppOpSet(hnd, PP_SET_DST_ADDR, dstYAddr);
                ret = ppOpPerform(hnd);
            }
        } break;
        default : {
            ret = BAD_VALUE;
        } break;
        }

        if (!ret) {
            VPUMemDuplicate(&frm->vpumem,&deInterlaceFrame);
        } else {
            DEINTER_ERR("ioctl: IPP_BLIT_ASYNC faild!");
        }
        VPUFreeLinear(&deInterlaceFrame);
    }
    return ret;
}

status_t deinterlace_dev::sync()
{
    status_t ret = NO_INIT;
    switch (dev_status) {
    case USING_IPP : {
        int result;
        struct pollfd fd;
        fd.fd = dev_fd;
        fd.events = POLLIN;
        if (poll(&fd, 1, -1) > 0) {
            ret = ioctl(dev_fd, IPP_GET_RESULT, &result);
            if (ret) {
                DEINTER_ERR("ioctl:IPP_GET_RESULT faild!");
            }
        } else {
            DEINTER_ERR("IPP poll faild!");
            ret = -ETIMEDOUT;
        }
    } break;
    case USING_IEP : {
        ret = ops.poll(api);
        if (ret != 0) {
            DEINTER_LOG("iep poll failure, return %d\n", ret);
        }
        ret = 0;
    } break;
    case USING_PP : {
        if (NULL != priv_data) {
            PP_OP_HANDLE hnd = (PP_OP_HANDLE)priv_data;
            ret = ppOpSync(hnd);
            if (ret) {
                DEINTER_ERR("ppOpSync faild!");
            }
        } else {
            DEINTER_ERR("no ppOp for doing deinterlace");
        }
    } break;
    default : {
    } break;
    }
    return ret;
}

status_t deinterlace_dev::status()
{
    return dev_status;
}

status_t deinterlace_dev::test()
{
    status_t ret = NO_INIT;
    switch (dev_status) {
    case USING_IPP : {
        struct rk29_ipp_req ipp_req;
        memset(&ipp_req, 0, sizeof(rk29_ipp_req));
        ipp_req.deinterlace_enable =2;
        DEINTER_LOG("test ipp is support or not");
        ret = ioctl(dev_fd, IPP_BLIT_ASYNC, &ipp_req);
        if (ret) {
            close(dev_fd);
            dev_status = USING_NULL;
        } else {
            DEINTER_LOG("test ipp ok");
        }
    } break;
    case USING_IEP: {
        ret = OK;
    } break;
    case USING_PP : {
        //VPUClientRelease(dev_fd);
        ret = OK;
    } break;
    default : {
    } break;
    }
    return ret;
}

}

