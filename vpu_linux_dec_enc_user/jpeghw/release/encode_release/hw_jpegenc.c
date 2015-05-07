#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <linux/fb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

#include "jpegencapi.h"
#include "ewl.h"
#include "hw_jpegenc.h"
#include <utils/Log.h>
#include "EncJpegInstance.h"

#include "rk29-ipp.h"
#include <poll.h>
#ifdef USE_PIXMAN
#include "pixman-private.h"
#endif
#ifdef USE_SWSCALE
#include "swscale.h"//if need crop, may be change to use pixman lib
#endif
#include <stdbool.h>
#include <rga.h>

//#define HW_JPEG_DEBUG

#define LOG_TAG "hw_jpeg_encode"

#ifdef HW_JPEG_DEBUG
#define WHENCLOG ALOGE
#else
#define WHENCLOG(...)
#endif

#define V4L2_PIX_FMT_NV12 1
#define V4L2_PIX_FMT_NV21 2


int rga_nv12_scale_crop(int src_width, int src_height, char *src, short int *dst, int dstbuf_width,int dst_width,int dst_height,int zoom_val,bool mirror)
{
    int rgafd = -1,ret = -1;

    if((rgafd = open("/dev/rga",O_RDWR)) < 0) {
    	ALOGE("%s(%d):open rga device failed!!",__FUNCTION__,__LINE__);
        ret = -1;
    	return ret;
	}

    struct rga_req  Rga_Request;
    int err = 0;
    
    memset(&Rga_Request,0x0,sizeof(Rga_Request));

	unsigned char *psY, *psUV;
	int srcW,srcH,cropW,cropH;
	int ratio = 0;
	int top_offset=0,left_offset=0;
//need crop ?
	if((src_width*100/src_height) != (dst_width*100/dst_height)){
		ratio = ((src_width*100/dst_width) >= (src_height*100/dst_height))?(src_height*100/dst_height):(src_width*100/dst_width);
		cropW = ratio*dst_width/100;
		cropH = ratio*dst_height/100;
		
		left_offset=((src_width-cropW)>>1) & (~0x01);
		top_offset=((src_height-cropH)>>1) & (~0x01);
	}else{
		cropW = src_width;
		cropH = src_height;
		top_offset=0;
		left_offset=0;
	}

    //zoom ?
    if(zoom_val > 100){
        cropW = cropW*100/zoom_val;
        cropH = cropH*100/zoom_val;
		left_offset=((src_width-cropW)>>1) & (~0x01);
		top_offset=((src_height-cropH)>>1) & (~0x01);
    }
    

	psY = (unsigned char*)(src)/*+top_offset*src_width+left_offset*/;
	//psUV = (unsigned char*)(src) +src_width*src_height+top_offset*src_width/2+left_offset;
	
	Rga_Request.src.yrgb_addr =  0;
    Rga_Request.src.uv_addr  = (int)psY;
    Rga_Request.src.v_addr   =  0;
    Rga_Request.src.vir_w =  src_width;
    Rga_Request.src.vir_h = src_height;
    Rga_Request.src.format = RK_FORMAT_YCbCr_420_SP;
    Rga_Request.src.act_w = cropW;
    Rga_Request.src.act_h = cropH;
    Rga_Request.src.x_offset = left_offset;
    Rga_Request.src.y_offset = top_offset;

    Rga_Request.dst.yrgb_addr = 0;
    Rga_Request.dst.uv_addr  = (int)dst;
    Rga_Request.dst.v_addr   = 0;
    Rga_Request.dst.vir_w = dstbuf_width;
    Rga_Request.dst.vir_h = dst_height;
    Rga_Request.dst.format = RK_FORMAT_YCbCr_420_SP;
    Rga_Request.clip.xmin = 0;
    Rga_Request.clip.xmax = dst_width - 1;
    Rga_Request.clip.ymin = 0;
    Rga_Request.clip.ymax = dst_height - 1;
    Rga_Request.dst.act_w = dst_width;
    Rga_Request.dst.act_h = dst_height;
    Rga_Request.dst.x_offset = 0;
    Rga_Request.dst.y_offset = 0;
    Rga_Request.mmu_info.mmu_en    = 1;
    Rga_Request.mmu_info.mmu_flag  = ((2 & 0x3) << 4) | 1;
    Rga_Request.alpha_rop_flag |= (1 << 5);             /* ddl@rock-chips.com: v0.4.3 */

	if((cropW != dst_width) || ( cropH != dst_height)){
		Rga_Request.sina = 0;
		Rga_Request.cosa = 0x10000;
		Rga_Request.scale_mode = 1;
    	Rga_Request.rotate_mode = mirror ? 2:1;
	}else{
		Rga_Request.sina = 0;
		Rga_Request.cosa =  0;
		Rga_Request.scale_mode = 0;
    	Rga_Request.rotate_mode = mirror ? 2:0;
	}
    

    if(ioctl(rgafd, RGA_BLIT_SYNC, &Rga_Request) != 0) {
        ALOGE("%s(%d):  RGA_BLIT_ASYNC Failed", __FUNCTION__, __LINE__);
        err = -1;
    }

    close(rgafd);
    return err;
}


