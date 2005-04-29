/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3virge/s3v.h,v 1.32 2004/02/13 23:58:43 dawes Exp $ */

/*
 * Copyright (C) 1994-1999 The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _S3V_H
#define _S3V_H

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"

/* All drivers need this */
#include "xf86_ansic.h"

/* Everything using inb/outb, etc needs "compiler.h" */
#include "compiler.h"

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"

/* Drivers that need to access the PCI config space directly need this */
#include "xf86Pci.h"

#include "xf86Cursor.h"

#include "vgaHW.h"

#include "s3v_macros.h"

/* All drivers initialising the SW cursor need this */
#include "mipointer.h"

/* All drivers using the mi colormap manipulation need this */
#include "micmap.h"

/* Drivers using cfb need this */

#define PSZ 8
#include "cfb.h"
#undef PSZ

/* Drivers supporting bpp 16, 24 or 32 with cfb need these */

#include "cfb16.h"
#include "cfb24.h"
#include "cfb32.h"
#include "cfb24_32.h"

/* fb support */

#include "fb.h"

/* Drivers using the XAA interface ... */
#include "xaa.h"
#include "xaalocal.h"
#include "xf86cmap.h"
#include "xf86i2c.h"

#include "vbe.h"

#include "xf86xv.h"
#include "Xv.h"
#include "fourcc.h"

#ifndef _S3V_VGAHWMMIO_H
#define _S3V_VGAHWMMIO_H

#define VGAIN8(addr) MMIO_IN8(ps3v->MapBase,(S3V_MMIO_REGSIZE + (addr)))
#define VGAIN16(addr) MMIO_IN16(ps3v->MapBase,(S3V_MMIO_REGSIZE + (addr)))
#define VGAIN(addr) MMIO_IN32(ps3v->MapBase,(S3V_MMIO_REGSIZE + (addr)))
#define VGAOUT8(addr,val) MMIO_OUT8(ps3v->MapBase,(S3V_MMIO_REGSIZE + (addr)),\
                                                                          val)
#define VGAOUT16(addr,val) MMIO_OUT16(ps3v->MapBase,\
				      (S3V_MMIO_REGSIZE + (addr)), val)
#define VGAOUT(addr, val) MMIO_OUT32(ps3v->MapBase,\
				     (S3V_MMIO_REGSIZE + (addr)), val)

#define INREG(addr) MMIO_IN32(ps3v->MapBase, addr)
#define OUTREG(addr, val) MMIO_OUT32(ps3v->MapBase, addr, val)
#define NEW_INREG(addr) MMIO_IN32(s3vMmioMem, addr)
#define NEW_OUTREG(addr, val) MMIO_OUT32(s3vMmioMem, addr, val)

#endif /*_S3V_VGAHWMMIO_H*/

/*******************  s3v_i2c  ****************************/

Bool S3V_I2CInit(ScrnInfoPtr pScrn);

/******************* s3v_accel ****************************/

void S3VGEReset(ScrnInfoPtr pScrn, int from_timeout, int line, char *file);


/*********************************************/
/* locals */

/* Some S3 ViRGE structs */
#include "newmmio.h"

/* More ViRGE defines */
#include "regs3v.h"

/*********************************************/


	      
/* Driver data structure; this should contain all needed info for a mode */
/* used to be in s3v_driver.h for pre 4.0 */
typedef struct {      
   unsigned char SR08, SR0A, SR0F;
   unsigned char SR10, SR11, SR12, SR13, SR15, SR18; /* SR9-SR1C, ext seq. */
   unsigned char SR29;
   unsigned char SR54, SR55, SR56, SR57;
   unsigned char Clock;
   unsigned char s3DacRegs[0x101];
   unsigned char CR31, CR33, CR34, CR36, CR3A, CR3B, CR3C;
   unsigned char CR40, CR41, CR42, CR43, CR45;
   unsigned char CR51, CR53, CR54, CR55, CR58, CR5D, CR5E;
   unsigned char CR63, CR65, CR66, CR67, CR68, CR69, CR6D; /* Video attrib. */
   unsigned char CR7B, CR7D;
   unsigned char CR85, CR86, CR87;
   unsigned char CR90, CR91, CR92, CR93;
   unsigned char ColorStack[8]; /* S3 hw cursor color stack CR4A/CR4B */
   unsigned int  STREAMS[22];   /* Streams regs */
   unsigned int  MMPR0, MMPR1, MMPR2, MMPR3;   /* MIU regs */
} S3VRegRec, *S3VRegPtr;


/*********************************/
/*   S3VPortPrivRec              */
/*********************************/

typedef struct {
   unsigned char brightness;
   unsigned char contrast;
   FBAreaPtr	area;
   RegionRec	clip;
   CARD32	colorKey;
   CARD32	videoStatus;
   Time		offTime;
   Time		freeTime;
   int		lastPort;
} S3VPortPrivRec, *S3VPortPrivPtr;


