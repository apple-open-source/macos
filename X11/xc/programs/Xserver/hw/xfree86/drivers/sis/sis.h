/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis.h,v 1.29 2003/01/29 15:42:16 eich Exp $ */
/*
 * Copyright 1998,1999 by Alan Hourihane, Wigan, England.
 * Parts Copyright 2001, 2002 by Thomas Winischhofer, Vienna, Austria
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holder not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The copyright holder makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Alan Hourihane, alanh@fairlite.demon.co.uk
 *           Mike Chapman <mike@paranoia.com>,
 *           Juanjo Santamarta <santamarta@ctv.es>, 
 *           Mitani Hiroshi <hmitani@drl.mei.co.jp> 
 *           David Thomas <davtom@dream.org.uk>.
 *
 *	     Thomas Winischhofer <thomas@winischhofer.net>:
 *              - 310/325 series (315/550/650/651/740/M650) support
 *		- (possibly incomplete) Xabre (SiS330) support
 *              - new mode switching code for 300, 310/325 and 330 series
 *              - many fixes for 300/540/630/730 chipsets,
 *              - many fixes for 5597/5598, 6326 and 530/620 chipsets,
 *              - VESA mode switching (deprecated),
 *              - extended CRT2/video bridge handling support,
 *              - dual head support on 300, 310/325 and 330 series
 *              - 650/LVDS (up to 1400x1050), 650/Chrontel 701x support
 *              - 30xB/30xLV/30xLVX video bridge support (300, 310/325, 330 series)
 *              - Xv support for 5597/5598, 6326, 530/620 and 310/325 series
 *              - video overlay enhancements for 300 series
 *              - TV and hi-res support for the 6326
 *		- Color hardware cursor support for 300/310/325/330 series
 *              - etc.
 */


#ifndef _SIS_H
#define _SIS_H_

/* Always unlock the registers (should be set!) */
#define UNLOCK_ALWAYS

#if 0
#define TWDEBUG    /* for debugging */
#endif

#include "xf86Pci.h"
#include "xf86Cursor.h"
#include "xf86_ansic.h"
#include "xf86xv.h"
#include "compiler.h"
#include "xaa.h"
#include "vgaHW.h"
#include "vbe.h"
#include "osdef.h"
#include "vgatypes.h"
#include "vstruct.h"

#ifdef XF86DRI
#include "xf86drm.h"
#include "sarea.h"
#define _XF86DRI_SERVER_
#include "xf86dri.h"
#include "dri.h"
#include "GL/glxint.h"
#include "sis_dri.h"
#endif

#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,0,0,0)
typedef unsigned long IOADDRESS;
#endif

#if 1
#define SISDUALHEAD  	/* TW: Include Dual Head code  */
#endif

#if 1
#define USE6326VIDEO	/* TW: Include 6326/530/620 Xv code */
#endif

#if 1			/* TW: Include code for 330 - highly preliminary */
#define INCL_SIS330
#endif

#if 1			/* TW: Include code for cycling CRT2 type via keyboard */
#define CYCLECRT2	/* (not functional yet) */
#endif

#if 1
#define SISGAMMA	/* TW: Include code for gamma correction */
#endif

#if 1			/* TW: Include code for color hardware cursors */
#define SIS_ARGB_CURSOR
#endif

/* TW: new for SiS315/550/650/740/330 - these should be moved elsewhere! */
#ifndef PCI_CHIP_SIS315H
#define PCI_CHIP_SIS315H		0x0310
#endif
#ifndef PCI_CHIP_SIS315
#define PCI_CHIP_SIS315			0x0315
#endif
#ifndef PCI_CHIP_SIS315PRO
#define PCI_CHIP_SIS315PRO		0x0325
#endif
#ifndef PCI_CHIP_SIS550
#define PCI_CHIP_SIS550			0x5315	/* This is 550_VGA */
#endif
#ifndef PCI_CHIP_SIS650
#define PCI_CHIP_SIS650 		0x6325  /* This is 650_VGA and 740_VGA */
#endif
#ifndef PCI_CHIP_SIS330
#define PCI_CHIP_SIS330 		0x0330
#endif