int arm_camera_yuv420_scale_arm(int v4l2_fmt_src, int v4l2_fmt_dst, 
									char *srcbuf, char *dstbuf,int src_w, int src_h,int dst_w, int dst_h,bool mirror,int zoom_val)
{
	unsigned char *psY,*pdY,*psUV,*pdUV; 
	unsigned char *src,*dst;
	int srcW,srcH,cropW,cropH,dstW,dstH;
	long zoomindstxIntInv,zoomindstyIntInv;
	long x,y;
	long yCoeff00,yCoeff01,xCoeff00,xCoeff01;
	long sX,sY;
	long r0,r1,a,b,c,d;
	int ret = 0;
	bool nv21DstFmt = false;
	int ratio = 0;
	int top_offset=0,left_offset=0;
	if((v4l2_fmt_src != V4L2_PIX_FMT_NV12) ||
		((v4l2_fmt_dst != V4L2_PIX_FMT_NV12) && (v4l2_fmt_dst != V4L2_PIX_FMT_NV21) )){
		ALOGE("%s:%d,not suppport this format ",__FUNCTION__,__LINE__);
		return -1;
	}

    //just copy ?
    if((v4l2_fmt_src == v4l2_fmt_dst) && (mirror == false)
        &&(src_w == dst_w) && (src_h == dst_h) && (zoom_val == 100)){
        memcpy(dstbuf,srcbuf,src_w*src_h*3/2);
        return 0;
    }else if((v4l2_fmt_dst == V4L2_PIX_FMT_NV21) 
            && (src_w == dst_w) && (src_h == dst_h) 
            && (mirror == false) && (zoom_val == 100)){
    //just convert fmt
        ALOGE("not support NV12 to NV21");
        /*cameraFormatConvert(V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_NV21, NULL, 
    					    srcbuf, dstbuf,0,0,src_w*src_h*3/2,
    					    src_w, src_h,src_w,
    					    dst_w, dst_h,dst_w,
    						mirror);*/
        return 0;

    }

	if ((v4l2_fmt_dst == V4L2_PIX_FMT_NV21)){
		nv21DstFmt = true;
		
	}

	//need crop ?
	if((src_w*100/src_h) != (dst_w*100/dst_h)){
		ratio = ((src_w*100/dst_w) >= (src_h*100/dst_h))?(src_h*100/dst_h):(src_w*100/dst_w);
		cropW = ratio*dst_w/100;
		cropH = ratio*dst_h/100;
		
		left_offset=((src_w-cropW)>>1) & (~0x01);
		top_offset=((src_h-cropH)>>1) & (~0x01);
	}else{
		cropW = src_w;
		cropH = src_h;
		top_offset=0;
		left_offset=0;
	}

    //zoom ?
    if(zoom_val > 100){
        cropW = cropW*100/zoom_val;
        cropH = cropH*100/zoom_val;
		left_offset=((src_w-cropW)>>1) & (~0x01);
		top_offset=((src_h-cropH)>>1) & (~0x01);
    }

	src = psY = (unsigned char*)(srcbuf)+top_offset*src_w+left_offset;
	//psUV = psY +src_w*src_h+top_offset*src_w/2+left_offset;
	psUV = (unsigned char*)(srcbuf) +src_w*src_h+top_offset*src_w/2+left_offset;

	
	srcW =src_w;
	srcH = src_h;
//	cropW = src_w;
//	cropH = src_h;

	
	dst = pdY = (unsigned char*)dstbuf; 
	pdUV = pdY + dst_w*dst_h;
	dstW = dst_w;
	dstH = dst_h;

	zoomindstxIntInv = ((unsigned long)(cropW)<<16)/dstW + 1;
	zoomindstyIntInv = ((unsigned long)(cropH)<<16)/dstH + 1;
	//y
	//for(y = 0; y<dstH - 1 ; y++ ) {	
	for(y = 0; y<dstH; y++ ) {	 
		yCoeff00 = (y*zoomindstyIntInv)&0xffff;
		yCoeff01 = 0xffff - yCoeff00; 
		sY = (y*zoomindstyIntInv >> 16);
		sY = (sY >= srcH - 1)? (srcH - 2) : sY; 	 
		for(x = 0; x<dstW; x++ ) {
			xCoeff00 = (x*zoomindstxIntInv)&0xffff;
			xCoeff01 = 0xffff - xCoeff00;	
			sX = (x*zoomindstxIntInv >> 16);
			sX = (sX >= srcW -1)?(srcW- 2) : sX;
			a = psY[sY*srcW + sX];
			b = psY[sY*srcW + sX + 1];
			c = psY[(sY+1)*srcW + sX];
			d = psY[(sY+1)*srcW + sX + 1];

			r0 = (a * xCoeff01 + b * xCoeff00)>>16 ;
			r1 = (c * xCoeff01 + d * xCoeff00)>>16 ;
			r0 = (r0 * yCoeff01 + r1 * yCoeff00)>>16;
			
			if(mirror)
				pdY[dstW -1 - x] = r0;
			else
				pdY[x] = r0;
		}
		pdY += dstW;
	}

	dstW /= 2;
	dstH /= 2;
	srcW /= 2;
	srcH /= 2;

	//UV
	//for(y = 0; y<dstH - 1 ; y++ ) {
	for(y = 0; y<dstH; y++ ) {
		yCoeff00 = (y*zoomindstyIntInv)&0xffff;
		yCoeff01 = 0xffff - yCoeff00; 
		sY = (y*zoomindstyIntInv >> 16);
		sY = (sY >= srcH -1)? (srcH - 2) : sY;		
		for(x = 0; x<dstW; x++ ) {
			xCoeff00 = (x*zoomindstxIntInv)&0xffff;
			xCoeff01 = 0xffff - xCoeff00;	
			sX = (x*zoomindstxIntInv >> 16);
			sX = (sX >= srcW -1)?(srcW- 2) : sX;
			//U
			a = psUV[(sY*srcW + sX)*2];
			b = psUV[(sY*srcW + sX + 1)*2];
			c = psUV[((sY+1)*srcW + sX)*2];
			d = psUV[((sY+1)*srcW + sX + 1)*2];

			r0 = (a * xCoeff01 + b * xCoeff00)>>16 ;
			r1 = (c * xCoeff01 + d * xCoeff00)>>16 ;
			r0 = (r0 * yCoeff01 + r1 * yCoeff00)>>16;
		
			if(mirror && nv21DstFmt)
				pdUV[dstW*2-1- (x*2)] = r0;
			else if(mirror)
				pdUV[dstW*2-1-(x*2+1)] = r0;
			else if(nv21DstFmt)
				pdUV[x*2 + 1] = r0;
			else
				pdUV[x*2] = r0;
			//V
			a = psUV[(sY*srcW + sX)*2 + 1];
			b = psUV[(sY*srcW + sX + 1)*2 + 1];
			c = psUV[((sY+1)*srcW + sX)*2 + 1];
			d = psUV[((sY+1)*srcW + sX + 1)*2 + 1];

			r0 = (a * xCoeff01 + b * xCoeff00)>>16 ;
			r1 = (c * xCoeff01 + d * xCoeff00)>>16 ;
			r0 = (r0 * yCoeff01 + r1 * yCoeff00)>>16;

			if(mirror && nv21DstFmt)
				pdUV[dstW*2-1- (x*2+1) ] = r0;
			else if(mirror)
				pdUV[dstW*2-1-(x*2)] = r0;
			else if(nv21DstFmt)
				pdUV[x*2] = r0;
			else
				pdUV[x*2 + 1] = r0;
		}
		pdUV += dstW*2;
	}
	return 0;
}	


//#define SECOND_USE_SOFT
/*                 JPEG HW ENCODE VERSION
* 
*   version: 2014-03-27, support softscale for no ipp,RUN INTO JPEGENCDOER
*   version: 2014-05-25, support IOMMU for rk3288,you need new kernel
*   version: 2014-06-13, fix compile bugs in 3288 
*       
*   note:You need compile the source code in rk3188,just by command "lunch" 
*        and choose rk3188.
*/
#define USE_IPP//must use ipp, encoder does not support scale
#define DO_APP1_THUMB

#define BACKUP_LEN 20//FFD8 + FFE0 0010 4A46 4946 00 0102 00 0064 0064 00 00

#define SETTWOBYTE(a,b,value) a = (unsigned short*)b;\
							*a = value;\
							b += 2;

#define SETFOURBYTE(a,b,value) a = (unsigned int*)b;\
							*a = value;\
							b += 4;

#define SETFOURBYTESTRING(b,str,len,increment) increment = 4;\
							while(increment-- > 0){\
								if(len > 0){\
									*b++ = *str++;\
									len--;\
								}else{\
									*b++ = '\0';\
								}\
							 }

