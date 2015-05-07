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
--  Abstract : Storage handling functionality
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264hwd_storage.c,v $
--  $Date: 2010/05/04 13:53:15 $
--  $Revision: 1.15 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "h264hwd_storage.h"
#include "h264hwd_util.h"
#include "h264hwd_slice_group_map.h"
#include "h264hwd_dpb.h"
#include "h264hwd_nal_unit.h"
#include "h264hwd_slice_header.h"
#include "h264hwd_seq_param_set.h"
#include "dwl.h"
#include <utils/Log.h>
#include <string.h>
#include <stdlib.h>

namespace android {

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

static u32 CheckPps(picParamSet_t * pps, seqParamSet_t * sps);

/*------------------------------------------------------------------------------

    Function name: h264bsdInitStorage

        Functional description:
            Initialize storage structure. Sets contents of the storage to '0'
            except for the active parameter set ids, which are initialized
            to invalid values.

        Inputs:

        Outputs:
            pStorage    initialized data stored here

        Returns:
            none

------------------------------------------------------------------------------*/

void h264bsdInitStorage(storage_t * pStorage)
{

/* Variables */

    u32 i;

/* Code */

    ASSERT(pStorage);

    memset(pStorage, 0, sizeof(storage_t));

    pStorage->activeSpsId = MAX_NUM_SEQ_PARAM_SETS;
    pStorage->activePpsId = MAX_NUM_PIC_PARAM_SETS;
    for (i = 0; i < MAX_NUM_VIEWS; i++)
        pStorage->activeViewSpsId[i] = MAX_NUM_SEQ_PARAM_SETS;
    pStorage->oldSpsId = MAX_NUM_SEQ_PARAM_SETS;
    pStorage->aub->firstCallFlag = HANTRO_TRUE;
}

static void h264bsdFreeVuiParameters(seqParamSet_t *sps)
{
    if (sps->vuiParameters) {
        free(sps->vuiParameters);
        sps->vuiParameters = NULL;
    }
}

void h264bsdFreeSeqParamSet(seqParamSet_t *sps)
{
    if (sps) {
        h264bsdFreeVuiParameters(sps);
        free(sps);
    }
}

void h264bsdFreeSeqParamSetById(storage_t * pStorage, u32 id)
{
    seqParamSet_t *sps = pStorage->sps[id];
    pStorage->sps[id] = NULL;
    if (sps) {
        h264bsdFreeSeqParamSet(sps);
    }
}


/*------------------------------------------------------------------------------

    Function: h264bsdStoreSeqParamSet

        Functional description:
            Store sequence parameter set into the storage. If active SPS is
            overwritten -> check if contents changes and if it does, set
            parameters to force reactivation of parameter sets

        Inputs:
            pStorage        pointer to storage structure
            pSeqParamSet    pointer to param set to be stored

        Outputs:
            none

        Returns:
            HANTRO_OK                success
            MEMORY_ALLOCATION_ERROR  failure in memory allocation

------------------------------------------------------------------------------*/

u32 h264bsdStoreSeqParamSet(storage_t * pStorage, seqParamSet_t * pSeqParamSet)
{

/* Variables */

    u32 id;

/* Code */

    ASSERT(pStorage);
    ASSERT(pSeqParamSet);
    ASSERT(pSeqParamSet->seqParameterSetId < MAX_NUM_SEQ_PARAM_SETS);

    id = pSeqParamSet->seqParameterSetId;

    /* sequence parameter set with id equal to id of active sps */
    if (id == pStorage->activeSpsId)
    {
        /* if seq parameter set contents changes
         *    -> overwrite and re-activate when next IDR picture decoded
         *    ids of active param sets set to invalid values to force
         *    re-activation. Memories allocated for old sps freed
         * otherwise free memeries allocated for just decoded sps and
         * continue */
        if (h264bsdCompareSeqParamSets(pSeqParamSet, pStorage->activeSps) != 0)
        {
            pStorage->activeSpsId = MAX_NUM_SEQ_PARAM_SETS + 1;
            pStorage->activePpsId = MAX_NUM_PIC_PARAM_SETS + 1;
            pStorage->activeSps = NULL;
            pStorage->activePps = NULL;
        }
    }
    h264bsdFreeSeqParamSetById(pStorage, id);

    pStorage->sps[id] = pSeqParamSet;

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: h264bsdStorePicParamSet

        Functional description:
            Store picture parameter set into the storage. If active PPS is
            overwritten -> check if active SPS changes and if it does -> set
            parameters to force reactivation of parameter sets

        Inputs:
            pStorage        pointer to storage structure
            pPicParamSet    pointer to param set to be stored

        Outputs:
            none

        Returns:
            HANTRO_OK                success
            MEMORY_ALLOCATION_ERROR  failure in memory allocation

------------------------------------------------------------------------------*/

void h264bsdModifyScalingLists(storage_t *pStorage, picParamSet_t *pPicParamSet)
{
    u32 i;
    seqParamSet_t *sps;

    sps = pStorage->sps[pPicParamSet->seqParameterSetId];
    /* SPS not yet decoded -> cannot copy */
    /* TODO: set flag to handle "missing" SPS lists properly */
    if (sps == NULL)
        return;

    if (!pPicParamSet->scalingMatrixPresentFlag &&
        sps->scalingMatrixPresentFlag)
    {
        pPicParamSet->scalingMatrixPresentFlag = 1;
        memcpy(pPicParamSet->scalingList, sps->scalingList, sizeof(sps->scalingList));
    }
    else if (sps->scalingMatrixPresentFlag)
    {
        if (!pPicParamSet->scalingListPresent[0])
        {
            /* we trust our memcpy */
            memcpy(pPicParamSet->scalingList[0], sps->scalingList[0], 16*sizeof(u8));
            for (i = 1; i < 3; i++)
                if (!pPicParamSet->scalingListPresent[i])
                    memcpy(pPicParamSet->scalingList[i],
                        pPicParamSet->scalingList[i-1],
                        16*sizeof(u8));
        }
        if (!pPicParamSet->scalingListPresent[3])
        {
            memcpy(pPicParamSet->scalingList[3], sps->scalingList[3], 16*sizeof(u8));
            for (i = 4; i < 6; i++)
                if (!pPicParamSet->scalingListPresent[i])
                    memcpy(pPicParamSet->scalingList[i],
                        pPicParamSet->scalingList[i-1],
                        16*sizeof(u8));
        }
        for (i = 6; i < 8; i++)
            if (!pPicParamSet->scalingListPresent[i])
                memcpy(pPicParamSet->scalingList[i], sps->scalingList[i],
                    64*sizeof(u8));
    }
}

u32 h264bsdStorePicParamSet(storage_t * pStorage, picParamSet_t * pPicParamSet)
{

/* Variables */

    u32 id;

/* Code */

    ASSERT(pStorage);
    ASSERT(pPicParamSet);
    ASSERT(pPicParamSet->picParameterSetId < MAX_NUM_PIC_PARAM_SETS);
    ASSERT(pPicParamSet->seqParameterSetId < MAX_NUM_SEQ_PARAM_SETS);

    id = pPicParamSet->picParameterSetId;

    /* picture parameter set with id equal to id of active pps */
    if (id == pStorage->activePpsId)
    {
        /* check whether seq param set changes, force re-activation of
         * param set if it does. Set activeSpsId to invalid value to
         * accomplish this */
        if(pPicParamSet->seqParameterSetId != pStorage->activeSpsId)
        {
            pStorage->activePpsId = MAX_NUM_PIC_PARAM_SETS + 1;
        }
    }

    /* Modify scaling_lists if necessary */
    h264bsdModifyScalingLists(pStorage, pPicParamSet);

    pStorage->pps[id] = *pPicParamSet;

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: h264bsdActivateParamSets

        Functional description:
            Activate certain SPS/PPS combination. This function shall be
            called in the beginning of each picture. Picture parameter set
            can be changed as wanted, but sequence parameter set may only be
            changed when the starting picture is an IDR picture.

            When new SPS is activated the function allocates memory for
            macroblock storages and slice group map and (re-)initializes the
            decoded picture buffer. If this is not the first activation the old
            allocations are freed and FreeDpb called before new allocations.

        Inputs:
            pStorage        pointer to storage data structure
            ppsId           identifies the PPS to be activated, SPS id obtained
                            from the PPS
            isIdr           flag to indicate if the picture is an IDR picture

        Outputs:
            none

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      non-existing or invalid param set combination,
                            trying to change SPS with non-IDR picture
            MEMORY_ALLOCATION_ERROR     failure in memory allocation

------------------------------------------------------------------------------*/

u32 h264bsdActivateParamSets(storage_t * pStorage, u32 ppsId, u32 isIdr)
{
    u32 tmp;

    ASSERT(pStorage);
    ASSERT(ppsId < MAX_NUM_PIC_PARAM_SETS);

    /* check that pps parameters do not violate picture size constraints */
    tmp = CheckPps(&pStorage->pps[ppsId], pStorage->sps[pStorage->pps[ppsId].seqParameterSetId]);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* first activation */
    if (pStorage->activePpsId == MAX_NUM_PIC_PARAM_SETS)
    {
        pStorage->activePpsId = ppsId;
        pStorage->activePps = &pStorage->pps[ppsId];
        pStorage->activeSpsId = pStorage->activePps->seqParameterSetId;
        pStorage->activeViewSpsId[pStorage->view] =
            pStorage->activePps->seqParameterSetId;
        pStorage->activeSps = pStorage->sps[pStorage->activeSpsId];
        pStorage->activeViewSps[pStorage->view] = *pStorage->sps[pStorage->activeSpsId];
    }
    else if(ppsId != pStorage->activePpsId)
    {
        /* sequence parameter set shall not change but before an IDR picture */
        if (pStorage->activeSpsId > MAX_NUM_SEQ_PARAM_SETS) {
            pStorage->activeSpsChanged = 1;
        }

        if (pStorage->pps[ppsId].seqParameterSetId !=
            pStorage->activeViewSpsId[pStorage->view] ||
            pStorage->activeSpsChanged)
        {
            DEBUG_PRINT(("SEQ PARAM SET CHANGING...\n"));
            pStorage->activePpsId = ppsId;
            pStorage->activePps = &pStorage->pps[ppsId];
            pStorage->activeViewSpsId[pStorage->view] = pStorage->activePps->seqParameterSetId;
            pStorage->activeViewSps[pStorage->view] = *pStorage->sps[pStorage->activeViewSpsId[pStorage->view]];
            if(isIdr) {
                ALOGW("TRYING TO CHANGE SPS IN NON-IDR SLICE\n");
            }
        }
        else
        {
            pStorage->activePpsId = ppsId;
            pStorage->activePps = &pStorage->pps[ppsId];
        }
    }

    if (isIdr)
        pStorage->numViews = pStorage->view != 0;

    pStorage->activeSpsId = pStorage->activeViewSpsId[pStorage->view];
    pStorage->activeSps = &pStorage->activeViewSps[pStorage->view];
    pStorage->dpb = pStorage->dpbs[pStorage->view];

    return (HANTRO_OK);
}

/*------------------------------------------------------------------------------

    Function: h264bsdResetStorage

        Functional description:
            Reset contents of the storage. This should be called before
            processing of new image is started.

        Inputs:
            pStorage    pointer to storage structure

        Outputs:
            none

        Returns:
            none

------------------------------------------------------------------------------*/
#if 0
void h264bsdResetStorage(storage_t * pStorage)
{

/* Variables */

    u32 i;

/* Code */

    ASSERT(pStorage);

    //pStorage->slice->numDecodedMbs = 0;
    //pStorage->slice->sliceId = 0;
}
#endif
/*------------------------------------------------------------------------------

    Function: h264bsdIsStartOfPicture

        Functional description:
            Determine if the decoder is in the start of a picture. This
            information is needed to decide if h264bsdActivateParamSets and
            h264bsdCheckGapsInFrameNum functions should be called. Function
            considers that new picture is starting if no slice headers
            have been successfully decoded for the current access unit.

        Inputs:
            pStorage    pointer to storage structure

        Outputs:
            none

        Returns:
            HANTRO_TRUE        new picture is starting
            HANTRO_FALSE       not starting

------------------------------------------------------------------------------*/

u32 h264bsdIsStartOfPicture(storage_t * pStorage)
{

/* Variables */

/* Code */

    if(pStorage->validSliceInAccessUnit == HANTRO_FALSE)
        return (HANTRO_TRUE);
    else
        return (HANTRO_FALSE);

}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckAccessUnitBoundary

        Functional description:
            Check if next NAL unit starts a new access unit. Following
            conditions specify start of a new access unit:

                -NAL unit types 6-11, 13-18 (e.g. SPS, PPS)

           following conditions checked only for slice NAL units, values
           compared to ones obtained from previous slice:

                -NAL unit type differs (slice / IDR slice)
                -frame_num differs
                -nal_ref_idc differs and one of the values is 0
                -POC information differs
                -both are IDR slices and idr_pic_id differs

        Inputs:
            strm        pointer to stream data structure
            nuNext      pointer to NAL unit structure
            storage     pointer to storage structure

        Outputs:
            accessUnitBoundaryFlag  the result is stored here, HANTRO_TRUE for
                                    access unit boundary, HANTRO_FALSE otherwise

        Returns:
            HANTRO_OK           success
            HANTRO_NOK          failure, invalid stream data
            PARAM_SET_ERROR     invalid param set usage

------------------------------------------------------------------------------*/

u32 h264bsdCheckAccessUnitBoundary(strmData_t * strm,
                                   nalUnit_t * nuNext,
                                   storage_t * storage,
                                   u32 * accessUnitBoundaryFlag)
{

/* Variables */

    u32 tmp, ppsId, frameNum, idrPicId, picOrderCntLsb;
    u32 fieldPicFlag = 0, bottomFieldFlag = 0;
    i32 deltaPicOrderCntBottom, deltaPicOrderCnt[2];
    seqParamSet_t *sps = NULL;
    picParamSet_t *pps = NULL;
    u32 view = 0;

/* Code */

    ASSERT(strm);
    ASSERT(nuNext);
    ASSERT(storage);
    ASSERT(storage->sps);
    ASSERT(storage->pps);

    DEBUG_PRINT(("h264bsdCheckAccessUnitBoundary #\n"));
    /* initialize default output to HANTRO_FALSE */
    *accessUnitBoundaryFlag = HANTRO_FALSE;

    /* TODO field_pic_flag, bottom_field_flag */

    if(((nuNext->nalUnitType > 5) && (nuNext->nalUnitType < 12)) ||
       ((nuNext->nalUnitType > 12) && (nuNext->nalUnitType <= 18)))
    {
        *accessUnitBoundaryFlag = HANTRO_TRUE;
        return (HANTRO_OK);
    }
    else if(nuNext->nalUnitType != NAL_CODED_SLICE &&
            nuNext->nalUnitType != NAL_CODED_SLICE_IDR &&
            nuNext->nalUnitType != NAL_CODED_SLICE_EXT)
    {
        return (HANTRO_OK);
    }

    /* check if this is the very first call to this function */
    if(storage->aub->firstCallFlag)
    {
        *accessUnitBoundaryFlag = HANTRO_TRUE;
        storage->aub->firstCallFlag = HANTRO_FALSE;
    }

    /* get picture parameter set id */
    tmp = h264bsdCheckPpsId(strm, &ppsId);
    if(tmp != HANTRO_OK)
        return (tmp);

    if (nuNext->nalUnitType == NAL_CODED_SLICE_EXT)
        view = 1;

    /* store sps and pps in separate pointers just to make names shorter */
    pps = &storage->pps[ppsId];

    sps = storage->sps[pps->seqParameterSetId];

    if (NULL == pps || NULL == sps)
        return (HANTRO_OK);

    /* another view does not start new access unit unless new viewId is
     * smaller than previous, but other views are handled like new access units
     * (param set activation etc) */
    if(storage->aub->nuPrev->viewId != nuNext->viewId)
        *accessUnitBoundaryFlag = HANTRO_TRUE;

    if(storage->aub->nuPrev->nalRefIdc != nuNext->nalRefIdc &&
       (storage->aub->nuPrev->nalRefIdc == 0 || nuNext->nalRefIdc == 0))
    {
        *accessUnitBoundaryFlag = HANTRO_TRUE;
        storage->aub->newPicture = HANTRO_TRUE;
    }
    else
        storage->aub->newPicture = HANTRO_FALSE;

    if((storage->aub->nuPrev->nalUnitType == NAL_CODED_SLICE_IDR &&
        nuNext->nalUnitType != NAL_CODED_SLICE_IDR) ||
       (storage->aub->nuPrev->nalUnitType != NAL_CODED_SLICE_IDR &&
        nuNext->nalUnitType == NAL_CODED_SLICE_IDR))
        *accessUnitBoundaryFlag = HANTRO_TRUE;

    tmp = h264bsdCheckFrameNum(strm, sps->maxFrameNum, &frameNum);
    if(tmp != HANTRO_OK)
        return (HANTRO_NOK);

    if(storage->aub->prevFrameNum != frameNum)
    {
        storage->aub->prevFrameNum = frameNum;
        *accessUnitBoundaryFlag = HANTRO_TRUE;
    }

    tmp = h264bsdCheckFieldPicFlag(strm, sps->maxFrameNum, nuNext->nalUnitType,
        !sps->frameMbsOnlyFlag, &fieldPicFlag);

    if (fieldPicFlag != storage->aub->prevFieldPicFlag)
    {
        storage->aub->prevFieldPicFlag = fieldPicFlag;
        *accessUnitBoundaryFlag = HANTRO_TRUE;
    }

    tmp = h264bsdCheckBottomFieldFlag(strm, sps->maxFrameNum,
        nuNext->nalUnitType,
        !sps->frameMbsOnlyFlag, &bottomFieldFlag);

    DEBUG_PRINT(("FIELD %d bottom %d\n",fieldPicFlag, bottomFieldFlag));

    if (bottomFieldFlag != storage->aub->prevBottomFieldFlag)
    {
        storage->aub->prevBottomFieldFlag = bottomFieldFlag;
        *accessUnitBoundaryFlag = HANTRO_TRUE;
    }

    if(nuNext->nalUnitType == NAL_CODED_SLICE_IDR)
    {
        tmp = h264bsdCheckIdrPicId(strm, sps->maxFrameNum,
            nuNext->nalUnitType, !sps->frameMbsOnlyFlag, &idrPicId);
        if(tmp != HANTRO_OK)
            return (HANTRO_NOK);

        if(storage->aub->nuPrev->nalUnitType == NAL_CODED_SLICE_IDR &&
           storage->aub->prevIdrPicId != idrPicId)
            *accessUnitBoundaryFlag = HANTRO_TRUE;

        storage->aub->prevIdrPicId = idrPicId;
    }

    if(sps->picOrderCntType == 0)
    {
        tmp = h264bsdCheckPicOrderCntLsb(strm, sps, nuNext->nalUnitType,
                                         &picOrderCntLsb);
        if(tmp != HANTRO_OK)
            return (HANTRO_NOK);

        if(storage->aub->prevPicOrderCntLsb != picOrderCntLsb)
        {
            storage->aub->prevPicOrderCntLsb = picOrderCntLsb;
            *accessUnitBoundaryFlag = HANTRO_TRUE;
        }

        if(pps->picOrderPresentFlag)
        {
            tmp = h264bsdCheckDeltaPicOrderCntBottom(strm, sps,
                                                     nuNext->nalUnitType,
                                                     &deltaPicOrderCntBottom);
            if(tmp != HANTRO_OK)
                return (tmp);

            if(storage->aub->prevDeltaPicOrderCntBottom !=
               deltaPicOrderCntBottom)
            {
                storage->aub->prevDeltaPicOrderCntBottom =
                    deltaPicOrderCntBottom;
                *accessUnitBoundaryFlag = HANTRO_TRUE;
            }
        }
    }
    else if(sps->picOrderCntType == 1 && !sps->deltaPicOrderAlwaysZeroFlag)
    {
        tmp = h264bsdCheckDeltaPicOrderCnt(strm, sps, nuNext->nalUnitType,
                                           pps->picOrderPresentFlag,
                                           deltaPicOrderCnt);
        if(tmp != HANTRO_OK)
            return (tmp);

        if(storage->aub->prevDeltaPicOrderCnt[0] != deltaPicOrderCnt[0])
        {
            storage->aub->prevDeltaPicOrderCnt[0] = deltaPicOrderCnt[0];
            *accessUnitBoundaryFlag = HANTRO_TRUE;
        }

        if(pps->picOrderPresentFlag)
            if(storage->aub->prevDeltaPicOrderCnt[1] != deltaPicOrderCnt[1])
            {
                storage->aub->prevDeltaPicOrderCnt[1] = deltaPicOrderCnt[1];
                *accessUnitBoundaryFlag = HANTRO_TRUE;
            }
    }

    *storage->aub->nuPrev = *nuNext;

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: CheckPps

        Functional description:
            Check picture parameter set. Contents of the picture parameter
            set information that depends on the image dimensions is checked
            against the dimensions in the sps.

        Inputs:
            pps     pointer to picture paramter set
            sps     pointer to sequence parameter set

        Outputs:
            none

        Returns:
            HANTRO_OK      everything ok
            HANTRO_NOK     invalid data in picture parameter set

------------------------------------------------------------------------------*/
u32 CheckPps(picParamSet_t * pps, seqParamSet_t * sps)
{

    u32 i;
    u32 picSize;

    if (NULL == sps || NULL == pps)
        return(HANTRO_NOK);

    picSize = sps->picWidthInMbs * sps->picHeightInMbs;

    /* check slice group params */
    if(pps->numSliceGroups > 1)
    {
        /* no FMO supported if stream may contain interlaced stuff */
        if (sps->frameMbsOnlyFlag == 0)
            return(HANTRO_NOK);

        if(pps->sliceGroupMapType == 0)
        {
            ASSERT(pps->runLength);
            for(i = 0; i < pps->numSliceGroups; i++)
            {
                if(pps->runLength[i] > picSize)
                    return (HANTRO_NOK);
            }
        }
        else if(pps->sliceGroupMapType == 2)
        {
            ASSERT(pps->topLeft);
            ASSERT(pps->bottomRight);
            for(i = 0; i < pps->numSliceGroups - 1; i++)
            {
                if(pps->topLeft[i] > pps->bottomRight[i] ||
                   pps->bottomRight[i] >= picSize)
                    return (HANTRO_NOK);

                if((pps->topLeft[i] % sps->picWidthInMbs) >
                   (pps->bottomRight[i] % sps->picWidthInMbs))
                    return (HANTRO_NOK);
            }
        }
        else if(pps->sliceGroupMapType > 2 && pps->sliceGroupMapType < 6)
        {
            if(pps->sliceGroupChangeRate > picSize)
                return (HANTRO_NOK);
        }
        else if (pps->sliceGroupMapType == 6 &&
                 pps->picSizeInMapUnits < picSize)
            return(HANTRO_NOK);
    }

    return (HANTRO_OK);
}

/*------------------------------------------------------------------------------

    Function: h264bsdValidParamSets

        Functional description:
            Check if any valid SPS/PPS combination exists in the storage.
            Function tries each PPS in the buffer and checks if corresponding
            SPS exists and calls CheckPps to determine if the PPS conforms
            to image dimensions of the SPS.

        Inputs:
            pStorage    pointer to storage structure

        Outputs:
            HANTRO_OK   there is at least one valid combination
            HANTRO_NOK  no valid combinations found

------------------------------------------------------------------------------*/

u32 h264bsdValidParamSets(storage_t * pStorage)
{

/* Variables */

    u32 i;

/* Code */

    ASSERT(pStorage);

    for(i = 0; i < MAX_NUM_PIC_PARAM_SETS; i++)
    {
        if(/*pStorage->pps[i] &&
           pStorage->sps[pStorage->pps[i]->seqParameterSetId] &&*/
           CheckPps(&pStorage->pps[i], pStorage->sps[pStorage->pps[i].seqParameterSetId]) ==
           HANTRO_OK)
        {
            return (HANTRO_OK);
        }
    }

    return (HANTRO_NOK);

}

/*------------------------------------------------------------------------------
    Function name   : h264bsdAllocateSwResources
    Description     :
    Return type     : u32
    Argument        : const void *dwl
    Argument        : storage_t * pStorage
    Argument        : u32 isHighSupported
------------------------------------------------------------------------------*/
u32 h264bsdAllocateSwResources(storage_t * pStorage, u32 isHighSupported, u32 ts_en)
{
    u32 tmp;
    const seqParamSet_t *pSps = pStorage->activeSps;

    pStorage->picSizeInMbs = pSps->picWidthInMbs * pSps->picHeightInMbs;

    /* note that calling ResetDpb here results in losing all
     * pictures currently in DPB -> nothing will be output from
     * the buffer even if noOutputOfPriorPicsFlag is HANTRO_FALSE */
    return h264bsdResetDpb(pStorage->dpb, pStorage->picSizeInMbs,
                          pSps->maxDpbSize, pSps->numRefFrames,
                          pSps->maxFrameNum,
                          pSps->monoChrome, isHighSupported, ts_en);
}
}

