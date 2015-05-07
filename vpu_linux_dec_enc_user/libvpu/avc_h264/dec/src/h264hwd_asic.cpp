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
--  Description : Hardware interface read/write
--
------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: h264hwd_asic.c,v $
--  $Revision: 1.51 $
--  $Date: 2010/05/11 09:32:47 $
--
------------------------------------------------------------------------------*/

#include "basetype.h"
#include "h264hwd_regdrv.h"
#include "h264hwd_asic.h"
#include "h264hwd_container.h"
#include "h264hwd_debug.h"
#include "h264hwd_util.h"
//#include "h264hwd_cabac.h"
#include "h264decapi.h"

#include "refbuffer.h"
//#include "h264_pp_multibuffer.h"

//#include "dwl_linux.h"
#include "vpu_drv.h"
#include "vpu.h"
#include "h264decapi.h"
#include <stdio.h>

#define ALOGE	printf
#define ALOGV	printf
namespace android {

#define ASIC_HOR_MV_MASK            0x07FFFU
#define ASIC_VER_MV_MASK            0x01FFFU

#define ASIC_HOR_MV_OFFSET          17U
#define ASIC_VER_MV_OFFSET          4U

//DispLinearMem   DisplayBuff[3];

static void H264FlushRegs(decContainer_t * pDecCont);
static void h264StreamPosUpdate(decContainer_t * pDecCont);
static void H264PrepareCabacInitTables(DecAsicBuffers_t * pAsicBuff);

VPUMemLinear_t   DisplayBuff;//[3];

void pipelineconfig(u32 *h264Regs, u32 memorydispaddr,u32 inwidth, u32 inheight, u32 outwidth, u32 outheight, ppOutFormat mode);


#ifndef TRACE_PP_CTRL
#ifdef LINUX
#define TRACE_PP_CTRL(...)          do{}while(0)
#else
#define TRACE_PP_CTRL               printf
#endif
#else
#undef TRACE_PP_CTRL
#define TRACE_PP_CTRL(...)          printf(__VA_ARGS__)
#endif

const u32 refBase[16] = {
    HWIF_REFER0_BASE, HWIF_REFER1_BASE, HWIF_REFER2_BASE,
    HWIF_REFER3_BASE, HWIF_REFER4_BASE, HWIF_REFER5_BASE,
    HWIF_REFER6_BASE, HWIF_REFER7_BASE, HWIF_REFER8_BASE,
    HWIF_REFER9_BASE, HWIF_REFER10_BASE, HWIF_REFER11_BASE,
    HWIF_REFER12_BASE, HWIF_REFER13_BASE, HWIF_REFER14_BASE,
    HWIF_REFER15_BASE
};

const u32 refFieldMode[16] = {
    HWIF_REFER0_FIELD_E, HWIF_REFER1_FIELD_E, HWIF_REFER2_FIELD_E,
    HWIF_REFER3_FIELD_E, HWIF_REFER4_FIELD_E, HWIF_REFER5_FIELD_E,
    HWIF_REFER6_FIELD_E, HWIF_REFER7_FIELD_E, HWIF_REFER8_FIELD_E,
    HWIF_REFER9_FIELD_E, HWIF_REFER10_FIELD_E, HWIF_REFER11_FIELD_E,
    HWIF_REFER12_FIELD_E, HWIF_REFER13_FIELD_E, HWIF_REFER14_FIELD_E,
    HWIF_REFER15_FIELD_E
};

const u32 refTopc[16] = {
    HWIF_REFER0_TOPC_E, HWIF_REFER1_TOPC_E, HWIF_REFER2_TOPC_E,
    HWIF_REFER3_TOPC_E, HWIF_REFER4_TOPC_E, HWIF_REFER5_TOPC_E,
    HWIF_REFER6_TOPC_E, HWIF_REFER7_TOPC_E, HWIF_REFER8_TOPC_E,
    HWIF_REFER9_TOPC_E, HWIF_REFER10_TOPC_E, HWIF_REFER11_TOPC_E,
    HWIF_REFER12_TOPC_E, HWIF_REFER13_TOPC_E, HWIF_REFER14_TOPC_E,
    HWIF_REFER15_TOPC_E
};

const u32 refPicNum[16] = {
    HWIF_REFER0_NBR, HWIF_REFER1_NBR, HWIF_REFER2_NBR,
    HWIF_REFER3_NBR, HWIF_REFER4_NBR, HWIF_REFER5_NBR,
    HWIF_REFER6_NBR, HWIF_REFER7_NBR, HWIF_REFER8_NBR,
    HWIF_REFER9_NBR, HWIF_REFER10_NBR, HWIF_REFER11_NBR,
    HWIF_REFER12_NBR, HWIF_REFER13_NBR, HWIF_REFER14_NBR,
    HWIF_REFER15_NBR
};


/*------------------------------------------------------------------------------
                Reference list initialization
------------------------------------------------------------------------------*/
#define IS_SHORT_TERM_FRAME(a) ((a)->status[0] == SHORT_TERM && \
                                (a)->status[1] == SHORT_TERM)
#define IS_SHORT_TERM_FRAME_F(a) ((a)->status[0] == SHORT_TERM || \
                                (a)->status[1] == SHORT_TERM)
#define IS_LONG_TERM_FRAME(a) ((a)->status[0] == LONG_TERM && \
                                (a)->status[1] == LONG_TERM)
#define IS_LONG_TERM_FRAME_F(a) ((a)->status[0] == LONG_TERM || \
                                (a)->status[1] == LONG_TERM)
#define IS_REF_FRAME(a) ((a)->status[0] && (a)->status[0] != EMPTY && \
                         (a)->status[1] && (a)->status[1] != EMPTY )
#define IS_REF_FRAME_F(a) (((a)->status[0] && (a)->status[0] != EMPTY) || \
                           ((a)->status[1] && (a)->status[1] != EMPTY) )

#define FRAME_POC(a) MIN((a)->picOrderCnt[0], (a)->picOrderCnt[1])
#define FIELD_POC(a) MIN(((a)->status[0] != EMPTY) ? \
                          (a)->picOrderCnt[0] : 0x7FFFFFFF,\
                         ((a)->status[1] != EMPTY) ? \
                          (a)->picOrderCnt[1] : 0x7FFFFFFF)

#define INVALID_IDX 0xFFFFFFFF
#define MIN_POC 0x80000000
#define MAX_POC 0x7FFFFFFF

const u32 refPicList0[16] = {
    HWIF_BINIT_RLIST_F0, HWIF_BINIT_RLIST_F1, HWIF_BINIT_RLIST_F2,
    HWIF_BINIT_RLIST_F3, HWIF_BINIT_RLIST_F4, HWIF_BINIT_RLIST_F5,
    HWIF_BINIT_RLIST_F6, HWIF_BINIT_RLIST_F7, HWIF_BINIT_RLIST_F8,
    HWIF_BINIT_RLIST_F9, HWIF_BINIT_RLIST_F10, HWIF_BINIT_RLIST_F11,
    HWIF_BINIT_RLIST_F12, HWIF_BINIT_RLIST_F13, HWIF_BINIT_RLIST_F14,
    HWIF_BINIT_RLIST_F15
};

const u32 refPicList1[16] = {
    HWIF_BINIT_RLIST_B0, HWIF_BINIT_RLIST_B1, HWIF_BINIT_RLIST_B2,
    HWIF_BINIT_RLIST_B3, HWIF_BINIT_RLIST_B4, HWIF_BINIT_RLIST_B5,
    HWIF_BINIT_RLIST_B6, HWIF_BINIT_RLIST_B7, HWIF_BINIT_RLIST_B8,
    HWIF_BINIT_RLIST_B9, HWIF_BINIT_RLIST_B10, HWIF_BINIT_RLIST_B11,
    HWIF_BINIT_RLIST_B12, HWIF_BINIT_RLIST_B13, HWIF_BINIT_RLIST_B14,
    HWIF_BINIT_RLIST_B15
};

const u32 refPicListP[16] = {
    HWIF_PINIT_RLIST_F0, HWIF_PINIT_RLIST_F1, HWIF_PINIT_RLIST_F2,
    HWIF_PINIT_RLIST_F3, HWIF_PINIT_RLIST_F4, HWIF_PINIT_RLIST_F5,
    HWIF_PINIT_RLIST_F6, HWIF_PINIT_RLIST_F7, HWIF_PINIT_RLIST_F8,
    HWIF_PINIT_RLIST_F9, HWIF_PINIT_RLIST_F10, HWIF_PINIT_RLIST_F11,
    HWIF_PINIT_RLIST_F12, HWIF_PINIT_RLIST_F13, HWIF_PINIT_RLIST_F14,
    HWIF_PINIT_RLIST_F15
};

