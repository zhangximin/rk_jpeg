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
--  Abstract : Hantro Encoder Wrapper Layer for OS services
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: ewl.h,v $
--  $Revision: 1.2 $
--
------------------------------------------------------------------------------*/

#ifndef __EWL_H__
#define __EWL_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "vpu_type.h"
#include "vpu_mem.h"
#include "vpu.h"

#define NULL ((void *)0)
/* Return values */
#define EWL_OK                      0
#define EWL_ERROR                  -1

#define EWL_HW_WAIT_OK              EWL_OK
#define EWL_HW_WAIT_ERROR           EWL_ERROR
#define EWL_HW_WAIT_TIMEOUT         1

/* HW configuration values */
#define EWL_HW_BUS_TYPE_UNKNOWN     0
#define EWL_HW_BUS_TYPE_AHB         1
#define EWL_HW_BUS_TYPE_OCP         2
#define EWL_HW_BUS_TYPE_AXI         3
#define EWL_HW_BUS_TYPE_PCI         4

#define EWL_HW_BUS_WIDTH_UNKNOWN     0
#define EWL_HW_BUS_WIDTH_32BITS      1
#define EWL_HW_BUS_WIDTH_64BITS      2
#define EWL_HW_BUS_WIDTH_128BITS     3

#define EWL_HW_SYNTHESIS_LANGUAGE_UNKNOWN     0
#define EWL_HW_SYNTHESIS_LANGUAGE_VHDL        1
#define EWL_HW_SYNTHESIS_LANGUAGE_VERILOG     2

#define EWL_HW_CONFIG_NOT_SUPPORTED    0
#define EWL_HW_CONFIG_ENABLED          1

/* Hardware configuration description */
    typedef struct EWLHwConfig
    {
        RK_U32 maxEncodedWidth; /* Maximum supported width for video encoding (not JPEG) */
        RK_U32 h264Enabled;     /* HW supports H.264 */
        RK_U32 jpegEnabled;     /* HW supports JPEG */
        RK_U32 mpeg4Enabled;    /* HW supports MPEG-4 */
        RK_U32 vsEnabled;       /* HW supports video stabilization */
        RK_U32 rgbEnabled;      /* HW supports RGB input */
        RK_U32 busType;         /* HW bus type in use */
        RK_U32 busWidth;
        RK_U32 synthesisLanguage;
    } EWLHwConfig_t;


/* EWLInitParam is used to pass parameters when initializing the EWL */
    typedef struct EWLInitParam
    {
        RK_U32 clientType;
    } EWLInitParam_t;

#define EWL_CLIENT_TYPE_H264_ENC         1U
#define EWL_CLIENT_TYPE_MPEG4_ENC        2U
#define EWL_CLIENT_TYPE_JPEG_ENC         3U
#define EWL_CLIENT_TYPE_VIDEOSTAB        4U

/*------------------------------------------------------------------------------
    4.  Function prototypes
------------------------------------------------------------------------------*/

/* Read and return the HW ID register value, static implementation */
    RK_U32 EWLReadAsicID(void);

/* Read and return HW configuration info, static implementation */
//    EWLHwConfig_t EWLReadAsicConfig(void);

/* Initialize the EWL instance 
 * Returns a wrapper instance or NULL for error
 * EWLInit is called when the encoder instance is initialized */
    const void *EWLInit(EWLInitParam_t * param);

/* Release the EWL instance
 * Returns EWL_OK or EWL_ERROR
 * EWLRelease is called when the encoder instance is released */
    RK_S32 EWLRelease(const void *inst);


/* Write value to a HW register
 * All registers are written at once at the beginning of frame encoding 
 * Offset is relative to the the HW ID register (#0) in bytes 
 * Enable indicates when the HW is enabled. If shadow registers are used then
 * they must be flushed to the HW registers when enable is '1' before
 * writing the register that enables the HW */
    void EWLWriteReg(const void *inst, RK_U32 offset, RK_U32 val);

/* Read and return the value of a HW register
 * The status register is read after every macroblock encoding by SW
 * The other registers which may be updated by the HW are read after
 * BUFFER_FULL or FRAME_READY interrupt
 * Offset is relative to the the HW ID register (#0) in bytes */
    RK_U32 EWLReadReg(const void *inst, RK_U32 offset);

    /* Writing all registers in one call *//*Not in use currently */
    void EWLWriteRegAll(const void *inst, const RK_U32 * table, RK_U32 size);
    /* Reading all registers in one call *//*Not in use currently */
    void EWLReadRegAll(const void *inst, RK_U32 * table, RK_U32 size);

/* HW enable/disable. This will write <val> to register <offset> and by */
/* this enablig/disabling the hardware */
    void EWLEnableHW(const void *inst, RK_U32 offset, RK_U32 val);
    void EWLDisableHW(const void *inst, RK_U32 offset, RK_U32 val);

/* Synchronize SW with HW
 * Returns EWL_HW_WAIT_OK, EWL_HW_WAIT_ERROR or EWL_HW_WAIT_TIMEOUT
 * EWLWaitHwRdy is called after enabling the HW to wait for IRQ from HW
 */
    RK_S32 EWLWaitHwRdy(const void *inst);

/* SW/SW shared memory handling */
    void *EWLmalloc(RK_U32 n);
//    void *EWLcalloc(RK_U32 n, RK_U32 s);
//    void EWLfree(void *p);
//    void *EWLmemcpy(void *d, const void *s, RK_U32 n);
    void *EWLmemset(void *d, RK_S32 c, RK_U32 n);

#ifdef __cplusplus
}
#endif
#endif /*__EWL_H__*/
