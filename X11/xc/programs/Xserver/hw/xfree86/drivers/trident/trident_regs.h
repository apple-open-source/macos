/*
 * Copyright 1992-2000 by Alan Hourihane, Wigan, England.
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
 * Author:  Alan Hourihane, alanh@fairlite.demon.co.uk
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/trident/trident_regs.h,v 1.26 2002/09/16 18:06:03 eich Exp $ */

#define DEBUG 1

#define NTSC 14.31818
#define PAL  17.73448

/* General Registers */
#define SPR	0x1F		/* Software Programming Register (videoram) */

/* 3C4 */
#define RevisionID 0x09
#define ConfPort1 0x0C
#define ConfPort2 0x0C
#define NewMode2 0x0D
#define OldMode2 0x00 /* Should be 0x0D - dealt with in trident_dac.c */
#define OldMode1 0x0E
#define NewMode1 0x0E
#define Protection 0x11
#define Threshold 0x12
#define MCLKLow 0x16
#define MCLKHigh 0x17
#define ClockLow 0x18
#define ClockHigh 0x19
#define SSetup 0x20
#define SKey 0x37
#define SPKey 0x57
#define GBslope1 0xB4
#define GBslope2 0xB5
#define GBslope3 0xB6
#define GBslope4 0xB7
#define GBintercept1 0xB8
#define GBintercept2 0xB9
#define GBintercept3 0xBA
#define GBintercept4 0xBB

/* 3x4 */
#define Offset 0x13
#define Underline 0x14
#define CRTCMode 0x17
#define CRTCModuleTest 0x1E
#define FIFOControl 0x20
#define LinearAddReg 0x21
#define DRAMTiming 0x23
#define New32 0x23
#define RAMDACTiming 0x25
#define CRTHiOrd 0x27
#define AddColReg 0x29
#define InterfaceSel 0x2A
#define HorizOverflow 0x2B
#define GETest 0x2D
#define Performance 0x2F
#define GraphEngReg 0x36
#define I2C 0x37
#define PixelBusReg 0x38
#define PCIReg 0x39
#define DRAMControl 0x3A
#define MiscContReg 0x3C
#define CursorXLow 0x40
#define CursorXHigh 0x41
#define CursorYLow 0x42
#define CursorYHigh 0x43
#define CursorLocLow 0x44
#define CursorLocHigh 0x45
#define CursorXOffset 0x46
#define CursorYOffset 0x47
#define CursorFG1 0x48
#define CursorFG2 0x49
#define CursorFG3 0x4A
#define CursorFG4 0x4B
#define CursorBG1 0x4C
#define CursorBG2 0x4D
#define CursorBG3 0x4E
#define CursorBG4 0x4F
#define CursorControl 0x50
#define PCIRetry 0x55
#define PreEndControl 0x56
#define PreEndFetch 0x57
#define PCIMaster 0x60
#define Enhancement0 0x62
#define NewEDO 0x64
#define TVinterface 0xC0
#define TVMode 0xC1
#define ClockControl 0xCF


/* 3CE */
#define MiscExtFunc 0x0F
#define MiscIntContReg 0x2F
#define CyberControl 0x30
#define CyberEnhance 0x31
#define FPConfig     0x33
#define VertStretch  0x52
#define HorStretch   0x53
#define BiosMode     0x5c
#define BiosNewMode1 0x5a
#define BiosNewMode2 0x5c
#define BiosReg      0x5d

/* Graphics Engine for 9420/9430 */

#define GER_INDEX	0x210A
#define GER_BYTE0	0x210C
#define GER_BYTE1	0x210D
#define GER_BYTE2	0x210E
#define GER_BYTE3	0x210F
#define MMIOBASE	0x7C
#define OLDGER_STATUS	0x90
#define OLDGER_MWIDTH	0xB8
#define OLDGER_MFORMAT	0xBC
#define OLDGER_STYLE	0xC4
#define OLDGER_FMIX	0xC8
#define OLDGER_BMIX	0xC8
#define OLDGER_FCOLOUR	0xD8
#define OLDGER_BCOLOUR	0xDC
#define OLDGER_DIMXY	0xE0
#define OLDGER_DESTLINEAR	0xE4
#define OLDGER_DESTXY	0xF8
#define OLDGER_COMMAND	0xFC
#define		OLDGE_FILL	0x000A0000	/* Area Fill */

/* Graphics Engine for 9440/9660/9680 */

