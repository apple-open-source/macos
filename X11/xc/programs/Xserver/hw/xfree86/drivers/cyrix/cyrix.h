/*
 * Copyright 2000 by Richard A. Hecker, California, United States
 * Copyright 2002 by Red Hat Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Alan Hourihane not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Alan Hourihane makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * RICHARD HECKER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL RICHARD HECKER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL RICHARD HECKER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Richard Hecker, hecker@cat.dfrc.nasa.gov
 *          Re-written for XFree86 v4.0
 * Chunks re-written again for XFree86 v4.2
 *	    Alan Cox <alan@redhat.com>
 * Previous driver (pre-XFree86 v4.0) by
 *          Annius V. Groenink (A.V.Groenink@zfc.nl, avg@cwi.nl),
 *          Dirk H. Hohndel (hohndel@suse.de),
 *          Portions: the GGI project & confidential CYRIX databooks.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/cyrix/cyrix.h,v 1.4 2002/11/06 11:38:59 alanh Exp $ */

#ifndef _CYRIX_H_
#define _CYRIX_H_

#include "xaa.h"
#include "xf86Cursor.h"
#include "vgaHW.h"

/* this code is partly based on the MediaGX sources from the GGI project
   based on CYRIX example code (gxvideo.c) and included with CYRIX and
   GGI's permission under the XFree86 license.
*/

/* XFree86 macros */
#define CYRIXPTR(p) ((CYRIXPrvPtr)((p)->driverPrivate))


/* Driver specific structures */
typedef struct {
    unsigned char cyrixRegs3x4[0x100];
    unsigned char cyrixRegs3CE[0x100];
    unsigned char cyrixRegs3C4[0x100];
    unsigned char cyrixRegsDAC[0x01];
    unsigned char cyrixRegsClock[0x03];
    unsigned char DacRegs[0x300];
    unsigned int Colormap[0x100];	/* Actually 18bit values */
} CYRIXRegRec, *CYRIXRegPtr;

typedef struct {
        /* extended SoftVGA registers */
	unsigned char VerticalTimingExtension;
	unsigned char ExtendedAddressControl;
	unsigned char ExtendedOffset;
	unsigned char Offset;
	unsigned char ExtendedColorControl;
	unsigned char DisplayCompression;
	unsigned char DriverControl;
	unsigned char DACControl;
	unsigned char ClockControl;
	unsigned char CrtClockFrequency;
	unsigned char CrtClockFrequencyFraction;
	unsigned char RefreshRate;

	/* display controller hardware registers */
	CARD32 DcGeneralCfg;
	CARD32 DcCursStOffset;
	CARD32 DcCbStOffset;
	CARD32 DcLineDelta;
	CARD32 DcBufSize;
	CARD32 DcCursorX;
	CARD32 DcCursorY;
	CARD32 DcCursorColor;

	/* graphics pipeline registers */
	CARD32 GpBlitStatus;

	/* save area for cursor image */
	char cursorPattern[256];
} prevExt;

typedef struct {
	/* struct for the server */
	CARD32 	 		IOAccelAddress;
	CARD32 			FbAddress;
        char*                   GXregisters;
        int                     CYRIXcursorAddress; /* relative to fb base */
        int                     CYRIXbltBuf0Address;/*relative to GXregisters*/
        int                     CYRIXbltBuf1Address;
        int                     CYRIXbltBufSize;
        EntityInfoPtr       	pEnt;
	unsigned char *		FbBase;
	pciVideoPtr		PciInfo;
	XAAInfoRecPtr		AccelInfoRec;
	xf86CursorInfoPtr   	CursorInfoRec;
	CloseScreenProcPtr  	CloseScreen;
	int			HwBpp;
	int			MinClock;
	int			MaxClock;
	int			Chipset;
	int			ChipRev;
	int			RamDac;
	long			FbMapSize;
	short			EngineOperation;
	CYRIXRegRec		SavedReg;
	CYRIXRegRec		ModeReg;
	vgaHWRec		std;
	prevExt			PrevExt;
	Bool			HWCursor;
/*	Bool			IsCyber;
	Bool			NewClockCode;*/
	Bool			NoAccel;
	Bool			NoCompress;
	Bool			ShadowFB;
	unsigned char *		ShadowPtr;
	int			ShadowPitch;
	int			Rotate;
	void			(*PointerMoved)(int index, int x, int y);
	OptionInfoPtr		Options;
    /* accel stuff */
        int bltBufWidth;
        int blitMode;
        int vectorMode;
        int transMode;
        int copyXdir;
        int setBlitModeOnSync;

} CYRIXPrivate, *CYRIXPrvPtr;

