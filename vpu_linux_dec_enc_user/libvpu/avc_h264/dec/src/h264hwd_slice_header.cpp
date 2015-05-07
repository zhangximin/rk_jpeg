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
--  Abstract : Decode slice header information from the stream
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264hwd_slice_header.c,v $
--  $Date: 2010/04/22 06:54:56 $
--  $Revision: 1.8 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of contents

     1. Include headers
     2. External compiler flags
     3. Module defines
     4. Local function prototypes
     5. Functions
          h264bsdDecodeSliceHeader
          NumSliceGroupChangeCycleBits
          RefPicListReordering
          DecRefPicMarking
          CheckPpsId
          CheckFrameNum
          CheckIdrPicId
          CheckPicOrderCntLsb
          CheckDeltaPicOrderCntBottom
          CheckDeltaPicOrderCnt
          CheckRedundantPicCnt

------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "h264hwd_slice_header.h"
#include "h264hwd_util.h"
#include "h264hwd_vlc.h"
#include "h264hwd_nal_unit.h"
#include "h264hwd_dpb.h"
#include "dwl.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

static u32 RefPicListReordering(strmData_t *, refPicListReordering_t *,
                                u32, u32, u32);

static u32 PredWeightTable(strmData_t *, sliceHeader_t *, u32 monoChrome);

static u32 NumSliceGroupChangeCycleBits(u32 picSizeInMbs,
                                        u32 sliceGroupChangeRate);

static u32 DecRefPicMarking(strmData_t * pStrmData,
                            decRefPicMarking_t * pDecRefPicMarking,
                            u32 idrPicFlag, u32 numRefFrames);

/*------------------------------------------------------------------------------

    Function name: h264bsdDecodeSliceHeader

        Functional description:
            Decode slice header data from the stream.

        Inputs:
            pStrmData       pointer to stream data structure
            pSeqParamSet    pointer to active sequence parameter set
            pPicParamSet    pointer to active picture parameter set
            pNalUnit        pointer to current NAL unit structure

        Outputs:
            pSliceHeader    decoded data is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data or end of stream

------------------------------------------------------------------------------*/

