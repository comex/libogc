#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "asm.h"
#include "processor.h"
#include "ogcsys.h"
#include "irq.h"
#include "exi.h"
#include "gx.h"
#include "lwp.h"
#include "system.h"
#include "video.h"
#include "video_types.h"

//#define _VIDEO_DEBUG

#define VIDEO_MQ					1

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))
#define VI_REGCHANGE(_reg)	\
	((u64)0x01<<(63-_reg))

GXRModeObj TVNtsc480IntDf = 
{
    VI_TVMODE_NTSC_INT,     // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 8,         // line n-1
		 8,         // line n-1
		10,         // line n
		12,         // line n
		10,         // line n
		 8,         // line n+1
		 8          // line n+1
	}
};

GXRModeObj TVNtsc480IntAa = 
{
    VI_TVMODE_NTSC_INT,     // viDisplayMode
    640,             // fbWidth
    242,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_TRUE,         // aa

    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},  // pix 1
		{9,2},{3,6},{9,10},  // pix 2
		{9,2},{3,6},{9,10}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 4,         // line n-1
		 8,         // line n-1
		12,         // line n
		16,         // line n
		12,         // line n
		 8,         // line n+1
		 4          // line n+1
	}
};

GXRModeObj TVMpal480IntDf = 
{
    VI_TVMODE_MPAL_INT,     // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_MPAL - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_MPAL - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 8,         // line n-1
		 8,         // line n-1
		10,         // line n
		12,         // line n
		10,         // line n
		 8,         // line n+1
		 8          // line n+1
	}
};

GXRModeObj TVPal264Ds = 
{
    VI_TVMODE_PAL_DS,       // viDisplayMode
    640,             // fbWidth
    264,             // efbHeight
    264,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL/2 - 528/2)/2,        // viYOrigin
    640,             // viWidth
    528,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVPal264DsAa = 
{
    VI_TVMODE_PAL_DS,       // viDisplayMode
    640,             // fbWidth
    264,             // efbHeight
    264,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL/2 - 528/2)/2,        // viYOrigin
    640,             // viWidth
    528,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_TRUE,         // aa

    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},  // pix 1
		{9,2},{3,6},{9,10},  // pix 2
		{9,2},{3,6},{9,10}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVPal264Int = 
{
    VI_TVMODE_PAL_INT,      // viDisplayMode
    640,             // fbWidth
    264,             // efbHeight
    264,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL - 528)/2,        // viYOrigin
    640,             // viWidth
    528,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_TRUE,         // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVPal264IntAa = 
{
    VI_TVMODE_PAL_INT,      // viDisplayMode
    640,             // fbWidth
    264,             // efbHeight
    264,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL - 528)/2,        // viYOrigin
    640,             // viWidth
    528,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_TRUE,         // field_rendering
    GX_TRUE,         // aa

    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},  // pix 1
		{9,2},{3,6},{9,10},  // pix 2
		{9,2},{3,6},{9,10}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVPal524IntAa = 
{
	VI_TVMODE_PAL_INT,
	640,
	264,
	524,
	(VI_MAX_WIDTH_PAL-640)/2,
	(VI_MAX_HEIGHT_PAL-528)/2,
	640,
	524,
	VI_XFBMODE_DF,
	GX_FALSE,
	GX_TRUE,
	
    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},				// pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},				// pix 1
		{9,2},{3,6},{9,10},				// pix 2
		{9,2},{3,6},{9,10}				// pix 3
	},
 
	 // vertical filter[7], 1/64 units, 6 bits each
	{
		4,         // line n-1
		8,         // line n-1
		12,        // line n
		16,        // line n
		12,        // line n
		8,         // line n+1
		4          // line n+1
	}
};

GXRModeObj TVPal528Int = 
{
    VI_TVMODE_PAL_INT,       // viDisplayMode
    640,             // fbWidth
    528,             // efbHeight
    528,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL - 528)/2,        // viYOrigin
    640,             // viWidth
    528,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVPal528IntDf = 
{
    VI_TVMODE_PAL_INT,      // viDisplayMode
    640,             // fbWidth
    528,             // efbHeight
    528,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL - 528)/2,        // viYOrigin
    640,             // viWidth
    528,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},
    // vertical filter[7], 1/64 units, 6 bits each
	{
		 8,         // line n-1
		 8,         // line n-1
		10,         // line n
		12,         // line n
		10,         // line n
		 8,         // line n+1
		 8          // line n+1
	}
};

GXRModeObj TVPal574IntDfScale = 
{
    VI_TVMODE_PAL_INT,      // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    574,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL - 574)/2,        // viYOrigin
    640,             // viWidth
    574,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},
    // vertical filter[7], 1/64 units, 6 bits each
	{
		 8,         // line n-1
		 8,         // line n-1
		10,         // line n
		12,         // line n
		10,         // line n
		 8,         // line n+1
		 8          // line n+1
	}
};

