/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/savage/savage_driver.h,v 1.17 2003/04/23 14:18:37 eich Exp $ */

#ifndef SAVAGE_VGAHWMMIO_H
#define SAVAGE_VGAHWMMIO_H

#include "xf86_ansic.h"
#include "compiler.h"
#include "vgaHW.h"
#include "xf86.h"
#include "xf86Resources.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"
#include "xf86_OSproc.h"
#include "xf86Cursor.h"
#include "mipointer.h"
#include "micmap.h"
#include "fb.h"
#include "xf86cmap.h"
#include "vbe.h"
#include "xaa.h"
#include "xf86xv.h"

#include "savage_regs.h"

#define VGAIN8(addr) MMIO_IN8(psav->MapBase+0x8000, addr)
#define VGAIN16(addr) MMIO_IN16(psav->MapBase+0x8000, addr)
#define VGAIN(addr) MMIO_IN32(psav->MapBase+0x8000, addr)
 
#define VGAOUT8(addr,val) MMIO_OUT8(psav->MapBase+0x8000, addr, val)
#define VGAOUT16(addr,val) MMIO_OUT16(psav->MapBase+0x8000, addr, val)
#define VGAOUT(addr,val) MMIO_OUT32(psav->MapBase+0x8000, addr, val)

#define INREG(addr) MMIO_IN32(psav->MapBase, addr)
#define OUTREG(addr,val) MMIO_OUT32(psav->MapBase, addr, val)
#define INREG16(addr) MMIO_IN16(psav->MapBase, addr)
#define OUTREG16(addr,val) MMIO_OUT16(psav->MapBase, addr, val)

#define SAVAGE_CRT_ON	1
#define SAVAGE_LCD_ON	2
#define SAVAGE_TV_ON	4

typedef struct _S3VMODEENTRY {
   unsigned short Width;
   unsigned short Height;
   unsigned short VesaMode;
   unsigned char RefreshCount;
   unsigned char * RefreshRate;
} SavageModeEntry, *SavageModeEntryPtr;


typedef struct _S3VMODETABLE {
   unsigned short NumModes;
   SavageModeEntry Modes[1];
} SavageModeTableRec, *SavageModeTablePtr;


typedef struct {
    unsigned int mode, refresh;
    unsigned char SR08, SR0E, SR0F;
    unsigned char SR10, SR11, SR12, SR13, SR15, SR18, SR1B, SR29, SR30;
    unsigned char SR54[8];
    unsigned char Clock;
    unsigned char CR31, CR32, CR33, CR34, CR36, CR3A, CR3B, CR3C;
    unsigned char CR40, CR41, CR42, CR43, CR45;
    unsigned char CR50, CR51, CR53, CR55, CR58, CR5B, CR5D, CR5E;
    unsigned char CR60, CR63, CR65, CR66, CR67, CR68, CR69, CR6D, CR6F;
    unsigned char CR86, CR88;
    unsigned char CR90, CR91, CRB0;
    unsigned int  STREAMS[22];	/* yuck, streams regs */
    unsigned int  MMPR0, MMPR1, MMPR2, MMPR3;
} SavageRegRec, *SavageRegPtr;