u32 h264bsdDecodeSliceHeader(strmData_t * pStrmData,
                             sliceHeader_t * pSliceHeader,
                             seqParamSet_t * pSeqParamSet,
                             picParamSet_t * pPicParamSet,
                             nalUnit_t * pNalUnit,
                             u32 numRefFrames)
{

/* Variables */

    u32 tmp, i, value;
    i32 itmp;
    u32 picSizeInMbs;
    u32 strmPos;

/* Code */

    ASSERT(pStrmData);
    ASSERT(pSliceHeader);
    ASSERT(pSeqParamSet);
    ASSERT(pPicParamSet);
    ASSERT(pNalUnit->nalUnitType == NAL_CODED_SLICE ||
           pNalUnit->nalUnitType == NAL_CODED_SLICE_IDR ||
           pNalUnit->nalUnitType == NAL_CODED_SLICE_EXT);

    memset(pSliceHeader, 0, sizeof(sliceHeader_t));

    picSizeInMbs = pSeqParamSet->picWidthInMbs * pSeqParamSet->picHeightInMbs;
    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);
    pSliceHeader->firstMbInSlice = value;
    if(value >= picSizeInMbs)
    {
        //ERROR_PRINT("first_mb_in_slice");
        return (HANTRO_NOK);
    }

    DPBDEBUG("\tfirst_mb_in_slice %4d\n", value);

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
	DPBDEBUG("sliceType %d, %d\n", tmp, value);
    if(tmp != HANTRO_OK)
        return (tmp);
    pSliceHeader->sliceType = value;
    /* slice type has to be either I, P or B slice. Only I slice is
     * allowed when current NAL unit is an IDR NAL unit or num_ref_frames
     * is 0 */
    if (!(IS_I_SLICE(pSliceHeader->sliceType) ||
        ((IS_P_SLICE(pSliceHeader->sliceType) ||
          IS_B_SLICE(pSliceHeader->sliceType)) &&
          pNalUnit->nalUnitType != NAL_CODED_SLICE_IDR &&
          pSeqParamSet->numRefFrames &&
          numRefFrames)))
    {
        DPBDEBUG("numRefFrames %d, %d\n", pSeqParamSet->numRefFrames, numRefFrames);
        return (HANTRO_NOK);
    }

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);
    pSliceHeader->picParameterSetId = value;
    if(pSliceHeader->picParameterSetId != pPicParamSet->picParameterSetId)
    {
        ERROR_PRINT("pic_parameter_set_id");
        return (HANTRO_NOK);
    }

    /* log2(maxFrameNum) -> num bits to represent frame_num */
    i = 0;
    while(pSeqParamSet->maxFrameNum >> i)
        i++;
    i--;

    tmp = h264bsdGetBits(pStrmData, i);
    if(tmp == END_OF_STREAM)
        return (HANTRO_NOK);
    /* removed for XXXXXXXX compliance */
    /*
    if(IS_IDR_NAL_UNIT(pNalUnit) && tmp != 0)
    {
        ERROR_PRINT("frame_num");
        return (HANTRO_NOK);
    }
    */
    pSliceHeader->frameNum = tmp;

    if (!pSeqParamSet->frameMbsOnlyFlag)
    {
        tmp = h264bsdGetBits(pStrmData, 1);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);
        pSliceHeader->fieldPicFlag = tmp;
        if (pSliceHeader->fieldPicFlag)
        {
            tmp = h264bsdGetBits(pStrmData, 1);
            if(tmp == END_OF_STREAM)
                return (HANTRO_NOK);
            pSliceHeader->bottomFieldFlag = tmp;
        }

    }

    if(IS_IDR_NAL_UNIT(pNalUnit))
    {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if(tmp != HANTRO_OK)
            return (tmp);
        pSliceHeader->idrPicId = value;
        if(value > 65535)
        {
            ERROR_PRINT("idr_pic_id");
            return (HANTRO_NOK);
        }
    }

    strmPos = pStrmData->strmBuffReadBits;
    pStrmData->emulByteCount = 0;
    if(pSeqParamSet->picOrderCntType == 0)
    {
        /* log2(maxPicOrderCntLsb) -> num bits to represent pic_order_cnt_lsb */
        i = 0;
        while(pSeqParamSet->maxPicOrderCntLsb >> i)
            i++;
        i--;

        tmp = h264bsdGetBits(pStrmData, i);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);
        pSliceHeader->picOrderCntLsb = tmp;

        if(pPicParamSet->picOrderPresentFlag && !pSliceHeader->fieldPicFlag)
        {
            tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
            if(tmp != HANTRO_OK)
                return (tmp);
            pSliceHeader->deltaPicOrderCntBottom = itmp;
        }

        /* check that picOrderCnt for IDR picture will be zero. See
         * DecodePicOrderCnt function to understand the logic here */
        /* removed for XXXXXXXX compliance */
        /*
        if(IS_IDR_NAL_UNIT(pNalUnit) &&
           ((pSliceHeader->picOrderCntLsb >
             pSeqParamSet->maxPicOrderCntLsb / 2) ||
            MIN((i32) pSliceHeader->picOrderCntLsb,
                (i32) pSliceHeader->picOrderCntLsb +
                pSliceHeader->deltaPicOrderCntBottom) != 0))
        {
            return (HANTRO_NOK);
        }
        */
    }

    if((pSeqParamSet->picOrderCntType == 1) &&
       !pSeqParamSet->deltaPicOrderAlwaysZeroFlag)
    {
        tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
        if(tmp != HANTRO_OK)
            return (tmp);
        pSliceHeader->deltaPicOrderCnt[0] = itmp;

        if(pPicParamSet->picOrderPresentFlag && !pSliceHeader->fieldPicFlag)
        {
            tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
            if(tmp != HANTRO_OK)
                return (tmp);
            pSliceHeader->deltaPicOrderCnt[1] = itmp;
        }

        /* check that picOrderCnt for IDR picture will be zero. See
         * DecodePicOrderCnt function to understand the logic here */
        if(IS_IDR_NAL_UNIT(pNalUnit) &&
            (pSliceHeader->fieldPicFlag ?
                pSliceHeader->bottomFieldFlag ?
                    pSliceHeader->deltaPicOrderCnt[0] +
                    pSeqParamSet->offsetForTopToBottomField +
                    pSliceHeader->deltaPicOrderCnt[1] != 0 :
                    pSliceHeader->deltaPicOrderCnt[0] != 0 :
               MIN(pSliceHeader->deltaPicOrderCnt[0],
                   pSliceHeader->deltaPicOrderCnt[0] +
                   pSeqParamSet->offsetForTopToBottomField +
                   pSliceHeader->deltaPicOrderCnt[1]) != 0))
        {
            return (HANTRO_NOK);
        }
    }

    pSliceHeader->pocLength = pStrmData->strmBuffReadBits - strmPos;
    pSliceHeader->pocLengthHw = pStrmData->strmBuffReadBits - strmPos -
        8*pStrmData->emulByteCount;
    if(pPicParamSet->redundantPicCntPresentFlag)
    {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if(tmp != HANTRO_OK)
            return (tmp);
        pSliceHeader->redundantPicCnt = value;
        if(value > 127)
        {
            ERROR_PRINT("redundant_pic_cnt");
            return (HANTRO_NOK);
        }

        /* don't decode redundant slices */
        if(value != 0)
        {
            DEBUG_PRINT(("h264bsdDecodeSliceHeader: REDUNDANT PICTURE SLICE\n"));
            /* return(HANTRO_NOK); */
        }
    }

    if (IS_B_SLICE(pSliceHeader->sliceType))
    {
        tmp = h264bsdGetBits(pStrmData, 1);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);
        pSliceHeader->directSpatialMvPredFlag = tmp;
    }
    if (IS_P_SLICE(pSliceHeader->sliceType) ||
        IS_B_SLICE(pSliceHeader->sliceType))
    {
        tmp = h264bsdGetBits(pStrmData, 1);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);
        pSliceHeader->numRefIdxActiveOverrideFlag = tmp;

        if(pSliceHeader->numRefIdxActiveOverrideFlag)
        {
            tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
            if(tmp != HANTRO_OK)
                return (tmp);
            if (value > 31 ||
                (pSliceHeader->fieldPicFlag == 0 && value > 15))
            {
                ERROR_PRINT("num_ref_idx_l0_active_minus1");
                return (HANTRO_NOK);
            }
            pSliceHeader->numRefIdxL0Active = value + 1;

            if (IS_B_SLICE(pSliceHeader->sliceType))
            {
                tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
                if(tmp != HANTRO_OK)
                    return (tmp);
                if (value > 31 ||
                    (pSliceHeader->fieldPicFlag == 0 && value > 15))
                {
                    ERROR_PRINT("num_ref_idx_l1_active_minus1");
                    return (HANTRO_NOK);
                }
                pSliceHeader->numRefIdxL1Active = value + 1;
            }
        }
        /* set numRefIdxL0Active from pic param set */
        else
        {
            /* if value (minus1) in picture parameter set exceeds 15 and
             * current picture is not field picture, it should have been
             * overridden here */
            if( (pPicParamSet->numRefIdxL0Active > 16 &&
                 !pSliceHeader->fieldPicFlag) ||
                (IS_B_SLICE(pSliceHeader->sliceType) &&
                 pPicParamSet->numRefIdxL1Active > 16 &&
                 !pSliceHeader->fieldPicFlag) )
            {
                ERROR_PRINT("num_ref_idx_active_override_flag");
                return (HANTRO_NOK);
            }
            pSliceHeader->numRefIdxL0Active = pPicParamSet->numRefIdxL0Active;
            pSliceHeader->numRefIdxL1Active = pPicParamSet->numRefIdxL1Active;
        }
    }

    if(!IS_I_SLICE(pSliceHeader->sliceType))
    {
        tmp = RefPicListReordering(pStrmData,
                                   &pSliceHeader->refPicListReordering,
                                   pSliceHeader->numRefIdxL0Active,
                                   pSliceHeader->fieldPicFlag ?
                                        2*pSeqParamSet->maxFrameNum :
                                          pSeqParamSet->maxFrameNum,
                                   pNalUnit->nalUnitType ==
                                        NAL_CODED_SLICE_EXT );
        if(tmp != HANTRO_OK)
            return (tmp);
    }
    if (IS_B_SLICE(pSliceHeader->sliceType))
    {
        tmp = RefPicListReordering(pStrmData,
                                   &pSliceHeader->refPicListReorderingL1,
                                   pSliceHeader->numRefIdxL1Active,
                                   pSliceHeader->fieldPicFlag ?
                                        2*pSeqParamSet->maxFrameNum :
                                          pSeqParamSet->maxFrameNum,
                                   pNalUnit->nalUnitType ==
                                        NAL_CODED_SLICE_EXT );
        if(tmp != HANTRO_OK)
            return (tmp);
    }

    if ( (pPicParamSet->weightedPredFlag &&
          IS_P_SLICE(pSliceHeader->sliceType)) ||
         (pPicParamSet->weightedBiPredIdc == 1 &&
          IS_B_SLICE(pSliceHeader->sliceType)) )
    {
        tmp = PredWeightTable(pStrmData, pSliceHeader, pSeqParamSet->monoChrome);
        if (tmp != HANTRO_OK)
            return (tmp);
    }

    if(pNalUnit->nalRefIdc != 0)
    {
        tmp = DecRefPicMarking(pStrmData, &pSliceHeader->decRefPicMarking,
                               IS_IDR_NAL_UNIT(pNalUnit),
                               pSeqParamSet->numRefFrames);
        if(tmp != HANTRO_OK)
            return (tmp);
    }
    else
        pSliceHeader->decRefPicMarking.strmLen = 0;

    if(pPicParamSet->entropyCodingModeFlag &&
       !IS_I_SLICE(pSliceHeader->sliceType))
    {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if(value > 2 || tmp != HANTRO_OK)
        {
            ERROR_PRINT("cabac_init_idc");
            return (HANTRO_NOK);
        }
        pSliceHeader->cabacInitIdc = value;
    }

    /* decode sliceQpDelta and check that initial QP for the slice will be on
     * the range [0, 51] */
    tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
    if(tmp != HANTRO_OK)
        return (tmp);
    pSliceHeader->sliceQpDelta = itmp;
    itmp += (i32) pPicParamSet->picInitQp;
    if((itmp < 0) || (itmp > 51))
    {
        ERROR_PRINT("slice_qp_delta");
        return (HANTRO_NOK);
    }

    if(pPicParamSet->deblockingFilterControlPresentFlag)
    {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if(tmp != HANTRO_OK)
            return (tmp);
        pSliceHeader->disableDeblockingFilterIdc = value;
        if(pSliceHeader->disableDeblockingFilterIdc > 2)
        {
            ERROR_PRINT("disable_deblocking_filter_idc");
            return (HANTRO_NOK);
        }

        if(pSliceHeader->disableDeblockingFilterIdc != 1)
        {
            tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
            if(tmp != HANTRO_OK)
                return (tmp);
            if((itmp < -6) || (itmp > 6))
            {
                ERROR_PRINT("slice_alpha_c0_offset_div2");
                return (HANTRO_NOK);
            }
            pSliceHeader->sliceAlphaC0Offset = itmp /* * 2 */ ;

            tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
            if(tmp != HANTRO_OK)
                return (tmp);
            if((itmp < -6) || (itmp > 6))
            {
                ERROR_PRINT("slice_beta_offset_div2");
                return (HANTRO_NOK);
            }
            pSliceHeader->sliceBetaOffset = itmp /* * 2 */ ;
        }
    }

    if((pPicParamSet->numSliceGroups > 1) &&
       (pPicParamSet->sliceGroupMapType >= 3) &&
       (pPicParamSet->sliceGroupMapType <= 5))
    {
        /* set tmp to number of bits used to represent slice_group_change_cycle
         * in the stream */
        tmp = NumSliceGroupChangeCycleBits(picSizeInMbs,
                                           pPicParamSet->sliceGroupChangeRate);
        value = h264bsdGetBits(pStrmData, tmp);
        if(value == END_OF_STREAM)
            return (HANTRO_NOK);
        pSliceHeader->sliceGroupChangeCycle = value;

        /* corresponds to tmp = Ceil(picSizeInMbs / sliceGroupChangeRate) */
        tmp = (picSizeInMbs + pPicParamSet->sliceGroupChangeRate - 1) /
            pPicParamSet->sliceGroupChangeRate;
        if(pSliceHeader->sliceGroupChangeCycle > tmp)
        {
            ERROR_PRINT("slice_group_change_cycle");
            return (HANTRO_NOK);
        }
    }

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: NumSliceGroupChangeCycleBits

        Functional description:
            Determine number of bits needed to represent
            slice_group_change_cycle in the stream. The standard states that
            slice_group_change_cycle is represented by
                Ceil( Log2( (picSizeInMbs / sliceGroupChangeRate) + 1) )

            bits. Division "/" in the equation is non-truncating division.

        Inputs:
            picSizeInMbs            picture size in macroblocks
            sliceGroupChangeRate

        Outputs:
            none

        Returns:
            number of bits needed

