/*
 * Copyright 1997,1998 by Alan Hourihane <alanh@fairlite.demon.co.uk>
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
 * ALAN HOURIHANE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ALAN HOURIHANE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Alan Hourihane, <alanh@fairlite.demon.co.uk>
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tga/tga.h,v 1.18 2001/11/21 22:32:58 alanh Exp $ */

#ifndef _TGA_H_
#define _TGA_H_

#include "xaa.h"
#include "xf86RamDac.h"

typedef struct {
	unsigned long tgaRegs[0x100];
} TGARegRec, *TGARegPtr;


#define TGAPTR(p)	((TGAPtr)((p)->driverPrivate))

typedef struct {
    pciVideoPtr		PciInfo;
    PCITAG		PciTag;
    int			Chipset;
    RamDacHelperRecPtr	RamDac;
    int                 ChipRev;
    int			HwBpp;
    int			BppShift;
    int			pprod;
    CARD32		CardAddress;
    CARD32		IOAddress;
    CARD32		FbAddress;
    unsigned char *     FbBase;
    unsigned char *     IOBase;
    unsigned char *     ClkBase; /* TGA2 */
    unsigned char *     DACBase; /* TGA2 */
    unsigned char *     HACKBase; /* TGA2 */
    long		FbMapSize;
    unsigned long	regOffset;
    Bool		NoAccel;
    Bool		NoXaaPolySegment;
    Bool		Dac6Bit;
    Bool		SyncOnGreen;
    Bool		HWCursor;
    Bool		UsePCIRetry;
    int			MinClock;
    int			MaxClock;
    TGARegRec		SavedReg;
    TGARegRec		ModeReg;
    CARD32		AccelFlags;
    RamDacRecPtr	RamDacRec;
    XAAInfoRecPtr	AccelInfoRec;
    xf86CursorInfoPtr   CursorInfoRec;
    CloseScreenProcPtr	CloseScreen;
    int                 CardType;
    unsigned char       Bt463modeReg[59];
    unsigned char       Bt463saveReg[59];
    unsigned char       Ibm561modeReg[59];
    unsigned char       Ibm561saveReg[59];
    EntityInfoPtr       pEnt;
    CARD32     *buffers[1];
    unsigned int        current_rop;
    unsigned int        current_planemask;
    int                 transparent_pattern_p;
    int                 blitdir;
    int                 block_or_opaque_p;
    int                 ce_height;
    int                 ce_width;
    int                 ce_x;
    int                 ce_y;
    int                 ce_skipleft;
    int                 line_pattern_length;
    CARD16              line_pattern; /* for dashed lines */
    int                 Bpp; /* bytes per pixel */
    int                 depthflag; /* either BPP8PACKED or BPP24 */
    OptionInfoPtr	Options;
} TGARec, *TGAPtr;

/* ?? this is a hack for initial TGA2 support */
struct monitor_data {
  unsigned int max_rows;   /* Monitor setup */
  unsigned int max_cols;
  unsigned int pixel_freq;
  unsigned int refresh_rate;
  unsigned int vert_slines;
  unsigned int vert_fp;
  unsigned int vert_sync;
  unsigned int vert_bp;
  unsigned int horz_pix;
  unsigned int horz_fp;
  unsigned int horz_sync;
  unsigned int horz_bp;
  unsigned int vco_div;      /* ICS setup */
  unsigned int ref_div;
  unsigned int vco_pre;
  unsigned int clk_div;
  unsigned int vco_out_div;
  unsigned int clk_out_en;
  unsigned int clk_out_enX;
  unsigned int res0;
  unsigned int clk_sel;
  unsigned int res1;
  unsigned int ibm561_vco_div; /* IBM561 PLL setup */
  unsigned int ibm561_ref;
};

extern struct monitor_data tga_crystal_table[];
extern int tga_crystal_table_entries;
extern struct monitor_data *tga_c_table;

/* Prototypes */

/* tga_dac.c */
void DEC21030Restore(ScrnInfoPtr pScrn,/* vgaRegPtr vgaReg,*/
		      TGARegPtr tgaReg/*, Bool restoreFonts*/);
