/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/mga/mga.h,v 1.85 2002/12/16 16:19:17 dawes Exp $ */
/*
 * MGA Millennium (MGA2064W) functions
 *
 * Copyright 1996 The XFree86 Project, Inc.
 *
 * Authors
 *		Dirk Hohndel
 *			hohndel@XFree86.Org
 *		David Dawes
 *			dawes@XFree86.Org
 */

#ifndef MGA_H
#define MGA_H

#include "compiler.h"
#include "xaa.h"
#include "xf86Cursor.h"
#include "vgaHW.h"
#include "colormapst.h"
#include "xf86DDC.h"
#include "xf86xv.h"



#ifdef XF86DRI
#include "xf86drm.h"


#define _XF86DRI_SERVER_
#include "mga_dripriv.h"
#include "dri.h"
#include "GL/glxint.h"




#include "xf86dri.h"
#include "dri.h"



#include "GL/glxint.h"
#include "mga_dri.h"
#endif



#ifdef USEMGAHAL
#include "client.h"
#endif
#include "mga_bios.h"

typedef enum {
    OPTION_SW_CURSOR,
    OPTION_HW_CURSOR,
    OPTION_PCI_RETRY,
    OPTION_SYNC_ON_GREEN,
    OPTION_NOACCEL,
    OPTION_SHOWCACHE,
    OPTION_OVERLAY,
    OPTION_MGA_SDRAM,
    OPTION_SHADOW_FB,
    OPTION_FBDEV,
    OPTION_COLOR_KEY,
    OPTION_SET_MCLK,
    OPTION_OVERCLOCK_MEM,
    OPTION_VIDEO_KEY,
    OPTION_ROTATE,
    OPTION_TEXTURED_VIDEO,
    OPTION_CRTC2HALF,
    OPTION_CRTC2RAM,
    OPTION_INT10,
    OPTION_AGP_MODE,
    OPTION_AGP_SIZE,
    OPTION_DIGITAL1,
    OPTION_DIGITAL2,
    OPTION_TV,
    OPTION_TVSTANDARD,
    OPTION_CABLETYPE,
    OPTION_USEIRQZERO,
    OPTION_NOHAL,
    OPTION_SWAPPED_HEAD,
    OPTION_DRI,
    OPTION_MERGEDFB,
    OPTION_HSYNC2,
    OPTION_VREFRESH2,
    OPTION_MONITOR2POS,
    OPTION_METAMODES
} MGAOpts;


#if !defined(EXTRADEBUG)
#define INREG8(addr) MMIO_IN8(pMga->IOBase, addr)
#define INREG16(addr) MMIO_IN16(pMga->IOBase, addr)
#define INREG(addr) MMIO_IN32(pMga->IOBase, addr)
#define OUTREG8(addr, val) MMIO_OUT8(pMga->IOBase, addr, val)
#define OUTREG16(addr, val) MMIO_OUT16(pMga->IOBase, addr, val)
#define OUTREG(addr, val) MMIO_OUT32(pMga->IOBase, addr, val)
#else /* !EXTRADEBUG */
CARD8 dbg_inreg8(ScrnInfoPtr,int,int);
CARD16 dbg_inreg16(ScrnInfoPtr,int,int);
CARD32 dbg_inreg32(ScrnInfoPtr,int,int);
void dbg_outreg8(ScrnInfoPtr,int,int);
void dbg_outreg16(ScrnInfoPtr,int,int);
void dbg_outreg32(ScrnInfoPtr,int,int);
#define INREG8(addr) dbg_inreg8(pScrn,addr,1)
#define INREG16(addr) dbg_inreg16(pScrn,addr,1)
#define INREG(addr) dbg_inreg32(pScrn,addr,1)
#define OUTREG8(addr,val) dbg_outreg8(pScrn,addr,val)
#define OUTREG16(addr,val) dbg_outreg16(pScrn,addr,val)
#define OUTREG(addr,val) dbg_outreg32(pScrn,addr,val)
#endif /* EXTRADEBUG */

/*
 * Read/write to the DAC via MMIO 
 */