static const struct _timing {
	u8 equ;
	u16 acv;
	u16 prbOdd,prbEven;
	u16 psbOdd,psbEven;
	u8 bs1,bs2,bs3,bs4;
	u16 be1,be2,be3,be4;
	u16 nhlines,hlw;
	u8 hsy,hcs,hce,hbe640;
	u16 hbs640;
	u8 hbeCCIR656;
	u16 hbsCCIR656;
} video_timing[6] = {
	{
		0x06,0x00F0,
		0x0018,0x0019,0x0003,0x0002,
		0x0C,0x0D,0x0C,0x0D,
		0x0208,0x0207,0x0208,0x0207,
		0x020D,0x01AD,
		0x40,0x47,0x69,0xA2,
		0x0175,
		0x7A,0x019C
	},
	{
		0x06,0x00F0,
		0x0018,0x0018,0x0004,0x0004,
		0x0C,0x0C,0x0C,0x0C,
		0x0208,0x0208,0x0208,0x0208,
		0x020E,0x01AD,
		0x40,0x47,0x69,0xA2,
		0x0175,
		0x7A,0x019C
	},
	{
		0x05,0x011F,
		0x0023,0x0024,0x0001,0x0000,
		0x0D,0x0C,0x0B,0x0A,
		0x026B,0x026A,0x0269,0x026C,
		0x0271,0x01B0,
		0x40,0x4B,0x6A,0xAC,
		0x017C,
		0x85,0x01A4	
	},
	{
		0x05,0x011F,
		0x0021,0x0021,0x0002,0x0002,
		0x0D,0x0B,0x0D,0x0B,
		0x026B,0x026D,0x026B,0x026D,
		0x0270,0x01B0,
		0x40,0x4B,0x6A,0xAC,
		0x017C,
		0x85,0x01A4
	},
	{
		0x06,0x00F0,
		0x0018,0x0019,0x0003,0x0002,
		0x10,0x0F,0x0E,0x0D,
		0x0206,0x0205,0x0204,0x0207,
		0x020D,0x01AD,
		0x40,0x4E,0x70,0xA2,
		0x0175,
		0x7A,0x019C
	},
	{
		0x06,0x00F0,
		0x0018,0x0018,0x0004,0x0004,
		0x10,0x0E,0x10,0x0E,
		0x0206,0x0208,0x0206,0x0208,
		0x020E,0x01AD,
		0x40,0x4E,0x70,0xA2,
		0x0175,
		0x7A,0x019C
	}
};

static const u16 taps[26] = {
	0x01F0,0x01DC,0x01AE,0x0174,0x0129,0x00DB,
	0x008E,0x0046,0x000C,0x00E2,0x00CB,0x00C0,
	0x00C4,0x00CF,0x00DE,0x00EC,0x00FC,0x0008,
	0x000F,0x0013,0x0013,0x000F,0x000C,0x0008,
	0x0001,0x0000                              
};

static struct _horVer {
	u16 dispPosX;
	u16 dispPosY;
	u16 dispSizeX;
	u16 dispSizeY;
	u16 adjustedDispPosX;
	u16 adjustedDispPosY;
	u16 adjustedDispSizeY;
	u16 adjustedPanPosY;
	u16 adjustedPanSizeY;
	u16 fbSizeX;
	u16 fbSizeY;
	u16 panPosX;
	u16 panPosY;
	u16 panSizeX;
	u16 panSizeY;
	u32 fbMode;	
	u32 nonInter;	
	u32 tv;
	u8 wordPerLine;
	u8 std;
	u8 wpl;
	void *bufAddr;
	u32 tfbb;
	u32 bfbb;
	u8 xof;
	s32 black;
	s32 threeD;
	void *rbufAddr;
	u32 rtfbb;
	u32 rbfbb;
	const struct _timing *timing;
} HorVer;

static u16 regs[60];
static u16 shdw_regs[60];
static u32 encoderType,fbSet = 0;
static s16 displayOffsetH;
static s16 displayOffsetV;
static u32 currTvMode,changeMode;
static u32 shdw_changeMode,flushFlag;
static u64 changed,shdw_changed;
static vu32 retraceCount;
static const struct _timing *currTiming;
static lwpq_t video_queue;
static VIRetraceCallback preRetraceCB = NULL;
static VIRetraceCallback postRetraceCB = NULL;

static vu16* const _viReg = (u16*)0xCC002000;

extern void __UnmaskIrq(u32);
extern void __MaskIrq(u32);

extern u32 __SYS_LockSram();
extern u32 __SYS_UnlockSram(u32 write);

