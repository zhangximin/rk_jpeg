/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2006 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Description : System Wrapper Layer for Linux. Polling version.
--
------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: dwl_x170_linux_no_drv.c,v $
--  $Revision: 1.13 $
--  $Date: 2010/03/24 06:21:33 $
--
------------------------------------------------------------------------------*/
#define LOG_TAG "vpu"
#include "vpu_type.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/timeb.h>
#include <sys/poll.h>

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>

//#include <cutils/sockets.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/syscall.h>

#include "vpu.h"
#include "vpu_drv.h"
#ifdef AVS40
#undef ALOGV
#define ALOGV printf


#undef ALOGI
#define ALOGI printf

#undef ALOGW
#define ALOGW printf

#endif

//add socket def
// Linux "abstract" (non-filesystem) namespace
#define ANDROID_SOCKET_NAMESPACE_ABSTRACT 0
// Android "reserved" (/dev/socket) namespace
#define ANDROID_SOCKET_NAMESPACE_RESERVED 1
// Normal filesystem namespace
#define ANDROID_SOCKET_NAMESPACE_FILESYSTEM 2


#define VPU_IOC_MAGIC                       'l'
#define VPU_IOC_SET_CLIENT_TYPE             _IOW(VPU_IOC_MAGIC, 1, unsigned long)
#define VPU_IOC_GET_HW_FUSE_STATUS          _IOW(VPU_IOC_MAGIC, 2, unsigned long)
#define VPU_IOC_SET_REG                     _IOW(VPU_IOC_MAGIC, 3, unsigned long)
#define VPU_IOC_GET_REG                     _IOW(VPU_IOC_MAGIC, 4, unsigned long)
typedef struct VPUReq
{
    RK_U32 *req;
    RK_U32  size;
} VPUReq_t;
static int vpu_service_status = -1;
#define VPU_SERVICE_TEST    \
    do { \
        if (vpu_service_status < 0) { \
            vpu_service_status = (access("/dev/vpu_service", F_OK) == 0); \
        } \
    } while (0)
pid_t gettid_()
{
     return syscall(SYS_gettid);
}
int VPUClientInit(VPU_CLIENT_TYPE type)
{
    VPU_SERVICE_TEST;
    if (vpu_service_status) {
        int ret;
        int fd = open("/dev/vpu_service", O_RDWR);
        if (fd == -1) {
            printf("VPUClient: failed to open vpu_service\n");
            return -1;
        }
        ret = ioctl(fd, VPU_IOC_SET_CLIENT_TYPE, (RK_U32)type);
        if (ret) {
            printf("VPUClient: ioctl VPU_IOC_SET_CLIENT_TYPE failed ret %d\n", ret);
            return -2;
        }
        return fd;
    } else {
#if 1
        int socket = -1;
        VPU_CMD_TYPE cmd;
        VPU_REGISTER_S chn;

        VPU_DEBUG("VPU: VPUClientInit\n");

        /* create socket */
    	/*socket = socket_local_client("vpu_server", ANDROID_SOCKET_NAMESPACE_RESERVED,
								   		SOCK_STREAM);*/
    	socket = vpu_socket_connect();
        if (socket < 0)
        {
            VPU_DEBUG("VPU: socket vpu_server link fail\n");
            return socket;
        }

        VPU_DEBUG("VPU: VPU client %d link success\n", socket);

        chn.client_type = type;

        chn.pid = gettid_();

        VPU_DEBUG("VPU: VPU client getpid %d\n", chn.pid);

        if ((VPU_SUCCESS == VPUSendMsg(socket, VPU_CMD_REGISTER, &chn, sizeof(VPU_REGISTER_S), 0)) &&
                (VPU_FAILURE != VPURecvMsg(socket, &cmd, NULL, 0, 0)) &&
                (VPU_CMD_REGISTER_ACK_OK == cmd))
        {
            VPU_DEBUG("VPUClientInit success\n");
        }
        else
        {
            VPU_DEBUG("VPUClientInit fail 0\n");
            if (-1 != socket)
            {
                shutdown(socket, SHUT_RDWR);
                close(socket);
                socket = -1;
            }

            VPU_DEBUG("VPUClientInit fail 1\n");
        }

        return socket;
#endif
    }
}

RK_S32 VPUClientRelease(int socket)
{
    VPU_SERVICE_TEST;
    if (vpu_service_status) {
        int fd = socket;
        if (fd > 0) {
            close(fd);
        }
        return VPU_SUCCESS;
    } else {
        if (-1 != socket)
        {
            while (VPUSendMsg(socket, VPU_CMD_UNREGISTER, NULL, 0, 0))
                usleep(10);

            // TODO: 这里是否还要 shutdown 和 close，好像服务器端会做同样的操作
            shutdown(socket, SHUT_RDWR);

            close(socket);

            socket = -1;
        }

        VPU_DEBUG("VPUClientRelease success\n");

        return VPU_SUCCESS;
    }
}