typedef struct _Savage {
    SavageRegRec	SavedReg;
    SavageRegRec	ModeReg;
    xf86CursorInfoPtr	CursorInfoRec;
    Bool		ModeStructInit;
    Bool		NeedSTREAMS;
    Bool		STREAMSRunning;
    int			Bpp, Bpl, ScissB;
    unsigned		PlaneMask;
    I2CBusPtr		I2C;

    int			videoRambytes;
    int			videoRamKbytes;
    int			MemOffScreen;
    CARD32		CursorKByte;

    /* These are physical addresses. */
    unsigned long	FrameBufferBase;
    unsigned long	MmioBase;
    unsigned long	ShadowPhysical;

    /* These are linear addresses. */
    unsigned char*	MapBase;
    unsigned char*	BciMem;
    unsigned char*	MapBaseDense;
    unsigned char*	FBBase;
    unsigned char*	FBStart;
    CARD32 volatile *	ShadowVirtual;

    Bool		PrimaryVidMapped;
    int			dacSpeedBpp;
    int			minClock, maxClock;
    int			HorizScaleFactor;
    int			MCLK, REFCLK, LCDclk;
    double		refclk_fact;
    int			GEResetCnt;

    /* Here are all the Options */

    OptionInfoPtr	Options;
    Bool		ShowCache;
    Bool		pci_burst;
    Bool		NoPCIRetry;
    Bool		fifo_conservative;
    Bool		fifo_moderate;
    Bool		fifo_aggressive;
    Bool		hwcursor;
    Bool		hwc_on;
    Bool		NoAccel;
    Bool		shadowFB;
    Bool		UseBIOS;
    int			rotate;
    double		LCDClock;
    Bool		ShadowStatus;
    Bool		CrtOnly;
    Bool		TvOn;
    Bool		PAL;
    Bool		ForceInit;
    int			iDevInfo;
    int			iDevInfoPrim;

    int			PanelX;		/* panel width */
    int			PanelY;		/* panel height */
    int			iResX;		/* crtc X display */
    int			iResY;		/* crtc Y display */
    int			XFactor;	/* overlay X factor */
    int			YFactor;	/* overlay Y factor */
    int			displayXoffset;	/* overlay X offset */
    int			displayYoffset;	/* overlay Y offset */
    int			XExp1;		/* expansion ratio in x */
    int			XExp2;
    int			YExp1;		/* expansion ratio in x */
    int			YExp2;
    int			cxScreen;
    int			TVSizeX;
    int			TVSizeY;

    CloseScreenProcPtr	CloseScreen;
    pciVideoPtr		PciInfo;
    PCITAG		PciTag;
    int			Chipset;
    int			ChipId;
    int			ChipRev;
    vbeInfoPtr		pVbe;
    int			EntityIndex;
    int			ShadowCounter;
    int			vgaIOBase;	/* 3b0 or 3d0 */

    /* The various Savage wait handlers. */
    int			(*WaitQueue)(struct _Savage *, int);
    int			(*WaitIdle)(struct _Savage *);
    int			(*WaitIdleEmpty)(struct _Savage *);

    /* Support for shadowFB and rotation */
    unsigned char *	ShadowPtr;
    int			ShadowPitch;
    void		(*PointerMoved)(int index, int x, int y);

    /* Support for XAA acceleration */
    XAAInfoRecPtr	AccelInfoRec;
    xRectangle		Rect;
    unsigned int	SavedBciCmd;
    unsigned int	SavedFgColor;
    unsigned int	SavedBgColor;
    unsigned int	SavedSbdOffset;
    unsigned int	SavedSbd;

    /* Support for Int10 processing */
    xf86Int10InfoPtr	pInt10;
    SavageModeTablePtr	ModeTable;

    /* Support for the Savage command overflow buffer. */
    unsigned long	cobIndex;	/* size index */
    unsigned long	cobSize;	/* size in bytes */
    unsigned long	cobOffset;	/* offset in frame buffer */

    /* Support for DGA */
    int			numDGAModes;
    DGAModePtr		DGAModes;
    Bool		DGAactive;
    int			DGAViewportStatus;

    /* Support for XVideo */

    unsigned int	videoFlags;
    unsigned int	blendBase;
    int			videoFourCC;
    XF86VideoAdaptorPtr	adaptor;
    int			VideoZoomMax;
    int			dwBCIWait2DIdle;
    XF86OffscreenImagePtr offscreenImages;

} SavageRec, *SavagePtr;

/* Video flags. */

#define VF_STREAMS_ON	0x0001

#define SAVPTR(p)	((SavagePtr)((p)->driverPrivate))

/* Make the names of these externals driver-unique */
#define gpScrn savagegpScrn
#define myOUTREG savageOUTREG
#define readdw savagereaddw
#define readfb savagereadfb
#define writedw savagewritedw
#define writefb savagewritefb
#define writescan savagewritescan

/* Prototypes. */

extern void SavageCommonCalcClock(long freq, int min_m, int min_n1,
			int max_n1, int min_n2, int max_n2,
			long freq_min, long freq_max,
			unsigned char *mdiv, unsigned char *ndiv);
void SavageAdjustFrame(int scrnIndex, int y, int x, int flags);
Bool SavageSwitchMode(int scrnIndex, DisplayModePtr mode, int flags);

/* In savage_cursor.c. */

Bool SavageHWCursorInit(ScreenPtr pScreen);
void SavageShowCursor(ScrnInfoPtr);
void SavageHideCursor(ScrnInfoPtr);

/* In savage_accel.c. */

Bool SavageInitAccel(ScreenPtr);
void SavageInitialize2DEngine(ScrnInfoPtr);
void SavageSetGBD(ScrnInfoPtr);
void SavageAccelSync(ScrnInfoPtr);

/* In savage_i2c.c. */

Bool SavageI2CInit(ScrnInfoPtr pScrn);

/* In savage_shadow.c */

void SavagePointerMoved(int index, int x, int y);
void SavageRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void SavageRefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void SavageRefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void SavageRefreshArea24(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void SavageRefreshArea32(ScrnInfoPtr pScrn, int num, BoxPtr pbox);

/* In savage_vbe.c */

void SavageSetTextMode( SavagePtr psav );
void SavageSetVESAMode( SavagePtr psav, int n, int Refresh );
void SavageFreeBIOSModeTable( SavagePtr psav, SavageModeTablePtr* ppTable );
SavageModeTablePtr SavageGetBIOSModeTable( SavagePtr psav, int iDepth );

unsigned short SavageGetBIOSModes( 
    SavagePtr psav,
    int iDepth,
    SavageModeEntryPtr s3vModeTable );


/* In savage_video.c */

void SavageInitVideo( ScreenPtr pScreen );

#endif /* SAVAGE_VGAHWMMIO_H */