#define SIS_NAME                "SIS"
#define SIS_DRIVER_NAME         "sis"
#define SIS_MAJOR_VERSION       0
#define SIS_MINOR_VERSION       6
#define SIS_PATCHLEVEL          0
#define SIS_CURRENT_VERSION     ((SIS_MAJOR_VERSION << 16) | \
                                (SIS_MINOR_VERSION << 8) | SIS_PATCHLEVEL )

/* pSiS->Flags (old series only) */
#define SYNCDRAM                0x00000001
#define RAMFLAG                 0x00000002
#define ESS137xPRESENT          0x00000004
#define SECRETFLAG              0x00000008
#define A6326REVAB              0x00000010
#define MMIOMODE                0x00010000
#define LFBQMODE                0x00020000
#define AGPQMODE                0x00040000
#define UMA                     0x80000000

#define BIOS_BASE               0xC0000
#define BIOS_SIZE               0x10000

/* TW: New mode switching code */
#define SR_BUFFER_SIZE          5
#define CR_BUFFER_SIZE          5

/* TW: VBFlags */
#define CRT2_DEFAULT            0x00000001
#define CRT2_LCD                0x00000002  /* TW: Never change the order of the CRT2_XXX entries */
#define CRT2_TV                 0x00000004  /*     (see SISCycleCRT2Type())                       */
#define CRT2_VGA                0x00000008
#define CRT2_ENABLE		(CRT2_LCD | CRT2_TV | CRT2_VGA)
#define DISPTYPE_DISP2		CRT2_ENABLE
#define TV_NTSC                 0x00000010
#define TV_PAL                  0x00000020
#define TV_HIVISION             0x00000040
#define TV_TYPE                 (TV_NTSC | TV_PAL | TV_HIVISION)
#define TV_AVIDEO               0x00000100
#define TV_SVIDEO               0x00000200
#define TV_SCART                0x00000400
#define TV_INTERFACE            (TV_AVIDEO | TV_SVIDEO | TV_SCART | TV_CHSCART | TV_CHHDTV)
#define TV_PALM                 0x00001000
#define TV_PALN                 0x00002000
#define TV_CHSCART              0x00008000
#define TV_CHHDTV               0x00010000
#define VGA2_CONNECTED          0x00040000
#define DISPTYPE_CRT1		0x00080000  	/* TW: CRT1 connected and used */
#define DISPTYPE_DISP1		DISPTYPE_CRT1
#define VB_301                  0x00100000	/* Video bridge type */
#define VB_301B                 0x00200000
#define VB_302B                 0x00400000
#define VB_303			0x00800000
#define VB_LVDS                 0x01000000
#define VB_CHRONTEL             0x02000000
#define VB_30xLV                0x04000000  	
#define VB_30xLVX               0x08000000  	
#define VB_TRUMPION		0x10000000     
#define VB_VIDEOBRIDGE		(VB_301|VB_301B|VB_302B|VB_303|VB_30xLV|VB_30xLVX| \
				 VB_LVDS|VB_CHRONTEL|VB_TRUMPION) /* TW */
#define VB_SISBRIDGE            (VB_301|VB_301B|VB_302B|VB_303|VB_30xLV|VB_30xLVX)
#define SINGLE_MODE             0x20000000   	/* TW: CRT1 or CRT2; determined by DISPTYPE_CRTx */
#define VB_DISPMODE_SINGLE	SINGLE_MODE  	/* TW: alias */
#define MIRROR_MODE		0x40000000   	/* TW: CRT1 + CRT2 identical (mirror mode) */
#define VB_DISPMODE_MIRROR	MIRROR_MODE  	/* TW: alias */
#define DUALVIEW_MODE		0x80000000   	/* TW: CRT1 + CRT2 independent (dual head mode) */
#define VB_DISPMODE_DUAL	DUALVIEW_MODE 	/* TW: alias */
#define DISPLAY_MODE            (SINGLE_MODE | MIRROR_MODE | DUALVIEW_MODE) /* TW */