#define GER_STATUS	0x2120		
#define		GE_BUSY	0x80
#define GER_OPERMODE	0x2122		/* Byte for 9440, Word for 96xx */
#define		DST_ENABLE	0x200	/* Destination Transparency */
#define GER_COMMAND	0x2124		
#define		GE_NOP		0x00	/* No Operation */
#define		GE_BLT		0x01	/* BitBLT ROP3 only */
#define		GE_BLT_ROP4	0x02	/* BitBLT ROP4 (96xx only) */
#define		GE_SCANLINE	0x03	/* Scan Line */
#define		GE_BRESLINE	0x04	/* Bresenham Line */
#define		GE_SHVECTOR	0x05	/* Short Vector */
#define		GE_FASTLINE	0x06	/* Fast Line (96xx only) */
#define		GE_TRAPEZ	0x07	/* Trapezoidal fill (96xx only) */
#define		GE_ELLIPSE	0x08	/* Ellipse (96xx only) (RES) */
#define		GE_ELLIP_FILL	0x09	/* Ellipse Fill (96xx only) (RES)*/
#define	GER_FMIX	0x2127
#define GER_DRAWFLAG	0x2128		/* long */
#define		FASTMODE	1<<28
#define		STENCIL		0x8000
#define		SOLIDFILL	0x4000
#define		TRANS_ENABLE	0x1000
#define 	TRANS_REVERSE	0x2000
#define		YMAJ		0x0400
#define		XNEG		0x0200
#define		YNEG		0x0100
#define		SRCMONO		0x0040
#define		PATMONO		0x0020
#define		SCR2SCR		0x0004
#define		PAT2SCR		0x0002
#define GER_FCOLOUR	0x212C		/* Word for 9440, long for 96xx */
#define GER_BCOLOUR	0x2130		/* Word for 9440, long for 96xx */
#define GER_PATLOC	0x2134		/* Word */
#define GER_DEST_XY	0x2138
#define GER_DEST_X	0x2138		/* Word */
#define GER_DEST_Y	0x213A		/* Word */
#define GER_SRC_XY	0x213C
#define GER_SRC_X	0x213C		/* Word */
#define GER_SRC_Y	0x213E		/* Word */
#define GER_DIM_XY	0x2140
#define GER_DIM_X	0x2140		/* Word */
#define GER_DIM_Y	0x2142		/* Word */
#define GER_STYLE	0x2144		/* Long */
#define GER_CKEY	0x2168		/* Long */
#define GER_FPATCOL	0x2178
#define GER_BPATCOL	0x217C
#define GER_PATTERN	0x2180		/* from 0x2180 to 0x21FF */

/* Additional - Graphics Engine for 96xx */
#define GER_SRCCLIP_XY	0x2148
#define GER_SRCCLIP_X	0x2148		/* Word */
#define GER_SRCCLIP_Y	0x214A		/* Word */
#define GER_DSTCLIP_XY	0x214C
#define GER_DSTCLIP_X	0x214C		/* Word */
#define GER_DSTCLIP_Y	0x214E		/* Word */

/* Defines for IMAGE Graphics Engine */
#define IMAGE_GE_STATUS 	0x2164
#define IMAGE_GE_DRAWENV	0x2120

/* Defines for BLADE Graphics Engine */
#define BLADE_GE_STATUS		0x2120
#define BLADE_XP_GER_OPERMODE	0x2125

#define REPLICATE(r)						\
{								\
	if (pScrn->bitsPerPixel == 16) {			\
		r = ((r & 0xFFFF) << 16) | (r & 0xFFFF);	\
	} else							\
	if (pScrn->bitsPerPixel == 8) { 			\
		r &= 0xFF;					\
		r |= (r<<8);					\
		r |= (r<<16);					\
	}							\
}

#define CHECKCLIPPING					\
	if (pTrident->Clipping)	{			\
		pTrident->Clipping = FALSE;		\
		if (pTrident->Chipset < PROVIDIA9682) { \
			TGUI_SRCCLIP_XY(0,0);		\
			TGUI_DSTCLIP_XY(4095,2047);	\
		}					\
	}


/* Merge XY */
#define XY_MERGE(x,y) \
		((((CARD32)(y)&0xFFFF) << 16) | ((CARD32)(x) & 0xffff))
#define XP_XY_MERGE(y,x) \
		((((CARD32)(y)&0xFFFF) << 16) | ((CARD32)(x) & 0xffff))

#define TRIDENT_WRITE_REG(v,r)					\
        MMIO_OUT32(pTrident->IOBase,(r),(v))