typedef struct {
	vgaHWRec std; /* IBM VGA */
	prevExt  ext;
} CYRIXRec, *CYRIXPtr;

/* Helper routines */
extern void    	Cyrix1bppColorMap(ScrnInfoPtr pScrn);
extern int	CyrixHWCursor(ScreenPtr pScr);
extern int	CyrixInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
extern void    	CyrixRestore(ScrnInfoPtr pScrn, CYRIXRegPtr cyrixReg);
extern void *  	CyrixSave(ScrnInfoPtr pScrn, CYRIXRegPtr cyrixReg);

/* Shadow routines */
extern void	CYRIXRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
extern void	CYRIXPointerMoved(int index, int x, int y);
extern void	CYRIXRefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
extern void	CYRIXRefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox);

extern void CYRIXsetBlitBuffers(void);
extern void CYRIXsetBlitBuffersOnOldChip(void);

/* 32 bit access to GX registers */
#define GX_REG(a) (*(volatile CARD32*)(pCyrix->GXregisters + (a)))


/* externs in cyrix_accel.c */
extern void CYRIXAccelInit(ScreenPtr);

/* A minor Sync() function that only waits for the graphics pipeline
   registers to be flushed so we can define a new operation */
#define CYRIXsetupSync()            \
                     while (GX_REG(GP_BLIT_STATUS) & BS_BLIT_PENDING)


/* access macros to the graphics pipeline registers */
#define CYRIXsetSrcXY(x, y)                  \
                     GX_REG(GP_SRC_XCOOR)    = (((y) << 16) | (x))

#define CYRIXsetDstXY(x, y)                  \
                     GX_REG(GP_DST_XCOOR)    = (((y) << 16) | (x))

#define CYRIXsetWH(w, h)                     \
                     GX_REG(GP_WIDTH)        = (((h) << 16) | (w))

#define CYRIXsetSourceColors01(p, col0, col1)   \
                     CYRIXsetColors01((p), GP_SRC_COLOR_0, (col0), (col1))

#define CYRIXsetPatColors01(p, col0, col1)      \
                     CYRIXsetColors01((p), GP_PAT_COLOR_0, (col0), (col1))

#define CYRIXsetPatColors23(p, col2, col3)      \
                     CYRIXsetColors01((p), GP_PAT_COLOR_2, (col2), (col3))

#define CYRIXsetPatData(data0, data1)        \
                     GX_REG(GP_PAT_DATA_0)   = (data0); \
                     GX_REG(GP_PAT_DATA_1)   = (data1)

#define CYRIXsetPatMode(xrop, pm)            \
                     GX_REG(GP_RASTER_MODE)  = ((pm) | windowsROPpatMask[(xrop)])

#define CYRIXsetPatModeX(xrop, pm)           \
                     GX_REG(GP_RASTER_MODE)  = ((pm) | windowsROPsrcMask[(xrop)])

#define CYRIXsetPatModeTrans(pm)             \
                     GX_REG(GP_RASTER_MODE)  = ((pm) | RM_CLIP_ENABLE | 0xC6);

#define CYRIXsetBlitMode()                   \
                     GX_REG(GP_BLIT_MODE)    = (pCyrix->blitMode)

#define CYRIXsetVectorMode()                 \
                     GX_REG(GP_VECTOR_MODE)  = (pCyrix->vectorMode)

#define IfDest(rop, planemask, val)                                        \
                                 (( (((rop) & 0x5) ^ (((rop) & 0xA) >> 1)) \
                                 || (~((planemask) & 0xFF))                \
                                 ) ? (val) : 0)

/* Generic MediaGX hardware register and value definitions */

#define GX_IOPORT_INDEX		0x22
#define GX_IOPORT_DATA		0x23

/*	I/O REGISTERS INDEX DEFINITIONS	*/