const u32 cabacInitValues[4*460*2/4] =
{
    0x14f10236, 0x034a14f1, 0x0236034a, 0xe47fe968, 0xfa35ff36, 0x07330000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x0029003f, 0x003f003f, 0xf7530456, 0x0061f948, 0x0d29033e, 0x000b0137,
    0x0045ef7f, 0xf3660052, 0xf94aeb6b, 0xe57fe17f, 0xe87fee5f, 0xe57feb72,
    0xe27fef7b, 0xf473f07a, 0xf573f43f, 0xfe44f154, 0xf368fd46, 0xf85df65a,
    0xe27fff4a, 0xfa61f95b, 0xec7ffc38, 0xfb52f94c, 0xea7df95d, 0xf557fd4d,
    0xfb47fc3f, 0xfc44f454, 0xf93ef941, 0x083d0538, 0xfe420140, 0x003dfe4e,
    0x01320734, 0x0a23002c, 0x0b26012d, 0x002e052c, 0x1f110133, 0x07321c13,
    0x10210e3e, 0xf36cf164, 0xf365f35b, 0xf45ef658, 0xf054f656, 0xf953f357,
    0xed5e0146, 0x0048fb4a, 0x123bf866, 0xf164005f, 0xfc4b0248, 0xf54bfd47,
    0x0f2ef345, 0x003e0041, 0x1525f148, 0x09391036, 0x003e0c48, 0x18000f09,
    0x08190d12, 0x0f090d13, 0x0a250c12, 0x061d1421, 0x0f1e042d, 0x013a003e,
    0x073d0c26, 0x0b2d0f27, 0x0b2a0d2c, 0x102d0c29, 0x0a311e22, 0x122a0a37,
    0x1133112e, 0x00591aed, 0x16ef1aef, 0x1ee71cec, 0x21e925e5, 0x21e928e4,
    0x26ef21f5, 0x28f129fa, 0x26012911, 0x1efa1b03, 0x1a1625f0, 0x23fc26f8,
    0x26fd2503, 0x26052a00, 0x23102716, 0x0e301b25, 0x153c0c44, 0x0261fd47,
    0xfa2afb32, 0xfd36fe3e, 0x003a013f, 0xfe48ff4a, 0xf75bfb43, 0xfb1bfd27,
    0xfe2c002e, 0xf040f844, 0xf64efa4d, 0xf656f45c, 0xf137f63c, 0xfa3efc41,
    0xf449f84c, 0xf950f758, 0xef6ef561, 0xec54f54f, 0xfa49fc4a, 0xf356f360,
    0xf561ed75, 0xf84efb21, 0xfc30fe35, 0xfd3ef347, 0xf64ff456, 0xf35af261,
    0x0000fa5d, 0xfa54f84f, 0x0042ff47, 0x003efe3c, 0xfe3bfb4b, 0xfd3efc3a,
    0xf742ff4f, 0x00470344, 0x0a2cf93e, 0x0f240e28, 0x101b0c1d, 0x012c1424,
    0x1220052a, 0x01300a3e, 0x112e0940, 0xf468f561, 0xf060f958, 0xf855f955,
    0xf755f358, 0x0442fd4d, 0xfd4cfa4c, 0x0a3aff4c, 0xff53f963, 0xf25f025f,
    0x004cfb4a, 0x0046f54b, 0x01440041, 0xf249033e, 0x043eff44, 0xf34b0b37,
    0x05400c46, 0x0f060613, 0x07100c0e, 0x120d0d0b, 0x0d0f0f10, 0x0c170d17,
    0x0f140e1a, 0x0e2c1128, 0x112f1811, 0x15151916, 0x1f1b161d, 0x13230e32,
    0x0a39073f, 0xfe4dfc52, 0xfd5e0945, 0xf46d24dd, 0x24de20e6, 0x25e22ce0,
    0x22ee22f1, 0x28f121f9, 0x23fb2100, 0x2602210d, 0x17230d3a, 0x1dfd1a00,
    0x161e1ff9, 0x23f122fd, 0x220324ff, 0x2205200b, 0x2305220c, 0x270b1e1d,
    0x221a1d27, 0x13421f15, 0x1f1f1932, 0xef78ec70, 0xee72f555, 0xf15cf259,
    0xe647f151, 0xf2500044, 0xf246e838, 0xe944e832, 0xf54a17f3, 0x1af328f1,
    0x31f22c03, 0x2d062c22, 0x21361352, 0xfd4bff17, 0x0122012b, 0x0036fe37,
    0x003d0140, 0x0044f75c, 0xf26af361, 0xf15af45a, 0xee58f649, 0xf74ff256,
    0xf649f646, 0xf645fb42, 0xf740fb3a, 0x023b15f6, 0x18f51cf8, 0x1cff1d03,
    0x1d092314, 0x1d240e43, 0x14f10236, 0x034a14f1, 0x0236034a, 0xe47fe968,
    0xfa35ff36, 0x07331721, 0x17021500, 0x01090031, 0xdb760539, 0xf34ef541,
    0x013e0c31, 0xfc491132, 0x1240092b, 0x1d001a43, 0x105a0968, 0xd27fec68,
    0x0143f34e, 0xf541013e, 0xfa56ef5f, 0xfa3d092d, 0xfd45fa51, 0xf5600637,
    0x0743fb56, 0x0258003a, 0xfd4cf65e, 0x05360445, 0xfd510058, 0xf943fb4a,
    0xfc4afb50, 0xf948013a, 0x0029003f, 0x003f003f, 0xf7530456, 0x0061f948,
    0x0d29033e, 0x002dfc4e, 0xfd60e57e, 0xe462e765, 0xe943e452, 0xec5ef053,
    0xea6eeb5b, 0xee66f35d, 0xe37ff95c, 0xfb59f960, 0xf36cfd2e, 0xff41ff39,
    0xf75dfd4a, 0xf75cf857, 0xe97e0536, 0x063c063b, 0x0645ff30, 0x0044fc45,
    0xf858fe55, 0xfa4eff4b, 0xf94d0236, 0x0532fd44, 0x0132062a, 0xfc51013f,
    0xfc460043, 0x0239fe4c, 0x0b230440, 0x013d0b23, 0x12190c18, 0x0d1d0d24,
    0xf65df949, 0xfe490d2e, 0x0931f964, 0x09350235, 0x0535fe3d, 0x00380038,
    0xf33ffb3c, 0xff3e0439, 0xfa450439, 0x0e270433, 0x0d440340, 0x013d093f,
    0x07321027, 0x052c0434, 0x0b30fb3c, 0xff3b003b, 0x1621052c, 0x0e2bff4e,
    0x003c0945, 0x0b1c0228, 0x032c0031, 0x002e022c, 0x0233002f, 0x0427023e,
    0x062e0036, 0x0336023a, 0x043f0633, 0x06390735, 0x06340637, 0x0b2d0e24,
    0x0835ff52, 0x0737fd4e, 0x0f2e161f, 0xff541907, 0x1ef91c03, 0x1c042000,
    0x22ff1e06, 0x1e062009, 0x1f131a1b, 0x1a1e2514, 0x1c221146, 0x0143053b,
    0x0943101e, 0x12201223, 0x161d181f, 0x1726122b, 0x14290b3f, 0x093b0940,
    0xff5efe59, 0xf76cfa4c, 0xfe2c002d, 0x0034fd40, 0xfe3bfc46, 0xfc4bf852,
    0xef66f74d, 0x0318002a, 0x00300037, 0xfa3bf947, 0xf453f557, 0xe277013a,
    0xfd1dff24, 0x0126022b, 0xfa37003a, 0x0040fd4a, 0xf65a0046, 0xfc1d051f,
    0x072a013b, 0xfe3afd48, 0xfd51f561, 0x003a0805, 0x0a0e0e12, 0x0d1b0228,
    0x003afd46, 0xfa4ff855, 0x0000f36a, 0xf06af657, 0xeb72ee6e, 0xf262ea6e,
    0xeb6aee67, 0xeb6be96c, 0xe670f660, 0xf45ffb5b, 0xf75dea5e, 0xfb560943,
    0xfc50f655, 0xff46073c, 0x093a053d, 0x0c320f32, 0x12311136, 0x0a29072e,
    0xff330731, 0x08340929, 0x062f0237, 0x0d290a2c, 0x06320535, 0x0d31043f,
    0x0640fe45, 0xfe3b0646, 0x0a2c091f, 0x0c2b0335, 0x0e220a26, 0xfd340d28,
    0x1120072c, 0x07260d32, 0x0a391a2b, 0x0e0b0b0e, 0x090b120b, 0x150917fe,
    0x20f120f1, 0x22eb27e9, 0x2adf29e1, 0x2ee426f4, 0x151d2de8, 0x35d330e6,
    0x41d52bed, 0x27f61e09, 0x121a141b, 0x0039f252, 0xfb4bed61, 0xdd7d1b00,
    0x1c001ffc, 0x1b062208, 0x1e0a1816, 0x21131620, 0x1a1f1529, 0x1a2c172f,
    0x10410e47, 0x083c063f, 0x11411518, 0x17141a17, 0x1b201c17, 0x1c181728,
    0x18201c1d, 0x172a1339, 0x1635163d, 0x0b560c28, 0x0b330e3b, 0xfc4ff947,
    0xfb45f746, 0xf842f644, 0xed49f445, 0xf046f143, 0xec3eed46, 0xf042ea41,
    0xec3f09fe, 0x1af721f7, 0x27f929fe, 0x2d033109, 0x2d1b243b, 0xfa42f923,
    0xf92af82d, 0xfb30f438, 0xfa3cfb3e, 0xf842f84c, 0xfb55fa51, 0xf64df951,
    0xef50ee49, 0xfc4af653, 0xf747f743, 0xff3df842, 0xf242003b, 0x023b15f3,
    0x21f227f9, 0x2efe3302, 0x3c063d11, 0x37222a3e, 0x14f10236, 0x034a14f1,
    0x0236034a, 0xe47fe968, 0xfa35ff36, 0x07331619, 0x22001000, 0xfe090429,
    0xe3760241, 0xfa47f34f, 0x05340932, 0xfd460a36, 0x1a221316, 0x28003902,
    0x29241a45, 0xd37ff165, 0xfc4cfa47, 0xf34f0534, 0x0645f35a, 0x0034082b,
    0xfe45fb52, 0xf660023b, 0x024bfd57, 0xfd640138, 0xfd4afa55, 0x003bfd51,
    0xf956fb5f, 0xff42ff4d, 0x0146fe56, 0xfb48003d, 0x0029003f, 0x003f003f,
    0xf7530456, 0x0061f948, 0x0d29033e, 0x0d0f0733, 0x0250d97f, 0xee5bef60,
    0xe651dd62, 0xe866e961, 0xe577e863, 0xeb6eee66, 0xdc7f0050, 0xfb59f95e,
    0xfc5c0027, 0x0041f154, 0xdd7ffe49, 0xf468f75b, 0xe17f0337, 0x07380737,
    0x083dfd35, 0x0044f94a, 0xf758f367, 0xf35bf759, 0xf25cf84c, 0xf457e96e,
    0xe869f64e, 0xec70ef63, 0xb27fba7f, 0xce7fd27f, 0xfc42fb4e, 0xfc47f848,
    0x023bff37, 0xf946fa4b, 0xf859de77, 0xfd4b2014, 0x1e16d47f, 0x0036fb3d,
    0x003aff3c, 0xfd3df843, 0xe754f24a, 0xfb410534, 0x0239003d, 0xf745f546,
    0x1237fc47, 0x003a073d, 0x09291219, 0x0920052b, 0x092f002c, 0x0033022e,
    0x1326fc42, 0x0f260c2a, 0x09220059, 0x042d0a1c, 0x0a1f21f5, 0x34d5120f,
    0x1c0023ea, 0x26e72200, 0x27ee20f4, 0x66a20000, 0x38f121fc, 0x1d0a25fb,
    0x33e327f7, 0x34de45c6, 0x43c12cfb, 0x200737e3, 0x20010000, 0x1b2421e7,
    0x22e224e4, 0x26e426e5, 0x22ee23f0, 0x22f220f8, 0x25fa2300, 0x1e0a1c12,
    0x1a191d29, 0x004b0248, 0x084d0e23, 0x121f1123, 0x151e112d, 0x142a122d,
    0x1b1a1036, 0x07421038, 0x0b490a43, 0xf674e970, 0xf147f93d, 0x0035fb42,
    0xf54df750, 0xf754f657, 0xde7feb65, 0xfd27fb35, 0xf93df54b, 0xf14def5b,
    0xe76be76f, 0xe47af54c, 0xf62cf634, 0xf639f73a, 0xf048f945, 0xfc45fb4a,
    0xf7560242, 0xf7220120, 0x0b1f0534, 0xfe37fe43, 0x0049f859, 0x03340704,
    0x0a081108, 0x10130325, 0xff3dfb49, 0xff46fc4e, 0x0000eb7e, 0xe97cec6e,
    0xe67ee77c, 0xef69e579, 0xe575ef66, 0xe675e574, 0xdf7af65f, 0xf264f85f,
    0xef6fe472, 0xfa59fe50, 0xfc52f755, 0xf851ff48, 0x05400143, 0x09380045,
    0x01450745, 0xf945fa43, 0xf04dfe40, 0x023dfa43, 0xfd400239, 0xfd41fd42,
    0x003e0933, 0xff42fe47, 0xfe4bff46, 0xf7480e3c, 0x1025002f, 0x12230b25,
    0x0c290a29, 0x02300c29, 0x0d29003b, 0x03321328, 0x03421232, 0x13fa12fa,
    0x0e001af4, 0x1ff021e7, 0x21ea25e4, 0x27e22ae2, 0x2fd62ddc, 0x31de29ef,
    0x200945b9, 0x3fc142c0, 0x4db636d9, 0x34dd29f6, 0x240028ff, 0x1e0e1c1a,
    0x17250c37, 0x0b4125df, 0x27dc28db, 0x26e22edf, 0x2ae228e8, 0x31e326f4,
    0x28f626fd, 0x2efb1f14, 0x1d1e192c, 0x0c300b31, 0x1a2d1616, 0x17161b15,
    0x21141a1c, 0x1e181b22, 0x122a1927, 0x12320c46, 0x15360e47, 0x0b531920,
    0x15311536, 0xfb55fa51, 0xf64df951, 0xef50ee49, 0xfc4af653, 0xf747f743,
    0xff3df842, 0xf242003b, 0x023b11f6, 0x20f32af7, 0x31fb3500, 0x4003440a,
    0x421b2f39, 0xfb470018, 0xff24fe2a, 0xfe34f739, 0xfa3ffc41, 0xfc43f952,
    0xfd51fd4c, 0xf948fa4e, 0xf448f244, 0xfd46fa4c, 0xfb42fb3e, 0x0039fc3d,
    0xf73c0136, 0x023a11f6, 0x20f32af7, 0x31fb3500, 0x4003440a, 0x421b2f39,
    0x14f10236, 0x034a14f1, 0x0236034a, 0xe47fe968, 0xfa35ff36, 0x07331d10,
    0x19000e00, 0xf633fd3e, 0xe5631a10, 0xfc55e866, 0x05390639, 0xef490e39,
    0x1428140a, 0x1d003600, 0x252a0c61, 0xe07fea75, 0xfe4afc55, 0xe8660539,
    0xfa5df258, 0xfa2c0437, 0xf559f167, 0xeb741339, 0x143a0454, 0x0660013f,
    0xfb55f36a, 0x053f064b, 0xfd5aff65, 0x0337fc4f, 0xfe4bf461, 0xf932013c,
    0x0029003f, 0x003f003f, 0xf7530456, 0x0061f948, 0x0d29033e, 0x0722f758,
    0xec7fdc7f, 0xef5bf25f, 0xe754e756, 0xf459ef5b, 0xe17ff24c, 0xee67f35a,
    0xdb7f0b50, 0x054c0254, 0x054efa37, 0x043df253, 0xdb7ffb4f, 0xf568f55b,
    0xe27f0041, 0xfe4f0048, 0xfc5cfa38, 0x0344f847, 0xf362fc56, 0xf458fb52,
    0xfd48fc43, 0xf848f059, 0xf745ff3b, 0x05420439, 0xfc47fe47, 0x023aff4a,
    0xfc2cff45, 0x003ef933, 0xfc2ffa2a, 0xfd29fa35, 0x084cf74e, 0xf5530934,
    0x0043fb5a, 0x0143f148, 0xfb4bf850, 0xeb53eb40, 0xf31fe740, 0xe35e094b,
    0x113ff84a, 0xfb23fe1b, 0x0d5b0341, 0xf945084d, 0xf642033e, 0xfd44ec51,
    0x001e0107, 0xfd17eb4a, 0x1042e97c, 0x11252cee, 0x32deea7f, 0x0427002a,
    0x07220b1d, 0x081f0625, 0x072a0328, 0x08210d2b, 0x0d24042f, 0x0337023a,
    0x063c082c, 0x0b2c0e2a, 0x07300438, 0x04340d25, 0x0931133a, 0x0a300c2d,
    0x00451421, 0x083f23ee, 0x21e71cfd, 0x180a1b00, 0x22f234d4, 0x27e81311,
    0x1f19241d, 0x1821220f, 0x1e141649, 0x1422131f, 0x1b2c1310, 0x0f240f24,
    0x151c1915, 0x1e141f0c, 0x1b10182a, 0x005d0e38, 0x0f391a26, 0xe87fe873,
    0xea52f73e, 0x0035003b, 0xf255f359, 0xf35ef55c, 0xe37feb64, 0xf239f443,
    0xf547f64d, 0xeb55f058, 0xe968f162, 0xdb7ff652, 0xf830f83d, 0xf842f946,
    0xf24bf64f, 0xf753f45c, 0xee6cfc4f, 0xea45f04b, 0xfe3a013a, 0xf34ef753,
    0xfc51f363, 0xf351fa26, 0xf33efa3a, 0xfe3bf049, 0xf64cf356, 0xf753f657,
    0x0000ea7f, 0xe77fe778, 0xe57fed72, 0xe975e776, 0xe675e871, 0xe476e178,
    0xdb7cf65e, 0xf166f663, 0xf36ace7f, 0xfb5c1139, 0xfb56f35e, 0xf45bfe4d,
    0x0047ff49, 0x0440f951, 0x05400f39, 0x01430044, 0xf6430144, 0x004d0240,
    0x0044fb4e, 0x0737053b, 0x02410e36, 0x0f2c053c, 0x0246fe4c, 0xee560c46,
    0x0540f446, 0x0b370538, 0x00450241, 0xfa4a0536, 0x0736fa4c, 0xf552fe4d,
    0xfe4d192a, 0x11f310f7, 0x11f41beb, 0x25e229d8, 0x2ad730d1, 0x27e02ed8,
    0x34cd2ed7, 0x34d92bed, 0x200b3dc9, 0x38d23ece, 0x51bd2dec, 0x23fe1c0f,
    0x22012701, 0x1e111426, 0x122d0f36, 0x004f24f0, 0x25f225ef, 0x2001220f,
    0x1d0f1819, 0x22161f10, 0x23121f1c, 0x2129241c, 0x1b2f153e, 0x121f131a,
    0x24181817, 0x1b10181e, 0x1f1d1629, 0x162a103c, 0x0f340e3c, 0x034ef07b,
    0x15351638, 0x193d1521, 0x1332113d, 0xfd4ef84a, 0xf748f648, 0xee4bf447,
    0xf53ffb46, 0xef4bf248, 0xf043f835, 0xf23bf734, 0xf54409fe, 0x1ef61ffc,
    0x21ff2107, 0x1f0c2517, 0x1f261440, 0xf747f925, 0xf82cf531, 0xf638f43b,
    0xf83ff743, 0xfa44f64f, 0xfd4ef84a, 0xf748f648, 0xee4bf447, 0xf53ffb46,
    0xef4bf248, 0xf043f835, 0xf23bf734, 0xf54409fe, 0x1ef61ffc, 0x21ff2107,
    0x1f0c2517, 0x1f261440
};

