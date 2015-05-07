/*
 * rk_jpeg.h
 *
 *  Created on: 2015-4-14
 *      Author: simon
 */

#ifndef RK_JPEG_H_
#define RK_JPEG_H_

#ifdef __cplusplus
extern "C"
{
#endif

int hwjpeg_decoder(char* data,char * data_out, int size, int loff,int toff,int width,int height);

#ifdef __cplusplus
}
#endif
#endif /* RK_JPEG_H_ */
