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
--  Abstract : Top level control of the decoder
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264hwd_decoder.c,v $
--  $Date: 2010/05/14 10:45:43 $
--  $Revision: 1.27 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#define ALOG_TAG "h264dec"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "h264hwd_container.h"

#include "h264hwd_decoder.h"
#include "h264hwd_nal_unit.h"
#include "h264hwd_byte_stream.h"
#include "h264hwd_seq_param_set.h"
#include "h264hwd_pic_param_set.h"
#include "h264hwd_slice_header.h"
#include "h264hwd_util.h"
#include "h264hwd_dpb.h"
#include "h264decapi.h"

#define ALOGD	printf
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

/*------------------------------------------------------------------------------

    Function name: h264bsdInit

        Functional description:
            Initialize the decoder.

        Inputs:
            noOutputReordering  flag to indicate the decoder that it does not
                                have to perform reordering of display images.

        Outputs:
            pStorage            pointer to initialized storage structure

        Returns:
            none

------------------------------------------------------------------------------*/

void h264bsdInit(storage_t * pStorage)
{

/* Variables */

/* Code */

    ASSERT(pStorage);

    h264bsdInitStorage(pStorage);

    pStorage->dpb = pStorage->dpbs[0];

}

/*------------------------------------------------------------------------------

    Function: h264bsdDecodeVlc

        Functional description:
            Decode a NAL unit until a slice header. This function calls other modules to perform
            tasks like
                * extract and decode NAL unit from the byte stream
                * decode parameter sets
                * decode slice header and slice data
                * conceal errors in the picture
                * perform deblocking filtering

            This function contains top level control logic of the decoder.

        Inputs:
            pStorage        pointer to storage data structure
            byteStrm        pointer to stream buffer given by application
            len             length of the buffer in bytes
            picId           identifier for a picture, assigned by the
                            application

        Outputs:
            readBytes       number of bytes read from the stream is stored
                            here

        Returns:
            H264BSD_RDY             decoding finished, nothing special
            H264BSD_PIC_RDY         decoding of a picture finished
            H264BSD_HDRS_RDY        param sets activated, information like
                                    picture dimensions etc can be read
            H264BSD_ERROR           error in decoding
            H264BSD_PARAM_SET_ERROR serius error in decoding, failed to
                                    activate param sets

------------------------------------------------------------------------------*/
u32 h264bsdDecode(decContainer_t * pDecCont, const u8 * byteStrm, u32 len,
                  u32 picId, u32 * readBytes)
{

/* Variables */

    u32 tmp;
    u32 accessUnitBoundaryFlag = HANTRO_FALSE;
    storage_t *pStorage;
    nalUnit_t nalUnit;
    strmData_t strm;
    u32 ret = 0;

    DEBUG_PRINT(("h264bsdDecode\n"));

/* Code */
    ASSERT(pDecCont);
    ASSERT(byteStrm);
    ASSERT(len);
    ASSERT(readBytes);

    pStorage = &pDecCont->storage;
    ASSERT(pStorage);

    DEBUG_PRINT(("Valid slice in access unit %d\n", pStorage->validSliceInAccessUnit));

    pStorage->strm[0].removeEmul3Byte = 0;
    strm.removeEmul3Byte = 0;

    /* if previous buffer was not finished and same pointer given -> skip NAL
     * unit extraction */
    if(pStorage->prevBufNotFinished && byteStrm == pStorage->prevBufPointer)
    {
        strm = pStorage->strm[0];
        strm.pStrmCurrPos = strm.pStrmBuffStart;
        strm.strmBuffReadBits = strm.bitPosInWord = 0;
        *readBytes = pStorage->prevBytesConsumed;
    }
    else
    {
        tmp = h264bsdExtractNalUnit(byteStrm, len, &strm, readBytes);
        if(tmp != HANTRO_OK)
        {
            ERROR_PRINT("BYTE_STREAM");
            return (H264BSD_ERROR);
        }
        /* store stream */
        pStorage->strm[0] = strm;
        pStorage->prevBytesConsumed = *readBytes;
        pStorage->prevBufPointer = byteStrm;
    }

    pStorage->prevBufNotFinished = HANTRO_FALSE;

    tmp = h264bsdDecodeNalUnit(&strm, &nalUnit);
    if(tmp != HANTRO_OK)
    {
        ret = H264BSD_ERROR;
        goto NEXT_NAL;
    }

    if (pDecCont->disable_mvc && nalUnit.nalUnitType == NAL_SUBSET_SEQ_PARAM_SET) {
        return H264BSD_MEMFAIL;
    }

    /* Discard unspecified, reserved, SPS extension and auxiliary picture slices */
    if(nalUnit.nalUnitType == 0 ||
        (nalUnit.nalUnitType >= 13 &&
         (pStorage->mvc == 0 ||
          (nalUnit.nalUnitType != 14 &&
           nalUnit.nalUnitType != 15 &&
           nalUnit.nalUnitType != 20))))
    {
        DEBUG_PRINT(("DISCARDED NAL (UNSPECIFIED, REGISTERED, SPS ext or AUX slice)\n"));
        ret = H264BSD_RDY;
        goto NEXT_NAL;
    }

    if (nalUnit.nalRefIdc == 0)
    {
        if (NULL == pStorage->activeSps || NULL == pStorage->activePps)
        {
            ret = H264BSD_NONREF_PIC_SKIPPED;
            goto NEXT_NAL;
        }
    }

    if (!pStorage->checkedAub)
    {
        tmp = h264bsdCheckAccessUnitBoundary(&strm,
                                             &nalUnit,
                                             pStorage, &accessUnitBoundaryFlag);
        if(tmp != HANTRO_OK)
        {
            ERROR_PRINT("ACCESS UNIT BOUNDARY CHECK");
            if(tmp == PARAM_SET_ERROR)
                ret = (H264BSD_PARAM_SET_ERROR);
            else
                ret = (H264BSD_ERROR);
            goto NEXT_NAL;
        }
    }
    else
    {
        pStorage->checkedAub = 0;
    }

    if(accessUnitBoundaryFlag)
    {
        DEBUG_PRINT(("Access unit boundary, NAL TYPE %d\n", nalUnit.nalUnitType));

        /* conceal if picture started and param sets activated */
        if(0)//(pStorage->picStarted && pStorage->activeSps != NULL)
        {
            DEBUG_PRINT(("New access unit and previous not finished\n"));
            DEBUG_PRINT(("PICTURE FREEZE CONCEAL...\n"));

            if (!pStorage->validSliceInAccessUnit)
            {
                DEBUG_PRINT(("!validSliceunit\n"));
                //if (!pStorage->secondField)
                {
                    //ALOGE("h264bsdAllocateDpbImage in h264bsdDecode 0\n");
                    //if (NULL == h264bsdAllocateDpbImage(pStorage->dpb)) {
                    //    return H264BSD_MEMFAIL;
                    //}
                    //pStorage->sliceHeader->fieldPicFlag = 0;
                }
            }

            pStorage->skipRedundantSlices = HANTRO_FALSE;

            /* current NAL unit should be decoded on next activation -> set
             * readBytes to 0 */
            *readBytes = 0;
            pStorage->prevBufNotFinished = HANTRO_TRUE;
            DEBUG_PRINT(("...DONE\n"));

            return (H264BSD_NEW_ACCESS_UNIT);
        }
        else
        {
            DEBUG_PRINT(("vali slice false\n"));
            pStorage->validSliceInAccessUnit = HANTRO_FALSE;
        }

        pStorage->skipRedundantSlices = HANTRO_FALSE;
    }

    DEBUG_PRINT(("nal unit type: %d\n", nalUnit.nalUnitType));

    switch (nalUnit.nalUnitType)
    {
    case NAL_SEQ_PARAM_SET:
    case NAL_SUBSET_SEQ_PARAM_SET: {
        DEBUG_PRINT(("SEQ PARAM SET\n"));
        seqParamSet_t *seqParamSet = (seqParamSet_t *)malloc(sizeof(seqParamSet_t));
        if (seqParamSet) {
            tmp = h264bsdDecodeSeqParamSet(&strm, seqParamSet, nalUnit.nalUnitType == NAL_SEQ_PARAM_SET ? 0 : 1);
            if(tmp != HANTRO_OK) {
                ERROR_PRINT("SEQ_PARAM_SET decoding");
                ret = H264BSD_ERROR;
            } else {
                tmp = h264bsdStoreSeqParamSet(pStorage, seqParamSet);
                if (tmp != HANTRO_OK) {
                    ERROR_PRINT("SEQ_PARAM_SET allocation");
                    ret = H264BSD_ERROR;
                }
                if (nalUnit.nalUnitType == NAL_SUBSET_SEQ_PARAM_SET) {
                    pStorage->viewId[0] = seqParamSet->mvc.viewId[0];
                    pStorage->viewId[1] = seqParamSet->mvc.viewId[1];
                }
            }

            if (pDecCont && (!seqParamSet->frameMbsOnlyFlag)) {
                /*if (seqParamSet->mbAdaptiveFrameFieldFlag) {
                    pDecCont->ts_en = 1;
                } else {*/
                    pDecCont->ts_en = 3;
                //}
            }

            if (ret) {
                h264bsdFreeSeqParamSet(seqParamSet);
            }
        }

        ret = H264BSD_RDY;
        goto NEXT_NAL;
    }
    case NAL_PIC_PARAM_SET: {
        picParamSet_t picParamSet;
        DEBUG_PRINT(("PIC PARAM SET\n"));
        tmp = h264bsdDecodePicParamSet(&strm, &picParamSet);
        if(tmp != HANTRO_OK)
        {
            ERROR_PRINT("PIC_PARAM_SET decoding");
            ret = H264BSD_ERROR;
        }
        else
        {
            tmp = h264bsdStorePicParamSet(pStorage, &picParamSet);
            if(tmp != HANTRO_OK)
            {
                ERROR_PRINT("PIC_PARAM_SET allocation");
                ret = H264BSD_ERROR;
            }
        }
        ret = H264BSD_RDY;
        goto NEXT_NAL;
    }
    case NAL_CODED_SLICE_IDR:
        DEBUG_PRINT(("IDR "));
        /* fall through */
    case NAL_CODED_SLICE:
    case NAL_CODED_SLICE_EXT: {
        DEBUG_PRINT(("decode slice header\n"));

        if (nalUnit.nalUnitType == NAL_CODED_SLICE_EXT)
        {
            pStorage->view = 1;
            /* view_id not equal to view_id of the 1. non-phy_base view -> skip */
            if (nalUnit.viewId != pStorage->viewId[pStorage->view])
                goto NEXT_NAL;
        }
        else
            pStorage->view = 0;

        /* picture successfully finished and still decoding same old
         * access unit -> no need to decode redundant slices */
        if (pStorage->skipRedundantSlices)
        {
            DEBUG_PRINT(("skipping redundant slice\n"));
            ret = H264BSD_RDY;
            goto NEXT_NAL;
        }

        if (h264bsdIsStartOfPicture(pStorage))
        {
            u32 ppsId, spsId;
            pStorage->numConcealedMbs = 0;
            pStorage->currentPicId = picId;

            tmp = h264bsdCheckPpsId(&strm, &ppsId);
            ASSERT(tmp == HANTRO_OK);
            /* store old activeSpsId and return headers ready
             * indication if activeSps changes */
            spsId = pStorage->activeViewSpsId[pStorage->view];

            tmp = h264bsdActivateParamSets(pStorage, ppsId,
                                           IS_IDR_NAL_UNIT(&nalUnit) ?
                                           HANTRO_TRUE : HANTRO_FALSE);
            if(tmp != HANTRO_OK)
            {
                ERROR_PRINT("Param set activation");
                ret = H264BSD_PARAM_SET_ERROR;
                goto NEXT_NAL;
            }

            if (pStorage->activeSpsChanged) {
                h264bsdFlushDpb(pStorage->dpb);
                *readBytes = 0;
                pStorage->activeSpsChanged = 0;
                pStorage->picStarted = HANTRO_FALSE;
                return (H264BSD_HDRS_RDY);
            }

            if (spsId != pStorage->activeSpsId)
            {
                seqParamSet_t *oldSPS = NULL;
                seqParamSet_t *newSPS = pStorage->activeSps;
                u32 noOutputOfPriorPicsFlag = 1;

                if (pStorage->oldSpsId < MAX_NUM_SEQ_PARAM_SETS)
                {
                    oldSPS = pStorage->sps[pStorage->oldSpsId];
                }

                *readBytes = 0;
                pStorage->prevBufNotFinished = HANTRO_TRUE;

                if (IS_IDR_NAL_UNIT(&nalUnit))
                {
                    tmp =
                        h264bsdCheckPriorPicsFlag(&noOutputOfPriorPicsFlag,
                                                  &strm, newSPS,
                                                  pStorage->activePps,
                                                  NAL_CODED_SLICE_IDR);
                                                  /*nalUnit.nalUnitType);*/
                }
                else
                {
                    tmp = HANTRO_NOK;
                }

                if((tmp != HANTRO_OK) ||
                   (noOutputOfPriorPicsFlag != 0) ||
                   (oldSPS == NULL) ||
                   (oldSPS->picWidthInMbs != newSPS->picWidthInMbs) ||
                   (oldSPS->picHeightInMbs != newSPS->picHeightInMbs) ||
                   (oldSPS->maxDpbSize != newSPS->maxDpbSize))
                {
                    pStorage->dpb->flushed = 0;
                }
                else
                {
                    h264bsdFlushDpb(pStorage->dpb);
                }

                pStorage->oldSpsId = pStorage->activeSpsId;
                pStorage->picStarted = HANTRO_FALSE;
                return (H264BSD_HDRS_RDY);
            }

            if(pStorage->activePps->numSliceGroups != 1)
            {
                *readBytes = 0;
                return (H264BSD_FMO);
            }
        }

        tmp = h264bsdDecodeSliceHeader(&strm, pStorage->sliceHeader + 1,
                                       pStorage->activeSps,
                                       pStorage->activePps,
                                       &nalUnit,
                                       pStorage->dpb->numRefFrames);
        if(tmp != HANTRO_OK)
        {
            if (H264DEC_SYNC_STREAM != pDecCont->decStat) {
                ERROR_PRINT("SLICE_HEADER");
            }
            ret = H264BSD_ERROR;
            pDecCont->decStat = H264DEC_SYNC_STREAM;
			DPBDEBUG("SLICE_HEADER error %d !!\n", tmp);
            goto NEXT_NAL;
        }
        if (H264DEC_SYNC_STREAM == pDecCont->decStat) {
            ALOGD("found resync point\n");
        }
		DPBDEBUG("frameNum=%d, bottomFieldFlag=%d, slicetype=%d\n", pStorage->sliceHeader[1].frameNum, 
			pStorage->sliceHeader[1].bottomFieldFlag, pStorage->sliceHeader[1].sliceType);
		
        pDecCont->decStat = H264DEC_INITIALIZED;

        pStorage->picStarted = HANTRO_TRUE;

        if (h264bsdIsStartOfPicture(pStorage))
        {
            sliceHeader_t *pSliceCurr = pStorage->sliceHeader + 1;
            sliceHeader_t *pSlicePrev = pStorage->sliceHeader + 0;
            u32 baseOppositeFieldPic;
            //tmp = pStorage->secondField;
            baseOppositeFieldPic = h264bsdIsOppositeFieldPic(pSliceCurr, pSlicePrev,
                                            NULL/*&pStorage->secondField*/, pStorage->dpb->prevRefFrameNum, pStorage->aub->newPicture);

            if (baseOppositeFieldPic && pStorage->dpb->previousOut)
            {
                DPBDEBUG("path 0\n");
                pStorage->dpb->currentOut  = pStorage->dpb->previousOut;
                pStorage->dpb->previousOut = NULL;
				pStorage->dpb->fieldmark |= 2;
            }
            else
            {
                u32 picStructCurr = (pSliceCurr->fieldPicFlag == 0) ? (u32)(FRAME) : (pSliceCurr->bottomFieldFlag);
                u32 picStructPrev = (pSlicePrev->fieldPicFlag == 0) ? (u32)(FRAME) : (pSlicePrev->bottomFieldFlag);
                u32 err = 0, needCheck = 0/*, matchField = 1*/;
				i32 diff = 0;

				if((pSliceCurr->sliceType == (B_SLICE+5)) || (pSliceCurr->sliceType == B_SLICE))				
				{
					dpbStorage_t *dpb = pStorage->dpb;	//b 场时若只有一帧,则认为出错;
					dpbPicture_t **buffer = dpb->buffer;
					u32 slicecnt = 0, index = 0;
					
					for (u32 i = 0; i < dpb->dpbSize; i++)
					{
						if (buffer[i] && buffer[i]->data)
						{
							slicecnt++;
							index = i;
						}
					}

					if((slicecnt == 1) && pDecCont->ts_en)
					{
						if(buffer[index]->isIdr == 0)	//not skip b slice when idr
						{
							pStorage->dpb->fieldmark = 0;
							pStorage->sliceHeader[0] = pStorage->sliceHeader[1];
							DPBOUTDEBUG("lost one p slice, skip b slice.");
							return (H264BSD_UNPAIRED_FIELD);
						}
					}
				}
				
				if((pStorage->dpb->fieldmark == 1) && pSliceCurr->fieldPicFlag && pSlicePrev->fieldPicFlag)
					DPBOUTDEBUG("it may be lost one field!");

                switch (picStructCurr) {
	                case FRAME : {
	                    if (picStructPrev == TOPFIELD) {
	                        DPBOUTDEBUG("current frame: missing previous bottom field");
	                        markErrorDpbSlot(pStorage->dpb);
	                    }
						pStorage->dpb->fieldmark = 3;
	                } break;
	                case TOPFIELD : {
	                    if ((picStructPrev == TOPFIELD) && ((pStorage->dpb->numRefFrames != 0))){// || ((pSliceCurr->sliceType != I_SLICE) && (pSliceCurr->sliceType != (I_SLICE+5))))) {
	                        DPBOUTDEBUG("current top field: missing previous bottom field");
	                        markErrorDpbSlot(pStorage->dpb);
	                        needCheck   = 1;
	                    }
						
						if(!needCheck && pStorage->dpb->fieldmark == 1)	//clear only one field, I slice 在errorconceal会被处理
						{
							markErrorDpbSlot(pStorage->dpb);
                					Mmcop5(pStorage->dpb, 0);
                					pStorage->dpb->fieldmark = 0;
                					pStorage->sliceHeader[0] = pStorage->sliceHeader[1];
                					return (H264BSD_UNPAIRED_FIELD);                        
						}
						pStorage->dpb->fieldmark = 1;
	                } break;
	                case BOTFIELD : {
	                    if (picStructPrev == BOTFIELD) {
	                        DPBOUTDEBUG("current bottom field: missing current top field or mismatch top");
	                        markErrorDpbSlot(pStorage->dpb);
	                        needCheck   = 1;
	                        pStorage->dpb->fieldmark = 0;
							break;	//这里是丢掉当前的bottom field
	                    }
						
						if(!needCheck && pStorage->dpb->fieldmark == 1)	//clear only one field, I slice 在errorconceal会被处理
						{
							markErrorDpbSlot(pStorage->dpb);
    					Mmcop5(pStorage->dpb, 0);
					pStorage->dpb->fieldmark = 0;
					pStorage->sliceHeader[0] = pStorage->sliceHeader[1];
					return (H264BSD_UNPAIRED_FIELD);
						}
						pStorage->dpb->fieldmark = 1;
                } break;
                default : {
                    err = 1;
                } break;
                }
                if (needCheck && pSliceCurr->frameNum != pSlicePrev->frameNum && pSliceCurr->frameNum != pStorage->dpb->prevRefFrameNum)
                {
                    DPBOUTDEBUG("mismatch frameNum %d and %d\n", pSliceCurr->frameNum, pSlicePrev->frameNum);
                    //if (matchField) {
                        pStorage->sliceHeader[0] = pStorage->sliceHeader[1];
                    //}
                    pStorage->dpb->prevRefFrameNum = 0;
                    err |= 2;
                }
                //ALOGE("path 1 err %d\n", err);
                //if (pStorage->numViews == 0 ?
                //    tmp : (pStorage->sliceHeader->fieldPicFlag &&
                //    pStorage->view == 0))
                if (err) {
                    //pStorage->secondField = 0;
                    //ALOGE("Second field missing...");
                    *readBytes = 0;
                    //pStorage->prevBufNotFinished = HANTRO_TRUE;
                    //pStorage->checkedAub = 1;
                    //ALOGE("found unpaired field\n");
                    Mmcop5(pStorage->dpb, 0);
					pStorage->dpb->fieldmark = 0;
                    return (H264BSD_UNPAIRED_FIELD);
                }

				diff = (i32)pSliceCurr->frameNum - (i32)pSlicePrev->frameNum;
				if (((((diff == 2) || (diff == -2))&&(pDecCont->ts_en==3))||((diff > 2) || (diff < -2))) && ((pSliceCurr->sliceType != (I_SLICE+5)) && (pSliceCurr->sliceType != I_SLICE))
					&& (pSliceCurr->frameNum>1) && pDecCont->ts_en)
                {
                    DPBOUTDEBUG("discard slice mismatch frameNum cur %d and prev %d, do mmco5!\n", pSliceCurr->frameNum, pSlicePrev->frameNum);

					markErrorDpbSlot(pStorage->dpb);
					Mmcop5(pStorage->dpb, 0);
					pStorage->dpb->fieldmark = 0;
					pStorage->sliceHeader[0] = pStorage->sliceHeader[1];
					return (H264BSD_UNPAIRED_FIELD);
                }
				
				if(((pSliceCurr->sliceType == (I_SLICE+5)) || (pSliceCurr->sliceType == I_SLICE))
							&& (!IS_IDR_NAL_UNIT(&nalUnit)) && ((diff > 2) || (diff < -2)) && (pDecCont->ts_en==3))
				{
					DPBOUTDEBUG("lost slice before I slice cur %d and prev %d, do mmco5!\n", pSliceCurr->frameNum, pSlicePrev->frameNum);

					markErrorDpbSlot(pStorage->dpb);
					Mmcop5(pStorage->dpb, 0);
				}
				
                if (!IS_IDR_NAL_UNIT(&nalUnit) && !pDecCont->gapsCheckedForThis)
                {
                    tmp = h264bsdCheckGapsInFrameNum(pStorage, pStorage->dpb,
                                                     pStorage->sliceHeader[1].frameNum,
                                                     nalUnit.nalRefIdc !=
                                                     0 ? HANTRO_TRUE :
                                                     HANTRO_FALSE,
                                                     /*pStorage->activeSps->gapsInFrameNumValueAllowedFlag*/0);

                    pDecCont->gapsCheckedForThis = HANTRO_TRUE;
                    if(tmp != HANTRO_OK)
                    {
                        pDecCont->gapsCheckedForThis = HANTRO_FALSE;
                        ERROR_PRINT("Gaps in frame num");
                        ret = H264BSD_ERROR;
                        goto NEXT_NAL;
                    }
                }

                pStorage->dpb->currentOut = NULL;
                if (NULL == h264bsdAllocateDpbImage(pStorage, pStorage->dpb)) {
                    return H264BSD_MEMFAIL;
                }
            }
        }
        else
        {
            if(pStorage->sliceHeader[1].redundantPicCnt != 0)
            {
                ret = H264BSD_RDY;
                goto NEXT_NAL;
            }
        }

        DEBUG_PRINT(("vali slice TRUE\n"));

        /* store slice header to storage if successfully decoded */
        pStorage->sliceHeader[0] = pStorage->sliceHeader[1];
        pStorage->validSliceInAccessUnit = HANTRO_TRUE;
        pStorage->prevNalUnit[0] = nalUnit;

        if(IS_B_SLICE(pStorage->sliceHeader[1].sliceType))
        {
            if((pDecCont->h264ProfileSupport == H264_BASELINE_PROFILE))
            {
                ERROR_PRINT("B_SLICE not allowed in baseline decoder");
                ret = H264BSD_ERROR;
                goto NEXT_NAL;
            }

            if(pDecCont->asicBuff->enableDmvAndPoc == 0)
            {
                DEBUG_PRINT(("B_SLICE in baseline stream!!! DMV and POC writing were not enabled!"));
                DEBUG_PRINT(("B_SLICE decoding will not be accurate for a while!"));

                /* enable DMV and POC writing */
                pDecCont->asicBuff->enableDmvAndPoc = 1;
            }
        }

        /* For VLC mode, end SW decode here */
        DEBUG_PRINT(("\tVLC mode! Skip slice data decoding\n"));
        SetPicNums(pStorage->dpb, pStorage->sliceHeader->frameNum);

        return (H264BSD_PIC_RDY);

        } break;

    case NAL_SEI:
        DEBUG_PRINT(("SEI MESSAGE, NOT DECODED\n"));
        ret = H264BSD_RDY;
        goto NEXT_NAL;
    case NAL_END_OF_SEQUENCE:
        DEBUG_PRINT(("END_OF_SEQUENCE, NOT DECODED\n"));
        ret = H264BSD_RDY;
        goto NEXT_NAL;
    case NAL_END_OF_STREAM:
        DEBUG_PRINT(("END_OF_STREAM, NOT DECODED\n"));
        ret = H264BSD_RDY;
        goto NEXT_NAL;
    case NAL_PREFIX:
        pStorage->view = 0;
        pStorage->interViewRef = nalUnit.interViewFlag;
        goto NEXT_NAL;
    default:
        DPBDEBUG("NOT IMPLEMENTED YET %d\n", nalUnit.nalUnitType);
        ret = H264BSD_RDY;
        goto NEXT_NAL;
    }

    return (H264BSD_RDY);

NEXT_NAL:
    {
        const u8 *next =
            h264bsdFindNextStartCode(strm.pStrmBuffStart, strm.strmBuffSize);

        if(next != NULL)
        {
            *readBytes = (u32) (next - byteStrm);
            pStorage->prevBytesConsumed = *readBytes;
        }
    }

    return ret;

}