------------------------------------------------------------------------------*/

u32 NumSliceGroupChangeCycleBits(u32 picSizeInMbs, u32 sliceGroupChangeRate)
{

/* Variables */

    u32 tmp, numBits, mask;

/* Code */

    ASSERT(picSizeInMbs);
    ASSERT(sliceGroupChangeRate);
    ASSERT(sliceGroupChangeRate <= picSizeInMbs);

    /* compute (picSizeInMbs / sliceGroupChangeRate + 1), rounded up */
    if(picSizeInMbs % sliceGroupChangeRate)
        tmp = 2 + picSizeInMbs / sliceGroupChangeRate;
    else
        tmp = 1 + picSizeInMbs / sliceGroupChangeRate;

    numBits = 0;
    mask = ~0U;

    /* set numBits to position of right-most non-zero bit */
    while(tmp & (mask << ++numBits))
        ;
    numBits--;

    /* add one more bit if value greater than 2^numBits */
    if(tmp & ((1 << numBits) - 1))
        numBits++;

    return (numBits);

}

/*------------------------------------------------------------------------------

    Function: RefPicListReordering

        Functional description:
            Decode reference picture list reordering syntax elements from
            the stream. Max number of reordering commands is numRefIdxActive.

        Inputs:
            pStrmData       pointer to stream data structure
            numRefIdxActive number of active reference indices to be used for
                            current slice
            maxPicNum       maxFrameNum from the active SPS

        Outputs:
            pRefPicListReordering   decoded data is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/

u32 RefPicListReordering(strmData_t * pStrmData,
                         refPicListReordering_t * pRefPicListReordering,
                         u32 numRefIdxActive, u32 maxPicNum, u32 mvc)
{

/* Variables */

    u32 tmp, value, i;
    u32 command;

