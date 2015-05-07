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
--  Abstract : Decoded Picture Buffer (DPB) handling
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264hwd_dpb.h,v $
--  $Date: 2010/01/12 07:06:02 $
--  $Revision: 1.11 $
--
------------------------------------------------------------------------------*/

#ifndef H264HWD_DPB_H
#define H264HWD_DPB_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "basetype.h"

#include "h264hwd_slice_header.h"
#include "h264hwd_image.h"

#include "dwl.h"
#include "vpu_list.h"

namespace android {

/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/

/* enumeration to represent status of buffered image */
typedef enum
{
    UNUSED = 0,
    NON_EXISTING,
    SHORT_TERM,
    LONG_TERM,
    EMPTY
} dpbPictureStatus_e;

#define MEM_AVAIL               (0x1)
#define MEM_DPB_USE             (0x2)
#define MEM_DPBDONE             (0x4)
#define MEM_OUT_USE             (0x8)
#define MEM_OUTDONE             (0x10)

#define DPBMEM_IS_AVAIL(p)      ((p)->memStatus&MEM_AVAIL)
#define DPBMEM_IS_DPB_USE(p)    ((p)->memStatus&MEM_DPB_USE)
#define DPBMEM_IS_DPBDONE(p)    ((p)->memStatus&MEM_DPBDONE)
#define DPBMEM_IS_OUT_USE(p)    ((p)->memStatus&MEM_OUT_USE)
#define DPBMEM_IS_OUTDONE(p)    ((p)->memStatus&MEM_OUTDONE)

typedef struct dpbMem {
    struct list_head list;
    u32 memStatus;
    u32 TimeLow;
    u32 TimeHigh;
    u32 phy_addr;
    u32 vir_addr;
    VPUMemLinear_t mem;
} dpbMem_t;

/* structure to represent a buffered picture */
typedef struct dpbPicture
{
    struct list_head list;
    u32 dpbIdx;
    dpbMem_t *data;
    i32 picNum;
    u32 frameNum;
    i32 picOrderCnt[2];
    dpbPictureStatus_e status[2];
    u32 toBeDisplayed;
    u32 picId;
    u32 numErrMbs;
    u32 isIdr;
	u32 isItype;
    u32 isFieldPic;             // 0: for frame 1: for top field ready 2 for bottom field ready
    u32 TimeLow;
    u32 TimeHigh;
	u32 h264hwStatus[2];
} dpbPicture_t;

/* structure to represent display image output from the buffer */
typedef struct
{
    struct list_head list;
    dpbMem_t *data;
    u32 picId;
    u32 numErrMbs;
    u32 isIdr;
    u32 interlaced;
    u32 fieldPicture;
    u32 topField;
    //DispLinearMem *pDispbuffer;
    u32 TimeLow;
    u32 TimeHigh;
	u32 isFieldPic;
	u32 h264hwStatus[2];
} dpbOutPicture_t;

/* structure to represent DPB */
typedef struct dpbStorage
{
    dpbPicture_t *buffer[17];
    dpbPicture_t *currentOut;
    dpbPicture_t *previousOut;
    dpbOutPicture_t *delayBuf;
    u32 refList[2];
    u32 currentPhyAddr;
    u32 currentVirAddr;
    u32 maxRefFrames;
    u32 dpbSize;
    u32 numRefFrames;
    u32 fullness;
    u32 minusPoc;
	u32 poc_interval;
	
    u32 maxFrameNum;
    u32 maxLongTermFrameIdx;
    u32 prevRefFrameNum;
    u32 lastContainsMmco5;
    u32 flushed;
    u32 picSizeInMbs;
    u32 dirMvOffset;
    u32 interlaced;

    u32 numOut;

    struct list_head memList;
    struct list_head dpbList;
    struct list_head dpbFree;
    struct list_head outList;

    /* flag to prevent output when display smoothing is used and second field
     * of a picture was just decoded */
    //u32 noOutput;

	u32 picbuffsie;
    i32 lastPicOrderCnt;

    dpbPicture_t dpbSlot[16+1];

	u32 fieldmark;
	u32 ts_en;
} dpbStorage_t;

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

u32 h264bsdInitDpb(dpbStorage_t * dpb,
                   u32 picSizeInMbs,
                   u32 dpbSize,
                   u32 numRefFrames, u32 maxFrameNum,
                   u32 monoChrome, u32 isHighSupported, u32 ts_en);

u32 h264bsdResetDpb(dpbStorage_t * dpb,
                    u32 picSizeInMbs,
                    u32 dpbSize,
                    u32 numRefFrames, u32 maxFrameNum,
                    u32 monoChrome, u32 isHighSupported, u32 ts_en);

u8 *h264bsdGetRefPicDataVlcMode(const dpbStorage_t * dpb, u32 index,
    u32 fieldMode);

u32 h264bsdMarkDecRefPic(dpbStorage_t * dpb,
                         /*@null@ */ const decRefPicMarking_t * mark,
                         u32 picStruct, u32 frameNum, i32 *picOrderCnt,
                         u32 isIdr, u32 picId, u32 numErrMbs, u32 isItype);

u32 h264bsdDpbOutputPicture(dpbStorage_t * dpb, void * pOutput);

void h264bsdFlushDpb(dpbStorage_t * dpb);

void h264bsdFreeDpb(dpbStorage_t * dpb);

void ShellSort(dpbStorage_t * dpb, u32 *list, u32 type, i32 par);
void ShellSortF(dpbStorage_t * dpb, u32 *list, u32 type, /*u32 parity,*/ i32 par);

void SetPicNums(dpbStorage_t * dpb, u32 currFrameNum);

void h264DpbUpdateOutputList(dpbStorage_t * dpb, u32 picStruct);

u32 Mmcop5(dpbStorage_t * dpb, u32 picStruct);

u32 OutBufFree(dpbStorage_t *dpb, dpbOutPicture_t *p);
i32 markErrorDpbSlot(dpbStorage_t *dpb);

}

#endif /* #ifdef H264HWD_DPB_H */