/* TW: pSiS->VBLCDFlags */
#define VB_LCD_320x480		0x00000001	/* TW: DSTN/FSTN for 550 */
#define VB_LCD_640x480          0x00000002
#define VB_LCD_800x600          0x00000004
#define VB_LCD_1024x768         0x00000008
#define VB_LCD_1280x1024        0x00000010
#define VB_LCD_1280x960    	0x00000020
#define VB_LCD_1600x1200	0x00000040
#define VB_LCD_2048x1536	0x00000080
#define VB_LCD_1400x1050        0x00000100
#define VB_LCD_1152x864         0x00000200
#define VB_LCD_1152x768         0x00000400
#define VB_LCD_1280x768         0x00000800
#define VB_LCD_1024x600         0x00001000

/* TW: More or less useful macros (although we often use pSiS->VGAEngine instead) */
#define SIS_IS_300_CHIPSET    	(pSiS->Chipset == PCI_CHIP_SIS300) || \
	     		       	(pSiS->Chipset == PCI_CHIP_SIS630) || \
	     			(pSiS->Chipset == PCI_CHIP_SIS540) || \
				(pSiS->Chipset == PCI_CHIP_SIS730)

/* This preliminaryly also contains SIS330 */
#define SIS_IS_315_CHIPSET    	(pSiS->Chipset == PCI_CHIP_SIS315) || \
	     		       	(pSiS->Chipset == PCI_CHIP_SIS315H) || \
	     			(pSiS->Chipset == PCI_CHIP_SIS315PRO) || \
				(pSiS->Chipset == PCI_CHIP_SIS550) || \
				(pSiS->Chipset == PCI_CHIP_SIS650) || \
				(pSiS->Chipset == PCI_CHIP_SIS330)

/* SiS6326Flags */
#define SIS6326_HASTV		0x00000001
#define SIS6326_TVSVIDEO        0x00000002
#define SIS6326_TVCVBS          0x00000004
#define SIS6326_TVPAL		0x00000008
#define SIS6326_TVDETECTED      0x00000010
#define SIS6326_TVON            0x80000000

#define HW_DEVICE_EXTENSION	SIS_HW_DEVICE_INFO

#ifdef  DEBUG
#define PDEBUG(p)       p
#else
#define PDEBUG(p)
#endif

typedef unsigned long ULong;
typedef unsigned short UShort;
typedef unsigned char UChar;

/* TW: VGA engine types */
#define UNKNOWN_VGA 0
#define SIS_530_VGA 1
#define SIS_OLD_VGA 2
#define SIS_300_VGA 3
#define SIS_315_VGA 4

/* TW: oldChipset */
#define OC_UNKNOWN  0
#define OC_SIS6205A 3
#define OC_SIS6205B 4
#define OC_SIS82204 5
#define OC_SIS6205C 6
#define OC_SIS6225  7
#define OC_SIS5597  8
#define OC_SIS6326  9
#define OC_SIS530A  11
#define OC_SIS530B  12

/* TW: Chrontel type */
#define CHRONTEL_700x 0
#define CHRONTEL_701x 1

/* Flags650 */
#define SiS650_LARGEOVERLAY 0x00000001

/* TW: For backup of register contents */
typedef struct {
        unsigned char sisRegs3C4[0x50];
        unsigned char sisRegs3D4[0x90];
        unsigned char sisRegs3C2;
        unsigned char VBPart1[0x50];
        unsigned char VBPart2[0x50];
        unsigned char VBPart3[0x50];
        unsigned char VBPart4[0x50];
        unsigned short ch70xx[64];
	unsigned long sisMMIO85C0;    /* TW: Queue location for 310/325 series */
	unsigned char sis6326tv[0x46];
	unsigned long sisRegsPCI50, sisRegsPCIA0;
} SISRegRec, *SISRegPtr;

typedef struct _sisModeInfoPtr {
    int width;
    int height;
    int bpp;
    int n;
    struct _sisModeInfoPtr *next;
} sisModeInfoRec, *sisModeInfoPtr;

/* TW: SISFBLayout is mainly there because of DGA. It holds the
       current layout parameters needed for acceleration and other
       stuff. When switching mode using DGA, these are set up
       accordingly and not necessarily match pScrn's. Therefore,
       driver modules should read these values instead of pScrn's.
 */
