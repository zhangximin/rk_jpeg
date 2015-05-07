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
--  Abstract : Application Programming Interface (API) functions
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264decapi.c,v $
--  $Date: 2010/05/27 09:32:48 $
--  $Revision: 1.242 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "basetype.h"
#include "h264hwd_container.h"
#include "h264decapi.h"
#include "h264hwd_decoder.h"
#include "h264hwd_util.h"
#include "h264hwd_dpb.h"
#include "h264hwd_asic.h"
#include "h264hwd_regdrv.h"
#include "h264hwd_byte_stream.h"
#include "deccfg.h"
//#include "h264_pp_multibuffer.h"
#include "vpu_drv.h"
#include "vpu.h"
//#include <utils/Log.h>
#include <stdio.h>

#include "dwl.h"

#define ALOGE	printf

namespace android {

/*------------------------------------------------------------------------------
       Version Information - DO NOT CHANGE!
------------------------------------------------------------------------------*/

#define H264DEC_MAJOR_VERSION 1
#define H264DEC_MINOR_VERSION 1

#define H264DEC_BUILD_MAJOR 2
#define H264DEC_BUILD_MINOR 177
#define H264DEC_SW_BUILD ((H264DEC_BUILD_MAJOR * 1000) + H264DEC_BUILD_MINOR)

/*
 * H264DEC_TRACE         Trace H264 Decoder API function calls. H264DecTrace
 *                       must be implemented externally.
 * H264DEC_EVALUATION    Compile evaluation version, restricts number of frames
 *                       that can be decoded
 */

#define H264DEC_TRACE

#ifdef H264DEC_TRACE
#define DEC_API_TRC(str)    H264DecTrace(str)
#else
#define DEC_API_TRC(str)    do{}while(0)
#endif

static void h264UpdateAfterPictureDecode(decContainer_t * pDecCont);
static u32 h264SpsSupported(const decContainer_t * pDecCont);
static u32 h264PpsSupported(const decContainer_t * pDecCont);
static u32 h264StreamIsBaseline(const decContainer_t * pDecCont);

static u32 h264AllocateResources(decContainer_t * pDecCont);
//static void h264InitPicFreezeOutput(decContainer_t * pDecCont, u32 fromOldDpb);
static void h264GetSarInfo(const storage_t * pStorage,
                           u32 * sar_width, u32 * sar_height);
extern void h264decErrorConCealMent(decContainer_t * pDecCont);

/*------------------------------------------------------------------------------

    Function: H264DecInit()

        Functional description:
            Initialize decoder software. Function reserves memory for the
            decoder instance and calls h264bsdInit to initialize the
            instance data.

        Inputs:
            noOutputReordering  flag to indicate decoder that it doesn't have
                                to try to provide output pictures in display
                                order, saves memory
            useVideoFreezeConcealment
                                flag to enable error concealment method where
                                decoding starts at next intra picture after
                                error in bitstream.
            useDisplaySmooothing
                                flag to enable extra buffering in DPB output
                                so that application may read output pictures
                                one by one
        Outputs:
            decInst             pointer to initialized instance is stored here

        Returns:
            H264DEC_OK        successfully initialized the instance
            H264DEC_INITFAIL  initialization failed
            H264DEC_PARAM_ERROR invalid parameters
            H264DEC_MEMFAIL   memory allocation failed
            H264DEC_DWL_ERROR error initializing the system interface
------------------------------------------------------------------------------*/

H264DecRet H264DecInit(decContainer_t *H264deccont, u32 useVideoFreezeConcealment)
{

    /*@null@ */ decContainer_t *pDecCont;
    /*@null@ */ int socket;

    VPUHwDecConfig_t hwCfg;
    DEC_API_TRC("H264DecInit#\n");

    /* check that right shift on negative numbers is performed signed */
    /*lint -save -e* following check causes multiple lint messages */
#if (((-1) >> 1) != (-1))
#error Right bit-shifting (>>) does not preserve the sign
#endif
    /*lint -restore */

    socket = VPUClientInit(VPU_DEC);
    if(socket < 0)
    {
        DEC_API_TRC("H264DecInit# ERROR: DWL Init failed\n");
        return (H264DEC_DWL_ERROR);
    }

    /* check that H.264 decoding supported in HW */
    memset(&hwCfg, 0, sizeof(DWLHwConfig_t));
    if (VPUClientGetHwCfg(socket, (RK_U32*)&hwCfg, sizeof(hwCfg)))
    {
        DEC_API_TRC("H264DecInit# Get HwCfg failed\n");
        VPUClientRelease(socket);
        return (H264DEC_DWL_ERROR);
    }

    if(!hwCfg.h264Support)
    {
        DEC_API_TRC("H264DecInit# ERROR: H264 not supported in HW\n");
        return H264DEC_FORMAT_NOT_SUPPORTED;
    }

    pDecCont = H264deccont;//(decContainer_t *) DWLmalloc(sizeof(decContainer_t));

    if(pDecCont == NULL)
    {
        (void) VPUClientRelease(socket);

        DEC_API_TRC("H264DecInit# ERROR: Memory allocation failed\n");
        return (H264DEC_MEMFAIL);
    }

    memset(pDecCont, 0, sizeof(decContainer_t));
    pDecCont->socket = socket;

    h264bsdInit(&pDecCont->storage);

    pDecCont->decStat = H264DEC_INITIALIZED;

    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_MODE, DEC_X170_MODE_H264);