/*------------------------------------------------------------------------------

    Function: h264bsdPicWidth

        Functional description:
            Get width of the picture in macroblocks

        Inputs:
            pStorage    pointer to storage data structure

        Outputs:
            none

        Returns:
            picture width
            0 if parameters sets not yet activated

------------------------------------------------------------------------------*/

u32 h264bsdPicWidth(storage_t * pStorage)
{

/* Variables */

/* Code */

    ASSERT(pStorage);

    if(pStorage->activeSps)
        return (pStorage->activeSps->picWidthInMbs);
    else
        return (0);

}

/*------------------------------------------------------------------------------

    Function: h264bsdPicHeight

        Functional description:
            Get height of the picture in macroblocks

        Inputs:
            pStorage    pointer to storage data structure

        Outputs:
            none

        Returns:
            picture width
            0 if parameters sets not yet activated

------------------------------------------------------------------------------*/

u32 h264bsdPicHeight(storage_t * pStorage)
{

/* Variables */

/* Code */

    ASSERT(pStorage);

    if(pStorage->activeSps)
        return (pStorage->activeSps->picHeightInMbs);
    else
        return (0);

}

/*------------------------------------------------------------------------------

    Function: h264bsdIsMonoChrome

        Functional description:

        Inputs:
            pStorage    pointer to storage data structure

        Outputs:

        Returns:

------------------------------------------------------------------------------*/