/* Code */

    ASSERT(pStrmData);
    ASSERT(pRefPicListReordering);
    ASSERT(numRefIdxActive);
    ASSERT(maxPicNum);

    tmp = h264bsdGetBits(pStrmData, 1);
    if(tmp == END_OF_STREAM)
        return (HANTRO_NOK);

    pRefPicListReordering->refPicListReorderingFlagL0 = tmp;

    if(pRefPicListReordering->refPicListReorderingFlagL0)
    {
        i = 0;

        do
        {
            if(i > numRefIdxActive)
            {
                ERROR_PRINT("Too many reordering commands");
                return (HANTRO_NOK);
            }

            tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &command);
            if(tmp != HANTRO_OK)
                return (tmp);
            if(command > (mvc ? 5 : 3))
            {
                ERROR_PRINT("reordering_of_pic_nums_idc");
                return (HANTRO_NOK);
            }

            pRefPicListReordering->command[i].reorderingOfPicNumsIdc = command;

            if((command == 0) || (command == 1))
            {
                tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
                if(tmp != HANTRO_OK)
                    return (tmp);
                if(value >= maxPicNum)
                {
                    ERROR_PRINT("abs_diff_pic_num_minus1");
                    return (HANTRO_NOK);
                }
                pRefPicListReordering->command[i].absDiffPicNum = value + 1;
            }
            else if(command == 2)
            {
                tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
                if(tmp != HANTRO_OK)
                    return (tmp);
                pRefPicListReordering->command[i].longTermPicNum = value;
            }
            else if(command == 4 || command == 5)
            {
                tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
                if(tmp != HANTRO_OK)
                    return (tmp);
                pRefPicListReordering->command[i].absDiffViewIdx = value + 1;
            }
            i++;
        }
        while(command != 3);

        /* there shall be at least one reordering command if
         * refPicListReorderingFlagL0 was set */
        if(i == 1)
        {
            ERROR_PRINT("ref_pic_list_reordering");
            return (HANTRO_NOK);
        }
    }

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: DecRefPicMarking

        Functional description:
            Decode decoded reference picture marking syntax elements from
            the stream.

        Inputs:
            pStrmData       pointer to stream data structure
            nalUnitType     type of the current NAL unit
            numRefFrames    max number of reference frames from the active SPS

        Outputs:
            pDecRefPicMarking   decoded data is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/

u32 DecRefPicMarking(strmData_t * pStrmData,
                     decRefPicMarking_t * pDecRefPicMarking,
                     u32 idrPicFlag, u32 numRefFrames)
{

/* Variables */

    u32 tmp, value;
    u32 i;
    u32 operation;
    u32 strmPos;

    /* variables for error checking purposes, store number of memory
     * management operations of certain type */
    u32 num4 = 0, num5 = 0, num6 = 0, num1to3 = 0;

/* Code */

    strmPos = pStrmData->strmBuffReadBits;
    pStrmData->emulByteCount = 0;

    if(idrPicFlag)
    {
        tmp = h264bsdGetBits(pStrmData, 1);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);
        pDecRefPicMarking->noOutputOfPriorPicsFlag = tmp;

        tmp = h264bsdGetBits(pStrmData, 1);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);
        pDecRefPicMarking->longTermReferenceFlag = tmp;
        if(!numRefFrames && pDecRefPicMarking->longTermReferenceFlag)
        {
            ERROR_PRINT("long_term_reference_flag");
            return (HANTRO_NOK);
        }
    }
    else
    {
        tmp = h264bsdGetBits(pStrmData, 1);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);
        pDecRefPicMarking->adaptiveRefPicMarkingModeFlag = tmp;
        if(pDecRefPicMarking->adaptiveRefPicMarkingModeFlag)
        {
            i = 0;
            do
            {
                /* see explanation of the MAX_NUM_MMC_OPERATIONS in
                 * slice_header.h */
                if(i > (2 * numRefFrames + 2))
                {
                    ERROR_PRINT("Too many management operations");
                    return (HANTRO_NOK);
                }

                tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &operation);
                if(tmp != HANTRO_OK)
                    return (tmp);
                if(operation > 6)
                {
                    ERROR_PRINT("memory_management_control_operation");
                    return (HANTRO_NOK);
                }

                pDecRefPicMarking->operation[i].
                    memoryManagementControlOperation = operation;
                if((operation == 1) || (operation == 3))
                {
                    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
                    if(tmp != HANTRO_OK)
                        return (tmp);
                    pDecRefPicMarking->operation[i].differenceOfPicNums =
                        value + 1;
                }
                if(operation == 2)
                {
                    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
                    if(tmp != HANTRO_OK)
                        return (tmp);
                    pDecRefPicMarking->operation[i].longTermPicNum = value;
                }
                if((operation == 3) || (operation == 6))
                {
                    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
                    if(tmp != HANTRO_OK)
                        return (tmp);
                    pDecRefPicMarking->operation[i].longTermFrameIdx = value;
                }
                if(operation == 4)
                {
                    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
                    if(tmp != HANTRO_OK)
                        return (tmp);
                    /* value shall be in range [0, numRefFrames] */
                    if(value > numRefFrames)
                    {
                        ERROR_PRINT("max_long_term_frame_idx_plus1");
                        return (HANTRO_NOK);
                    }
                    if(value == 0)
                    {
                        pDecRefPicMarking->operation[i].
                            maxLongTermFrameIdx = NO_LONG_TERM_FRAME_INDICES;
                    }
                    else
                    {
                        pDecRefPicMarking->operation[i].
                            maxLongTermFrameIdx = value - 1;
                    }
                    num4++;
                }
                if(operation == 5)
                {
                    num5++;
                }
                if(operation && operation <= 3)
                    num1to3++;
                if(operation == 6)
                    num6++;

                i++;
            }
            while(operation != 0);

            /* error checking */
            if(num4 > 1 || num5 > 1 || num6 > 1 || (num1to3 && num5))
                return (HANTRO_NOK);

        }
    }

    pDecRefPicMarking->strmLen = pStrmData->strmBuffReadBits - strmPos -
        8*pStrmData->emulByteCount;

    return (HANTRO_OK);
}

/*------------------------------------------------------------------------------

    Function name: h264bsdCheckPpsId

        Functional description:
            Peek value of pic_parameter_set_id from the slice header. Function
            does not modify current stream positions but copies the stream
            data structure to tmp structure which is used while accessing
            stream data.

        Inputs:
            pStrmData       pointer to stream data structure

        Outputs:
            picParamSetId   value is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/

u32 h264bsdCheckPpsId(strmData_t * pStrmData, u32 * picParamSetId)
{

/* Variables */

    u32 tmp, value;
    strmData_t tmpStrmData[1];

/* Code */

    ASSERT(pStrmData);

    /* don't touch original stream position params */
    *tmpStrmData = *pStrmData;

    /* first_mb_in_slice */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* slice_type */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);
    if(value >= MAX_NUM_PIC_PARAM_SETS)
        return (HANTRO_NOK);

    *picParamSetId = value;

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckFrameNum

        Functional description:
            Peek value of frame_num from the slice header. Function does not
            modify current stream positions but copies the stream data
            structure to tmp structure which is used while accessing stream
            data.

        Inputs:
            pStrmData       pointer to stream data structure
            maxFrameNum

        Outputs:
            frameNum        value is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/

