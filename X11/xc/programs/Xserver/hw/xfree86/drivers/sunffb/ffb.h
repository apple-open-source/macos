/*
 * Acceleration for the Creator and Creator3D framebuffer - defines.
 *
 * Copyright (C) 1998,1999,2000 Jakub Jelinek (jakub@redhat.com)
 * Copyright (C) 1998 Michal Rehacek (majkl@iname.com)
 * Copyright (C) 1999 David S. Miller (davem@redhat.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * JAKUB JELINEK, MICHAL REHACEK, OR DAVID MILLER BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb.h,v 1.8 2002/12/06 02:44:03 tsi Exp $ */

#ifndef FFB_H
#define FFB_H

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86RamDac.h"
#include "Xmd.h"
#include "gcstruct.h"
#include "windowstr.h"
#include "ffb_regs.h"
#include "xf86sbusBus.h"
#include "ffb_dac.h"
#ifdef XF86DRI
#include "xf86drm.h"
#include "ffb_drishare.h"
#endif
#ifndef  DPMS_SERVER
#define  DPMS_SERVER
#endif   /* DPMS_SERVER */
#include "extensions/dpms.h"


/* Various offsets in virtual (ie. mmap()) spaces Linux and Solaris support. */
/* Note: do not mmap FFB_DFB8R_VOFF and following mappings using one mmap together
   with any previous SFB mapping, otherwise the OS won't be able to use 4M pages
   for the DFB mappings. -jj */
#define	FFB_SFB8R_VOFF		0x00000000
#define	FFB_SFB8G_VOFF		0x00400000
#define	FFB_SFB8B_VOFF		0x00800000
#define	FFB_SFB8X_VOFF		0x00c00000
#define	FFB_SFB32_VOFF		0x01000000
#define	FFB_SFB64_VOFF		0x02000000
#define	FFB_FBC_REGS_VOFF	0x04000000
#define	FFB_BM_FBC_REGS_VOFF	0x04002000
#define	FFB_DFB8R_VOFF		0x04004000
#define	FFB_DFB8G_VOFF		0x04404000
#define	FFB_DFB8B_VOFF		0x04804000
#define	FFB_DFB8X_VOFF		0x04c04000
#define	FFB_DFB24_VOFF		0x05004000
#define	FFB_DFB32_VOFF		0x06004000
#define	FFB_DFB422A_VOFF	0x07004000	/* DFB 422 mode write to A */
#define	FFB_DFB422AD_VOFF	0x07804000	/* DFB 422 mode with line doubling */
#define	FFB_DFB24B_VOFF		0x08004000	/* DFB 24bit mode write to B */
#define	FFB_DFB422B_VOFF	0x09004000	/* DFB 422 mode write to B */
#define	FFB_DFB422BD_VOFF	0x09804000	/* DFB 422 mode with line doubling */
#define	FFB_SFB16Z_VOFF		0x0a004000	/* 16bit mode Z planes */
#define	FFB_SFB8Z_VOFF		0x0a404000	/* 8bit mode Z planes */
#define	FFB_SFB422_VOFF		0x0ac04000	/* SFB 422 mode write to A/B */
#define	FFB_SFB422D_VOFF	0x0b404000	/* SFB 422 mode with line doubling */
#define	FFB_FBC_KREGS_VOFF	0x0bc04000
#define	FFB_DAC_VOFF		0x0bc06000
#define	FFB_PROM_VOFF		0x0bc08000
#define	FFB_EXP_VOFF		0x0bc18000
 
#if defined(__GNUC__) && defined(USE_VIS)
#define FFB_ALIGN64	__attribute__((aligned(8)))
#else
#define FFB_ALIGN64
#endif

typedef struct {
	unsigned int fg FFB_ALIGN64, bg;	/* FG/BG colors for stipple	*/
	unsigned int patalign;			/* X/Y alignment of bits	*/
	unsigned char alu;			/* ALU operation		*/
	unsigned char inhw;			/* Cached in FFB		*/
	unsigned char pagable;			/* Can be used in a pagefill	*/
	unsigned char ph;			/* Page buffer height		*/
	unsigned int bits[32];			/* The stipple bits themselves	*/
} CreatorStippleRec, *CreatorStipplePtr;