u32 h264bsdIsMonoChrome(storage_t * pStorage)
{

/* Variables */

/* Code */

    ASSERT(pStorage);

    if(pStorage->activeSps)
        return (pStorage->activeSps->monoChrome);
    else
        return (0);

}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckValidParamSets

        Functional description:
            Check if any valid parameter set combinations (SPS/PPS) exists.

        Inputs:
            pStorage    pointer to storage structure

        Returns:
            1       at least one valid SPS/PPS combination found
            0       no valid param set combinations found

------------------------------------------------------------------------------*/

u32 h264bsdCheckValidParamSets(storage_t * pStorage)
{

/* Variables */

/* Code */

    ASSERT(pStorage);

    return (h264bsdValidParamSets(pStorage) == HANTRO_OK ? 1 : 0);

}

/*------------------------------------------------------------------------------

    Function: h264bsdAspectRatioIdc

        Functional description:
            Get value of aspect_ratio_idc received in the VUI data

        Inputs:
            pStorage    pointer to storage structure

        Outputs:
            value of aspect_ratio_idc if received
            0   otherwise (this is the default value)

------------------------------------------------------------------------------*/
u32 h264bsdAspectRatioIdc(const storage_t * pStorage)
{
/* Variables */
    const seqParamSet_t *sps;

/* Code */

    ASSERT(pStorage);
    sps = pStorage->activeSps;

    if(sps && sps->vuiParametersPresentFlag &&
       sps->vuiParameters->aspectRatioPresentFlag)
        return (sps->vuiParameters->aspectRatioIdc);
    else    /* default unspecified */
        return (0);

}