/*
 * These were functions.  Use macros instead to avoid the need to
 * pass pMga to them.
 */

#define inMGAdreg(reg) INREG8(RAMDAC_OFFSET + (reg))

#define outMGAdreg(reg, val) OUTREG8(RAMDAC_OFFSET + (reg), val)

#define inMGAdac(reg) \
	(outMGAdreg(MGA1064_INDEX, reg), inMGAdreg(MGA1064_DATA))

#define outMGAdac(reg, val) \
	(outMGAdreg(MGA1064_INDEX, reg), outMGAdreg(MGA1064_DATA, val))

#define outMGAdacmsk(reg, mask, val) \
	do { /* note: mask and reg may get evaluated twice */ \
	    unsigned char tmp = (mask) ? (inMGAdac(reg) & (mask)) : 0; \
	    outMGAdreg(MGA1064_INDEX, reg); \
	    outMGAdreg(MGA1064_DATA, tmp | (val)); \
	} while (0)

#define PORT_OFFSET 	(0x1F00 - 0x300)

#define MGA_VERSION 4000
#define MGA_NAME "MGA"
#define MGA_C_NAME MGA
#define MGA_MODULE_DATA mgaModuleData
#define MGA_DRIVER_NAME "mga"
#define MGA_MAJOR_VERSION 1
#define MGA_MINOR_VERSION 1
#define MGA_PATCHLEVEL 0

typedef struct {
    unsigned char	ExtVga[6];
    unsigned char 	DacClk[6];
    unsigned char *     DacRegs;
    unsigned long	crtc2[0x58];
    unsigned char	dac2[0x21];
    CARD32		Option;
    CARD32		Option2;
    CARD32		Option3;
    long                Clock;
    Bool                PIXPLLCSaved;
} MGARegRec, *MGARegPtr;

/* For programming the second CRTC */
typedef struct {
   CARD32   ulDispWidth;        /* Display Width in pixels*/
   CARD32   ulDispHeight;       /* Display Height in pixels*/
   CARD32   ulBpp;              /* Bits Per Pixels / input format*/
   CARD32   ulPixClock;         /* Pixel Clock in kHz*/
   CARD32   ulHFPorch;          /* Horizontal front porch in pixels*/
   CARD32   ulHSync;            /* Horizontal Sync in pixels*/
   CARD32   ulHBPorch;          /* Horizontal back porch in pixels*/
   CARD32   ulVFPorch;          /* Vertical front porch in lines*/
   CARD32   ulVSync;            /* Vertical Sync in lines*/
   CARD32   ulVBPorch;          /* Vertical back Porch in lines*/
   CARD32   ulFBPitch;          /* Pitch*/
   CARD32   flSignalMode;       /* Signal Mode*/
} xMODEINFO;


typedef struct {
   int          brightness;
   int          contrast;
   Bool         doubleBuffer;
   unsigned char currentBuffer;
   FBLinearPtr	linear;
   RegionRec	clip;
   CARD32	colorKey;
   CARD32	videoStatus;
   Time		offTime;
   Time		freeTime;
   int		lastPort;
} MGAPortPrivRec, *MGAPortPrivPtr;

typedef struct {
    Bool	isHwCursor;
    int		CursorMaxWidth;
    int 	CursorMaxHeight;
    int		CursorFlags;
    int		CursorOffscreenMemSize;
    Bool	(*UseHWCursor)(ScreenPtr, CursorPtr);
    void	(*LoadCursorImage)(ScrnInfoPtr, unsigned char*);
    void	(*ShowCursor)(ScrnInfoPtr);
    void	(*HideCursor)(ScrnInfoPtr);
    void	(*SetCursorPosition)(ScrnInfoPtr, int, int);
    void	(*SetCursorColors)(ScrnInfoPtr, int, int);
    long	maxPixelClock;
    long	MemoryClock;
    MessageType ClockFrom;
    MessageType MemClkFrom;
    Bool	SetMemClk;
    void	(*LoadPalette)(ScrnInfoPtr, int, int*, LOCO*, VisualPtr);
    void	(*PreInit)(ScrnInfoPtr);
    void	(*Save)(ScrnInfoPtr, vgaRegPtr, MGARegPtr, Bool);
    void	(*Restore)(ScrnInfoPtr, vgaRegPtr, MGARegPtr, Bool);
    Bool	(*ModeInit)(ScrnInfoPtr, DisplayModePtr);
} MGARamdacRec, *MGARamdacPtr;