/*------------------------------------------------------------------------------
    Function name : AllocateAsicBuffers
    Description   :

    Return type   : i32
    Argument      : DecAsicBuffers_t * asicBuff
    Argument      : u32 mbs
------------------------------------------------------------------------------*/
u32 AllocateAsicBuffers(decContainer_t * pDecCont, DecAsicBuffers_t * asicBuff,
                        u32 mbs)
{
    i32 ret = 0;
    u32 tmp;

    if(pDecCont->h264ProfileSupport != H264_BASELINE_PROFILE)
    {
        tmp = ASIC_CABAC_INIT_BUFFER_SIZE + ASIC_SCALING_LIST_SIZE +
            ASIC_POC_BUFFER_SIZE;
        ret |= VPUMallocLinear(&asicBuff[0].cabacInit, tmp);
        if(ret == 0)
        {
            H264PrepareCabacInitTables(asicBuff);
        }
    }

    if(pDecCont->refBufSupport)
    {
        RefbuInit(&pDecCont->refBufferCtrl, DEC_X170_MODE_H264,
                  pDecCont->storage.activeSps->picWidthInMbs,
                  pDecCont->storage.activeSps->picHeightInMbs,
                  pDecCont->refBufSupport);
    }

    if(ret != 0)
        return 1;
    else
        return 0;
}