/*------------------------------------------------------------------------------

    Function: h264bsdSarSize

        Functional description:
            Get value of sample_aspect_ratio size received in the VUI data

        Inputs:
            pStorage    pointer to storage structure

        Outputs:
            values of sample_aspect_ratio size if received
            0   otherwise (this is the default value)

------------------------------------------------------------------------------*/
void h264bsdSarSize(const storage_t * pStorage, u32 * sar_width,
                    u32 * sar_height)
{
/* Variables */
    const seqParamSet_t *sps;

/* Code */

    ASSERT(pStorage);
    sps = pStorage->activeSps;

    if(sps && pStorage->activeSps->vuiParametersPresentFlag &&
       sps->vuiParameters->aspectRatioPresentFlag &&
       sps->vuiParameters->aspectRatioIdc == 255)
    {
        *sar_width = sps->vuiParameters->sarWidth;
        *sar_height = sps->vuiParameters->sarHeight;
    }
    else
    {
        *sar_width = 0;
        *sar_height = 0;
    }

}

/*------------------------------------------------------------------------------

    Function: h264bsdVideoRange

        Functional description:
            Get value of video_full_range_flag received in the VUI data.

        Inputs:
            pStorage    pointer to storage structure

        Returns:
            1   video_full_range_flag received and value is 1
            0   otherwise

------------------------------------------------------------------------------*/