#define BEFORE_EXIF_LEN 12
int customInsertApp1Header(unsigned char * startpos, JpegEncInInfo *inInfo, uint32_t**jpeglenPos){
	RkExifInfo *exifinfo = inInfo->exifInfo;//can't be null
	RkGPSInfo *gpsinfo = inInfo->gpsInfo;
	unsigned short* shortpos = NULL;
	uint32_t *intpos = NULL;
	int stringlen = 0;
	int increment = 0;
	unsigned char* initpos = NULL;
	int offset = 0;
	*startpos++ = 0xff;//SOI
	*startpos++ = 0xd8;

	*startpos++ = 0xff;//app1
	*startpos++ = 0xe1;

	//length
	*startpos++ = 0x00;
	*startpos++ = 0x00;

	*startpos++ = 0x45;//EXIF
	*startpos++ = 0x78;
	*startpos++ = 0x69;
	*startpos++ = 0x66;

	*startpos++ = 0x00;
	*startpos++ = 0x00;

	initpos = startpos;
	SETTWOBYTE(shortpos,startpos,0x4949)//intel align
	SETTWOBYTE(shortpos,startpos,0x2a)
	SETFOURBYTE(intpos,startpos,0x08)//offset to ifd0
	if(gpsinfo != NULL){
		SETTWOBYTE(shortpos,startpos,0x0a)//num of directory entry
		offset = 10 + 0x0a*12 + 4;
	}else{
		SETTWOBYTE(shortpos,startpos,0x09)//num of directory entry
		offset = 10 + 0x09*12 + 4;
	}
//1
	SETTWOBYTE(shortpos,startpos,0x010e)//imagedescription
	SETTWOBYTE(shortpos,startpos,0x02)
	SETFOURBYTE(intpos,startpos,0x04)
	
	SETTWOBYTE(shortpos,startpos,0x726b)//imagedescription
	SETTWOBYTE(shortpos,startpos,0x2020)//imagedescription
	//SETFOURBYTE(intpos,startpos,0x726b2020)
//2	
	SETTWOBYTE(shortpos,startpos,0x010f)//Make
	SETTWOBYTE(shortpos,startpos,0x02)
	stringlen = exifinfo->makerchars;
	SETFOURBYTE(intpos,startpos,stringlen)
	if(stringlen <= 4){
		SETFOURBYTESTRING(startpos,exifinfo->maker,stringlen, increment)
	}else{
		SETFOURBYTE(intpos,startpos,offset)
		memcpy(initpos + offset,exifinfo->maker,stringlen);
		offset += stringlen;
	}
//3	
	SETTWOBYTE(shortpos,startpos,0x0110)//Model
	SETTWOBYTE(shortpos,startpos,0x02)
	stringlen = exifinfo->modelchars;
	SETFOURBYTE(intpos,startpos,stringlen)
	if(stringlen <= 4){
		SETFOURBYTESTRING(startpos,exifinfo->modelstr,stringlen, increment)
	}else{
		SETFOURBYTE(intpos,startpos,offset)
		memcpy(initpos + offset,exifinfo->modelstr,stringlen);
		offset += stringlen;
	}
//4
	SETTWOBYTE(shortpos,startpos,0x0112)//Orientation
	SETTWOBYTE(shortpos,startpos,0x03)
	SETFOURBYTE(intpos,startpos,0x01)
	SETFOURBYTE(intpos,startpos,exifinfo->Orientation)
//5
	SETTWOBYTE(shortpos,startpos,0x011a)//XResolution
	SETTWOBYTE(shortpos,startpos,0x05)
	SETFOURBYTE(intpos,startpos,0x01)
	SETFOURBYTE(intpos,startpos,offset)
	intpos = (uint32_t*)(initpos + offset);
	*intpos++ = 0x48;//72inch
	*intpos = 0x01;
	offset += 8;
//6
	SETTWOBYTE(shortpos,startpos,0x011b)//YResolution
	SETTWOBYTE(shortpos,startpos,0x05)
	SETFOURBYTE(intpos,startpos,0x01)
	SETFOURBYTE(intpos,startpos,offset)
	intpos = (uint32_t*)(initpos + offset);
	*intpos++ = 0x48;//72inch
	*intpos = 0x01;
	offset += 8;
//7
	SETTWOBYTE(shortpos,startpos,0x0128)//ResolutionUnit
	SETTWOBYTE(shortpos,startpos,0x03)
	SETFOURBYTE(intpos,startpos,0x01)
	SETFOURBYTE(intpos,startpos,0x02)//inch
//8
	SETTWOBYTE(shortpos,startpos,0x0132)//DateTime
	SETTWOBYTE(shortpos,startpos,0x02)
	SETFOURBYTE(intpos,startpos,0x14)
	SETFOURBYTE(intpos,startpos,offset)
	memcpy(initpos + offset, exifinfo->DateTime, 0x14);
	offset += 0x14;
//9
	SETTWOBYTE(shortpos,startpos,0x8769)//exif sub ifd
	SETTWOBYTE(shortpos,startpos,0x04)
	SETFOURBYTE(intpos,startpos,0x01)
	SETFOURBYTE(intpos,startpos,offset)
	{
		unsigned char* subifdPos = initpos+offset;
		int numofifd = 0x1c;
		if(exifinfo->makernote){
			numofifd += 1;
		}
		SETTWOBYTE(shortpos,subifdPos,numofifd)//num of ifd
		offset += 2 + numofifd*12 + 4;
		SETTWOBYTE(shortpos,subifdPos,0x829a)//ExposureTime
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->ExposureTime.num;
		*intpos = exifinfo->ExposureTime.denom;
		offset += 8;
		
		SETTWOBYTE(shortpos,subifdPos,0x829d)//FNumber
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->ApertureFNumber.num;
		*intpos = exifinfo->ApertureFNumber.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0x8827)//ISO Speed Ratings
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->ISOSpeedRatings)

		SETTWOBYTE(shortpos,subifdPos,0x9000)//ExifVersion
		SETTWOBYTE(shortpos,subifdPos,0x07)
		SETFOURBYTE(intpos,subifdPos,0x04)
		SETFOURBYTE(intpos,subifdPos,0x31323230)

		SETTWOBYTE(shortpos,subifdPos,0x9003)//DateTimeOriginal
		SETTWOBYTE(shortpos,subifdPos,0x02)
		SETFOURBYTE(intpos,subifdPos,0x14)
		SETFOURBYTE(intpos,subifdPos,offset)
		memcpy(initpos + offset, exifinfo->DateTime, 0x14);
		offset += 0x14;
		
		SETTWOBYTE(shortpos,subifdPos,0x9004)//DateTimeDigitized
		SETTWOBYTE(shortpos,subifdPos,0x02)
		SETFOURBYTE(intpos,subifdPos,0x14)
		SETFOURBYTE(intpos,subifdPos,offset)
		memcpy(initpos + offset, exifinfo->DateTime, 0x14);
		offset += 0x14;
		
		SETTWOBYTE(shortpos,subifdPos,0x9101)//ComponentsConfiguration
		SETTWOBYTE(shortpos,subifdPos,0x07)
		SETFOURBYTE(intpos,subifdPos,0x04)
		SETFOURBYTE(intpos,subifdPos,0x00030201)//YCbCr-format
		
		SETTWOBYTE(shortpos,subifdPos,0x9102)//CompressedBitsPerPixel
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->CompressedBitsPerPixel.num;
		*intpos = exifinfo->CompressedBitsPerPixel.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0x9201)//ShutterSpeedValue
		SETTWOBYTE(shortpos,subifdPos,0x0a)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->ShutterSpeedValue.num;
		*intpos = exifinfo->ShutterSpeedValue.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0x9202)//ApertureValue
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->ApertureValue.num;
		*intpos = exifinfo->ApertureValue.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0x9204)//ExposureBiasValue
		SETTWOBYTE(shortpos,subifdPos,0x0a)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->ExposureBiasValue.num;
		*intpos = exifinfo->ExposureBiasValue.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0x9205)//MaxApertureValue
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->MaxApertureValue.num;
		*intpos = exifinfo->MaxApertureValue.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0x9207)//MeteringMode
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->MeteringMode)
		
		SETTWOBYTE(shortpos,subifdPos,0x9209)//Flash
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->Flash)

		SETTWOBYTE(shortpos,subifdPos,0x920a)//FocalLength
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->FocalLength.num;
		*intpos = exifinfo->FocalLength.denom;
		offset += 8;

		if(exifinfo->makernote){//add for asus
			SETTWOBYTE(shortpos,subifdPos,0x927c)//makernote
			SETTWOBYTE(shortpos,subifdPos,0x07)
			stringlen = exifinfo->makernotechars;
			SETFOURBYTE(intpos,subifdPos,stringlen)
			if(stringlen <= 4){
				SETFOURBYTESTRING(subifdPos,exifinfo->makernote,stringlen, increment)
			}else{
				SETFOURBYTE(intpos,subifdPos,offset)
				memcpy(initpos + offset,exifinfo->makernote,stringlen);
				offset += stringlen;
			}
		}

		SETTWOBYTE(shortpos,subifdPos,0xa001)//ColorSpace
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,0x01)//sRGB

		SETTWOBYTE(shortpos,subifdPos,0xa002)//exifImageWidth
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,inInfo->inputW)

		SETTWOBYTE(shortpos,subifdPos,0xa003)//exifImageHeight
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,inInfo->inputH)

		SETTWOBYTE(shortpos,subifdPos,0xa20e)//FocalPlaneXResolution
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->FocalPlaneXResolution.num;
		*intpos = exifinfo->FocalPlaneXResolution.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0xa20f)//FocalPlaneYResolution
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->FocalPlaneYResolution.num;
		*intpos = exifinfo->FocalPlaneYResolution.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0xa210)//FocalPlaneResolutionUnit
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,0x02)//inch

		SETTWOBYTE(shortpos,subifdPos,0xa217)//SensingMethod
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->SensingMethod)

		SETTWOBYTE(shortpos,subifdPos,0xa300)//FileSource
		SETTWOBYTE(shortpos,subifdPos,0x07)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->FileSource)

		SETTWOBYTE(shortpos,subifdPos,0xa401)//CustomRendered
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->CustomRendered)

		SETTWOBYTE(shortpos,subifdPos,0xa402)//ExposureMode
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->ExposureMode)

		SETTWOBYTE(shortpos,subifdPos,0xa403)//WhiteBalance
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->WhiteBalance)

		SETTWOBYTE(shortpos,subifdPos,0xa404)//DigitalZoomRatio
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->DigitalZoomRatio.num;
		*intpos = exifinfo->DigitalZoomRatio.denom;
		offset += 8;