/*------------------------------------------------------------------------------
    Function name : ReleaseAsicBuffers
    Description   :

    Return type   : void
    Argument      : DecAsicBuffers_t * asicBuff
------------------------------------------------------------------------------*/
void ReleaseAsicBuffers(DecAsicBuffers_t * asicBuff)
{
    if(asicBuff[0].cabacInit.vir_addr != NULL)
    {
        VPUFreeLinear(&asicBuff[0].cabacInit);
        asicBuff[0].cabacInit.vir_addr = NULL;
    }
}

/*------------------------------------------------------------------------------
    Function name : H264RunAsic
    Description   :

    Return type   : hw status
    Argument      : DecAsicBuffers_t * pAsicBuff
------------------------------------------------------------------------------*/
u32 H264RunAsic(decContainer_t * pDecCont, DecAsicBuffers_t * pAsicBuff)
{
    const seqParamSet_t *pSps = pDecCont->storage.activeSps;
    const sliceHeader_t *pSliceHeader = pDecCont->storage.sliceHeader;
    const picParamSet_t *pPps = pDecCont->storage.activePps;
    const dpbStorage_t *dpb = pDecCont->storage.dpb;
	u32 h264regtmp[DEC_X170_REGISTERS];
    u32 asic_status = 0;
    i32 ret = 0;
    u32 i;

	for(i = 0; i < 16; i++)
	{
		if((i < dpb->dpbSize) && pAsicBuff->refPicList[i])
			SetDecRegister(pDecCont->h264Regs, refBase[i], pAsicBuff->refPicList[i]);
		else
			SetDecRegister(pDecCont->h264Regs, refBase[i], pAsicBuff->outPhyAddr);
	}

	
    /* inter-view reference picture */
    if (pDecCont->storage.view && pDecCont->storage.interViewRef)
    {
        SetDecRegister(pDecCont->h264Regs, HWIF_INTER_VIEW_BASE,
            pDecCont->storage.dpbs[0]->currentPhyAddr);
        SetDecRegister(pDecCont->h264Regs, HWIF_REFER_VALID_E,
            GetDecRegister(pDecCont->h264Regs, HWIF_REFER_VALID_E) |
            (pSliceHeader->fieldPicFlag ? 0x3 : 0x10000) );
    }
    SetDecRegister(pDecCont->h264Regs, HWIF_MVC_E, pDecCont->storage.view != 0);

    SetDecRegister(pDecCont->h264Regs, HWIF_FILTERING_DIS, pAsicBuff->filterDisable);

    if(pSliceHeader->fieldPicFlag && pSliceHeader->bottomFieldFlag)
    {
        SetDecRegister(pDecCont->h264Regs, HWIF_DEC_OUT_BASE,
                       pAsicBuff->outPhyAddr + pSps->picWidthInMbs * 16);
    }
    else
    {
        SetDecRegister(pDecCont->h264Regs, HWIF_DEC_OUT_BASE,
                       pAsicBuff->outPhyAddr);
    }

    SetDecRegister(pDecCont->h264Regs, HWIF_CH_QP_OFFSET,
                   pAsicBuff->chromaQpIndexOffset);
    SetDecRegister(pDecCont->h264Regs, HWIF_CH_QP_OFFSET2,
                   pAsicBuff->chromaQpIndexOffset2);

    {
        /* new 8190 stuff (not used/ignored in 8170) */
        if(pDecCont->h264ProfileSupport == H264_BASELINE_PROFILE)
        {
            goto skipped_high_profile;  /* leave high profile stuff disabled */
        }

        /* direct mv writing not enabled if HW config says that
         * SW_DEC_H264_PROF="10" */
        if(pAsicBuff->enableDmvAndPoc && pDecCont->h264ProfileSupport != 2)
        {
            u32 dirMvOffset;

            SetDecRegister(pDecCont->h264Regs, HWIF_WRITE_MVS_E,
                           pDecCont->storage.prevNalUnit->nalRefIdc != 0);

            dirMvOffset = dpb->dirMvOffset +
                (pSliceHeader->bottomFieldFlag ? (dpb->picSizeInMbs * 32) : 0);

            /*
            ASSERT(pAsicBuff->outBuffer->busAddress ==
                   dpb->buffer[dpb->currentOutPos].data->busAddress);
                   */

            SetDecRegister(pDecCont->h264Regs, HWIF_DIR_MV_BASE,
                           pAsicBuff->outPhyAddr + dirMvOffset);
        }

        SetDecRegister(pDecCont->h264Regs, HWIF_DIR_8X8_INFER_E,
                       pSps->direct8x8InferenceFlag);
        SetDecRegister(pDecCont->h264Regs, HWIF_WEIGHT_PRED_E,
                       pPps->weightedPredFlag);
        SetDecRegister(pDecCont->h264Regs, HWIF_WEIGHT_BIPR_IDC,
                       pPps->weightedBiPredIdc);
        SetDecRegister(pDecCont->h264Regs, HWIF_REFIDX1_ACTIVE,
                       pPps->numRefIdxL1Active);
        SetDecRegister(pDecCont->h264Regs, HWIF_FIELDPIC_FLAG_E,
                       !pSps->frameMbsOnlyFlag);
        SetDecRegister(pDecCont->h264Regs, HWIF_PIC_INTERLACE_E,
                       !pSps->frameMbsOnlyFlag &&
                       (pSps->mbAdaptiveFrameFieldFlag || pSliceHeader->fieldPicFlag));

        SetDecRegister(pDecCont->h264Regs, HWIF_PIC_FIELDMODE_E,
                       pSliceHeader->fieldPicFlag);

			DEBUG_PRINT(("framembs %d, mbaff %d, fieldpic %d\n",
               pSps->frameMbsOnlyFlag,
               pSps->mbAdaptiveFrameFieldFlag, pSliceHeader->fieldPicFlag));

        SetDecRegister(pDecCont->h264Regs, HWIF_PIC_TOPFIELD_E,
                       !pSliceHeader->bottomFieldFlag);

        SetDecRegister(pDecCont->h264Regs, HWIF_SEQ_MBAFF_E,
                       pSps->mbAdaptiveFrameFieldFlag);

        SetDecRegister(pDecCont->h264Regs, HWIF_8X8TRANS_FLAG_E,
                       pPps->transform8x8Flag);

        SetDecRegister(pDecCont->h264Regs, HWIF_BLACKWHITE_E,
                       pSps->profileIdc >= 100 &&
                       pSps->chromaFormatIdc == 0);

        SetDecRegister(pDecCont->h264Regs, HWIF_TYPE1_QUANT_E,
                       pPps->scalingMatrixPresentFlag);
        if(pPps->scalingMatrixPresentFlag)
        {
            u32 j, tmp;
            u32 *p;

            u8(*scalingList)[64];

            p = (u32 *) ((u8 *) pDecCont->asicBuff->cabacInit.vir_addr + ASIC_CABAC_INIT_BUFFER_SIZE +
                         ASIC_POC_BUFFER_SIZE);
            scalingList = pDecCont->storage.activePps->scalingList;
            for(i = 0; i < 6; i++)
            {
                for(j = 0; j < 4; j++)
                {
                    tmp = (scalingList[i][4 * j + 0] << 24) |
                        (scalingList[i][4 * j + 1] << 16) |
                        (scalingList[i][4 * j + 2] << 8) |
                        (scalingList[i][4 * j + 3]);
                    *p++ = tmp;
                }
            }
            for(i = 6; i < 8; i++)
            {
                for(j = 0; j < 16; j++)
                {
                    tmp = (scalingList[i][4 * j + 0] << 24) |
                        (scalingList[i][4 * j + 1] << 16) |
                        (scalingList[i][4 * j + 2] << 8) |
                        (scalingList[i][4 * j + 3]);
                    *p++ = tmp;
                }
            }
            VPUMemClean(&pDecCont->asicBuff->cabacInit);
        }
    }

/***************************************************************************/
    skipped_high_profile:

    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_OUT_DIS, 0);

    /* Setup reference picture buffer */
    if(pDecCont->refBufSupport)
    {
        u32 fieldPicFlag;
        u32 mbaff;
        u32 picId0;
        u32 picId1;
        u32 flags = 0;
        const picParamSet_t *pPps = pDecCont->storage.activePps;
        /* TODO in this */
        fieldPicFlag = pSliceHeader->fieldPicFlag;
        mbaff = pDecCont->storage.activeSps->mbAdaptiveFrameFieldFlag;
        //picId0 = pDecCont->storage.dpb[0].list[0];
        //picId1 = pDecCont->storage.dpb[0].list[1];
        picId0 = pDecCont->storage.dpb[0].refList[0];
        picId1 = pDecCont->storage.dpb[0].refList[1];
        if( pPps->numRefIdxL0Active > 1 ||
            ( IS_B_SLICE(pDecCont->storage.sliceHeader->sliceType) &&
              pPps->numRefIdxL0Active > 1 ) )
        {
            flags |= REFBU_MULTIPLE_REF_FRAMES;
        }

        RefbuSetup(&pDecCont->refBufferCtrl, pDecCont->h264Regs,
                   fieldPicFlag ? REFBU_FIELD : (mbaff ? REFBU_MBAFF : REFBU_FRAME),
                   IS_IDR_NAL_UNIT(pDecCont->storage.prevNalUnit),
                   IS_B_SLICE(pDecCont->storage.sliceHeader->sliceType),
                   picId0, picId1, flags );
    }

    SetDecRegister(pDecCont->h264Regs, HWIF_CH_8PIX_ILEAV_E, 0);
    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_E, 1);
	DPBDEBUG("before pDecCont->h264Regs[12]=0x%x\n", pDecCont->h264Regs[12]);
		
	if(pSliceHeader->fieldPicFlag)
		pDecCont->h264hwdecLen[pSliceHeader->bottomFieldFlag] = pDecCont->h264Regs[12];
	else
		pDecCont->h264hwdecLen[0] = pDecCont->h264Regs[12];
	if(pSliceHeader->fieldPicFlag)
		pDecCont->h264hwdecLen[pSliceHeader->bottomFieldFlag] = pDecCont->h264Regs[12];
	else
		pDecCont->h264hwdecLen[0] = pDecCont->h264Regs[12];
	memcpy(h264regtmp, pDecCont->h264Regs, sizeof(h264regtmp));
	
	for(i=0;i<2;i++)	//for ts
	{
		H264FlushRegs(pDecCont);

	    {
		VPU_CMD_TYPE cmd;	
	    i32 len;

	    ret = VPUClientWaitResult(pDecCont->socket, pDecCont->h264Regs, DEC_X170_REGISTERS, &cmd, &len);
	    VPU_DEBUG("VPUClientWaitResult: ret %d, cmd %d, len %d\n", ret, cmd, len);
		if(pSliceHeader->fieldPicFlag){
			pDecCont->h264hwdecLen[pSliceHeader->bottomFieldFlag] = pDecCont->h264Regs[12] - pDecCont->h264hwdecLen[pSliceHeader->bottomFieldFlag];
			pDecCont->h264hwStatus[pSliceHeader->bottomFieldFlag] = pDecCont->h264Regs[1];
			pDecCont->h264hwFrameLen[pSliceHeader->bottomFieldFlag] = pDecCont->hwLength;
		}else{
			pDecCont->h264hwdecLen[0] = pDecCont->h264Regs[12] - pDecCont->h264hwdecLen[0];
			pDecCont->h264hwStatus[0] = pDecCont->h264Regs[1];		
			pDecCont->h264hwFrameLen[0] = pDecCont->hwLength;		
		}	
		if ((VPU_SUCCESS != ret) || (cmd != VPU_SEND_CONFIG_ACK_OK)) {
		        ALOGE("VPUClientWaitResult ret %d\n", ret);
		        return X170_DEC_SYSTEM_ERROR;
	    }
    	}

		if(!pDecCont->ts_en)
			break;
		if((!(pDecCont->h264Regs[1]&0x1000)) && (i==0))
		{
			u32 picwidth = pSps->picWidthInMbs*16;
			u32 picheight = pSps->picHeightInMbs*16;
			u32 j;
			if(dpb->currentOut->data->mem.phy_addr != pAsicBuff->outPhyAddr)
				ALOGE("MEMORY IS ERROR!");

			if(pSliceHeader->fieldPicFlag)
			{
				u32 *p = (u32 *)(pAsicBuff->outVirAddr+pSliceHeader->bottomFieldFlag*picwidth);
				for(j=0;j<picheight;j+=2)
					p[j*picwidth/4] = 0x66339988;
				VPUMemClean(&dpb->currentOut->data->mem);	
			}
			else
			{
				u32 *p = (u32 *)pAsicBuff->outVirAddr;
				for(j=0;j<picheight;j++)
					p[j*picwidth/4] = 0x66339988;
				VPUMemClean(&dpb->currentOut->data->mem);
			}
			memcpy(pDecCont->h264Regs, h264regtmp, sizeof(h264regtmp));
		}else
		{
			break;
		}
	}
	
	DPBDEBUG("pDecCont->h264Regs[1]=0x%x\n", pDecCont->h264Regs[1]);
	DPBDEBUG("pDecCont->h264Regs[12]=0x%x\n", pDecCont->h264Regs[12]);
	SetDecRegister(pDecCont->h264Regs, HWIF_DEC_IRQ_STAT, 0);
	SetDecRegister(pDecCont->h264Regs, HWIF_DEC_IRQ, 0);

    /* React to the HW return value */
    //if decode success
