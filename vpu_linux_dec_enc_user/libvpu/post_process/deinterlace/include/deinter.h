#ifndef DEINTER_H_
#define DEINTER_H_

#include "vpu_global.h"
#include "vpu.h"
#include "android/Errors.h"
#include "rk_list.h"
#include "rk29-ipp.h"
#include <fcntl.h>
#include <poll.h>


namespace android {

typedef enum DEINTER_DEV_TYPE {
    USING_NULL    = -1,
    USING_IPP     = 0,
    USING_PP      = 1,
    USING_IEP     = 2,

    USING_BUTT,
}DEINTER_DEV_TYPE;

typedef struct iep_ops {
    void* (*claim)();
    void* (*reclaim)(void *iep_obj);
    int (*init_discrete)(void *iep_obj,
                          int src_act_w, int src_act_h,
                          int src_x_off, int src_y_off,
                          int src_vir_w, int src_vir_h,
                          int src_format,
                          int src_mem_addr, int src_uv_addr, int src_v_addr,
                          int dst_act_w, int dst_act_h,
                          int dst_x_off, int dst_y_off,
                          int dst_vir_w, int dst_vir_h,
                          int dst_format,
                          int dst_mem_addr, int dst_uv_addr, int dst_v_addr);
    int (*config_yuv_deinterlace)(void *iep_obj);
    int (*run_async_ncb)(void *iep_obj);
    int (*poll)(void *iep_obj);
}iep_ops_t;

typedef struct deinterlace_dev
{
    deinterlace_dev();
    ~deinterlace_dev();

    status_t dev_status;
    int dev_fd;
    void *priv_data;
    status_t perform(VPU_FRAME *frm, uint32_t bypass);
    status_t sync();
    status_t status();
    status_t test();
    void *api;
    void *iep_lib_handle;
    struct iep_ops ops;
}deinterlace_dev_t;
}
#endif