#ifdef _VIDEO_DEBUG
static void printRegs()
{
	printf("%08x%08x\n",(u32)(shdw_changed>>32),(u32)shdw_changed);

	printf("%04x %04x %04x %04x %04x %04x %04x %04x\n",regs[0],regs[1],regs[2],regs[3],regs[4],regs[5],regs[6],regs[7]);
	printf("%04x %04x %04x %04x %04x %04x %04x %04x\n",regs[8],regs[9],regs[10],regs[11],regs[12],regs[13],regs[14],regs[15]);
	printf("%04x %04x %04x %04x %04x %04x %04x %04x\n",regs[16],regs[17],regs[18],regs[19],regs[20],regs[21],regs[22],regs[23]);
	printf("%04x %04x %04x %04x %04x %04x %04x %04x\n",regs[24],regs[25],regs[26],regs[27],regs[28],regs[29],regs[30],regs[31]);
	printf("%04x %04x %04x %04x %04x %04x %04x %04x\n",regs[32],regs[33],regs[34],regs[35],regs[36],regs[37],regs[38],regs[39]);
	printf("%04x %04x %04x %04x %04x %04x %04x %04x\n",regs[40],regs[41],regs[42],regs[43],regs[44],regs[45],regs[46],regs[47]);
	printf("%04x %04x %04x %04x %04x %04x %04x %04x\n",regs[48],regs[49],regs[50],regs[51],regs[52],regs[53],regs[54],regs[55]);
	printf("%04x %04x %04x %04x\n",regs[56],regs[57],regs[58],regs[59]);

}
#endif

static __inline__ u32 cntlzd(u64 bit)
{
	u32 hi,lo,value = 0;

	hi = (u32)(bit>>32);
	lo = (u32)(bit&-1);

	value = cntlzw(hi);
	if(value>=32) value += cntlzw(lo);

	return value;
}

static const struct _timing* __gettiming(u32 vimode)
{
	if(vimode>0x0015) return NULL;

	switch(vimode) {
		case VI_TVMODE_NTSC_INT:
			return &video_timing[0];
			break;
		case VI_TVMODE_NTSC_DS:
			return &video_timing[1];
			break;
		case VI_TVMODE_PAL_INT:
			return &video_timing[2];
			break;
		case VI_TVMODE_PAL_DS:
			return &video_timing[3];
			break;
		case VI_TVMODE_MPAL_INT:
			return &video_timing[4];
			break;
		case VI_TVMODE_MPAL_DS:
			return &video_timing[5];
			break;
	}
	return NULL;
}

static void __setInterruptRegs(const struct _timing *tm)
{
	u16 hlw;

	hlw = 0;
	if(tm->nhlines%2) hlw = tm->hlw;
	regs[24] = 0x1000|((tm->nhlines/2)+1);
	regs[25] = hlw+1;
	changed |= VI_REGCHANGE(24);
	changed |= VI_REGCHANGE(25);
}

static void __setPicConfig(u16 fbSizeX,u32 xfbMode,u16 panPosX,u16 panSizeX,u8 *wordPerLine,u8 *std,u8 *wpl,u8 *xof)
{
	*wordPerLine = (fbSizeX+15)/16;
	*std = *wordPerLine;
	if(xfbMode==VI_XFBMODE_DF) *std <<= 1;
	
	*xof = panPosX%16;
	*wpl = (*xof+(panSizeX+15))/16;
	regs[36] = (*wpl<<8)|*std;
	changed |= VI_REGCHANGE(36);
}

static void __setBBIntervalRegs(const struct _timing *tm)
{
	regs[10] = tm->bs3|(tm->be3<<5);
	regs[11] = tm->bs1|(tm->be1<<5);
	changed |= VI_REGCHANGE(10);
	changed |= VI_REGCHANGE(11);

	regs[12] = tm->bs4|(tm->be4<<5);
	regs[13] = tm->bs2|(tm->be2<<5);
	changed |= VI_REGCHANGE(12);
	changed |= VI_REGCHANGE(13);
}

static void __setScalingRegs(u16 panSizeX,u16 dispSizeX,s32 threeD)
{
	if(threeD) panSizeX = (panSizeX<<1)&0x1fffe;
	if(panSizeX<dispSizeX) {
		regs[37] = 0x1000|((dispSizeX+((panSizeX<<8)-1))/dispSizeX);
		regs[56] = panSizeX;
		changed |= VI_REGCHANGE(37);
		changed |= VI_REGCHANGE(56);
	} else {
		regs[37] = 0x100;
		changed |= VI_REGCHANGE(37);
	}
}