typedef struct {
	int type;
	unsigned int linepat;
	CreatorStipplePtr stipple;
	void (*PolySegment)(DrawablePtr, GCPtr, int, xSegment *);
	void (*Polylines)(DrawablePtr, GCPtr, int, int, DDXPointPtr);
} CreatorPrivGCRec, *CreatorPrivGCPtr;

/* WID and framebuffer controls are a property of the
 * window.
 */
typedef struct {
	CreatorStipplePtr	Stipple;
	unsigned int		fbc_base;
	unsigned int		wid;
} CreatorPrivWinRec, *CreatorPrivWinPtr;

enum ffb_resolution {
	ffb_res_standard = 0,	/* 1280 x 1024 */
	ffb_res_high,		/* 1920 x 1360 */
	ffb_res_stereo,		/* 960  x 580 */
	ffb_res_portrait	/* 1280 x 2048 */
};

enum ffb_chip_type {
	ffb1_prototype = 0,	/* Early pre-FCS FFB */
	ffb1_standard,		/* First FCS FFB, 100Mhz UPA, 66MHz gclk */
	ffb1_speedsort,		/* Second FCS FFB, 100Mhz UPA, 75MHz gclk */
	ffb2_prototype,		/* Early pre-FCS vertical FFB2 */
	ffb2_vertical,		/* First FCS FFB2/vertical, 100Mhz UPA, 100MHZ gclk,
				   75(SingleBuffer)/83(DoubleBuffer) MHz fclk */
	ffb2_vertical_plus,	/* Second FCS FFB2/vertical, same timings */
	ffb2_horizontal,	/* First FCS FFB2/horizontal, same timings as FFB2/vert */
	ffb2_horizontal_plus,	/* Second FCS FFB2/horizontal, same timings */
	afb_m3,			/* FCS Elite3D, 3 float chips */
	afb_m6			/* FCS Elite3D, 6 float chips */
};

#ifdef XF86DRI
typedef struct {
	int	index;
} FFBConfigPrivRec, *FFBConfigPrivPtr;
#endif

typedef struct {
	unsigned short fifo_cache;
	unsigned short rp_active;
	ffb_fbcPtr regs;
	unsigned int ppc_cache;
	unsigned int pmask_cache;
	unsigned int rop_cache;
	unsigned int drawop_cache;
	unsigned int fg_cache;
	unsigned int bg_cache;
	unsigned int fontw_cache;
	unsigned int fontinc_cache;
	unsigned int fbc_cache;
	unsigned int wid_cache;
	enum ffb_chip_type ffb_type;
	CreatorStipplePtr laststipple;
	unsigned *fb;
	unsigned *sfb32;
	unsigned *sfb8r;
	unsigned *sfb8x;
	unsigned *dfb24;
	unsigned *dfb8r;
	unsigned *dfb8x;

	/* Slot offset 0x0200000, used to probe board type. */
	volatile unsigned int *strapping_bits;

	/* Needed for some 3DRAM revisions and ffb1 in hires */
	unsigned char disable_pagefill;

	/* Needed for FFB2/AFB when in stereo or hires */
	unsigned char disable_fastfill_ap;

	/* Needed for FFB1 in highres mode */
	unsigned char disable_vscroll;

	/* Needed on all FFB1 boards. */
	unsigned char has_brline_bug;

	/* Available on FFB2 and AFB */
	unsigned char use_blkread_prefetch;

	/* Framebuffer configuration */
	unsigned char has_double_res;
	unsigned char has_z_buffer;
	unsigned char has_double_buffer;

	enum ffb_resolution ffb_res;
	BoxRec ClippedBoxBuf[64];
	xRectangle Pf_Fixups[4];
	short Pf_AlignTab[0x800];

	ffb_dacPtr dac;
	sbusDevicePtr psdp;
	Bool HWCursor;
	Bool NoAccel;
	Bool vtSema;
	CloseScreenProcPtr CloseScreen;
	xf86CursorInfoPtr CursorInfoRec;
	unsigned char CursorShiftX, CursorShiftY;
	unsigned char *CursorData;

	PixmapPtr pix32, pix8;

	void *I2C;
	struct ffb_dac_info dac_info;

#ifdef XF86DRI
	void *pDRIInfo;
	int numVisualConfigs;
	void *pVisualConfigs;
	FFBConfigPrivPtr pVisualConfigsPriv;
	int drmSubFD;
	Bool dri_enabled;
	ffb_dri_state_t *pFfbSarea;
#endif

	OptionInfoPtr Options;
} FFBRec, *FFBPtr;