/*		
		SETTWOBYTE(shortpos,subifdPos,0xa405)//FocalLengthIn35mmFilm
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->FocalLengthIn35mmFilm)
*/
		SETTWOBYTE(shortpos,subifdPos,0xa406)//SceneCaptureType
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->SceneCaptureType)
		
		SETFOURBYTE(intpos,subifdPos,0x00)//link to next ifd
	}
	if(gpsinfo != NULL){
		//10
		SETTWOBYTE(shortpos,startpos,0x8825)//gps ifd
		SETTWOBYTE(shortpos,startpos,0x04)
		SETFOURBYTE(intpos,startpos,0x01)
		SETFOURBYTE(intpos,startpos,offset)
			unsigned char* gpsPos = initpos+offset;
			SETTWOBYTE(shortpos,gpsPos,0x0a)//num of ifd
			offset += 2 + 0x0a*12 + 4;

			SETTWOBYTE(shortpos,gpsPos,0x00)//GPSVersionID
			SETTWOBYTE(shortpos,gpsPos,0x01)
			SETFOURBYTE(intpos,gpsPos,0x04)
			SETFOURBYTE(intpos,gpsPos,0x0202)

			SETTWOBYTE(shortpos,gpsPos,0x01)//GPSLatitudeRef
			SETTWOBYTE(shortpos,gpsPos,0x02)
			SETFOURBYTE(intpos,gpsPos,0x02)
			*gpsPos++ = gpsinfo->GPSLatitudeRef[0];
			*gpsPos++ = gpsinfo->GPSLatitudeRef[1];
			*gpsPos++ = 0;*gpsPos++ = 0;

			SETTWOBYTE(shortpos,gpsPos,0x02)//GPSLatitude
			SETTWOBYTE(shortpos,gpsPos,0x05)
			SETFOURBYTE(intpos,gpsPos,0x03)
			SETFOURBYTE(intpos,gpsPos,offset)
			intpos = (uint32_t*)(initpos + offset);
			*intpos++ = gpsinfo->GPSLatitude[0].num;
			*intpos++ = gpsinfo->GPSLatitude[0].denom;
			*intpos++ = gpsinfo->GPSLatitude[1].num;
			*intpos++ = gpsinfo->GPSLatitude[1].denom;
			*intpos++ = gpsinfo->GPSLatitude[2].num;
			*intpos = gpsinfo->GPSLatitude[2].denom;
			offset += 8*3;

			SETTWOBYTE(shortpos,gpsPos,0x03)//GPSLongitudeRef
			SETTWOBYTE(shortpos,gpsPos,0x02)
			SETFOURBYTE(intpos,gpsPos,0x02)
			*gpsPos++ = gpsinfo->GPSLongitudeRef[0];
			*gpsPos++ = gpsinfo->GPSLongitudeRef[1];
			*gpsPos++ = 0;*gpsPos++ = 0;

			SETTWOBYTE(shortpos,gpsPos,0x04)//GPSLongitude
			SETTWOBYTE(shortpos,gpsPos,0x05)
			SETFOURBYTE(intpos,gpsPos,0x03)
			SETFOURBYTE(intpos,gpsPos,offset)
			intpos = (uint32_t*)(initpos + offset);
			*intpos++ = gpsinfo->GPSLongitude[0].num;
			*intpos++ = gpsinfo->GPSLongitude[0].denom;
			*intpos++ = gpsinfo->GPSLongitude[1].num;
			*intpos++ = gpsinfo->GPSLongitude[1].denom;
			*intpos++ = gpsinfo->GPSLongitude[2].num;
			*intpos = gpsinfo->GPSLongitude[2].denom;
			offset += 8*3;

			SETTWOBYTE(shortpos,gpsPos,0x05)//GPSAltitudeRef
			SETTWOBYTE(shortpos,gpsPos,0x02)
			SETFOURBYTE(intpos,gpsPos,0x01)
			*gpsPos++ = gpsinfo->GPSAltitudeRef;
			*gpsPos++ = 0;*gpsPos++ = 0;*gpsPos++ = 0;

			SETTWOBYTE(shortpos,gpsPos,0x06)//GPSAltitude
			SETTWOBYTE(shortpos,gpsPos,0x05)
			SETFOURBYTE(intpos,gpsPos,0x01)
			SETFOURBYTE(intpos,gpsPos,offset)
			intpos = (uint32_t*)(initpos + offset);
			*intpos++ = gpsinfo->GPSAltitude.num;
			*intpos = gpsinfo->GPSAltitude.denom;
			offset += 8;

			SETTWOBYTE(shortpos,gpsPos,0x07)//GpsTimeStamp
			SETTWOBYTE(shortpos,gpsPos,0x05)
			SETFOURBYTE(intpos,gpsPos,0x03)
			SETFOURBYTE(intpos,gpsPos,offset)
			intpos = (uint32_t*)(initpos + offset);
			*intpos++ = gpsinfo->GpsTimeStamp[0].num;
			*intpos++ = gpsinfo->GpsTimeStamp[0].denom;
			*intpos++ = gpsinfo->GpsTimeStamp[1].num;
			*intpos++ = gpsinfo->GpsTimeStamp[1].denom;
			*intpos++ = gpsinfo->GpsTimeStamp[2].num;
			*intpos = gpsinfo->GpsTimeStamp[2].denom;
			offset += 8*3;
			
			SETTWOBYTE(shortpos,gpsPos,0x1b)//GPSProcessingMethod
			SETTWOBYTE(shortpos,gpsPos,0x07)
			stringlen = gpsinfo->GpsProcessingMethodchars;
			SETFOURBYTE(intpos,gpsPos,stringlen)
			if(stringlen <= 4){
				SETFOURBYTESTRING(gpsPos,gpsinfo->GPSProcessingMethod,stringlen, increment)
               #if      0
                int i =0;
                increment = 4;
                while(increment > 0)
                {
                  if(stringlen > 0)
                  {
                    *gpsPos++ = gpsinfo->GPSProcessingMethod[i];
                     stringlen--;
                     i++;
                  }else{
                    *gpsPos++ ='\0';
                  }
                  increment--;
                }	
              #endif
			}else{
				SETFOURBYTE(intpos,gpsPos,offset)
				memcpy(initpos + offset,gpsinfo->GPSProcessingMethod,stringlen);
				offset += stringlen;
			}

			SETTWOBYTE(shortpos,gpsPos,0x1d)//GPSDateStamp
			SETTWOBYTE(shortpos,gpsPos,0x02)
			SETFOURBYTE(intpos,gpsPos,0x11)
			SETFOURBYTE(intpos,gpsPos,offset)
			memcpy(initpos + offset, gpsinfo->GpsDateStamp, 11);
			offset += 11;
			
			SETFOURBYTE(intpos,gpsPos,0x00)//link to next ifd
	}

	SETFOURBYTE(intpos,startpos,offset)//next ifd offset
	startpos = initpos + offset;
	SETTWOBYTE(shortpos,startpos,0x03)
	offset += 2 + 0x03*12 + 4;
	if(((offset+BEFORE_EXIF_LEN)&63) != 0){//do not align 64, adjust
		stringlen = 64 - ((offset+BEFORE_EXIF_LEN)&63);
		offset += stringlen;
	}
	SETTWOBYTE(shortpos,startpos,0x0103)//Compression
	SETTWOBYTE(shortpos,startpos,0x03)
	SETFOURBYTE(intpos,startpos,0x01)
	SETFOURBYTE(intpos,startpos,0x06)//JPEG

	SETTWOBYTE(shortpos,startpos,0x0201)//JPEG data offset
	SETTWOBYTE(shortpos,startpos,0x04)
	SETFOURBYTE(intpos,startpos,0x01)
	SETFOURBYTE(intpos,startpos,offset)
	
	SETTWOBYTE(shortpos,startpos,0x0202)//JPEG data count
	SETTWOBYTE(shortpos,startpos,0x04)
	SETFOURBYTE(intpos,startpos,0x01)
	SETFOURBYTE(intpos,startpos,0x0)
	*jpeglenPos = intpos;//save the been set pos
	SETFOURBYTE(intpos,startpos,0x00)//link to next ifd
	if(stringlen != 0){
		memset(startpos,0,stringlen);
	}
	return offset + BEFORE_EXIF_LEN;
}