//    pDispoutbuff->pDispbuffer->Isdisplay = 1;

    asic_status = GetDecRegister(pDecCont->h264Regs, HWIF_DEC_IRQ_STAT);

    /* any B stuff detected by HW? (note reg name changed from
     * b_detected to pic_inf) */
    if(GetDecRegister(pDecCont->h264Regs, HWIF_DEC_PIC_INF))
    {
        ASSERT(pDecCont->h264ProfileSupport != H264_BASELINE_PROFILE);
        if((pDecCont->h264ProfileSupport != H264_BASELINE_PROFILE) &&
           (pAsicBuff->enableDmvAndPoc == 0))
        {
            DEBUG_PRINT(("HW Detected B slice in baseline profile stream!!!\n"));
            pAsicBuff->enableDmvAndPoc = 1;
        }

        SetDecRegister(pDecCont->h264Regs, HWIF_DEC_PIC_INF, 0);
    }

    /* update reference buffer stat */
    if (asic_status & DEC_X170_IRQ_DEC_RDY)
    {
        u32 *pMv = (u32 *)pAsicBuff->outVirAddr;
        u32 tmp = dpb->dirMvOffset;

        if(pSliceHeader->fieldPicFlag && pSliceHeader->bottomFieldFlag)
        {
             tmp += dpb->picSizeInMbs * 32;
        }
        pMv = (u32 *) ((u8*)pMv + tmp);

        if(pDecCont->refBufSupport &&
           !IS_B_SLICE(pDecCont->storage.sliceHeader->sliceType))
        {
            RefbuMvStatistics(&pDecCont->refBufferCtrl, pDecCont->h264Regs,
                              pMv, pAsicBuff->enableDmvAndPoc,
                              IS_IDR_NAL_UNIT(pDecCont->storage.prevNalUnit));
        }
        else if ( pDecCont->refBufSupport )
        {
            RefbuMvStatisticsB( &pDecCont->refBufferCtrl, pDecCont->h264Regs );
        }
    }

    {
        u32 last_read_address;
        u32 bytes_processed;
        const u32 start_address = pDecCont->hwStreamStartBus & (~DEC_X170_ALIGN_MASK);
        const u32 offset_bytes = pDecCont->hwStreamStartBus & DEC_X170_ALIGN_MASK;

        last_read_address =
            GetDecRegister(pDecCont->h264Regs, HWIF_RLC_VLC_BASE);

        bytes_processed = last_read_address - start_address;

        DEBUG_PRINT(("HW updated stream position: %08x\n"
                     "           processed bytes: %8d\n"
                     "     of which offset bytes: %8d\n",
                     last_read_address, bytes_processed, offset_bytes));

        if(!(asic_status & DEC_X170_IRQ_ASO))
        {
            /* from start of the buffer add what HW has decoded */

            /* end - start smaller or equal than maximum */
            if((bytes_processed - offset_bytes) > pDecCont->hwLength)
            {

                if(!(asic_status & DEC_X170_IRQ_STREAM_ERROR))
                {
                    DEBUG_PRINT(("New stream position out of range!\n"));
                    ASSERT(0);
                }

                /* consider all buffer processed */
                pDecCont->pHwStreamStart += pDecCont->hwLength;
                pDecCont->hwStreamStartBus += pDecCont->hwLength;
                pDecCont->hwLength = 0; /* no bytes left */
            }
            else
            {
                pDecCont->hwLength -= (bytes_processed - offset_bytes);
                pDecCont->pHwStreamStart += (bytes_processed - offset_bytes);
                pDecCont->hwStreamStartBus += (bytes_processed - offset_bytes);
            }
			if(pSliceHeader->fieldPicFlag){
				if(!(pDecCont->h264hwStatus[pSliceHeader->bottomFieldFlag]&0x1000))
					pDecCont->hwLength = 0;
			}else{
				if(!(pDecCont->h264hwStatus[0]&0x1000))
					pDecCont->hwLength = 0;
			}	
        }
        /* else will continue decoding from the beginning of buffer */

        pDecCont->streamPosUpdated = 1;
    }

    return asic_status;
}