u32 h264bsdCheckFrameNum(strmData_t * pStrmData,
                         u32 maxFrameNum, u32 * frameNum)
{

/* Variables */

    u32 tmp, value, i;
    strmData_t tmpStrmData[1];

/* Code */

    ASSERT(pStrmData);
    ASSERT(maxFrameNum);
    ASSERT(frameNum);

    /* don't touch original stream position params */
    *tmpStrmData = *pStrmData;

    /* skip first_mb_in_slice */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* skip slice_type */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* skip pic_parameter_set_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* log2(maxFrameNum) -> num bits to represent frame_num */
    i = 0;
    while(maxFrameNum >> i)
        i++;
    i--;

    /* frame_num */
    tmp = h264bsdGetBits(tmpStrmData, i);
    if(tmp == END_OF_STREAM)
        return (HANTRO_NOK);
    *frameNum = tmp;

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckIdrPicId

        Functional description:
            Peek value of idr_pic_id from the slice header. Function does not
            modify current stream positions but copies the stream data
            structure to tmp structure which is used while accessing stream
            data.

        Inputs:
            pStrmData       pointer to stream data structure
            maxFrameNum     max frame number from active SPS
            nalUnitType     type of the current NAL unit

        Outputs:
            idrPicId        value is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/

u32 h264bsdCheckIdrPicId(strmData_t * pStrmData,
                         u32 maxFrameNum,
                         nalUnitType_e nalUnitType, u32 fieldPicFlag,
                         u32 * idrPicId)
{

/* Variables */

    u32 tmp, value, i;
    strmData_t tmpStrmData[1];

/* Code */

    ASSERT(pStrmData);
    ASSERT(maxFrameNum);
    ASSERT(idrPicId);

    /* nalUnitType must be equal to 5 because otherwise idrPicId is not
     * present */
    if(nalUnitType != NAL_CODED_SLICE_IDR)
        return (HANTRO_NOK);

    /* don't touch original stream position params */
    *tmpStrmData = *pStrmData;

    /* skip first_mb_in_slice */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* skip slice_type */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* skip pic_parameter_set_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* log2(maxFrameNum) -> num bits to represent frame_num */
    i = 0;
    while(maxFrameNum >> i)
        i++;
    i--;

    /* skip frame_num */
    tmp = h264bsdGetBits(tmpStrmData, i);
    if(tmp == END_OF_STREAM)
        return (HANTRO_NOK);

    if (fieldPicFlag)
    {
        tmp = h264bsdGetBits(tmpStrmData, i);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);
        if (tmp)
        {
            tmp = h264bsdGetBits(tmpStrmData, i);
            if(tmp == END_OF_STREAM)
                return (HANTRO_NOK);
        }
    }

    /* idr_pic_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, idrPicId);
    if(tmp != HANTRO_OK)
        return (tmp);

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckPicOrderCntLsb

        Functional description:
            Peek value of pic_order_cnt_lsb from the slice header. Function
            does not modify current stream positions but copies the stream
            data structure to tmp structure which is used while accessing
            stream data.

        Inputs:
            pStrmData       pointer to stream data structure
            pSeqParamSet    pointer to active SPS
            nalUnitType     type of the current NAL unit

        Outputs:
            picOrderCntLsb  value is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/

u32 h264bsdCheckPicOrderCntLsb(strmData_t * pStrmData,
                               seqParamSet_t * pSeqParamSet,
                               nalUnitType_e nalUnitType,
                               u32 * picOrderCntLsb)
{

/* Variables */

    u32 tmp, value, i;
    strmData_t tmpStrmData[1];

/* Code */

    ASSERT(pStrmData);
    ASSERT(pSeqParamSet);
    ASSERT(picOrderCntLsb);

    /* picOrderCntType must be equal to 0 */
    ASSERT(pSeqParamSet->picOrderCntType == 0);
    ASSERT(pSeqParamSet->maxFrameNum);
    ASSERT(pSeqParamSet->maxPicOrderCntLsb);

    /* don't touch original stream position params */
    *tmpStrmData = *pStrmData;

    /* skip first_mb_in_slice */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* skip slice_type */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* skip pic_parameter_set_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* log2(maxFrameNum) -> num bits to represent frame_num */
    i = 0;
    while(pSeqParamSet->maxFrameNum >> i)
        i++;
    i--;

    /* skip frame_num */
    tmp = h264bsdGetBits(tmpStrmData, i);
    if(tmp == END_OF_STREAM)
        return (HANTRO_NOK);

    if (!pSeqParamSet->frameMbsOnlyFlag)
    {
        tmp = h264bsdGetBits(tmpStrmData, 1);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);
        if (tmp)
        {
            tmp = h264bsdGetBits(tmpStrmData, 1);
            if(tmp == END_OF_STREAM)
                return (HANTRO_NOK);
        }
    }
    /* skip idr_pic_id when necessary */
    if(nalUnitType == NAL_CODED_SLICE_IDR)
    {
        tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
        if(tmp != HANTRO_OK)
            return (tmp);
    }

    /* log2(maxPicOrderCntLsb) -> num bits to represent pic_order_cnt_lsb */
    i = 0;
    while(pSeqParamSet->maxPicOrderCntLsb >> i)
        i++;
    i--;

    /* pic_order_cnt_lsb */
    tmp = h264bsdGetBits(tmpStrmData, i);
    if(tmp == END_OF_STREAM)
        return (HANTRO_NOK);
    *picOrderCntLsb = tmp;

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckDeltaPicOrderCntBottom

        Functional description:
            Peek value of delta_pic_order_cnt_bottom from the slice header.
            Function does not modify current stream positions but copies the
            stream data structure to tmp structure which is used while
            accessing stream data.

        Inputs:
            pStrmData       pointer to stream data structure
            pSeqParamSet    pointer to active SPS
            nalUnitType     type of the current NAL unit

        Outputs:
            deltaPicOrderCntBottom  value is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/