#define	GX_IOIDX_PCR		0x20
#define	GX_IOIDX_CCR1		0xC1
#define	GX_IOIDX_CCR2		0xC2
#define	GX_IOIDX_CCR3		0xC3
#define	GX_IOIDX_CCR4		0xE8
#define	GX_IOIDX_DIR0		0xFE
#define	GX_IOIDX_DIR1		0xFF
#define	GX_IOIDX_SMAR0		0xCD
#define	GX_IOIDX_SMAR1		0xCE
#define	GX_IOIDX_SMAR2		0xCF
#define	GX_IOIDX_SMHR0		0xB0
#define	GX_IOIDX_SMHR1		0xB1
#define	GX_IOIDX_SMHR2		0xB2
#define	GX_IOIDX_SMHR3		0xB3
#define	GX_IOIDX_GCR		0xB8
#define	GX_IOIDX_VGACTL		0xB9
#define	GX_IOIDX_VGAM0		0xBA
#define	GX_IOIDX_VGAM1		0xBB
#define	GX_IOIDX_VGAM2		0xBC
#define	GX_IOIDX_VGAM3		0xBD

/*	BUS CONTROLLER REGISTER DEFINITIONS	*/

#define BC_DRAM_TOP		0x8000
#define BC_XMAP_1		0x8004
#define BC_XMAP_2		0x8008
#define BC_XMAP_3		0x800C

/*	GRAPHICS PIPELINE REGISTER DEFINITIONS	*/

#define GP_DST_XCOOR		0x8100		/* x destination origin	*/
#define GP_DST_YCOOR		0x8102		/* y destination origin	*/
#define GP_WIDTH		0x8104		/* pixel width */
#define GP_HEIGHT		0x8106		/* pixel height */
#define GP_SRC_XCOOR		0x8108		/* x source origin */
#define GP_SRC_YCOOR		0x810A		/* y source origin */

#define GP_VECTOR_LENGTH	0x8104		/* vector length */
#define GP_INIT_ERROR		0x8106		/* vector initial error	*/
#define GP_AXIAL_ERROR		0x8108		/* axial error increment */
#define GP_DIAG_ERROR		0x810A		/* diagonal error increment */

#define GP_SRC_COLOR_0		0x810C		/* source color 0 */
#define GP_SRC_COLOR_1		0x810E		/* source color 1 */
#define GP_PAT_COLOR_0		0x8110		/* pattern color 0 */
#define GP_PAT_COLOR_1		0x8112		/* pattern color 1 */
#define GP_PAT_COLOR_2		0x8114		/* pattern color 2 */
#define GP_PAT_COLOR_3		0x8116		/* pattern color 3 */
#define GP_PAT_DATA_0		0x8120		/* bits 31:0 of pattern	*/
#define GP_PAT_DATA_1		0x8124		/* bits 63:32 of pattern */
#define GP_PAT_DATA_2		0x8128		/* bits 95:64 of pattern */
#define GP_PAT_DATA_3		0x812C		/* bits 127:96 of pattern */

#define GP_RASTER_MODE		0x8200		/* raster operation */
#define GP_VECTOR_MODE		0x8204		/* vector mode register	*/
#define GP_BLIT_MODE		0x8208		/* blit mode register */
#define GP_BLIT_STATUS		0x820C		/* blit status register	*/

/*	"GP_VECTOR_MODE" BIT DEFINITIONS	*/

#define VM_X_MAJOR		0x0000		/* X major vector */
#define VM_Y_MAJOR		0x0001		/* Y major vector */
#define VM_MAJOR_INC		0x0002		/* positive major axis step */
#define VM_MINOR_INC		0x0004		/* positive minor axis step */
#define VM_READ_DST_FB		0x0008		/* read destination data */

/*	"GP_RASTER_MODE" BIT DEFINITIONS	*/

#define RM_PAT_DISABLE		0x0000		/* pattern is disabled */
#define RM_PAT_MONO		0x0100		/* 1BPP pattern expansion */
#define RM_PAT_DITHER		0x0200		/* 2BPP pattern expansion */
#define RM_PAT_COLOR		0x0300		/* 8BPP or 16BPP pattern */
#define RM_PAT_MASK		0x0300		/* mask for pattern mode */
#define RM_PAT_TRANSPARENT	0x0400		/* transparent 1BPP pattern */
#define RM_SRC_TRANSPARENT	0x0800		/* transparent 1BPP source */
#define RM_CLIP_ENABLE		0x1000		/* enables clipping */

/*	"GP_BLIT_STATUS" BIT DEFINITIONS	*/

