#ifndef HW_JPEGDECUTILS_H
#define HW_JPEGDECUTILS_H
#include "vpu_type.h"

#ifdef __cplusplus
extern "C"
{
#endif

JpegDecRet JpegDecGetSimpleImageInfo
	(HwJpegInputInfo* hwInfo, JpegDecInput * pDecIn, 
JpegDecImageInfo * pImageInfo, RK_U32 *ret);

#ifdef __cplusplus
}
#endif

enum{
	JPEGDEC_THUMB_SUPPORTED = 0,
	JPEGDEC_UNSUPPORTEDSIZE = 1,
	JPEGDEC_THUMB_UNSUPPORTEDSIZE = 1<<1,
	JPEGDEC_YUV_UNSUPPORTED = 1<<2,
	JPEGDEC_THUMB_YUV_UNSUPPORTED = 1<<3,
	JPEGDEC_HAVE_DRI = 1 << 4,
	JPEGDEC_THUMB_HAVE_DRI = 1 << 5,
	JPEGDEC_THUMB_SAMPLE_UNSUPPORT = 1 << 6
};

#define MOTOROLA_ALIGN 0
#define INTEL_ALIGN 1

#define REVERSE_BYTES(bytesAlign, bytes)	if(stream.errorOccur){	\
												errorCode = 1;	\
												breaknow = 1;	\
                                        		break;	\
											}	\
											do{	\
												if(bytesAlign == INTEL_ALIGN){	\
												bytes =  ((bytes & 0xff000000) >> 24)	\
												| ((bytes & 0x00ff0000) >> 8)	\
												| ((bytes & 0x0000ff00) << 8)	\
												| ((bytes & 0x000000ff) << 24);	\
												}	\
											}while(0)

#define REVERSE_LOW_TWO_BYTES(bytesAlign, bytes)	if(stream.errorOccur){	\
														errorCode = 1;	\
														breaknow = 1;	\
                                        				break;	\
													}	\
													do{	\
														if(bytesAlign == INTEL_ALIGN){	\
														bytes =  ((bytes & 0x0000ff00) >> 8)	\
														| ((bytes & 0x000000ff) << 8);	\
														}	\
													}while(0)

#define CHECK_BYTE(value)		currentByte = hw_JpegDecGetByte(&(stream), src);	\
								appBits += 8;	\
								if(currentByte != value){	\
									WHLOG("APP14, do not read 0x%08x, is 0x%08x", value, currentByte);	\
                    				stream.readBits += ((headerLength * 8) - appBits);	\
									(*src->skip_input_data)(src->info,((headerLength * 8) - appBits) / 8);	\
                    				break;	\
								}
//Tags used by IFD1 (thumbnail image)
#define ImageWidth_Tag 0x0100	//1
#define ImageLength_Tag 0x0101	//1
#define BitsPerSample_Tag 0x0102	//3
#define Compression_Tag 0x0103	//1, 1 means no compression, 6 means JPEG compression
#define PhotometricInterpretation_Tag 0x0106	//1, 1 means monochrome, 2 means RGB, 6 means YCbCr
#define StripOffsets_Tag 0x0111
#define SamplesPerPixel_Tag 0x0115	//at color image, value is 3.
#define RowsPerStrip_Tag 0x0116
#define StripByteCounts_Tag 0x0117
#define XResolution_Tag 0x011a
#define YResolution_Tag 0x011b
#define PalnarConfiguration_Tag 0x011c
#define ResolutionUnit_Tag 0x0128	// 1 means inch, 2 means centimeter
#define JpegIFOffset_Tag 0x0201
#define JpegIFByteCount_Tag 0x0202
#define YCbCrCoefficients_Tag 0x0211	//in usual, 0.299,0.587,0.114
#define YCbCrSubSampling_Tag 0x0212
#define YCbCrPositioning_Tag 0x0213
#define ReferenceBlackWhite_Tag 0x0214

static int DataFormat[12] = {1,1,2,4,8,1,1,2,4,8,4,8};
#endif