/* Acceleration */
extern Bool FFBAccelInit(ScreenPtr, FFBPtr);
extern void CreatorVtChange (ScreenPtr pScreen, int enter);

/* HW cursor support */
extern Bool FFBHWCursorInit(ScreenPtr);

/* Exported DAC layer routines. */
extern void FFBDacLoadCursorPos(FFBPtr, int, int);
extern void FFBDacLoadCursorColor(FFBPtr, int, int);
extern void FFBDacCursorEnableDisable(FFBPtr, int);
extern void FFBDacCursorLoadBitmap(FFBPtr, int, int, unsigned int *);
extern void FFBDacLoadPalette(ScrnInfoPtr, int, int *, LOCO *, VisualPtr);
extern Bool FFBDacInit(FFBPtr);
extern void FFBDacFini(FFBPtr);
extern void FFBDacEnterVT(FFBPtr);
extern void FFBDacLeaveVT(FFBPtr);
extern Bool FFBDacSaveScreen(FFBPtr, int);
extern void FFBDacDPMSMode(FFBPtr, int, int);

/* Exported WID layer routines. */
extern void FFBWidPoolInit(FFBPtr);
extern unsigned int FFBWidAlloc(FFBPtr, int, int, Bool);
extern void FFBWidFree(FFBPtr, unsigned int);
extern unsigned int FFBWidUnshare(FFBPtr, unsigned int);
extern unsigned int FFBWidReshare(FFBPtr, unsigned int);
extern void FFBWidChangeBuffer(FFBPtr, unsigned int, int);
extern Bool FFBWidIsShared(FFBPtr pFfb, unsigned int wid);

/* Accelerated double-buffering. */
extern Bool FFBDbePreInit(ScreenPtr);

#ifdef XF86DRI
/* DRI support */
extern Bool FFBDRIScreenInit(ScreenPtr);
extern Bool FFBDRIFinishScreenInit(ScreenPtr);
extern void FFBDRICloseScreen(ScreenPtr);
#endif

/* The fastfill and pagefill buffer sizes change based upon
 * the resolution.
 */
struct fastfill_parms {
	int	fastfill_small_area;
	int	pagefill_small_area;

	int	fastfill_height;
	int	fastfill_width;
	int	pagefill_height;
	int	pagefill_width;
};

extern struct fastfill_parms ffb_fastfill_parms[];

#define FFB_FFPARMS(__fpriv)	(ffb_fastfill_parms[(__fpriv)->ffb_res])

extern int  CreatorScreenPrivateIndex;
extern int  CreatorGCPrivateIndex;
extern int  CreatorWindowPrivateIndex;

#define GET_FFB_FROM_SCRN(p)	((FFBPtr)((p)->driverPrivate))

#define GET_FFB_FROM_SCREEN(s)						\
((FFBPtr)(s)->devPrivates[CreatorScreenPrivateIndex].ptr)

#define CreatorGetGCPrivate(g)						\
((CreatorPrivGCPtr) (g)->devPrivates [CreatorGCPrivateIndex].ptr)

#define CreatorGetWindowPrivate(w)					\
((CreatorPrivWinPtr) (w)->devPrivates[CreatorWindowPrivateIndex].ptr)
                            
#define CreatorSetWindowPrivate(w,p) 					\
((w)->devPrivates[CreatorWindowPrivateIndex].ptr = (pointer) p)

#undef DEBUG_FFB

#ifdef DEBUG_FFB

extern FILE *FDEBUG_FD;

static __inline__ void FFB_DEBUG_init(void)
{
	FDEBUG_FD = fopen("/tmp/FFB.DEBUG", "a");
}
#define FDEBUG(__x)				\
do {	fprintf __x; 				\
	fflush(FDEBUG_FD); 			\
} while(0)
#else
#define FFB_DEBUG_init()	do { } while(0)
#define FDEBUG(__x)		do { } while(0)
#endif

/* Enable this to get very verbose tracing of the driver onto stderr. */
#undef TRACE_FFB
#ifdef TRACE_FFB
#define FFBLOG(__x)		ErrorF __x
#else
#define FFBLOG(__x)		do { } while(0)
#endif

#endif /* FFB_H */