void DEC21030Save(ScrnInfoPtr pScrn, /*vgaRegPtr vgaReg,*/ TGARegPtr tgaReg/*,
		   Bool saveFonts*/);
Bool DEC21030Init(ScrnInfoPtr pScrn, DisplayModePtr mode);
void write_av9110(ScrnInfoPtr pScrn, unsigned int *);
void TGA2SetupMode(ScrnInfoPtr pScrn);
void Ibm561Init(TGAPtr pTga);
void Bt463Init(TGAPtr pTga);

/* tga_accel.c */
Bool DEC21030AccelInit(ScreenPtr pScreen);

/* BTramdac.c */
void tgaBTOutIndReg(ScrnInfoPtr pScrn,
		     CARD32 reg, unsigned char mask, unsigned char data);
unsigned char tgaBTInIndReg(ScrnInfoPtr pScrn, CARD32 reg);
void tgaBTWriteAddress(ScrnInfoPtr pScrn, CARD32 index);
void tgaBTReadAddress(ScrnInfoPtr pScrn, CARD32 index);
void tgaBTWriteData(ScrnInfoPtr pScrn, unsigned char data);
unsigned char tgaBTReadData(ScrnInfoPtr pScrn);

void tga2BTOutIndReg(ScrnInfoPtr pScrn,
		     CARD32 reg, unsigned char mask, unsigned char data);
unsigned char tga2BTInIndReg(ScrnInfoPtr pScrn, CARD32 reg);
void tga2BTWriteAddress(ScrnInfoPtr pScrn, CARD32 index);
void tga2BTReadAddress(ScrnInfoPtr pScrn, CARD32 index);
void tga2BTWriteData(ScrnInfoPtr pScrn, unsigned char data);
unsigned char tga2BTReadData(ScrnInfoPtr pScrn);

/* BT463ramdac.c */
void BT463ramdacSave(ScrnInfoPtr pScrn, unsigned char *data);
void BT463ramdacRestore(ScrnInfoPtr pScrn, unsigned char *data);

/* IBM561ramdac.c */
void IBM561ramdacSave(ScrnInfoPtr pScrn, unsigned char *data);
void IBM561ramdacHWInit(ScrnInfoPtr pScrn);
void IBM561ramdacRestore(ScrnInfoPtr pScrn, unsigned char *data);
unsigned char IBM561ReadReg(ScrnInfoPtr pScrn, CARD32 reg);
void IBM561WriteReg(ScrnInfoPtr pScrn, CARD32 reg, unsigned char data);

/* tga_cursor.c */
Bool TGAHWCursorInit(ScreenPtr pScreen);

/* tga_line.c */

void TGAPolySegment(DrawablePtr pDrawable, GCPtr pGC, int nseg,
		    xSegment *pSeg);
void TGAPolyLines(DrawablePtr pDrawable, GCPtr pGC, int mode, int npt,
		  DDXPointPtr pptInit);
void TGAPolySegmentDashed(DrawablePtr pDrawable, GCPtr pGC, int	nseg,
			  xSegment *pSeg);
void TGAPolyLinesDashed(DrawablePtr pDrawable, GCPtr pGC, int mode, int npt,
			DDXPointPtr pptInit);

/* line functions */
void
TGASetupForSolidLine(ScrnInfoPtr pScrn, int color, int rop,
		      unsigned int planemask);
void
TGASubsequentSolidHorVertLine(ScrnInfoPtr pScrn, int x, int y, int len,
			      int dir);
void
TGASubsequentSolidLine(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2,
		       int octant, int flags);
void
TGASetupForClippedLine(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2,
		       int octant);
void
TGASubsequentClippedSolidLine(ScrnInfoPtr pScrn, int x1, int y1, int len,
			      int err);

void
TGASetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, int rop,
		      unsigned int planemask, int length,
		      unsigned char *pattern);
void
TGASubsequentDashedLine(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2,
			int octant, int flags, int phase);
void
TGASubsequentClippedDashedLine(ScrnInfoPtr pScrn, int x1, int y1, int len,
			       int err, int phase);




#endif /* _TGA_H_ */