typedef struct {
    int bitsPerPixel;
    int depth;
    int displayWidth;
    rgb weight;
    Bool Overlay8Plus24;
    DisplayModePtr mode;
} MGAFBLayout;

/* Card-specific driver information */

typedef struct {
    Bool update;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
} MGAPaletteInfo;

#define MGAPTR(p) ((MGAPtr)((p)->driverPrivate))

/*avoids segfault by returning false if pMgaHwInfo not defined*/
#define ISDIGITAL1(p) (p->pMgaHwInfo && ((p->pMgaHwInfo->ulCapsFirstOutput) & MGAHWINFOCAPS_OUTPUT_DIGITAL))
#define ISDIGITAL2(p) (p->pMgaHwInfo && ((p->pMgaHwInfo->ulCapsSecondOutput) & MGAHWINFOCAPS_OUTPUT_DIGITAL))
#define ISTV1(p) (p->pMgaHwInfo && ((p->pMgaHwInfo->ulCapsFirstOutput) & MGAHWINFOCAPS_OUTPUT_TV))
#define ISTV2(p) (p->pMgaHwInfo && ((p->pMgaHwInfo->ulCapsSecondOutput) & MGAHWINFOCAPS_OUTPUT_TV))

#ifdef DISABLE_VGA_IO
typedef struct mgaSave {
    pciVideoPtr pvp;
    Bool enable;
} MgaSave, *MgaSavePtr;
#endif


typedef enum {
    mgaLeftOf,
    mgaRightOf,
    mgaAbove,
    mgaBelow,
    mgaClone
} MgaScrn2Rel;

typedef struct {
    int			lastInstance;
#ifdef USEMGAHAL
    LPCLIENTDATA	pClientStruct;
    LPBOARDHANDLE	pBoard;
    LPMGAHWINFO		pMgaHwInfo;
#endif
    int			refCount;
    CARD32		masterFbAddress;
    long		masterFbMapSize;
    CARD32		slaveFbAddress;
    long		slaveFbMapSize;
    int			mastervideoRam;
    int			slavevideoRam;
    Bool		directRenderingEnabled;
    ScrnInfoPtr 	pScrn_1;
    ScrnInfoPtr 	pScrn_2;
} MGAEntRec, *MGAEntPtr;