typedef struct {
    int                bitsPerPixel;   	/* TW: Copy from pScrn->bitsPerPixel */
    int                depth;		/* TW: Copy from pScrn->depth */
    int                displayWidth;	/* TW: Copy from pScrn->displayWidth */
    DisplayModePtr     mode;		/* TW: Copy from pScrn->currentMode */
} SISFBLayout;

/* TW: Dual head private entity structure */
#ifdef SISDUALHEAD
typedef struct {
    ScrnInfoPtr         pScrn_1;
    ScrnInfoPtr         pScrn_2;
    unsigned char *     BIOS;
    SiS_Private   *     SiS_Pr;                 /* TW: For new mode switching code */
    int			CRT1ModeNo;		/* Current display mode for CRT1 */
    DisplayModePtr	CRT1DMode;		/* Current display mode for CRT1 */
    int 		CRT2ModeNo;		/* Current display mode for CRT2 */
    DisplayModePtr	CRT2DMode;		/* Current display mode for CRT2 */
    int			refCount;
    int 		lastInstance;		/* number of entities */
    Bool		DisableDual;		/* Emergency flag */
    Bool		ErrorAfterFirst;	/* Emergency flag: Error after first init -> Abort second */
    Bool		HWCursor;		/* Backup master settings for use on slave */
    Bool                TurboQueue;
    int                 ForceCRT2Type;
    int                 OptTVStand;
    int                 OptTVOver;
    int			OptTVSOver;
    int                 OptROMUsage;
    int                 PDC;
    Bool                NoAccel;
    Bool                NoXvideo;
    int			forceCRT1;
    int			DSTN;
    Bool		XvOnCRT2;
    int                 maxUsedClock;  		/* Max used pixelclock on master head */
    unsigned long       masterFbAddress;	/* Framebuffer addresses and sizes */
    unsigned long	masterFbSize;
    unsigned long       slaveFbAddress;
    unsigned long	slaveFbSize;
    unsigned char *     FbBase;         	/* VRAM linear address */
    unsigned char *     IOBase;         	/* MMIO linear address */
    unsigned short      MapCountIOBase;		/* map/unmap queue counter */
    unsigned short      MapCountFbBase;		/* map/unmap queue counter */
    Bool 		forceUnmapIOBase;	/* ignore counter and unmap */
    Bool		forceUnmapFbBase;	/* ignore counter and unmap */
#ifdef __alpha__
    unsigned char *     IOBaseDense;    	/* MMIO for Alpha platform */
    unsigned short      MapCountIOBaseDense;
    Bool		forceUnmapIOBaseDense;  /* ignore counter and unmap */
#endif
    int			chtvlumabandwidthcvbs;  /* TW: TV settings for Chrontel TV encoder */
    int			chtvlumabandwidthsvideo;
    int			chtvlumaflickerfilter;
    int			chtvchromabandwidth;
    int			chtvchromaflickerfilter;
    int			chtvcvbscolor;
    int			chtvtextenhance;
    int			chtvcontrast;
    int			sistvedgeenhance;	/* TW: TV settings for SiS bridge */
    int			sistvantiflicker;
    int			sistvsaturation;
    int			tvxpos;
    int			tvypos;
    int			ForceTVType;
    int			chtvtype;
    int                 NonDefaultPAL;
    unsigned short	tvx, tvy;
    unsigned char	p2_01, p2_02, p2_2d;
    unsigned short      cursorBufferNum;
    BOOLEAN		restorebyset;
} SISEntRec, *SISEntPtr;
#endif

#define SISPTR(p)       ((SISPtr)((p)->driverPrivate))
#define XAAPTR(p)       ((XAAInfoRecPtr)(SISPTR(p)->AccelInfoPtr))