#define BS_BLIT_BUSY		0x0001		/* blit engine is busy */
#define BS_PIPELINE_BUSY	0x0002		/* graphics pipeline is bus */
#define BS_BLIT_PENDING		0x0004		/* blit pending	*/
#define BC_FLUSH		0x0080		/* flush pipeline requests */
#define BC_8BPP			0x0000		/* 8BPP mode */
#define BC_16BPP		0x0100		/* 16BPP mode */
#define BC_FB_WIDTH_1024	0x0000		/* framebuffer width = 1024 */
#define BC_FB_WIDTH_2048	0x0200		/* framebuffer width = 2048 */

/*	"GP_BLIT_MODE" BIT DEFINITIONS		*/

#define	BM_READ_SRC_NONE	0x0000		/* source foreground color */
#define BM_READ_SRC_FB		0x0001		/* read source from FB 	*/
#define BM_READ_SRC_BB0		0x0002		/* read source from BB0 */
#define BM_READ_SRC_BB1		0x0003		/* read source from BB1	*/
#define BM_READ_SRC_MASK	0x0003		/* read source mask */

#define	BM_READ_DST_NONE	0x0000		/* no destination data */
#define BM_READ_DST_BB0		0x0008		/* destination from BB0	*/
#define BM_READ_DST_BB1		0x000C		/* destination from BB1	*/
#define BM_READ_DST_FB0		0x0010		/* dest from FB (store BB0) */
#define BM_READ_DST_FB1		0x0014		/* dest from FB (store BB1)*/
#define BM_READ_DST_MASK	0x001C		/* read destination mask */

#define BM_WRITE_FB		0x0000		/* write to framebuffer	*/
#define	BM_WRITE_MEM		0x0020		/* write to memory */
#define BM_WRITE_MASK		0x0020		/* write mask */

#define	BM_SOURCE_COLOR		0x0000		/* source is 8BPP or 16BPP */
#define BM_SOURCE_EXPAND	0x0040		/* source is 1BPP */
#define BM_SOURCE_TEXT		0x00C0		/* source is 1BPP text */
#define BM_SOURCE_MASK		0x00C0		/* source mask */

#define BM_REVERSE_Y		0x0100		/* reverse Y direction */

/*	DISPLAY CONTROLLER REGISTER DEFINITIONS	*/

#define DC_UNLOCK		0x8300		/* lock register */
#define DC_GENERAL_CFG		0x8304		/* config registers... */
#define DC_TIMING_CFG		0x8308
#define DC_OUTPUT_CFG		0x830C

#define DC_FB_ST_OFFSET		0x8310		/* framebuffer start offset */
#define DC_CB_ST_OFFSET		0x8314		/* compression start offset */
#define DC_CURS_ST_OFFSET	0x8318		/* cursor start offset*/
#define DC_ICON_ST_OFFSET	0x831C		/* icon start offset */
#define DC_VID_ST_OFFSET	0x8320		/* video start offset */
#define DC_LINE_DELTA		0x8324		/* fb and cb skip counts */
#define DC_BUF_SIZE		0x8328		/* fb and cb line size */

#define DC_H_TIMING_1		0x8330		/* horizontal timing...	*/
#define DC_H_TIMING_2		0x8334
#define DC_H_TIMING_3		0x8338
#define DC_FP_H_TIMING		0x833C

#define DC_V_TIMING_1		0x8340		/* vertical timing... */
#define DC_V_TIMING_2		0x8344
#define DC_V_TIMING_3		0x8348
#define DC_FP_V_TIMING		0x834C

#define DC_CURSOR_X		0x8350		/* cursor x position */
#define DC_ICON_X		0x8354		/* HACK - 1.3 definition */
#define DC_V_LINE_CNT		0x8354		/* vertical line counter */
#define DC_CURSOR_Y		0x8358		/* cursor y position */
#define DC_ICON_Y		0x835C		/* HACK - 1.3 definition */
#define DC_SS_LINE_COMPARE	0x835C		/* line compare value */

#define DC_CURSOR_COLOR		0x8360		/* cursor colors */
#define DC_ICON_COLOR		0x8364		/* icon colors */
#define DC_BORDER_COLOR		0x8368		/* border color	*/

#define DC_PAL_ADDRESS		0x8370		/* palette address */
#define DC_PAL_DATA		0x8374		/* palette data	*/

/*	PALETTE ADDRESS DEFINITIONS		*/

#define PAL_CURSOR_COLOR_0	0x100
#define PAL_CURSOR_COLOR_1	0x101
#define PAL_ICON_COLOR_0	0x102
#define PAL_ICON_COLOR_1	0x103
#define PAL_OVERSCAN_COLOR	0x104