#define TRIDENT_READ_REG(r) \
        MMIO_IN32(pTrident->IOBase,(r))

#define OUTB(addr, data) \
{ \
	if (IsPciCard && UseMMIO) { \
            MMIO_OUT8(pTrident->IOBase, addr, data); \
	} else { \
	    outb(pTrident->PIOBase + (addr), data); \
	} \
}
#define OUTW(addr, data) \
{ \
	if (IsPciCard && UseMMIO) { \
            MMIO_OUT16(pTrident->IOBase, addr, data); \
	} else { \
	    outw(pTrident->PIOBase + (addr), data); \
	} \
}
#define INB(addr) \
( \
	(IsPciCard && UseMMIO) ? \
	    MMIO_IN8(pTrident->IOBase, addr) : \
	    inb(pTrident->PIOBase + (addr)) \
)

#define OUTW_3C4(reg) \
    	OUTW(0x3C4, (tridentReg->tridentRegs3C4[reg])<<8 | (reg))
#define OUTW_3CE(reg) \
    	OUTW(0x3CE, (tridentReg->tridentRegs3CE[reg])<<8 | (reg))
#define OUTW_3x4(reg) \
    	OUTW(vgaIOBase + 4, (tridentReg->tridentRegs3x4[reg])<<8 | (reg))
#define INB_3x4(reg) \
    	OUTB(vgaIOBase + 4, reg); \
    	tridentReg->tridentRegs3x4[reg] = INB(vgaIOBase + 5)
#define INB_3C4(reg) \
    	OUTB(0x3C4, reg); \
    	tridentReg->tridentRegs3C4[reg] = INB(0x3C5);
#define INB_3CE(reg) \
    	OUTB(0x3CE, reg); \
    	tridentReg->tridentRegs3CE[reg] = INB(0x3CF);

#define VIDEOOUT(val,reg) \
	if (pTrident->Chipset >= CYBER9397) { 		\
		OUTW(0x3C4, (val << 8) | reg); 		\
	} else {					\
		OUTB(0x83C8, reg);			\
		OUTB(0x83C6, val);			\
	}


#define BLTBUSY(b) \
	(b = MMIO_IN8(pTrident->IOBase,GER_STATUS) & GE_BUSY)
#define OLDBLTBUSY(b) \
	(b = MMIO_IN8(pTrident->IOBase,OLDGER_STATUS) & GE_BUSY)
#define IMAGE_STATUS(c) \
	MMIO_OUT32(pTrident->IOBase, IMAGE_GE_STATUS, (c))
#define TGUI_STATUS(c) \
	MMIO_OUT8(pTrident->IOBase, GER_STATUS, (c))
#define OLDTGUI_STATUS(c) \
	MMIO_OUT8(pTrident->IOBase, OLDGER_STATUS, (c))
#define TGUI_OPERMODE(c) \
	MMIO_OUT16(pTrident->IOBase, GER_OPERMODE, (c))
#define BLADE_XP_OPERMODE(c) \
	MMIO_OUT8(pTrident->IOBase, BLADE_XP_GER_OPERMODE, (c))
/* XXX */
#define OLDTGUI_OPERMODE(c) \
	{ \
		MMIO_OUT16(pTrident->IOBase, OLDGER_MWIDTH, \
			            vga256InfoRec.displayWidth - 1); \
		MMIO_OUT8(pTrident->IOBase, OLDGER_MFORMAT, (c)); \
	}
#define TGUI_FCOLOUR(c) \
	MMIO_OUT32(pTrident->IOBase, GER_FCOLOUR, (c))
#define TGUI_FPATCOL(c) \
	MMIO_OUT32(pTrident->IOBase, GER_FPATCOL, (c))
#define OLDTGUI_FCOLOUR(c) \
	MMIO_OUT32(pTrident->IOBase, OLDGER_FCOLOUR, (c))
#define TGUI_BCOLOUR(c) \
	MMIO_OUT32(pTrident->IOBase, GER_BCOLOUR, (c))
#define TGUI_BPATCOL(c) \
	MMIO_OUT32(pTrident->IOBase, GER_BPATCOL, (c))
#define OLDTGUI_BCOLOUR(c) \
	MMIO_OUT32(pTrident->IOBase, OLDGER_BCOLOUR, (c))
#define IMAGE_DRAWENV(c) \
	MMIO_OUT32(pTrident->IOBase, IMAGE_GE_DRAWENV, (c))