typedef struct {
    ScrnInfoPtr         pScrn;		/* -------------- DON'T INSERT ANYTHING HERE --------------- */
    pciVideoPtr         PciInfo;	/* -------- OTHERWISE sis_dri.so MUST BE RECOMPILED -------- */
    PCITAG              PciTag;
    EntityInfoPtr       pEnt;
    int                 Chipset;
    int                 ChipRev;
    int			VGAEngine;      /* TW: see above */
    int	                hasTwoOverlays; /* TW: Chipset supports two video overlays? */
    HW_DEVICE_EXTENSION sishw_ext;      /* TW: For new mode switching code */
    SiS_Private   *     SiS_Pr;         /* TW: For new mode switching code */
    int			DSTN; 		/* TW: For 550 FSTN/DSTN; set by option, no detection */
    unsigned long       FbAddress;      /* VRAM physical address (in DHM: for each Fb!) */
    unsigned long       realFbAddress;  /* For DHM/PCI mem mapping: store global FBAddress */
    unsigned char *     FbBase;         /* VRAM virtual linear address */
    CARD32              IOAddress;      /* MMIO physical address */
    unsigned char *     IOBase;         /* MMIO linear address */
    IOADDRESS           IODBase;        /* Base of PIO memory area */
#ifdef __alpha__
    unsigned char *     IOBaseDense;    /* MMIO for Alpha platform */
#endif
    CARD16              RelIO;          /* Relocated IO Ports baseaddress */
    unsigned char *     BIOS;
    int                 MemClock;
    int                 BusWidth;
    int                 MinClock;
    int                 MaxClock;
    int                 Flags;          /* HW config flags */
    long                FbMapSize;	/* Used for Mem Mapping - DON'T CHANGE THIS */
    long                availMem;       /* Really available Fb mem (minus TQ, HWCursor) */
    unsigned long	maxxfbmem;      /* limit fb memory X is to use to this (KB) */
    unsigned long       sisfbMem;       /* heapstart of sisfb (if running) */
#ifdef SISDUALHEAD
    unsigned long	dhmOffset;	/* Offset to memory for each head (0 or ..) */
#endif
    DGAModePtr          DGAModes;
    int                 numDGAModes;
    Bool                DGAactive;
    int                 DGAViewportStatus;
    int                 OldMode;        /* TW: Back old modeNo (if available) */
    Bool                NoAccel;
    Bool                NoXvideo;
    Bool		XvOnCRT2;       /* TW: see sis_opt.c */
    Bool                HWCursor;
    Bool                UsePCIRetry;
    Bool                TurboQueue;
    int			VESA;
    int                 ForceCRT2Type;
    int                 OptTVStand;
    int                 OptTVOver;
    int                 OptROMUsage;
    int                 UseCHOverScan;
    Bool                ValidWidth;
    Bool                FastVram;		/* TW: now unused */
    int			forceCRT1;
    Bool		CRT1changed;
    unsigned char       oldCR17;
    unsigned char       oldCR32;
    unsigned char       newCR32;
    int                 VBFlags;		/* TW: Video bridge configuration */
    int                 VBFlags_backup;         /* TW: Backup for SlaveMode-modes */
    int                 VBLCDFlags;             /* TW: Moved LCD panel size bits here */
    int                 ChrontelType;           /* TW: CHRONTEL_700x or CHRONTEL_701x */
    int                 PDC;			/* TW: PanelDelayCompensation */
    short               scrnOffset;		/* TW: Screen pitch (data) */
    short               scrnPitch;		/* TW: Screen pitch (display; regarding interlace) */
    short               DstColor;
    int                 xcurrent;               /* for temp use in accel */
    int                 ycurrent;               /* for temp use in accel */
    long		SiS310_AccelDepth; 	/* used in accel for 310/325 series */
    int                 Xdirection;  		/* for temp use in accel */
    int                 Ydirection;  		/* for temp use in accel */
    int                 sisPatternReg[4];
    int                 ROPReg;
    int                 CommandReg;
    int                 MaxCMDQueueLen;
    int                 CurCMDQueueLen;
    int                 MinCMDQueueLen;
    CARD16		CursorSize;  		/* TW: Size of HWCursor area (bytes) */
    CARD32		cursorOffset;		/* TW: see sis_driver.c and sis_cursor.c */
    int                 DstX;
    int                 DstY;
    unsigned char *     XAAScanlineColorExpandBuffers[2];
    CARD32              AccelFlags;
    Bool                ClipEnabled;
    Bool                DoColorExpand;
    SISRegRec           SavedReg;
    SISRegRec           ModeReg;
    xf86CursorInfoPtr   CursorInfoPtr;
    XAAInfoRecPtr       AccelInfoPtr;
    CloseScreenProcPtr  CloseScreen;
    unsigned int        (*ddc1Read)(ScrnInfoPtr);
    Bool        	(*ModeInit)(ScrnInfoPtr pScrn, DisplayModePtr mode);
    void        	(*SiSSave)(ScrnInfoPtr pScrn, SISRegPtr sisreg);
    void        	(*SiSSave2)(ScrnInfoPtr pScrn, SISRegPtr sisreg);
    void        	(*SiSSave3)(ScrnInfoPtr pScrn, SISRegPtr sisreg);
    void        	(*SiSSaveLVDSChrontel)(ScrnInfoPtr pScrn, SISRegPtr sisreg);
    void        	(*SiSRestore)(ScrnInfoPtr pScrn, SISRegPtr sisreg);
    void        	(*SiSRestore2)(ScrnInfoPtr pScrn, SISRegPtr sisreg);
    void        	(*SiSRestore3)(ScrnInfoPtr pScrn, SISRegPtr sisreg);
    void        	(*SiSRestoreLVDSChrontel)(ScrnInfoPtr pScrn, SISRegPtr sisreg);
    void        	(*SetThreshold)(ScrnInfoPtr pScrn, DisplayModePtr mode,
                                unsigned short *Low, unsigned short *High);
    void        	(*LoadCRT2Palette)(ScrnInfoPtr pScrn, int numColors,
                		int *indicies, LOCO *colors, VisualPtr pVisual);

    int                 cmdQueueLen;		/* TW: Current cmdQueueLength (for 2D and 3D) */
    int	 		*cmdQueueLenPtr;
    unsigned long 	agpHandle;
    CARD32 		agpAddr;
    unsigned char 	*agpBase;
    unsigned int 	agpSize;
    CARD32 		agpCmdBufAddr;
    unsigned char 	*agpCmdBufBase;
    unsigned int 	agpCmdBufSize;
    unsigned int 	agpCmdBufFree;
    Bool 		irqEnabled;
    int 		irq;

    int 		ColorExpandRingHead;
    int 		ColorExpandRingTail;
    int 		PerColorExpandBufferSize;
    int 		ColorExpandBufferNumber;
    int 		ColorExpandBufferCountMask;
    unsigned char 	*ColorExpandBufferAddr[32];
    int 		ColorExpandBufferScreenOffset[32];
    int 		ImageWriteBufferSize;
    unsigned char 	*ImageWriteBufferAddr;

    int 		Rotate;
    void        	(*PointerMoved)(int index, int x, int y);

    /* ShadowFB support */
    Bool 		ShadowFB;
    unsigned char 	*ShadowPtr;
    int  		ShadowPitch;

#ifdef XF86DRI
    Bool 		directRenderingEnabled;
    DRIInfoPtr 		pDRIInfo;
    int 		drmSubFD;
    int 		numVisualConfigs;
    __GLXvisualConfig* 	pVisualConfigs;
    SISConfigPrivPtr 	pVisualConfigsPriv;
    SISRegRec 		DRContextRegs;
#endif

    XF86VideoAdaptorPtr adaptor;
    ScreenBlockHandlerProcPtr BlockHandler;
    void                (*VideoTimerCallback)(ScrnInfoPtr, Time);

    OptionInfoPtr 	Options;
    unsigned char 	LCDon;
#ifdef SISDUALHEAD
    Bool		BlankCRT1, BlankCRT2;
#endif
    Bool 		Blank;
    unsigned char 	BIOSModeSave;
    int 		CRT1off;		/* TW: 1=CRT1 off, 0=CRT1 on */
    CARD16 		LCDheight;		/* TW: Vertical resolution of LCD panel */
    CARD16 		LCDwidth;		/* TW: Horizontal resolution of LCD panel */
    vbeInfoPtr 		pVbe;			/* TW: For VESA mode switching */
    CARD16 		vesamajor;
    CARD16 		vesaminor;
    VbeInfoBlock 	*vbeInfo;
    int 		UseVESA;
    sisModeInfoPtr      SISVESAModeList;
    xf86MonPtr 		monitor;
    CARD16 		maxBytesPerScanline;
    CARD32 		*pal, *savedPal;
    int 		mapPhys, mapOff, mapSize;
    int 		statePage, stateSize, stateMode;
    CARD8 		*fonts;
    CARD8 		*state, *pstate;
    void 		*base, *VGAbase;
#ifdef SISDUALHEAD
    BOOL 		DualHeadMode;		/* TW: TRUE if we use dual head mode */
    BOOL 		SecondHead;		/* TW: TRUE is this is the second head */
    SISEntPtr 		entityPrivate;		/* TW: Ptr to private entity (see above) */
    BOOL		SiSXinerama;		/* TW: Do we use Xinerama mode? */
#endif
    SISFBLayout         CurrentLayout;		/* TW: Current framebuffer layout */
    Bool                (*i2cInit)(ScrnInfoPtr);/* I2C stuff */
    I2CBusPtr           I2C;
    USHORT              SiS_DDC2_Index;
    USHORT              SiS_DDC2_Data;
    USHORT              SiS_DDC2_Clk;
    BOOL		Primary;		/* TW: Display adapter is primary */
    xf86Int10InfoPtr    pInt;			/* TW: Our int10 */
    int                 oldChipset;		/* TW: Type of old chipset */
    CARD32              RealVideoRam;		/* TW: 6326 can only address 4MB, but TQ can be above */
    CARD32              CmdQueLenMask;		/* TW: Mask of queue length in MMIO register */
    CARD32              CmdQueLenFix;           /* TW: Fix value to subtract from QueLen (530/620) */
    CARD32              CmdQueMaxLen;           /* TW: (6326/5597/5598) Amount of cmds the queue can hold */
    CARD32              TurboQueueLen;		/* TW: For future use */
    CARD32              detectedCRT2Devices;	/* TW: detected CRT2 devices before mask-out */
    Bool                NoHostBus;		/* TW: Enable/disable 5597/5598 host bus */
    Bool		noInternalModes;	/* TW: Use our own default modes? */
    char *              sbiosn;			/* TW: For debug */
    int			OptUseOEM;		/* TW: Use internal OEM data? */
    int			chtvlumabandwidthcvbs;  /* TW: TV settings for Chrontel TV encoder */
    int			chtvlumabandwidthsvideo;
    int			chtvlumaflickerfilter;
    int			chtvchromabandwidth;
    int			chtvchromaflickerfilter;
    int			chtvcvbscolor;
    int			chtvtextenhance;
    int			chtvcontrast;
    int			sistvedgeenhance;	/* TW: TV settings for SiS bridges */
    int			sistvantiflicker;
    int			sistvsaturation;
    int			OptTVSOver;		/* TW: Chrontel 7005: Superoverscan */
    int			tvxpos;
    int			tvypos;
    int			SiS6326Flags;		/* TW: SiS6326 TV settings */
    int			sis6326antiflicker;
    int			sis6326enableyfilter;
    int			sis6326yfilterstrong;
    BOOL		donttrustpdc;		/* TW: Don't trust the detected PDC */
    unsigned char	sisfbpdc;
    int			NoYV12;			/* TW: Disable Xv YV12 support (old series) */
    unsigned char       postVBCR32;
    int			newFastVram;		/* TW: Replaces FastVram */
    int			ForceTVType;
    int                 NonDefaultPAL;
    unsigned long       lockcalls;		/* TW: Count unlock calls for debug */
    unsigned short	tvx, tvy;		/* TW: Backup TV position registers */
    unsigned char	p2_01, p2_02, p2_2d;    /* TW: Backup TV position registers */
    unsigned short      tvx1, tvx2, tvx3, tvy1; /* TW: Backup TV position registers */
    BOOLEAN		ForceCursorOff;	
    BOOLEAN		HaveCustomModes;	
    BOOLEAN		IsCustom;
    DisplayModePtr	backupmodelist;
    int			chtvtype;
    Atom                xvBrightness, xvContrast, xvColorKey, xvHue, xvSaturation;
    Atom                xvAutopaintColorKey, xvSetDefaults;
    unsigned long       Flags650;
    BOOLEAN		UseHWARGBCursor;
    int			OptUseColorCursor;
    int			OptUseColorCursorBlend;
    CARD32		OptColorCursorBlendThreshold;
    unsigned short      cursorBufferNum;
    BOOLEAN		restorebyset;
} SISRec, *SISPtr;