u32 h264bsdCheckDeltaPicOrderCntBottom(strmData_t * pStrmData,
                                       seqParamSet_t * pSeqParamSet,
                                       nalUnitType_e nalUnitType,
                                       i32 * deltaPicOrderCntBottom)
{

/* Variables */

    u32 tmp, value, i;
    strmData_t tmpStrmData[1];

/* Code */

    ASSERT(pStrmData);
    ASSERT(pSeqParamSet);
    ASSERT(deltaPicOrderCntBottom);

    /* picOrderCntType must be equal to 0 and picOrderPresentFlag must be HANTRO_TRUE
     * */
    ASSERT(pSeqParamSet->picOrderCntType == 0);
    ASSERT(pSeqParamSet->maxFrameNum);
    ASSERT(pSeqParamSet->maxPicOrderCntLsb);

    /* don't touch original stream position params */
    *tmpStrmData = *pStrmData;

    /* skip first_mb_in_slice */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* skip slice_type */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* skip pic_parameter_set_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* log2(maxFrameNum) -> num bits to represent frame_num */
    i = 0;
    while(pSeqParamSet->maxFrameNum >> i)
        i++;
    i--;

    /* skip frame_num */
    tmp = h264bsdGetBits(tmpStrmData, i);
    if(tmp == END_OF_STREAM)
        return (HANTRO_NOK);

    if (!pSeqParamSet->frameMbsOnlyFlag)
    {
        tmp = h264bsdGetBits(tmpStrmData, 1);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);
        if (tmp)
        {
            tmp = h264bsdGetBits(tmpStrmData, 1);
            if(tmp == END_OF_STREAM)
                return (HANTRO_NOK);
        }
    }
    /* skip idr_pic_id when necessary */
    if(nalUnitType == NAL_CODED_SLICE_IDR)
    {
        tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
        if(tmp != HANTRO_OK)
            return (tmp);
    }

    /* log2(maxPicOrderCntLsb) -> num bits to represent pic_order_cnt_lsb */
    i = 0;
    while(pSeqParamSet->maxPicOrderCntLsb >> i)
        i++;
    i--;

    /* skip pic_order_cnt_lsb */
    tmp = h264bsdGetBits(tmpStrmData, i);
    if(tmp == END_OF_STREAM)
        return (HANTRO_NOK);

    /* delta_pic_order_cnt_bottom */
    tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, deltaPicOrderCntBottom);
    if(tmp != HANTRO_OK)
        return (tmp);

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckDeltaPicOrderCnt

        Functional description:
            Peek values delta_pic_order_cnt[0] and delta_pic_order_cnt[1]
            from the slice header. Function does not modify current stream
            positions but copies the stream data structure to tmp structure
            which is used while accessing stream data.

        Inputs:
            pStrmData               pointer to stream data structure
            pSeqParamSet            pointer to active SPS
            nalUnitType             type of the current NAL unit
            picOrderPresentFlag     flag indicating if delta_pic_order_cnt[1]
                                    is present in the stream

        Outputs:
            deltaPicOrderCnt        values are stored here

        Returns:
            HANTRO_OK               success
            HANTRO_NOK              invalid stream data

------------------------------------------------------------------------------*/

u32 h264bsdCheckDeltaPicOrderCnt(strmData_t * pStrmData,
                                 seqParamSet_t * pSeqParamSet,
                                 nalUnitType_e nalUnitType,
                                 u32 picOrderPresentFlag,
                                 i32 * deltaPicOrderCnt)
{

/* Variables */

    u32 tmp, value, i;
    u32 fieldPicFlag = 0;
    strmData_t tmpStrmData[1];

/* Code */

    ASSERT(pStrmData);
    ASSERT(pSeqParamSet);
    ASSERT(deltaPicOrderCnt);

    /* picOrderCntType must be equal to 1 and deltaPicOrderAlwaysZeroFlag must
     * be HANTRO_FALSE */
    ASSERT(pSeqParamSet->picOrderCntType == 1);
    ASSERT(!pSeqParamSet->deltaPicOrderAlwaysZeroFlag);
    ASSERT(pSeqParamSet->maxFrameNum);

    /* don't touch original stream position params */
    *tmpStrmData = *pStrmData;

    /* skip first_mb_in_slice */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* skip slice_type */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* skip pic_parameter_set_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* log2(maxFrameNum) -> num bits to represent frame_num */
    i = 0;
    while(pSeqParamSet->maxFrameNum >> i)
        i++;
    i--;

    /* skip frame_num */
    tmp = h264bsdGetBits(tmpStrmData, i);
    if(tmp == END_OF_STREAM)
        return (HANTRO_NOK);

    if (!pSeqParamSet->frameMbsOnlyFlag)
    {
        tmp = h264bsdGetBits(tmpStrmData, 1);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);
        fieldPicFlag = tmp;
        if (tmp)
        {
            tmp = h264bsdGetBits(tmpStrmData, 1);
            if(tmp == END_OF_STREAM)
                return (HANTRO_NOK);
        }
    }
    /* skip idr_pic_id when necessary */
    if(nalUnitType == NAL_CODED_SLICE_IDR)
    {
        tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
        if(tmp != HANTRO_OK)
            return (tmp);
    }

    /* delta_pic_order_cnt[0] */
    tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, &deltaPicOrderCnt[0]);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* delta_pic_order_cnt[1] if present */
    if(picOrderPresentFlag && !fieldPicFlag)
    {
        tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, &deltaPicOrderCnt[1]);
        if(tmp != HANTRO_OK)
            return (tmp);
    }

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------
    Function name   : h264bsdCheckPriorPicsFlag
    Description     :
    Return type     : u32
    Argument        : u32 * noOutputOfPriorPicsFlag
    Argument        : const strmData_t * pStrmData
    Argument        : const seqParamSet_t * pSeqParamSet
    Argument        : const picParamSet_t * pPicParamSet
    Argument        : nalUnitType_e nalUnitType