RK_S32 VPUClientSendReg(int socket, RK_U32 *regs, RK_U32 nregs)
{
    VPU_SERVICE_TEST;
    if (vpu_service_status) {
        int fd = socket;
        RK_S32 ret;
        VPUReq_t req;
        nregs *= sizeof(RK_U32);
        req.req     = regs;
        req.size    = nregs;
        ret = (RK_S32)ioctl(fd, VPU_IOC_SET_REG, (RK_U32)&req);
        if (ret) {
            printf("VPUClient: ioctl VPU_IOC_SET_REG failed ret %d errno %d %s\n", ret, errno, strerror(errno));
        }
        return ret;
    } else {
        nregs *= sizeof(RK_U32);
        RK_S32 ret = VPUSendMsg(socket, VPU_SEND_CONFIG, regs, nregs, 0);

        if (VPU_SUCCESS == ret)
        {
            VPU_DEBUG("VPUClientSendReg VPUSendMsg success\n");
        }
        else
        {
            VPU_DEBUG("VPUClientSendReg VPUSendMsg fail\n");
        }

        return ret;
    }
}

// TODO: 看是否客户端需要返回消息的长度
RK_S32 VPUClientWaitResult(int socket, RK_U32 *regs, RK_U32 nregs, VPU_CMD_TYPE *cmd, RK_S32 *len)
{
    VPU_SERVICE_TEST;
    if (vpu_service_status) {
        int fd = socket;
        RK_S32 ret;
        VPUReq_t req;
        nregs *= sizeof(RK_U32);
        req.req     = regs;
        req.size    = nregs;
        ret = (RK_S32)ioctl(fd, VPU_IOC_GET_REG, (RK_U32)&req);
        if (ret) {
            printf("VPUClient: ioctl VPU_IOC_GET_REG failed ret %d errno %d %s\n", ret, errno, strerror(errno));
            *cmd = VPU_SEND_CONFIG_ACK_FAIL;
        } else {
            *cmd = VPU_SEND_CONFIG_ACK_OK;
        }
        return ret;
    } else {
        RK_S32 ret;
        RK_S32 retlen = -1;
        nregs *= sizeof(RK_U32);

        VPU_DEBUG("VPUClientWaitResult start\n");

        retlen = VPURecvMsg(socket, cmd, regs, nregs, 0);

        if (retlen >= 0)
        {
            VPU_DEBUG("VPUClientWaitResult success\n");
            ret = VPU_SUCCESS;
        }
        else
        {
            VPU_DEBUG("VPUClientWaitResult fail\n");
            ret = VPU_FAILURE;
        }

        *len = retlen;

        return ret;
    }
}

RK_S32 VPUClientGetHwCfg(int socket, RK_U32 *cfg, RK_U32 cfg_size)
{
    VPU_SERVICE_TEST;
    if (vpu_service_status) {
        int fd = socket;
        RK_S32 ret;
        VPUReq_t req;
        req.req     = cfg;
        req.size    = cfg_size;
        ret = (RK_S32)ioctl(fd, VPU_IOC_GET_HW_FUSE_STATUS, (RK_U32)&req);
        if (ret) {
            printf("VPUClient: ioctl VPU_IOC_GET_HW_FUSE_STATUS failed ret %d\n", ret);
        }
        return ret;
    } else {
        RK_S32 ret;
        RK_S32 retlen = -1;
        VPU_CMD_TYPE cmd;

        ret = VPUSendMsg(socket, VPU_GET_HW_INFO, NULL, 0, 0);

        if (VPU_SUCCESS == ret)
        {
            VPU_DEBUG("VPUClientGetHwCfg VPUSendMsg success\n");
        }
        else
        {
            VPU_DEBUG("VPUClientGetHwCfg VPUSendMsg fail\n");
            return ret;
        }

        retlen = VPURecvMsg(socket, &cmd, cfg, cfg_size, 0);

        if ((0 < retlen) && (retlen <= (RK_S32)cfg_size) && (VPU_GET_HW_INFO_ACK_OK == cmd))
        {
            VPU_DEBUG("VPUClientGetHwCfg VPURecvMsg success\n");
            ret = VPU_SUCCESS;
        }
        else
        {
            VPU_DEBUG("VPUClientGetHwCfg VPURecvMsg fail\n");
            ret = VPU_FAILURE;
        }

        return ret;
    }
}

#if BUILD_VPU_TEST
#include <pthread.h>
#define MAX_THREAD_NUM      10
void *vpu_test(void *pdata)
{
    for (;;) {
        int fd = open("/dev/vpu_service", O_RDWR);
        if (fd < 0) {
            ALOGV("failed to open /dev/vpu_service ret %d\n", fd);
            return NULL;
        }
        close(fd);
    }
    return NULL;
}

int main()
{
    int i;
    ALOGV("vpu test start\n");
    pthread_t threads[MAX_THREAD_NUM];
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    for (i = 0; i < MAX_THREAD_NUM; i++) {
        pthread_create(&thread[i], &attr, vpu_test, NULL);
    }
    pthread_attr_destroy(&attr);
    for (i = 0; i < 10; i++)
        sleep(1);

    void *dummy;

    for (i = 0; i < MAX_THREAD_NUM; i++) {
        pthread_join(&thread[i], &dummy);
    }
    ALOGV("vpu test end\n");
    return 0;
}
#endif