typedef struct _ModeInfoData {
    int mode;
    VbeModeInfoBlock *data;
    VbeCRTCInfoBlock *block;
} ModeInfoData;

typedef struct _myhddctiming {
    int           whichone;
    unsigned char mask;
    float         rate;
} myhddctiming;

typedef struct _myvddctiming {
    int           whichone;
    unsigned char mask;
    int           rate;
} myvddctiming;

typedef struct _myddcstdmodes {
    int hsize;
    int vsize;
    int refresh;
    float hsync;
} myddcstdmodes;

typedef struct _pdctable {
    int subsysVendor;
    int subsysCard;
    int pdc;
    char *vendorName;
    char *cardName;
} pdctable;

typedef struct _chswtable {
    int subsysVendor;
    int subsysCard;
    char *vendorName;
    char *cardName;
} chswtable;

extern void  sisSaveUnlockExtRegisterLock(SISPtr pSiS, unsigned char *reg1, unsigned char *reg2);
extern void  sisRestoreExtRegisterLock(SISPtr pSiS, unsigned char reg1, unsigned char reg2);
extern void  SiSOptions(ScrnInfoPtr pScrn);
extern const OptionInfoRec * SISAvailableOptions(int chipid, int busid);
extern void  SiSSetup(ScrnInfoPtr pScrn);
extern void  SISVGAPreInit(ScrnInfoPtr pScrn);
extern Bool  SiSAccelInit(ScreenPtr pScreen);
extern Bool  SiS300AccelInit(ScreenPtr pScreen);
extern Bool  SiS310AccelInit(ScreenPtr pScreen);
extern Bool  SiS530AccelInit(ScreenPtr pScreen);
extern Bool  SiSHWCursorInit(ScreenPtr pScreen);
extern Bool  SISDGAInit(ScreenPtr pScreen);
extern void  SISInitVideo(ScreenPtr pScreen);
extern void  SIS6326InitVideo(ScreenPtr pScreen);