------------------------------------------------------------------------------*/
u32 h264bsdCheckPriorPicsFlag(u32 * noOutputOfPriorPicsFlag,
                              const strmData_t * pStrmData,
                              const seqParamSet_t * pSeqParamSet,
                              const picParamSet_t * pPicParamSet,
                              nalUnitType_e nalUnitType)
{
/* Variables */

    u32 tmp, value, i;
    i32 ivalue;
    u32 fieldPicFlag = 0;
    strmData_t tmpStrmData[1];

/* Code */

    ASSERT(pStrmData);
    ASSERT(pSeqParamSet);
    ASSERT(pPicParamSet);
    ASSERT(noOutputOfPriorPicsFlag);

    /* must be IDR lsice */
    ASSERT(nalUnitType == NAL_CODED_SLICE_IDR);

    /* don't touch original stream position params */
    *tmpStrmData = *pStrmData;

    /* skip first_mb_in_slice */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* slice_type */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* skip pic_parameter_set_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* log2(maxFrameNum) -> num bits to represent frame_num */
    i = 0;
    while(pSeqParamSet->maxFrameNum >> i)
        i++;
    i--;

    /* skip frame_num */
    tmp = h264bsdGetBits(tmpStrmData, i);
    if(tmp == END_OF_STREAM)
        return (HANTRO_NOK);

    if (!pSeqParamSet->frameMbsOnlyFlag)
    {
        tmp = h264bsdGetBits(tmpStrmData, 1);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);
        fieldPicFlag = tmp;
        if (tmp)
        {
            tmp = h264bsdGetBits(tmpStrmData, 1);
            if(tmp == END_OF_STREAM)
                return (HANTRO_NOK);
        }
    }
    /* skip idr_pic_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    if(pSeqParamSet->picOrderCntType == 0)
    {
        /* log2(maxPicOrderCntLsb) -> num bits to represent pic_order_cnt_lsb */
        i = 0;
        while(pSeqParamSet->maxPicOrderCntLsb >> i)
            i++;
        i--;

        /* skip pic_order_cnt_lsb */
        tmp = h264bsdGetBits(tmpStrmData, i);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);

        if(pPicParamSet->picOrderPresentFlag && !fieldPicFlag)
        {
            /* skip delta_pic_order_cnt_bottom */
            tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, &ivalue);
            if(tmp != HANTRO_OK)
                return (tmp);
        }
    }

    if(pSeqParamSet->picOrderCntType == 1 &&
       !pSeqParamSet->deltaPicOrderAlwaysZeroFlag)
    {
        /* skip delta_pic_order_cnt[0] */
        tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, &ivalue);
        if(tmp != HANTRO_OK)
            return (tmp);

        /* skip delta_pic_order_cnt[1] if present */
        if(pPicParamSet->picOrderPresentFlag && !fieldPicFlag)
        {
            tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, &ivalue);
            if(tmp != HANTRO_OK)
                return (tmp);
        }
    }

    /* skip redundant_pic_cnt */
    if(pPicParamSet->redundantPicCntPresentFlag)
    {
        tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
        if(tmp != HANTRO_OK)
            return (tmp);
    }

    *noOutputOfPriorPicsFlag = h264bsdGetBits(tmpStrmData, 1);
    if(*noOutputOfPriorPicsFlag == END_OF_STREAM)
        return (HANTRO_NOK);

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: PredWeightTable

        Functional description:

        Inputs:

        Outputs:

        Returns:

------------------------------------------------------------------------------*/

u32 PredWeightTable(strmData_t *pStrmData, sliceHeader_t *pSliceHeader,
    u32 monoChrome)
{

/* Variables */

    u32 tmp, value, i;
    i32 itmp;

/* Code */

    ASSERT(pStrmData);
    ASSERT(pSliceHeader);

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
    if (!monoChrome)
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
    for (i = 0; i < pSliceHeader->numRefIdxL0Active; i++)
    {
        tmp = h264bsdGetBits(pStrmData, 1);
        if (tmp == 1)
        {
            tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
            tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
        }
        if (!monoChrome)
        {
            tmp = h264bsdGetBits(pStrmData, 1);
            if (tmp == 1)
            {
                tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
                tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
                tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
                tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
            }
        }
    }

    if (IS_B_SLICE(pSliceHeader->sliceType))
    {
        for (i = 0; i < pSliceHeader->numRefIdxL1Active; i++)
        {
            tmp = h264bsdGetBits(pStrmData, 1);
            if (tmp == 1)
            {
                tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
                tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
            }
            if (!monoChrome)
            {
                tmp = h264bsdGetBits(pStrmData, 1);
                if (tmp == 1)
                {
                    tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
                    tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
                    tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
                    tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
                }
            }
        }
    }

    return (HANTRO_OK);
}

/*------------------------------------------------------------------------------

    Function name: h264bsdIsOppositeFieldPic

        Functional description:

        Inputs:

        Outputs:

        Returns:

------------------------------------------------------------------------------*/

u32 h264bsdIsOppositeFieldPic(sliceHeader_t * pSliceCurr,
                              sliceHeader_t * pSlicePrev,
                              u32 *secondField, u32 prevRefFrameNum,
                              u32 newPicture)
{

/* Variables */

/* Code */

    ASSERT(pSliceCurr);
    ASSERT(pSlicePrev);

    if ((pSliceCurr->frameNum == pSlicePrev->frameNum ||
         pSliceCurr->frameNum == prevRefFrameNum) &&
        pSliceCurr->fieldPicFlag && pSlicePrev->fieldPicFlag &&
        pSliceCurr->bottomFieldFlag != pSlicePrev->bottomFieldFlag &&
        /**secondField &&*/ !newPicture)
    {
        //*secondField = 0;
        return HANTRO_TRUE;
    }
    else
    {
        //*secondField = pSliceCurr->fieldPicFlag ? 1 : 0;
        return HANTRO_FALSE;
    }

}

/*------------------------------------------------------------------------------

    Function: h264bsdCheckFieldPicFlag

        Functional description:

        Inputs:
            pStrmData       pointer to stream data structure
            maxFrameNum     max frame number from active SPS
            nalUnitType     type of the current NAL unit

        Outputs:
            idrPicId        value is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/

u32 h264bsdCheckFieldPicFlag(strmData_t * pStrmData,
                         u32 maxFrameNum,
                         nalUnitType_e nalUnitType,
                         u32 fieldPicFlagPresent,
                         u32 *fieldPicFlag)
{

/* Variables */

    u32 tmp, value, i;
    strmData_t tmpStrmData[1];