    /* these parameters are defined in deccfg.h */

    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_OUT_ENDIAN,
                   DEC_X170_OUTPUT_PICTURE_ENDIAN);
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_IN_ENDIAN,
                   DEC_X170_INPUT_DATA_ENDIAN);
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_STRENDIAN_E,
                   DEC_X170_INPUT_STREAM_ENDIAN);
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_OUT_TILED_E,
                   DEC_X170_OUTPUT_FORMAT);
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_MAX_BURST,
                   DEC_X170_BUS_BURST_LENGTH);
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_SCMD_DIS,
                   DEC_X170_SCMD_DISABLE);
    DEC_SET_APF_THRESHOLD(pDecCont->h264Regs);
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_LATENCY,
                   DEC_X170_LATENCY_COMPENSATION);
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_DATA_DISC_E,
                   DEC_X170_DATA_DISCARD_ENABLE);
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_OUTSWAP32_E,
                   DEC_X170_OUTPUT_SWAP_32_ENABLE);
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_INSWAP32_E,
                   DEC_X170_INPUT_DATA_SWAP_32_ENABLE);
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_STRSWAP32_E,
                   DEC_X170_INPUT_STREAM_SWAP_32_ENABLE);

#if( DEC_X170_HW_TIMEOUT_INT_ENA  != 0)
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_TIMEOUT_E, 1);
#else
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_TIMEOUT_E, 0);
#endif

#if( DEC_X170_INTERNAL_CLOCK_GATING != 0)
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_CLK_GATE_E, 1);
#else
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_CLK_GATE_E, 0);
#endif

#if( DEC_X170_USING_IRQ  == 0)
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_IRQ_DIS, 1);
#else
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_IRQ_DIS, 0);
#endif

    /* set AXI RW IDs */
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_AXI_RD_ID,
        (DEC_X170_AXI_ID_R & 0xFFU));
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_AXI_WR_ID,
        (DEC_X170_AXI_ID_W & 0xFFU));

    /* Set prediction filter taps */
    SetDecRegister(pDecCont->h264Regs, HWIF_PRED_BC_TAP_0_0, 1);
    SetDecRegister(pDecCont->h264Regs, HWIF_PRED_BC_TAP_0_1, (u32)(-5));
    SetDecRegister(pDecCont->h264Regs, HWIF_PRED_BC_TAP_0_2, 20);

    /* save HW version so we dont need to check it all the time when deciding the control stuff */
    pDecCont->h264ProfileSupport = hwCfg.h264Support;

    /* save ref buffer support status */
    pDecCont->refBufSupport = hwCfg.refBufSupport;
//#ifndef _DISABLE_PIC_FREEZE
//    pDecCont->storage.intraFreeze = useVideoFreezeConcealment;
//#endif
    pDecCont->storage.pictureBroken = HANTRO_FALSE;

    pDecCont->maxDecPicWidth = hwCfg.maxDecPicWidth;    /* max decodable picture width */

    pDecCont->checksum = pDecCont;  /* save instance as a checksum */

    DEC_API_TRC("H264DecInit# OK\n");

    return (H264DEC_OK);
}

/*------------------------------------------------------------------------------

    Function: H264DecGetInfo()

        Functional description:
            This function provides read access to decoder information. This
            function should not be called before H264DecDecode function has
            indicated that headers are ready.

        Inputs:
            decInst     decoder instance

        Outputs:
            pDecInfo    pointer to info struct where data is written

        Returns:
            H264DEC_OK            success
            H264DEC_PARAM_ERROR     invalid parameters
            H264DEC_HDRS_NOT_RDY  information not available yet

------------------------------------------------------------------------------*/
H264DecRet H264DecGetInfo(decContainer_t *pDecCont, H264DecInfo * pDecInfo)
{
    u32 croppingFlag;
    storage_t *pStorage;

    DEC_API_TRC("H264DecGetInfo#");

    if(pDecCont == NULL || pDecInfo == NULL)
    {
        DEC_API_TRC("H264DecGetInfo# ERROR: decInst or pDecInfo is NULL\n");
        return (H264DEC_PARAM_ERROR);
    }

    /* Check for valid decoder instance */
    if(pDecCont->checksum != pDecCont)
    {
        DEC_API_TRC("H264DecGetInfo# ERROR: Decoder not initialized\n");
        return (H264DEC_NOT_INITIALIZED);
    }

    pStorage = &pDecCont->storage;

    if(pStorage->activeSps == NULL || pStorage->activePps == NULL)
    {
        DEC_API_TRC("H264DecGetInfo# ERROR: Headers not decoded yet\n");
        return (H264DEC_HDRS_NOT_RDY);
    }

    /* h264bsdPicWidth and -Height return dimensions in macroblock units,
     * picWidth and -Height in pixels */
    pDecInfo->picWidth = h264bsdPicWidth(pStorage) << 4;
    pDecInfo->picHeight = h264bsdPicHeight(pStorage) << 4;
    pDecInfo->videoRange = h264bsdVideoRange(pStorage);
    pDecInfo->matrixCoefficients = h264bsdMatrixCoefficients(pStorage);
    pDecInfo->monoChrome = h264bsdIsMonoChrome(pStorage);
    pDecInfo->interlacedSequence = pStorage->activeSps->frameMbsOnlyFlag == 0 ? 1 : 0;
    pDecInfo->picBuffSize = pStorage->dpb->dpbSize + 1;

    h264GetSarInfo(pStorage, &pDecInfo->sarWidth, &pDecInfo->sarHeight);

    h264bsdCroppingParams(pStorage,
                          &croppingFlag,
                          &pDecInfo->cropParams.cropLeftOffset,
                          &pDecInfo->cropParams.cropOutWidth,
                          &pDecInfo->cropParams.cropTopOffset,
                          &pDecInfo->cropParams.cropOutHeight);

    if(croppingFlag == 0)
    {
        pDecInfo->cropParams.cropLeftOffset = 0;
        pDecInfo->cropParams.cropTopOffset = 0;
        pDecInfo->cropParams.cropOutWidth = pDecInfo->picWidth;
        pDecInfo->cropParams.cropOutHeight = pDecInfo->picHeight;
    }

    if(pDecInfo->monoChrome)
    {
        pDecInfo->outputFormat = H264DEC_YUV400;
    }
    else if(GetDecRegister(pDecCont->h264Regs, HWIF_DEC_OUT_TILED_E))
    {
        pDecInfo->outputFormat = H264DEC_TILED_YUV420;
    }
    else
    {
        pDecInfo->outputFormat = H264DEC_SEMIPLANAR_YUV420;
    }

    DEC_API_TRC("H264DecGetInfo# OK\n");

    return (H264DEC_OK);
}

