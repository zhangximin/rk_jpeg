#include "jpegdecapi.h"
#include "dwl.h"
#include "hw_jpegdecapi.h"
#include "jpegdeccontainer.h"
#include "jpegdecmarkers.h"
#include "hw_jpegdecutils.h"

#define STRM_ERROR 0xFFFFFFFFU

#ifndef OK
#define OK 0
#endif

//#define LOGYC	printf
#define LOGYC

typedef struct{
	//RK_U8 * pCurrPos;
	RK_U32 bitPosInByte;
	RK_U32 streamLength;
	RK_U32 readBits;
	RK_U32 appnFlag;
	RK_U32 thumbnail;
	HW_BOOL errorOccur;
}JpegStream;

static RK_U32 hw_JpegDecGetByte(JpegStream * pStream, struct hw_jpeg_source_mgr *src){
	RK_U32 tmp;
	unsigned char ch;
	if((pStream->readBits + 8) > (8 * pStream->streamLength))
    {
		LOGYC("GetBytes beyond length.");
		return (STRM_ERROR);
	}
	pStream->errorOccur = !(*src->read_1_byte)(src->info,&ch);// if false return STRM_ERROR
    tmp = ch;
	//(pStream->pCurrPos)++;
	pStream->errorOccur = !(*src->read_1_byte)(src->info,&ch);
	//syn
	src->next_input_byte--;
	src->bytes_in_buffer++;
    tmp = (tmp << 8) | ch;//*(pStream->pCurrPos);
    tmp = (tmp >> (8 - pStream->bitPosInByte)) & 0xFF;
    pStream->readBits += 8;
WHALLLOG("read one byte: %08x",(unsigned char)tmp);
	if(pStream->errorOccur){
		return STRM_ERROR;
	}
    return (tmp);
}

static RK_U32 hw_JpegDecGet2Bytes(JpegStream * pStream, struct hw_jpeg_source_mgr *src)
{
    RK_U32 tmp;
	unsigned char ch;
    if((pStream->readBits + 16) > (8 * pStream->streamLength))
    {
		LOGYC("Get2Bytes beyond length.");
		return (STRM_ERROR);
	}
	pStream->errorOccur = !(*src->read_1_byte)(src->info,&ch);
    tmp = ch;
	//(pStream->pCurrPos)++;
	pStream->errorOccur = !(*src->read_1_byte)(src->info,&ch);
    tmp = (tmp << 8) | ch;
	//(pStream->pCurrPos)++;
	pStream->errorOccur = !(*src->read_1_byte)(src->info,&ch);
	if(pStream->errorOccur){
		return STRM_ERROR;
	}
	//syn
	src->next_input_byte--;
	src->bytes_in_buffer++;
    tmp = (tmp << 8) | ch;//*(pStream->pCurrPos);
    tmp = (tmp >> (8 - pStream->bitPosInByte)) & 0xFFFF;
    pStream->readBits += 16;
WHALLLOG("read two byte: %d, %x",tmp, tmp);
    return (tmp);
}

static RK_U32 hw_JpegDecFlushBits(JpegStream * pStream, RK_U32 bits, struct hw_jpeg_source_mgr *src)
{
    RK_U32 tmp;
    RK_U32 extraBits = 0;
	unsigned char ch1,ch2;

    if((pStream->readBits + bits) > (8 * pStream->streamLength))
    {
        /* there are not so many bits left in buffer */
        /* stream pointers to the end of the stream  */
        /* and return value STRM_ERROR               */
        pStream->readBits = 8 * pStream->streamLength;
        pStream->bitPosInByte = 0;
        //pStream->pCurrPos = pStream->pStartOfStream + pStream->streamLength;
		LOGYC("FlushBits beyond length.");
        return (STRM_ERROR);
    }
    else
    {
        tmp = 0;
        while(tmp < bits)
        {
            if(bits - tmp < 8)
            {
                if((8 - pStream->bitPosInByte) > (bits - tmp))
                {
                    /* inside one byte */
                    pStream->bitPosInByte += bits - tmp;
                    tmp = bits;
                }
                else
                {
					pStream->errorOccur = !(*src->read_1_byte)(src->info,&ch1);
					pStream->errorOccur = !(*src->read_1_byte)(src->info,&ch2);
					if(pStream->errorOccur){
						return STRM_ERROR;
					}
                    //if(pStream->pCurrPos[0] == 0xFF &&
                    //   pStream->pCurrPos[1] == 0x00)
					if(ch1 == 0xFF && ch2 == 0x00)
                    {
                        extraBits += 8;
                        //pStream->pCurrPos += 2;
                    }
                    else
                    {
						//syn
						src->next_input_byte--;
						src->bytes_in_buffer++;
                        //pStream->pCurrPos++;
                    }
                    tmp += 8 - pStream->bitPosInByte;
                    pStream->bitPosInByte = 0;
                    pStream->bitPosInByte = bits - tmp;
                    tmp = bits;
                }
            }
            else
            {
                tmp += 8;
                if(pStream->appnFlag)
                {
					pStream->errorOccur = !(*src->read_1_byte)(src->info,&ch1);
                    //pStream->pCurrPos++;
					if(pStream->errorOccur){
						return STRM_ERROR;
					}
                }
                else
                {
					pStream->errorOccur = !(*src->read_1_byte)(src->info,&ch1);
					pStream->errorOccur = !(*src->read_1_byte)(src->info,&ch2);
					if(pStream->errorOccur){
						return STRM_ERROR;
					}
					if(ch1 == 0xFF && ch2 == 0x00)
                    //if(pStream->pCurrPos[0] == 0xFF &&
                    //  pStream->pCurrPos[1] == 0x00)
                    {
                        extraBits += 8;
                        //pStream->pCurrPos += 2;
                    }
                    else
                    {
						//syn
						src->next_input_byte--;
						src->bytes_in_buffer++;
                        //pStream->pCurrPos++;
                    }
                }
            }
        }
        /* update stream pointers */
        pStream->readBits += bits + extraBits;
        return (OK);
    }
}

/*------------------------------------------------------------------------------

    Function name: Hw_JpegDecGetImageInfo

        Functional description:
            Get image information of the JFIF
        Outputs:
            JPEGDEC_OK
			JPEGDEC_ERROR
            JPEGDEC_UNSUPPORTED
            JPEGDEC_PARAM_ERROR
			JPEGDEC_INCREASE_BUFFER
            JPEGDEC_INVALID_STREAM_LENGTH
            JPEGDEC_INVALID_INPUT_BUFFER_SIZE

------------------------------------------------------------------------------*/

