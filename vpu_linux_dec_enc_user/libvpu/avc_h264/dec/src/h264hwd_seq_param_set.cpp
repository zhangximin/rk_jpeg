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
--  Abstract : Decode seq parameter set information from the stream
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264hwd_seq_param_set.c,v $
--  $Date: 2010/04/22 06:54:56 $
--  $Revision: 1.10 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "h264hwd_seq_param_set.h"
#include "h264hwd_util.h"
#include "h264hwd_vlc.h"
#include "h264hwd_vui.h"
#include "h264hwd_cfg.h"
#include "dwl.h"
#include <utils/Log.h>
#include <stdlib.h>

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/* enumeration to indicate invalid return value from the GetDpbSize function */
enum {INVALID_DPB_SIZE = 0x7FFFFFFF};

const u32 default4x4Intra[16] =
    {6,13,13,20,20,20,28,28,28,28,32,32,32,37,37,42};
const u32 default4x4Inter[16] =
    {10,14,14,20,20,20,24,24,24,24,27,27,27,30,30,34};
const u32 default8x8Intra[64] =
    { 6,10,10,13,11,13,16,16,16,16,18,18,18,18,18,23,
     23,23,23,23,23,25,25,25,25,25,25,25,27,27,27,27,
     27,27,27,27,29,29,29,29,29,29,29,31,31,31,31,31,
     31,33,33,33,33,33,36,36,36,36,38,38,38,40,40,42};
const u32 default8x8Inter[64] =
    { 9,13,13,15,13,15,17,17,17,17,19,19,19,19,19,21,
     21,21,21,21,21,22,22,22,22,22,22,22,24,24,24,24,
     24,24,24,24,25,25,25,25,25,25,25,27,27,27,27,27,
     27,28,28,28,28,28,30,30,30,30,32,32,32,33,33,35};

static const u32 zigZag4x4[16] = {
    0,  1,  4,  8, 5,  2,  3,  6, 9, 12, 13, 10, 7, 11, 14, 15 };

static const u32 zigZag8x8[64] = {
     0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63 };


/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

static u32 GetDpbSize(u32 picSizeInMbs, u32 levelIdc);
static u32 DecodeMvcExtension(strmData_t *pStrmData,
    seqParamSet_t *pSeqParamSet);

/*------------------------------------------------------------------------------

    Function name: h264bsdDecodeSeqParamSet

        Functional description:
            Decode sequence parameter set information from the stream.

            Function allocates memory for offsetForRefFrame array if
            picture order count type is 1 and numRefFramesInPicOrderCntCycle
            is greater than zero.

        Inputs:
            pStrmData       pointer to stream data structure

        Outputs:
            pSeqParamSet    decoded information is stored here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      failure, invalid information or end of stream
            MEMORY_ALLOCATION_ERROR for memory allocation failure

------------------------------------------------------------------------------*/

void ScalingList(u8 scalingList[][64], strmData_t *pStrmData, u32 idx)
{

    u32 lastScale = 8, nextScale = 8;
    u32 i, size;
    u32 useDefault = 0;
    i32 delta;
    const u32 *defList[8] = {
        default4x4Intra, default4x4Intra, default4x4Intra,
        default4x4Inter, default4x4Inter, default4x4Inter,
        default8x8Intra, default8x8Inter };

    const u32 *zigZag;

    size = idx < 6 ? 16 : 64;

    zigZag = idx < 6 ? zigZag4x4 : zigZag8x8;

    for (i = 0; i < size; i++)
    {
        if (nextScale)
        {
            u32 tmp = h264bsdDecodeExpGolombSigned(pStrmData, &delta);
            (void)tmp; /* TODO: should we check for error here? */

            nextScale = (lastScale + delta + 256)&0xFF;
            if (!i && !nextScale)
            {
                useDefault = 1;
                break;
            }
        }
        scalingList[idx][zigZag[i]] = nextScale ? nextScale : lastScale;
        lastScale = scalingList[idx][zigZag[i]];
    }

    if (useDefault)
        for (i = 0; i < size; i++)
            scalingList[idx][zigZag[i]] = defList[idx][i];

}