/*------------------------------------------------------------------------------

    Function: H264DecRelease()

        Functional description:
            Release the decoder instance. Function calls h264bsdShutDown to
            release instance data and frees the memory allocated for the
            instance.

        Inputs:
            decInst     Decoder instance

        Outputs:
            none

        Returns:
            none

------------------------------------------------------------------------------*/

void H264DecRelease(decContainer_t *pDecCont)
{
    DEC_API_TRC("H264DecRelease#\n");

    if(pDecCont == NULL)
    {
        DEC_API_TRC("H264DecRelease# ERROR: decInst == NULL\n");
        return;
    }

    /* Check for valid decoder instance */
    if(pDecCont->checksum != pDecCont)
    {
        DEC_API_TRC("H264DecRelease# ERROR: Decoder not initialized\n");
        return;
    }

    VPUClientRelease(pDecCont->socket);

    for (u32 i = 0; i < MAX_NUM_SEQ_PARAM_SETS; i++) {
        if (pDecCont->storage.sps[i]) {
            h264bsdFreeSeqParamSetById(&pDecCont->storage, i);
        }
    }


    h264bsdFreeDpb(pDecCont->storage.dpbs[0]);

    ReleaseAsicBuffers(pDecCont->asicBuff);

    pDecCont->checksum = NULL;

    DEC_API_TRC("H264DecRelease# OK\n");

    return;
}