static void __calcFbbs(u32 bufAddr,u16 panPosX,u16 panPosY,u8 wordperline,u32 xfbMode,u16 dispPosY,u32 *tfbb,u32 *bfbb)
{
	u32 bytesPerLine,tmp;

	bytesPerLine = (wordperline<<5)&0x1fe0;
	*tfbb = bufAddr+(((panPosX<<1)&0x1ffe0)+(panPosY*bytesPerLine));
	*bfbb = *tfbb;
	if(xfbMode==VI_XFBMODE_DF) *bfbb = *tfbb+bytesPerLine;

	if(dispPosY%2) {
		tmp = *tfbb;
		*tfbb = *bfbb;
		*bfbb = tmp;
	}

	*tfbb = MEM_VIRTUAL_TO_PHYSICAL(*tfbb);
	*bfbb = MEM_VIRTUAL_TO_PHYSICAL(*bfbb);
}

static void __setFbbRegs(struct _horVer *horVer,u32 *tfbb,u32 *bfbb,u32 *rtfbb,u32 *rbfbb)
{
	u32 flag;
	__calcFbbs((u32)horVer->bufAddr,horVer->panPosX,horVer->adjustedPanPosY,horVer->wordPerLine,horVer->fbMode,horVer->adjustedDispPosY,tfbb,bfbb);
	if(horVer->threeD) __calcFbbs((u32)horVer->rbufAddr,horVer->panPosX,horVer->adjustedPanPosY,horVer->wordPerLine,horVer->fbMode,horVer->adjustedDispPosY,rtfbb,rbfbb);

	flag = 1;
	if((*tfbb)<0x01000000 && (*bfbb)<0x01000000
		&& (*rtfbb)<0x01000000 && (*rbfbb)<0x01000000) flag = 0;

	if(flag) {
		*tfbb >>= 5;
		*bfbb >>= 5;
		*rtfbb >>= 5;
		*rbfbb >>= 5;
	}

	regs[14] = _SHIFTL(flag,12,1)|_SHIFTL(horVer->xof,8,4)|_SHIFTR(*tfbb,16,8);
	regs[15] = *tfbb&0xffff;
	changed |= VI_REGCHANGE(14);
	changed |= VI_REGCHANGE(15);

	regs[18] = _SHIFTR(*bfbb,16,8);
	regs[19] = *bfbb&0xffff;
	changed |= VI_REGCHANGE(18);
	changed |= VI_REGCHANGE(19);
	
	if(horVer->threeD) {
		regs[16] = _SHIFTR(*rtfbb,16,8);
		regs[17] = *rtfbb&0xffff;
		changed |= VI_REGCHANGE(16);
		changed |= VI_REGCHANGE(17);
	
		regs[20] = _SHIFTR(*rbfbb,16,8);
		regs[21] = *rbfbb&0xffff;
		changed |= VI_REGCHANGE(20);
		changed |= VI_REGCHANGE(21);
	}
}

static void __setHorizontalRegs(const struct _timing *tm,u16 dispPosX,u16 dispSizeX)
{
	u32 val1,val2;

	regs[2] = (tm->hcs<<8)|tm->hce;
	regs[3] = tm->hlw;
	changed |= VI_REGCHANGE(2);
	changed |= VI_REGCHANGE(3);

	val1 = (tm->hbe640+dispPosX-40)&0x01ff;
	val2 = (tm->hbs640+dispPosX+40)-(720-dispSizeX);
	regs[4] = (val1>>9)|(val2<<1);
	regs[5] = (val1<<7)|tm->hsy;
	changed |= VI_REGCHANGE(4);
	changed |= VI_REGCHANGE(5);
}

static void __setVerticalRegs(u16 dispPosY,u16 dispSizeY,u8 equ,u16 acv,u16 prbOdd,u16 prbEven,u16 psbOdd,u16 psbEven,s32 black)
{
	u32 tmp;
	u32 div1,div2;
	u32 psb,prb;
	u32 psbodd,prbodd;
	u32 psbeven,prbeven;

	div1 = 2;
	div2 = 1;
	if(equ>=10) {
		div1 = 1;
		div2 = 2;
	}
	
	prb = div2*dispPosY;
	psb = div2*(((acv*div1)-dispSizeY)-dispPosY);
	if(dispPosY%2) {
		prbodd = prbEven+prb;
		psbodd = psbEven+psb;
		prbeven = prbOdd+prb;
		psbeven = psbOdd+psb;
	} else {
		prbodd = prbOdd+prb;
		psbodd = psbOdd+psb;
		prbeven = prbEven+prb;
		psbeven = psbEven+psb;
	}

	tmp = dispSizeY/div1;
	if(black) {
		prbodd += ((tmp<<1)-2);
		prbeven += ((tmp<<1)-2);
		psbodd += 2;
		psbeven += 2;
		tmp = 0;
	}
	
	regs[0] = ((tmp<<4)&~0x0f)|equ;
	changed |= VI_REGCHANGE(0);

	regs[6] = psbodd;
	regs[7] = prbodd;
	changed |= VI_REGCHANGE(6);
	changed |= VI_REGCHANGE(7);

	regs[8] = psbeven;
	regs[9] = prbeven;
	changed |= VI_REGCHANGE(8);
	changed |= VI_REGCHANGE(9);
}