typedef struct {
#ifdef USEMGAHAL
    LPCLIENTDATA	pClientStruct;
    LPBOARDHANDLE	pBoard;
    LPMGAMODEINFO	pMgaModeInfo;
    LPMGAHWINFO		pMgaHwInfo;
#endif
    EntityInfoPtr	pEnt;
    MGABiosInfo		Bios;
    MGABios2Info	Bios2;
    CARD8               BiosOutputMode;
    pciVideoPtr		PciInfo;
    PCITAG		PciTag;
    xf86AccessRec	Access;
    int			Chipset;
    int                 ChipRev;
    Bool		Primary;
    Bool		Interleave;
    int			HwBpp;
    int			Roundings[4];
    int			BppShifts[4];
    Bool		HasFBitBlt;
    Bool		OverclockMem;
    int			YDstOrg;
    int			DstOrg;
    int			SrcOrg;
    unsigned long	IOAddress;
    unsigned long	FbAddress;
    unsigned long	ILOADAddress;
    int			FbBaseReg;
    unsigned long	BiosAddress;
    MessageType		BiosFrom;
    unsigned char *     IOBase;
    unsigned char *	FbBase;
    unsigned char *	ILOADBase;
    unsigned char *	FbStart;
    long		FbMapSize;
    long		FbUsableSize;
    long		FbCursorOffset;
    MGARamdacRec	Dac;
    Bool		HasSDRAM;
    Bool		NoAccel;
    Bool		SyncOnGreen;
    Bool		Dac6Bit;
    Bool		HWCursor;
    Bool		UsePCIRetry;
    Bool		ShowCache;
    Bool		Overlay8Plus24;
    Bool		ShadowFB;
    unsigned char *	ShadowPtr;
    int			ShadowPitch;
    int			MemClk;
    int			MinClock;
    int			MaxClock;
    MGARegRec		SavedReg;
    MGARegRec		ModeReg;
    int			MaxFastBlitY;
    CARD32		BltScanDirection;
    CARD32		FilledRectCMD;
    CARD32		SolidLineCMD;
    CARD32		PatternRectCMD;
    CARD32		DashCMD;
    CARD32		NiceDashCMD;
    CARD32		AccelFlags;
    CARD32		PlaneMask;
    CARD32		FgColor;
    CARD32		BgColor;
    CARD32		MAccess;
    int			FifoSize;
    int			StyleLen;
    XAAInfoRecPtr	AccelInfoRec;
    xf86CursorInfoPtr	CursorInfoRec;
    DGAModePtr		DGAModes;
    int			numDGAModes;
    Bool		DGAactive;
    int			DGAViewportStatus;
    CARD32		*Atype;
    CARD32		*AtypeNoBLK;
    void		(*PreInit)(ScrnInfoPtr pScrn);
    void		(*Save)(ScrnInfoPtr, vgaRegPtr, MGARegPtr, Bool);
    void		(*Restore)(ScrnInfoPtr, vgaRegPtr, MGARegPtr, Bool);
    Bool		(*ModeInit)(ScrnInfoPtr, DisplayModePtr);
    void		(*PointerMoved)(int index, int x, int y);
    CloseScreenProcPtr	CloseScreen;
    ScreenBlockHandlerProcPtr BlockHandler;
    unsigned int	(*ddc1Read)(ScrnInfoPtr);
    void (*DDC1SetSpeed)(ScrnInfoPtr, xf86ddcSpeed);
    Bool		(*i2cInit)(ScrnInfoPtr);
    I2CBusPtr		I2C;
    Bool		FBDev;
    int			colorKey;
    int			videoKey;
    int			fifoCount;
    int			Rotate;
    MGAFBLayout		CurrentLayout;
    Bool		DrawTransparent;
    int			MaxBlitDWORDS;
    Bool		TexturedVideo;
    MGAPortPrivPtr	portPrivate;
    unsigned char	*ScratchBuffer;
    unsigned char	*ColorExpandBase;
    int			expandRows;
    int			expandDWORDs;
    int			expandRemaining;
    int			expandHeight;
    int			expandY;
#ifdef XF86DRI
    Bool 		directRenderingEnabled;
    DRIInfoPtr 		pDRIInfo;
    int 		drmFD;
    int 		numVisualConfigs;
    __GLXvisualConfig*	pVisualConfigs;
    MGAConfigPrivPtr 	pVisualConfigsPriv;
    MGADRIServerPrivatePtr DRIServerInfo;

    MGARegRec		DRContextRegs;

    Bool		haveQuiescense;
    void		(*GetQuiescence)(ScrnInfoPtr pScrn);

    int 		agpMode;
    int                 agpSize;

    int                 irq;
    CARD32              reg_ien;
#endif
    XF86VideoAdaptorPtr adaptor;
    Bool		DualHeadEnabled;
    Bool		SecondCrtc;
    Bool                SecondOutput;
    GDevPtr		device;
    /* The hardware's real SrcOrg */
    int			realSrcOrg;
    MGAEntPtr		entityPrivate;
    void		(*SetupForSolidFill)(ScrnInfoPtr pScrn, int color,
					     int rop, unsigned int planemask);
    void		(*SubsequentSolidFillRect)(ScrnInfoPtr pScrn,
					     int x, int y, int w, int h);
    void		(*RestoreAccelState)(ScrnInfoPtr pScrn);
    int			allowedWidth;
    void		(*VideoTimerCallback)(ScrnInfoPtr, Time);
    void		(*PaletteLoadCallback)(ScrnInfoPtr);
    void		(*RenderCallback)(ScrnInfoPtr);
    Time		RenderTime;
    MGAPaletteInfo	palinfo[256];  /* G400 hardware bug workaround */
    FBLinearPtr		LinearScratch;
    Bool                softbooted;
#ifdef USEMGAHAL
    Bool                HALLoaded;
#endif
    OptionInfoPtr	Options;
/* Merged Framebuffer data */
    Bool                MergedFB;

    /* Real values specific to monitor1, since the original ones are replaced */
    DisplayModePtr	M1modes;	 /* list of actual modes */
    DisplayModePtr	M1currentMode; /* current mode */
    int			M1frameX0;	/* viewport position */
    int			M1frameY0;
    int			M1frameX1;
    int			M1frameY1;
    
    ScrnInfoPtr       pScrn2; /*pointer to second CRTC screeninforec,
                                       if in merged mode */
/* End of Merged Framebuffer Data */
  int			HALGranularityOffX, HALGranularityOffY;
} MGARec, *MGAPtr;