/*------------------------------------------------------------------------------

    Function: H264DecDecode

        Functional description:
            Decode stream data. Calls h264bsdDecode to do the actual decoding.

        Input:
            decInst     decoder instance
            pInput      pointer to input struct

        Outputs:
            pOutput     pointer to output struct

        Returns:
            H264DEC_NOT_INITIALIZED   decoder instance not initialized yet
            H264DEC_PARAM_ERROR         invalid parameters

            H264DEC_STRM_PROCESSED    stream buffer decoded
            H264DEC_HDRS_RDY    headers decoded, stream buffer not finished
            H264DEC_PIC_DECODED decoding of a picture finished
            H264DEC_STRM_ERROR  serious error in decoding, no valid parameter
                                sets available to decode picture data
            H264DEC_PENDING_FLUSH   next invocation of H264DecDecode() function
                                    flushed decoded picture buffer, application
                                    needs to read all output pictures (using
                                    H264DecNextPicture function) before calling
                                    H264DecDecode() again. Used only when
                                    useDisplaySmoothing was enabled in init.

            H264DEC_HW_BUS_ERROR    decoder HW detected a bus error
            H264DEC_SYSTEM_ERROR    wait for hardware has failed
            H264DEC_MEMFAIL         decoder failed to allocate memory
            H264DEC_DWL_ERROR   System wrapper failed to initialize
            H264DEC_HW_TIMEOUT  HW timeout
            H264DEC_HW_RESERVED HW could not be reserved
------------------------------------------------------------------------------*/
H264DecRet H264DecDecode(decContainer_t *pDecCont,  const H264DecInput * pInput,
                         H264DecOutput * pOutput)
{
    u32 strmLen;
    const u8 *tmpStream;
    H264DecRet returnValue = H264DEC_STRM_PROCESSED;

    PRINTFLINE
    /* Check that function input parameters are valid */
    if(pInput == NULL || pOutput == NULL || pDecCont == NULL)
    {
        DEC_API_TRC("H264DecDecode# ERROR: NULL arg(s)\n");
        return (H264DEC_PARAM_ERROR);
    }

    /* Check for valid decoder instance */
    if(pDecCont->checksum != pDecCont)
    {
        DEC_API_TRC("H264DecDecode# ERROR: Decoder not initialized\n");
        return (H264DEC_NOT_INITIALIZED);
    }

    if(pInput->dataLen == 0 ||
       pInput->dataLen > DEC_X170_MAX_STREAM ||
       X170_CHECK_VIRTUAL_ADDRESS(pInput->pStream) ||
       X170_CHECK_BUS_ADDRESS(pInput->streamBusAddress))
    {
        printf("H264DecDecode# ERROR: Invalid arg value,    pInput->dataLen:%d  DEC_X170_MAX_STREAM:%d \n", pInput->dataLen, DEC_X170_MAX_STREAM);
        return H264DEC_PARAM_ERROR;
    }

    pDecCont->streamPosUpdated = 0;
    pDecCont->hwStreamStartBus = pInput->streamBusAddress;
    pDecCont->pHwStreamStart = pInput->pStream;
    strmLen = pDecCont->hwLength = pInput->dataLen;
    tmpStream = pInput->pStream;

    do
    {
        RET_E decResult;
        u32 numReadBytes = 0;
        storage_t *pStorage = &pDecCont->storage;

        switch (pDecCont->decStat) {
        case H264DEC_NEW_HEADERS : {
            decResult = H264BSD_HDRS_RDY;
            pDecCont->decStat = H264DEC_INITIALIZED;
        } break;
        default : {
            decResult = (RET_E)h264bsdDecode(pDecCont, tmpStream, strmLen, pInput->picId, &numReadBytes);

            if(H264BSD_MEMFAIL == decResult)
                return H264DEC_MEMFAIL;

            ASSERT(numReadBytes <= strmLen);

            tmpStream += numReadBytes;
            strmLen -= numReadBytes;
        } break;
        }

        switch (decResult)
        {
        case H264BSD_HDRS_RDY: {
            if(pStorage->dpb->flushed && pStorage->dpb->numOut)
            {
                /* output first all DPB stored pictures */
                pStorage->dpb->flushed = 0;
                pDecCont->decStat = H264DEC_NEW_HEADERS;
                /* if display smoothing used -> indicate that all pictures
                 * have to be read out */
                /*
                if (pStorage->dpb->totBuffers > pStorage->dpb->dpbSize + 1)
                    returnValue = H264DEC_PENDING_FLUSH;
                else
                    returnValue = H264DEC_PIC_DECODED;
                */
                returnValue = H264DEC_PENDING_FLUSH;

                DEC_API_TRC("H264DecDecode# H264DEC_PIC_DECODED (DPB flush caused by new SPS)\n");
                strmLen = 0;
                break;
            }

            if (!h264SpsSupported(pDecCont))
            {
                pStorage->activeSpsId = MAX_NUM_SEQ_PARAM_SETS;
                pStorage->activePpsId = MAX_NUM_PIC_PARAM_SETS;

                const seqParamSet_t *sps = pDecCont->storage.activeSps;
                if ((sps->picWidthInMbs * sps->picHeightInMbs) > ((1920 >> 4) * (1088 >> 4))) {
                    returnValue = H264DEC_SIZE_TOO_LARGE;
                    DEC_API_TRC("H264DecDecode# H264DEC_SIZE_TOO_LARGE\n");
                } else {
                    returnValue = H264DEC_STREAM_NOT_SUPPORTED;
                    DEC_API_TRC("H264DecDecode# H264DEC_STREAM_NOT_SUPPORTED\n");
                }
            }
            else if((h264bsdAllocateSwResources(pStorage, (pDecCont->h264ProfileSupport == H264_HIGH_PROFILE) ? 1 : 0, pDecCont->ts_en) != 0) ||
                    (h264AllocateResources(pDecCont) != 0))
            {
                /* signal that decoder failed to init parameter sets */
                /* TODO: miten viewit */
                pStorage->activeSpsId = MAX_NUM_SEQ_PARAM_SETS;
                pStorage->activePpsId = MAX_NUM_PIC_PARAM_SETS;

                returnValue = H264DEC_MEMFAIL;
                DEC_API_TRC("H264DecDecode# H264DEC_MEMFAIL\n");
            }
            else
            {
                if((pStorage->activePps->numSliceGroups != 1) && (h264StreamIsBaseline(pDecCont) == 0))
                {
                    pStorage->activeSpsId = MAX_NUM_SEQ_PARAM_SETS;
                    pStorage->activePpsId = MAX_NUM_PIC_PARAM_SETS;

                    returnValue = H264DEC_STREAM_NOT_SUPPORTED;
                    DEC_API_TRC("H264DecDecode# H264DEC_STREAM_NOT_SUPPORTED, FMO in Main/High Profile\n");
                }

                pDecCont->asicBuff->enableDmvAndPoc = 0;
                pStorage->dpb->interlaced = (pStorage->activeSps->frameMbsOnlyFlag == 0) ? 1 : 0;

                /* FMO always decoded in rlc mode */
                if (pStorage->activePps->numSliceGroups != 1)
                {
                    /* set to uninit state */
                    pStorage->activeSpsId = MAX_NUM_SEQ_PARAM_SETS;
                    pStorage->activeViewSpsId[0] = pStorage->activeViewSpsId[1] = MAX_NUM_SEQ_PARAM_SETS;
                    pStorage->activePpsId = MAX_NUM_PIC_PARAM_SETS;
                    pStorage->picStarted = HANTRO_FALSE;
                    pDecCont->decStat = H264DEC_INITIALIZED;

                    //pDecCont->rlcMode = 1;
                    ALOGE("FMO found\n");
                    pStorage->prevBufNotFinished = HANTRO_FALSE;
                    DEC_API_TRC("H264DecDecode# H264DEC_ADVANCED_TOOLS, FMO\n");

                    returnValue = H264DEC_ADVANCED_TOOLS;
                }
                else
                {
                    /* enable direct MV writing and POC tables for
                     * high/main streams.
                     * enable it also for any "baseline" stream which have
                     * main/high tools enabled */
                    if((pStorage->activeSps->profileIdc > 66 &&
                        pStorage->activeSps->constrained_set0_flag == 0) ||
                       (h264StreamIsBaseline(pDecCont) == 0))
                    {
                        pDecCont->asicBuff->enableDmvAndPoc = 1;
                    }

                    DEC_API_TRC("H264DecDecode# H264DEC_HDRS_RDY\n");
                    returnValue = H264DEC_HDRS_RDY;
                }
            }

            //if (!pStorage->view)
            {
                /* reset strmLen only for phy_base view -> no HDRS_RDY to
                 * application when param sets activated for stereo view */
                strmLen = 0;
                //pDecCont->storage.secondField = 0;
            }
        } break;

        case H264BSD_PIC_RDY: {
            u32 asic_status;
            DecAsicBuffers_t *pAsicBuff = pDecCont->asicBuff;
            dpbStorage_t *dpb = pStorage->dpb;
            dpbPicture_t **buffer = dpb->buffer;
			u32 tmpAddr, decReval=0;
			
            if (dpb->currentOut == dpb->previousOut) {
                dpb->currentOut->TimeLow  = pInput->TimeLow;
                dpb->currentOut->TimeHigh = pInput->TimeHigh;
            }

            /* setup the reference frame list; just at picture start */
            /* list in reorder */
            if(!h264PpsSupported(pDecCont))
            {
                pStorage->activeSpsId = MAX_NUM_SEQ_PARAM_SETS;
                pStorage->activePpsId = MAX_NUM_PIC_PARAM_SETS;

                returnValue = H264DEC_STREAM_NOT_SUPPORTED;
                DEC_API_TRC("H264DecDecode# H264DEC_STREAM_NOT_SUPPORTED, Main/High Profile tools detected\n");
                goto end;
            }

			tmpAddr = 0;
			
			for (u32 i = 0; i < dpb->dpbSize; i++)
			{
				if (buffer[i] && buffer[i]->data)
				{
					if((buffer[i]->status[0] !=EMPTY) && (buffer[i]->status[1] != EMPTY))
					{
						tmpAddr = buffer[i]->data->phy_addr;
						break;
					}
				}
			}

			if(tmpAddr == 0)
			{
				tmpAddr = dpb->currentPhyAddr;
			}
			
            for (u32 i = 0; i < dpb->dpbSize; i++)
            {
                if (buffer[i] && buffer[i]->data)
	                pAsicBuff->refPicList[i] = buffer[i]->data->phy_addr;
            	else
                    pAsicBuff->refPicList[i] = tmpAddr;//dpb->currentPhyAddr;
                    
				if (buffer[i] && buffer[i]->data)
					DPBDEBUG("pAsicBuff->refPicList[%d], poc0=%d, poc1=%d\n", i, buffer[i]->picOrderCnt[0], buffer[i]->picOrderCnt[1]);
				else
					DPBDEBUG("pAsicBuff->refPicList[%d]=0x%x\n", i, dpb->currentPhyAddr);
            }

            pAsicBuff->maxRefFrm    = dpb->maxRefFrames;
            pAsicBuff->outPhyAddr   = dpb->currentPhyAddr;
            pAsicBuff->outVirAddr   = dpb->currentVirAddr;
            pAsicBuff->chromaQpIndexOffset = pStorage->activePps->chromaQpIndexOffset;
            pAsicBuff->chromaQpIndexOffset2 = pStorage->activePps->chromaQpIndexOffset2;
            pAsicBuff->filterDisable = 0;

            h264bsdDecodePicOrderCnt(pStorage->poc,
                                     pStorage->activeSps,
                                     pStorage->sliceHeader,
                                     pStorage->prevNalUnit);
            dpb->minusPoc = pStorage->poc->minusPoc;
            H264SetupVlcRegs(pDecCont);

            DEBUG_PRINT(("Save DPB status\n"));
            /* we trust our memcpy; ignore return value */
            memcpy(&pStorage->dpb[1], &pStorage->dpb[0], sizeof(*pStorage->dpb));

            DEBUG_PRINT(("Save POC status\n"));
            memcpy(&pStorage->poc[1], &pStorage->poc[0], sizeof(*pStorage->poc));

			/* determine initial reference picture lists */
    		H264InitRefPicList(pDecCont);
			
            /* run asic and react to the status */
            asic_status = H264RunAsic(pDecCont, pAsicBuff);

			h264decErrorConCealMent(pDecCont);	//ÐÞ²¹error
				
            /* create output picture list */
        	h264UpdateAfterPictureDecode(pDecCont);

			if((pDecCont->dommco5) && pDecCont->ts_en)
			{
				markErrorDpbSlot(pStorage->dpb);
				Mmcop5(pStorage->dpb, 0);
				pStorage->dpb->fieldmark = 0;
				pDecCont->dommco5 = 0;
				decReval = 1;
			}
			
            if(asic_status == X170_DEC_SYSTEM_ERROR)
            {
                DEC_API_TRC("H264DecDecode# H264DEC_SYSTEM_ERROR\n");
                returnValue = H264DEC_SYSTEM_ERROR;
                goto end;
            }

            /* Handle possible common HW error situations */
            if (asic_status &
                (DEC_X170_IRQ_BUS_ERROR
                |DEC_X170_IRQ_TIMEOUT
                |DEC_X170_IRQ_BUFFER_EMPTY
                |DEC_X170_IRQ_ASO
                |DEC_X170_IRQ_STREAM_ERROR))
            {
                DEBUG_PRINT(("reset picStarted\n"));
                pDecCont->storage.picStarted = HANTRO_FALSE;

                //pOutput->dataLeft = pInput->dataLen;
                pOutput->dataLeft = 0;


                DEBUG_PRINT(("recover DPB status\n"));
                /* we trust our memcpy; ignore return value */
                memcpy(&pStorage->dpb[0], &pStorage->dpb[1], sizeof(*pStorage->dpb));
                DEBUG_PRINT(("recover POC status\n"));
                memcpy(&pStorage->poc[0], &pStorage->poc[1], sizeof(*pStorage->poc));


                pDecCont->streamPosUpdated = 0;
                pStorage->dpb->lastPicOrderCnt = 0x7FFFFFF0;
                ALOGE("HW found stream error!\n");

                returnValue = H264DEC_NUMSLICE_ERROR;
                goto end;
            }

            if (IS_IDR_NAL_UNIT(pStorage->prevNalUnit) )
            {
                pDecCont->storage.pictureBroken = HANTRO_FALSE;
            }

            if(0)//( pDecCont->storage.activePps->entropyCodingModeFlag)
            {
                u32 tmp;

                strmData_t strmTmp = *pDecCont->storage.strm;
                tmp = pDecCont->pHwStreamStart-pInput->pStream;
                strmTmp.pStrmCurrPos = pDecCont->pHwStreamStart;
                strmTmp.strmBuffReadBits = 8*tmp;
                strmTmp.bitPosInWord = 0;
                strmTmp.strmBuffSize = pInput->dataLen;
                tmp = h264CheckCabacZeroWords( &strmTmp );
                if( tmp != HANTRO_OK )
                {
                    DEBUG_PRINT(("Error decoding CABAC zero words\n"));
                    {
                        strmData_t *strm = pDecCont->storage.strm;
                        const u8 *next =
                            h264bsdFindNextStartCode(strm->pStrmBuffStart,
                                                     strm->strmBuffSize);

                        if(next != NULL)
                        {
                            u32 consumed;

                            tmpStream -= numReadBytes;
                            strmLen += numReadBytes;

                            consumed = (u32) (next - tmpStream);
                            tmpStream += consumed;
                            strmLen -= consumed;
                        }
                    }

                    //ASSERT(pDecCont->rlcMode == 0);
                }
            }

            DEBUG_PRINT(("Skip redundant VLC\n"));
            pStorage->skipRedundantSlices = HANTRO_TRUE;

            returnValue = H264DEC_PIC_DECODED;
            strmLen = 0;
            if(decReval)
                returnValue = H264DEC_STRM_ERROR;
        } break;

        case H264BSD_PARAM_SET_ERROR: {
            if(!h264bsdCheckValidParamSets(&pDecCont->storage) && strmLen == 0)
            {
                DEC_API_TRC("H264DecDecode# H264DEC_STRM_ERROR, Invalid parameter set(s)\n");
                returnValue = H264DEC_STRM_ERROR;
            }

            /* update HW buffers if VLC mode */
            pDecCont->hwLength -= numReadBytes;
            pDecCont->hwStreamStartBus = pInput->streamBusAddress + (u32) (tmpStream - pInput->pStream);
            pDecCont->pHwStreamStart = tmpStream;
        } break;

        case H264BSD_NEW_ACCESS_UNIT: {
            pDecCont->streamPosUpdated = 0;
            pDecCont->storage.pictureBroken = HANTRO_TRUE;
            //h264InitPicFreezeOutput(pDecCont, 0);
            h264UpdateAfterPictureDecode(pDecCont);
            DEC_API_TRC("H264DecDecode# H264DEC_PIC_DECODED, NEW_ACCESS_UNIT\n");
            returnValue = H264DEC_PIC_DECODED;
            strmLen = 0;
        } break;

        case H264BSD_FMO: {
            DEBUG_PRINT(("FMO dedected\n"));
            DEC_API_TRC("H264DecDecode# H264DEC_ADVANCED_TOOLS, FMO\n");
            returnValue = H264DEC_ADVANCED_TOOLS;
            strmLen = 0;
        } break;

        case H264BSD_UNPAIRED_FIELD: {
            /* unpaired field detected and PP still running (wait after
             * second field decoded) -> wait here */
            DEC_API_TRC("H264DecDecode# H264DEC_PIC_DECODED, UNPAIRED_FIELD\n");
            //returnValue = H264DEC_PIC_DECODED;
            returnValue = H264DEC_STRM_ERROR;
            strmLen = 0;
        } break;

        case H264BSD_ERROR:
            markErrorDpbSlot(pDecCont->storage.dpb);
        //    ALOGE("h264bsdDecode ret H264BSD_ERROR\n");
        //    Mmcop5(pDecCont->storage.dpb, 0);
        //    goto end;
        case H264BSD_NONREF_PIC_SKIPPED:
            if (decResult  == H264BSD_NONREF_PIC_SKIPPED)
                returnValue = H264DEC_NONREF_PIC_SKIPPED;
            /* fall through */
        default:   /* case H264BSD_ERROR, H264BSD_RDY */
            {
                pDecCont->hwLength -= numReadBytes;
                pDecCont->hwStreamStartBus = pInput->streamBusAddress +
                    (u32) (tmpStream - pInput->pStream);

                pDecCont->pHwStreamStart = tmpStream;
            }
        }
    } while (strmLen);

end:

    /*  If Hw decodes stream, update stream buffers from "storage" */
    if (pDecCont->streamPosUpdated){
        pOutput->dataLeft = pDecCont->hwLength;
		if(pDecCont->ts_en || (pDecCont->storage.sliceHeader[1].fieldPicFlag==0))
			pOutput->dataLeft = 0;
    }else /* else update based on SW stream decode stream values */
        pOutput->dataLeft = pInput->dataLen - (u32) (tmpStream - pInput->pStream);

    if (returnValue == H264DEC_PIC_DECODED)
        pDecCont->gapsCheckedForThis = HANTRO_FALSE;

    if (returnValue < 0) {
        markErrorDpbSlot(pDecCont->storage.dpb);
        pDecCont->storage.dpb->previousOut = NULL;
    }

    pDecCont->storage.dpb->currentOut = NULL;

    return (returnValue);
}