static void __adjustPosition(u16 acv)
{
	u32 fact,field;
	s16 dispPosX,dispPosY;
	s16 dispSizeY;
	
	dispPosX = (HorVer.dispPosX+displayOffsetH);
	if(dispPosX<=(720-HorVer.dispSizeX)) {
		if(dispPosX>=0) HorVer.adjustedDispPosX = dispPosX;
		else HorVer.adjustedDispPosX = 0;
	} else HorVer.adjustedDispPosX = (720-HorVer.dispSizeX);

	if(HorVer.fbMode==VI_XFBMODE_SF) fact = 2;
	else fact = 1;

	field = HorVer.dispPosY&0x0001;
	dispPosY = HorVer.dispPosY+displayOffsetV;
	if(dispPosY>field) HorVer.adjustedDispPosY = dispPosY;
	else HorVer.adjustedDispPosY = field;

	dispSizeY = HorVer.dispPosY+HorVer.dispSizeY+displayOffsetV;
	if(dispSizeY>((acv<<1)-field)) dispSizeY -= (acv<<1)-field;
	else dispSizeY = 0;
	
	dispPosY = HorVer.dispPosY+displayOffsetV;
	if(dispPosY<field) dispPosY -= field;
	else dispPosY = 0;
	HorVer.adjustedDispSizeY = HorVer.dispSizeY+dispPosY-dispSizeY;
	
	dispPosY = HorVer.dispPosY+displayOffsetV;
	if(dispPosY<field) dispPosY -= field;
	else dispPosY = 0;
	HorVer.adjustedPanPosY = HorVer.panPosY-(dispPosY/fact);

	dispSizeY = HorVer.dispPosY+HorVer.dispSizeY+displayOffsetV;
	if(dispSizeY>((acv<<1)-field)) dispSizeY -= (acv<<1)-field;
	else dispSizeY = 0;
	
	dispPosY = HorVer.dispPosY+displayOffsetV;
	if(dispPosY<field) dispPosY -= field;
	else dispPosY = 0;
	HorVer.adjustedPanSizeY = HorVer.panSizeY+(dispPosY/fact)-(dispSizeY/fact);
}

static void __importAdjustingValues()
{
	syssram *sram;

	sram = (syssram*)__SYS_LockSram();
	displayOffsetH = sram->display_offsetH;
	displayOffsetV = 0;
	__SYS_UnlockSram(0);
}

static void __VIInit(u32 vimode)
{
	u32 cnt;
	u32 vi_mode,interlace,prog;
	const struct _timing *cur_timing = NULL;

	vi_mode = vimode>>2;
	interlace = vimode&0x01;
	prog = vimode&0x02;
	
	cur_timing = __gettiming(vimode);

	//reset the interface
	cnt = 0;
	_viReg[1] = 0x02;
	while(cnt<1000) cnt++;
	_viReg[1] = 0x00;

	// now begin to setup the interface
	_viReg[2] = ((cur_timing->hcs<<8)|cur_timing->hce);		//set HCS & HCE
	_viReg[3] = cur_timing->hlw;							//set Half Line Width

	_viReg[4] = (cur_timing->hbs640<<1);					//set HBS640
	_viReg[5] = ((cur_timing->hbe640<<7)|cur_timing->hsy);	//set HBE640 & HSY
	
	_viReg[0] = cur_timing->equ;

	_viReg[6] = (cur_timing->psbOdd+2);							//set PSB odd field
	_viReg[7] = (cur_timing->prbOdd+((cur_timing->acv<<1)-2));	//set PRB odd field

	_viReg[8] = (cur_timing->psbEven+2);						//set PSB even field
	_viReg[9] = (cur_timing->prbEven+((cur_timing->acv<<1)-2));	//set PRB even field

	_viReg[10] = ((cur_timing->be3<<5)|cur_timing->bs3);		//set BE3 & BS3
	_viReg[11] = ((cur_timing->be1<<5)|cur_timing->bs1);		//set BE1 & BS1
	
	_viReg[12] = ((cur_timing->be4<<5)|cur_timing->bs4);		//set BE4 & BS4
	_viReg[13] = ((cur_timing->be2<<5)|cur_timing->bs2);		//set BE2 & BS2

	_viReg[24] = (0x1000|((cur_timing->nhlines/2)+1));
	_viReg[25] = (cur_timing->hlw+1);
	
	_viReg[26] = 0x1001;		//set DI1
	_viReg[27] = 0x0001;		//set DI1
	_viReg[36] = 0x2828;		//set HSR
	
	if(vimode==0x0002 || vimode==0x0003) {
		_viReg[1] = ((vi_mode<<8)|0x0005);		//set MODE & INT & enable
		_viReg[54] = 0x0001;
	} else {
		_viReg[1] = ((vi_mode<<8)|(prog<<2)|0x0001);
		_viReg[54] = 0x0000;
	}
}