//flag: scale algorithm(see at swscale.h if use swscale, else see at pixman-private.h)
int doSoftScale(uint8_t *srcy, uint8_t *srcuv, int srcw, int srch, uint8_t *dsty, uint8_t *dstuv, int dstw, int dsth, int flag, int format){
#if !defined(USE_PIXMAN)&&!defined(USE_SWSCALE)
	return -1;
#endif
	if(srcw <= 0 || srch <= 0 || dstw <= 0 || dsth <= 0){
		return -1;
	}
#ifdef USE_PIXMAN
	if(srcuv != srcy+srcw*srch || dstuv != dsty+dstw*dsth){
		ALOGE("uv data must follow y data.");
		return -1;
	}
	pixman_format_code_t format = PIXMAN_yv12;
	pixman_filter_t f = (pixman_filter_t)flag;
	pixman_image_t *src_img = NULL, *dst_img = NULL;
	pixman_transform_t transform, pform;
	pixman_box16_t box;
	box.x1 = 384;
	box.y1 = 256;
	box.x2 = 768;
	box.y2 = 512;
	pixman_transform_init_identity(&transform);
	pixman_transform_init_identity(&pform);
	double xscale = (double)dstw/(double)srcw;
	double yscale = (double)dsth/(double)srch;
	//pixman_transform_init_scale(&transform, pixman_int_to_fixed(srcw), pixman_int_to_fixed(srch));
	if(!pixman_transform_scale(&pform, &transform, pixman_double_to_fixed(xscale), pixman_double_to_fixed(yscale))){
		ALOGE("pixman_transform_scale fail.");
		return -1;
	}
	pixman_transform_bounds(&pform, &box);
	ALOGE("foward box: %d, %d, %d, %d", box.x1, box.y1, box.x2, box.y2);
	box.x1 = 384;
	box.y1 = 256;
	box.x2 = 768;
	box.y2 = 512;
	pixman_transform_bounds(&transform, &box);
	ALOGE("reverse box: %d, %d, %d, %d", box.x1, box.y1, box.x2, box.y2);
	
	src_img = pixman_image_create_bits(format, srcw, srch, srcy, srcw);
	if(src_img == NULL){
		ALOGE("pixman create src imge fail.");
		return -1;
	}
	dst_img = pixman_image_create_bits(format, dstw, dsth, dsty, dstw);
	if(dst_img == NULL){
		ALOGE("pixman create dst imge fail.");
		return -1;
	}
	if(!pixman_image_set_transform(src_img, &transform) ||
		!pixman_image_set_filter(src_img, f, NULL, 0)){
		ALOGE("pixman_image_set_transform or pixman_image_set_filter fail.");
		return -1;
	}
	
	//yuv unsupport out yuv
	pixman_image_composite(PIXMAN_OP_SRC, src_img,
						NULL, dst_img,
						0, 0, 0, 0,
						0, 0, dstw, dsth);
	pixman_image_unref(src_img);
	pixman_image_unref(dst_img);
	return -1;
#endif
#ifdef USE_SWSCALE
	uint8_t * yuvsrc[4] = {srcy, srcuv, NULL, NULL};
	int yuvsrcstride[4] = {srcw, srcw, 0, 0};
	uint8_t * yuvdst[4] = {dsty, dstuv, NULL, NULL};
	int yuvdststride[4] = {dstw, dstw, 0, 0};
	struct SwsContext *sws = NULL;
	if(format == PIX_FMT_RGB565){
		yuvsrcstride[0] = 2*srcw;
		yuvsrcstride[1] = 0;
		yuvdststride[0] = 2*dstw;
                yuvdststride[1] = 0;
	}else if(format == PIX_FMT_RGB32){
		yuvsrcstride[0] = 4*srcw;
                yuvsrcstride[1] = 0;
		yuvdststride[0] = 4*dstw;
                yuvdststride[1] = 0;
	}
	sws = sws_getContext(srcw,srch, format, dstw, dsth, format,
		flag, NULL, NULL, NULL);
	if(sws == NULL){
		return -1;
	}
	WHENCLOG("before sws_scale.");
	sws_scale(sws, yuvsrc, yuvsrcstride, 0, srch, yuvdst, yuvdststride);
	WHENCLOG("after sws_scale.");
	sws_freeContext(sws);
#endif
	return 0;
}