void H264DecReset(decContainer_t *pDecCont)
{
    DEC_API_TRC("H264DecReset#\n");

    if(pDecCont == NULL)
    {
        DEC_API_TRC("H264DecReset# ERROR: decInst == NULL\n");
        return;
    }

    /* Check for valid decoder instance */
    if(pDecCont->checksum != pDecCont)
    {
        DEC_API_TRC("H264DecReset# ERROR: Decoder not initialized\n");
        return;
    }

    h264bsdFlushDpb(pDecCont->storage.dpbs[0]);

    DEC_API_TRC("H264DecReset# OK\n");

    return;
}

/*------------------------------------------------------------------------------

    Function: H264DecNextPicture

        Functional description:
            Get next picture in display order if any available.

        Input:
            decInst     decoder instance.

        Output:
            pOutput     pointer to output structure

        Returns:
            H264DEC_OK            no pictures available for display
            H264DEC_PIC_RDY       picture available for display
            H264DEC_PARAM_ERROR     invalid parameters

------------------------------------------------------------------------------*/
H264DecRet H264DecNextPicture(decContainer_t *pDecCont, H264DecPicture * pOutput)
{
    u32 getFrame = 0;
    dpbStorage_t *outDpb;

    DEC_API_TRC("H264DecNextPicture#\n");

    if(pDecCont == NULL || pOutput == NULL)
    {
        DEC_API_TRC("H264DecNextPicture# ERROR: pDecCont or pOutput is NULL\n");
        return (H264DEC_PARAM_ERROR);
    }

    /* Check for valid decoder instance */
    if(pDecCont->checksum != pDecCont)
    {
        DEC_API_TRC("H264DecNextPicture# ERROR: Decoder not initialized\n");
        return (H264DEC_NOT_INITIALIZED);
    }

    outDpb = pDecCont->storage.dpbs[0];

    /* if display order is the same as decoding order and PP is used and
     * cannot be used in pipeline (rotation) -> do not perform PP here but
     * while decoding next picture (parallel processing instead of
     * DEC followed by PP followed by DEC...) */
    ASSERT(pDecCont->storage.numViews ||
           pDecCont->storage.outView == 0);

    pDecCont->storage.dpb = pDecCont->storage.dpbs[0];

    getFrame = h264bsdDpbOutputPicture(pDecCont->storage.dpb, (void*)pOutput);

    if (getFrame)
    {
        H264CropParams *pCrop = &pOutput->cropParams;
        h264bsdCroppingParams(&pDecCont->storage,
                              &pCrop->croppingFlag,
                              &pCrop->cropLeftOffset,
                              &pCrop->cropOutWidth,
                              &pCrop->cropTopOffset,
                              &pCrop->cropOutHeight);

        //if (!pDecCont->storage.numViews)
        //    pOutput->viewId = 0;

        pOutput->picWidth = h264bsdPicWidth(&pDecCont->storage) << 4;
        pOutput->picHeight = h264bsdPicHeight(&pDecCont->storage) << 4;

        if (pCrop->croppingFlag == 0)
        {
            pCrop->cropLeftOffset = 0;
            pCrop->cropTopOffset = 0;
            pCrop->cropOutWidth = pOutput->picWidth;
            pCrop->cropOutHeight = pOutput->picHeight;
        }

        DEC_API_TRC("H264DecNextPicture# H264DEC_PIC_RDY\n");
        return (H264DEC_PIC_RDY);
    }
    else
    {
        DEC_API_TRC("H264DecNextPicture# H264DEC_OK\n");
        return (H264DEC_OK);
    }
}