#define TGUI_DRAWFLAG(c) \
	MMIO_OUT32(pTrident->IOBase, GER_DRAWFLAG, (c))
#define OLDTGUI_STYLE(c) \
	MMIO_OUT16(pTrident->IOBase, OLDGER_STYLE, (c))
#define TGUI_FMIX(c) \
	MMIO_OUT8(pTrident->IOBase, GER_FMIX, (c))
#define OLDTGUI_FMIX(c) \
	MMIO_OUT8(pTrident->IOBase, OLDGER_FMIX, (c))
#define OLDTGUI_BMIX(c) \
	MMIO_OUT8(pTrident->IOBase, OLDGER_BMIX, (c))
#define TGUI_DIM_XY(w,h) \
	MMIO_OUT32(pTrident->IOBase, GER_DIM_XY, XY_MERGE((w)-1,(h)-1))
#define XP_DIM_XY(w,h) \
	MMIO_OUT32(pTrident->IOBase, GER_DIM_XY, XY_MERGE((h),(w)))
#define TGUI_STYLE(c) \
	MMIO_OUT32(pTrident->IOBase, GER_STYLE, (c))
#define OLDTGUI_DIMXY(w,h) \
	MMIO_OUT32(pTrident->IOBase, OLDGER_DIMXY, XY_MERGE((w)-1,(h)-1))
#define TGUI_SRC_XY(x,y) \
	MMIO_OUT32(pTrident->IOBase, GER_SRC_XY, XY_MERGE(x,y))
#define XP_SRC_XY(x,y) \
	MMIO_OUT32(pTrident->IOBase, GER_SRC_XY, XP_XY_MERGE(x,y))
#define TGUI_DEST_XY(x,y) \
	MMIO_OUT32(pTrident->IOBase, GER_DEST_XY, XY_MERGE(x,y))
#define XP_DEST_XY(x,y) \
	MMIO_OUT32(pTrident->IOBase, GER_DEST_XY, XP_XY_MERGE(x,y))
#define OLDTGUI_DESTXY(x,y) \
	MMIO_OUT32(pTrident->IOBase, OLDGER_DESTXY, XY_MERGE(x,y))
#define OLDTGUI_DESTLINEAR(c) \
	MMIO_OUT32(pTrident->IOBase, OLDGER_DESTLINEAR, (c))
#define TGUI_SRCCLIP_XY(x,y) \
	MMIO_OUT32(pTrident->IOBase, GER_SRCCLIP_XY, XY_MERGE(x,y))
#define TGUI_DSTCLIP_XY(x,y) \
	MMIO_OUT32(pTrident->IOBase, GER_DSTCLIP_XY, XY_MERGE(x,y))
#define TGUI_PATLOC(addr) \
	MMIO_OUT16(pTrident->IOBase, GER_PATLOC, (addr))
#define TGUI_CKEY(c) \
	MMIO_OUT32(pTrident->IOBase, GER_CKEY, (c))
#define IMAGEBUSY(b) \
	(b = MMIO_IN32(pTrident->IOBase,IMAGE_GE_STATUS) & 0xF0000000)
#define BLADEBUSY(b) \
	(b = MMIO_IN32(pTrident->IOBase,BLADE_GE_STATUS) & 0xFA800000)
#define IMAGE_OUT(addr, c) \
	MMIO_OUT32(pTrident->IOBase, addr, (c))
#define BLADE_OUT(addr, c) \
	MMIO_OUT32(pTrident->IOBase, addr, (c))
#define TGUI_OUTL(addr, c) \
	MMIO_OUT32(pTrident->IOBase, addr, (c))
#define TGUI_COMMAND(c) \
	MMIO_OUT8(pTrident->IOBase, GER_COMMAND, (c))
#define OLDTGUI_COMMAND(c) \
	do { \
		OLDTGUI_OPERMODE(GE_OP); \
		OLDTGUISync(); \
		MMIO_OUT32(pTrident->IOBase, OLDGER_COMMAND, (c)); \
	} while (0)

/* Cyber FP support */
#define SHADOW_ENABLE(oldval) \
        do {\
	       OUTB(0x3CE, CyberControl); \
	       oldval = INB(0x3CF);\
	       OUTB(0x3CF,oldval | (1 << 6));\
        } while (0)
#define SHADOW_RESTORE(val) \
        do {\
               OUTB(0x3CE, CyberControl); \
	       OUTB(0x3CF,val); \
        } while (0);