/*------------------------------------------------------------------------------
    Function name   : H264FlushRegs
    Description     :
    Return type     : void
    Argument        : decContainer_t * pDecCont
------------------------------------------------------------------------------*/
static void H264FlushRegs(decContainer_t * pDecCont)
{
    VPU_DEBUG("H264FlushRegs\n");

    if (VPUClientSendReg(pDecCont->socket, pDecCont->h264Regs, DEC_X170_REGISTERS)) {
        VPU_DEBUG("H264FlushRegs fail\n");
    } else {
        VPU_DEBUG("H264FlushRegs success\n");
    }
}

/*------------------------------------------------------------------------------
    Function name : H264SetupVlcRegs
    Description   : set up registers for vlc mode

    Return type   :
    Argument      : container
------------------------------------------------------------------------------*/
void H264SetupVlcRegs(decContainer_t * pDecCont)
{
    u32 tmp, i;
    u32 longTermflags = 0;
    u32 validFlags = 0;
    u32 longTermTmp = 0;
    i32 diffPoc, currPoc, itmp;

    const seqParamSet_t *pSps = pDecCont->storage.activeSps;
    const sliceHeader_t *pSliceHeader = pDecCont->storage.sliceHeader;
    const picParamSet_t *pPps = pDecCont->storage.activePps;
    const dpbStorage_t *pDpb = pDecCont->storage.dpb;
    const storage_t *pStorage = &pDecCont->storage;

    SetDecRegister(pDecCont->h264Regs, HWIF_DEC_OUT_DIS, 0);

    SetDecRegister(pDecCont->h264Regs, HWIF_RLC_MODE_E, 0);

    SetDecRegister(pDecCont->h264Regs, HWIF_INIT_QP, pPps->picInitQp);

    SetDecRegister(pDecCont->h264Regs, HWIF_REFIDX0_ACTIVE,
                   pPps->numRefIdxL0Active);

    SetDecRegister(pDecCont->h264Regs, HWIF_REF_FRAMES, pSps->numRefFrames);

    i = 0;
    while(pSps->maxFrameNum >> i)
    {
        i++;
    }
    SetDecRegister(pDecCont->h264Regs, HWIF_FRAMENUM_LEN, i - 1);

    SetDecRegister(pDecCont->h264Regs, HWIF_FRAMENUM, pSliceHeader->frameNum);

    SetDecRegister(pDecCont->h264Regs, HWIF_CONST_INTRA_E,
                   pPps->constrainedIntraPredFlag);

    SetDecRegister(pDecCont->h264Regs, HWIF_FILT_CTRL_PRES,
                   pPps->deblockingFilterControlPresentFlag);

    SetDecRegister(pDecCont->h264Regs, HWIF_RDPIC_CNT_PRES,
                   pPps->redundantPicCntPresentFlag);

    SetDecRegister(pDecCont->h264Regs, HWIF_REFPIC_MK_LEN,
                   pSliceHeader->decRefPicMarking.strmLen);

    SetDecRegister(pDecCont->h264Regs, HWIF_IDR_PIC_E,
                   IS_IDR_NAL_UNIT(pStorage->prevNalUnit));
    SetDecRegister(pDecCont->h264Regs, HWIF_IDR_PIC_ID, pSliceHeader->idrPicId);
    SetDecRegister(pDecCont->h264Regs, HWIF_PPS_ID, pStorage->activePpsId);
    SetDecRegister(pDecCont->h264Regs, HWIF_POC_LENGTH,
                   pSliceHeader->pocLengthHw);

    /* reference picture flags */

    /* TODO separate fields */
    if(pSliceHeader->fieldPicFlag)
    {
        ASSERT(pDecCont->h264ProfileSupport != H264_BASELINE_PROFILE);

        for(i = 0; i < 32; i++)
        {
            if (!pDpb->buffer[i / 2]) {
                longTermflags <<= 1;
                validFlags <<= 1;
            } else {
                longTermTmp = pDpb->buffer[i / 2]->status[i & 1] == 3;
                longTermflags = longTermflags << 1 | longTermTmp;

                tmp = h264bsdGetRefPicDataVlcMode(pDpb, i, 1) != NULL;
                validFlags = validFlags << 1 | tmp;
            }
        }
        SetDecRegister(pDecCont->h264Regs, HWIF_REFER_LTERM_E, longTermflags);
        SetDecRegister(pDecCont->h264Regs, HWIF_REFER_VALID_E, validFlags);
    }
    else
    {
        for(i = 0; i < 16; i++)
        {
            if (!pDpb->buffer[i]) {
                longTermflags <<= 1;
                validFlags <<= 1;
            } else {
                longTermTmp = pDpb->buffer[i]->status[0] == LONG_TERM &&
                              pDpb->buffer[i]->status[1] == LONG_TERM;
                longTermflags = longTermflags << 1 | longTermTmp;

                tmp = h264bsdGetRefPicDataVlcMode(pDpb, i, 0) != NULL;
                validFlags = validFlags << 1 | tmp;
            }
        }
        SetDecRegister(pDecCont->h264Regs, HWIF_REFER_LTERM_E,
                       longTermflags << 16);
        SetDecRegister(pDecCont->h264Regs, HWIF_REFER_VALID_E,
                       validFlags << 16);
    }

    if(pSliceHeader->fieldPicFlag)
    {
        currPoc = pStorage->poc->picOrderCnt[pSliceHeader->bottomFieldFlag];
    }
    else
    {
        currPoc = MIN(pStorage->poc->picOrderCnt[0],
                      pStorage->poc->picOrderCnt[1]);
    }
    for(i = 0; i < 16; i++)
    {
        if (pDpb->buffer[i]) {
            if(pDpb->buffer[i]->status[0] == 3 || pDpb->buffer[i]->status[1] == 3) {
                SetDecRegister(pDecCont->h264Regs, refPicNum[i],
                               pDpb->buffer[i]->picNum);
            } else {
                SetDecRegister(pDecCont->h264Regs, refPicNum[i],
                               pDpb->buffer[i]->frameNum);
            }
            diffPoc = ABS(pDpb->buffer[i]->picOrderCnt[0] - currPoc);
            itmp    = ABS(pDpb->buffer[i]->picOrderCnt[1] - currPoc);

            pDecCont->asicBuff->refPicList[i] |=
                (diffPoc < itmp ? 0x1 : 0) | (pDpb->buffer[i]->isFieldPic ? 0x2 : 0);
        }
    }

    if(pDecCont->h264ProfileSupport != H264_BASELINE_PROFILE)
    {
        if(pDecCont->asicBuff->enableDmvAndPoc)
        {
            u32 *pocBase;

            SetDecRegister(pDecCont->h264Regs, HWIF_PICORD_COUNT_E, 1);
            pocBase = (u32 *) ((u8 *) pDecCont->asicBuff->cabacInit.vir_addr +
                         ASIC_CABAC_INIT_BUFFER_SIZE);
            for(i = 0; i < 32; i++)
            {
                if (pDpb->buffer[i / 2]) {
                    *pocBase++ = pDpb->buffer[i / 2]->picOrderCnt[i & 0x1];
                } else {
                    *pocBase++ = 0;
                }
            }

            if(pSliceHeader[0].fieldPicFlag || !pSps->mbAdaptiveFrameFieldFlag)
            {
                *pocBase++ = currPoc;
            }
            else
            {
                *pocBase++ = pStorage->poc->picOrderCnt[0];
                *pocBase++ = pStorage->poc->picOrderCnt[1];
            }
        }
        else
        {
            SetDecRegister(pDecCont->h264Regs, HWIF_PICORD_COUNT_E, 0);
        }

        SetDecRegister(pDecCont->h264Regs, HWIF_CABAC_E,
                       pPps->entropyCodingModeFlag);
        VPUMemClean(&pDecCont->asicBuff->cabacInit);
    }

    h264StreamPosUpdate(pDecCont);
}