/*************************/
/* S3VRec  		 */
/*************************/

typedef struct tagS3VRec {
	/* accel additions */
	CARD32		AccelFlags;
	CARD32		AccelCmd;
	CARD32		SrcBaseY, DestBaseY;
	CARD32		Stride;
	CARD32		CommonCmd;
	CARD32		FullPlaneMask;
	GCPtr		CurrentGC;
        /* fb support */
        DrawablePtr CurrentDrawable;
	/* end accel stuff */
  /* ViRGE specifics -start- */   
  /* Xv support */
  XF86VideoAdaptorPtr adaptor;
  S3VPortPrivPtr portPrivate;

  /* S3V console saved mode registers */
  S3VRegRec 		SavedReg;
  /* XServer video state mode registers */
  S3VRegRec 		ModeReg;
  /* HW Cursor info */
  xf86CursorInfoPtr	CursorInfoRec;
  /* Flag indicating ModeReg has been */
  /* duped from console state. */
  Bool		ModeStructInit;
  /* Is STREAMS processor needed for */
  /* this mode? */
  Bool 		NeedSTREAMS;
  /* Is STREAMS running now ? */
  Bool 		STREAMSRunning;
  /* Compatibility variables */
  int 		vgaCRIndex, vgaCRReg;
  int 		Width, Bpp,Bpl, ScissB;   
  /* XAA */
  unsigned 		PlaneMask;
  int 		bltbug_width1, bltbug_width2;
  /* In units as noted, set in PreInit */
  int			videoRambytes;
  int			videoRamKbytes;
  /* In Kbytes, set in PreInit */
  int			MemOffScreen;
  /* Holds the virtual memory address */
  /* returned when the MMIO registers */
  /* are mapped with xf86MapPciMem    */
  unsigned char *	MapBase;
  unsigned char *       MapBaseDense;
  
  /* Same as MapBase, except framebuffer*/
  unsigned char *	FBBase;
  /* Current visual FB starting location */
  unsigned char *	FBStart;
  /* Cursor storage location */
  CARD32			FBCursorOffset;
  /* Saved CR53 value */
  unsigned char	EnableMmioCR53;
  /* Extended reg unlock storage */
  unsigned char	CR38,CR39,CR40;
  /* Flag indicating if vgaHWMapMem was */
  /* used successfully for this screen */
  Bool		PrimaryVidMapped;
  /* Clock value */
  int			dacSpeedBpp;
  int			minClock;
  /* Maximum clock for present bpp */
  int			maxClock;
  int			HorizScaleFactor;
  Bool		bankedMono;
  /* Memory Clock */
  int 		MCLK;
  /* input reference Clock */
  int 		REFCLK;
  /* MX LCD clock			*/
  int			LCDClk;
  /* MX reference clock scale		*/
  double refclk_fact;
  /* Limit the number of errors	*/
  /* printed using a counter 	*/
  int			GEResetCnt;
  /* Accel WaitFifo function */
  void (*pWaitFifo)(struct tagS3VRec *, int);
  /* Accel WaitCmd function */
  void (*pWaitCmd)(struct tagS3VRec *);

  /*************************/
  /* ViRGE options -start- */
  /*************************/  
  OptionInfoPtr	Options;
  /* Enable PCI burst mode for reads? */
  Bool 		pci_burst;
  /* Diasable PCI retries */
  Bool		NoPCIRetry;
  /* Adjust fifo for acceleration? */
  Bool 		fifo_conservative;
  Bool 		fifo_moderate;
  Bool 		fifo_aggressive;
  /* Set memory options */
  Bool 		slow_edodram;
  Bool 		slow_dram;
  Bool 		fast_dram;
  Bool 		fpm_vram;
  /* Disable Acceleration */
  Bool		NoAccel;
  /* Adjust memory ras precharge */ 
  /* timing */
  Bool		ShowCache;
  Bool 		early_ras_precharge;
  Bool 		late_ras_precharge;
  /* MX LCD centering		*/
  Bool		lcd_center;
  /* hardware cursor enabled */
  Bool		hwcursor;
  Bool          UseFB;
  Bool          mx_cr3a_fix;
  Bool          XVideo;
  /* ViRGE options -end- */
  /***********************/
  /* ViRGE specifics -end- */
  
  /* Used by ViRGE driver, but generic */
  
  /* Pointer used to save wrapped */
  /* CloseScreen function.	*/
  CloseScreenProcPtr	CloseScreen;
  /* XAA info Rec 	*/
  XAAInfoRecPtr 	AccelInfoRec;
  /* PCI info vars.	*/
  pciVideoPtr 	PciInfo;
  PCITAG 		PciTag;
  /* Chip info, set using PCI	*/
  /* above.			*/
  int			Chipset;
  int			ChipRev;
  /* DGA2 */
  DGAModePtr	DGAModes;
  int			numDGAModes;
  Bool			DGAactive;
  int			DGAViewportStatus;
  I2CBusPtr             I2C;
    vbeInfoPtr          pVbe;
    Bool         shadowFB;
    int rotate;
    unsigned char * ShadowPtr;
    int ShadowPitch;
    void	(*PointerMoved)(int index, int x, int y);

    /* Used by ViRGE driver, but generic -end- */
  
  
} S3VRec, *S3VPtr;


