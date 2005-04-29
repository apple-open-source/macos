/*
 *      Copyright 2001  Ani Joshi <ajoshi@unixbox.com>
 * 
 *      XFree86 4.x driver for S3 chipsets
 * 
 * 
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation and
 * that the name of Ani Joshi not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Ani Joshi makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *                 
 * ANI JOSHI DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ANI JOSHI BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3/s3.h,v 1.18 2003/07/17 08:19:35 eich Exp $ */


#ifndef _S3_H
#define _S3_H

#include "xf86.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"
#include "xf86RamDac.h"
#include "xaa.h"
#include "vbe.h"
#include "xf86_ansic.h"
#include "vgaHW.h"


#include "xf86xv.h"
#include "Xv.h"
#include "fourcc.h"


#ifndef S3_USEFB
#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb16.h"
#include "cfb24.h"
#include "cfb32.h"
#endif

typedef struct _S3RegRec {
	unsigned char	cr31, cr32, cr33, cr34, cr3a, cr3b, cr3c;
	unsigned char	cr3b2, cr3c2;
	unsigned char	cr40, cr42, cr43, cr44, cr45;
	unsigned char	cr50, cr51, cr53, cr54, cr55, cr58, cr59, cr5a,
			cr5d, cr5e;
	unsigned char	cr60, cr61, cr62, cr65, cr66, cr67, cr6d;
	unsigned char	s3save[10];
	unsigned char	s3syssave[46];
	unsigned char	dacregs[0x101];
	unsigned char	color_stack[8];
	unsigned char	clock;
} S3RegRec, *S3RegPtr;


typedef struct {
        unsigned char brightness;
        unsigned char contrast;
        FBAreaPtr     area;
        RegionRec     clip;
        CARD32        colorKey;
        CARD32        videoStatus;
        Time          offTime;
        Time          freeTime;
        int           lastPort;
} S3PortPrivRec, *S3PortPrivPtr;


typedef struct {
	int bitsPerPixel;
	int depth;
	int displayWidth;
	int pixel_code;
	int pixel_bytes;
	DisplayModePtr mode;
} S3FBLayout;


typedef struct _S3Rec {
        pciVideoPtr             PciInfo;
        PCITAG                  PciTag;
        EntityInfoPtr           pEnt;
        unsigned long           IOAddress;
        unsigned long           FBAddress; 
        unsigned char *         FBBase;   
        unsigned char *         MMIOBase;
        unsigned long           videoRam;
        OptionInfoPtr           Options;
        unsigned int            Flags;
        Bool                    NoAccel;
	Bool			SWCursor;
	Bool			SlowDRAMRefresh;
	Bool			SlowDRAM;
	Bool			SlowEDODRAM;
	Bool			SlowVRAM;
	Bool			S3NewMMIO;
	Bool			PCIRetry;
	Bool			ColorExpandBug;

        XAAInfoRecPtr           pXAA;
	xf86CursorInfoPtr	pCurs;
	xf86Int10InfoPtr	pInt10;
	vbeInfoPtr		pVBE;
        XF86VideoAdaptorPtr     adaptor;
        S3PortPrivPtr           portPrivate;

	DGAModePtr		DGAModes;
	int			numDGAModes;
	Bool			DGAactive;
	int			DGAViewportStatus;

	S3FBLayout		CurrentLayout;

	RamDacHelperRecPtr	RamDac;
	RamDacRecPtr		RamDacRec;

	int			vgaCRIndex, vgaCRReg;

	int			s3Bpp, s3BppDisplayWidth, HDisplay;
	int			mclk, MaxClock;
	int			pixMuxShift;

        int                     Chipset, ChipRev;
	int			RefClock;

	int			s3ScissB, s3ScissR;
	unsigned short		BltDir;
	int			trans_color;
	int			FBCursorOffset;

	S3RegRec		SavedRegs;
	S3RegRec		ModeRegs;

	unsigned char		SAM256;

	void			(*DacPreInit)(ScrnInfoPtr pScrn);
	void			(*DacInit)(ScrnInfoPtr pScrn,
					   DisplayModePtr mode);
	void			(*DacSave)(ScrnInfoPtr pScrn);
	void			(*DacRestore)(ScrnInfoPtr pScrn);
	Bool			(*CursorInit)(ScreenPtr pScreen);

	void			(*LoadPalette)(ScrnInfoPtr pScrn, int numColors,
					       int *indicies, LOCO *colors,
					       VisualPtr pVisual);

        Bool                    (*CloseScreen)(int, ScreenPtr);

	unsigned char		*imageBuffer;
	int			imageWidth;
	int			imageHeight;
    Bool			hwCursor;
} S3Rec, *S3Ptr;

