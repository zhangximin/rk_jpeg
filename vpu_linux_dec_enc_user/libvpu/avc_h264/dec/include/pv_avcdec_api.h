#ifndef _PV_AVCDEC_API_H_
#define _PV_AVCDEC_API_H_

//#include <media/stagefright/MediaBufferGroup.h>
//#include <utils/threads.h>
#include "Errors.h"

typedef int     int32_t;
typedef unsigned int uint32_t;
typedef int status_t;
typedef unsigned char uint8_t;

namespace android {

typedef struct decContainer decContainer_t;
typedef struct VPUMem VPUMemLinear_t;

class On2_AvcDecoder
{
public:
     On2_AvcDecoder();
    ~On2_AvcDecoder();
    int32_t pv_on2avcdecoder_init(uint32_t ts_en);
    int32_t pv_on2avcdecoder_oneframe(uint8_t* aOutBuffer, uint32_t *aOutputLength,
                                      uint8_t* aInputBuf,  uint32_t* aInBufSize,
                                      int64_t &InputTimestamp);
    int32_t pv_on2avc_flushOneFrameInDpb(uint8_t* aOutBuffer, uint32_t *aOutputLength);
    int32_t pv_on2avcdecoder_deinit();
    void    pv_on2avcdecoder_rest();
    int32_t pv_on2avcdecoder_perform(int32_t cmd, void *data);

private:
    int32_t isFlashPlayer;
    int32_t getOneFrame(uint8_t* aOutBuffer, uint32_t *aOutputLength, int64_t& InputTimestamp);
    int32_t prepareStream(uint8_t* aInputBuf, uint32_t aInBufSize);
    uint32_t status;
    uint32_t streamSize;
    VPUMemLinear_t *streamMem;
    decContainer_t *H264deccont;

//#define STREAM_DEBUG
#ifdef  STREAM_DEBUG
    FILE *fpStream;
#endif
//#define OUTPUT_DEBUG
#ifdef  OUTPUT_DEBUG
    FILE *fpOut;
#endif
//#define PTS_DEBUG
#ifdef  PTS_DEBUG
    FILE *pts_in;
    FILE *pts_out;
#endif
#define FLASH_DROP_FRAME
#ifdef FLASH_DROP_FRAME
	int32_t dropThreshold;
	int32_t curFrameNum;
#endif
};

}

#endif