u32 h264bsdVideoRange(storage_t * pStorage)
{
/* Variables */
    const seqParamSet_t *sps;

/* Code */

    ASSERT(pStorage);
    sps = pStorage->activeSps;

    if(sps && sps->vuiParametersPresentFlag &&
       sps->vuiParameters->videoSignalTypePresentFlag &&
       sps->vuiParameters->videoFullRangeFlag)
        return (1);
    else    /* default value of video_full_range_flag is 0 */
        return (0);

}

/*------------------------------------------------------------------------------

    Function: h264bsdMatrixCoefficients

        Functional description:
            Get value of matrix_coefficients received in the VUI data

        Inputs:
            pStorage    pointer to storage structure

        Outputs:
            value of matrix_coefficients if received
            2   otherwise (this is the default value)

------------------------------------------------------------------------------*/

u32 h264bsdMatrixCoefficients(storage_t * pStorage)
{
/* Variables */
    const seqParamSet_t *sps;

/* Code */

    ASSERT(pStorage);
    sps = pStorage->activeSps;

    if(sps && sps->vuiParametersPresentFlag &&
       sps->vuiParameters->videoSignalTypePresentFlag &&
       sps->vuiParameters->colourDescriptionPresentFlag)
        return (sps->vuiParameters->matrixCoefficients);
    else    /* default unspecified */
        return (2);

}