extern void  SiS_SetCHTVlumabandwidthcvbs(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetCHTVlumabandwidthsvideo(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetCHTVlumaflickerfilter(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetCHTVchromabandwidth(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetCHTVchromaflickerfilter(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetCHTVcvbscolor(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetCHTVtextenhance(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetCHTVcontrast(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetSISTVedgeenhance(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetSISTVantiflicker(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetSISTVsaturation(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetSIS6326TVantiflicker(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetSIS6326TVenableyfilter(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetSIS6326TVyfilterstrong(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetTVxposoffset(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetTVyposoffset(ScrnInfoPtr pScrn, int val);
extern int   SiS_GetCHTVlumabandwidthcvbs(ScrnInfoPtr pScrn);
extern int   SiS_GetCHTVlumabandwidthsvideo(ScrnInfoPtr pScrn);
extern int   SiS_GetCHTVlumaflickerfilter(ScrnInfoPtr pScrn);
extern int   SiS_GetCHTVchromabandwidth(ScrnInfoPtr pScrn);
extern int   SiS_GetCHTVchromaflickerfilter(ScrnInfoPtr pScrn);
extern int   SiS_GetCHTVcvbscolor(ScrnInfoPtr pScrn);
extern int   SiS_GetCHTVtextenhance(ScrnInfoPtr pScrn);
extern int   SiS_GetCHTVcontrast(ScrnInfoPtr pScrn);
extern int   SiS_GetSISTVedgeenhance(ScrnInfoPtr pScrn);
extern int   SiS_GetSISTVantiflicker(ScrnInfoPtr pScrn);
extern int   SiS_GetSISTVsaturation(ScrnInfoPtr pScrn);
extern int   SiS_GetSIS6326TVantiflicker(ScrnInfoPtr pScrn);
extern int   SiS_GetSIS6326TVenableyfilter(ScrnInfoPtr pScrn);
extern int   SiS_GetSIS6326TVyfilterstrong(ScrnInfoPtr pScrn);
extern int   SiS_GetTVxposoffset(ScrnInfoPtr pScrn);
extern int   SiS_GetTVyposoffset(ScrnInfoPtr pScrn);
#endif
