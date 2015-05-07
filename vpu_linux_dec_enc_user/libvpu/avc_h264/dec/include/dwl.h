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
--  Description : Sytem Wrapper Layer
--
------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: dwl.h,v $
--  $Revision: 1.19 $
--  $Date: 2010/05/11 09:33:19 $
--
------------------------------------------------------------------------------*/
#ifndef __DWL_H__
#define __DWL_H__

#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include "basetype.h"
#include "decapicommon.h"
#include "vpu_mem.h"
#include "vpu.h"

#define DWL_OK                      0
#define DWL_ERROR                  -1

#define DWL_HW_WAIT_OK              DWL_OK
#define DWL_HW_WAIT_ERROR           DWL_ERROR
#define DWL_HW_WAIT_TIMEOUT         1

#define DWL_CLIENT_TYPE_H264_DEC         1U
#define DWL_CLIENT_TYPE_MPEG4_DEC        2U
#define DWL_CLIENT_TYPE_JPEG_DEC         3U
#define DWL_CLIENT_TYPE_PP               4U
#define DWL_CLIENT_TYPE_VC1_DEC          5U
#define DWL_CLIENT_TYPE_MPEG2_DEC        6U
#define DWL_CLIENT_TYPE_VP6_DEC          7U
#define DWL_CLIENT_TYPE_AVS_DEC          9U /* TODO: fix */
#define DWL_CLIENT_TYPE_RV_DEC           8U
#define DWL_CLIENT_TYPE_VP8_DEC          10U


    /* Linear memory display buffer */
    typedef struct DispLinearMem
    {    
        u32 *vir_addr;    
        u32 phy_addr;
        u32 size; 
        u32 width;
        u32 height;
        u32 Isdisplay;
    } DispLinearMem;

    /* DWLInitParam is used to pass parameters when initializing the DWL */
    typedef struct DWLInitParam
    {
        u32 clientType;
    } DWLInitParam_t;

    /* Hardware configuration description */

    typedef struct DWLHwConfig
    {
        u32 maxDecPicWidth;  /* Maximum video decoding width supported  */
        u32 maxPpOutPicWidth;   /* Maximum output width of Post-Processor */
        u32 h264Support;     /* HW supports h.264 */
        u32 jpegSupport;     /* HW supports JPEG */
        u32 mpeg4Support;    /* HW supports MPEG-4 */
        u32 customMpeg4Support; /* HW supports custom MPEG-4 features */
        u32 vc1Support;      /* HW supports VC-1 Simple */
        u32 mpeg2Support;    /* HW supports MPEG-2 */
        u32 ppSupport;       /* HW supports post-processor */
        u32 ppConfig;        /* HW post-processor functions bitmask */
        u32 sorensonSparkSupport;   /* HW supports Sorenson Spark */
        u32 refBufSupport;   /* HW supports reference picture buffering */
        u32 vp6Support;      /* HW supports VP6 */
        u32 vp7Support;      /* HW supports VP7 */
        u32 vp8Support;      /* HW supports VP8 */
        u32 avsSupport;      /* HW supports AVS */
        u32 jpegESupport;    /* HW supports JPEG extensions */
        u32 rvSupport;       /* HW supports REAL */
        u32 mvcSupport;      /* HW supports H264 MVC extension */
    } DWLHwConfig_t;

	typedef struct DWLHwFuseStatus
    {
        u32 h264SupportFuse;     /* HW supports h.264 */
        u32 mpeg4SupportFuse;    /* HW supports MPEG-4 */
        u32 mpeg2SupportFuse;    /* HW supports MPEG-2 */
        u32 sorensonSparkSupportFuse;   /* HW supports Sorenson Spark */
		u32 jpegSupportFuse;     /* HW supports JPEG */
        u32 vp6SupportFuse;      /* HW supports VP6 */
        u32 vp7SupportFuse;      /* HW supports VP6 */
        u32 vp8SupportFuse;      /* HW supports VP6 */
        u32 vc1SupportFuse;      /* HW supports VC-1 Simple */
		u32 jpegProgSupportFuse; /* HW supports Progressive JPEG */
        u32 ppSupportFuse;       /* HW supports post-processor */
        u32 ppConfigFuse;        /* HW post-processor functions bitmask */
        u32 maxDecPicWidthFuse;  /* Maximum video decoding width supported  */
        u32 maxPpOutPicWidthFuse; /* Maximum output width of Post-Processor */
        u32 refBufSupportFuse;   /* HW supports reference picture buffering */
		u32 avsSupportFuse;      /* one of the AVS values defined above */
		u32 rvSupportFuse;       /* one of the REAL values defined above */
		u32 mvcSupportFuse;
        u32 customMpeg4SupportFuse; /* Fuse for custom MPEG-4 */

    } DWLHwFuseStatus_t;

/* HW configuration retrieving, static implementation */
    void DWLReadAsicConfig(DWLHwConfig_t * pHwCfg);

/* HW fuse retrieving, static implementation */
	//void DWLReadAsicFuseStatus(DWLHwFuseStatus_t * pHwFuseSts);

/* DWL initilaization and release */
    int VPUClientInit(VPU_CLIENT_TYPE type);
    i32 VPUClientRelease(int socket);
    i32 VPUClientSendReg(int socket, u32 *regs, u32 nregs);
    i32 VPUClientWaitResult(int socket, u32 *regs, u32 nregs, VPU_CMD_TYPE *cmd, i32 *len);

/* HW sharing */
    //i32 DWLReserveHw(const void *instance);
    //void DWLReleaseHw(const void *instance);

/* D-Cache coherence */
    //void DWLDCacheRangeFlush(const void *instance, VPULinearMem_t * info);  /* NOT in use */
    //void DWLDCacheRangeRefresh(const void *instance, VPULinearMem_t * info);    /* NOT in use */

/* Register access */
    void DWLWriteReg(const void *instance, u32 offset, u32 value);
    u32 DWLReadReg(const void *instance, u32 offset);

    //void DWLWriteRegAll(const void *instance, const u32 * table, u32 size); /* NOT in use */
    //void DWLReadRegAll(const void *instance, u32 * table, u32 size);    /* NOT in use */

/* HW starting/stopping */
    //void DWLEnableHW(const void *instance, u32 offset, u32 value);
    //void DWLDisableHW(const void *instance, u32 offset, u32 value);

/* HW synchronization */
    i32 VPUWaitHwReady(int socket, u32 timeout);

#ifdef __cplusplus
}
#endif

#endif                       /* __DWL_H__ */