/*	DC BIT DEFINITIONS			*/

#define DC_UNLOCK_VALUE		0x00004758	/* used to unlock DC regs */

#define DC_GCFG_DFLE		0x00000001	/* display FIFO load enable */
#define DC_GCFG_CURE		0x00000002	/* cursor enable */
#define DC_GCFG_ICNE		0x00000004	/* HACK - 1.3 definition */
#define DC_GCFG_PLNO		0x00000004	/* planar offset LSB */
#define DC_GCFG_VIDE		0x00000008	/* HACK - 1.3 definition */
#define DC_GCFG_PPC		0x00000008	/* pixel pan compatibility */
#define DC_GCFG_CMPE		0x00000010	/* compression enable */
#define DC_GCFG_DECE		0x00000020	/* decompression enable	*/
#define DC_GCFG_DCLK_MASK	0x000000C0	/* dotclock multiplier */
#define DC_GCFG_DCLK_POS	6		/* dotclock multiplier */
#define DC_GCFG_DFHPSL_MASK	0x00000F00	/* FIFO high-priority start */
#define DC_GCFG_DFHPSL_POS	8		/* FIFO high-priority start */
#define DC_GCFG_DFHPEL_MASK	0x0000F000	/* FIFO high-priority end */
#define DC_GCFG_DFHPEL_POS	12		/* FIFO high-priority end */
#define DC_GCFG_CIM_MASK	0x00030000	/* compressor insert mode */
#define DC_GCFG_CIM_POS		16		/* compressor insert mode */
#define DC_GCFG_FDTY		0x00040000	/* frame dirty mode */
#define DC_GCFG_RTPM		0x00080000	/* real-time perf. monitor */
#define DC_GCFG_DAC_RS_MASK	0x00700000	/* DAC register selects */
#define DC_GCFG_DAC_RS_POS	20		/* DAC register selects	*/
#define DC_GCFG_CKWR		0x00800000	/* clock write */
#define DC_GCFG_LDBL		0x01000000	/* line double */
#define DC_GCFG_DIAG		0x02000000	/* FIFO diagnostic mode	*/
#define DC_GCFG_CH4S		0x04000000	/* sparse refresh mode */
#define DC_GCFG_SSLC		0x08000000	/* enable line compare */
#define DC_GCFG_FBLC		0x10000000	/* enable fb offset compare */
#define DC_GCFG_DFCK		0x20000000	/* divide flat-panel clock */
#define DC_GCFG_DPCK		0x40000000	/* divide pixel clock */
#define DC_GCFG_DDCK		0x80000000	/* divide dot clock */

#define DC_TCFG_FPPE		0x00000001	/* flat-panel power enable */
#define DC_TCFG_HSYE		0x00000002	/* horizontal sync enable */
#define DC_TCFG_VSYE		0x00000004	/* vertical sync enable	*/
#define DC_TCFG_BLKE		0x00000008	/* blank enable	*/
#define DC_TCFG_DDCK		0x00000010	/* DDC clock */
#define DC_TCFG_TGEN		0x00000020	/* timing generator enable */
#define DC_TCFG_VIEN		0x00000040	/* vertical interrupt enable */
#define DC_TCFG_BLNK		0x00000080	/* blink enable	*/
#define DC_TCFG_CHSP		0x00000100	/* horizontal sync polarity */
#define DC_TCFG_CVSP		0x00000200	/* vertical sync polarity */
#define DC_TCFG_FHSP		0x00000400	/* panel horz sync polarity */
#define DC_TCFG_FVSP		0x00000800	/* panel vert sync polarity */
#define DC_TCFG_FCEN		0x00001000	/* flat-panel centering	*/
#define DC_TCFG_CDCE		0x00002000	/* HACK - 1.3 definition */
#define DC_TCFG_PLNR		0x00002000	/* planar mode enable */
#define DC_TCFG_INTL		0x00004000	/* interlace scan */
#define DC_TCFG_PXDB		0x00008000	/* pixel double	*/
#define DC_TCFG_BKRT		0x00010000	/* blink rate */
#define DC_TCFG_PSD_MASK	0x000E0000	/* power sequence delay	*/
#define DC_TCFG_PSD_POS		17		/* power sequence delay	*/
#define DC_TCFG_DDCI		0x08000000	/* DDC input (RO) */
#define DC_TCFG_SENS		0x10000000	/* monitor sense (RO) */
#define DC_TCFG_DNA		0x20000000	/* display not active (RO) */
#define DC_TCFG_VNA		0x40000000	/* vertical not active (RO) */
#define DC_TCFG_VINT		0x80000000	/* vertical interrupt (RO) */