#define IPP_TIMES_PER 8
#define ALIGNVAL 31
int encodeThumb(JpegEncInInfo *inInfo, JpegEncOutInfo *outInfo){
	JpegEncInInfo JpegInInfo;
	JpegEncOutInfo JpegOutInfo;
#ifdef USE_IPP
	VPUMemLinear_t inMem;
	int tmpw = 0;
	int tmph = 0;
	int tmpwhIsValid = 0;
#endif
	int outw = inInfo->thumbW;
	int outh = inInfo->thumbH;
	if(inInfo->rotateDegree == DEGREE_90 || inInfo->rotateDegree == DEGREE_270){
		outw = inInfo->thumbH;
		outh = inInfo->thumbW;
	}
#ifdef DO_APP1_THUMB
	unsigned char *startpos = (unsigned char*)outInfo->outBufVirAddr;
	uint32_t *thumbPos = NULL; 
	int headerlen = 0;
	int addzerolen = 0;
#endif
#ifdef USE_IPP
	//ipp
	struct rk29_ipp_req ipp_req;
	int ipp_fd = -1;
	//static struct pollfd fd;
	int ippret = -1;
#endif

#ifdef HW_JPEG_DEBUG
FILE *file = NULL;
FILE *ippoutyuv = NULL;
#endif

#ifdef USE_IPP

#if 1
	struct timeval start;
	struct timeval end;
	struct timeval rga_start;
	struct timeval rga_end;
	gettimeofday(&start, NULL);
	gettimeofday(&rga_start, NULL);
#endif
#if defined(USE_SWSCALE)
	int colorFormat = PIX_FMT_NV12;
#endif
	int pixelBytesM2 = 3;
	switch(inInfo->type){
	case JPEGENC_YUV420_SP:
		//colorFormat = PIX_FMT_NV12;
		WHENCLOG("open ipp.");
        	ipp_fd = open("/dev/rk29-ipp",O_RDWR,0);
		break;
	case HWJPEGENC_RGB565:
		pixelBytesM2 = 4;
#if defined(USE_SWSCALE)
		colorFormat = PIX_FMT_RGB565;
#endif
		break;
	case HWJPEGENC_RGB888:
		pixelBytesM2 = 8;
#if defined(USE_SWSCALE)
		colorFormat = PIX_FMT_RGB32;
#endif
                break;
	default:
	    ALOGE("source type is not support, can not do thumbnail.");
            return 0;
	}
	gettimeofday(&rga_end, NULL);
	long long d_rga = 1000000 *(rga_end.tv_sec - rga_start.tv_sec) + (rga_end.tv_usec - rga_start.tv_usec);
	d_rga = d_rga/1000;
	ALOGE("%s , is ready to scale: %d ms", ipp_fd<0?("rga scale"):("ipp scale"), d_rga);
	if(ipp_fd < 0)
	{
#if defined(USE_PIXMAN)||defined(USE_SWSCALE)
		if(1){
			ALOGE("encodethumb, open /dev/rk29-ipp fail! we try to do softscale,over.");
		}else{
			return 0;
		}
		//if(inInfo->rotateDegree != 0){
		//	ALOGE("encodethumb , soft scale do not support rotate, call wh TODO.");
		//	return 0;
		//}
		gettimeofday(&rga_start, NULL);
		if(VPUMallocLinear(&inMem, (outw*outh)*pixelBytesM2/2) != 0){
                	ALOGE("encodethumb malloc pmem fail ...");
        	        return 0;
	    }
		gettimeofday(&rga_end, NULL);
	    long long d_rga = 1000000 *(rga_end.tv_sec - rga_start.tv_sec) + (rga_end.tv_usec - rga_start.tv_usec);
	    d_rga = d_rga/1000;
	    ALOGE("%s , is ready to scale: %d ms", ipp_fd<0?("rga scale"):("ipp scale"), d_rga);
		//do softscale
		{
			//need flush camera mem?
			uint8_t * y_src = (uint8_t*)inInfo->y_vir_addr;
			uint8_t * uv_src = (uint8_t*)inInfo->uv_vir_addr;
			uint8_t * dst = (uint8_t*)inMem.vir_addr;
		ALOGD("srcy:%p,srcuv:%p, dsty:%p,dstuv:%p",y_src,uv_src, dst, dst+outw*outh);
		ALOGD("srcwh: %d, %d, outwh: %d,%d", inInfo->inputW, inInfo->inputH,outw, outh);
/*
#ifndef USE_PIXMAN
			if(doSoftScale(y_src, uv_src, inInfo->inputW, inInfo->inputH,
				dst, dst+outw*outh, outw, outh, SWS_FAST_BILINEAR, colorFormat) < 0){
#endif
				ALOGE("encodethumb, doSoftScale faild!");
				VPUFreeLinear(&inMem);
				return 0;
			}*/
			ALOGD("encodethumb,do rga_nv12_scale_crop");
			gettimeofday(&rga_start, NULL);
			rga_nv12_scale_crop(inInfo->inputW, inInfo->inputH,(char*)(y_src),
            (short int*)dst,outw,outw, outh,false,100);
			gettimeofday(&rga_end, NULL);
	        long long d_rga = 1000000 *(rga_end.tv_sec - rga_start.tv_sec) + (rga_end.tv_usec - rga_start.tv_usec);
	        d_rga = d_rga/1000;
	        ALOGE("%s , scale: %d ms", ipp_fd<0?("rga scale"):("ipp scale"), d_rga);
			VPUMemClean(&inMem);
		}
		ipp_req.dst0.YrgbMst = inMem.phy_addr;
		if(colorFormat == PIX_FMT_NV12){
			ipp_req.dst0.CbrMst = inMem.phy_addr + IOMMUOffsetFix(outw*outh);
		}else{
			ipp_req.dst0.CbrMst = 0;
		}
		ipp_req.dst0.w = outw;
		ipp_req.dst0.h = outh;
#else
	ALOGE("open ipp failed, return.");
	return 0;
#endif	
	}
#if 1
	gettimeofday(&end, NULL);
	long long d = 1000000 *(end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
	d = d/1000;
	ALOGE("%s , scale: %d ms", ipp_fd<0?("soft scale"):("ipp scale"), d);
#endif
#if 0//def HW_JPEG_DEBUG
	ippoutyuv = fopen("/flash/ippout.yuv","wb");
	VPUMemFlush(&inMem);
	if(tmpwhIsValid){
		fwrite(inMem.vir_addr+tmpw*tmph*3/2, 1, outw*outh*3/2, ippoutyuv);
	}else{
		fwrite(inMem.vir_addr, 1, outw*outh*3/2, ippoutyuv);
	}
	fclose(ippoutyuv);
#endif

#endif
	JpegInInfo.frameHeader = 1;
	if(inInfo->rotateDegree != DEGREE_180){
		JpegInInfo.rotateDegree = inInfo->rotateDegree;
	}else{
		JpegInInfo.rotateDegree = DEGREE_0;
	}
#ifdef USE_IPP
	JpegInInfo.y_rgb_addr = ipp_req.dst0.YrgbMst;
	JpegInInfo.uv_addr = ipp_req.dst0.CbrMst;
	JpegInInfo.inputW = ipp_req.dst0.w;
	JpegInInfo.inputH = ipp_req.dst0.h;
#else
	JpegInInfo.y_rgb_addr = inInfo->y_rgb_addr;
	JpegInInfo.uv_addr = inInfo->uv_addr;
	JpegInInfo.inputW = inInfo->inputW;
	JpegInInfo.inputH = inInfo->inputH;
#endif
	JpegInInfo.type = inInfo->type;
	JpegInInfo.qLvl = inInfo->thumbqLvl;
	JpegInInfo.doThumbNail = 0;
	JpegInInfo.thumbData = NULL;
	JpegInInfo.thumbDataLen = -1;
	JpegInInfo.thumbW = -1;
	JpegInInfo.thumbH = -1;
	JpegInInfo.thumbqLvl = 0;
	JpegInInfo.exifInfo = NULL;
	JpegInInfo.gpsInfo = NULL;
#ifdef DO_APP1_THUMB
    WHENCLOG("exifInfo is : 0x%x", inInfo->exifInfo);
	if(inInfo->frameHeader && inInfo->exifInfo != NULL){
        //LOGD("do customInsertApp1Header.");
		headerlen =   customInsertApp1Header(startpos, inInfo, &thumbPos);
	}else{
		headerlen = 0;
	}
#endif
#ifdef DO_APP1_THUMB
	JpegOutInfo.outBufPhyAddr = outInfo->outBufPhyAddr+IOMMUOffsetFix(headerlen);//ffd8 + app0 header - ffd9
	JpegOutInfo.outBufVirAddr = outInfo->outBufVirAddr+headerlen;
	JpegOutInfo.outBuflen = outw * outh + headerlen;
#else
	JpegOutInfo.outBufPhyAddr = outInfo->outBufPhyAddr;
	JpegOutInfo.outBufVirAddr = outInfo->outBufVirAddr;
	JpegOutInfo.outBuflen = outw * outh;
#endif
	if(JpegOutInfo.outBuflen < 1024){
		JpegOutInfo.outBuflen = 1024;
	}
	JpegOutInfo.jpegFileLen = 0;
	JpegOutInfo.cacheflush = outInfo->cacheflush;
	
	WHENCLOG("encodethumb, outbuflen: %d", JpegOutInfo.outBuflen);
	if(hw_jpeg_encode(&JpegInInfo, &JpegOutInfo) < 0 || JpegOutInfo.jpegFileLen <= 0){
		ALOGE("encodethumb, hw jpeg encode fail.");
#ifdef USE_IPP
		VPUFreeLinear(&inMem);
#endif
		return 0;
	}

#ifdef DO_APP1_THUMB
    //LOGD("headerlen: %d",headerlen);
	if(headerlen != 0){
		*thumbPos = JpegOutInfo.jpegFileLen;
		int retlen = JpegOutInfo.jpegFileLen + headerlen - BACKUP_LEN;
		if((retlen & 63) != 0){
			addzerolen = (64 - (retlen & 63));
			memset(startpos + JpegOutInfo.jpegFileLen + headerlen, 0, addzerolen);
		}
		WHENCLOG("thumb filelen: %d, headerlen: %d, addzerolen: %d, before ffd8 len: %d",
			 JpegOutInfo.jpegFileLen, headerlen, addzerolen, retlen+addzerolen);
		retlen = JpegOutInfo.jpegFileLen + headerlen - 4/*LEN_BEFORE_APP1LEN*/ + addzerolen;
		*((unsigned short*)(startpos+4)) = ((retlen&0xff00)>>8) | ((retlen&0xff)<<8);
	}
#endif

#if 0//def HW_JPEG_DEBUG
	{
		ALOGE("encodethumb, final file size: %d", JpegOutInfo.jpegFileLen);
		file = fopen("/flash/outthumb.jpg","wb");
		fwrite((char*)JpegOutInfo.outBufVirAddr + JpegOutInfo.finalOffset, 1, JpegOutInfo.jpegFileLen, file);
		fclose(file);
	}
#endif

#ifdef USE_IPP
	VPUFreeLinear(&inMem);
#endif

#ifdef DO_APP1_THUMB
	if(headerlen != 0){
		//flush out buf
		if(JpegOutInfo.cacheflush){
			JpegOutInfo.cacheflush(1, 0, 0x7fffffff/*JpegOutInfo.jpegFileLen + headerlen*/);
		}
		return JpegOutInfo.jpegFileLen + headerlen - BACKUP_LEN + addzerolen;
	}else {
		return JpegOutInfo.jpegFileLen;
	}
#else
	return JpegOutInfo.jpegFileLen;
#endif
}

#define IPP_ONCE_WIDTH 0x7f0
#define IPP_ONCE_HEIGHT 0x430

//just for 180 rotate
int hw_jpeg_rotate(JpegEncInInfo *inInfo){
#ifdef USE_IPP
	struct rk29_ipp_req ipp_req;
	int ipp_fd;
	int ippret = -1;
	int tmpw = inInfo->inputW;
	int tmph = inInfo->inputH;
	int ippwdotimes = 0;
	int ipphdotimes = 0;
	int i = 0, j = 0;
	int remainw = 0;
	int remainh = 0;
	ipp_fd = open("/dev/rk29-ipp",O_RDWR,0);
	if(ipp_fd < 0)
	{
		ALOGE("hw_jpeg_rotate, open /dev/rk29-ipp fail!");
		return -1;
	}
	ipp_req.timeout = 3000;
	ippwdotimes = tmpw/IPP_ONCE_WIDTH+tmpw%IPP_ONCE_WIDTH==0?0:1;
	ipphdotimes = tmph/IPP_ONCE_WIDTH+tmph%IPP_ONCE_WIDTH==0?0:1;
	remainw = tmpw-IPP_ONCE_WIDTH*(ippwdotimes-1);
	remainh = tmph-IPP_ONCE_HEIGHT*(ipphdotimes-1);
for(j=0;j<ipphdotimes;j++){
	int width = 0;
	int height = 0;
	height = i<ipphdotimes-1?IPP_ONCE_HEIGHT:remainh;
	for(i=0;i<ippwdotimes;i++){
		width = i<ippwdotimes-1?IPP_ONCE_WIDTH:remainw;
		int offset = j*tmpw*IPP_ONCE_HEIGHT+i*IPP_ONCE_WIDTH;
		ipp_req.src0.YrgbMst = inInfo->y_rgb_addr+(offset);
		ipp_req.src0.CbrMst = inInfo->uv_addr+(offset/2);
		ipp_req.src0.w = width;
		ipp_req.src0.h = height;
		ipp_req.src0.fmt = IPP_Y_CBCR_H2V2;//420SP

		offset = (j==ipphdotimes-1?0:remainh+(ipphdotimes-1-j)*IPP_ONCE_HEIGHT)*tmpw+
			i==ippwdotimes-1?0:remainw+(ippwdotimes-1-i)*IPP_ONCE_WIDTH;
		ipp_req.dst0.YrgbMst = inInfo->yuvaddrfor180+IOMMUOffsetFix(offset);
		ipp_req.dst0.CbrMst = inInfo->yuvaddrfor180 + IOMMUOffsetFix(tmpw*tmph + offset/2);
		ipp_req.dst0.w = width;
		ipp_req.dst0.h = height;

		ipp_req.src_vir_w = inInfo->inputW;
		ipp_req.dst_vir_w = tmpw;
		ipp_req.timeout = 500;
		ipp_req.store_clip_mode = 1;
		if(inInfo->rotateDegree == DEGREE_180){
			ipp_req.flag = IPP_ROT_180;
		}else{
			//ipp_req.flag = IPP_ROT_0;
		}
		ipp_req.complete = NULL;
		ippret = ioctl(ipp_fd, IPP_BLIT_SYNC, &ipp_req);//sync
		if(ippret != 0){
			ALOGE("hw_jpeg_rotate, ioctl: IPP_BLIT_SYNC faild!");
			close(ipp_fd);
			return -1;
		}
	}
}
	if(close(ipp_fd) < 0){
		ALOGE("close ipp fd fail. Check here.");
	}
	return 0;
#else
	return -1;
#endif
}

int hw_jpeg_encode(JpegEncInInfo *inInfo, JpegEncOutInfo *outInfo)
{
	JpegEncRet ret = JPEGENC_OK;
	JpegEncInst encoder = NULL;
	JpegEncCfg cfg;
	JpegEncIn encIn;
	JpegEncOut encOut;
	JpegEncThumb thumbCfg;

	/* Output buffer size: For most images 1 byte/pixel is enough
	for high quality JPEG */
	RK_U32 outbuf_size = outInfo->outBuflen;
	RK_U32 pict_bus_address = 0;
#ifdef DO_APP1_THUMB
	unsigned char *startpos = NULL;
	unsigned char endch[BACKUP_LEN] = {0};
	inInfo->uv_addr = inInfo->y_rgb_addr + IOMMUOffsetFix(inInfo->inputH*inInfo->inputW);
#endif
    ALOGD("version: 2014-03-27, support softscale for no ipp,RUN INTO JPEGENCDOER.\n");
    ALOGD("version: 2014-05-25, support IOMMU for rk3288,you need new kernel");
    ALOGD("version: 2014-06-13, fix compile bus in 3288");
	ALOGD("----------outbuf_size :%x---------1",outbuf_size);
	/* Step 1: Initialize an encoder instance */
	memset(&cfg, 0, sizeof(JpegEncCfg));
	cfg.qLevel = inInfo->qLvl;
	cfg.frameType = inInfo->type;
	cfg.markerType = JPEGENC_SINGLE_MARKER;
	cfg.unitsType = JPEGENC_DOTS_PER_INCH;
	cfg.xDensity = 72;
	cfg.yDensity = 72;
	/*  no cropping */
	cfg.inputWidth = inInfo->inputW;
	cfg.codingWidth = inInfo->inputW;
	cfg.inputHeight = inInfo->inputH;
	cfg.codingHeight = inInfo->inputH;
	cfg.xOffset = 0;
	cfg.yOffset = 0;
	if(inInfo->rotateDegree == DEGREE_90 || inInfo->rotateDegree == DEGREE_270){
		cfg.codingWidth = inInfo->inputH;
		cfg.codingHeight = inInfo->inputW;
		cfg.rotation = inInfo->rotateDegree;
	}else{
		cfg.rotation = JPEGENC_ROTATE_0;
		if(inInfo->rotateDegree == DEGREE_180){
			if(hw_jpeg_rotate(inInfo) < 0){
				ALOGD("hw_jpeg_rotate fail.");
				goto end;
			}
		}
	}
	cfg.codingType = JPEGENC_WHOLE_FRAME;
	/* disable */
	cfg.restartInterval = 0;

	outInfo->finalOffset = 0;

	thumbCfg.dataLength = 0;
	if(inInfo->doThumbNail)
	{
		thumbCfg.data = NULL;
		thumbCfg.format = JPEGENC_THUMB_JPEG;//just use jpeg enc
		thumbCfg.width = inInfo->thumbW;
		thumbCfg.height = inInfo->thumbH;
		if(NULL != inInfo->thumbData){
			thumbCfg.dataLength = inInfo->thumbDataLen;
			thumbCfg.data = inInfo->thumbData;
		}else{
			if((thumbCfg.dataLength = encodeThumb(inInfo, outInfo)) <= 0){
				goto end;
			}
//goto end;
			thumbCfg.data = (void*)outInfo->outBufVirAddr;
			//thumbCfg.dataLength -= 2;//ffd8 ffd9or0000
#ifdef DO_APP1_THUMB
			if(inInfo->frameHeader && NULL != inInfo->exifInfo){
				startpos = (((unsigned char*)outInfo->outBufVirAddr) + thumbCfg.dataLength);
				memcpy(endch, startpos, BACKUP_LEN);
				WHENCLOG("thumbnail datalen: %d", thumbCfg.dataLength);
			}
#endif
		}

	}

	if((ret = JpegEncInit(&cfg, &encoder)) != JPEGENC_OK)
	{
		/* Handle here the error situation */
		ALOGD( "JPEGENCDOER: JpegEncInit fail.\n");
		goto end;
	}

	jpegInstance_s *pEncInst = (jpegInstance_s*)encoder;
	pEncInst->cacheflush = outInfo->cacheflush;

#ifdef DO_APP1_THUMB
	if(startpos != NULL){
		//only app1 thumbnail, startpos is not null
		outInfo->finalOffset = 0;
		encIn.pOutBuf = (RK_U8*)outInfo->outBufVirAddr + thumbCfg.dataLength;// == startpos
		encIn.busOutBuf = outInfo->outBufPhyAddr + IOMMUOffsetFix(thumbCfg.dataLength);
		encIn.outBufSize = outbuf_size - thumbCfg.dataLength;
		ALOGD("----------encIn.outBufSize :%x---------2",encIn.outBufSize );
		
	}else{
		outInfo->finalOffset = ((thumbCfg.dataLength+63)&(~63));
		encIn.pOutBuf = (RK_U8*)outInfo->outBufVirAddr + outInfo->finalOffset;
		encIn.busOutBuf = outInfo->outBufPhyAddr + IOMMUOffsetFix(outInfo->finalOffset);
		encIn.outBufSize = outbuf_size - outInfo->finalOffset;
		ALOGD("----------encIn.outBufSize  :%x---------3",encIn.outBufSize  );
		/* Step 2: Configuration of thumbnail */
		if(inInfo->doThumbNail)
		{
			if((ret = JpegEncSetThumbnail(encoder, &thumbCfg)) != JPEGENC_OK)
			{
				/* Handle here the error situation */
				ALOGD( "JPEGENCDOER: JpegEncSetPictureSize fail.\n");
				goto close;
			}
		}
	}
#endif

	/* Step 3: Configuration of picture size */
	if((ret = JpegEncSetPictureSize(encoder, &cfg)) != JPEGENC_OK)
	{
		/* Handle here the error situation */
		ALOGD( "JPEGENCDOER: JpegEncSetPictureSize fail.\n");
		goto close;
	}


	/* frame headers */
	encIn.frameHeader = inInfo->frameHeader;

	/* not 420_p */
	if(inInfo->rotateDegree != DEGREE_180){
		encIn.busLum = inInfo->y_rgb_addr;
		encIn.busCb = inInfo->uv_addr;
	}else{
		encIn.busLum = inInfo->yuvaddrfor180;
		encIn.busCb = inInfo->yuvaddrfor180 + IOMMUOffsetFix(inInfo->inputW*inInfo->inputH);

	}
		encIn.busCr = encIn.busCb+IOMMUOffsetFix(inInfo->inputW*inInfo->inputH/4);

	/* Step 5: Encode the picture */
	/* Loop until the frame is ready */

	//ALOGD( "\t-JPEG: JPEG output bus: 0x%08x\n",
    //               		encIn.busOutBuf);

	//ALOGD( "\t-JPEG: File input bus: 0x%08x\n",
    //                	encIn.busLum);
	//ALOGD("\t\t encoder: %p\n", encoder);
	do
	{
		ret = JpegEncEncode(encoder, &encIn, &encOut);
		switch (ret){
			case JPEGENC_RESTART_INTERVAL:
				ALOGD("JPEGENC_RESTART_INTERVAL");
			case JPEGENC_FRAME_READY:
				//fLOGD(stdout, "OUTPUT JPG length is %d\n",encOut.jfifSize);
				//fout = fopen(outPath, "wb");
				//fwrite(encIn.pOutBuf, encOut.jfifSize, 1, fout);
				//fclose(fout);
				//memcpy(outInfo->outBufVirAddr, outMem.vir_addr,encOut.jfifSize);
				outInfo->jpegFileLen = encOut.jfifSize;
			break;
			default:
			/* All the others are error codes */
			break;
		}
	}
	while (ret == JPEGENC_RESTART_INTERVAL);

close:
end:
	/* Last Step: Release the encoder instance */
	if(JpegEncRelease(encoder) != JPEGENC_OK){
		ALOGD( "JPEGENCDOER: JpegEncRelease fail with : %d\n", ret);
	}else{
#ifdef DO_APP1_THUMB
WHENCLOG("00000000000000000000 ret : %d, startpos: %x, FileLen+datalength: %d", ret, startpos,
outInfo->jpegFileLen + thumbCfg.dataLength);
		if(ret >= 0 && startpos != NULL){
			//only app1 thumbnail, startpos is not null
			memcpy(startpos, endch, BACKUP_LEN);
			//flush out buf
			if(outInfo->cacheflush){
				outInfo->cacheflush(1, 0, 0x7fffffff/*outInfo->jpegFileLen + headerlen*/);
			}
			outInfo->jpegFileLen += thumbCfg.dataLength;			
		}
#endif
	}
	/* release output buffer and picture buffer, if needed */
 	/* nothing else to do */
	ALOGD( "RUN OUT JPEGENCDOER. jpeg len : %d\n", outInfo->jpegFileLen);
	//ALOGD("%x", *((int*)outInfo->outBufVirAddr));
return 0;
}