static u32 __getCurrentHalfLine()
{
	u32 vpos_old;
	u32 vpos = 0;
	u32 hpos = 0;

	vpos = _viReg[22]&0x07FF;
	do {
		vpos_old = vpos;
		hpos = _viReg[23]&0x07FF;
		vpos = _viReg[22]&0x07FF;
	} while(vpos_old!=vpos);
	
	hpos--;
	vpos--;
	vpos <<= 1;

	return vpos+(hpos/currTiming->hlw);	
}

static u32 __getCurrFieldEvenOdd()
{
	u32 hline;

	hline = __getCurrentHalfLine();
	if(hline<currTiming->nhlines) return 1;

	return 0;
}

static u32 __VISetRegs()
{
	u32 val;
	u64 mask;

	if(shdw_changeMode==1){
		if(!__getCurrFieldEvenOdd()) return 0;
	}

	while(shdw_changed) {
		val = cntlzd(shdw_changed);
		_viReg[val] = shdw_regs[val];
		mask = VI_REGCHANGE(val);
		shdw_changed &= ~mask;
	}
	shdw_changeMode = 0;
	currTiming = HorVer.timing;
	currTvMode = HorVer.tv;
	
	return 1;
}

static void __VIRetraceHandler(u32 nIrq,void *pCtx)
{
	u32 ret = 0;
	
	if(_viReg[24]&0x8000) {
		_viReg[24] &= ~0x8000;
		ret |= 0x01;
	}
	if(_viReg[26]&0x8000) {
		_viReg[26] &= ~0x8000;
		ret |= 0x02;
	}
	if(_viReg[28]&0x8000) {
		_viReg[28] &= ~0x8000;
		ret |= 0x04;
	}
	if(_viReg[30]&0x8000) {
		_viReg[30] &= ~0x8000;
		ret |= 0x08;
	}
	if(ret&0xc) return;

	retraceCount++;
	if(preRetraceCB)
		preRetraceCB(retraceCount);

	if(flushFlag) {
		if(__VISetRegs()) {
			flushFlag = 0;
		}
	}

	if(postRetraceCB)
		postRetraceCB(retraceCount);

	LWP_WakeThread(video_queue);
}

void VIDEO_Init()
{
	u32 vimode = 0;

	if(!(_viReg[1]&0x0001))
		__VIInit(VI_TVMODE_NTSC_INT);

	retraceCount = 0;
	changed = 0;
	shdw_changed = 0;
	shdw_changeMode = 0;
	flushFlag = 0;
	encoderType = 1;
	
	_viReg[38] = ((taps[1]>>6)|(taps[2]<<4));
	_viReg[39] = (taps[0]|_SHIFTL(taps[1],10,6));
	_viReg[40] = ((taps[4]>>6)|(taps[5]<<4));
	_viReg[41] = (taps[3]|_SHIFTL(taps[4],10,6));
	_viReg[42] = ((taps[7]>>6)|(taps[8]<<4));
	_viReg[43] = (taps[6]|_SHIFTL(taps[7],10,6));
	_viReg[44] = (taps[11]|(taps[12]<<8));
	_viReg[45] = (taps[9]|(taps[10]<<8));
	_viReg[46] = (taps[15]|(taps[16]<<8));
	_viReg[47] = (taps[13]|(taps[14]<<8));
	_viReg[48] = (taps[19]|(taps[20]<<8));
	_viReg[49] = (taps[17]|(taps[18]<<8));
	_viReg[50] = (taps[23]|(taps[24]<<8));
	_viReg[51] = (taps[21]|(taps[22]<<8));
	_viReg[56] = 640;

	__importAdjustingValues();

	HorVer.nonInter = _SHIFTR(_viReg[1],2,1);
	HorVer.tv = _SHIFTR(_viReg[1],8,2);

	vimode = HorVer.nonInter;
	if(HorVer.tv!=VI_DEBUG) vimode += (HorVer.tv<<2);
	currTiming = __gettiming(vimode);
	currTvMode = HorVer.tv;

	regs[1] = _viReg[1];
	HorVer.timing = currTiming;
	HorVer.dispSizeX = 640;
	HorVer.dispSizeY = currTiming->acv<<1;
	HorVer.dispPosX = (VI_MAX_WIDTH_NTSC-HorVer.dispSizeX)/2;
	HorVer.dispPosY = 0;
	
	__adjustPosition(currTiming->acv);

	HorVer.fbSizeX = 640;
	HorVer.fbSizeY = currTiming->acv<<1;
	HorVer.panPosX = 0;
	HorVer.panPosY = 0;
	HorVer.panSizeX = 640;
	HorVer.panSizeY = currTiming->acv<<1;
	HorVer.fbMode = VI_XFBMODE_SF;
	HorVer.wordPerLine = 40;
	HorVer.std = 40;
	HorVer.wpl = 40;
	HorVer.xof = 0;
	HorVer.black = 1;
	HorVer.threeD = 0;
	
	_viReg[24] &= ~0x8000;
	_viReg[26] &= ~0x8000;

	preRetraceCB = NULL;
	postRetraceCB = NULL;
	
	LWP_InitQueue(&video_queue);

	IRQ_Request(IRQ_PI_VI,__VIRetraceHandler,NULL);
	__UnmaskIrq(IRQMASK(IRQ_PI_VI));
}