/*------------------------------------------------------------------------------
    Function name   : H264InitRefPicList1
    Description     :
    Return type     : void
    Argument        : decContainer_t *pDecCont
    Argument        : u32 *list0
    Argument        : u32 *list1
------------------------------------------------------------------------------*/
void H264InitRefPicList1(decContainer_t * pDecCont, u32 * list0, u32 * list1)
{
    u32 tmp, i;
    u32 idx, idx0, idx1, idx2;
    i32 refPoc;
    storage_t *pStorage = &pDecCont->storage;
    dpbPicture_t **pic;

    refPoc = MIN(pStorage->poc->picOrderCnt[0], pStorage->poc->picOrderCnt[1]);
    i = 0;

    pic = pStorage->dpb->buffer;
    while(pic[list0[i]] && IS_SHORT_TERM_FRAME(pic[list0[i]]) &&
          MIN(pic[list0[i]]->picOrderCnt[0], pic[list0[i]]->picOrderCnt[1]) <
          refPoc)
        i++;

    idx0 = i;

    while(pic[list0[i]] && IS_SHORT_TERM_FRAME(pic[list0[i]]))
        i++;

    idx1 = i;

    while(pic[list0[i]] && IS_LONG_TERM_FRAME(pic[list0[i]]))
        i++;

    idx2 = i;

    /* list L1 */
    for(i = idx0, idx = 0; i < idx1; i++, idx++)
        list1[idx] = list0[i];

    for(i = 0; i < idx0; i++, idx++)
        list1[idx] = list0[i];

    for(i = idx1; idx < 16; idx++, i++)
        list1[idx] = list0[i];

    if(idx2 > 1)
    {
        tmp = 0;
        for(i = 0; i < idx2; i++)
        {
            tmp += (list0[i] != list1[i]) ? 1 : 0;
        }
        /* lists are equal -> switch list1[0] and list1[1] */
        if(!tmp)
        {
            i = list1[0];
            list1[0] = list1[1];
            list1[1] = i;
        }
    }

}

/*------------------------------------------------------------------------------
    Function name   : H264InitRefPicList1F
    Description     :
    Return type     : void
    Argument        : decContainer_t *pDecCont
    Argument        : u32 *list0
    Argument        : u32 *list1
------------------------------------------------------------------------------*/
void H264InitRefPicList1F(decContainer_t * pDecCont, u32 * list0, u32 * list1)
{
    u32 i;
    u32 idx, idx0, idx1;
    i32 refPoc;
    storage_t *pStorage = &pDecCont->storage;
    dpbPicture_t **pic;

    refPoc =
        pStorage->poc->picOrderCnt[pStorage->sliceHeader[0].bottomFieldFlag];
    i = 0;

    pic = pStorage->dpb->buffer;
    while(pic[list0[i]] && IS_SHORT_TERM_FRAME_F(pic[list0[i]]) &&
          FIELD_POC(pic[list0[i]]) <= refPoc)
        i++;

    idx0 = i;

    while(pic[list0[i]] && IS_SHORT_TERM_FRAME_F(pic[list0[i]]))
        i++;

    idx1 = i;

    /* list L1 */
    for(i = idx0, idx = 0; i < idx1; i++, idx++)
        list1[idx] = list0[i];

    for(i = 0; i < idx0; i++, idx++)
        list1[idx] = list0[i];

    for(i = idx1; idx < 16; idx++, i++)
        list1[idx] = list0[i];

}

/*------------------------------------------------------------------------------
    Function name   : H264InitRefPicList
    Description     :
    Return type     : void
    Argument        : decContainer_t *pDecCont
------------------------------------------------------------------------------*/
void H264InitRefPicList(decContainer_t * pDecCont)
{
    sliceHeader_t *pSliceHeader = pDecCont->storage.sliceHeader;
    pocStorage_t *poc = pDecCont->storage.poc;
    dpbStorage_t *dpb = pDecCont->storage.dpb;
    u32 i;
    u32 list0[34] =
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33
    };
    u32 list1[34] =
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33
    };
    u32 listP[34] =
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33
    };

    /* B lists */
    if(pSliceHeader->fieldPicFlag)
    {
        /* list 0 */
        ShellSortF(dpb, list0, 1,
                   poc->picOrderCnt[pSliceHeader->bottomFieldFlag]);
        if (pDecCont->storage.view && pDecCont->storage.interViewRef)
        {
            i = 0;
            while (IS_REF_FRAME_F(dpb->buffer[list0[i]])) i++;
            list0[i] = 15;
        }
        /* list 1 */
        H264InitRefPicList1F(pDecCont, list0, list1);
        for(i = 0; i < 16; i++)
        {
            SetDecRegister(pDecCont->h264Regs, refPicList0[i], list0[i]);
            SetDecRegister(pDecCont->h264Regs, refPicList1[i], list1[i]);
        }
    }
    else
    {
        /* list 0 */
        ShellSort(dpb, list0, 1,
                  MIN(poc->picOrderCnt[0], poc->picOrderCnt[1]));
        if (pDecCont->storage.view && pDecCont->storage.interViewRef)
        {
            i = 0;
            while (IS_REF_FRAME(dpb->buffer[list0[i]])) i++;
            list0[i] = 15;
        }
        /* list 1 */
        H264InitRefPicList1(pDecCont, list0, list1);
        for(i = 0; i < 16; i++)
        {
            SetDecRegister(pDecCont->h264Regs, refPicList0[i], list0[i]);
            SetDecRegister(pDecCont->h264Regs, refPicList1[i], list1[i]);
        }
    }

    /* P list */
    if(pSliceHeader->fieldPicFlag)
    {
        ShellSortF(dpb, listP, 0, 0);
        if (pDecCont->storage.view && pDecCont->storage.interViewRef)
        {
            i = 0;
            while (IS_REF_FRAME_F(dpb->buffer[listP[i]])) i++;
            listP[i] = 15;
        }
        for(i = 0; i < 16; i++)
        {
            SetDecRegister(pDecCont->h264Regs, refPicListP[i], listP[i]);

            /* copy to dpb for error handling purposes */
            //dpb[0].list[i] = listP[i];
            //dpb[1].list[i] = listP[i];
            if (i < 2) {
                dpb[0].refList[i] = dpb[1].refList[i] = listP[i];
            }
        }
    }
    else
    {
        ShellSort(dpb, listP, 0, 0);
        if (pDecCont->storage.view && pDecCont->storage.interViewRef)
        {
            i = 0;
            while (IS_REF_FRAME(dpb->buffer[listP[i]])) i++;
            listP[i] = 15;
        }
        for(i = 0; i < 16; i++)
        {
            SetDecRegister(pDecCont->h264Regs, refPicListP[i], listP[i]);
            /* copy to dpb for error handling purposes */
            //dpb[0].list[i] = listP[i];
            //dpb[1].list[i] = listP[i];
            if (i < 2) {
                dpb[0].refList[i] = dpb[1].refList[i] = listP[i];
            }
        }
    }
}

