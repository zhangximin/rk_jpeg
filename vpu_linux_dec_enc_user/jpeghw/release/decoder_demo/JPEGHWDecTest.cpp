#include "SkStream.h"
//#include "hw_jpegdecapi.h"
#include "SkHwJpegUtility.h"
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
//#include "sxun_hwjpeg_decode.h"
#include <ion.h>
#include <rockchip_ion.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>


//int hwjpeg_decode(char* data, int size, int stride, void* pDest,int width, int height)
//unsigned char * hwjpeg_decode(char* data, int size, int stride,void * pDest,int width, int height)
bool hwjpeg_decode(char* data, char * data_out, int size, int stride,int width, int height)
{
    printf("   <%s>_%d \n", __func__, __LINE__);
    SkMemoryStream stream(data,size,false);
    //SkAutoTUnref<SkMemoryStream> stream(new SkMemoryStream(data,size,false)); 
    HwJpegInputInfo hwInfo;
	//memset(hwInfo,0,sizeof(hwInfo));
    sk_hw_jpeg_source_mgr sk_hw_stream(&stream,&hwInfo, false);
    
    hwInfo.justcaloutwh = 0;
    hwInfo.streamCtl.inStream = &sk_hw_stream;
    hwInfo.streamCtl.wholeStreamLength = stream.getLength();// > 64M? break;
    hwInfo.streamCtl.thumbOffset = -1;
    hwInfo.streamCtl.thumbLength = -1;
    hwInfo.streamCtl.useThumb = 0;
    
    ReusePmem thumbPmem;
    thumbPmem.reuse = 0;
   
    HwJpegOutputInfo outInfo;
	//memset(hwInfo,0,sizeof(outInfo));
    outInfo.thumbPmem = &thumbPmem;
    outInfo.outAddr = NULL;
    
    PostProcessInfo * ppInfo = &hwInfo.ppInfo;
    ppInfo->outFomart = 1;
    ppInfo->shouldDither = 0;
    ppInfo->scale_denom = 1;
    ppInfo->cropX = 0;
    ppInfo->cropY = 0;
    ppInfo->cropW = -1;
    ppInfo->cropH = -1;
    
    int ret = -1;  
    char reuseBitmap = 0;
    printf("   <%s>_%d \n", __func__, __LINE__);
//    printf("PSurface HWjpeg00_decode start width:%d, height:%d, stride:%d ", width, height, stride);
    if((ret = hw_jpeg_decode(&hwInfo,&outInfo, &reuseBitmap, width, height)) >= 0)
    {
#if 0
                        FILE * testfs = fopen("./testfs.yuv", "wb");
                        char * testds = (char *)outInfo.outAddr;
                        fwrite(testds, 1, width * height * 4, testfs);
                        fclose(testfs);
#endif
	memcpy(data_out, (char *)outInfo.outAddr, width * height * 4);
     //   printf("hw_jpeg_decode is ok\n");
	hw_jpeg_release(outInfo.decoderHandle);
	outInfo.decoderHandle = NULL;
	//stream = NULL;
	printf("%p\n",outInfo.outAddr);
	usleep(100);
	return true;
    }
	hw_jpeg_release(outInfo.decoderHandle);
	outInfo.decoderHandle = NULL;
	//stream = NULL;
    
  //  printf("hw_jpeg_decode is error error\n");
   // printf("hw_jpeg_decode error outInfo.outAddr=================%p\n",outInfo.outAddr);

    return false;
}





size_t getFileSize(char* filename)
{
	size_t filesize = 0;
	FILE* File = fopen(filename,"rb");
	if(File == NULL)
	{
		printf("<%s>_%d  fopen jpgFile(%s) fail: \n", __func__, __LINE__, filename);
		return 0;
	}
	fseek(File, 0, SEEK_END);
	filesize = ftell(File);
	fseek(File, 0, SEEK_SET);
	fclose(File);
	
	return filesize; 
}

int main(int argc,const char * argv[])
{

	FILE *jpgFile = NULL;
	FILE *bgraFile = NULL;
#if 1
	int width = atoi(argv[3]);
	int height = atoi(argv[4]);
	int out_size = width * height * 4;	//atoi(argv[3])*atoi(argv[4])*4;
	int stride = width * 4;	//atoi(argv[3])*4;
#endif
	int infilesize=0;
	char * outDest = NULL;
	char * src = NULL;
	size_t  jpeg_file_size = getFileSize((char *)argv[1]); 
	printf("   <%s>_%d  yc    jpeg_file_size = %d\n", __func__, __LINE__, jpeg_file_size);
      
//	for(int i = 0; i < 1000;i++)        
//	{
	src = (char*)malloc(jpeg_file_size);
	memset(src,0,jpeg_file_size);
	if(NULL == src){
		printf("malloc is error\n");
		return -1;
	}
	outDest = (char*)malloc(out_size);
	memset(outDest,0,out_size);
	if(NULL == outDest){
		printf("malloc outDest is error\n");
                return -1;
	}
        jpgFile = fopen((char *)argv[1],"rb");
	fread(src,1,jpeg_file_size,jpgFile);
	printf("   <%s>_%d \n", __func__, __LINE__);
	bgraFile = fopen((char *)argv[2],"wb");
	if (bgraFile == NULL)
	{
		printf("fopen <%s>_%d  bgraFile(%s) is fail: \n", __func__, __LINE__, (char *)argv[2]);
		return -1;
	}
	printf("   <%s>_%d \n", __func__, __LINE__);

	if(hwjpeg_decode(src, outDest, jpeg_file_size, stride,width,height)){
		printf("   <%s>_%d    out_size = %d   outDest = %p\n", __func__, __LINE__, out_size, outDest);
		fwrite(outDest, 1, out_size, bgraFile);
	}else{
		printf("   <%s>_%d     dec error ~\n", __func__, __LINE__);
	}

	fclose(bgraFile);
        fclose(jpgFile);
	free(src);

//	}
	return 1;
}