/* Code */

    ASSERT(pStrmData);
    ASSERT(maxFrameNum);
    ASSERT(fieldPicFlag);

    /* don't touch original stream position params */
    *tmpStrmData = *pStrmData;

    /* skip first_mb_in_slice */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* skip slice_type */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* skip pic_parameter_set_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* log2(maxFrameNum) -> num bits to represent frame_num */
    i = 0;
    while(maxFrameNum >> i)
        i++;
    i--;

    /* skip frame_num */
    tmp = h264bsdGetBits(tmpStrmData, i);
    if(tmp == END_OF_STREAM)
        return (HANTRO_NOK);

    if (fieldPicFlagPresent)
    {
        tmp = h264bsdGetBits(tmpStrmData, 1);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);
        *fieldPicFlag = tmp;
    }

    return (HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: h264bsdBottomFieldFlag

        Functional description:

        Inputs:
            pStrmData       pointer to stream data structure
            maxFrameNum     max frame number from active SPS
            nalUnitType     type of the current NAL unit

        Outputs:
            idrPicId        value is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/

u32 h264bsdCheckBottomFieldFlag(strmData_t * pStrmData,
                         u32 maxFrameNum,
                         nalUnitType_e nalUnitType, u32 fieldPicFlag,
                         u32 * bottomFieldFlag)
{

/* Variables */

    u32 tmp, value, i;
    strmData_t tmpStrmData[1];

/* Code */

    ASSERT(pStrmData);
    ASSERT(maxFrameNum);
    ASSERT(bottomFieldFlag);

    /* don't touch original stream position params */
    *tmpStrmData = *pStrmData;

    /* skip first_mb_in_slice */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* skip slice_type */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* skip pic_parameter_set_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* log2(maxFrameNum) -> num bits to represent frame_num */
    i = 0;
    while(maxFrameNum >> i)
        i++;
    i--;

    /* skip frame_num */
    tmp = h264bsdGetBits(tmpStrmData, i);
    if(tmp == END_OF_STREAM)
        return (HANTRO_NOK);

    if (fieldPicFlag)
    {
        tmp = h264bsdGetBits(tmpStrmData, 1);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);
        if (tmp)
        {
            tmp = h264bsdGetBits(tmpStrmData, 1);
            if(tmp == END_OF_STREAM)
                return (HANTRO_NOK);
            *bottomFieldFlag = tmp;
        }
    }

    return (HANTRO_OK);

}


/*------------------------------------------------------------------------------

    Function: h264bsdCheckFirstMbInSlice

        Functional description:
            Peek value of first_mb_in_slice from the slice header. Function does not
            modify current stream positions but copies the stream data
            structure to tmp structure which is used while accessing stream
            data.

        Inputs:
            pStrmData       pointer to stream data structure
            nalUnitType     type of the current NAL unit

        Outputs:
            firstMbInSlice  value is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/

u32 h264bsdCheckFirstMbInSlice(strmData_t * pStrmData,
                         nalUnitType_e nalUnitType,
                         u32 * firstMbInSlice)
{

/* Variables */

    u32 tmp, value;
    strmData_t tmpStrmData[1];

/* Code */

    ASSERT(pStrmData);
    ASSERT(firstMbInSlice);

    /* nalUnitType must be equal to 5 because otherwise we shouldn't be
     * here */
    if(nalUnitType != NAL_CODED_SLICE_IDR)
        return (HANTRO_NOK);

    /* don't touch original stream position params */
    *tmpStrmData = *pStrmData;

    /* skip first_mb_in_slice */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    *firstMbInSlice = value;

    return (HANTRO_OK);

}


/*------------------------------------------------------------------------------
    Function name   : CheckRedundantPicCnt
    Description     :
    Return type     : u32
    Argument        : u32 * redundantPicCnt
    Argument        : const strmData_t * pStrmData
    Argument        : const seqParamSet_t * pSeqParamSet
    Argument        : const picParamSet_t * pPicParamSet
    Argument        : nalUnitType_e nalUnitType
------------------------------------------------------------------------------*/
u32 h264bsdCheckRedundantPicCnt(const strmData_t * pStrmData,
                                const seqParamSet_t * pSeqParamSet,
                                const picParamSet_t * pPicParamSet,
                                nalUnitType_e nalUnitType,
                                u32 * redundantPicCnt )
{
/* Variables */

    u32 tmp, value, i;
    i32 ivalue;
    u32 fieldPicFlag = 0;
    strmData_t tmpStrmData[1];

/* Code */

    ASSERT(pStrmData);
    ASSERT(pSeqParamSet);
    ASSERT(pPicParamSet);
    ASSERT(redundantPicCnt);

    /* must be IDR lsice */
    ASSERT(nalUnitType == NAL_CODED_SLICE_IDR);

    if(!pPicParamSet->redundantPicCntPresentFlag)
    {
        *redundantPicCnt = 0;
        return HANTRO_OK;
    }

    /* don't touch original stream position params */
    *tmpStrmData = *pStrmData;

    /* skip first_mb_in_slice */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* slice_type */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* skip pic_parameter_set_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    /* log2(maxFrameNum) -> num bits to represent frame_num */
    i = 0;
    while(pSeqParamSet->maxFrameNum >> i)
        i++;
    i--;

    /* skip frame_num */
    tmp = h264bsdGetBits(tmpStrmData, i);
    if(tmp == END_OF_STREAM)
        return (HANTRO_NOK);

    if (!pSeqParamSet->frameMbsOnlyFlag)
    {
        tmp = h264bsdGetBits(tmpStrmData, 1);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);
        fieldPicFlag = tmp;
        if (tmp)
        {
            tmp = h264bsdGetBits(tmpStrmData, 1);
            if(tmp == END_OF_STREAM)
                return (HANTRO_NOK);
        }
    }
    /* skip idr_pic_id */
    tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
    if(tmp != HANTRO_OK)
        return (tmp);

    if(pSeqParamSet->picOrderCntType == 0)
    {
        /* log2(maxPicOrderCntLsb) -> num bits to represent pic_order_cnt_lsb */
        i = 0;
        while(pSeqParamSet->maxPicOrderCntLsb >> i)
            i++;
        i--;

        /* skip pic_order_cnt_lsb */
        tmp = h264bsdGetBits(tmpStrmData, i);
        if(tmp == END_OF_STREAM)
            return (HANTRO_NOK);

        if(pPicParamSet->picOrderPresentFlag && !fieldPicFlag)
        {
            /* skip delta_pic_order_cnt_bottom */
            tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, &ivalue);
            if(tmp != HANTRO_OK)
                return (tmp);
        }
    }

    if(pSeqParamSet->picOrderCntType == 1 &&
       !pSeqParamSet->deltaPicOrderAlwaysZeroFlag)
    {
        /* skip delta_pic_order_cnt[0] */
        tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, &ivalue);
        if(tmp != HANTRO_OK)
            return (tmp);

        /* skip delta_pic_order_cnt[1] if present */
        if(pPicParamSet->picOrderPresentFlag && !fieldPicFlag)
        {
            tmp = h264bsdDecodeExpGolombSigned(tmpStrmData, &ivalue);
            if(tmp != HANTRO_OK)
                return (tmp);
        }
    }

    /* redundant_pic_cnt */
    if(pPicParamSet->redundantPicCntPresentFlag)
    {
        tmp = h264bsdDecodeExpGolombUnsigned(tmpStrmData, &value);
        if(tmp != HANTRO_OK)
            return (tmp);
        *redundantPicCnt = value;
    }

    return (HANTRO_OK);

}