void FallbackScaling(u8 scalingList[][64], u32 idx)
{

    u32 i;

    ASSERT(idx < 8);

    switch (idx)
    {
        case 0:
            for (i = 0; i < 16; i++)
                scalingList[idx][zigZag4x4[i]] = default4x4Intra[i];
            break;
        case 3:
            for (i = 0; i < 16; i++)
                scalingList[idx][zigZag4x4[i]] = default4x4Inter[i];
            break;
        case 6:
            for (i = 0; i < 64; i++)
                scalingList[idx][zigZag8x8[i]] = default8x8Intra[i];
            break;
        case 7:
            for (i = 0; i < 64; i++)
                scalingList[idx][zigZag8x8[i]] = default8x8Inter[i];
            break;
        default:
            for (i = 0; i < 16; i++)
                scalingList[idx][i] = scalingList[idx-1][i];
            break;
    }
}


u32 h264bsdDecodeSeqParamSet(strmData_t *pStrmData, seqParamSet_t *pSeqParamSet,
    u32 mvcFlag)
{

/* Variables */

    u32 tmp, i, value;

/* Code */

    ASSERT(pStrmData);
    ASSERT(pSeqParamSet);

    memset(pSeqParamSet, 0, sizeof(seqParamSet_t));

    /* profile_idc */
    tmp = h264bsdGetBits(pStrmData, 8);
    if (tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    if (tmp != 66)
    {
        DEBUG_PRINT(("NOT BASELINE PROFILE %d\n", tmp));
    }
    pSeqParamSet->profileIdc = tmp;

    /* constrained_set0_flag */
    tmp = h264bsdGetBits(pStrmData, 1);
    pSeqParamSet->constrained_set0_flag = tmp;
    /* constrained_set1_flag */
    tmp = h264bsdGetBits(pStrmData, 1);
    pSeqParamSet->constrained_set1_flag = tmp;
    /* constrained_set2_flag */
    tmp = h264bsdGetBits(pStrmData, 1);
    pSeqParamSet->constrained_set2_flag = tmp;
    /* constrained_set3_flag */
    tmp = h264bsdGetBits(pStrmData, 1);
    pSeqParamSet->constrained_set3_flag = tmp;
    if (tmp == END_OF_STREAM)
        return(HANTRO_NOK);

    /* reserved_zero_4bits, values of these bits shall be ignored */
    tmp = h264bsdGetBits(pStrmData, 4);
    if (tmp == END_OF_STREAM)
    {
        ERROR_PRINT("reserved_zero_4bits");
        return(HANTRO_NOK);
    }

    tmp = h264bsdGetBits(pStrmData, 8);
    if (tmp == END_OF_STREAM)
    {
        ERROR_PRINT("level_idc");
        return(HANTRO_NOK);
    }
    pSeqParamSet->levelIdc = tmp;

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
        &pSeqParamSet->seqParameterSetId);
    if (tmp != HANTRO_OK)
        return(tmp);
    if (pSeqParamSet->seqParameterSetId >= MAX_NUM_SEQ_PARAM_SETS)
    {
        ERROR_PRINT("seq_param_set_id");
        return(HANTRO_NOK);
    }

    if( pSeqParamSet->profileIdc >= 100 )
    {
        /* chroma_format_idc */
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
            &value);
        if (tmp != HANTRO_OK)
            return(tmp);
        if( tmp > 1 )
            return(HANTRO_NOK);
        pSeqParamSet->chromaFormatIdc = value;
        if (pSeqParamSet->chromaFormatIdc == 0)
            pSeqParamSet->monoChrome = 1;
        /* residual_colour_transform_flag (skipped) */

        /* bit_depth_luma_minus8 */
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
            &value);
        if (tmp != HANTRO_OK)
            return(tmp);

        /* bit_depth_chroma_minus8 */
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
            &value);
        if (tmp != HANTRO_OK)
            return(tmp);

        /* qpprime_y_zero_transform_bypass_flag */
        tmp = h264bsdGetBits(pStrmData, 1);
        if (tmp == END_OF_STREAM)
            return(HANTRO_NOK);

        /* seq_scaling_matrix_present_flag */
        tmp = h264bsdGetBits(pStrmData, 1);
        if (tmp == END_OF_STREAM)
            return(HANTRO_NOK);
        pSeqParamSet->scalingMatrixPresentFlag = tmp;
        if (tmp)
        {
            for (i = 0; i < 8; i++)
            {
                tmp = h264bsdGetBits(pStrmData, 1);
                pSeqParamSet->scalingListPresent[i] = tmp;
                if (tmp)
                {
                    ScalingList(pSeqParamSet->scalingList, pStrmData, i);
                }
                else
                    FallbackScaling(pSeqParamSet->scalingList, i);
            }
        }

    }
    else
    {
        pSeqParamSet->chromaFormatIdc = 1; /* 4:2:0 */
        pSeqParamSet->scalingMatrixPresentFlag = 0;
    }

    /* log2_max_frame_num_minus4 */
    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
    if (tmp != HANTRO_OK)
    {
        ERROR_PRINT("log2_max_frame_num_minus4");
        return(tmp);
    }
    if (value > 12)
    {
        ERROR_PRINT("log2_max_frame_num_minus4");
        return(HANTRO_NOK);
    }
    /* maxFrameNum = 2^(log2_max_frame_num_minus4 + 4) */
    pSeqParamSet->maxFrameNum = 1 << (value+4);

    /* valid POC types are 0, 1 and 2 */
    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
    if (tmp != HANTRO_OK)
    {
        ERROR_PRINT("pic_order_cnt_type");
        return(tmp);
    }
    if (value > 2)
    {
        ERROR_PRINT("pic_order_cnt_type");
        return(HANTRO_NOK);
    }
    pSeqParamSet->picOrderCntType = value;

    if (pSeqParamSet->picOrderCntType == 0)
    {
        /* log2_max_pic_order_cnt_lsb_minus4 */
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if (tmp != HANTRO_OK)
            return(tmp);
        if (value > 12)
        {
            ERROR_PRINT("log2_max_pic_order_cnt_lsb_minus4");
            return(HANTRO_NOK);
        }
        /* maxPicOrderCntLsb = 2^(log2_max_pic_order_cnt_lsb_minus4 + 4) */
        pSeqParamSet->maxPicOrderCntLsb = 1 << (value+4);
    }
    else if (pSeqParamSet->picOrderCntType == 1)
    {
        tmp = h264bsdGetBits(pStrmData, 1);
        if (tmp == END_OF_STREAM)
            return(HANTRO_NOK);
        pSeqParamSet->deltaPicOrderAlwaysZeroFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

        tmp = h264bsdDecodeExpGolombSigned(pStrmData,
            &pSeqParamSet->offsetForNonRefPic);
        if (tmp != HANTRO_OK)
            return(tmp);

        tmp = h264bsdDecodeExpGolombSigned(pStrmData,
            &pSeqParamSet->offsetForTopToBottomField);
        if (tmp != HANTRO_OK)
            return(tmp);

        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
            &pSeqParamSet->numRefFramesInPicOrderCntCycle);
        if (tmp != HANTRO_OK)
            return(tmp);
        if (pSeqParamSet->numRefFramesInPicOrderCntCycle > 255)
        {
            ERROR_PRINT("num_ref_frames_in_pic_order_cnt_cycle");
            return(HANTRO_NOK);
        }

        if (pSeqParamSet->numRefFramesInPicOrderCntCycle)
        {
            /* NOTE: This has to be freed somewhere! */
//            ALLOCATE(pSeqParamSet->offsetForRefFrame, pSeqParamSet->numRefFramesInPicOrderCntCycle, i32);
//		pSeqParamSet->offsetForRefFrame = DWLmalloc(pSeqParamSet->numRefFramesInPicOrderCntCycle*sizeof(i32));

            if (pSeqParamSet->offsetForRefFrame == NULL)
                return(MEMORY_ALLOCATION_ERROR);

            for (i = 0; i < pSeqParamSet->numRefFramesInPicOrderCntCycle; i++)
            {
                tmp =  h264bsdDecodeExpGolombSigned(pStrmData,
                    pSeqParamSet->offsetForRefFrame + i);
                if (tmp != HANTRO_OK)
                    return(tmp);
            }
        }
    }

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
        &pSeqParamSet->numRefFrames);
    if (tmp != HANTRO_OK)
        return(tmp);
    if (pSeqParamSet->numRefFrames > MAX_NUM_REF_PICS)
    {
        ERROR_PRINT("num_ref_frames");
        return(HANTRO_NOK);
    }
    if (pSeqParamSet->numRefFrames == 0) {
        ALOGW("warning: numRefFrames is zero, it will be set to 1");
        pSeqParamSet->numRefFrames = 1;
    }

    tmp = h264bsdGetBits(pStrmData, 1);
    if (tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pSeqParamSet->gapsInFrameNumValueAllowedFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
    if (tmp != HANTRO_OK)
        return(tmp);
    pSeqParamSet->picWidthInMbs = value + 1;

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
    if (tmp != HANTRO_OK)
        return(tmp);
    pSeqParamSet->picHeightInMbs = value + 1;

    /* frame_mbs_only_flag, shall be 1 for baseline profile */
    tmp = h264bsdGetBits(pStrmData, 1);
    if (tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pSeqParamSet->frameMbsOnlyFlag = tmp;

    if (!pSeqParamSet->frameMbsOnlyFlag)
    {
        pSeqParamSet->mbAdaptiveFrameFieldFlag =
            h264bsdGetBits(pStrmData, 1);
        pSeqParamSet->picHeightInMbs *= 2;
    }

    /* direct_8x8_inference_flag */
    tmp = h264bsdGetBits(pStrmData, 1);
    if (tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pSeqParamSet->direct8x8InferenceFlag = tmp;

    tmp = h264bsdGetBits(pStrmData, 1);
    if (tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pSeqParamSet->frameCroppingFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

    if (pSeqParamSet->frameCroppingFlag)
    {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
            &pSeqParamSet->frameCropLeftOffset);
        if (tmp != HANTRO_OK)
            return(tmp);
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
            &pSeqParamSet->frameCropRightOffset);
        if (tmp != HANTRO_OK)
            return(tmp);
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
            &pSeqParamSet->frameCropTopOffset);
        if (tmp != HANTRO_OK)
            return(tmp);
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData,
            &pSeqParamSet->frameCropBottomOffset);
        if (tmp != HANTRO_OK)
            return(tmp);

        /* check that frame cropping params are valid, parameters shall
         * specify non-negative area within the original picture */
        if ( ( (i32)pSeqParamSet->frameCropLeftOffset >
               ( 8 * (i32)pSeqParamSet->picWidthInMbs -
                 ((i32)pSeqParamSet->frameCropRightOffset + 1) ) ) ||
             ( (i32)pSeqParamSet->frameCropTopOffset >
               ( 8 * (i32)pSeqParamSet->picHeightInMbs -
                 ((i32)pSeqParamSet->frameCropBottomOffset + 1) ) ) )
        {
            ERROR_PRINT("frame_cropping");
            return(HANTRO_NOK);
        }
    }

    /* check that image dimensions and levelIdc match */
    tmp = pSeqParamSet->picWidthInMbs * pSeqParamSet->picHeightInMbs;
    value = GetDpbSize(tmp, pSeqParamSet->levelIdc);
    if (value == INVALID_DPB_SIZE || pSeqParamSet->numRefFrames > value)
    {
        DEBUG_PRINT(("WARNING! Invalid DPB size based on SPS Level!\n"));
        DEBUG_PRINT(("WARNING! Using num_ref_frames =%d for DPB size!\n",
                        pSeqParamSet->numRefFrames));
        /* set maxDpbSize to 1 if numRefFrames is zero */
        value = pSeqParamSet->numRefFrames ? pSeqParamSet->numRefFrames : 1;
    }
    pSeqParamSet->maxDpbSize = value;

    tmp = h264bsdGetBits(pStrmData, 1);
    if (tmp == END_OF_STREAM)
        return(HANTRO_NOK);
    pSeqParamSet->vuiParametersPresentFlag = (tmp == 1) ? HANTRO_TRUE : HANTRO_FALSE;

    /* VUI */
    if (pSeqParamSet->vuiParametersPresentFlag)
    {
	    pSeqParamSet->vuiParameters = (vuiParameters_t *)malloc(sizeof(vuiParameters_t));

        if (pSeqParamSet->vuiParameters == NULL)
            return(MEMORY_ALLOCATION_ERROR);
        tmp = h264bsdDecodeVuiParameters(pStrmData, pSeqParamSet->vuiParameters);
        if (tmp == END_OF_STREAM)
        {
            pSeqParamSet->vuiParameters->bitstreamRestrictionFlag |= 1;
            pSeqParamSet->vuiParameters->maxDecFrameBuffering =
                pSeqParamSet->maxDpbSize;
        }
        else if (tmp != HANTRO_OK)
            return(tmp);
        /* check numReorderFrames and maxDecFrameBuffering */
        if (pSeqParamSet->vuiParameters->bitstreamRestrictionFlag)
        {
            if (pSeqParamSet->vuiParameters->numReorderFrames >
                    pSeqParamSet->vuiParameters->maxDecFrameBuffering ||
                pSeqParamSet->vuiParameters->maxDecFrameBuffering <
                    pSeqParamSet->numRefFrames ||
                pSeqParamSet->vuiParameters->maxDecFrameBuffering >
                    pSeqParamSet->maxDpbSize)
            {
                ERROR_PRINT("Not valid vuiParameters->bitstreamRestriction");
                /*return(HANTRO_NOK);*/
                pSeqParamSet->vuiParameters->numReorderFrames = pSeqParamSet->vuiParameters->maxDecFrameBuffering;
            }

            /* standard says that "the sequence shall not require a DPB with
             * size of more than max(1, maxDecFrameBuffering) */
            pSeqParamSet->maxDpbSize =
                MAX(1, pSeqParamSet->vuiParameters->maxDecFrameBuffering);
        }
    }

    if (mvcFlag)
    {
        if (pSeqParamSet->profileIdc == 118 || pSeqParamSet->profileIdc == 128)
        {
            /* bit_equal_to_one */
            tmp = h264bsdGetBits(pStrmData, 1);
            tmp = DecodeMvcExtension(pStrmData, pSeqParamSet);
            if (tmp != HANTRO_OK)
                return tmp;
        }

        /* additional_extension2_flag, shall be zero */
        tmp = h264bsdGetBits(pStrmData, 1);
        /* TODO: skip rest of the stuff if equal to 1 */
    }

    tmp = h264bsdRbspTrailingBits(pStrmData);

    /* ignore possible errors in trailing bits of parameters sets */
    return(HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: GetDpbSize

        Functional description:
            Get size of the DPB in frames. Size is determined based on the
            picture size and MaxDPB for the specified level. These determine
            how many pictures may fit into to the buffer. However, the size
            is also limited to a maximum of 16 frames and therefore function
            returns the minimum of the determined size and 16.

        Inputs:
            picSizeInMbs    number of macroblocks in the picture
            levelIdc        indicates the level

        Outputs:
            none

        Returns:
            size of the DPB in frames
            INVALID_DPB_SIZE when invalid levelIdc specified or picSizeInMbs
            is higher than supported by the level in question

------------------------------------------------------------------------------*/

u32 GetDpbSize(u32 picSizeInMbs, u32 levelIdc)
{

/* Variables */

    u32 tmp;
    u32 maxPicSizeInMbs;

/* Code */

    ASSERT(picSizeInMbs);

    /* use tmp as the size of the DPB in bytes, computes as 1024 * MaxDPB
     * (from table A-1 in Annex A) */
    switch (levelIdc)
    {
        case 10:
            tmp = 152064;
            maxPicSizeInMbs = 99;
            break;

        case 11:
            tmp = 345600;
            maxPicSizeInMbs = 396;
            break;

        case 12:
            tmp = 912384;
            maxPicSizeInMbs = 396;
            break;

        case 13:
            tmp = 912384;
            maxPicSizeInMbs = 396;
            break;

        case 20:
            tmp = 912384;
            maxPicSizeInMbs = 396;
            break;

        case 21:
            tmp = 1824768;
            maxPicSizeInMbs = 792;
            break;

        case 22:
            tmp = 3110400;
            maxPicSizeInMbs = 1620;
            break;

        case 30:
            tmp = 3110400;
            maxPicSizeInMbs = 1620;
            break;

        case 31:
            tmp = 6912000;
            maxPicSizeInMbs = 3600;
            break;

        case 32:
            tmp = 7864320;
            maxPicSizeInMbs = 5120;
            break;

        case 40:
            tmp = 12582912;
            maxPicSizeInMbs = 8192;
            break;

        case 41:
            tmp = 12582912;
            maxPicSizeInMbs = 8192;
            break;

        case 42:
            tmp = 12582912;
            maxPicSizeInMbs = 8192;
            break;

        case 50:
            /* standard says 42301440 here, but corrigendum "corrects" this to
             * 42393600 */
            tmp = 42393600;
            maxPicSizeInMbs = 22080;
            break;

        case 51:
            tmp = 70778880;
            maxPicSizeInMbs = 36864;
            break;

        default:
            return(INVALID_DPB_SIZE);
    }

    /* this is not "correct" return value! However, it results in error in
     * decoding and this was easiest place to check picture size */
    if (picSizeInMbs > maxPicSizeInMbs)
        return(INVALID_DPB_SIZE);

    tmp /= (picSizeInMbs*384);

    return(MIN(tmp, 16));

}

/*------------------------------------------------------------------------------

    Function name: h264bsdCompareSeqParamSets

        Functional description:
            Compare two sequence parameter sets.

        Inputs:
            pSps1   pointer to a sequence parameter set
            pSps2   pointer to another sequence parameter set

        Outputs:
            0       sequence parameter sets are equal
            1       otherwise

------------------------------------------------------------------------------*/

u32 h264bsdCompareSeqParamSets(seqParamSet_t *pSps1, seqParamSet_t *pSps2)
{

/* Variables */

    u32 i;

/* Code */

    ASSERT(pSps1);
    ASSERT(pSps2);

    /* first compare parameters whose existence does not depend on other
     * parameters and only compare the rest of the params if these are equal */
    if (pSps1->profileIdc        == pSps2->profileIdc &&
        pSps1->levelIdc          == pSps2->levelIdc &&
        pSps1->maxFrameNum       == pSps2->maxFrameNum &&
        pSps1->picOrderCntType   == pSps2->picOrderCntType &&
        pSps1->numRefFrames      == pSps2->numRefFrames &&
        pSps1->gapsInFrameNumValueAllowedFlag ==
            pSps2->gapsInFrameNumValueAllowedFlag &&
        pSps1->picWidthInMbs     == pSps2->picWidthInMbs &&
        pSps1->picHeightInMbs    == pSps2->picHeightInMbs &&
        pSps1->frameCroppingFlag == pSps2->frameCroppingFlag &&
        pSps1->frameMbsOnlyFlag  == pSps2->frameMbsOnlyFlag &&
        pSps1->vuiParametersPresentFlag == pSps2->vuiParametersPresentFlag &&
        pSps1->scalingMatrixPresentFlag == pSps2->scalingMatrixPresentFlag)
    {

        if (pSps1->picOrderCntType == 0)
        {
            if (pSps1->maxPicOrderCntLsb != pSps2->maxPicOrderCntLsb)
                return 1;
        }
        else if (pSps1->picOrderCntType == 1)
        {
            if (pSps1->deltaPicOrderAlwaysZeroFlag !=
                    pSps2->deltaPicOrderAlwaysZeroFlag ||
                pSps1->offsetForNonRefPic != pSps2->offsetForNonRefPic ||
                pSps1->offsetForTopToBottomField !=
                    pSps2->offsetForTopToBottomField ||
                pSps1->numRefFramesInPicOrderCntCycle !=
                    pSps2->numRefFramesInPicOrderCntCycle)
            {
                return 1;
            }
            else
            {
                for (i = 0; i < pSps1->numRefFramesInPicOrderCntCycle; i++)
                    if (pSps1->offsetForRefFrame[i] !=
                        pSps2->offsetForRefFrame[i])
                    {
                        return 1;
                    }
            }
        }
        if (pSps1->frameCroppingFlag)
        {
            if (pSps1->frameCropLeftOffset   != pSps2->frameCropLeftOffset ||
                pSps1->frameCropRightOffset  != pSps2->frameCropRightOffset ||
                pSps1->frameCropTopOffset    != pSps2->frameCropTopOffset ||
                pSps1->frameCropBottomOffset != pSps2->frameCropBottomOffset)
            {
                return 1;
            }
        }

        if (!pSps1->frameMbsOnlyFlag)
            if (pSps1->mbAdaptiveFrameFieldFlag !=
                pSps2->mbAdaptiveFrameFieldFlag)
                return 1;

        /* copy scaling matrices if used */
        if (pSps1->scalingMatrixPresentFlag)
        {
            u32 i, j;

            for (i = 0; i < 8; i++)
            {
                pSps2->scalingListPresent[i] = pSps1->scalingListPresent[i];
                for (j = 0; j < 64; j++)
                    pSps2->scalingList[i][j] = pSps1->scalingList[i][j];
            }
        }

        return 0;
    }

    return 1;
}

u32 DecodeMvcExtension(strmData_t *pStrmData, seqParamSet_t *pSeqParamSet)
{

/* Variables */

    u32 tmp, i, j, k;
    u32 value, tmpCount, tmpCount1, tmpCount2;
    hrdParameters_t hrdParams;

/* Code */

    ASSERT(pStrmData);
    ASSERT(pSeqParamSet);

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
    if (tmp != HANTRO_OK)
        return(tmp);
    pSeqParamSet->mvc.numViews = value + 1;
    /*
    if (pSeqParamSet->mvc.numViews > MAX_NUM_VIEWS)
        return(HANTRO_NOK);
    */

    /* view_id */
    for (i = 0; i < pSeqParamSet->mvc.numViews; i++)
    {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if (tmp != HANTRO_OK)
            return(tmp);
        if (i < MAX_NUM_VIEWS)
            pSeqParamSet->mvc.viewId[i] = value;
    }

    for (i = 1; i < pSeqParamSet->mvc.numViews; i++)
    {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if (tmp != HANTRO_OK)
            return(tmp);
        /*pSeqParamSet->mvc.numAnchorRefsL0[i] = value;*/
        tmpCount = value;
        for (j = 0; j < tmpCount; j++)
        {
            tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
            if (tmp != HANTRO_OK)
                return(tmp);
            /*pSeqParamSet->mvc.anchorRefL0[i][j] = value;*/
        }

        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if (tmp != HANTRO_OK)
            return(tmp);
        /*pSeqParamSet->mvc.numNonAnchorRefsL0[i] = value;*/
        tmpCount = value;
        for (j = 0; j < tmpCount; j++)
        {
            tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
            if (tmp != HANTRO_OK)
                return(tmp);
            /*pSeqParamSet->mvc.nonAnchorRefL0[i][j] = value;*/
        }
    }

    for (i = 1; i < pSeqParamSet->mvc.numViews; i++)
    {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if (tmp != HANTRO_OK)
            return(tmp);
        /*pSeqParamSet->mvc.numAnchorRefsL1[i] = value;*/
        tmpCount = value;
        for (j = 0; j < tmpCount; j++)
        {
            tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
            if (tmp != HANTRO_OK)
                return(tmp);
            /*pSeqParamSet->mvc.anchorRefL1[i][j] = value;*/
        }

        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if (tmp != HANTRO_OK)
            return(tmp);
        /*pSeqParamSet->mvc.numNonAnchorRefsL1[i] = value;*/
        tmpCount = value;
        for (j = 0; j < tmpCount; j++)
        {
            tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
            if (tmp != HANTRO_OK)
                return(tmp);
            /*pSeqParamSet->mvc.nonAnchorRefL1[i][j] = value;*/
        }
    }

    /* num_level_values_signalled_minus1 */
    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
    if (tmp != HANTRO_OK)
        return(tmp);
    tmpCount = value + 1;
    for (i = 0; i < tmpCount; i++)
    {
        /* level_idc */
        tmp = h264bsdGetBits(pStrmData, 8);
        /* num_applicable_ops_minus1 */
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if (tmp != HANTRO_OK)
            return(tmp);
        tmpCount1 = value + 1;
        for (j = 0; j < tmpCount1; j++)
        {
            /* applicable_op_temporal_id  */
            tmp = h264bsdGetBits(pStrmData, 3);
            /* applicable_op_num_target_views_minus1 */
            tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
            if (tmp != HANTRO_OK)
                return(tmp);
            tmpCount2 = value + 1;
            for (k = 0; k < tmpCount2; k++)
            {
                /* applicable_op_target_view_id */
                tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
            }
            /* applicable_op_num_views_minus1 */
            tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
            if (tmp != HANTRO_OK)
                return(tmp);
        }
    }

    /* mvc_vui_parameters_present_flag */
    tmp = h264bsdGetBits(pStrmData, 1);
    if (tmp == 1)
    {
        /* vui_mvc_num_ops_minus1 */
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if (tmp != HANTRO_OK)
            return(tmp);
        tmpCount = value + 1;
        for (i = 0; i < tmpCount; i++)
        {
            /* vui_mvc_temporal_id  */
            tmp = h264bsdGetBits(pStrmData, 3);
            /* vui_mvc_num_target_output_views_minus1 */
            tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
            if (tmp != HANTRO_OK)
                return(tmp);
            tmpCount1 = value + 1;
            for (k = 0; k < tmpCount1; k++)
            {
                /* vui_mvc_view_id */
                tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
            }
            /* vui_mvc_timing_info_present_flag  */
            tmp = h264bsdGetBits(pStrmData, 1);
            if (tmp == 1)
            {
                /* vui_mvc_num_units_in_tick  */
                tmp = h264bsdShowBits(pStrmData,32);
                if (h264bsdFlushBits(pStrmData, 32) == END_OF_STREAM)
                    return(END_OF_STREAM);
                /* vui_mvc_time_scale  */
                tmp = h264bsdShowBits(pStrmData,32);
                if (h264bsdFlushBits(pStrmData, 32) == END_OF_STREAM)
                    return(END_OF_STREAM);
                /* vui_mvc_fixed_frame_rate_flag */
                tmp = h264bsdGetBits(pStrmData, 1);
            }

            j = 0;
            /* vui_mvc_nal_hrd_parameters_present_flag */
            tmp = h264bsdGetBits(pStrmData, 1);
            if (tmp == 1)
            {
                j = 1;
                tmp = h264bsdDecodeHrdParameters(pStrmData, &hrdParams);
            }

            /* vui_mvc_vcl_hrd_parameters_present_flag */
            tmp = h264bsdGetBits(pStrmData, 1);
            if (tmp == 1)
            {
                j = 1;
                tmp = h264bsdDecodeHrdParameters(pStrmData, &hrdParams);
            }

            if (j)
            {
                /* vui_mvc_low_delay_hrd_flag */
                tmp = h264bsdGetBits(pStrmData, 1);
            }
            /* vui_mvc_pic_struct_present_flag */
            tmp = h264bsdGetBits(pStrmData, 1);
        }

    }

    return(HANTRO_OK);

}