/*------------------------------------------------------------------------------
    Function name : h264UpdateAfterPictureDecode
    Description   :

    Return type   : void
    Argument      : decContainer_t * pDecCont
------------------------------------------------------------------------------*/
void h264UpdateAfterPictureDecode(decContainer_t * pDecCont)
{
    storage_t *pStorage = &pDecCont->storage;
    dpbStorage_t *dpb = pStorage->dpb;
    sliceHeader_t *sliceHeader = pStorage->sliceHeader;
    u32 picStruct;

    //h264bsdResetStorage(pStorage);

    ASSERT((pStorage));

    if (pStorage->sliceHeader->fieldPicFlag == 0)
        picStruct = FRAME;
    else
        picStruct = pStorage->sliceHeader->bottomFieldFlag;

    if (pStorage->poc->containsMmco5)
    {
        u32 tmp;

        tmp = MIN(pStorage->poc->picOrderCnt[0], pStorage->poc->picOrderCnt[1]);
        pStorage->poc->picOrderCnt[0] -= tmp;
        pStorage->poc->picOrderCnt[1] -= tmp;
    }

    //pStorage->currentMarked = pStorage->validSliceInAccessUnit;

    if (pStorage->validSliceInAccessUnit)
    {
    	DPBDEBUG("pStorage->prevNalUnit->nalRefIdc=%d\n", pStorage->prevNalUnit->nalRefIdc);
        if (pStorage->prevNalUnit->nalRefIdc)
        {
            (void) h264bsdMarkDecRefPic(dpb,
                                        &sliceHeader->decRefPicMarking,
                                        picStruct,
                                        sliceHeader->frameNum,
                                        pStorage->poc->picOrderCnt,
                                        IS_IDR_NAL_UNIT(pStorage->prevNalUnit) ?
                                        HANTRO_TRUE : HANTRO_FALSE,
                                        pStorage->currentPicId,
                                        pStorage->numConcealedMbs, IS_I_SLICE(pStorage->sliceHeader->sliceType));
        }
        else
        {
            /* non-reference picture, just store for possible display
             * reordering */
            (void) h264bsdMarkDecRefPic(dpb, NULL,
                                        picStruct,
                                        sliceHeader->frameNum,
                                        pStorage->poc->picOrderCnt,
                                        HANTRO_FALSE,
                                        pStorage->currentPicId,
                                        pStorage->numConcealedMbs, IS_I_SLICE(pStorage->sliceHeader->sliceType));
        }

        h264DpbUpdateOutputList(dpb, picStruct);
    }
    else
    {
        ;//pStorage->secondField = 0;
    }

    pStorage->picStarted = HANTRO_FALSE;
    pStorage->validSliceInAccessUnit = HANTRO_FALSE;
}