extern CARD32 MGAAtype[16];
extern CARD32 MGAAtypeNoBLK[16];

#define USE_RECTS_FOR_LINES	0x00000001
#define FASTBLT_BUG		0x00000002
#define CLIPPER_ON		0x00000004
#define BLK_OPAQUE_EXPANSION	0x00000008
#define TRANSC_SOLID_FILL	0x00000010
#define	NICE_DASH_PATTERN	0x00000020
#define	TWO_PASS_COLOR_EXPAND	0x00000040
#define	MGA_NO_PLANEMASK	0x00000080
#define USE_LINEAR_EXPANSION	0x00000100
#define LARGE_ADDRESSES		0x00000200

#define MGAIOMAPSIZE		0x00004000
#define MGAILOADMAPSIZE		0x00400000

#define TRANSPARENCY_KEY	255
#define KEY_COLOR		0


/* Prototypes */

void MGAAdjustFrame(int scrnIndex, int x, int y, int flags);
Bool MGASwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
void MGAFillModeInfoStruct(ScrnInfoPtr pScrn, DisplayModePtr mode);
Bool MGAGetRec(ScrnInfoPtr pScrn);
void MGAProbeDDC(ScrnInfoPtr pScrn, int index);
void MGASoftReset(ScrnInfoPtr pScrn);
void MGAFreeRec(ScrnInfoPtr pScrn);
void MGAReadBios(ScrnInfoPtr pScrn);
void MGADisplayPowerManagementSet(ScrnInfoPtr pScrn, int PowerManagementMode,
				  int flags);
void MGAAdjustFrameCrtc2(int scrnIndex, int x, int y, int flags);
void MGADisplayPowerManagementSetCrtc2(ScrnInfoPtr pScrn,
					     int PowerManagementMode,
					     int flags);
void MGAAdjustGranularity(ScrnInfoPtr pScrn, int* x, int* y); 


void MGA2064SetupFuncs(ScrnInfoPtr pScrn);
void MGAGSetupFuncs(ScrnInfoPtr pScrn);

void MGAStormSync(ScrnInfoPtr pScrn);
void MGAStormEngineInit(ScrnInfoPtr pScrn);
Bool MGAStormAccelInit(ScreenPtr pScreen);
Bool MGAHWCursorInit(ScreenPtr pScreen);

Bool Mga8AccelInit(ScreenPtr pScreen);
Bool Mga16AccelInit(ScreenPtr pScreen);
Bool Mga24AccelInit(ScreenPtr pScreen);
Bool Mga32AccelInit(ScreenPtr pScreen);

void Mga8InitSolidFillRectFuncs(MGAPtr pMga);
void Mga16InitSolidFillRectFuncs(MGAPtr pMga);
void Mga24InitSolidFillRectFuncs(MGAPtr pMga);
void Mga32InitSolidFillRectFuncs(MGAPtr pMga);

void MGAPolyArcThinSolid(DrawablePtr, GCPtr, int, xArc*);

Bool MGADGAInit(ScreenPtr pScreen);

void MGARefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void MGARefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void MGARefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void MGARefreshArea24(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void MGARefreshArea32(ScrnInfoPtr pScrn, int num, BoxPtr pbox);

void Mga8SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir,
				int ydir, int rop, unsigned int planemask,
				int trans);
void Mga16SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir,
				int ydir, int rop, unsigned int planemask,
				int trans);
void Mga24SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir,
				int ydir, int rop, unsigned int planemask,
				int trans);
void Mga32SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir,
				int ydir, int rop, unsigned int planemask,
				int trans);

void Mga8SetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
				unsigned int planemask);
void Mga16SetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
				unsigned int planemask);
void Mga24SetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
				unsigned int planemask);
void Mga32SetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
				unsigned int planemask);

void MGAPointerMoved(int index, int x, int y);

void MGAInitVideo(ScreenPtr pScreen);
void MGAResetVideo(ScrnInfoPtr pScrn);

#ifdef XF86DRI

#define MGA_FRONT	0x1
#define MGA_BACK	0x2
#define MGA_DEPTH	0x4

Bool MGADRIScreenInit( ScreenPtr pScreen );
void MGADRICloseScreen( ScreenPtr pScreen );
Bool MGADRIFinishScreenInit( ScreenPtr pScreen );

Bool MGALockUpdate( ScrnInfoPtr pScrn, drmLockFlags flags );

void MGAGetQuiescence( ScrnInfoPtr pScrn );
void MGAGetQuiescenceShared( ScrnInfoPtr pScrn );

void MGASelectBuffer(ScrnInfoPtr pScrn, int which);
Bool MgaCleanupDma(ScrnInfoPtr pScrn);
Bool MgaInitDma(ScrnInfoPtr pScrn, int prim_size);

#define MGA_AGP_1X_MODE		0x01
#define MGA_AGP_2X_MODE		0x02
#define MGA_AGP_4X_MODE		0x04
#define MGA_AGP_MODE_MASK	0x07

#endif

void MGACRTC2Set(ScrnInfoPtr pScrn, xMODEINFO *pModeInfo);
void MGAEnableSecondOutPut(ScrnInfoPtr pScrn, xMODEINFO *pModeInfo);
void MGACRTC2SetPitch(ScrnInfoPtr pSrcn, xMODEINFO *pModeInfo);
void MGACRTC2SetDisplayStart(ScrnInfoPtr pScrn, xMODEINFO *pModeInfo, CARD32 base, CARD32 ulX, CARD32 ulY);

void MGACRTC2Get(ScrnInfoPtr pScrn, xMODEINFO *pModeInfo);
void MGACRTC2GetPitch(ScrnInfoPtr pSrcn, xMODEINFO *pModeInfo);
void MGACRTC2GetDisplayStart(ScrnInfoPtr pScrn, xMODEINFO *pModeInfo, CARD32 base, CARD32 ulX, CARD32 ulY);

double MGAG450SetPLLFreq(ScrnInfoPtr pScrn, long f_out);
#ifdef DEBUG
void MGAG450PrintPLL(ScrnInfoPtr pScrn);
#endif
long MGAG450SavePLLFreq(ScrnInfoPtr pScrn);
void MGAprintDac(ScrnInfoPtr pScrn);

#ifdef USEMGAHAL
/************ ESC Call Definition ***************/
typedef struct {
    char *function;
    void (*funcptr)(ScrnInfoPtr pScrn, unsigned long *param, char *out, DisplayModePtr pMode);
} MGAEscFuncRec, *MGAEscFuncPtr;

typedef struct {
    char function[32];
    unsigned long parameters[32];
} EscCmdStruct;

extern LPMGAMODEINFO pMgaModeInfo[2];
extern MGAMODEINFO   TmpMgaModeInfo[2];

extern void MGAExecuteEscCmd(ScrnInfoPtr pScrn, char *cmdline , char *sResult, DisplayModePtr pMode);
void MGAFillDisplayModeStruct(DisplayModePtr pMode, LPMGAMODEINFO pModeInfo);
/************************************************/
#endif

#endif