/*------------------------------------------------------------------------------

    Function: hh264bsdCroppingParams

        Functional description:
            Get cropping parameters of the active SPS

        Inputs:
            pStorage    pointer to storage structure

        Outputs:
            croppingFlag    flag indicating if cropping params present is
                            stored here
            leftOffset      cropping left offset in pixels is stored here
            width           width of the image after cropping is stored here
            topOffset       cropping top offset in pixels is stored here
            height          height of the image after cropping is stored here

        Returns:
            none

------------------------------------------------------------------------------*/

void h264bsdCroppingParams(storage_t * pStorage, u32 * croppingFlag,
                           u32 * leftOffset, u32 * width, u32 * topOffset,
                           u32 * height)
{
/* Variables */
    const seqParamSet_t *sps;
    u32 tmp1, tmp2;

/* Code */

    ASSERT(pStorage);
    sps = pStorage->activeSps;

    if(sps && sps->frameCroppingFlag)
    {
        tmp1 = sps->monoChrome ? 1 : 2;
        tmp2 = sps->frameMbsOnlyFlag ? 1 : 2;
        *croppingFlag = 1;
        *leftOffset = tmp1 * sps->frameCropLeftOffset;
        *width = 16 * sps->picWidthInMbs -
            tmp1 * (sps->frameCropLeftOffset + sps->frameCropRightOffset);

        *topOffset = tmp1 * tmp2 * sps->frameCropTopOffset;
        *height = 16 * sps->picHeightInMbs -
            tmp1 * tmp2 * (sps->frameCropTopOffset +
                           sps->frameCropBottomOffset);
    }
    else
    {
        *croppingFlag = 0;
        *leftOffset = 0;
        *width = 0;
        *topOffset = 0;
        *height = 0;
    }

}

}