#define DC_OCFG_8BPP		0x00000001	/* 8/16 bpp select */
#define DC_OCFG_555		0x00000002	/* 16 bpp format */
#define DC_OCFG_PCKE		0x00000004	/* PCLK enable */
#define DC_OCFG_FRME		0x00000008	/* frame rate mod enable */
#define DC_OCFG_DITE		0x00000010	/* dither enable */
#define DC_OCFG_2PXE		0x00000020	/* 2 pixel enable */
#define DC_OCFG_2XCK		0x00000040	/* 2 x pixel clock */
#define DC_OCFG_2IND		0x00000080	/* 2 index enable */
#define DC_OCFG_34ADD		0x00000100	/* 3- or 4-bit add */
#define DC_OCFG_FRMS		0x00000200	/* frame rate mod select */
#define DC_OCFG_CKSL		0x00000400	/* clock select	*/
#define DC_OCFG_PRMP		0x00000800	/* palette re-map */
#define DC_OCFG_PDEL		0x00001000	/* panel data enable low */
#define DC_OCFG_PDEH		0x00002000	/* panel data enable high */
#define DC_OCFG_CFRW		0x00004000	/* comp line buffer r/w sel */
#define DC_OCFG_DIAG		0x00008000	/* comp line buffer diag */

/*	MC REGISTER DEFINITIONS			*/

#define MC_GBASE_ADD		0x8414		/* Graphics mem base address */
#define MC_DR_ADD		0x8418		/* Dirty RAM address index */
#define MC_DR_ACC		0x841C		/* Dirty RAM memory access */
#define MC_RAMDAC_ACC		0x8420		/* RAMDAC register access */

/*	CPU REGISTER REGISTER DEFINITIONS	*/

#define BB0_BASE		0xFFFFFF0C	/* Blit buffer 0 base */
#define BB1_BASE		0xFFFFFF1C	/* Blit buffer 1 base */



/* SoftVGA CRTC register indices and bit definitions */

#define CrtcExtendedRegisterLock       0x30
#define CrtcGraphicsMemorySize         0x3E
#define CrtcModeSwitchControl          0x3F
#define CrtcVerticalTimingExtension    0x41
#define CrtcExtendedAddressControl     0x43
#define CrtcExtendedStartAddress       0x44
#define CrtcExtendedOffset             0x45
#define CrtcExtendedColorControl       0x46
#define CrtcWriteMemoryAperture        0x47
#define CrtcReadMemoryAperture         0x48
#define CrtcDisplayCompression         0x49
#define CrtcDriverControl              0x4A
#define CrtcDACControl                 0x4B
#define CrtcClockControl               0x4C
#define CrtcClockFrequency             0x4D
#define CrtcClockFrequencyFraction     0x4E
#define CrtcRefreshRate                0x4F

/* ExtendedAddressControl bit definitions */
#define EAC_PIXEL_DOUBLE               0x04
#define EAC_DIRECT_FRAME_BUFFER        0x02
#define EAC_PACKED_CHAIN4              0x01

/* ExtendedColorControl bit definitions */
#define ECC_32BPP                      0x04     /* MXi only */
#define ECC_555_FORMAT                 0x02
#define ECC_565_FORMAT                 0x00
#define ECC_16BPP                      0x01
#define ECC_8BPP                       0x00

/* DriverControl bit definitions */
#define DRVCT_DISPLAY_DRIVER_ACTIVE    0x01

/* DACControl bit definitions */
#define DACCT_HALF_PIXEL_PER_CLOCK     0x08
#define DACCT_TWO_PIXELS_PER_CLOCK     0x04
#define DACCT_ENABLE_16BIT_BUS         0x02

/* ClockControl bit definitions */
#define CLKCT_EXT_CLOCK_MODE           0x80
#define CLKCT_HALVE_DOT_CLOCK          0x20
#define CLKCT_DOUBLE_DOT_CLOCK         0x10
#define CLKCT_GENDAC_ICS5342           0x00
#define CLKCT_GENDAC_MASK              0x07

#endif /* _CYRIX_H_ */