/*------------------------------------------------------------------------------
    Function name : h264SpsSupported
    Description   :

    Return type   : u32
    Argument      : const decContainer_t * pDecCont
------------------------------------------------------------------------------*/
u32 h264SpsSupported(const decContainer_t * pDecCont)
{
    const seqParamSet_t *sps = pDecCont->storage.activeSps;

    /* check picture size */
    if(sps->picWidthInMbs * 16 > pDecCont->maxDecPicWidth ||
       sps->picWidthInMbs < 3 || sps->picHeightInMbs < 3 ||
       (sps->picWidthInMbs * sps->picHeightInMbs) > /*((1920 >> 4) * (1088 >> 4))*/((3840>>4) * (2160>>4)))
    {
        DEBUG_PRINT(("Picture size not supported!\n"));
        return 0;
    }

    if(pDecCont->h264ProfileSupport == H264_BASELINE_PROFILE)
    {
        if(sps->frameMbsOnlyFlag != 1)
        {
            DEBUG_PRINT(("INTERLACED!!! Not supported in baseline decoder\n"));
            return 0;
        }
        if(sps->chromaFormatIdc != 1)
        {
            DEBUG_PRINT(("CHROMA!!! Only 4:2:0 supported in baseline decoder\n"));
            return 0;
        }
        if(sps->scalingMatrixPresentFlag != 0)
        {
            DEBUG_PRINT(("SCALING Matrix!!! Not supported in baseline decoder\n"));
            return 0;
        }
    }

    return 1;
}

