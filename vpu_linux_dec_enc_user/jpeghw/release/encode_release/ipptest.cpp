/*
 **
 ** Copyright 2006, The Android Open Source Project
 **
 ** Licensed under the Apache License, Version 2.0 (the "License"); 
 ** you may not use this file except in compliance with the License. 
 ** You may obtain a copy of the License at 
 **
 **     http://www.apache.org/licenses/LICENSE-2.0 
 **
 ** Unless required by applicable law or agreed to in writing, software 
 ** distributed under the License is distributed on an "AS IS" BASIS, 
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 ** See the License for the specific language governing permissions and 
 ** limitations under the License.
 */

#include <stdlib.h>
#include <stdio.h>
#include <utils/Log.h>
#include "rk29-ipp.h"
#include <fcntl.h>
#include <poll.h>
#include <time.h>
//using namespace android;

int main(int argc, char** argv)
{
	struct rk29_ipp_req ipp_req;
	int ipp_fd;
	 int i=0,j;
	 int ret = -1,ret1=-1,ret2=-2;
	 int result = -1;
	
	unsigned int size = 800*480;
	
	static struct pollfd fd;


#define PMEM_VPU_BASE 0x76000000	
	ipp_req.src0.YrgbMst = PMEM_VPU_BASE;
	ipp_req.src0.CbrMst = PMEM_VPU_BASE+size;
	ipp_req.src0.w = 800;
	ipp_req.src0.h = 480;
	ipp_req.src0.fmt = IPP_Y_CBCR_H2V2;
	
	ipp_req.dst0.YrgbMst = PMEM_VPU_BASE+size*2;
	ipp_req.dst0.CbrMst = PMEM_VPU_BASE+size*3;
	ipp_req.dst0.w = 800;
	ipp_req.dst0.h = 480;

	ipp_req.src_vir_w = 800;
	ipp_req.dst_vir_w = 800;
	ipp_req.timeout = 100;
	ipp_req.flag = IPP_ROT_0;
	ipp_req.complete = NULL;//user态的回调函数必须是NULL
/*
	ipp_req.deinterlace_enable = 1;
	ipp_req.deinterlace_para0 = 16;
	ipp_req.deinterlace_para1 = 0;
	ipp_req.deinterlace_para2 = 16;
*/
	//LOGD("open /dev/rk29-ipp");
	ipp_fd = open("/dev/rk29-ipp",O_RDWR,0);
	if(ipp_fd < 0)
	{
		LOGE("open /dev/rk29-ipp fail!");
		return 0;
	}
	
	fd.fd = ipp_fd;
	fd.events = POLLIN;

/*1. loop test IPP_BLIT_SYNC*/	
/*	
	LOGD("ioctl(ipp_fd, IPP_BLIT_SYNC, &ipp_req)");
	do
	{
		ret = ioctl(ipp_fd, IPP_BLIT_SYNC, &ipp_req);
	}while(ret == 0);
*/

/*2. loop test IPP_BLIT_SYNC*/	


	LOGD("test IPP_BLIT_ASYNC!");
	while(1)
	{
		
		ret = ioctl(ipp_fd, IPP_BLIT_ASYNC, &ipp_req);
		
		//usleep(500000);

		if(ret == 0)
		{
			if( poll(&fd,1,-1)>0)
			{
				//LOGD("receive events!");
				//if (fd.revents & POLLIN)
				ret = ioctl(ipp_fd, IPP_GET_RESULT, &result);
				if(ret == 0)
				{
					//LOGD("get result = %d",result);
				}
				else
				{
					LOGE("ioctl:IPP_GET_RESULT faild!");
					break;
				}
			}
			else
			{
				LOGE("IPP poll faild!");
				break;
			}
		}
		else
		{
			LOGE("ioctl: IPP_BLIT_ASYNC faild!");
			break;
		}
		usleep(50000);
	}



/*3.连续下发两个异步请求， 在第二个ioctl处被阻塞住。可能要新开一个线程调用ioctl才行，但用户态应该没有这种情况     
[  131.421795] ipp_blit_async
[  131.421842] ipp_blit_async2这行是在wait_event_interruptible(blit_wait_queue, idle_condition);之后的打印
[  131.424463] ipp_blit_async
[  131.427135] ipp_blit_complete
[  131.430071] ipp_blit_async2
[  131.434494] ipp_blit_complete
*/

/*
	ret1 = ioctl(ipp_fd, IPP_BLIT_ASYNC, &ipp_req);
	
	ret2 = ioctl(ipp_fd, IPP_BLIT_ASYNC, &ipp_req);
	
	if(ret1 == 0)
	{
		if( poll(&fd,1,-1)>0)
		{
			LOGD("receive events1!");
			//if (fd.revents & POLLIN)
			ret1 = ioctl(ipp_fd, IPP_GET_RESULT, &result);
			if(ret1 == 0)
			{
				LOGD("get result1 = %d",result);
			}
			else
			{
				LOGE("ioctl:IPP_GET_RESULT1 faild!");
			}
		}
		else
		{
			LOGE("IPP poll1 faild!");
	
		}
	}
	else
	{
		LOGE("ioctl: IPP_BLIT_ASYNC1 faild!");

	}

	if(ret2 == 0)
	{
		if( poll(&fd,1,-1)>0)
		{
			LOGD("receive events2!");
			//if (fd.revents & POLLIN)
			ret2 = ioctl(ipp_fd, IPP_GET_RESULT, &result);
			if(ret2 == 0)
			{
				LOGD("get result2 = %d",result);
			}
			else
			{
				LOGE("ioctl:IPP_GET_RESULT2 faild!");
				
			}
		}
		else
		{
			LOGE("IPP poll2 faild!");
			
		}
	}
	else
	{
		LOGE("ioctl: IPP_BLIT_ASYNC2 faild!");
		
	}
*/

/*4 延时一段时间后才去poll*/
/*
	ret = ioctl(ipp_fd, IPP_BLIT_ASYNC, &ipp_req);
	sleep(5);
	if(ret == 0)
	{
		if( poll(&fd,1,-1)>0)
		{
			//LOGD("receive events!");
			//if (fd.revents & POLLIN)
			ret = ioctl(ipp_fd, IPP_GET_RESULT, &result);
			if(ret == 0)
			{
				//LOGD("get result = %d",result);
			}
			else
			{
				LOGE("ioctl:IPP_GET_RESULT faild!");
		
			}
		}
		else
		{
			LOGE("IPP poll faild!");
		
		}
	}
	else
	{
		LOGE("ioctl: IPP_BLIT_ASYNC faild!");
	}
*/


    return 0;
}