#define S3VPTR(p) ((S3VPtr)((p)->driverPrivate))
      

/* #define S3V_DEBUG */
		 
#ifdef S3V_DEBUG
#define PVERB5(arg) ErrorF(arg)
#define VERBLEV	1
#else
#define PVERB5(arg) xf86ErrorFVerb(5, arg)
#define VERBLEV	5
#endif


/******************* regs3v *******************************/

/* cep kjb */
#define VertDebug 1

/* #ifndef MetroLink */ 
#if !defined (MetroLink) && !defined (VertDebug)
#define VerticalRetraceWait() do { \
   VGAOUT8(vgaCRIndex, 0x17); \
   if ( VGAIN8(vgaCRReg) & 0x80 ) { \
       while ((VGAIN8(vgaIOBase + 0x0A) & 0x08) == 0x00) ; \
       while ((VGAIN8(vgaIOBase + 0x0A) & 0x08) == 0x08) ; \
       while ((VGAIN8(vgaIOBase + 0x0A) & 0x08) == 0x00) ; \
       }\
} while (0)

#else
#define SPIN_LIMIT 1000000
#define VerticalRetraceWait() do { \
   VGAOUT8(vgaCRIndex, 0x17); \
   if ( VGAIN8(vgaCRReg) & 0x80 ) { \
	volatile unsigned long _spin_me; \
	for (_spin_me = 0; \
	 ((VGAIN8(vgaIOBase + 0x0A) & 0x08) == 0x00) && _spin_me <= SPIN_LIMIT; \
	 _spin_me++) ; \
	if (_spin_me > SPIN_LIMIT) \
	    ErrorF("s3v: warning: VerticalRetraceWait timed out(1:3).\n"); \
	for (_spin_me = 0; \
	 ((VGAIN8(vgaIOBase + 0x0A) & 0x08) == 0x08) && _spin_me <= SPIN_LIMIT; \
	 _spin_me++) ; \
	if (_spin_me > SPIN_LIMIT) \
	    ErrorF("s3v: warning: VerticalRetraceWait timed out(2:3).\n"); \
	for (_spin_me = 0; \
	 ((VGAIN8(vgaIOBase + 0x0A) & 0x08) == 0x00) && _spin_me <= SPIN_LIMIT; \
	 _spin_me++) ; \
	if (_spin_me > SPIN_LIMIT) \
	    ErrorF("s3v: warning: VerticalRetraceWait timed out(3:3).\n"); \
   } \
} while (0)
#endif


/*********************************************************/


/* Various defines which are used to pass flags between the Setup and 
 * Subsequent functions. 
 */

#define NO_MONO_FILL      0x00
#define NEED_MONO_FILL    0x01
#define MONO_TRANSPARENCY 0x02

/* prototypes */
/* s3v_dac.c */
extern void S3VCommonCalcClock(ScrnInfoPtr pScrn, DisplayModePtr mode, 
			long freq, int min_m, int min_n1, int max_n1,
			int min_n2, int max_n2, long freq_min, long freq_max,
			unsigned char * mdiv, unsigned char * ndiv);

/* s3v_accel.c */
extern Bool S3VAccelInit(ScreenPtr pScreen);
extern Bool S3VAccelInit32(ScreenPtr pScreen);
void S3VAccelSync(ScrnInfoPtr);
void S3VWaitFifoGX2(S3VPtr ps3v, int slots );
void S3VWaitFifoMain(S3VPtr ps3v, int slots );
void S3VWaitCmdGX2(S3VPtr ps3v);
void S3VWaitDummy(S3VPtr ps3v);

/* s3v_hwcurs.c */
extern Bool S3VHWCursorInit(ScreenPtr pScreen);

/* s3v_driver.c */
void S3VAdjustFrame(int scrnIndex, int x, int y, int flags);
Bool S3VSwitchMode(int scrnIndex, DisplayModePtr mode, int flags);

/* s3v_dga.c */
Bool S3VDGAInit(ScreenPtr pScreen);

/* in s3v_shadow.c */
void s3vPointerMoved(int index, int x, int y);
void s3vRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void s3vRefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void s3vRefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void s3vRefreshArea24(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void s3vRefreshArea32(ScrnInfoPtr pScrn, int num, BoxPtr pbox);

/* s3v_xv.c  X Video Extension support */
void S3VInitVideo(ScreenPtr pScreen);
int S3VQueryXvCapable(ScrnInfoPtr);

#endif  /*_S3V_H*/


/*EOF*/