/*------------------------------------------------------------------------------
    Function name : h264PpsSupported
    Description   :

    Return type   : u32
    Argument      : const decContainer_t * pDecCont
------------------------------------------------------------------------------*/
u32 h264PpsSupported(const decContainer_t * pDecCont)
{
    const picParamSet_t *pps = pDecCont->storage.activePps;

    if(pDecCont->h264ProfileSupport == H264_BASELINE_PROFILE)
    {
        if(pps->entropyCodingModeFlag != 0)
        {
            DEBUG_PRINT(("CABAC!!! Not supported in baseline decoder\n"));
            return 0;
        }
        if(pps->weightedPredFlag != 0 || pps->weightedBiPredIdc != 0)
        {
            DEBUG_PRINT(("WEIGHTED Pred!!! Not supported in baseline decoder\n"));
            return 0;
        }
        if(pps->transform8x8Flag != 0)
        {
            DEBUG_PRINT(("TRANSFORM 8x8!!! Not supported in baseline decoder\n"));
            return 0;
        }
        if(pps->scalingMatrixPresentFlag != 0)
        {
            DEBUG_PRINT(("SCALING Matrix!!! Not supported in baseline decoder\n"));
            return 0;
        }
    }
    return 1;
}

/*------------------------------------------------------------------------------
    Function name   : h264StreamIsBaseline
    Description     :
    Return type     : u32
    Argument        : const decContainer_t * pDecCont
------------------------------------------------------------------------------*/
u32 h264StreamIsBaseline(const decContainer_t * pDecCont)
{
    const picParamSet_t *pps = pDecCont->storage.activePps;
    const seqParamSet_t *sps = pDecCont->storage.activeSps;

    if(sps->frameMbsOnlyFlag != 1)
    {
        return 0;
    }
    if(sps->chromaFormatIdc != 1)
    {
        return 0;
    }
    if(sps->scalingMatrixPresentFlag != 0)
    {
        return 0;
    }
    if(pps->entropyCodingModeFlag != 0)
    {
        return 0;
    }
    if(pps->weightedPredFlag != 0 || pps->weightedBiPredIdc != 0)
    {
        return 0;
    }
    if(pps->transform8x8Flag != 0)
    {
        return 0;
    }
    if(pps->scalingMatrixPresentFlag != 0)
    {
        return 0;
    }
    return 1;
}

/*------------------------------------------------------------------------------
    Function name : h264AllocateResources
    Description   :

    Return type   : u32
    Argument      : decContainer_t * pDecCont
------------------------------------------------------------------------------*/
u32 h264AllocateResources(decContainer_t * pDecCont)
{
    u32 ret, mbs_in_pic;
    DecAsicBuffers_t *asic = pDecCont->asicBuff;
    storage_t *pStorage = &pDecCont->storage;

    const seqParamSet_t *sps = pStorage->activeSps;

    SetDecRegister(pDecCont->h264Regs, HWIF_PIC_MB_WIDTH, sps->picWidthInMbs);
    SetDecRegister(pDecCont->h264Regs, HWIF_PIC_MB_HEIGHT_P,
                   sps->picHeightInMbs);

    ReleaseAsicBuffers(asic);

    ret = AllocateAsicBuffers(pDecCont, asic, pStorage->picSizeInMbs);
    if(ret == 0)
    {
        if(pDecCont->h264ProfileSupport != H264_BASELINE_PROFILE)
        {
            SetDecRegister(pDecCont->h264Regs, HWIF_QTABLE_BASE,
                           asic->cabacInit.phy_addr);
        }
    }

    return ret;
}

/*------------------------------------------------------------------------------
    Function name   : h264GetSarInfo
    Description     : Returns the sample aspect ratio size info
    Return type     : void
    Argument        : storage_t *pStorage - decoder storage
    Argument        : u32 * sar_width - SAR width returned here
    Argument        : u32 *sar_height - SAR height returned here
------------------------------------------------------------------------------*/
void h264GetSarInfo(const storage_t * pStorage, u32 * sar_width,
                    u32 * sar_height)
{
    switch (h264bsdAspectRatioIdc(pStorage))
    {
    case 0:
        *sar_width = 0;
        *sar_height = 0;
        break;
    case 1:
        *sar_width = 1;
        *sar_height = 1;
        break;
    case 2:
        *sar_width = 12;
        *sar_height = 11;
        break;
    case 3:
        *sar_width = 10;
        *sar_height = 11;
        break;
    case 4:
        *sar_width = 16;
        *sar_height = 11;
        break;
    case 5:
        *sar_width = 40;
        *sar_height = 33;
        break;
    case 6:
        *sar_width = 24;
        *sar_height = 1;
        break;
    case 7:
        *sar_width = 20;
        *sar_height = 11;
        break;
    case 8:
        *sar_width = 32;
        *sar_height = 11;
        break;
    case 9:
        *sar_width = 80;
        *sar_height = 33;
        break;
    case 10:
        *sar_width = 18;
        *sar_height = 11;
        break;
    case 11:
        *sar_width = 15;
        *sar_height = 11;
        break;
    case 12:
        *sar_width = 64;
        *sar_height = 33;
        break;
    case 13:
        *sar_width = 160;
        *sar_height = 99;
        break;
    case 255:
        h264bsdSarSize(pStorage, sar_width, sar_height);
        break;
    default:
        *sar_width = 0;
        *sar_height = 0;
    }
}

void H264DecTrace( char *string)
{
#if 0
    FILE *fp;

#if 0
    fp = fopen("dec_api.trc", "at");
#else
    fp = stderr;
#endif

    if(!fp)
        return;

    fprintf(fp, "%s", string);

    if(fp != stderr)
        fclose(fp);
#endif
   ALOGE("%s", string);

}

}