void VIDEO_Configure(GXRModeObj *rmode)
{
	u16 dcr;
	u32 nonint,vimode,level;
	const struct _timing *curtiming;

	_CPU_ISR_Disable(level);
	nonint = (rmode->viTVMode&0x0003);
	if(nonint!=HorVer.nonInter) {
		changeMode = 1;
		HorVer.nonInter = nonint;
	}
	HorVer.tv = _SHIFTR(rmode->viTVMode,2,3);
	HorVer.dispPosX = rmode->viXOrigin;
	HorVer.dispPosY = rmode->viYOrigin;
	if(HorVer.nonInter==VI_NON_INTERLACE) HorVer.dispPosY = HorVer.dispPosY<<1;

	HorVer.dispSizeX = rmode->viWidth;
	HorVer.fbSizeX = rmode->fbWidth;
	HorVer.fbSizeY = rmode->xfbHeight;
	HorVer.fbMode = rmode->xfbMode;
	HorVer.panSizeX = HorVer.fbSizeX;
	HorVer.panSizeY = HorVer.fbSizeY;
	HorVer.panPosX = 0;
	HorVer.panPosY = 0;
	
	if(HorVer.nonInter==VI_PROGRESSIVE || HorVer.nonInter==(VI_NON_INTERLACE|VI_PROGRESSIVE)) HorVer.dispSizeY = HorVer.panSizeY;
	else if(HorVer.fbMode==VI_XFBMODE_SF) HorVer.dispSizeY = HorVer.panSizeY<<1;
	else HorVer.dispSizeY = HorVer.panSizeY;
	
	if(HorVer.nonInter==(VI_NON_INTERLACE|VI_PROGRESSIVE)) HorVer.threeD = 1;
	else HorVer.threeD = 0;

	vimode = VI_TVMODE(HorVer.tv,HorVer.nonInter);
	curtiming = __gettiming(vimode);
	HorVer.timing = curtiming;

	__adjustPosition(curtiming->acv);
	
	if(!encoderType) HorVer.tv = VI_DEBUG;
	
	__setInterruptRegs(curtiming);
	
	dcr = regs[1]&~0x030c;
	dcr |= _SHIFTL(HorVer.threeD,3,1);
	if(HorVer.nonInter==VI_PROGRESSIVE || HorVer.nonInter==(VI_NON_INTERLACE|VI_PROGRESSIVE)) dcr |= 0x0004;
	else dcr |= _SHIFTL(HorVer.nonInter,2,1);
	if(!(HorVer.tv==VI_DEBUG_PAL || HorVer.tv==VI_EURGB60)) dcr |= _SHIFTL(HorVer.tv,8,2);
	regs[1] = dcr;
	changed |= VI_REGCHANGE(1);

	regs[54] &= ~0x0001;
	if(rmode->viTVMode==VI_TVMODE_NTSC_PROG || rmode->viTVMode==VI_TVMODE_NTSC_PROG_DS) regs[54] |= 0x0001;
	changed |= VI_REGCHANGE(54);

	__setScalingRegs(HorVer.panSizeX,HorVer.dispSizeX,HorVer.threeD);
	__setHorizontalRegs(curtiming,HorVer.adjustedDispPosX,HorVer.dispSizeX);
	__setBBIntervalRegs(curtiming);
	__setPicConfig(HorVer.fbSizeX,HorVer.fbMode,HorVer.panPosX,HorVer.panSizeX,&HorVer.wordPerLine,&HorVer.std,&HorVer.wpl,&HorVer.xof);

	if(fbSet) __setFbbRegs(&HorVer,&HorVer.tfbb,&HorVer.bfbb,&HorVer.rtfbb,&HorVer.rbfbb);

	__setVerticalRegs(HorVer.adjustedDispPosY,HorVer.adjustedDispSizeY,curtiming->equ,curtiming->acv,curtiming->prbOdd,curtiming->prbEven,curtiming->psbOdd,curtiming->psbEven,HorVer.black);
	_CPU_ISR_Restore(level);
}