/*------------------------------------------------------------------------------
    Function name : h264StreamPosUpdate
    Description   : Set stream phy_base and length related registers

    Return type   :
    Argument      : container
------------------------------------------------------------------------------*/
void h264StreamPosUpdate(decContainer_t * pDecCont)
{
    u32 tmp;

    DEBUG_PRINT(("h264StreamPosUpdate:\n"));
    tmp = 0;

    /* NAL start prefix in stream start is 0 0 0 or 0 0 1 */
    if(!(*pDecCont->pHwStreamStart + *(pDecCont->pHwStreamStart + 1)))
    {
        if(*(pDecCont->pHwStreamStart + 2) < 2)
        {
            tmp = 1;
        }
    }

    DEBUG_PRINT(("\tByte stream   %8d\n", tmp));
    SetDecRegister(pDecCont->h264Regs, HWIF_START_CODE_E, tmp);

    /* bit offset if phy_base is unaligned */
    tmp = (pDecCont->hwStreamStartBus & DEC_X170_ALIGN_MASK) * 8;

    DEBUG_PRINT(("\tStart bit pos %8d\n", tmp));

    SetDecRegister(pDecCont->h264Regs, HWIF_STRM_START_BIT, tmp);

    pDecCont->hwBitPos = tmp;

    tmp = pDecCont->hwStreamStartBus;   /* unaligned phy_base */
    tmp &= (~DEC_X170_ALIGN_MASK);  /* align the phy_base */

    DEBUG_PRINT(("\tStream phy_base   %08x\n", tmp));
    SetDecRegister(pDecCont->h264Regs, HWIF_RLC_VLC_BASE, tmp);

    tmp = pDecCont->hwLength;   /* unaligned stream */
    tmp += pDecCont->hwBitPos / 8;  /* add the alignmet bytes */

    DEBUG_PRINT(("\tStream length %8d\n", tmp));
    SetDecRegister(pDecCont->h264Regs, HWIF_STREAM_LEN, tmp);

}

/*------------------------------------------------------------------------------
    Function name : H264PrepareCabacInitTables
    Description   : Prepare CABAC initialization tables

    Return type   : void
    Argument      : DecAsicBuffers_t * pAsicBuff
------------------------------------------------------------------------------*/
void H264PrepareCabacInitTables(DecAsicBuffers_t * pAsicBuff)
{
    ASSERT(pAsicBuff->cabacInit.vir_addr != NULL);
    memcpy(pAsicBuff->cabacInit.vir_addr, cabacInitValues, 4 * 460 * 2);
    VPUMemClean(&pAsicBuff->cabacInit);
}


void DispbuffMalloc(u32 width, u32 height)
{
    u32 i,size;
//    VPUMemLinear_t info;

    /*for(i=0;i<3;i++)
    {
        size = width*height*3/2;
        memset(&info, 0, sizeof(info));
        VPUMallocLinear(&info,size);
        DisplayBuff[i].size = info.size;
        DisplayBuff[i].vir_addr = info.vir_addr;
        DisplayBuff[i].phy_addr = info.phy_addr;
        DisplayBuff[i].width = width;
        DisplayBuff[i].height = height;
        DisplayBuff[i].Isdisplay = 0;
    }*/

    size = width*height*3/2;
    memset(&DisplayBuff, 0, sizeof(DisplayBuff));
    i = VPUMallocLinear(&DisplayBuff,size);
    if(i)
    {
        ALOGE("DisplayBuff VPUMallocLinear fail \n");
    }
}

void DispbuffFree(void)
{
    u32 i;
    VPUMemLinear_t info;

    /*for(i=0;i<3;i++)
    {
        info.vir_addr = DisplayBuff[i].vir_addr;
        info.phy_addr = DisplayBuff[i].phy_addr;
        info.size = DisplayBuff[i].size ;
        VPUFreeLinear(&info);
        DisplayBuff[i].Isdisplay = 0;
    }*/

    VPUFreeLinear(&DisplayBuff);
}

DispLinearMem *GetDispbuff(void)
{
    u32 i;

    return NULL;//&DisplayBuff;

    /*for(i=0;i<3;i++)
    {
       if(DisplayBuff[i].Isdisplay == 0)
            return &DisplayBuff[i];
    }

    printf("Malloc dispbuff failure!");
    while(1);
    return NULL;    //*/
}


void pipelineconfig(u32 *h264Regs, u32 memorydispaddr,u32 inwidth, u32 inheight, u32 outwidth, u32 outheight, ppOutFormat mode)//, u32 width, u32 height)
{
    u32 *ppRegs = h264Regs + 60;//DEC_X170_REGISTERS;

    memset(ppRegs, 0, 41*4);

    switch(inwidth)
    {
        case 160:
            inheight = 120;
            outwidth = 480;
            outheight = 360;
            break;

        case 320:
            break;

        case 640:
            return;

        default:
            return;
    }

    switch(mode)
    {
        case SCALE_PP:
            if(outwidth > inwidth)
            {
                SetDecRegister(h264Regs, HWIF_HOR_SCALE_MODE, 1);

                SetDecRegister(h264Regs, HWIF_SCALE_WRATIO, ((outwidth-1)*65536)/(inwidth-1));

                SetDecRegister(h264Regs, HWIF_WSCALE_INVRA, ((inwidth-1)*65536)/(outwidth-1));
            }else{
                SetDecRegister(h264Regs, HWIF_HOR_SCALE_MODE, 2);

                SetDecRegister(h264Regs, HWIF_WSCALE_INVRA, ((outwidth)*65536)/inwidth);
            }

            if(outheight > inheight)
            {
                SetDecRegister(h264Regs, HWIF_VER_SCALE_MODE, 1);

                SetDecRegister(h264Regs, HWIF_SCALE_HRATIO, ((outheight-1)*65536)/(inheight-1));

                SetDecRegister(h264Regs, HWIF_HSCALE_INVRA, ((inheight-1)*65536)/(outheight-1));
            }else{
                SetDecRegister(h264Regs, HWIF_VER_SCALE_MODE, 2);

                SetDecRegister(h264Regs, HWIF_HSCALE_INVRA, ((outheight)*65536/inheight+1));
            }

            h264Regs[60] = 0x2;
            h264Regs[61] = 0xffff05f0;
            h264Regs[62] = 0;
            h264Regs[63] = h264Regs[13];
            h264Regs[64] = (h264Regs[13]+inwidth*((inheight+15)&(~0xf)));
            h264Regs[65] = 0;
            h264Regs[66] = memorydispaddr;
            h264Regs[67] = (memorydispaddr+outwidth*((outheight+15)&(~0xf)));
            h264Regs[68] = 0;
            h264Regs[69] = 0;
            h264Regs[70] = 0;
            h264Regs[71] = 0;
            h264Regs[72] = (inwidth>>4)|((inheight>>4)<<9);//0x8878;
            h264Regs[73] = 0;
            h264Regs[74] = 0;
//            h264Regs[79] += 0x20000000;
//            h264Regs[80] = 0x05000000;

            h264Regs[82] = 0;
            h264Regs[83] = 0;
            h264Regs[84] = 0;

            h264Regs[85] = 0x34000000+(outwidth<<4)+(outheight<<15)+((inheight>>3)&1)+((inwidth>>2)&2);//0x34f03200;
            h264Regs[86] = 0;
            h264Regs[87] = 0;

            h264Regs[88] = (inwidth>>4)<<23;//0x3c000000;
            h264Regs[89] = 0;
            h264Regs[90] = 0;
            h264Regs[91] = 0;

            h264Regs[92] = outwidth;//0x320;
            h264Regs[93] = 0;
            h264Regs[94] = 0;

            break;

        default :
            ALOGV("PP mode not use! haha!");
            break;
    }

}

}

