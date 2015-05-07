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
--  $RCSfile: h264hwd_dpb.c,v $
--  $Date: 2010/03/29 14:07:01 $
--  $Revision: 1.29 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#define ALOG_TAG "DPB"
#include <stdlib.h>
#include <unistd.h>
#include "h264hwd_cfg.h"
#include "h264hwd_dpb.h"
#include "h264hwd_slice_header.h"
#include "h264hwd_image.h"
#include "h264hwd_util.h"
#include "h264decapi.h"
#include "basetype.h"
#include "dwl.h"
#include <utils/Log.h>


namespace android {

#define SHOW_DPB        0
#define SHOW_SMALL_POC  0

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/* Function style implementation for IS_REFERENCE() macro to fix compiler
 * warnings */
static u32 IsReference( const dpbPicture_t *a, const u32 f ) {
    switch(f) {
        case TOPFIELD: return a->status[0] && a->status[0] != EMPTY;
        case BOTFIELD: return a->status[1] && a->status[1] != EMPTY;
        default: return a->status[0] && a->status[0] != EMPTY &&
                     a->status[1] && a->status[1] != EMPTY;
    }
}

static u32 IsReferenceField( const dpbPicture_t *a)
{
    return (a->status[0] != UNUSED && a->status[0] != EMPTY) ||
        (a->status[1] != UNUSED && a->status[1] != EMPTY);
}

static u32 IsExisting(const dpbPicture_t *a, const u32 f)
{
    if(f < FRAME)
    {
        return  (a->status[f] > NON_EXISTING) &&
            (a->status[f] != EMPTY);
    }
    else
    {
        return (a->status[0] > NON_EXISTING) &&
            (a->status[0] != EMPTY) &&
            (a->status[1] > NON_EXISTING) &&
            (a->status[1] != EMPTY);
    }
}

static u32 IsShortTerm(const dpbPicture_t *a, const u32 f)
{
    if((f < FRAME))
    {
        return (a->status[f] == NON_EXISTING || a->status[f] == SHORT_TERM);
    }
    else
    {
        return (a->status[0] == NON_EXISTING || a->status[0] == SHORT_TERM) &&
            (a->status[1] == NON_EXISTING || a->status[1] == SHORT_TERM);
    }
}

static u32 IsShortTermField(const dpbPicture_t *a)
{
    return (a->status[0] == NON_EXISTING || a->status[0] == SHORT_TERM) ||
        (a->status[1] == NON_EXISTING || a->status[1] == SHORT_TERM);
}

static u32 IsLongTerm(const dpbPicture_t *a, const u32 f)
{
    if(f < FRAME)
    {
        return a->status[f] == LONG_TERM;
    }
    else
    {
        return a->status[0] == LONG_TERM && a->status[1] == LONG_TERM;
    }
}

static u32 IsLongTermField(const dpbPicture_t *a)
{
    return (a->status[0] == LONG_TERM) || (a->status[1] == LONG_TERM);
}

static u32 IsUnused(const dpbPicture_t *a, const u32 f)
{
    if(f < FRAME)
    {
        return (a->status[f] == UNUSED);
    }
    else
    {
        return (a->status[0] == UNUSED) && (a->status[1] == UNUSED);
    }
}

static void SetStatus(dpbPicture_t *pic,const dpbPictureStatus_e s,
                      const u32 f)
{
    if (f < FRAME)
    {
        pic->status[f] = s;
    }
    else
    {
        pic->status[0] = pic->status[1] = s;
    }
}

static void SetPoc(dpbPicture_t *pic, const i32 *poc, const u32 f)
{
    if (f < FRAME)
    {
        #if SHOW_DPB
            ALOGE("field set poc %d ts high %d low %d\n", poc[f], pic->TimeHigh, pic->TimeLow);
        #endif
        pic->picOrderCnt[f] = poc[f];
    }
    else
    {
        #if SHOW_DPB
            ALOGE("frame set poc %d %d ts high %d low %d\n", poc[0], poc[1], pic->TimeHigh, pic->TimeLow);
        #endif
        pic->picOrderCnt[0] = poc[0];
        pic->picOrderCnt[1] = poc[1];
    }
}

i32 GetPoc(dpbPicture_t *pic)
{
    i32 poc0 = (pic->status[0] == EMPTY ? 0x7FFFFFFF : pic->picOrderCnt[0]);
    i32 poc1 = (pic->status[1] == EMPTY ? 0x7FFFFFFF : pic->picOrderCnt[1]);
    return MIN(poc0,poc1);
}

#define IS_REFERENCE(a,f)       IsReference(a,f)
#define IS_EXISTING(a,f)        IsExisting((a),f)
#define IS_REFERENCE_F(a)       IsReferenceField((a))
#define IS_SHORT_TERM(a,f)      IsShortTerm((a),f)
#define IS_SHORT_TERM_F(a)      IsShortTermField((a))
#define IS_LONG_TERM(a,f)       IsLongTerm((a),f)
#define IS_LONG_TERM_F(a)       IsLongTermField((a))
#define IS_UNUSED(a,f)          IsUnused((a),f)
#define SET_STATUS(pic,s,f)     SetStatus((pic),s,f)
#define SET_POC(pic,poc,f)      SetPoc((pic),poc,f)
#define GET_POC(pic)            GetPoc((pic))

#define MAX_NUM_REF_IDX_L0_ACTIVE 16

#define MEM_STAT_DPB 0x1
#define MEM_STAT_OUT 0x2
#define INVALID_MEM_IDX 0xFF

static u32 DpbBufFree(dpbStorage_t *dpb, dpbPicture_t *p);

#define DISPLAY_SMOOTHING (dpb->totBuffers > dpb->dpbSize + 1)
/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

static i32 ComparePictures(const void *ptr1, const void *ptr2);
static i32 ComparePicturesB(const void *ptr1, const void *ptr2, i32 currPoc);

static i32 CompareFields(const void *ptr1, const void *ptr2);
static i32 CompareFieldsB(const void *ptr1, const void *ptr2, i32 currPoc);

static u32 Mmcop1(dpbStorage_t * dpb, u32 currPicNum, u32 differenceOfPicNums,
                  u32 picStruct);

static u32 Mmcop2(dpbStorage_t * dpb, u32 longTermPicNum, u32 picStruct);

static u32 Mmcop3(dpbStorage_t * dpb, u32 currPicNum, u32 differenceOfPicNums,
                  u32 longTermFrameIdx, u32 picStruct);

static u32 Mmcop4(dpbStorage_t * dpb, u32 maxLongTermFrameIdx, u32 picStruct);

u32 Mmcop5(dpbStorage_t * dpb, u32 picStruct);

static u32 Mmcop6(dpbStorage_t * dpb, u32 frameNum, i32 * picOrderCnt,
                  u32 longTermFrameIdx, u32 picStruct);

static u32 SlidingWindowRefPicMarking(dpbStorage_t * dpb);

static /*@null@ */ dpbPicture_t *FindSmallestPicOrderCnt(dpbStorage_t * dpb, u32 flush);

static u32 OutputPicture(dpbStorage_t * dpb, u32 flush);
static void h264bsdDpbDump(dpbStorage_t * dpb);

dpbMem_t* newDpbMem(dpbStorage_t *dpb)
{
    dpbMem_t *p = (dpbMem_t*)malloc(sizeof(dpbMem_t));
    if (p) {
        memset(p, 0, sizeof(dpbMem_t));
        if (0 == VPUMallocLinear(&p->mem, dpb->picbuffsie)) {
            p->phy_addr = (u32)p->mem.phy_addr;
            p->vir_addr = (u32)p->mem.vir_addr;
            p->memStatus = MEM_AVAIL;
            list_add_tail(&p->list, &dpb->memList);
            return p;
        }
        free(p);
    }
    return NULL;
}

i32 freeDpbMem(dpbMem_t** p)
{
    dpbMem_t *ptr = NULL;
    if (!p || !(ptr = *p) || !DPBMEM_IS_AVAIL(ptr)) {
        ALOGE("freeDpbMem invalid parameter\n");
        //if (ptr)
        //    free(ptr);
        if (p)
            *p = NULL;
        return -1;
    }

	DPBDEBUG("freeDpbMem 0x%x\n", ptr->mem.phy_addr);

    VPUFreeLinear(&ptr->mem);
    list_del_init(&ptr->list);
    ptr->memStatus = 0;
    ptr->phy_addr  = 0;
    ptr->vir_addr  = 0;
    free(ptr);
    *p = NULL;
    return 0;
}

i32 freeDpbMemFromDpb(dpbMem_t** p)
{
    dpbMem_t *ptr;
    if (!p || !(ptr = *p) || !DPBMEM_IS_AVAIL(ptr)) {
        ALOGE("freeDpbMemFromDpb invalid parameter\n");
        //free(p);
        return -1;
    }

    ptr->memStatus &= ~MEM_DPB_USE;
    ptr->memStatus |= MEM_DPBDONE;

	DPBDEBUG("freeDpbMemFromDpb ptr->memStatus=%d\n", ptr->memStatus);

    if (DPBMEM_IS_DPBDONE(ptr) && DPBMEM_IS_OUTDONE(ptr)) {
        return freeDpbMem(p);
    }
    return 0;
}

i32 freeDpbMemFromOut(dpbMem_t** p)
{
    dpbMem_t *ptr = NULL;
    if (!p || !(ptr = *p) || !DPBMEM_IS_AVAIL(ptr)) {
        ALOGE("freeDpbMemFromOut invalid parameter\n");
        //free(p);
        return -1;
    }

    ptr->memStatus &= ~MEM_OUT_USE;
    ptr->memStatus |= MEM_OUTDONE;

	DPBDEBUG("freeDpbMemFromOut ptr->memStatus=%d\n", ptr->memStatus);
    if (DPBMEM_IS_DPBDONE(ptr) && DPBMEM_IS_OUTDONE(ptr)) {
        return freeDpbMem(p);
    }
    return 0;
}

dpbPicture_t* newDpbSlot(dpbStorage_t *dpb)
{
	DPBDEBUG("newDpbSlot#\n");
    if (list_empty(&dpb->dpbFree)) {
        DPBDEBUG("no dpbSlot to allocate, error !!\n");
        //free(dpb);
        /*return NULL;*/
		if(SlidingWindowRefPicMarking(dpb) == HANTRO_NOK)
		{
			DPBDEBUG("no dpbSlot to FREE, error !!\n");
			return NULL;
		}
    }

	if (!list_empty(&dpb->dpbFree))
	{
		dpbPicture_t *p = list_entry(dpb->dpbFree.next, dpbPicture_t, list);
	    list_del_init(&p->list);
	    memset(p, 0, sizeof(dpbPicture_t));
	    return p;
	}else{
		DPBDEBUG("DPB also not free one dpbslot!\n");
		return NULL;
	}

}

i32 freeDpbSlot(dpbStorage_t *dpb, dpbPicture_t** p)
{
    dpbPicture_t *ptr = NULL;
    if (!p || !(ptr = *p)) {
        ALOGE("freeDpbSlot invalid parameter ptr is 0x%.8x\n", (u32)p);
        return -1;
    }

	DPBDEBUG("freeDpbSlot# %d\n", GET_POC(ptr));

    list_del_init(&ptr->list);
    memset(ptr, 0, sizeof(dpbPicture_t));
    list_add_tail(&ptr->list, &dpb->dpbFree);
    *p = NULL;
    return 0;
}

dpbOutPicture_t* newOutSlot(dpbStorage_t *dpb)
{
    dpbOutPicture_t *p = (dpbOutPicture_t*)malloc(sizeof(dpbOutPicture_t));
    if (p) {
        memset(p, 0, sizeof(dpbOutPicture_t));
        list_add_tail(&p->list, &dpb->outList);
    }
    return p;
}

i32 freeOutSlot(dpbOutPicture_t* p)
{
    if (!p) {
        ALOGE("freeOutSlot invalid zero parameter\n");
        return -1;
    }

    list_del_init(&p->list);
    free(p);
    return 0;
}

i32 markErrorDpbSlot(dpbStorage_t *dpb)
{
    i32 ret = 0;
	dpbOutPicture_t *pOut;
	
    //ALOGE("markErrorDpbSlot");
    if (dpb->currentOut) {
        //ALOGE("clear currentOut  idx %d", dpb->currentOut->dpbIdx);
		list_for_each_entry(pOut, &dpb->outList, list){
			if(pOut->data == dpb->currentOut->data)	//find the outslot in outlist, not clear currentOut
				return ret;
		}
        if (dpb->currentOut->toBeDisplayed && dpb->fullness)
            dpb->fullness--;
        if (IS_REFERENCE_F(dpb->currentOut))
            dpb->numRefFrames--;
        ret = freeDpbMem(&dpb->currentOut->data);
        if (ret) {
            ALOGE("freeDpbMem error in markErrorDpbSlot\n");
        }
        if (dpb->buffer[dpb->currentOut->dpbIdx]) {
            ret = freeDpbSlot(dpb, &dpb->buffer[dpb->currentOut->dpbIdx]);
            if (ret) {
                ALOGE("freeDpbSlot error in markErrorDpbSlot\n");
            }
        }
        if (dpb->previousOut == dpb->currentOut) {
            ALOGE("clear previousOut");
            dpb->previousOut = NULL;
        }
    }
    dpb->currentOut = NULL;
    if (dpb->previousOut && dpb->previousOut->isFieldPic != 0 && dpb->previousOut->isFieldPic != 3) {
        //ALOGE("clear previousOut idx %d", dpb->previousOut->dpbIdx);
        if (dpb->previousOut->toBeDisplayed && dpb->fullness)
            dpb->fullness--;
        if (IS_REFERENCE_F(dpb->previousOut))
            dpb->numRefFrames--;
        ret = freeDpbMem(&dpb->previousOut->data);
        if (ret) {
            ALOGE("freeDpbMem error in markErrorDpbSlot\n");
        }
        if (dpb->buffer[dpb->previousOut->dpbIdx]) {
            ret = freeDpbSlot(dpb, &dpb->buffer[dpb->previousOut->dpbIdx]);
            if (ret) {
                ALOGE("freeDpbSlot error in markErrorDpbSlot\n");
            }
        }
    }
    dpb->previousOut = NULL;
    return ret;
}

/*------------------------------------------------------------------------------

    Function: ComparePictures

        Functional description:
            Function to compare dpb pictures, used by the qsort() function.
            Order of the pictures after sorting shall be as follows:
                1) short term reference pictures starting with the largest
                   picNum
                2) long term reference pictures starting with the smallest
                   longTermPicNum
                3) pictures unused for reference but needed for display
                4) other pictures

        Returns:
            -1      pic 1 is greater than pic 2
             0      equal from comparison point of view
             1      pic 2 is greater then pic 1

------------------------------------------------------------------------------*/

i32 ComparePictures(const void *ptr1, const void *ptr2)
{

/* Variables */

    const dpbPicture_t *pic1, *pic2;

/* Code */

    ASSERT(ptr1);
    ASSERT(ptr2);

    if (!ptr1 && !ptr2)
        return 0;
    else if (ptr1 && !ptr2)
        return (-1);
    else if (!ptr1 && ptr2)
        return (1);

    pic1 = (dpbPicture_t *) ptr1;
    pic2 = (dpbPicture_t *) ptr2;

    /* both are non-reference pictures, check if needed for display */
    if(!IS_REFERENCE(pic1, FRAME) && !IS_REFERENCE(pic2, FRAME))
    {
        if(pic1->toBeDisplayed && !pic2->toBeDisplayed)
            return (-1);
        else if(!pic1->toBeDisplayed && pic2->toBeDisplayed)
            return (1);
        else
            return (0);
    }
    /* only pic 1 needed for reference -> greater */
    else if(!IS_REFERENCE(pic2, FRAME))
        return (-1);
    /* only pic 2 needed for reference -> greater */
    else if(!IS_REFERENCE(pic1, FRAME))
        return (1);
    /* both are short term reference pictures -> check picNum */
    else if(IS_SHORT_TERM(pic1, FRAME) && IS_SHORT_TERM(pic2, FRAME))
    {
        if(pic1->picNum > pic2->picNum)
            return (-1);
        else if(pic1->picNum < pic2->picNum)
            return (1);
        else
            return (0);
    }
    /* only pic 1 is short term -> greater */
    else if(IS_SHORT_TERM(pic1, FRAME))
        return (-1);
    /* only pic 2 is short term -> greater */
    else if(IS_SHORT_TERM(pic2, FRAME))
        return (1);
    /* both are long term reference pictures -> check picNum (contains the
     * longTermPicNum */
    else
    {
        if(pic1->picNum > pic2->picNum)
            return (1);
        else if(pic1->picNum < pic2->picNum)
            return (-1);
        else
            return (0);
    }
}

i32 CompareFields(const void *ptr1, const void *ptr2)
{

/* Variables */

    dpbPicture_t *pic1, *pic2;

/* Code */
    if (!ptr1 && !ptr2)
        return 0;
    else if (ptr1 && !ptr2)
        return (-1);
    else if (!ptr1 && ptr2)
        return (1);

    ASSERT(ptr1);
    ASSERT(ptr2);

    pic1 = (dpbPicture_t *) ptr1;
    pic2 = (dpbPicture_t *) ptr2;

    /* both are non-reference pictures, check if needed for display */
    if(!IS_REFERENCE_F(pic1) && !IS_REFERENCE_F(pic2))
        return (0);
    /* only pic 1 needed for reference -> greater */
    else if(!IS_REFERENCE_F(pic2))
        return (-1);
    /* only pic 2 needed for reference -> greater */
    else if(!IS_REFERENCE_F(pic1))
        return (1);
    /* both are short term reference pictures -> check picNum */
    else if(IS_SHORT_TERM_F(pic1) && IS_SHORT_TERM_F(pic2))
    {
        if(pic1->picNum > pic2->picNum)
            return (-1);
        else if(pic1->picNum < pic2->picNum)
            return (1);
        else
            return (0);
    }
    /* only pic 1 is short term -> greater */
    else if(IS_SHORT_TERM_F(pic1))
        return (-1);
    /* only pic 2 is short term -> greater */
    else if(IS_SHORT_TERM_F(pic2))
        return (1);
    /* both are long term reference pictures -> check picNum (contains the
     * longTermPicNum */
    else
    {
        if(pic1->picNum > pic2->picNum)
            return (1);
        else if(pic1->picNum < pic2->picNum)
            return (-1);
        else
            return (0);
    }
}

/*------------------------------------------------------------------------------

    Function: ComparePicturesB

        Functional description:
            Function to compare dpb pictures, used by the qsort() function.
            Order of the pictures after sorting shall be as follows:
                1) short term reference pictures with POC less than current POC
                   in descending order
                2) short term reference pictures with POC greater than current
                   POC in ascending order
                3) long term reference pictures starting with the smallest
                   longTermPicNum

        Returns:
            -1      pic 1 is greater than pic 2
             0      equal from comparison point of view
             1      pic 2 is greater then pic 1

------------------------------------------------------------------------------*/

i32 ComparePicturesB(const void *ptr1, const void *ptr2, i32 currPoc)
{

/* Variables */

    dpbPicture_t *pic1, *pic2;
    i32 poc1, poc2;

/* Code */

    ASSERT(ptr1);
    ASSERT(ptr2);

    if (!ptr1 && !ptr2)
        return 0;
    else if (ptr1 && !ptr2)
        return (-1);
    else if (!ptr1 && ptr2)
        return (1);

    pic1 = (dpbPicture_t *) ptr1;
    pic2 = (dpbPicture_t *) ptr2;

    /* both are non-reference pictures */
    if(!IS_REFERENCE(pic1, FRAME) && !IS_REFERENCE(pic2, FRAME))
        return (0);
    /* only pic 1 needed for reference -> greater */
    else if(!IS_REFERENCE(pic2, FRAME))
        return (-1);
    /* only pic 2 needed for reference -> greater */
    else if(!IS_REFERENCE(pic1, FRAME))
        return (1);
    /* both are short term reference pictures -> check picOrderCnt */
    else if(IS_SHORT_TERM(pic1, FRAME) && IS_SHORT_TERM(pic2, FRAME))
    {
        poc1 = MIN(pic1->picOrderCnt[0], pic1->picOrderCnt[1]);
        poc2 = MIN(pic2->picOrderCnt[0], pic2->picOrderCnt[1]);

        if(poc1 < currPoc && poc2 < currPoc)
            return (poc1 < poc2 ? 1 : -1);
        else
            return (poc1 < poc2 ? -1 : 1);
    }
    /* only pic 1 is short term -> greater */
    else if(IS_SHORT_TERM(pic1, FRAME))
        return (-1);
    /* only pic 2 is short term -> greater */
    else if(IS_SHORT_TERM(pic2, FRAME))
        return (1);
    /* both are long term reference pictures -> check picNum (contains the
     * longTermPicNum */
    else
    {
        if(pic1->picNum > pic2->picNum)
            return (1);
        else if(pic1->picNum < pic2->picNum)
            return (-1);
        else
            return (0);
    }
}

i32 CompareFieldsB(const void *ptr1, const void *ptr2, i32 currPoc)
{

/* Variables */

    dpbPicture_t *pic1, *pic2;
    i32 poc1, poc2;

/* Code */

    ASSERT(ptr1);
    ASSERT(ptr2);

    if (!ptr1 && !ptr2)
        return 0;
    else if (ptr1 && !ptr2)
        return (-1);
    else if (!ptr1 && ptr2)
        return (1);

    pic1 = (dpbPicture_t *) ptr1;
    pic2 = (dpbPicture_t *) ptr2;

    /* both are non-reference pictures */
    if(!IS_REFERENCE_F(pic1) && !IS_REFERENCE_F(pic2))
        return (0);
    /* only pic 1 needed for reference -> greater */
    else if(!IS_REFERENCE_F(pic2))
        return (-1);
    /* only pic 2 needed for reference -> greater */
    else if(!IS_REFERENCE_F(pic1))
        return (1);
    /* both are short term reference pictures -> check picOrderCnt */
    else if(IS_SHORT_TERM_F(pic1) && IS_SHORT_TERM_F(pic2))
    {
        poc1 = IS_SHORT_TERM(pic1, FRAME) ?
            MIN(pic1->picOrderCnt[0], pic1->picOrderCnt[1]) :
            IS_SHORT_TERM(pic1, TOPFIELD) ? pic1->picOrderCnt[0] :
            pic1->picOrderCnt[1];
        poc2 = IS_SHORT_TERM(pic2, FRAME) ?
            MIN(pic2->picOrderCnt[0], pic2->picOrderCnt[1]) :
            IS_SHORT_TERM(pic2, TOPFIELD) ? pic2->picOrderCnt[0] :
            pic2->picOrderCnt[1];

        if(poc1 <= currPoc && poc2 <= currPoc)
            return (poc1 < poc2 ? 1 : -1);
        else
            return (poc1 < poc2 ? -1 : 1);
    }
    /* only pic 1 is short term -> greater */
    else if(IS_SHORT_TERM_F(pic1))
        return (-1);
    /* only pic 2 is short term -> greater */
    else if(IS_SHORT_TERM_F(pic2))
        return (1);
    /* both are long term reference pictures -> check picNum (contains the
     * longTermPicNum */
    else
    {
        if(pic1->picNum > pic2->picNum)
            return (1);
        else if(pic1->picNum < pic2->picNum)
            return (-1);
        else
            return (0);
    }
}

/*------------------------------------------------------------------------------

    Function: FindDpbPic

        Functional description:
            Function to find a reference picture from the buffer. The picture
            to be found is identified by picNum and isShortTerm flag.

        Returns:
            index of the picture in the buffer
            -1 if the specified picture was not found in the buffer

------------------------------------------------------------------------------*/

static dpbPicture_t* FindDpbPic(dpbStorage_t * dpb, i32 picNum, u32 isShortTerm,
                      u32 field)
{

/* Variables */

    dpbPicture_t *tmp;
    dpbPicture_t *ret = NULL;

/* Code */

    if(isShortTerm)
    {
        list_for_each_entry(tmp, &dpb->dpbList, list) {
            if (IS_SHORT_TERM(tmp, field) && tmp->frameNum == picNum) {
                ret = tmp;
                break;
            }
        }
    }
    else
    {
        ASSERT(picNum >= 0);
        list_for_each_entry(tmp, &dpb->dpbList, list) {
            if (IS_LONG_TERM(tmp, field) && tmp->picNum == picNum) {
                ret = tmp;
                break;
            }
        }
    }

    return ret;
}

/*------------------------------------------------------------------------------

    Function: Mmcop1

        Functional description:
            Function to mark a short-term reference picture unused for
            reference, memory_management_control_operation equal to 1

        Returns:
            HANTRO_OK      success
            HANTRO_NOK     failure, picture does not exist in the buffer

------------------------------------------------------------------------------*/

static u32 Mmcop1(dpbStorage_t * dpb, u32 currPicNum, u32 differenceOfPicNums,
                  u32 picStruct)
{

/* Variables */

    i32 picNum;
    u32 field = FRAME;
    dpbPicture_t *dpbPic;

/* Code */

    ASSERT(currPicNum < dpb->maxFrameNum);

    if(picStruct == FRAME)
    {
        picNum = (i32) currPicNum - (i32) differenceOfPicNums;
        if(picNum < 0)
            picNum += dpb->maxFrameNum;
    }
    else
    {
        picNum = (i32) currPicNum *2 + 1 - (i32) differenceOfPicNums;

        if(picNum < 0)
            picNum += dpb->maxFrameNum * 2;
        field = (picNum & 1) ^ (u32)(picStruct == TOPFIELD);
        picNum /= 2;
    }

    dpbPic = FindDpbPic(dpb, picNum, HANTRO_TRUE, field);
    if(NULL == dpbPic)
        return (HANTRO_NOK);

    SET_STATUS(dpbPic, UNUSED, field);
    if(IS_UNUSED(dpbPic, FRAME))
    {
        DpbBufFree(dpb, dpbPic);
    }

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: Mmcop2

        Functional description:
            Function to mark a long-term reference picture unused for
            reference, memory_management_control_operation equal to 2

        Returns:
            HANTRO_OK      success
            HANTRO_NOK     failure, picture does not exist in the buffer

------------------------------------------------------------------------------*/

static u32 Mmcop2(dpbStorage_t * dpb, u32 longTermPicNum, u32 picStruct)
{

/* Variables */

    u32 field = FRAME;
    dpbPicture_t *dpbPic;

/* Code */

    if(picStruct != FRAME)
    {
        field = (longTermPicNum & 1) ^ (u32)(picStruct == TOPFIELD);
        longTermPicNum /= 2;
    }
    dpbPic = FindDpbPic(dpb, (i32) longTermPicNum, HANTRO_FALSE, field);
    if(NULL == dpbPic)
        return (HANTRO_NOK);

    SET_STATUS(dpbPic, UNUSED, field);
    if(IS_UNUSED(dpbPic, FRAME))
    {
        DpbBufFree(dpb, dpbPic);
    }

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: Mmcop3

        Functional description:
            Function to assing a longTermFrameIdx to a short-term reference
            frame (i.e. to change it to a long-term reference picture),
            memory_management_control_operation equal to 3

        Returns:
            HANTRO_OK      success
            HANTRO_NOK     failure, short-term picture does not exist in the
                           buffer or is a non-existing picture, or invalid
                           longTermFrameIdx given

------------------------------------------------------------------------------*/

static u32 Mmcop3(dpbStorage_t * dpb, u32 currPicNum, u32 differenceOfPicNums,
                  u32 longTermFrameIdx, u32 picStruct)
{

/* Variables */

    i32 picNum;
    u32 field = FRAME;
    dpbPicture_t *dpbPic;

/* Code */

    ASSERT(dpb);
    ASSERT(currPicNum < dpb->maxFrameNum);

    if(picStruct == FRAME)
    {
        picNum = (i32) currPicNum - (i32) differenceOfPicNums;
        if(picNum < 0)
            picNum += dpb->maxFrameNum;
    }
    else
    {
        picNum = (i32) currPicNum *2 + 1 - (i32) differenceOfPicNums;

        if(picNum < 0)
            picNum += dpb->maxFrameNum * 2;
        field = (picNum & 1) ^ (u32)(picStruct == TOPFIELD);
        picNum /= 2;
    }

    if((dpb->maxLongTermFrameIdx == NO_LONG_TERM_FRAME_INDICES) ||
       (longTermFrameIdx > dpb->maxLongTermFrameIdx))
        return (HANTRO_NOK);

    /* check if a long term picture with the same longTermFrameIdx already
     * exist and remove it if necessary */
    list_for_each_entry(dpbPic, &dpb->dpbList, list) {
        if (IS_LONG_TERM_F(dpbPic) &&
            (u32) dpbPic->picNum == longTermFrameIdx &&
            dpbPic->frameNum != picNum)
        {
            SET_STATUS(dpbPic, UNUSED, FRAME);
            if(IS_UNUSED(dpbPic, FRAME))
            {
                DpbBufFree(dpb, dpbPic);
            }
            break;
        }
    }

    dpbPic = FindDpbPic(dpb, picNum, HANTRO_TRUE, field);
    if(NULL == dpbPic)
        return (HANTRO_NOK);
    if(!IS_EXISTING(dpbPic, field))
        return (HANTRO_NOK);

    SET_STATUS(dpbPic, LONG_TERM, field);
    dpbPic->picNum = (i32) longTermFrameIdx;

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: Mmcop4

        Functional description:
            Function to set maxLongTermFrameIdx,
            memory_management_control_operation equal to 4

        Returns:
            HANTRO_OK      success

------------------------------------------------------------------------------*/

static u32 Mmcop4(dpbStorage_t * dpb, u32 maxLongTermFrameIdx, u32 picStruct)
{

/* Variables */

    //u32 i;
    dpbPicture_t *dpbPic, *tmp;

/* Code */

    dpb->maxLongTermFrameIdx = maxLongTermFrameIdx;

    list_for_each_entry_safe(dpbPic, tmp, &dpb->dpbList, list) {
        if (IS_LONG_TERM(dpbPic, TOPFIELD) &&
            (((u32) dpbPic->picNum > maxLongTermFrameIdx) ||
            (dpb->maxLongTermFrameIdx == NO_LONG_TERM_FRAME_INDICES))) {
            SET_STATUS(dpbPic, UNUSED, TOPFIELD);
            if(IS_UNUSED(dpbPic, FRAME))
            {
                DpbBufFree(dpb, dpbPic);
            }
        }
        if (IS_LONG_TERM(dpbPic, BOTFIELD) &&
            (((u32) dpbPic->picNum > maxLongTermFrameIdx) ||
            (dpb->maxLongTermFrameIdx == NO_LONG_TERM_FRAME_INDICES))) {
            SET_STATUS(dpbPic, UNUSED, BOTFIELD);
            if(IS_UNUSED(dpbPic, FRAME))
            {
                DpbBufFree(dpb, dpbPic);
            }
        }
    }
    /*
    for(i = 0; i <= dpb->dpbSize; i++)
    {
        if(dpb->buffer[i] && IS_LONG_TERM(dpb->buffer[i], TOPFIELD) &&
           (((u32) dpb->buffer[i]->picNum > maxLongTermFrameIdx) ||
            (dpb->maxLongTermFrameIdx == NO_LONG_TERM_FRAME_INDICES)))
        {
            SET_STATUS(dpb->buffer[i], UNUSED, TOPFIELD);
            if(IS_UNUSED(dpb->buffer[i], FRAME))
            {
                DpbBufFree(dpb, dpb->buffer[i]);
            }
        }
        if(dpb->buffer[i] && IS_LONG_TERM(dpb->buffer[i], BOTFIELD) &&
           (((u32) dpb->buffer[i]->picNum > maxLongTermFrameIdx) ||
            (dpb->maxLongTermFrameIdx == NO_LONG_TERM_FRAME_INDICES)))
        {
            SET_STATUS(dpb->buffer[i], UNUSED, BOTFIELD);
            if(IS_UNUSED(dpb->buffer[i], FRAME))
            {
                DpbBufFree(dpb, dpb->buffer[i]);
            }
        }
    }
    */

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: Mmcop5

        Functional description:
            Function to mark all reference pictures unused for reference and
            set maxLongTermFrameIdx to NO_LONG_TERM_FRAME_INDICES,
            memory_management_control_operation equal to 5. Function flushes
            the buffer and places all pictures that are needed for display into
            the output buffer.

        Returns:
            HANTRO_OK      success

------------------------------------------------------------------------------*/

u32 Mmcop5(dpbStorage_t * dpb, u32 picStruct)
{
/* Variables */

    dpbPicture_t *dpbPic, *tmp;

/* Code */
#if SHOW_DPB
    ALOGE("Mmcop5\n");
#endif

    list_for_each_entry_safe(dpbPic, tmp, &dpb->dpbList, list) {
        if (IS_REFERENCE_F(dpbPic)) {
            SET_STATUS(dpbPic, UNUSED, FRAME);
            DpbBufFree(dpb, dpbPic);
        }
    }

    /* output all pictures */
    while(OutputPicture(dpb, 1) == HANTRO_OK)
        ;
    dpb->numRefFrames = 0;
    dpb->maxLongTermFrameIdx = NO_LONG_TERM_FRAME_INDICES;
    dpb->prevRefFrameNum = 0;
    dpb->lastPicOrderCnt = 0x7ffffff0;

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: Mmcop6

        Functional description:
            Function to assign longTermFrameIdx to the current picture,
            memory_management_control_operation equal to 6

        Returns:
            HANTRO_OK      success
            HANTRO_NOK     invalid longTermFrameIdx or no room for current
                           picture in the buffer

------------------------------------------------------------------------------*/

static u32 Mmcop6(dpbStorage_t * dpb, u32 frameNum, i32 * picOrderCnt,
                  u32 longTermFrameIdx, u32 picStruct)
{

/* Variables */

    u32 i;
    dpbPicture_t *dpbPic;

/* Code */

    ASSERT(frameNum < dpb->maxFrameNum);

    if((dpb->maxLongTermFrameIdx != NO_LONG_TERM_FRAME_INDICES) &&
       (longTermFrameIdx > dpb->maxLongTermFrameIdx))
        return (HANTRO_NOK);

    /* check if a long term picture with the same longTermFrameIdx already
     * exist and remove it if necessary */
    list_for_each_entry(dpbPic, &dpb->dpbList, list) {
        if (IS_LONG_TERM_F(dpbPic) &&
           (u32) dpbPic->picNum == longTermFrameIdx &&
           dpbPic != dpb->currentOut)
        {
            SET_STATUS(dpbPic, UNUSED, FRAME);
            if(IS_UNUSED(dpbPic, FRAME)) {
                DpbBufFree(dpb, dpbPic);
            }
            break;
        }
    }
/*
    for(i = 0; i <= dpb->dpbSize; i++)
        if(dpb->buffer[i] && IS_LONG_TERM_F(dpb->buffer[i]) &&
           (u32) dpb->buffer[i]->picNum == longTermFrameIdx &&
           dpb->buffer[i] != dpb->currentOut)
        {
            SET_STATUS(dpb->buffer[i], UNUSED, FRAME);
            if(IS_UNUSED(dpb->buffer[i], FRAME))
            {
                DpbBufFree(dpb, dpb->buffer[i]);
            }
            break;
        }
*/
    dpbPicture_t *currentOut = dpb->currentOut;
    /* another field of current frame already marked */
    if (picStruct != FRAME && currentOut->status[(u32)!picStruct] != EMPTY)
    {
        currentOut->picNum = (i32) longTermFrameIdx;
        SET_POC(currentOut, picOrderCnt, picStruct);
        SET_STATUS(currentOut, LONG_TERM, picStruct);
        return (HANTRO_OK);
    }
    else if(dpb->numRefFrames <= dpb->maxRefFrames)
    {
        currentOut->frameNum = frameNum;
        currentOut->picNum = (i32) longTermFrameIdx;
        SET_POC(currentOut, picOrderCnt, picStruct);
        SET_STATUS(currentOut, LONG_TERM, picStruct);
        currentOut->toBeDisplayed = HANTRO_TRUE;
        dpb->numRefFrames++;
        dpb->fullness++;
        return (HANTRO_OK);
    }
    /* if there is no room, return an error */
    else
    {
        #if SHOW_DPB
            ALOGE("no room in mmco6\n");
        #endif
        return (HANTRO_NOK);
    }

}

/*------------------------------------------------------------------------------

    Function: h264bsdMarkDecRefPic

        Functional description:
            Function to perform reference picture marking process. This
            function should be called both for reference and non-reference
            pictures.  Non-reference pictures shall have mark pointer set to
            NULL.

        Inputs:
            dpb         pointer to the DPB data structure
            mark        pointer to reference picture marking commands
            image       pointer to current picture to be placed in the buffer
            frameNum    frame number of the current picture
            picOrderCnt picture order count for the current picture
            isIdr       flag to indicate if the current picture is an
                        IDR picture
            currentPicId    identifier for the current picture, from the
                            application, stored along with the picture
            numErrMbs       number of concealed macroblocks in the current
                            picture, stored along with the picture

        Outputs:
            dpb         'buffer' modified, possible output frames placed into
                        'outBuf'

        Returns:
            HANTRO_OK   success
            HANTRO_NOK  failure

------------------------------------------------------------------------------*/

u32 h264bsdMarkDecRefPic(dpbStorage_t * dpb,
                         const decRefPicMarking_t * mark,
                         u32 picStruct,
                         u32 frameNum, i32 * picOrderCnt,
                         u32 isIdr, u32 currentPicId, u32 numErrMbs, u32 isItype)
{

/* Variables */

    u32 status;
    u32 markedAsLongTerm;
    u32 toBeDisplayed;
    u32 secondField = 0;
    dpbPicture_t *currentOut = dpb->currentOut;

/* Code */

	DPBDEBUG("h264bsdMarkDecRefPic# %d\n", picStruct);
    ASSERT(dpb);
    ASSERT(mark || !isIdr);
    /* removed for XXXXXXXX compliance */
    /*
     * ASSERT(!isIdr ||
     * (frameNum == 0 &&
     * picStruct == FRAME ? MIN(picOrderCnt[0],picOrderCnt[1]) == 0 :
     * picOrderCnt[picStruct] == 0));
     */
    ASSERT(frameNum < dpb->maxFrameNum);
    ASSERT(picStruct <= FRAME);

	if(picStruct < FRAME) {
		if(picStruct == 0)
		{
			currentOut->isIdr = isIdr;
			currentOut->isItype = isItype;
		}
	}else{
		currentOut->isIdr = isIdr;
		currentOut->isItype = isItype;
	}
    currentOut->picId = currentPicId;
    currentOut->numErrMbs = numErrMbs;

    if(picStruct < FRAME) {
        ASSERT(currentOut->status[picStruct] == EMPTY);
        if (currentOut->status[(u32)!picStruct] != EMPTY) {
            secondField = 1;
        }
        currentOut->isFieldPic |= 1 << picStruct;
    } else {
        currentOut->isFieldPic = 0;
    }

    dpb->lastContainsMmco5 = HANTRO_FALSE;
    status = HANTRO_OK;

    toBeDisplayed = HANTRO_TRUE;

    /* non-reference picture, stored for display reordering purposes */
    if(mark == NULL)
    {
        SET_STATUS(currentOut, UNUSED, picStruct);
        currentOut->frameNum = frameNum;
        currentOut->picNum = (i32) frameNum;
        SET_POC(currentOut, picOrderCnt, picStruct);
        /* TODO: if current pic is first field of pic and will be output ->
         * output will only contain first field, second (if present) will
         * be output separately. This shall be fixed when field mode output
         * is implemented */
        if ((!secondField ||   /* first field of frame */
            (!currentOut->toBeDisplayed) /* first already output */ ))
            dpb->fullness++;
        currentOut->toBeDisplayed = toBeDisplayed;

        #if SHOW_DPB
            ALOGE("mark poc %d is NULL index %d\n", *picOrderCnt, currentOut->data->mem.offset);
        #endif
    }
    /* IDR picture */
    else if(isIdr)
    {
        /* flush the buffer */
        (void) Mmcop5(dpb, picStruct);
        /* added for XXXXXXXX compliance */
        dpb->prevRefFrameNum = frameNum;
        /* if noOutputOfPriorPicsFlag was set -> the pictures preceding the
         * IDR picture shall not be output -> set output buffer empty */
        if(mark->noOutputOfPriorPicsFlag)
        {
            while (!list_empty(&dpb->outList)) {
                dpbOutPicture_t *p = list_entry(dpb->outList.next, dpbOutPicture_t, list);
                list_del_init(&p->list);
                OutBufFree(dpb, p);
            }
            /*
            u32 i;
            for (i = 0; i < dpb->numOut; i++)
                OutBufFree(dpb, (dpb->outIndexW+i)%(dpb->dpbSize+1));
            */
            dpb->numOut = 0;
        }

        if(mark->longTermReferenceFlag)
        {
            SET_STATUS(currentOut, LONG_TERM, picStruct);
            dpb->maxLongTermFrameIdx = 0;
        }
        else
        {
            SET_STATUS(currentOut, SHORT_TERM, picStruct);
            dpb->maxLongTermFrameIdx = NO_LONG_TERM_FRAME_INDICES;
        }
        /* changed for XXXXXXXX compliance */
        currentOut->frameNum = frameNum;
        currentOut->picNum = (i32) frameNum;
        SET_POC(currentOut, picOrderCnt, picStruct);
        currentOut->toBeDisplayed = toBeDisplayed;
        dpb->fullness = 1;
        dpb->numRefFrames = 1;
        #if SHOW_DPB
            ALOGE("mark poc %d is idr\n", *picOrderCnt);
        #endif
    }
    /* reference picture */
    else
    {
        #if SHOW_DPB
            ALOGE("mark poc %d reference picture\n", *picOrderCnt);
        #endif
        markedAsLongTerm = HANTRO_FALSE;
        if(mark->adaptiveRefPicMarkingModeFlag)
        {
            const memoryManagementOperation_t *operation;

            operation = mark->operation;    /* = &mark->operation[0] */
            #if SHOW_DPB
                ALOGE("adaptiveRefPicMarkingModeFlag opt 0x%x, mmco=%d\n", operation, operation->memoryManagementControlOperation);
            #endif

            while(operation->memoryManagementControlOperation)
            {

                switch (operation->memoryManagementControlOperation)
                {
                case 1:
                    status = Mmcop1(dpb,
                                    frameNum, operation->differenceOfPicNums,
                                    picStruct);
                    break;

                case 2:
                    status = Mmcop2(dpb, operation->longTermPicNum,
                                    picStruct);
                    break;

                case 3:
                    status = Mmcop3(dpb,
                                    frameNum,
                                    operation->differenceOfPicNums,
                                    operation->longTermFrameIdx,
                                    picStruct);
                    break;

                case 4:
                    status = Mmcop4(dpb, operation->maxLongTermFrameIdx,
                                    picStruct);
                    break;

                case 5:
                    status = Mmcop5(dpb, picStruct);
                    dpb->lastContainsMmco5 = HANTRO_TRUE;
                    frameNum = 0;
                    break;

                case 6:
                    status = Mmcop6(dpb,
                                    frameNum,
                                    picOrderCnt, operation->longTermFrameIdx,
                                    picStruct);
                    if(status == HANTRO_OK)
                        markedAsLongTerm = HANTRO_TRUE;
                    break;

                default:   /* invalid memory management control operation */
                    status = HANTRO_NOK;
                    break;
                }

                if(status != HANTRO_OK)
                {
                    DPBOUTDEBUG("op %d type %d MMCO error found continue!\n", operation - mark->operation, operation->memoryManagementControlOperation);
                    //break;
                }

                operation++;    /* = &mark->operation[i] */
            }
        }
        /* force sliding window marking if first field of current frame was
         * non-reference frame (don't know if this is allowed, but may happen
         * at least in erroneous streams) */
        else if(!secondField || currentOut->status[(u32)!picStruct] == UNUSED)
        {
            #if SHOW_DPB
                ALOGE("SlidingWindowRefPicMarking\n");
            #endif
            status = SlidingWindowRefPicMarking(dpb);
        }
        /* if current picture was not marked as long-term reference by
         * memory management control operation 6 -> mark current as short
         * term and insert it into dpb (if there is room) */
        if(!markedAsLongTerm)
        {
            if(dpb->numRefFrames >= dpb->maxRefFrames && (secondField==0))
            {
				if(SlidingWindowRefPicMarking(dpb) == HANTRO_NOK)
				{
					DPBOUTDEBUG("h264bsdMarkDecRefPic no dpbSlot to FREE, error !!\n");
					return NULL;
				}
			}

            currentOut->frameNum = frameNum;
            currentOut->picNum = (i32) frameNum;
            SET_STATUS(currentOut, SHORT_TERM, picStruct);
            SET_POC(currentOut, picOrderCnt, picStruct);
            if(!secondField)
            {
                currentOut->toBeDisplayed = toBeDisplayed;
                dpb->fullness++;
                dpb->numRefFrames++;
            }
            /* first field non-reference and already output (kind of) */
            else if (currentOut->status[(u32)!picStruct] == UNUSED &&
                     currentOut->toBeDisplayed == 0)
            {
                dpb->fullness++;
                dpb->numRefFrames++;
            }

            /* no room */
            /*else
            {
                #if 1//SHOW_DPB
                    DPBDEBUG("no room for buffer numRefFrames %d maxRefFrames %d\n",
                    dpb->numRefFrames, dpb->maxRefFrames);
					ALOGE("no room for buffer numRefFrames %d maxRefFrames %d secondField %d\n", dpb->numRefFrames, dpb->maxRefFrames, secondField);
                #endif
                status = HANTRO_NOK;
            }*/
        }else
        {
            if(dpb->numRefFrames >= dpb->maxRefFrames )
            {
				if(SlidingWindowRefPicMarking(dpb) == HANTRO_NOK)
				{
					DPBOUTDEBUG("h264bsdMarkDecRefPic no dpbSlot to FREE, error !!\n");
					return HANTRO_NOK;
				}
			}
        }
    }

    return (status);
}

void h264DpbTryClearSlot(dpbStorage_t * dpb)
{
    u32 i;
    u32 clear = 0;

    for (i = 0; i <= dpb->dpbSize; i++) {
        if (dpb->buffer[i]) {
            dpbPicture_t *dpbPic = dpb->buffer[i];
            if (!IS_EXISTING(dpbPic, FRAME)) {
				dpbOutPicture_t *pOut;
				list_for_each_entry(pOut, &dpb->outList, list){
					if(pOut->data == dpbPic->data)	//find the outslot in outlist, must free it
					{
						freeOutSlot(pOut);
						break;
					}
				}
                freeDpbMemFromDpb(&dpbPic->data);
                freeDpbMemFromOut(&dpbPic->data);
				DPBDEBUG("freemem dpb->buffer[%d]=0x%x, poc=%d, %d", i, (u32)dpb->buffer[i], dpbPic->picOrderCnt[0], dpbPic->picOrderCnt[1]);
				DPBDEBUG("freemem poc=%d", GET_POC(dpbPic));
				if((dpbPic->status[0]==SHORT_TERM)||(dpbPic->status[0]==LONG_TERM)||(dpbPic->status[1]==SHORT_TERM)||(dpbPic->status[1]==LONG_TERM))
					dpb->numRefFrames--;
				freeDpbSlot(dpb, &dpb->buffer[i]);
                clear++;
				dpb->fullness--;
            } else if (IS_UNUSED(dpbPic, FRAME)) {
                freeDpbMemFromDpb(&dpbPic->data);
                freeDpbMemFromOut(&dpbPic->data);
				DPBDEBUG("unused freemem poc=%d", GET_POC(dpbPic));
                freeDpbSlot(dpb, &dpb->buffer[i]);
                clear++;
				dpb->fullness--;
            }
        }
    }
    if (!clear) {
        clear = SlidingWindowRefPicMarking(dpb);
        //ALOGE("SlidingWindowRefPicMarking ret %d", clear);
    }
}

/*------------------------------------------------------------------------------
    Function name   : h264DpbUpdateOutputList
    Description     :
    Return type     : void
    Argument        : dpbStorage_t * dpb
    Argument        : const image_t * image
------------------------------------------------------------------------------*/
void h264DpbUpdateOutputList(dpbStorage_t * dpb, u32 picStruct)
{
	int find = 0;

        if (picStruct != BOTFIELD) {
            dpbPicture_t *currentOut = dpb->currentOut;
            dpbPicture_t *dpbPic = NULL;
            int currenoutinlist=1;
            list_for_each_entry(dpbPic, &dpb->dpbList, list){
                if(dpbPic == dpb->currentOut)
                {
                    currenoutinlist = 0;
                    break;
                }
            }
            if(currenoutinlist)
                list_add_tail(&currentOut->list, &dpb->dpbList);
        }else{
            dpbPicture_t *dpbPic = NULL;
            dpbPicture_t *currentOut = NULL;
            list_for_each_entry(dpbPic, &dpb->dpbList, list){
                if(dpbPic == dpb->currentOut)
                {
                    find = 1;
                    break;
                }else{
                    continue;
                }
            }
            if(!find)
            {
                DPBDEBUG("currentOut not in list, add it\n", dpb->currentOut);
                currentOut = dpb->currentOut;
                list_add_tail(&currentOut->list, &dpb->dpbList);
        }
    }

    /* dpb was initialized not to reorder the pictures -> output current
     * picture immediately */
    /* output pictures if buffer full */

    do {
        if (HANTRO_NOK == OutputPicture(dpb, 0))
            break;
    } while (1);

    /* if currentOut is the last element of list -> exchange with first empty
     * slot so that only first 16 elements used as reference */

    if (NULL == dpb->currentOut)
        return ;

    if ((picStruct != BOTFIELD) || !find) {
        u32 i, loop = 0;
        u32 found = 0;
        do {
            found = 0;
            for (i = 0; i <= dpb->dpbSize; i++)
            {
            	if (dpb->buffer[i] == dpb->currentOut) {
					found = 1;
					break;
				}

                if (NULL == dpb->buffer[i]) {
                    dpb->buffer[i] = dpb->currentOut;
                    dpb->currentOut->dpbIdx = i;
                    found = 1;
                    break;
                }
            }

			//change with nonreference frame
			if((dpb->currentOut == dpb->buffer[dpb->dpbSize]) && found)
			{
				for(i = 0; i < dpb->dpbSize; i++)
				{
					if(!dpb->buffer[i]->toBeDisplayed &&
					   !IS_REFERENCE(dpb->buffer[i], 0) &&
					   !IS_REFERENCE(dpb->buffer[i], 1))
					{
						dpbPicture_t tmpPic = *dpb->currentOut;

						*dpb->currentOut = *dpb->buffer[i];
						*dpb->buffer[i] = tmpPic;
						dpb->currentOut = dpb->buffer[i];
						break;
					}
				}
			}

            if (!found) {
                DPBDEBUG("no dpbSlot for currentOut\n");
                //h264bsdDpbDump(dpb);
                //free(dpb);
                h264DpbTryClearSlot(dpb);
                loop++;
            }
        } while (!found && loop < 2);
    }
}

/*------------------------------------------------------------------------------

    Function: h264bsdGetRefPicDataVlcMode

        Functional description:
            Function to get reference picture data from the reference picture
            list

        Returns:
            pointer to desired reference picture data
            NULL if invalid index or non-existing picture referred

------------------------------------------------------------------------------*/

u8 *h264bsdGetRefPicDataVlcMode(const dpbStorage_t * dpb, u32 index,
                                u32 fieldMode)
{

/* Variables */

/* Code */

    if(!fieldMode)
    {
        if(index >= dpb->dpbSize)
            return (NULL);
        else if(!IS_EXISTING(dpb->buffer[index], FRAME))
            return (NULL);
        else
            return (u8 *) (dpb->buffer[index]->data->vir_addr);
    }
    else
    {
        const u32 field = (index & 1) ? BOTFIELD : TOPFIELD;
        if(index / 2 >= dpb->dpbSize)
            return (NULL);
        else if(!IS_EXISTING(dpb->buffer[index / 2], field))
            return (NULL);
        else
            return (u8 *) (dpb->buffer[index / 2]->data->vir_addr);
    }

}

void h264bsdDpbDump(dpbStorage_t * dpb)
{
    dpbPicture_t *tmp;
    dpbPicture_t *ret = NULL;
    list_for_each_entry(tmp, &dpb->dpbList, list) {
        ALOGE("normal list: dpbIdx %d picNum %d frameNum %d poc %d toBeDisplayed %d IS_REFERENCE_F %d\n",
            tmp->dpbIdx, tmp->picNum, tmp->frameNum, GET_POC(tmp), tmp->toBeDisplayed, IS_REFERENCE_F(tmp));
    }
    if (dpb->currentOut) {
        tmp = dpb->currentOut;
        ALOGE("currentOut : dpbIdx %d picNum %d frameNum %d poc %d toBeDisplayed %d IS_REFERENCE_F %d\n",
            tmp->dpbIdx, tmp->picNum, tmp->frameNum, GET_POC(tmp), tmp->toBeDisplayed, IS_REFERENCE_F(tmp));
    }
    if (dpb->previousOut) {
        tmp = dpb->previousOut;
        ALOGE("previousOut: dpbIdx %d picNum %d frameNum %d poc %d toBeDisplayed %d IS_REFERENCE_F %d\n",
            tmp->dpbIdx, tmp->picNum, tmp->frameNum, GET_POC(tmp), tmp->toBeDisplayed, IS_REFERENCE_F(tmp));
    }
    ALOGE("lastPicOrderCnt %d\n", dpb->lastPicOrderCnt);

    return ;
}

/*------------------------------------------------------------------------------

    Function: h264bsdAllocateDpbImage

        Functional description:
            function to allocate memory for a image. This function does not
            really allocate any memory but reserves one of the buffer
            positions for decoding of current picture

        Returns:
            pointer to memory area for the image

------------------------------------------------------------------------------*/

void *h264bsdAllocateDpbImage(storage_t * pStorage, dpbStorage_t * dpb)
{
    dpbMem_t *pMem = NULL;
    i32 failed = 0;
	seqParamSet_t *pSps = pStorage->activeSps;
	u32 picwidth = pSps->picWidthInMbs*16;
	u32 picheight = pSps->picHeightInMbs*16;

    do {
        pMem = newDpbMem(dpb);
        if (NULL == pMem) {
            failed++;
            usleep(20000);
        }
    } while (pMem == NULL && failed <= 20);

    if (NULL == pMem) {
        ALOGE("h264bsdAllocateDpbImage failed");
        return NULL;
    }

    if (dpb->currentOut) {
        ALOGE("h264bsdAllocateDpbImage found currentOut is not NULL\n");
    }

    dpbPicture_t *currentOut = newDpbSlot(dpb);
    if (currentOut == NULL) {
        return NULL;
    }

	/*if(dpb->ts_en)//写标记,用来判定出错位置
	{
		int i;
		u32 *p = pMem->mem.vir_addr;
		//LOGE("dpb->picWidth=%d, %d", picwidth, picheight);
		for(i=0;i<picheight;i++)
			p[i*picwidth/4] = 0x66339988;
		VPUMemClean(&pMem->mem);
	}*/

    currentOut->status[0] = currentOut->status[1] = EMPTY;
    currentOut->data = pMem;
    currentOut->dpbIdx = dpb->dpbSize + 1;
    currentOut->h264hwStatus[0] = 0;
    currentOut->h264hwStatus[1] = 0;
    dpb->currentOut = dpb->previousOut = currentOut;
    pMem->memStatus |= MEM_DPB_USE;
    dpb->currentPhyAddr = pMem->phy_addr;
    dpb->currentVirAddr = pMem->vir_addr;

    return pMem;
}

/*------------------------------------------------------------------------------

    Function: SlidingWindowRefPicMarking

        Functional description:
            Function to perform sliding window refence picture marking process.

        Outputs:
            HANTRO_OK      success
            HANTRO_NOK     failure, no short-term reference frame found that
                           could be marked unused

------------------------------------------------------------------------------*/

static u32 SlidingWindowRefPicMarking(dpbStorage_t * dpb)
{
	DPBDEBUG("SlidingWindowRefPicMarking");
    if (dpb->numRefFrames < dpb->maxRefFrames)
    {
    	DPBDEBUG("=%d, %d\n", dpb->numRefFrames, dpb->maxRefFrames);
        return (HANTRO_OK);
    }
    else
    {
        i32 picNum = 0;
        dpbPicture_t *dpbPic = NULL, *tmp = NULL;
		DPBDEBUG("\n");
        list_for_each_entry(tmp, &dpb->dpbList, list) {
            if (tmp != dpb->currentOut && IS_SHORT_TERM_F(tmp)) {
				DPBDEBUG("tmp->poc=%d, picnum=%d, 0x%x\n", GET_POC(tmp), tmp->picNum, tmp);
                if (tmp->picNum < picNum || dpbPic == NULL)
                {
                    dpbPic = tmp;
                    picNum = tmp->picNum;
                }
            }
        }
        if (dpbPic) {
            SET_STATUS(dpbPic, UNUSED, FRAME);
            DpbBufFree(dpb, dpbPic);

            return (HANTRO_OK);
        }
#if 0
        u32 i;
        i32 index = -1, picNum = 0;
        /* find the oldest short term picture */
        for(i = 0; i < dpb->dpbSize; i++) {
            if(dpb->buffer[i] && IS_SHORT_TERM_F(dpb->buffer[i]))
            {
                if(dpb->buffer[i]->picNum < picNum || index == -1)
                {
                    index = (i32) i;
                    picNum = dpb->buffer[i]->picNum;
                }
            }
        }
        if(index >= 0)
        {
            SET_STATUS(dpb->buffer[index], UNUSED, FRAME);
            DpbBufFree(dpb, dpb->buffer[index]);

            return (HANTRO_OK);
        }
#endif
    }

    return (HANTRO_NOK);

}

/*------------------------------------------------------------------------------

    Function: h264bsdInitDpb

        Functional description:
            Function to initialize DPB. Reserves memories for the buffer,
            reference picture list and output buffer. dpbSize indicates
            the maximum DPB size indicated by the levelIdc in the stream.
            If noReordering flag is HANTRO_FALSE the DPB stores dpbSize pictures
            for display reordering purposes. On the other hand, if the
            flag is HANTRO_TRUE the DPB only stores maxRefFrames reference pictures
            and outputs all the pictures immediately.

        Inputs:
            picSizeInMbs    picture size in macroblocks
            dpbSize         size of the DPB (number of pictures)
            maxRefFrames    max number of reference frames
            maxFrameNum     max frame number
            noReordering    flag to indicate that DPB does not have to
                            prepare to reorder frames for display

        Outputs:
            dpb             pointer to dpb data storage

        Returns:
            HANTRO_OK       success
            MEMORY_ALLOCATION_ERROR if memory allocation failed

------------------------------------------------------------------------------*/

u32 h264bsdInitDpb(dpbStorage_t * dpb,
                   u32 picSizeInMbs,
                   u32 dpbSize,
                   u32 maxRefFrames, u32 maxFrameNum,
                   u32 monoChrome, u32 isHighSupported, u32 ts_en)
{

/* Variables */

    u32 i;
    u32 pic_buff_size;

/* Code */

    ASSERT(picSizeInMbs);
    ASSERT(maxRefFrames <= MAX_NUM_REF_PICS);
    ASSERT(maxRefFrames <= dpbSize);
    ASSERT(maxFrameNum);
    ASSERT(dpbSize);

    /* we trust our memset; ignore return value */
    memset(dpb, 0, sizeof(*dpb)); /* make sure all is clean */
    INIT_LIST_HEAD(&dpb->memList);
    INIT_LIST_HEAD(&dpb->dpbList);
    INIT_LIST_HEAD(&dpb->dpbFree);
    INIT_LIST_HEAD(&dpb->outList);
    dpb->picSizeInMbs = picSizeInMbs;
    dpb->maxLongTermFrameIdx = NO_LONG_TERM_FRAME_INDICES;
    dpb->maxRefFrames = MAX(maxRefFrames, 1);
    dpb->dpbSize = dpbSize;
    dpb->maxFrameNum = maxFrameNum;
    dpb->fullness = 0;
    dpb->numRefFrames = 0;
    dpb->prevRefFrameNum = 0;
    dpb->numOut = 0;
    dpb->lastPicOrderCnt = 0x7ffffff0;
	dpb->poc_interval = 2;
	dpb->ts_en = ts_en;

    if(isHighSupported)
    {
        /* yuv picture + direct mode motion vectors */
        pic_buff_size = picSizeInMbs * ((monoChrome ? 256 : 384) + 64);
        dpb->dirMvOffset = picSizeInMbs * (monoChrome ? 256 : 384);
    }
    else
    {
        pic_buff_size = picSizeInMbs * 384;
    }

    dpb->picbuffsie = pic_buff_size;
/*
    dpb->totBuffers = dpb->dpbSize + 1;
    if (displaySmoothing) {
        if (isFlashPlayer) {
            dpb->totBuffers += dpb->dpbSize + 1;
        } else {
            dpb->totBuffers += MIN(4, dpb->dpbSize + 1);

            i = (63*1024*1024)/pic_buff_size;

            if (dpb->totBuffers > i)
                dpb->totBuffers = i;
        }
    }
    #if SHOW_DPB
        ALOGE("h264bsdInitDpb: isFlashPlayer %d dpbSize %d totBuffers %d \n", isFlashPlayer, dpbSize, dpb->totBuffers);
    #endif
    */

    INIT_LIST_HEAD(&dpb->memList);
    INIT_LIST_HEAD(&dpb->dpbList);
    INIT_LIST_HEAD(&dpb->dpbFree);
    INIT_LIST_HEAD(&dpb->outList);
/*
    for(i = 0; i < dpb->totBuffers; i++)
    {
        dpb->buffer[i] = NULL;
        dpb->numFreeBuffers++;
    }
*/
	DPBDEBUG("DPBSIZE IS %d", dpbSize);
    for (i = 0; i <= dpbSize; i++) {
        list_add_tail(&dpb->dpbSlot[i].list, &dpb->dpbFree);
    }

    return (HANTRO_OK);
}

/*------------------------------------------------------------------------------

    Function: h264bsdResetDpb

        Functional description:
            Function to reset DPB. This function should be called when an IDR
            slice (other than the first) activates new sequence parameter set.
            Function calls h264bsdFreeDpb to free old allocated memories and
            h264bsdInitDpb to re-initialize the DPB. Same inputs, outputs and
            returns as for h264bsdInitDpb.

------------------------------------------------------------------------------*/

u32 h264bsdResetDpb(dpbStorage_t * dpb,
                    u32 picSizeInMbs,
                    u32 dpbSize,
                    u32 maxRefFrames, u32 maxFrameNum,
                    u32 monoChrome, u32 isHighSupported, u32 ts_en)
{

/* Code */

    ASSERT(picSizeInMbs);
    ASSERT(maxRefFrames <= MAX_NUM_REF_PICS);
    ASSERT(maxRefFrames <= dpbSize);
    ASSERT(maxFrameNum);
    ASSERT(dpbSize);

    if(dpb->picSizeInMbs == picSizeInMbs)
    {
        dpb->maxLongTermFrameIdx = NO_LONG_TERM_FRAME_INDICES;
        dpb->maxRefFrames = (maxRefFrames != 0) ? maxRefFrames : 1;
        dpb->maxFrameNum = maxFrameNum;
        dpb->flushed = 0;

        if(dpb->dpbSize == dpbSize)
        {
            /* number of pictures and DPB size are not changing */
            /* no need to reallocate DPB */
            return (HANTRO_OK);
        }
    }

    h264bsdFreeDpb(dpb);

    return h264bsdInitDpb(dpb, picSizeInMbs, dpbSize, maxRefFrames,
                          maxFrameNum, monoChrome, isHighSupported, ts_en);
}

/*------------------------------------------------------------------------------

    Function: SetPicNums

        Functional description:
            Function to set picNum values for short-term pictures in the
            buffer. Numbering of pictures is based on frame numbers and as
            frame numbers are modulo maxFrameNum -> frame numbers of older
            pictures in the buffer may be bigger than the currFrameNum.
            picNums will be set so that current frame has the largest picNum
            and all the short-term frames in the buffer will get smaller picNum
            representing their "distance" from the current frame. This
            function kind of maps the modulo arithmetic back to normal.

------------------------------------------------------------------------------*/

void SetPicNums(dpbStorage_t * dpb, u32 currFrameNum)
{

/* Variables */

    //u32 i;
    i32 frameNumWrap;
    dpbPicture_t *dpbPic;

/* Code */

    ASSERT(dpb);
    ASSERT(currFrameNum < dpb->maxFrameNum);

    list_for_each_entry(dpbPic, &dpb->dpbList, list) {
        if (IS_SHORT_TERM(dpbPic, FRAME))    //buffer is short term?
        {
            if (dpbPic->frameNum > currFrameNum)
                frameNumWrap = (i32) dpbPic->frameNum - (i32) dpb->maxFrameNum;
            else
                frameNumWrap = (i32) dpbPic->frameNum;
            dpbPic->picNum = frameNumWrap;
        }
    }
/*
    for(i = 0; i <= dpb->dpbSize; i++) {
        if(dpb->buffer[i] && IS_SHORT_TERM(dpb->buffer[i], FRAME))    //buffer is short term?
        {
            if(dpb->buffer[i]->frameNum > currFrameNum)
                frameNumWrap =
                    (i32) dpb->buffer[i]->frameNum - (i32) dpb->maxFrameNum;
            else
                frameNumWrap = (i32) dpb->buffer[i]->frameNum;
            dpb->buffer[i]->picNum = frameNumWrap;
        }
    }
*/
}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckGapsInFrameNum

        Functional description:
            Function to check gaps in frame_num and generate non-existing
            (short term) reference pictures if necessary. This function should
            be called only for non-IDR pictures.

        Inputs:
            dpb         pointer to dpb data structure
            frameNum    frame number of the current picture
            isRefPic    flag to indicate if current picture is a reference or
                        non-reference picture

        Outputs:
            dpb         'buffer' possibly modified by inserting non-existing
                        pictures with sliding window marking process

        Returns:
            HANTRO_OK   success
            HANTRO_NOK  error in sliding window reference picture marking or
                        frameNum equal to previous reference frame used for
                        a reference picture

------------------------------------------------------------------------------*/
u32 h264bsdCheckGapsInFrameNum(storage_t * pStorage, dpbStorage_t * dpb, u32 frameNum, u32 isRefPic,
                               u32 gapsAllowed)
{

/* Variables */

    u32 unUsedShortTermFrameNum;

/* Code */

    ASSERT(dpb);
    ASSERT(dpb->fullness <= dpb->dpbSize);
    ASSERT(frameNum < dpb->maxFrameNum);

    if(!gapsAllowed)
    {
        return HANTRO_OK;
    }

    ALOGE("h264bsdCheckGapsInFrameNum");

    if((frameNum != dpb->prevRefFrameNum) &&
       (frameNum != ((dpb->prevRefFrameNum + 1) % dpb->maxFrameNum)))
    {
        unUsedShortTermFrameNum = (dpb->prevRefFrameNum + 1) % dpb->maxFrameNum;

        do
        {
            /* store data pointer of last buffer position to be used as next
             * "allocated" data pointer. if last buffer position after this process
             * contains data pointer located in outBuf (buffer placed in the output
             * shall not be overwritten by the current picture) */
            (void)h264bsdAllocateDpbImage(pStorage, dpb);

            SetPicNums(dpb, unUsedShortTermFrameNum);

            if (SlidingWindowRefPicMarking(dpb) != HANTRO_OK)
            {
                return (HANTRO_NOK);
            }

            SET_STATUS(dpb->currentOut, NON_EXISTING, FRAME);
            dpb->currentOut->frameNum = unUsedShortTermFrameNum;
            dpb->currentOut->picNum = (i32) unUsedShortTermFrameNum;
            dpb->currentOut->picOrderCnt[0] = 0;
            dpb->currentOut->picOrderCnt[1] = 0;
            dpb->currentOut->toBeDisplayed = HANTRO_FALSE;
            dpb->fullness++;
            dpb->numRefFrames++;

            /* output pictures if buffer full */
            h264DpbUpdateOutputList(dpb, 0);

            unUsedShortTermFrameNum = (unUsedShortTermFrameNum + 1) %
                dpb->maxFrameNum;

        }
        while(unUsedShortTermFrameNum != frameNum);

        //(void)h264bsdAllocateDpbImage(dpb);
        /* pictures placed in output buffer -> check that 'data' in
         * buffer position dpbSize is not in the output buffer (this will be
         * "allocated" by h264bsdAllocateDpbImage). If it is -> exchange data
         * pointer with the one stored in the beginning */
    }
    /* frameNum for reference pictures shall not be the same as for previous
     * reference picture, otherwise accesses to pictures in the buffer cannot
     * be solved unambiguously */
    else if(isRefPic && frameNum == dpb->prevRefFrameNum)
    {
        return (HANTRO_NOK);
    }

    /* save current frame_num in prevRefFrameNum. For non-reference frame
     * prevFrameNum is set to frame number of last non-existing frame above */
    if(isRefPic)
        dpb->prevRefFrameNum = frameNum;
    else if(frameNum != dpb->prevRefFrameNum)
    {
        dpb->prevRefFrameNum =
            (frameNum + dpb->maxFrameNum - 1) % dpb->maxFrameNum;
    }

    return (HANTRO_OK);
}

/*------------------------------------------------------------------------------

    Function: FindSmallestPicOrderCnt

        Functional description:
            Function to find picture with smallest picture order count. This
            will be the next picture in display order.

        Returns:
            pointer to the picture, NULL if no pictures to be displayed

------------------------------------------------------------------------------*/
#if SHOW_SMALL_POC
#define SMALL_POC_LOG       ALOGE
#else
#define SMALL_POC_LOG
#endif

dpbPicture_t *FindSmallestPicOrderCnt(dpbStorage_t * dpb, u32 flush)
{

/* Variables */

    u32 i = 0;
    i32 picOrderCnt;
    dpbPicture_t *dpbPic = NULL, *tmp = NULL;
    u32 isfull = 0;

/* Code */

    ASSERT(dpb);

	if(!dpb->ts_en)
	{
		if((dpb->currentOut)&&(dpb->currentOut->isFieldPic))	//场模式下才做这个判断
			isfull = (dpb->fullness > dpb->dpbSize) && (dpb->currentOut->isFieldPic == 3);
		else
			isfull = (dpb->fullness > dpb->dpbSize);
	} else
		isfull = (dpb->fullness > dpb->dpbSize);

    picOrderCnt = 0x7FFFFFFF;

#if SHOW_SMALL_POC
    ALOGE("OutputPicture fullness %d, dpbSize %d flush %d, numRefFrames %d\n", dpb->fullness, dpb->dpbSize, flush, dpb->numRefFrames);
    h264bsdDpbDump(dpb);
#endif

	if(dpb->numRefFrames > dpb->fullness)
	{
		DPBDEBUG("fullness not equal dpb->numRefFrames\n");
	}

    list_for_each_entry(dpbPic, &dpb->dpbList, list) {
		i32 poc = GET_POC(dpbPic);
        if (!dpbPic->toBeDisplayed){
			SMALL_POC_LOG("POC=%d, toBeDisplayed phy=0x%x continue\n", poc, dpbPic->data->mem.phy_addr);
            continue;
		} else {
			SMALL_POC_LOG("POC=%d,  phy=0x%x\n", poc, dpbPic->data->mem.phy_addr);
		}

        if (poc >= picOrderCnt) {
            SMALL_POC_LOG("continue poc >= picOrderCnt", poc, picOrderCnt);
            continue;
        }

        if (!flush && !isfull)
        {
            SMALL_POC_LOG("!flush && !isfull");
            if (0x7FFFFFF0 == dpb->lastPicOrderCnt) {
                SMALL_POC_LOG("0x7FFFFFF0 == dpb->lastPicOrderCnt");
                if (dpb->minusPoc) {
                    SMALL_POC_LOG("continue dpb->minusPoc");
                    continue;
                }

                if (dpb->currentOut) {
                    if (dpb->currentOut->isFieldPic == 1 || dpb->currentOut->isFieldPic == 2)
                        continue;
                }
                if (dpb->fullness == 1) {
                    SMALL_POC_LOG("continue for dpb->fullness == 1");
                    continue;
                }
            }

			if ((dpb->fullness <= dpb->dpbSize) && dpb->ts_en == 1) {//??????????????dbp??????
                SMALL_POC_LOG("continue (dpb->fullness %d <= dpb->dpbSize %d) && dpb->ts_en %d",
                    dpb->fullness, dpb->dpbSize, dpb->ts_en);
                continue;
			}

            if ((poc > (i32)(dpb->lastPicOrderCnt + dpb->poc_interval))) {
                SMALL_POC_LOG("continue (poc %d > dpb->lastPicOrderCnt %d + dpb->poc_interval %d)",
                    poc, dpb->lastPicOrderCnt, dpb->poc_interval);
                continue;
            }

			if ((dpb->lastPicOrderCnt == 0)&&(dpbPic->frameNum == 1)) {
                SMALL_POC_LOG("continue (dpb->lastPicOrderCnt == 0)&&(dpbPic->frameNum == 1)");
                //tmp = dpbPic;
                //picOrderCnt = dpbPic->picOrderCnt[0];
				//break;
                continue;
			}
        }

        if (!flush) {
            SMALL_POC_LOG("!flush");
            // not output incompelte field
            if (dpb->currentOut)
        	{
				if ((dpbPic == dpb->currentOut)&&(dpbPic->isFieldPic != 0 && dpbPic->isFieldPic != 0x3))
				{
					if(isfull)
						picOrderCnt = poc;	//dpb full的时候,当前只有一个场也要判断,不然poc输出会出错
					if(tmp&&isfull)		//dpb full时发现输出帧的poc比当前帧的大,说明出错,赶紧不输出
						if(GET_POC(tmp) > poc)
							tmp = NULL;
					continue;
				}
        	}
			else
			{
	            if (dpbPic->isFieldPic != 0 && dpbPic->isFieldPic != 0x3)
	                continue;
			}
        }

        if (dpbPic->picOrderCnt[1] >= picOrderCnt) {
            DEBUG_PRINT(("HEP %d %d\n", dpbPic->picOrderCnt[1],
                   picOrderCnt));
        }
        tmp = dpbPic;
        picOrderCnt = dpbPic->picOrderCnt[0];
    }

	if((tmp) && (dpb->lastPicOrderCnt == 0) && (picOrderCnt > 0 ))
	{
		if(picOrderCnt > 2)
			dpb->poc_interval = 2;
		else
			dpb->poc_interval = picOrderCnt - dpb->lastPicOrderCnt;
	}

	if(tmp)
	{
		DPBDEBUG("poc compare end %d!\n", GET_POC(tmp));
	}
#if 0
    for(i = 0; i <= dpb->dpbSize; i++)
    {
        /* TODO: currently only outputting frames, asssumes that fields of a
         * frame are output consecutively */
        /*
         * MIN(dpb->buffer[i].picOrderCnt[0],dpb->buffer[i].picOrderCnt[1]) <
         * picOrderCnt &&
         * dpb->buffer[i].status[0] != EMPTY &&
         * dpb->buffer[i].status[1] != EMPTY)
         */
        //ALOGE("%2d disp %d poc %d last %d\n", i, dpb->buffer[i].toBeDisplayed, poc, dpb->lastPicOrderCnt);
        if (!dpb->buffer[i])
            continue;

        if (!dpb->buffer[i]->toBeDisplayed)
            continue;

        i32 poc = GET_POC(dpb->buffer[i]);
        if (poc >= picOrderCnt)
            continue;

        if (!flush && !isfull)
        {
            if (0x7FFFFFF0 == dpb->lastPicOrderCnt && 0 != poc)
                continue;

            if (poc > dpb->lastPicOrderCnt + 2)
                continue;
        }

        if(dpb->buffer[i]->picOrderCnt[1] >= picOrderCnt)
        {
            DEBUG_PRINT(("HEP %d %d\n", dpb->buffer[i]->picOrderCnt[1],
                   picOrderCnt));
        }
        tmp = dpb->buffer[i];
        picOrderCnt = dpb->buffer[i]->picOrderCnt[0];
    }

#if SHOW_DPB
    static u32 last_no_out = 0;
    if (NULL == tmp)
    {
        ALOGE("OutputPicture ret NULL %d\n", last_no_out);
        if (last_no_out)
        {
            for(i = 0; i <= dpb->dpbSize; i++)
            {
                ALOGE("%2d disp %d ref %d poc %d last %d\n",
                    i,
                    dpb->buffer[i]->toBeDisplayed,
                    IS_REFERENCE_F(dpb->buffer[i]),
                    GET_POC(dpb->buffer[i]),
                    dpb->lastPicOrderCnt);
            }
        }
        last_no_out++;
    }
    else
    {
        last_no_out = 0;
        ALOGE("OutputPicture get poc %d last %d\n", GET_POC(*tmp), dpb->lastPicOrderCnt);
    }
#endif

#endif

    return (tmp);
}

/*------------------------------------------------------------------------------

    Function: OutputPicture

        Functional description:
            Function to put next display order picture into the output buffer.

        Returns:
            HANTRO_OK      success
            HANTRO_NOK     no pictures to display

------------------------------------------------------------------------------*/

u32 OutputPicture(dpbStorage_t * dpb, u32 flush)
{

/* Variables */

    dpbPicture_t *tmp;
    dpbOutPicture_t *dpbOut;

/* Code */

    ASSERT(dpb);

    tmp = FindSmallestPicOrderCnt(dpb, flush);

    /* no pictures to be displayed */
    if (tmp == NULL)
    {
        return (HANTRO_NOK);
    }

    /* remove it from DPB */
    tmp->toBeDisplayed = HANTRO_FALSE;
/*
    if (!dpb->isFlashPlayer) {
        if ((0x7ffffff0 != dpb->lastPicOrderCnt) &&
            (GET_POC(tmp) < dpb->lastPicOrderCnt))
        {
            if(!IS_REFERENCE_F(tmp))
            {
                dpb->fullness--;
            }
            return HANTRO_NOK;
        }
    }
*/

    /* updated output list */
    //dpbOut = &dpb->outBuf[dpb->outIndexW]; /* next output */
    dpbOut = newOutSlot(dpb);
    if (NULL == dpbOut)
        return HANTRO_NOK;
    dpbOut->data = tmp->data;
    dpbOut->isIdr = tmp->isIdr;
    dpbOut->picId = tmp->picId;
    dpbOut->numErrMbs = tmp->numErrMbs;
    dpbOut->interlaced = dpb->interlaced;
    dpbOut->fieldPicture = 0;
    dpbOut->TimeLow = tmp->TimeLow;
    dpbOut->TimeHigh = tmp->TimeHigh;
    dpbOut->isFieldPic = tmp->isFieldPic;
    dpbOut->h264hwStatus[0] = tmp->h264hwStatus[0];
    dpbOut->h264hwStatus[1] = tmp->h264hwStatus[1];
    dpb->lastPicOrderCnt = GET_POC(tmp);

    dpbOut->data->memStatus |= MEM_OUT_USE;

    //dpb->memStat[tmp->memIdx] |= MEM_STAT_OUT;

    if (tmp->isFieldPic)
    {
        if(tmp->status[0] == EMPTY || tmp->status[1] == EMPTY)
        {
            dpbOut->fieldPicture = 1;
            dpbOut->topField = (tmp->status[0] == EMPTY) ? 0 : 1;

            DEBUG_PRINT(("dec pic %d MISSING FIELD! %s\n", dpbOut->picId,
                   dpbOut->topField ? "BOTTOM" : "TOP"));
        }
    }

	if ((tmp->isFieldPic==1) || (tmp->isFieldPic==2))		//只有一个场就直接输出
	{
		ALOGE("find olny one field output! poc0=%d, poc1=%d", tmp->picOrderCnt[0], tmp->picOrderCnt[1]);
	    	if((tmp->status[0] == EMPTY) && IS_EXISTING(tmp,1))
		{
			SET_STATUS(tmp, UNUSED,  1);
		}

	    	if((tmp->status[1] == EMPTY) && IS_EXISTING(tmp,0))
		{
			SET_STATUS(tmp, UNUSED,  0);
		}
	}

    dpb->numOut++;

    if (IS_UNUSED(tmp, TOPFIELD) || IS_UNUSED(tmp, BOTFIELD))
    {
        freeDpbMemFromDpb(&tmp->data);
        if (dpb->buffer[tmp->dpbIdx]) {
            freeDpbSlot(dpb, &dpb->buffer[tmp->dpbIdx]);
        } else {
            freeDpbSlot(dpb, &dpb->currentOut);
        }
        if (tmp == dpb->currentOut)
            dpb->currentOut = NULL;
    }

    if (!IS_REFERENCE_F(tmp))
    {
        dpb->fullness--;
    }

    return (HANTRO_OK);
}

/*------------------------------------------------------------------------------

    Function: h264bsdDpbOutputPicture

        Functional description:
            Function to get next display order picture from the output buffer.

        Return:
            pointer to output picture structure, NULL if no pictures to
            display

------------------------------------------------------------------------------*/

u32 h264bsdDpbOutputPicture(dpbStorage_t * dpb, void * ptr)
{

/* Variables */

    u32 tmpIdx;
    H264DecPicture * pOutput = (H264DecPicture *)ptr;

/* Code */

#if SHOW_DPB
    ALOGE("h264bsdDpbOutputPicture numOut %d\n", dpb->numOut);
#endif
    if (dpb->numOut && !list_empty(&dpb->outList)) {
        dpbOutPicture_t *pOut = list_entry(dpb->outList.next, dpbOutPicture_t, list);
        dpb->numOut--;

		/*if(((pOut->h264hwStatus[0]&0x10000) || (pOut->h264hwStatus[1]&0x10000)) && (pOut->isFieldPic))
		{
			OutBufFree(dpb, pOut);

			freeOutSlot(pOut);

			return 0;
		}else*/
		{
	        pOutput->TimeLow  = pOut->TimeLow;
	        pOutput->TimeHigh = pOut->TimeHigh;

			VPUMemDuplicate(&pOutput->lineardata, &pOut->data->mem);
	        pOutput->pOutputPicture = (u8*)pOut->data->vir_addr;
	        pOutput->outputPictureBusAddress = pOut->data->phy_addr;

	        OutBufFree(dpb, pOut);

	        pOutput->picId = pOut->picId;
	        pOutput->isIdrPicture = pOut->isIdr;
	        pOutput->nbrOfErrMBs = pOut->numErrMbs;

	        pOutput->interlaced = pOut->interlaced;
	        //pOutput->fieldPicture = pOut->fieldPicture;
	        //pOutput->topField = pOut->topField;

			freeOutSlot(pOut);

			return 1;
		}
    }
    return 0;
/*
    if(dpb->numOut && !dpb->noOutput)
    {
        tmpIdx = dpb->outIndexR++;
        dpbOutPicture_t *pOut = dpb->outBuf + tmpIdx;
        if (dpb->outIndexR == dpb->dpbSize + 1)
            dpb->outIndexR = 0;
        dpb->numOut--;
        dpb->prevOutIdx = pOut->memIdx;
        //ALOGE("h264bsdDpbOutputPicture time high %d low %d\n", pOut->TimeHigh, pOut->TimeLow);
        OutBufFree(dpb, tmpIdx);
        return (pOut);
    }
    else
        return (NULL);
*/
}

/*------------------------------------------------------------------------------

    Function: h264bsdFlushDpb

        Functional description:
            Function to flush the DPB. Function puts all pictures needed for
            display into the output buffer. This function shall be called in
            the end of the stream to obtain pictures buffered for display
            re-ordering purposes.

------------------------------------------------------------------------------*/
void h264bsdFlushDpb(dpbStorage_t * dpb)
{
    dpb->flushed = 1;

    /* output all pictures */
#if SHOW_DPB
    ALOGE("h264bsdFlushDpb\n");
#endif

    if (dpb->dpbList.next && dpb->dpbList.prev) {
        dpbPicture_t *dpbPic, *tmp;
        list_for_each_entry_safe(dpbPic, tmp, &dpb->dpbList, list) {
            if (dpbPic && IS_REFERENCE_F(dpbPic)) {
                SET_STATUS(dpbPic, UNUSED, FRAME);
                DpbBufFree(dpb, dpbPic);
            }
        }
        while(OutputPicture(dpb, 1) == HANTRO_OK) ;
    }
    dpb->lastPicOrderCnt = 0x7ffffff0;
}

/*------------------------------------------------------------------------------

    Function: h264bsdFreeDpb

        Functional description:
            Function to free memories reserved for the DPB.

------------------------------------------------------------------------------*/

void h264bsdFreeDpb(dpbStorage_t * dpb)
{
    ASSERT(dpb);

    dpbOutPicture_t *pOut, *nOut;
    dpbPicture_t    *pSlt, *nSlt;

    if (dpb->outList.next && dpb->outList.prev) {
        list_for_each_entry_safe(pOut, nOut, &dpb->outList, list) {
            if (pOut->data)
                freeDpbMemFromOut(&pOut->data);
            freeOutSlot(pOut);
        }
    }

    if (dpb->dpbList.next && dpb->dpbList.prev) {
        list_for_each_entry_safe(pSlt, nSlt, &dpb->dpbList, list) {
            if (pSlt->data && DPBMEM_IS_AVAIL(pSlt->data))
                freeDpbMem(&pSlt->data);
            freeDpbSlot(dpb, &dpb->buffer[pSlt->dpbIdx]);
        }
    }
}

/*------------------------------------------------------------------------------

    Function: ShellSort

        Functional description:
            Sort pictures in the buffer. Function implements Shell's method,
            i.e. diminishing increment sort. See e.g. "Numerical Recipes in C"
            for more information.

------------------------------------------------------------------------------*/

void ShellSort(dpbStorage_t * dpb, u32 * list, u32 type, i32 par)
{
    u32 i, j;
    u32 step;
    u32 tmp;
    dpbPicture_t **pPic = dpb->buffer;
    u32 num = dpb->dpbSize + 1;

    step = 7;

    while(step)
    {
        for(i = step; i < num; i++)
        {
            tmp = list[i];
            j = i;
            while(j >= step &&
                  (type ?
                   ComparePicturesB(pPic[list[j - step]], pPic[tmp], par) :
                   ComparePictures(pPic[list[j - step]], pPic[tmp])) > 0)
            {
                list[j] = list[j - step];
                j -= step;
            }
            list[j] = tmp;
        }
        step >>= 1;
    }
}

/*------------------------------------------------------------------------------

    Function: ShellSortF

        Functional description:
            Sort pictures in the buffer. Function implements Shell's method,
            i.e. diminishing increment sort. See e.g. "Numerical Recipes in C"
            for more information.

------------------------------------------------------------------------------*/

void ShellSortF(dpbStorage_t * dpb, u32 * list, u32 type, i32 par)
{
    u32 i, j;
    u32 step;
    u32 tmp;
    dpbPicture_t **pPic = dpb->buffer;
    u32 num = dpb->dpbSize + 1;

    step = 7;

    while(step)
    {
        for(i = step; i < num; i++)
        {
            tmp = list[i];
            j = i;
            while(j >= step &&
                  (type ?
                   CompareFieldsB(pPic[list[j - step]], pPic[tmp], par) :
                   CompareFields(pPic[list[j - step]], pPic[tmp])) > 0)
            {
                list[j] = list[j - step];
                j -= step;
            }
            list[j] = tmp;
        }
        step >>= 1;
    }
}

/* picture marked as unused and not to be displayed -> buffer is free for next
 * decoded pictures */
u32 DpbBufFree(dpbStorage_t *dpb, dpbPicture_t *p)
{
    dpb->numRefFrames--;

    #if SHOW_DPB
        ALOGE("DpbBufFree poc %d\n", GET_POC(p));
    #endif

    if (!p->toBeDisplayed)
    {
    	DPBDEBUG("DpbBufFree poc=%d\n", GET_POC(p));
        freeDpbMemFromDpb(&p->data);
        if (dpb->buffer[p->dpbIdx])
            freeDpbSlot(dpb, &dpb->buffer[p->dpbIdx]);
        dpb->fullness--;
    }


    return 0;
}

/* picture removed from output list -> add to free buffers if not used for
 * reference anymore */
u32 OutBufFree(dpbStorage_t *dpb, dpbOutPicture_t *p)
{
    if (0 == freeDpbMemFromOut(&p->data)) {
        //dpb->memStat[p->memIdx] &= ~MEM_STAT_OUT;
    }

    return 0;
}

void h264decErrorConCealMent(decContainer_t * pDecCont)
{
    storage_t *pStorage = &pDecCont->storage;
    dpbStorage_t *dpb = pStorage->dpb;
    sliceHeader_t *sliceHeader = pStorage->sliceHeader;
    dpbPicture_t *currentOut = dpb->currentOut;
	seqParamSet_t *pSps = pDecCont->storage.activeSps;
    u32 picStruct;
	i32 threshold = 200;

    ASSERT((pStorage));

	if(!pDecCont->ts_en)
		return ;

	if (NULL == dpb->currentOut)
        return ;

	//针对backup.ts那个源
	if((pStorage->sliceHeader->sliceType == (I_SLICE+5)) || (pStorage->sliceHeader->sliceType == I_SLICE))
	{
		u32 i, slicecnt = 0;
		dpbStorage_t *dpb = pStorage->dpb;
		dpbPicture_t **buffer = dpb->buffer;
		dpbPicture_t *tmp = NULL;

		for (i = 0; i < dpb->dpbSize; i++)
		{
			if (buffer[i] && buffer[i]->data)
				slicecnt++;
		}

		//fprintf(fplog, "slicecnt=%d, dpb->dpbSize=%d\n", slicecnt, dpb->dpbSize);
		if(1)//(slicecnt == dpb->dpbSize)
		{

			i32 poc,diff = (dpb->dpbSize*2+6);
			i32 currpoc = MIN(pStorage->poc->picOrderCnt[0],
                      pStorage->poc->picOrderCnt[1]);
			for (i = 0; i < dpb->dpbSize; i++)
			{
				if(buffer[i] && buffer[i]->data)
				{
					poc = GET_POC(buffer[i]);
					poc = currpoc - poc;
					if((poc > diff) || (poc < -diff))
					{
						if(IS_SHORT_TERM(buffer[i], FRAME) && !buffer[i]->toBeDisplayed)
						{
							if(poc > 0)diff = poc;
							else diff = -poc;
							tmp = buffer[i];
						}
					}
				}
			}

			if((tmp) && (!tmp->isItype))
			{
				DPBOUTDEBUG("remove not use reference buffer at long time! %d\n", diff);
				SET_STATUS(tmp, UNUSED, FRAME);
				if(IS_UNUSED(tmp, FRAME))
				{
					DpbBufFree(dpb, tmp);
				}
			}
		}
	}

    if (pStorage->sliceHeader->fieldPicFlag == 0){
		currentOut->h264hwStatus[0] = pDecCont->h264hwStatus[0];
        picStruct = FRAME;
    }else{
		picStruct = pStorage->sliceHeader->bottomFieldFlag;
		currentOut->h264hwStatus[picStruct] = pDecCont->h264hwStatus[picStruct];
		//ALOGE("pDecCont->h264hwStatus[%d]=0x%x", picStruct, pDecCont->h264hwStatus[picStruct]);
	}

	//return ;

    if(picStruct < FRAME) {

		if (!(currentOut->h264hwStatus[picStruct] & 0x1000))
		{
			dpbPicture_t *dpbPic = NULL;
			dpbPicture_t *tmp = NULL;
			i32 pocdelat = 0x7FFFFFFF;
			i32 current_poc = GET_POC(currentOut);

            if(pDecCont->ts_en == 3)
            {   
            pDecCont->dommco5 = 1;
            return ;    //发现出现就flush dpblist
            }
			DPBOUTDEBUG("currentOut->h264hwStatus[%d]=0x%x", picStruct, currentOut->h264hwStatus[picStruct]);

			if((pStorage->sliceHeader->sliceType==I_SLICE)||(pStorage->sliceHeader->sliceType==(I_SLICE+5)))
			{
				DPBOUTDEBUG("I find i field has error, do mmco5");
				pDecCont->dommco5 = 1;
				return ;
			}

			list_for_each_entry(dpbPic, &dpb->dpbList, list) {
				i32 poc = GET_POC(dpbPic);

				if(poc >= current_poc)
					poc -= current_poc;
				else
					poc = current_poc - poc;

		        if ((poc >= pocdelat)||(!(dpbPic->h264hwStatus[picStruct]&0x1000))||
					(dpbPic == currentOut))
		            continue;

		        pocdelat = poc;
		        tmp = dpbPic;
		    }

			if(tmp == NULL)
			{
				pocdelat = 0x7FFFFFFF;
				tmp = NULL;
				dpbPic = NULL;

				list_for_each_entry(dpbPic, &dpb->dpbList, list) {
					i32 poc = GET_POC(dpbPic);

					if(poc >= current_poc)
						poc -= current_poc;
					else
						poc = current_poc - poc;

					if ((poc >= pocdelat)||(dpbPic == currentOut))
						continue;

					pocdelat = poc;
					tmp = dpbPic;
				}

				if(tmp == NULL)
				{
					DPBOUTDEBUG("I CAN NOT FIND REFRENCE BUFFER! %d");//, pDecCont->writeframecounter);
					pDecCont->dommco5 = 1;
					return ;
				}
			}


			{
				u32 picwidth = pSps->picWidthInMbs*16;
				u32 picheight = pSps->picHeightInMbs*16;
				u8 *ysrc = (u8 *)tmp->data->mem.vir_addr;
				u8 *csrc = (u8 *)tmp->data->mem.vir_addr + picwidth*picheight;
				u8 *ydst = (u8 *)dpb->currentOut->data->mem.vir_addr;
				u8 *cdst = (u8 *)dpb->currentOut->data->mem.vir_addr + picwidth*picheight;
				u8 *srcy, *dsty, *srcc, *dstc;
				u32 i,j;

				//fprintf(fplog, "reference buffer's poc is=%d\n", GET_POC(tmp));
				DPBOUTDEBUG("reference buffer's poc is=%d", GET_POC(tmp));

				VPUMemInvalidate(&dpb->currentOut->data->mem);
				VPUMemInvalidate(&tmp->data->mem);

				if((!(pDecCont->h264hwStatus[picStruct]&0x1000)) && (picStruct == 0))
				{
					u32 *p = (u32 *)ydst;
					#if 1
					for(i=0;i<picheight;i+=2)
						if(p[i*picwidth/4] == 0x66339988)
						{
							DPBOUTDEBUG("I FIND top field OUTPUT DATAADDRESS=%d", i);
							break;
						}
					#else
					i = 0;
					#endif


					if(i&2)
						i -= 2;
					if(i > 32)
						i -= 32;
					else
						i = 0;
					srcy = ysrc + i*picwidth;
					srcc = csrc + i*picwidth/2;
					dsty = ydst + i*picwidth;
					dstc = cdst + i*picwidth/2;
					for(;i<picheight;i+=2)
					{
						memcpy(dsty, srcy, picwidth);
						srcy += picwidth*2;
						dsty += picwidth*2;
						if(!(i&2))
						{
							memcpy(dstc, srcc, picwidth);
							srcc += picwidth*2;
							dstc += picwidth*2;
						}
					}
				}

				if((!(pDecCont->h264hwStatus[picStruct]&0x1000)) && (picStruct == 1))
				{
					u32 *p = (u32 *)ydst;
					#if 1
					for(i=1;i<picheight;i+=2)
						if(p[i*picwidth/4] == 0x66339988)
						{
							DPBOUTDEBUG("I FIND bottom field OUTPUT DATAADDRESS=%d", i);
							break;
						}
					#else
					i = 1;
					#endif

					//if(i > picheight)return;

					if(i&2)
						i -= 2;
					j = (i+1)/2;
					if(i > 32)j-=16;
					else j = 1;
					if(i > 32)i-=32;
					else i = 1;
					srcy = ysrc + i*picwidth;
					srcc = csrc + j*picwidth;
					dsty = ydst + i*picwidth;
					dstc = cdst + j*picwidth;
					for(;i<picheight;i+=2)
					{
						memcpy(dsty, srcy, picwidth);
						srcy += (picwidth*2);
						dsty += (picwidth*2);
						if(!(i&2))
						{
							memcpy(dstc, srcc, picwidth);
							srcc += picwidth*2;
							dstc += picwidth*2;
						}
					}
				}

				VPUMemClean(&dpb->currentOut->data->mem);
			}
		}

		#ifdef DPBWRITEFILE
		if(currentOut->status[(u32)!picStruct] != EMPTY)	//把写文件放在补偿之后
		{
			#define OUTPUT_WIDTH		80
			#define OUTPUT_HEIGHT		96
			VPUMemLinear_t dstmem;
			u8 *src = (u8 *)dpb->currentOut->data->mem.vir_addr;
			u8 *dst;
			u32 scalex = pSps->picWidthInMbs*16/OUTPUT_WIDTH;
			u32 scaley = pSps->picHeightInMbs*16/OUTPUT_HEIGHT;
			u32 i,j;

			memset(&dstmem, 0, sizeof(dstmem));
			VPUMallocLinear(&dstmem, (OUTPUT_WIDTH*OUTPUT_HEIGHT*2));
			dst = (u8 *)dstmem.vir_addr;

			VPUMemInvalidate(&dpb->currentOut->data->mem);

			for(i=0;i<OUTPUT_HEIGHT;i++)
				for(j=0;j<OUTPUT_WIDTH;j++)
				{
					dst[i*OUTPUT_WIDTH+j] = src[pSps->picWidthInMbs*16*i*scaley+j*scalex];
				}

			src = src + pSps->picWidthInMbs*16*pSps->picHeightInMbs*16;
			dst = dst + OUTPUT_WIDTH*OUTPUT_HEIGHT;
			for(i=0;i<OUTPUT_HEIGHT/2;i++)
				for(j=0;j<OUTPUT_WIDTH/2;j++)
				{
					dst[i*OUTPUT_WIDTH+j*2] = src[pSps->picWidthInMbs*16*i*scaley+j*scalex*2];
					dst[i*OUTPUT_WIDTH+j*2+1] = src[pSps->picWidthInMbs*16*i*scaley+j*scalex*2+1];
				}

			DPBDEBUG("write output file %d\n", pDecCont->writeframecounter);
			fwrite(dstmem.vir_addr, 1, OUTPUT_WIDTH*OUTPUT_HEIGHT*3/2, pDecCont->fpout);
			pDecCont->writeframecounter++;
			VPUFreeLinear(&dstmem);
		}
		#endif

		return ;

    }else{

		dpbPicture_t *dpbPic = NULL;
		dpbPicture_t *tmp = NULL;
		i32 pocdelat = 0x7FFFFFFF;
		i32 current_poc = GET_POC(currentOut);

		if (!(currentOut->h264hwStatus[0] & 0x1000))	//这样判断是因为不管有没出错都会写文件
		{
		    if(pDecCont->ts_en == 3)
            {   
		    pDecCont->dommco5 = 1;
            return ;    //发现出现就flush dpblist
            }
			list_for_each_entry(dpbPic, &dpb->dpbList, list) {
				i32 poc = GET_POC(dpbPic);

				if(poc >= current_poc)
					poc -= current_poc;
				else
					poc = current_poc - poc;

				if ((poc >= pocdelat)||(!(dpbPic->h264hwStatus[0]&0x1000)))
					continue;

				pocdelat = poc;
				tmp = dpbPic;
			}

			if(tmp == NULL)
			{
				pocdelat = 0x7FFFFFFF;
				tmp = NULL;
				dpbPic = NULL;

				list_for_each_entry(dpbPic, &dpb->dpbList, list) {
					i32 poc = GET_POC(dpbPic);

					if(poc >= current_poc)
						poc -= current_poc;
					else
						poc = current_poc - poc;

					if (poc >= pocdelat)
						continue;

					pocdelat = poc;
					tmp = dpbPic;
				}

				if(tmp == NULL)
				{
					DPBOUTDEBUG("I CAN NOT FIND REFRENCE BUFFER!, DO MMCO5");
					pDecCont->dommco5 = 1;
					return ;
				}
			}

			#if 1
			{
				u32 picwidth = pSps->picWidthInMbs*16;
				u32 picheight = pSps->picHeightInMbs*16;
				u8 *ysrc = (u8 *)tmp->data->mem.vir_addr;
				u8 *csrc = (u8 *)tmp->data->mem.vir_addr + picwidth*picheight;
				u8 *ydst = (u8 *)dpb->currentOut->data->mem.vir_addr;
				u8 *cdst = (u8 *)dpb->currentOut->data->mem.vir_addr + picwidth*picheight;
				u8 *srcy, *dsty, *srcc, *dstc;
				u32 i,j;

				DPBOUTDEBUG("frame reference buffer's poc is=%d", GET_POC(tmp));

				VPUMemInvalidate(&dpb->currentOut->data->mem);
				VPUMemInvalidate(&tmp->data->mem);

				{
					u32 *p = (u32 *)ydst;
					#if 1
					for(i=0;i<picheight;i++)
						if(p[i*picwidth/4] == 0x66339988)
						{
							DPBOUTDEBUG("I FIND frame OUTPUT DATAADDRESS=%d", i);
							break;
						}
					if(i&0xf)
						i = i - (i&0xf);
					if(i > 32)
						i -= 32;
					else
						i = 0;
					#else
					i = 0;
					#endif

					srcy = ysrc + i*picwidth;
					srcc = csrc + i*picwidth/2;
					dsty = ydst + i*picwidth;
					dstc = cdst + i*picwidth/2;
					for(;i<picheight;i++)
					{
						memcpy(dsty, srcy, picwidth);
						srcy += picwidth;
						dsty += picwidth;
						if(!(i&2))
						{
							memcpy(dstc, srcc, picwidth);
							srcc += picwidth;
							dstc += picwidth;
						}
					}
				}

				VPUMemClean(&dpb->currentOut->data->mem);

			}
			#else
			{
				i32 insize,totalsize;
				u32 picwidth = pSps->picWidthInMbs*16;
				u32 picheight = pSps->picHeightInMbs*16;
				u8 *ysrc = (u8 *)tmp->data->mem.vir_addr;
				u8 *csrc = (u8 *)tmp->data->mem.vir_addr + picwidth*picheight;
				u8 *ydst = (u8 *)dpb->currentOut->data->mem.vir_addr;
				u8 *cdst = (u8 *)dpb->currentOut->data->mem.vir_addr + picwidth*picheight;
				u32 errorlines;

				VPUMemInvalidate(&tmp->data->mem);
				memcpy((ydst), (ysrc), picheight*picwidth*3/2); //y and c
				VPUMemClean(&dpb->currentOut->data->mem);

				return ;
			}
			#endif
		}

		#ifdef DPBWRITEFILE
		{
			//#define OUTPUT_WIDTH		96
			//#define OUTPUT_HEIGHT 	80
			VPUMemLinear_t dstmem;
			u8 *src = (u8 *)dpb->currentOut->data->mem.vir_addr;
			u8 *dst;
			u32 scalex = pSps->picWidthInMbs*16/OUTPUT_WIDTH;
			u32 scaley = pSps->picHeightInMbs*16/OUTPUT_HEIGHT;
			u32 i,j;

			memset(&dstmem, 0, sizeof(dstmem));
			VPUMallocLinear(&dstmem, (OUTPUT_WIDTH*OUTPUT_HEIGHT*2));
			dst = (u8 *)dstmem.vir_addr;

			VPUMemInvalidate(&dpb->currentOut->data->mem);

			for(i=0;i<OUTPUT_HEIGHT;i++)
				for(j=0;j<OUTPUT_WIDTH;j++)
				{
					dst[i*OUTPUT_WIDTH+j] = src[pSps->picWidthInMbs*16*i*scaley+j*scalex];
				}

			src = src + pSps->picWidthInMbs*16*pSps->picHeightInMbs*16;
			dst = dst + OUTPUT_WIDTH*OUTPUT_HEIGHT;
			for(i=0;i<OUTPUT_HEIGHT/2;i++)
				for(j=0;j<OUTPUT_WIDTH/2;j++)
				{
					dst[i*OUTPUT_WIDTH+j*2] = src[pSps->picWidthInMbs*16*i*scaley+j*scalex*2];
					dst[i*OUTPUT_WIDTH+j*2+1] = src[pSps->picWidthInMbs*16*i*scaley+j*scalex*2+1];
				}

			DPBDEBUG("write output file %d\n", pDecCont->writeframecounter);
			fwrite(dstmem.vir_addr, 1, OUTPUT_WIDTH*OUTPUT_HEIGHT*3/2, pDecCont->fpout);
			pDecCont->writeframecounter++;
			VPUFreeLinear(&dstmem);
		}
		#endif

		#if 0
		if((pDecCont->startwritefile)&&(pDecCont->writeframecounter < 30))
		{
			u8 *ydst = (u8 *)dpb->currentOut->data->mem.vir_addr;
			LOGE("write output file %d\n", pDecCont->writeframecounter);
			fprintf(fplog,	"write output file %d\n", pDecCont->writeframecounter);
			fwrite(ydst, 1, pSps->picWidthInMbs*pSps->picHeightInMbs*384, fpout);
			pDecCont->writeframecounter++;
		}
		#endif

		return ;
	}

	return ;
}

}