#define S3PTR(p)		((S3Ptr)((p)->driverPrivate))


#define DRIVER_NAME     "s3"
#define DRIVER_VERSION  "0.3.5"
#define VERSION_MAJOR   0
#define VERSION_MINOR   3
#define PATCHLEVEL      5
#define S3_VERSION     ((VERSION_MAJOR << 24) | \
                        (VERSION_MINOR << 16) | PATCHLEVEL)




/*
 * Prototypes
 */

Bool S3AccelInit(ScreenPtr pScreen);
Bool S3AccelInitNewMMIO(ScreenPtr pScreen);
Bool S3AccelInitPIO(ScreenPtr pScreen);
Bool S3DGAInit(ScreenPtr pScreen);
Bool S3SwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
int S3GetRefClock(ScrnInfoPtr pScrn);

void S3InitVideo(ScreenPtr pScreen);
void S3InitStreams(ScrnInfoPtr pScrn, DisplayModePtr mode);

/* IBMRGB */
extern RamDacSupportedInfoRec S3IBMRamdacs[];
Bool S3ProbeIBMramdac(ScrnInfoPtr pScrn);
void S3IBMRGB_PreInit(ScrnInfoPtr pScrn);
void S3IBMRGB_Init(ScrnInfoPtr pScrn, DisplayModePtr mode);
void S3IBMRGB_Save(ScrnInfoPtr pScrn);
void S3IBMRGB_Restore(ScrnInfoPtr pScrn);
Bool S3IBMRGB_CursorInit(ScreenPtr pScreen);

/* TRIO64 */
Bool S3Trio64DACProbe(ScrnInfoPtr pScrn);
void S3Trio64DAC_PreInit(ScrnInfoPtr pScrn);
void S3Trio64DAC_Init(ScrnInfoPtr pScrn, DisplayModePtr mode);
void S3Trio64DAC_Save(ScrnInfoPtr pScrn);
void S3Trio64DAC_Restore(ScrnInfoPtr pScrn);

/* Ti */
Bool S3TiDACProbe(ScrnInfoPtr pScrn);
void S3TiDAC_PreInit(ScrnInfoPtr pScrn);
void S3TiDAC_Init(ScrnInfoPtr pScrn, DisplayModePtr mode);
void S3TiDAC_Save(ScrnInfoPtr pScrn);
void S3TiDAC_Restore(ScrnInfoPtr pScrn);
void S3TiLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indicies, LOCO *colors,
		     VisualPtr pVisual);
Bool S3Ti_CursorInit(ScreenPtr pScreen);
void S3OutTiIndReg(ScrnInfoPtr pScrn, CARD32 reg, unsigned char mask,
		   unsigned char data);

/* s3 gen cursor */
Bool S3_CursorInit(ScreenPtr pScreen);

#define TRIO64_RAMDAC	0x8811
#define	TI3025_RAMDAC	0x3025
#define	TI3020_RAMDAC	0x3020

#define BIOS_BSIZE	1024
#define	BIOS_BASE	0xc0000

/*
 * Chip...Sets...
 */

#define S3_964_SERIES()		((pS3->Chipset == PCI_CHIP_964_0) ||	\
			 	 (pS3->Chipset == PCI_CHIP_964_1))
#define	S3_TRIO_SERIES()	((pS3->Chipset == PCI_CHIP_TRIO) ||	\
			 	 (pS3->Chipset == PCI_CHIP_AURORA64VP) || \
				 (pS3->Chipset == PCI_CHIP_TRIO64UVP) || \
				 (pS3->Chipset == PCI_CHIP_TRIO64V2_DXGX))

#endif /* _S3_H */