void VIDEO_WaitVSync(void)
{
	u32 level;
	u32 retcnt;
	
	_CPU_ISR_Disable(level);
	retcnt = retraceCount;
	do {
		LWP_SleepThread(video_queue);
	} while(retraceCount==retcnt);
	_CPU_ISR_Restore(level);
}

void VIDEO_SetFramebuffer(void *fb)
{
	u32 level,flag;
	u32 tfbb,bfbb;

	_CPU_ISR_Disable(level);
	tfbb = MEM_VIRTUAL_TO_PHYSICAL(fb);
	bfbb = tfbb+1280;

	flag = 1;
	if(tfbb<0x01000000 && bfbb<0x01000000) flag = 0;
	if(flag) {
		tfbb >>= 5;
		bfbb >>= 5;
	}

	_viReg[14] = (flag<<12)|(0<<8)|(tfbb>>16);
	_viReg[15] = tfbb;

	_viReg[18] = bfbb>>16;
	_viReg[19] = bfbb&0xffff;
	
	_CPU_ISR_Restore(level);
}

void VIDEO_SetNextFramebuffer(void *fb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	fbSet = 1;
	HorVer.bufAddr = fb;
	__setFbbRegs(&HorVer,&HorVer.tfbb,&HorVer.bfbb,&HorVer.rtfbb,&HorVer.rbfbb);
	_CPU_ISR_Restore(level);
}

void VIDEO_SetNextRightFramebuffer(void *fb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	fbSet = 1;
	HorVer.rbufAddr = fb;
	__setFbbRegs(&HorVer,&HorVer.tfbb,&HorVer.bfbb,&HorVer.rtfbb,&HorVer.rbfbb);
	_CPU_ISR_Restore(level);
}

void VIDEO_Flush()
{
	u32 level;
	u32 val;
	u64 mask;

	_CPU_ISR_Disable(level);
	shdw_changeMode = changeMode;
	changeMode = 0;

	shdw_changed |= changed;
	while(changed) {
		val = cntlzd(changed);
		shdw_regs[val] = regs[val];
		mask = VI_REGCHANGE(val);
		changed &= ~mask;
	}
	flushFlag = 1;
#ifdef _VIDEO_DEBUG
	printRegs();
#endif
	_CPU_ISR_Restore(level);
}

void VIDEO_SetBlack(boolean black)
{
	u32 level;
	const struct _timing *curtiming;
	
	_CPU_ISR_Disable(level);
	HorVer.black = black;
	curtiming = HorVer.timing;
	__setVerticalRegs(HorVer.adjustedDispPosY,HorVer.dispSizeY,curtiming->equ,curtiming->acv,curtiming->prbOdd,curtiming->prbEven,curtiming->psbOdd,curtiming->psbEven,HorVer.black);
	_CPU_ISR_Restore(level);
}

u32 VIDEO_GetNextField()
{
	u32 level,field;

	_CPU_ISR_Disable(level);
	field = __getCurrFieldEvenOdd()^1;
	_CPU_ISR_Restore(level);
	
	return field^(HorVer.adjustedDispPosY&0x0001);
}

u32 VIDEO_GetCurrentTvMode()
{
	u32 mode;
	u32 level;
	u32 tv;

	_CPU_ISR_Disable(level);
	mode = currTvMode;
	if(mode==VI_DEBUG) tv = VI_NTSC;
	else if(mode==VI_EURGB60) tv = mode;
	else if(mode==VI_MPAL) tv = VI_MPAL;
	else if(mode==VI_NTSC) tv = VI_NTSC;
	else tv = VI_PAL;
	_CPU_ISR_Restore(level);

	return tv;
}

VIRetraceCallback VIDEO_SetPreRetraceCallback(VIRetraceCallback callback)
{
	u32 level = 0;
	VIRetraceCallback ret = preRetraceCB;
	_CPU_ISR_Disable(level);
	preRetraceCB = callback;
	_CPU_ISR_Restore(level);
	return ret;
}

VIRetraceCallback VIDEO_SetPostRetraceCallback(VIRetraceCallback callback)
{
	u32 level = 0;
	VIRetraceCallback ret = postRetraceCB;
	_CPU_ISR_Disable(level);
	postRetraceCB = callback;
	_CPU_ISR_Restore(level);
	return ret;
}