/* Get image information of the JFIF */
JpegDecRet JpegDecGetSimpleImageInfo(HwJpegInputInfo * hwInfo, JpegDecInput * pDecIn,
                               JpegDecImageInfo * pImageInfo, RK_U32 *ret)
{
//#define PTR_JPGC ((JpegDecContainer *) decInst)
    RK_U32 Nf = 0;
    RK_U32 Ns = 0;
    RK_U32 NsThumb = 0;
    RK_U32 i, j = 0;
    RK_U32 init = 0;
    RK_U32 initThumb = 0;
    RK_U32 H[MAX_NUMBER_OF_COMPONENTS];
    RK_U32 V[MAX_NUMBER_OF_COMPONENTS];
    RK_U32 Htn[MAX_NUMBER_OF_COMPONENTS];
    RK_U32 Vtn[MAX_NUMBER_OF_COMPONENTS];
    RK_U32 Hmax = 0;
    RK_U32 Vmax = 0;
    RK_U32 headerLength = 0;
    RK_U32 currentByte = 0;
    RK_U32 currentBytes = 0;
    RK_U32 appLength = 0;
    RK_U32 appBits = 0;
    RK_U32 thumbnail = 0;
    RK_U32 errorCode = 0;

#ifdef JPEGDEC_ERROR_RESILIENCE
    RK_U32 errorResilience = 0;
    RK_U32 errorResilienceThumb = 0;
#endif /* JPEGDEC_ERROR_RESILIENCE */

    JpegStream stream;
	struct hw_jpeg_source_mgr *src = hwInfo->streamCtl.inStream;
    //LOGYC("JpegDecGetImageInfo check param\n");

    /* check pointers & parameters */
    if(src == NULL || pDecIn == NULL || pImageInfo == NULL)
    {
        LOGYC("JpegDecGetImageInfo JPEGDEC_PARAM_ERROR\n");
        return (JPEGDEC_PARAM_ERROR);
    }

    /* Check the stream lenth */
    if(pDecIn->streamLength < 1)
    {
        LOGYC("JpegDecGetImageInfo ERROR: pDecIn->streamLength\n");
        return (JPEGDEC_INVALID_STREAM_LENGTH);
    }

    /* Check the stream lenth */
    if((pDecIn->streamLength > DEC_X170_MAX_STREAM) &&
       (pDecIn->bufferSize < JPEGDEC_X170_MIN_BUFFER ||
        pDecIn->bufferSize > JPEGDEC_X170_MAX_BUFFER))
    {
        LOGYC("JpegDecGetImageInfo ERROR: pDecIn->bufferSize = %d,pDecIn->streamLength = %d\n",
			pDecIn->bufferSize,pDecIn->streamLength);
        return (JPEGDEC_INVALID_INPUT_BUFFER_SIZE);
    }

    /* Check the stream buffer size */
    if(pDecIn->bufferSize && (pDecIn->bufferSize < JPEGDEC_X170_MIN_BUFFER ||
                              pDecIn->bufferSize > JPEGDEC_X170_MAX_BUFFER))
    {
        LOGYC("JpegDecGetImageInfo ERROR: pDecIn->bufferSize = %d\n",pDecIn->bufferSize);
        return (JPEGDEC_INVALID_INPUT_BUFFER_SIZE);
    }

    /* Check the stream buffer size */
    if(pDecIn->bufferSize && ((pDecIn->bufferSize % 8) != 0))
    {
        LOGYC("JpegDecGetImageInfo ERROR: JPEGDEC_INVALID_INPUT_BUFFER_SIZE\n");
        return (JPEGDEC_INVALID_INPUT_BUFFER_SIZE);
    }

    /* reset sampling factors */
    for(i = 0; i < MAX_NUMBER_OF_COMPONENTS; i++)
    {
        H[i] = 0;
        V[i] = 0;
        Htn[i] = 0;
        Vtn[i] = 0;
    }

    /* imageInfo initialization */
    pImageInfo->displayWidth = 0;
    pImageInfo->displayHeight = 0;
    pImageInfo->outputWidth = 0;
    pImageInfo->outputHeight = 0;
    pImageInfo->version = 0;
    pImageInfo->units = 0;
    pImageInfo->xDensity = 0;
    pImageInfo->yDensity = 0;
    pImageInfo->outputFormat = 0;

    /* Default value to "Thumbnail" */
    pImageInfo->thumbnailType = JPEGDEC_NO_THUMBNAIL;
    pImageInfo->displayWidthThumb = 0;
    pImageInfo->displayHeightThumb = 0;
    pImageInfo->outputWidthThumb = 0;
    pImageInfo->outputHeightThumb = 0;
    pImageInfo->outputFormatThumb = 0;

    /* utils initialization */
    stream.bitPosInByte = 0;
//    stream.pCurrPos = (RK_U8 *) pDecIn->streamBuffer.pVirtualAddress;
//    stream.pStartOfStream = (RK_U8 *) pDecIn->streamBuffer.pVirtualAddress;
    stream.readBits = 0;
    stream.appnFlag = 0;
	stream.errorOccur = 0;

    /* stream length */
    if(!pDecIn->bufferSize)
        stream.streamLength = pDecIn->streamLength;
    else
        stream.streamLength = pDecIn->bufferSize;
	//LOGYC("JpegDecGetImageInfo start get marker\n");
    /* Read decoding parameters */
    for(stream.readBits = 0; (stream.readBits / 8) < stream.streamLength;)//stream.readBits++)//why++ ?
    {
        /* Look for marker prefix byte from stream */
        if(hw_JpegDecGetByte(&(stream),src) == 0xFF)
        {
            currentByte = hw_JpegDecGetByte(&(stream),src);
			WHALLLOG("MAKR : 0xFF%X", currentByte);
            /* switch to certain header decoding */
            switch (currentByte)
            {
			case EOI:
				return JPEGDEC_OK;
                /* baseline marker */
            case SOF0:
                /* progresive marker */
            case SOF2:
				//LOGYC("JpegDecGetImageInfo get SOF2\n");
                if(currentByte == SOF0)
                    pImageInfo->codingMode = JPEGDEC_BASELINE;//PTR_JPGC->info.operationType =
                else
                    pImageInfo->codingMode = JPEGDEC_PROGRESSIVE;//PTR_JPGC->info.operationType =
                /* Frame header */
                i++;
                Hmax = 0;
                Vmax = 0;

                /* SOF0/SOF2 length */
                headerLength = hw_JpegDecGet2Bytes(&(stream),src);
                if(headerLength == STRM_ERROR ||
                   ((stream.readBits + ((headerLength * 8) - 16)) >
                    (8 * stream.streamLength)))
                {
                    errorCode = 1;
                    break;
                }

                /* Sample precision (only 8 bits/sample supported) */
                currentByte = hw_JpegDecGetByte(&(stream),src);
                if(currentByte != 8)
                {
                    LOGYC("JpegDecGetImageInfo ERROR: Sample precision");
                    return (JPEGDEC_UNSUPPORTED);
                }

                /* Number of Lines */
                pImageInfo->outputHeight = hw_JpegDecGet2Bytes(&(stream),src);
                pImageInfo->displayHeight = pImageInfo->outputHeight;
                if(pImageInfo->outputHeight < 1)
                {
                    LOGYC("JpegDecGetImageInfo ERROR: pImageInfo->outputHeight Unsupported");
                    return (JPEGDEC_UNSUPPORTED);
                }

#ifdef JPEGDEC_ERROR_RESILIENCE
                if((pImageInfo->outputHeight & 0xF) &&
                   (pImageInfo->outputHeight & 0xF) <= 8)
                    errorResilience = 1;
#endif /* JPEGDEC_ERROR_RESILIENCE */

                /* round up to next multiple-of-16 */
                pImageInfo->outputHeight += 0xf;
                pImageInfo->outputHeight &= ~(0xf);
                //PTR_JPGC->frame.hwY = pImageInfo->outputHeight;

                /* Number of Samples per Line */
                pImageInfo->outputWidth = hw_JpegDecGet2Bytes(&(stream),src);
                pImageInfo->displayWidth = pImageInfo->outputWidth;
                if(pImageInfo->outputWidth < 1)
                {
                    LOGYC("JpegDecGetImageInfo ERROR: pImageInfo->outputWidth unsupported");
                    return (JPEGDEC_UNSUPPORTED);
                }
                pImageInfo->outputWidth += 0xf;
                pImageInfo->outputWidth &= ~(0xf);
                //PTR_JPGC->frame.hwX = pImageInfo->outputWidth;

                /* check for minimum and maximum dimensions */
                if(pImageInfo->outputWidth < JPEGDEC_MIN_WIDTH ||
                   pImageInfo->outputHeight < JPEGDEC_MIN_HEIGHT ||
                   pImageInfo->outputWidth > JPEGDEC_MAX_WIDTH_8190 ||
                   pImageInfo->outputHeight > JPEGDEC_MAX_HEIGHT_8190 ||
                   (pImageInfo->outputWidth * pImageInfo->outputHeight) >
                   JPEGDEC_MAX_PIXEL_AMOUNT_8190)
                {
                    LOGYC("JpegDecGetImageInfo image: Unsupported size");
					*ret |= JPEGDEC_UNSUPPORTEDSIZE;
                    //return (JPEGDEC_UNSUPPORTED);
                }

                /* Number of Image Components per Frame */
                Nf = hw_JpegDecGetByte(&(stream),src);
                if(Nf != 3 && Nf != 1)
                {
                    LOGYC("JpegDecGetImageInfo ERROR: Number of Image Components per Frame");
                    return (JPEGDEC_UNSUPPORTED);
                }
                for(j = 0; j < Nf; j++)
                {
                    /* jump over component identifier */
                    if(hw_JpegDecFlushBits(&(stream), 8, src) == STRM_ERROR)
                    {
                        errorCode = 1;
                        break;
                    }

                    /* Horizontal sampling factor */
                    currentByte = hw_JpegDecGetByte(&(stream), src);
                    H[j] = (currentByte >> 4);

                    /* Vertical sampling factor */
                    V[j] = (currentByte & 0xF);

                    /* jump over Tq */
                    if(hw_JpegDecFlushBits(&(stream), 8, src) == STRM_ERROR)
                    {
                        errorCode = 1;
                        break;
                    }

                    if(H[j] > Hmax)
                        Hmax = H[j];
                    if(V[j] > Vmax)
                        Vmax = V[j];
                }
                if(Hmax == 0 || Vmax == 0)
                {
                    LOGYC("JpegDecGetImageInfo ERROR: Hmax == 0 || Vmax == 0");
                    return (JPEGDEC_UNSUPPORTED);
                }
#ifdef JPEGDEC_ERROR_RESILIENCE
                if(H[0] == 2 && V[0] == 2 &&
                   H[1] == 1 && V[1] == 1 && H[2] == 1 && V[2] == 1)
                {
                    pImageInfo->outputFormat = JPEGDEC_YCbCr420_SEMIPLANAR;
                }
                else
                {
                    /* check if fill needed */
                    if(errorResilience)
                    {
                        pImageInfo->outputHeight -= 16;
                        pImageInfo->displayHeight = pImageInfo->outputHeight;
                    }
                }
#endif /* JPEGDEC_ERROR_RESILIENCE */

                /* check format */
                if(H[0] == 2 && V[0] == 2 &&
                   H[1] == 1 && V[1] == 1 && H[2] == 1 && V[2] == 1)
                {
                    pImageInfo->outputFormat = JPEGDEC_YCbCr420_SEMIPLANAR;
                    //PTR_JPGC->frame.numMcuInRow = (PTR_JPGC->frame.hwX / 16);
                    //PTR_JPGC->frame.numMcuInFrame = ((PTR_JPGC->frame.hwX *
                                                      //PTR_JPGC->frame.hwY) /
                                                     //256);
                }
                else if(H[0] == 2 && V[0] == 1 &&
                        H[1] == 1 && V[1] == 1 && H[2] == 1 && V[2] == 1)
                {
                    pImageInfo->outputFormat = JPEGDEC_YCbCr422_SEMIPLANAR;
                    //PTR_JPGC->frame.numMcuInRow = (PTR_JPGC->frame.hwX / 16);
                    //PTR_JPGC->frame.numMcuInFrame = ((PTR_JPGC->frame.hwX *
                                                     // PTR_JPGC->frame.hwY) /
                                                     //128);
                }
                else if(H[0] == 1 && V[0] == 2 &&
                        H[1] == 1 && V[1] == 1 && H[2] == 1 && V[2] == 1)
                {
                    pImageInfo->outputFormat = JPEGDEC_YCbCr440;
                    //PTR_JPGC->frame.numMcuInRow = (PTR_JPGC->frame.hwX / 8);
                    //PTR_JPGC->frame.numMcuInFrame = ((PTR_JPGC->frame.hwX *
                                                      //PTR_JPGC->frame.hwY) /
                                                     //128);
                }
                else if(H[0] == 1 && V[0] == 1 &&
                        H[1] == 0 && V[1] == 0 && H[2] == 0 && V[2] == 0)
                {
                    pImageInfo->outputFormat = JPEGDEC_YCbCr400;
                    //PTR_JPGC->frame.numMcuInRow = (PTR_JPGC->frame.hwX / 8);
                    //PTR_JPGC->frame.numMcuInFrame = ((PTR_JPGC->frame.hwX *
                                                      //PTR_JPGC->frame.hwY) /
                                                     //64);
                }
                else if(//PTR_JPGC->extensionsSupported &&
                        H[0] == 4 && V[0] == 1 &&
                        H[1] == 1 && V[1] == 1 && H[2] == 1 && V[2] == 1)
                {
                    /* YUV411 output has to be 32 pixel multiple */
                    if(pImageInfo->outputWidth & 0x1F)
                    {
                        pImageInfo->outputWidth += 16;
                        //PTR_JPGC->frame.hwX = pImageInfo->outputWidth;
                    }

                    /* check for maximum dimensions */
                    if(pImageInfo->outputWidth > JPEGDEC_MAX_WIDTH_8190 ||
                       (pImageInfo->outputWidth * pImageInfo->outputHeight) >
                       JPEGDEC_MAX_PIXEL_AMOUNT_8190)
                    {
                        LOGYC("JpegDecGetImageInfo 411: Unsupported size");
						*ret |= JPEGDEC_UNSUPPORTEDSIZE;
                        //return (JPEGDEC_UNSUPPORTED);
                    }

                    pImageInfo->outputFormat = JPEGDEC_YCbCr411_SEMIPLANAR;
                    //PTR_JPGC->frame.numMcuInRow = (PTR_JPGC->frame.hwX / 32);
                    //PTR_JPGC->frame.numMcuInFrame = ((PTR_JPGC->frame.hwX *
                                                      //PTR_JPGC->frame.hwY) /
                                                    // 256);
                }
                else if(//PTR_JPGC->extensionsSupported &&
                        H[0] == 1 && V[0] == 1 &&
                        H[1] == 1 && V[1] == 1 && H[2] == 1 && V[2] == 1)
                {
                    pImageInfo->outputFormat = JPEGDEC_YCbCr444_SEMIPLANAR;
                    //PTR_JPGC->frame.numMcuInRow = (PTR_JPGC->frame.hwX / 8);
                    //PTR_JPGC->frame.numMcuInFrame = ((PTR_JPGC->frame.hwX *
                                                      //PTR_JPGC->frame.hwY) /
                                                     //64);
                }
                else
                {
                    LOGYC("JpegDecGetImageInfo ERROR: Unsupported YCbCr format");
					*ret |= JPEGDEC_YUV_UNSUPPORTED;
                    //return (JPEGDEC_UNSUPPORTED);
                }

                /* restore output format */
                //PTR_JPGC->info.yCbCrMode = PTR_JPGC->info.getInfoYCbCrMode =
                //    pImageInfo->outputFormat;
                break;
            case SOS:
if(hwInfo->justcaloutwh){//must read enough to judge it support hard decode
	return JPEGDEC_OK;//if just cal wh return at once.
}
                /* SOS length */
				//LOGYC("JpegDecGetImageInfo get SOS\n");
                headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                if(headerLength == STRM_ERROR ||
                   ((stream.readBits + ((headerLength * 8) - 16)) >
                    (8 * stream.streamLength)))
                {
                    errorCode = 1;
                    break;
                }

                /* check if interleaved or non-ibnterleaved */
                Ns = hw_JpegDecGetByte(&(stream), src);
                if(Ns == MIN_NUMBER_OF_COMPONENTS &&
                   pImageInfo->outputFormat != JPEGDEC_YCbCr400 &&
                   pImageInfo->codingMode == JPEGDEC_BASELINE)
                {
                    pImageInfo->codingMode = //PTR_JPGC->info.operationType =
                        JPEGDEC_NONINTERLEAVED;
                }

                /* jump over SOS header */
                if(headerLength != 0)
                {
                    stream.readBits += ((headerLength * 8) - 16);
					if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
						errorCode = 1;
                    	break;
					}
                    //stream.pCurrPos += (((headerLength * 8) - 16) / 8);
                }

                if((stream.readBits + 8) < (8 * stream.streamLength))
                {
                    //PTR_JPGC->info.init = 1;
                    init = 1;
                }
                else
                {
                    LOGYC("JpegDecGetImageInfo ERROR: Needs to increase input buffer");
                    return (JPEGDEC_INCREASE_INPUT_BUFFER);
                }
                break;
            case DQT:
                /* DQT length */
				//LOGYC("JpegDecGetImageInfo get DQT\n");
                headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                if(headerLength == STRM_ERROR ||
                   ((stream.readBits + ((headerLength * 8) - 16)) >
                    (8 * stream.streamLength)))
                {
                    errorCode = 1;
                    break;
                }
                /* jump over DQT header */
                if(headerLength != 0)
                {
                    stream.readBits += ((headerLength * 8) - 16);
					if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
						errorCode = 1;
                    	break;
					}
                    //stream.pCurrPos += (((headerLength * 8) - 16) / 8);
                }
                break;
            case DHT:
                /* DHT length */
				//LOGYC("JpegDecGetImageInfo get DHT\n");
                headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                if(headerLength == STRM_ERROR ||
                   ((stream.readBits + ((headerLength * 8) - 16)) >
                    (8 * stream.streamLength)))
                {
                    errorCode = 1;
                    break;
                }
                /* jump over DHT header */
                if(headerLength != 0)
                {
                    stream.readBits += ((headerLength * 8) - 16);
					if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
						errorCode = 1;
                    	break;
					}
                    //stream.pCurrPos += (((headerLength * 8) - 16) / 8);
                }
                break;
            case DRI:
                /* DRI length */
				//LOGYC("JpegDecGetImageInfo get DRI\n");
                headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                if(headerLength == STRM_ERROR ||
                   ((stream.readBits + ((headerLength * 8) - 16)) >
                    (8 * stream.streamLength)))
                {
                    errorCode = 1;
                    break;
                }
#if 0
                /* jump over DRI header */
                if(headerLength != 0)
                {
                    stream.readBits += ((headerLength * 8) - 16);
                    stream.pCurrPos += (((headerLength * 8) - 16) / 8);
                }
#endif
                headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                if(headerLength == STRM_ERROR ||
                   ((stream.readBits + ((headerLength * 8) - 16)) >
                    (8 * stream.streamLength)))
                {
                    errorCode = 1;
                    break;
                }
                //PTR_JPGC->frame.Ri = headerLength;
				LOGYC("JpegDecGetImageInfo ERROR: Unsupported DRI.");
		//		*ret |= JPEGDEC_HAVE_DRI;
                break;
                /* application segments */
            case APP0:
				LOGYC("JpegDecGetImageInfo get APP0\n");
				if(pImageInfo->thumbnailType != JPEGDEC_NO_THUMBNAIL){
					LOGYC("APP0 warning-------------------- thumbnail has been catch.");
					/* APPn length */
					//LOGYC("JpegDecGetImageInfo get other APP\n");
                	headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                	if(headerLength == STRM_ERROR ||
                  	 ((stream.readBits + ((headerLength * 8) - 16)) >
                  	  (8 * stream.streamLength)))
                		{
                 		   	errorCode = 1;
                    		break;
                		}
                		/* jump over APPn header */
                		if(headerLength != 0)
                		{
                    		stream.readBits += ((headerLength * 8) - 16);
							if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8))
							{
								errorCode = 1;
                    			break;
							}
                    		//stream.pCurrPos += (((headerLength * 8) - 16) / 8);
                		}	
					break;
				}
                /* reset */
                appBits = 0;
                appLength = 0;
                stream.appnFlag = 0;
				thumbnail = 0;
				initThumb = 0;

                /* APP0 length */
                headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                if(headerLength == STRM_ERROR ||
                   ((stream.readBits + ((headerLength * 8) - 16)) >
                    (8 * stream.streamLength)))
                {
                    errorCode = 1;
                    break;
                }
                appLength = headerLength;
                if(appLength < 16)
                {
                    stream.appnFlag = 1;
                    if(hw_JpegDecFlushBits(&(stream), ((appLength * 8) - 16), src) ==
                       STRM_ERROR)
                    {
                        errorCode = 1;
                        break;
                    }
                    break;
                }
                appBits += 16;

                /* check identifier */
                currentBytes = hw_JpegDecGet2Bytes(&(stream), src);
                appBits += 16;
                if(currentBytes != 0x4A46)
                {
                    stream.appnFlag = 1;
                    if(hw_JpegDecFlushBits(&(stream), ((appLength * 8) - appBits), src)
                       == STRM_ERROR)
                    {
                        errorCode = 1;
                        break;
                    }
                    break;
                }
                currentBytes = hw_JpegDecGet2Bytes(&(stream), src);
                appBits += 16;
                if(currentBytes != 0x4946 && currentBytes != 0x5858)
                {
                    stream.appnFlag = 1;
                    if(hw_JpegDecFlushBits(&(stream), ((appLength * 8) - appBits), src)
                       == STRM_ERROR)
                    {
                        errorCode = 1;
                        break;
                    }
                    break;
                }

                /* APP0 Extended */
                if(currentBytes == 0x5858)
                {
                    thumbnail = 1;
                }
                currentByte = hw_JpegDecGetByte(&(stream), src);
                appBits += 8;
                if(currentByte != 0x00)
                {
                    stream.appnFlag = 1;
                    if(hw_JpegDecFlushBits(&(stream), ((appLength * 8) - appBits), src)
                       == STRM_ERROR)
                    {
                        errorCode = 1;
                        break;
                    }
                    stream.appnFlag = 0;
                    break;
                }

                /* APP0 Extended thumb type */
                if(thumbnail)
                {
                    /* extension code */
                    currentByte = hw_JpegDecGetByte(&(stream), src);
                    if(currentByte == JPEGDEC_THUMBNAIL_JPEG)
                    {
                        pImageInfo->thumbnailType = JPEGDEC_THUMBNAIL_JPEG;
                        appBits += 8;
                        stream.appnFlag = 1;

                        /* check thumbnail data */
                        Hmax = 0;
                        Vmax = 0;
						
						hwInfo->streamCtl.thumbOffset = src->cur_offset_instream - src->bytes_in_buffer;
						hwInfo->streamCtl.thumbLength = (appLength * 8) - appBits;
						LOGYC("app0 thumb offset,length: %d,%d",hwInfo->streamCtl.thumbOffset,hwInfo->streamCtl.thumbLength);

                        /* Read decoding parameters */
                        for(; (stream.readBits / 8) < stream.streamLength;
                            stream.readBits++)
                        {
                            /* Look for marker prefix byte from stream */
                            appBits += 8;
                            if(hw_JpegDecGetByte(&(stream), src) == 0xFF)
                            {
                                /* switch to certain header decoding */
                                appBits += 8;

                                currentByte = hw_JpegDecGetByte(&(stream), src);
                                switch (currentByte)
                                {
                                    /* baseline marker */
                                case SOF0:
                                    /* progresive marker */
                                case SOF2:
                                    if(currentByte == SOF0)
                                        pImageInfo->codingModeThumb =
                                            //PTR_JPGC->info.operationTypeThumb =
                                            JPEGDEC_BASELINE;
                                    else
                                        pImageInfo->codingModeThumb =
                                            //PTR_JPGC->info.operationTypeThumb =
                                            JPEGDEC_PROGRESSIVE;
                                    /* Frame header */
                                    i++;

                                    /* jump over Lf field */
                                    if(hw_JpegDecFlushBits(&(stream), 16, src) ==
                                       STRM_ERROR)
                                    {
                                        errorCode = 1;
                                        break;
                                    }
                                    appBits += 16;

                                    /* Sample precision (only 8 bits/sample supported) */
                                    currentByte = hw_JpegDecGetByte(&(stream), src);
                                    appBits += 8;
                                    if(currentByte != 8)
                                    {
                                        LOGYC("JpegDecGetImageInfo ERROR: Thumbnail Sample precision");
                                        *ret |= JPEGDEC_THUMB_SAMPLE_UNSUPPORT;
                                    }

                                    /* Number of Lines */
                                    pImageInfo->outputHeightThumb =
                                        hw_JpegDecGet2Bytes(&(stream), src);
                                    appBits += 16;
                                    pImageInfo->displayHeightThumb =
                                        pImageInfo->outputHeightThumb;
                                    if(pImageInfo->outputHeightThumb < 1)
                                    {
                                        LOGYC("JpegDecGetImageInfo ERROR: pImageInfo->outputHeightThumb unsupported");
										*ret |= JPEGDEC_THUMB_UNSUPPORTEDSIZE;                                        
										//return (JPEGDEC_UNSUPPORTED);
                                    }
#ifdef JPEGDEC_ERROR_RESILIENCE
                                    if((pImageInfo->outputHeightThumb & 0xF) &&
                                       (pImageInfo->outputHeightThumb & 0xF) <=
                                       8)
                                        errorResilienceThumb = 1;
#endif /* JPEGDEC_ERROR_RESILIENCE */

                                    /* round up to next multiple-of-16 */
                                    pImageInfo->outputHeightThumb += 0xf;
                                    pImageInfo->outputHeightThumb &= ~(0xf);

                                    /* Number of Samples per Line */
                                    pImageInfo->outputWidthThumb =
                                        hw_JpegDecGet2Bytes(&(stream), src);
                                    appBits += 16;
                                    pImageInfo->displayWidthThumb =
                                        pImageInfo->outputWidthThumb;
                                    if(pImageInfo->outputWidthThumb < 1)
                                    {
                                        LOGYC("JpegDecGetImageInfo ERROR: pImageInfo->outputWidthThumb unsupported");
                                        *ret |= JPEGDEC_THUMB_UNSUPPORTEDSIZE;
										//return (JPEGDEC_UNSUPPORTED);
                                    }
                                    pImageInfo->outputWidthThumb += 0xf;
                                    pImageInfo->outputWidthThumb &= ~(0xf);
                                    if(pImageInfo->outputWidthThumb <
                                       JPEGDEC_MIN_WIDTH ||
                                       pImageInfo->outputHeightThumb <
                                       JPEGDEC_MIN_HEIGHT ||
                                       pImageInfo->outputWidthThumb >
                                       JPEGDEC_MAX_WIDTH_8190 ||
                                       pImageInfo->outputHeightThumb >
                                       JPEGDEC_MAX_HEIGHT_8190)
                                    {

                                        LOGYC("JpegDecGetImageInfo ERROR: Thumbnail Unsupported size");
										*ret |= JPEGDEC_THUMB_UNSUPPORTEDSIZE;
                                        //return (JPEGDEC_UNSUPPORTED);
                                    }

                                    /* Number of Image Components per Frame */
                                    Nf = hw_JpegDecGetByte(&(stream), src);
                                    appBits += 8;
                                    if(Nf != 3 && Nf != 1)
                                    {
                                        LOGYC("JpegDecGetImageInfo ERROR: Thumbnail Number of Image Components per Frame");
										*ret |= JPEGDEC_THUMB_YUV_UNSUPPORTED;
                                        //return (JPEGDEC_UNSUPPORTED);
                                    }
                                    for(j = 0; j < Nf; j++)
                                    {

                                        /* jump over component identifier */
                                        if(hw_JpegDecFlushBits(&(stream), 8, src) ==
                                           STRM_ERROR)
                                        {
                                            errorCode = 1;
                                            break;
                                        }
                                        appBits += 8;

                                        /* Horizontal sampling factor */
                                        currentByte = hw_JpegDecGetByte(&(stream), src);
                                        appBits += 8;
                                        Htn[j] = (currentByte >> 4);

                                        /* Vertical sampling factor */
                                        Vtn[j] = (currentByte & 0xF);

                                        /* jump over Tq */
                                        if(hw_JpegDecFlushBits(&(stream), 8, src) ==
                                           STRM_ERROR)
                                        {
                                            errorCode = 1;
                                            break;
                                        }
                                        appBits += 8;

                                        if(Htn[j] > Hmax)
                                            Hmax = Htn[j];
                                        if(Vtn[j] > Vmax)
                                            Vmax = Vtn[j];
                                    }
                                    if(Hmax == 0 || Vmax == 0)
                                    {
                                        LOGYC("JpegDecGetImageInfo ERROR: Thumbnail Hmax == 0 || Vmax == 0");
                                        //return (JPEGDEC_UNSUPPORTED);
										*ret |= JPEGDEC_THUMB_YUV_UNSUPPORTED;
                                    }
#ifdef JPEGDEC_ERROR_RESILIENCE
                                    if(Htn[0] == 2 && Vtn[0] == 2 &&
                                       Htn[1] == 1 && Vtn[1] == 1 &&
                                       Htn[2] == 1 && Vtn[2] == 1)
                                    {
                                        pImageInfo->outputFormatThumb =
                                            JPEGDEC_YCbCr420_SEMIPLANAR;
                                    }
                                    else
                                    {
                                        /* check if fill needed */
                                        if(errorResilienceThumb)
                                        {
                                            pImageInfo->outputHeightThumb -= 16;
                                            pImageInfo->displayHeightThumb =
                                                pImageInfo->outputHeightThumb;
                                        }
                                    }
#endif /* JPEGDEC_ERROR_RESILIENCE */

                                    /* check format */
                                    if(Htn[0] == 2 && Vtn[0] == 2 &&
                                       Htn[1] == 1 && Vtn[1] == 1 &&
                                       Htn[2] == 1 && Vtn[2] == 1)
                                    {
                                        pImageInfo->outputFormatThumb =
                                            JPEGDEC_YCbCr420_SEMIPLANAR;
                                    }
                                    else if(Htn[0] == 2 && Vtn[0] == 1 &&
                                            Htn[1] == 1 && Vtn[1] == 1 &&
                                            Htn[2] == 1 && Vtn[2] == 1)
                                    {
                                        pImageInfo->outputFormatThumb =
                                            JPEGDEC_YCbCr422_SEMIPLANAR;
                                    }
                                    else if(Htn[0] == 1 && Vtn[0] == 2 &&
                                            Htn[1] == 1 && Vtn[1] == 1 &&
                                            Htn[2] == 1 && Vtn[2] == 1)
                                    {
                                        pImageInfo->outputFormatThumb =
                                            JPEGDEC_YCbCr440;
                                    }
                                    else if(Htn[0] == 1 && Vtn[0] == 1 &&
                                            Htn[1] == 0 && Vtn[1] == 0 &&
                                            Htn[2] == 0 && Vtn[2] == 0)
                                    {
                                        pImageInfo->outputFormatThumb =
                                            JPEGDEC_YCbCr400;
                                    }
                                    else if(//PTR_JPGC->is8190 &&
                                            Htn[0] == 4 && Vtn[0] == 1 &&
                                            Htn[1] == 1 && Vtn[1] == 1 &&
                                            Htn[2] == 1 && Vtn[2] == 1)
                                    {
                                        pImageInfo->outputFormatThumb =
                                            JPEGDEC_YCbCr411_SEMIPLANAR;
                                    }
                                    else if(//PTR_JPGC->is8190 &&
                                            Htn[0] == 1 && Vtn[0] == 1 &&
                                            Htn[1] == 1 && Vtn[1] == 1 &&
                                            Htn[2] == 1 && Vtn[2] == 1)
                                    {
                                        pImageInfo->outputFormatThumb =
                                            JPEGDEC_YCbCr444_SEMIPLANAR;
                                    }
                                    else
                                    {
										*ret |= JPEGDEC_THUMB_YUV_UNSUPPORTED;
                                        LOGYC("JpegDecGetImageInfo ERROR: Thumbnail Unsupported YCbCr format");
                                        //return (JPEGDEC_UNSUPPORTED);
                                    }
                                    //PTR_JPGC->info.initThumb = 1;
                                    initThumb = 1;
                                    break;
                                case SOS:
                                    /* SOS length */
                                    headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                                    if(headerLength == STRM_ERROR ||
                                       ((stream.readBits +
                                         ((headerLength * 8) - 16)) >
                                        (8 * stream.streamLength)))
                                    {
                                        errorCode = 1;
                                        break;
                                    }

                                    /* check if interleaved or non-ibnterleaved */
                                    NsThumb = hw_JpegDecGetByte(&(stream), src);
                                    if(NsThumb == MIN_NUMBER_OF_COMPONENTS &&
                                       pImageInfo->outputFormatThumb !=
                                       JPEGDEC_YCbCr400 &&
                                       pImageInfo->codingModeThumb ==
                                       JPEGDEC_BASELINE)
                                    {
                                        pImageInfo->codingModeThumb =
                                            //PTR_JPGC->info.operationTypeThumb =
                                            JPEGDEC_NONINTERLEAVED;
                                    }

                                    /* jump over SOS header */
                                    if(headerLength != 0)
                                    {
                                        stream.readBits +=
                                            ((headerLength * 8) - 16);
										if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
											errorCode = 1;
                    						break;
										}
                                        //stream.pCurrPos +=
                                        //    (((headerLength * 8) - 16) / 8);
                                    }

                                    if((stream.readBits + 8) <
                                       (8 * stream.streamLength))
                                    {
                                        //PTR_JPGC->info.init = 1;
                                        //init = 1;
                                    }
                                    else
                                    {
                                        LOGYC("JpegDecGetImageInfo ERROR: Needs to increase input buffer");
                                        return (JPEGDEC_INCREASE_INPUT_BUFFER);
                                    }
                                    break;
                                case DQT:
                                    /* DQT length */
                                    headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                                    if(headerLength == STRM_ERROR)
                                    {
                                        errorCode = 1;
                                        break;
                                    }
                                    /* jump over DQT header */
                                    if(headerLength != 0)
                                    {
                                        stream.readBits +=
                                            ((headerLength * 8) - 16);
										if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8))
										{
											errorCode = 1;
                    						break;
										}
                                        //stream.pCurrPos +=
                                        //    (((headerLength * 8) - 16) / 8);
                                    }
                                    appBits += (headerLength * 8);
                                    break;
                                case DHT:
                                    /* DHT length */
                                    headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                                    if(headerLength == STRM_ERROR)
                                    {
                                        errorCode = 1;
                                        break;
                                    }
                                    /* jump over DHT header */
                                    if(headerLength != 0)
                                    {
                                        stream.readBits +=
                                            ((headerLength * 8) - 16);
										if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8))
										{
											errorCode = 1;
                    						break;
										}
                                        //stream.pCurrPos +=
                                        //    (((headerLength * 8) - 16) / 8);
                                    }
                                    appBits += (headerLength * 8);
                                    break;
                                case DRI:
                                    /* DRI length */
                                    headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                                    if(headerLength == STRM_ERROR)
                                    {
                                        errorCode = 1;
                                        break;
                                    }
                                    /* jump over DRI header */
                                    if(headerLength != 0)
                                    {
                                        stream.readBits +=
                                            ((headerLength * 8) - 16);
										if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
											errorCode = 1;
                    						break;
										}
                                        //stream.pCurrPos +=
                                        //    (((headerLength * 8) - 16) / 8);
                                    }
                                    appBits += (headerLength * 8);
				//LOGYC("JpegDecGetImageInfo thumb ERROR: Unsupported DRI.");
				//*ret |= JPEGDEC_THUMB_HAVE_DRI;
                                    break;
                                case APP0:
                                case APP1:
                                case APP2:
                                case APP3:
                                case APP4:
                                case APP5:
                                case APP6:
                                case APP7:
                                case APP8:
                                case APP9:
                                case APP10:
                                case APP11:
                                case APP12:
                                case APP13:
                                case APP14:
                                case APP15:
                                    /* APPn length */
                                    headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                                    if(headerLength == STRM_ERROR)
                                    {
                                        errorCode = 1;
                                        break;
                                    }
                                    /* jump over APPn header */
                                    if(headerLength != 0)
                                    {
                                        stream.readBits +=
                                            ((headerLength * 8) - 16);
										if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
											errorCode = 1;
                    						break;
										}
                                        //stream.pCurrPos +=
                                        //    (((headerLength * 8) - 16) / 8);
                                    }
                                    appBits += (headerLength * 8);
                                    break;
                                case DNL:
                                    /* DNL length */
                                    headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                                    if(headerLength == STRM_ERROR)
                                    {
                                        errorCode = 1;
                                        break;
                                    }
                                    /* jump over DNL header */
                                    if(headerLength != 0)
                                    {
                                        stream.readBits +=
                                            ((headerLength * 8) - 16);
                                        if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
											errorCode = 1;
                    						break;
										}
										//stream.pCurrPos +=
                                        //    (((headerLength * 8) - 16) / 8);
                                    }
                                    appBits += (headerLength * 8);
                                    break;
								/* unsupported coding styles */
                                case SOF1:
                                case SOF3:
                                case SOF5:
                                case SOF6:
                                case SOF7:
                                case SOF9:
                                case SOF10:
                                case SOF11:
                                case SOF13:
                                case SOF14:
                                case SOF15:
                                case DAC:
                                case DHP:
                                    LOGYC("JpegDecGetImageInfo ERROR: Unsupported coding styles");
                                    //return (JPEGDEC_UNSUPPORTED);
                                case COM:
                                    /* COM length */
                                    headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                                    if(headerLength == STRM_ERROR)
                                    {
                                        errorCode = 1;
                                        break;
                                    }
                                    /* jump over COM header */
                                    if(headerLength != 0)
                                    {
                                        stream.readBits +=
                                            ((headerLength * 8) - 16);
										if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
											errorCode = 1;
                    						break;
										}
                                        //stream.pCurrPos +=
                                        //    (((headerLength * 8) - 16) / 8);
                                    }
                                    appBits += (headerLength * 8);
                                    break;
                                default:
                                    break;
                                }
                                if(initThumb)
                                {
                                    /* flush the rest of thumbnail data */
                                    if(hw_JpegDecFlushBits
                                       (&(stream),
                                        ((appLength * 8) - appBits), src) ==
                                       STRM_ERROR)
                                    {
                                        errorCode = 1;
                                        break;
                                    }
                                    stream.appnFlag = 0;
                                    break;
                                }
                            }
                            else
                            {
                                if(!initThumb &&
                                   pDecIn->bufferSize)
                                    return (JPEGDEC_INCREASE_INPUT_BUFFER);
                                else
                                    return (JPEGDEC_STRM_ERROR);
                            }
                        }
                        break;
                    }
                    else
                    {
                        appBits += 8;
                        pImageInfo->thumbnailType =
                            JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT;
                        stream.appnFlag = 1;
                        if(hw_JpegDecFlushBits
                           (&(stream),
                            ((appLength * 8) - appBits), src) == STRM_ERROR)
                        {
                            errorCode = 1;
                            break;
                        }
                        stream.appnFlag = 0;
                        break;
                    }
                }
                else
                {
                    /* version */
                    pImageInfo->version = hw_JpegDecGet2Bytes(&(stream), src);
                    appBits += 16;

                    /* units */
                    currentByte = hw_JpegDecGetByte(&(stream), src);
                    if(currentByte == 0)
                    {
                        pImageInfo->units = JPEGDEC_NO_UNITS;
                    }
                    else if(currentByte == 1)
                    {
                        pImageInfo->units = JPEGDEC_DOTS_PER_INCH;
                    }
                    else if(currentByte == 2)
                    {
                        pImageInfo->units = JPEGDEC_DOTS_PER_CM;
                    }
                    appBits += 8;

                    /* Xdensity */
                    pImageInfo->xDensity = hw_JpegDecGet2Bytes(&(stream), src);
                    appBits += 16;

                    /* Ydensity */
                    pImageInfo->yDensity = hw_JpegDecGet2Bytes(&(stream), src);
                    appBits += 16;

                    /* jump over rest of header data */
                    stream.appnFlag = 1;
                    if(hw_JpegDecFlushBits(&(stream), ((appLength * 8) - appBits), src)
                       == STRM_ERROR)
                    {
                        errorCode = 1;
                        break;
                    }
                    stream.appnFlag = 0;
                    break;
                }
            case APP1:
				/*codes is ugly below, fix someday. maybe.*/
				LOGYC("JpegDecGetImageInfo GET APP1 MARKER.");
				if(pImageInfo->thumbnailType != JPEGDEC_NO_THUMBNAIL){
					LOGYC("APP1 warning-------------------- thumbnail has been catch.");
					/* APPn length */
					//LOGYC("JpegDecGetImageInfo get other APP\n");
                	headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                	if(headerLength == STRM_ERROR ||
                  	 ((stream.readBits + ((headerLength * 8) - 16)) >
                  	  (8 * stream.streamLength)))
                		{
                 		   	errorCode = 1;
                    		break;
                		}
                		/* jump over APPn header */
                		if(headerLength != 0)
                		{
                    		stream.readBits += ((headerLength * 8) - 16);
							if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
								errorCode = 1;
                    			break;
							}
                    		//stream.pCurrPos += (((headerLength * 8) - 16) / 8);
                		}	
					break;
				}
				/*reset*/
				appBits = 0;
				appLength = 0;
				stream.appnFlag = 0;
				initThumb = 0;
				pImageInfo->thumbnailType = JPEGDEC_NO_THUMBNAIL;
				/*APP1 length*/
				headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                if(headerLength == STRM_ERROR ||
                   ((stream.readBits + ((headerLength * 8) - 16)) >
                    (8 * stream.streamLength)))
                {
                    errorCode = 1;
                    break;
                }
                appLength = headerLength;
                appBits += 16;
				
				/* check identifier */
                currentBytes = hw_JpegDecGet2Bytes(&(stream), src);
                appBits += 16;
				if(currentBytes != 0x4578)//'E' 'x'
                {
					LOGYC("APP1, do not read E x. Is 0x%08x", currentBytes);
                    stream.appnFlag = 1;
                    if(hw_JpegDecFlushBits(&(stream), ((appLength * 8) - appBits), src)
                       == STRM_ERROR)
                    {
                        errorCode = 1;
                        break;
                    }
                    break;
                }
				currentBytes = hw_JpegDecGet2Bytes(&(stream), src);
                appBits += 16;
				if(currentBytes != 0x6966)//'i' 'f'
                {
					LOGYC("APP1, do not read i f.Is 0x%08x", currentBytes);
                    stream.appnFlag = 1;
                    if(hw_JpegDecFlushBits(&(stream), ((appLength * 8) - appBits), src)
                       == STRM_ERROR)
                    {
                        errorCode = 1;
                        break;
                    }
                    break;
                }
				currentBytes = hw_JpegDecGet2Bytes(&(stream), src);
                appBits += 16;
				if(currentBytes != 0x0000)
                {
					LOGYC("APP1, do not read 0x0000.Is 0x%08x", currentBytes);
                    stream.appnFlag = 1;
                    if(hw_JpegDecFlushBits(&(stream), ((appLength * 8) - appBits), src)
                       == STRM_ERROR)
                    {
                        errorCode = 1;
                        break;
                    }
                    break;
                }
			HW_BOOL breaknow = 0;
			do{				
				JpegStream backupStream = stream;
				RK_U32 app1_readedBytes = 0;
				int bytesAlign = INTEL_ALIGN;
				RK_U32 numBytesCom = 0;
				RK_U16 dataformat = 0;
				RK_U16 num_directory_entry = 0;
				RK_U16 tagNum = 0;
				
				/* discover byte order */
				currentBytes = hw_JpegDecGet2Bytes(&(stream), src);
                appBits += 16;	
				app1_readedBytes += 2;
				if(currentBytes != 0x4949 && currentBytes != 0x4D4D) //'II' 'MM'
                {
					LOGYC("APP1, do not read right bytes align, : %d.", currentBytes);
                    stream.appnFlag = 1;
                    if(hw_JpegDecFlushBits(&(stream), ((appLength * 8) - appBits), src)
                       == STRM_ERROR)
                    {
                        errorCode = 1;
                        break;
                    }
                    break;
                }
				if(currentBytes == 0x4D4D){
					bytesAlign = MOTOROLA_ALIGN;
				}
				
				/* 0x2A 0x00 */
				currentBytes = hw_JpegDecGet2Bytes(&(stream), src);
				REVERSE_LOW_TWO_BYTES(bytesAlign, currentBytes);
                appBits += 16;
				app1_readedBytes += 2;
				if(currentBytes != 0x002A) //0x2A 0x00
                {
					LOGYC("APP1, do not read 0x002A. Is 0x%08x", currentBytes);
                    stream.appnFlag = 1;
                    if(hw_JpegDecFlushBits(&(stream), ((appLength * 8) - appBits), src)
                       == STRM_ERROR)
                    {
                        errorCode = 1;
                        break;
                    }
                    break;
                } 
				
				/* offset to first IFD */
				currentBytes = (hw_JpegDecGet2Bytes(&(stream), src) << 16) | hw_JpegDecGet2Bytes(&(stream), src);//usually 0x00000008
                appBits += 32;
				app1_readedBytes += 4;
				REVERSE_BYTES(bytesAlign, currentBytes);
				//if not 08
				if(currentBytes > 8){
					currentBytes -= 8;
					LOGYC("offset to IFD0 remain bytes: %d", currentBytes);
					if(!(*src->skip_input_data)(src->info, currentBytes)){
						errorCode = 1;
                    	break;
					}
					stream.readBits += (currentBytes)*8;
					appBits += (currentBytes)*8;
					app1_readedBytes += currentBytes;
				}
				
				/* num of directory entry */
				currentBytes = hw_JpegDecGet2Bytes(&(stream), src);
				REVERSE_LOW_TWO_BYTES(bytesAlign, currentBytes);
				appBits += 16;
				app1_readedBytes += 2;
				LOGYC("num of directory entry in IFD0: %d", currentBytes);
				//skip entries, 12 bytes per entry.
				//Tag num : 2 bytes, kind of data : 2 bytes, num of components: 4 bytes, a data value or offset to data value: 4 bytes
				if(!(*src->skip_input_data)(src->info, currentBytes*12)){
					errorCode = 1;
                    break;
				}
				stream.readBits += (currentBytes*12)*8;
				appBits += (currentBytes*12)*8;
				app1_readedBytes += currentBytes*12;
				/*offset next IFD*/
				currentBytes = (hw_JpegDecGet2Bytes(&(stream), src) << 16) | hw_JpegDecGet2Bytes(&(stream), src);
				appBits += 32;
				app1_readedBytes += 4;
				REVERSE_BYTES(bytesAlign, currentBytes);
				LOGYC("offset next IFD from 0x4949: %d", currentBytes);
				if(currentBytes == 0 || currentBytes >= appLength - 8){//0 means no IFD1, no thumbnail image, or have error length
					if(appLength*8 - appBits != 0)
					{
						if(!(*src->skip_input_data)(src->info, appLength-appBits/8)){
							errorCode = 1;
                    		break;
						}
						stream.readBits += (appLength*8 - appBits);
					}
					break;			
				}
				//skip Data area of IFD0: Exif SubIFD(Data area of Exif SubIFD: Interoperability IFD or Makernote IFD)
				if(!(*src->skip_input_data)(src->info, currentBytes - app1_readedBytes)){
					errorCode = 1;
                    break;
				}
				stream.readBits += (currentBytes - app1_readedBytes)*8;
				appBits += (currentBytes - app1_readedBytes)*8;
				app1_readedBytes = currentBytes;
				//thumbnail image usually is 160*120
				//IFD1
				num_directory_entry = hw_JpegDecGet2Bytes(&(stream), src);
				REVERSE_LOW_TWO_BYTES(bytesAlign, num_directory_entry);
				appBits += 16;
				app1_readedBytes += 2;
				LOGYC("num of directory entry in IFD1: %d", num_directory_entry);
				for(;num_directory_entry > 0 && !breaknow;num_directory_entry--){
					tagNum = hw_JpegDecGet2Bytes(&(stream), src);
					REVERSE_LOW_TWO_BYTES(bytesAlign, tagNum);
					appBits += 16;
					app1_readedBytes += 2;
					/*data format*/
					currentBytes = hw_JpegDecGet2Bytes(&(stream), src);
					REVERSE_LOW_TWO_BYTES(bytesAlign, currentBytes);
					appBits += 16;
					app1_readedBytes += 2;
					dataformat = DataFormat[currentBytes - 1];
					/*num of component*/
					numBytesCom = (hw_JpegDecGet2Bytes(&(stream), src) << 16) | hw_JpegDecGet2Bytes(&(stream), src);
					appBits += 32;
					app1_readedBytes += 4;
					REVERSE_BYTES(bytesAlign, numBytesCom);
					/* value or offset of value */
					currentBytes = (hw_JpegDecGet2Bytes(&(stream), src) << 16) | hw_JpegDecGet2Bytes(&(stream), src);
					appBits += 32;
					app1_readedBytes += 4;
					REVERSE_BYTES(bytesAlign, currentBytes);
					
					switch(tagNum){
						case Compression_Tag:
							if(currentBytes == 0x06){
								pImageInfo->thumbnailType = JPEGDEC_THUMBNAIL_JPEG;
							} else {
								pImageInfo->thumbnailType =
                            		JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT;
								LOGYC("APP1 contains non-jpeg thumbnail.");
							}
							break;
						case JpegIFOffset_Tag:
							LOGYC("app1 jpeg thumb offset : 0x%08x to IIMM", currentBytes);
							hwInfo->streamCtl.thumbOffset = backupStream.readBits/8 + currentBytes;
							if(currentBytes >= appLength - 8){
								LOGYC("jpegifoffset is wrong value, greater than appLength");
								breaknow = 1;
								break;							
							}
							LOGYC("app1 jpeg thumb offset : 0x%08x in total file", hwInfo->streamCtl.thumbOffset);
							break;
						case JpegIFByteCount_Tag:
							LOGYC("app1 jpeg thumb length : 0x%08x", currentBytes);
							hwInfo->streamCtl.thumbLength = currentBytes;
							if(currentBytes >= appLength - 8){
								LOGYC("jpegifbyteCount is wrong value, greater than appLength");
								breaknow = 1;
								break;							
							}
							break;
						default:
							break;
					}
				}
				if(breaknow)break;
				if(pImageInfo->thumbnailType == JPEGDEC_THUMBNAIL_JPEG){
					currentBytes = hwInfo->streamCtl.thumbOffset - stream.readBits/8;
					LOGYC("start thumb jpeg: skip 0x%x",currentBytes);
					if(!(*src->skip_input_data)(src->info, currentBytes)){
						errorCode = 1;
                    	break;
					}
					stream.readBits += currentBytes;
					appBits += currentBytes*8;
					app1_readedBytes += currentBytes;
					stream.appnFlag = 1;

                    /* check thumbnail data */
                    Hmax = 0;
                    Vmax = 0;
                    /* Read decoding parameters */
                	for(app1_readedBytes = appBits;(appBits - app1_readedBytes) < hwInfo->streamCtl.thumbLength*8;)
                    {
                       /* Look for marker prefix byte from stream */
                       appBits += 8;
                       if(hw_JpegDecGetByte(&(stream), src) == 0xFF)
                       {
                         	/* switch to certain header decoding */
                        	appBits += 8;
							currentByte = hw_JpegDecGetByte(&(stream), src);
                            switch (currentByte)
                            {
								/* baseline marker */
                                case SOF0:
                                /* progresive marker */
                                case SOF2:
                                    if(currentByte == SOF0)
                                        pImageInfo->codingModeThumb =
                                            JPEGDEC_BASELINE;
                                    else
                                        pImageInfo->codingModeThumb =
                                            JPEGDEC_PROGRESSIVE;
                                    /* jump over Lf field */
                                    if(hw_JpegDecFlushBits(&(stream), 16, src) ==
                                       STRM_ERROR)
                                    {
                                        errorCode = 1;
                                        break;
                                    }
                                    appBits += 16;

                                    /* Sample precision (only 8 bits/sample supported) */
                                    currentByte = hw_JpegDecGetByte(&(stream), src);
                                    appBits += 8;
                                    if(currentByte != 8)
                                    {
                                        LOGYC("JpegDecGetImageInfo ERROR: Thumbnail Sample precision");
                                        *ret |= JPEGDEC_THUMB_SAMPLE_UNSUPPORT;
                                    }

                                    /* Number of Lines */
                                    pImageInfo->outputHeightThumb =
                                        hw_JpegDecGet2Bytes(&(stream), src);
                                    appBits += 16;
                                    pImageInfo->displayHeightThumb =
                                        pImageInfo->outputHeightThumb;
                                    if(pImageInfo->outputHeightThumb < 1)
                                    {
                                        LOGYC("JpegDecGetImageInfo ERROR: pImageInfo->outputHeightThumb unsupported");
										*ret |= JPEGDEC_THUMB_UNSUPPORTEDSIZE;                                        
										//return (JPEGDEC_UNSUPPORTED);
                                    }
#ifdef JPEGDEC_ERROR_RESILIENCE
                                    if((pImageInfo->outputHeightThumb & 0xF) &&
                                       (pImageInfo->outputHeightThumb & 0xF) <=
                                       8)
                                        errorResilienceThumb = 1;
#endif /* JPEGDEC_ERROR_RESILIENCE */

                                    /* round up to next multiple-of-16 */
                                    pImageInfo->outputHeightThumb += 0xf;
                                    pImageInfo->outputHeightThumb &= ~(0xf);

                                    /* Number of Samples per Line */
                                    pImageInfo->outputWidthThumb =
                                        hw_JpegDecGet2Bytes(&(stream), src);
                                    appBits += 16;
                                    pImageInfo->displayWidthThumb =
                                        pImageInfo->outputWidthThumb;
                                    if(pImageInfo->outputWidthThumb < 1)
                                    {
                                        LOGYC("JpegDecGetImageInfo ERROR: pImageInfo->outputWidthThumb unsupported");
                                        *ret |= JPEGDEC_THUMB_UNSUPPORTEDSIZE;
										//return (JPEGDEC_UNSUPPORTED);
                                    }
                                    pImageInfo->outputWidthThumb += 0xf;
                                    pImageInfo->outputWidthThumb &= ~(0xf);
                                    if(pImageInfo->outputWidthThumb <
                                       JPEGDEC_MIN_WIDTH ||
                                       pImageInfo->outputHeightThumb <
                                       JPEGDEC_MIN_HEIGHT ||
                                       pImageInfo->outputWidthThumb >
                                       JPEGDEC_MAX_WIDTH_8190 ||
                                       pImageInfo->outputHeightThumb >
                                       JPEGDEC_MAX_HEIGHT_8190)
                                    {

                                        LOGYC("JpegDecGetImageInfo ERROR: Thumbnail Unsupported size");
										*ret |= JPEGDEC_THUMB_UNSUPPORTEDSIZE;
                                        //return (JPEGDEC_UNSUPPORTED);
                                    }

                                    /* Number of Image Components per Frame */
                                    Nf = hw_JpegDecGetByte(&(stream), src);
                                    appBits += 8;
                                    if(Nf != 3 && Nf != 1)
                                    {
                                        LOGYC("JpegDecGetImageInfo ERROR: Thumbnail Number of Image Components per Frame");
										*ret |= JPEGDEC_THUMB_YUV_UNSUPPORTED;
                                        //return (JPEGDEC_UNSUPPORTED);
                                    }
                                    for(j = 0; j < Nf; j++)
                                    {

                                        /* jump over component identifier */
                                        if(hw_JpegDecFlushBits(&(stream), 8, src) ==
                                           STRM_ERROR)
                                        {
                                            errorCode = 1;
                                            break;
                                        }
                                        appBits += 8;

                                        /* Horizontal sampling factor */
                                        currentByte = hw_JpegDecGetByte(&(stream), src);
                                        appBits += 8;
                                        Htn[j] = (currentByte >> 4);

                                        /* Vertical sampling factor */
                                        Vtn[j] = (currentByte & 0xF);

                                        /* jump over Tq */
                                        if(hw_JpegDecFlushBits(&(stream), 8, src) ==
                                           STRM_ERROR)
                                        {
                                            errorCode = 1;
                                            break;
                                        }
                                        appBits += 8;

                                        if(Htn[j] > Hmax)
                                            Hmax = Htn[j];
                                        if(Vtn[j] > Vmax)
                                            Vmax = Vtn[j];
                                    }
                                    if(Hmax == 0 || Vmax == 0)
                                    {
                                        LOGYC("JpegDecGetImageInfo ERROR: Thumbnail Hmax == 0 || Vmax == 0");
                                        //return (JPEGDEC_UNSUPPORTED);
										*ret |= JPEGDEC_THUMB_YUV_UNSUPPORTED;
                                    }
#ifdef JPEGDEC_ERROR_RESILIENCE
                                    if(Htn[0] == 2 && Vtn[0] == 2 &&
                                       Htn[1] == 1 && Vtn[1] == 1 &&
                                       Htn[2] == 1 && Vtn[2] == 1)
                                    {
                                        pImageInfo->outputFormatThumb =
                                            JPEGDEC_YCbCr420_SEMIPLANAR;
                                    }
                                    else
                                    {
                                        /* check if fill needed */
                                        if(errorResilienceThumb)
                                        {
                                            pImageInfo->outputHeightThumb -= 16;
                                            pImageInfo->displayHeightThumb =
                                                pImageInfo->outputHeightThumb;
                                        }
                                    }
#endif /* JPEGDEC_ERROR_RESILIENCE */

                                    /* check format */
                                    if(Htn[0] == 2 && Vtn[0] == 2 &&
                                       Htn[1] == 1 && Vtn[1] == 1 &&
                                       Htn[2] == 1 && Vtn[2] == 1)
                                    {
                                        pImageInfo->outputFormatThumb =
                                            JPEGDEC_YCbCr420_SEMIPLANAR;
                                    }
                                    else if(Htn[0] == 2 && Vtn[0] == 1 &&
                                            Htn[1] == 1 && Vtn[1] == 1 &&
                                            Htn[2] == 1 && Vtn[2] == 1)
                                    {
                                        pImageInfo->outputFormatThumb =
                                            JPEGDEC_YCbCr422_SEMIPLANAR;
                                    }
                                    else if(Htn[0] == 1 && Vtn[0] == 2 &&
                                            Htn[1] == 1 && Vtn[1] == 1 &&
                                            Htn[2] == 1 && Vtn[2] == 1)
                                    {
                                        pImageInfo->outputFormatThumb =
                                            JPEGDEC_YCbCr440;
                                    }
                                    else if(Htn[0] == 1 && Vtn[0] == 1 &&
                                            Htn[1] == 0 && Vtn[1] == 0 &&
                                            Htn[2] == 0 && Vtn[2] == 0)
                                    {
                                        pImageInfo->outputFormatThumb =
                                            JPEGDEC_YCbCr400;
                                    }
                                    else if(//PTR_JPGC->is8190 &&
                                            Htn[0] == 4 && Vtn[0] == 1 &&
                                            Htn[1] == 1 && Vtn[1] == 1 &&
                                            Htn[2] == 1 && Vtn[2] == 1)
                                    {
                                        pImageInfo->outputFormatThumb =
                                            JPEGDEC_YCbCr411_SEMIPLANAR;
                                    }
                                    else if(//PTR_JPGC->is8190 &&
                                            Htn[0] == 1 && Vtn[0] == 1 &&
                                            Htn[1] == 1 && Vtn[1] == 1 &&
                                            Htn[2] == 1 && Vtn[2] == 1)
                                    {
                                        pImageInfo->outputFormatThumb =
                                            JPEGDEC_YCbCr444_SEMIPLANAR;
                                    }
                                    else
                                    {
										*ret |= JPEGDEC_THUMB_YUV_UNSUPPORTED;
                                        LOGYC("JpegDecGetImageInfo ERROR: Thumbnail Unsupported YCbCr format");
                                        //return (JPEGDEC_UNSUPPORTED);
                                    }
                                    //PTR_JPGC->info.initThumb = 1;
                                    initThumb = 1;
                                    break;
                                case SOS:
                                    /* SOS length */
                                    headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                                    if(headerLength == STRM_ERROR ||
                                       ((stream.readBits +
                                         ((headerLength * 8) - 16)) >
                                        (8 * stream.streamLength)))
                                    {
                                        errorCode = 1;
                                        break;
                                    }

                                    /* check if interleaved or non-ibnterleaved */
                                    NsThumb = hw_JpegDecGetByte(&(stream), src);
                                    if(NsThumb == MIN_NUMBER_OF_COMPONENTS &&
                                       pImageInfo->outputFormatThumb !=
                                       JPEGDEC_YCbCr400 &&
                                       pImageInfo->codingModeThumb ==
                                       JPEGDEC_BASELINE)
                                    {
                                        pImageInfo->codingModeThumb =
                                            JPEGDEC_NONINTERLEAVED;
                                    }

                                    /* jump over SOS header */
                                    if(headerLength != 0)
                                    {
                                        stream.readBits +=
                                            ((headerLength * 8) - 16);
										if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
											errorCode = 1;
                    						break;
										}
                                        //stream.pCurrPos +=
                                        //    (((headerLength * 8) - 16) / 8);
                                    }

                                    if((stream.readBits + 8) <
                                       (8 * stream.streamLength))
                                    {
                                        //PTR_JPGC->info.init = 1;
                                        //init = 1;
                                    }
                                    else
                                    {
                                        LOGYC("JpegDecGetImageInfo ERROR: Needs to increase input buffer");
                                        return (JPEGDEC_INCREASE_INPUT_BUFFER);
                                    }
                                    break;
                                case DQT:
                                    /* DQT length */
                                    headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                                    if(headerLength == STRM_ERROR)
                                    {
                                        errorCode = 1;
                                        break;
                                    }
                                    /* jump over DQT header */
                                    if(headerLength != 0)
                                    {
                                        stream.readBits +=
                                            ((headerLength * 8) - 16);
										if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
											errorCode = 1;
                    						break;
										}
                                        //stream.pCurrPos +=
                                        //    (((headerLength * 8) - 16) / 8);
                                    }
                                    appBits += (headerLength * 8);
                                    break;
                                case DHT:
                                    /* DHT length */
                                    headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                                    if(headerLength == STRM_ERROR)
                                    {
                                        errorCode = 1;
                                        break;
                                    }
                                    /* jump over DHT header */
                                    if(headerLength != 0)
                                    {
                                        stream.readBits +=
                                            ((headerLength * 8) - 16);
										if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
											errorCode = 1;
                    						break;
										}
                                        //stream.pCurrPos +=
                                        //    (((headerLength * 8) - 16) / 8);
                                    }
                                    appBits += (headerLength * 8);
                                    break;
                                case DRI:
                                    /* DRI length */
                                    headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                                    if(headerLength == STRM_ERROR)
                                    {
                                        errorCode = 1;
                                        break;
                                    }
                                    /* jump over DRI header */
                                    if(headerLength != 0)
                                    {
                                        stream.readBits +=
                                            ((headerLength * 8) - 16);
										if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
											errorCode = 1;
                    						break;
										}
                                        //stream.pCurrPos +=
                                        //    (((headerLength * 8) - 16) / 8);
                                    }
                                    appBits += (headerLength * 8);
									//LOGYC("JpegDecGetImageInfo thumb ERROR: Unsupported DRI.");
									//*ret |= JPEGDEC_THUMB_HAVE_DRI;
                                    break;
                                case APP0:
                                case APP1:
                                case APP2:
                                case APP3:
                                case APP4:
                                case APP5:
                                case APP6:
                                case APP7:
                                case APP8:
                                case APP9:
                                case APP10:
                                case APP11:
                                case APP12:
                                case APP13:
                                case APP14:
                                case APP15:
                                    /* APPn length */
                                    headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                                    if(headerLength == STRM_ERROR)
                                    {
                                        errorCode = 1;
                                        break;
                                    }
                                    /* jump over APPn header */
                                    if(headerLength != 0)
                                    {
                                        stream.readBits +=
                                            ((headerLength * 8) - 16);
										if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
											errorCode = 1;
                    						break;
										}
                                        //stream.pCurrPos +=
                                        //    (((headerLength * 8) - 16) / 8);
                                    }
                                    appBits += (headerLength * 8);
                                    break;
                                case DNL:
                                    /* DNL length */
                                    headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                                    if(headerLength == STRM_ERROR)
                                    {
                                        errorCode = 1;
                                        break;
                                    }
                                    /* jump over DNL header */
                                    if(headerLength != 0)
                                    {
                                        stream.readBits +=
                                            ((headerLength * 8) - 16);
                                        if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
											errorCode = 1;
                    						break;
										}
										//stream.pCurrPos +=
                                        //    (((headerLength * 8) - 16) / 8);
                                    }
                                    appBits += (headerLength * 8);
                                    break;
								/* unsupported coding styles */
                                case SOF1:
                                case SOF3:
                                case SOF5:
                                case SOF6:
                                case SOF7:
                                case SOF9:
                                case SOF10:
                                case SOF11:
                                case SOF13:
                                case SOF14:
                                case SOF15:
                                case DAC:
                                case DHP:
                                    LOGYC("JpegDecGetImageInfo ERROR: Unsupported coding styles");
                                    //return (JPEGDEC_UNSUPPORTED);
                                case COM:
                                    /* COM length */
                                    headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                                    if(headerLength == STRM_ERROR)
                                    {
                                        errorCode = 1;
                                        break;
                                    }
                                    /* jump over COM header */
                                    if(headerLength != 0)
                                    {
                                        stream.readBits +=
                                            ((headerLength * 8) - 16);
										if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
											errorCode = 1;
                    						break;
										}
                                        //stream.pCurrPos +=
                                        //    (((headerLength * 8) - 16) / 8);
                                    }
                                    appBits += (headerLength * 8);
                                    break;
                                default:
                                    break;
                                }
                                if(initThumb)
                                {
                                    /* flush the rest of thumbnail data */
                                    if(hw_JpegDecFlushBits
                                       (&(stream),
                                        ((appLength * 8) - appBits), src) ==
                                       STRM_ERROR)
                                    {
                                        errorCode = 1;
                                        break;
                                    }
                                    stream.appnFlag = 0;
                                    break;
                                }
                            }
                            else
                            {
                                if(!initThumb &&
                                   pDecIn->bufferSize)
                                    return (JPEGDEC_INCREASE_INPUT_BUFFER);
                                else
                                    return (JPEGDEC_STRM_ERROR);
                            }
                        }
                        break;
                    }
                    else
                    {
                        hwInfo->streamCtl.thumbLength = -1;
                        stream.appnFlag = 1;
                        if(hw_JpegDecFlushBits
                           (&(stream),
                            ((appLength * 8) - appBits), src) == STRM_ERROR)
                        {
                            errorCode = 1;
                            break;
                        }
                        stream.appnFlag = 0;
                        break;
                    }
			}while(0);
			if(breaknow){
				if(appLength*8 - appBits != 0)
				{
					LOGYC("app1, break now: %d,%d, %d", appLength, appBits/8, stream.readBits/8);
					if(!(*src->skip_input_data)(src->info, appLength-appBits/8)){
						errorCode = 1;
                    	break;
					}
					stream.readBits += (appLength*8 - appBits);
				}
			}
			break;
			case APP14:
				headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                if(headerLength == STRM_ERROR ||
                   ((stream.readBits + ((headerLength * 8) - 16)) >
                    (8 * stream.streamLength)))
                {
                    errorCode = 1;
                    break;
                }
                /* jump over APP14 header */
                if(headerLength != 14)
                {
                    stream.readBits += ((headerLength * 8) - 16);
					if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
						errorCode = 1;
                    	break;
					}
                    break;
                }
				appBits = 16;
				CHECK_BYTE(0x41);
				CHECK_BYTE(0x64);
				CHECK_BYTE(0x6F);
				CHECK_BYTE(0x62);
				CHECK_BYTE(0x65);
				if(headerLength-appBits/8 > 6){// version flags0 flags1
					stream.readBits += 6*8;
					if(!(*src->skip_input_data)(src->info,6)){
						errorCode = 1;
                    	break;
					}
					appBits += 6*8;
				}
				currentByte = hw_JpegDecGetByte(&(stream), src);
				appBits += 8;
				if(currentByte != 1){
					LOGYC("APP14, color space is not YCbCr");
                	//stream.readBits += ((headerLength * 8) - appBits);
					//(*src->skip_input_data)(src->info,((headerLength * 8) - appBits) / 8);
                	return JPEGDEC_UNSUPPORTED;
				}
				break;
            case APP2:
            case APP3:
            case APP4:
            case APP5:
            case APP6:
            case APP7:
            case APP8:
            case APP9:
            case APP10:
            case APP11:
            case APP12:
            case APP13:
            //case APP14:
            case APP15:
                /* APPn length */
				//LOGYC("JpegDecGetImageInfo get other APP\n");
                headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                if(headerLength == STRM_ERROR ||
                   ((stream.readBits + ((headerLength * 8) - 16)) >
                    (8 * stream.streamLength)))
                {
                    errorCode = 1;
                    break;
                }
                /* jump over APPn header */
                if(headerLength != 0)
                {
                    stream.readBits += ((headerLength * 8) - 16);
					if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
						errorCode = 1;
                    	break;
					}
                    //stream.pCurrPos += (((headerLength * 8) - 16) / 8);
                }
                break;
            case DNL:
                /* DNL length */
				//LOGYC("JpegDecGetImageInfo get DNL\n");
                headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                if(headerLength == STRM_ERROR ||
                   ((stream.readBits + ((headerLength * 8) - 16)) >
                    (8 * stream.streamLength)))
                {
                    errorCode = 1;
                    break;
                }
                /* jump over DNL header */
                if(headerLength != 0)
                {
                    stream.readBits += ((headerLength * 8) - 16);
					if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
						errorCode = 1;
                    	break;
					}
                    //stream.pCurrPos += (((headerLength * 8) - 16) / 8);
                }
                break;
            case COM:
                headerLength = hw_JpegDecGet2Bytes(&(stream), src);
                if(headerLength == STRM_ERROR ||
                   ((stream.readBits + ((headerLength * 8) - 16)) >
                    (8 * stream.streamLength)))
                {
                    errorCode = 1;
                    break;
                }
                /* jump over COM header */
                if(headerLength != 0)
                {
                    stream.readBits += ((headerLength * 8) - 16);
					if(!(*src->skip_input_data)(src->info,((headerLength * 8) - 16) / 8)){
						errorCode = 1;
                    	break;
					}
                    //stream.pCurrPos += (((headerLength * 8) - 16) / 8);
                }
                break;
                /* unsupported coding styles */
            case SOF1:
            case SOF3:
            case SOF5:
            case SOF6:
            case SOF7:
            case SOF9:
            case SOF10:
            case SOF11:
            case SOF13:
            case SOF14:
            case SOF15:
            case DAC:
            case DHP:
                LOGYC("JpegDecGetImageInfo error Unsupported coding styles");
                return (JPEGDEC_UNSUPPORTED);
            default:
                break;
            }
            if(init)
                break;

            if(errorCode)
            {
                if(pDecIn->bufferSize)
                {
                    LOGYC("JpegDecGetImageInfo Image info failed!");
                    return (JPEGDEC_INCREASE_INPUT_BUFFER);
                }
                else
                {
                    LOGYC("JpegDecGetImageInfo Stream error");
                    return (JPEGDEC_STRM_ERROR);
                }
            }
        }
        else
        {
			LOGYC("JpegDecGetImageInfo Could not get marker");
            if(!init)
                return (JPEGDEC_INCREASE_INPUT_BUFFER);
            else
                return (JPEGDEC_STRM_ERROR);
        }
    }
#if 0
    if(init)
    {
        //if(pDecIn->bufferSize)
        //    PTR_JPGC->info.initBufferSize = pDecIn->bufferSize;

        //LOGYC("JpegDecGetImageInfo OK\n");
        return (JPEGDEC_OK);
    }
    else
    {
        LOGYC("JpegDecGetImageInfo ERROR\n");
        return (JPEGDEC_ERROR);
    }
#else
	return (JPEGDEC_OK);
#endif
//#undef PTR_JPGC
}

