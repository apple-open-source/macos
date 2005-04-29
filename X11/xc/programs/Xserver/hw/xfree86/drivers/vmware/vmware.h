/* **********************************************************
 * Copyright (C) 1998-2001 VMware, Inc.
 * All Rights Reserved
 * Id: vmware.h,v 1.6 2001/01/30 18:13:47 bennett Exp $
 * **********************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/vmware/vmware.h,v 1.12 2003/11/17 22:20:38 dawes Exp $ */

#ifndef VMWARE_H
#define VMWARE_H

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Resources.h"

#include "compiler.h"	        /* inb/outb */

#include "xf86PciInfo.h"	/* pci vendor id */
#include "xf86Pci.h"		/* pci */
#include "xf86Cursor.h"		/* hw cursor */

#include "vgaHW.h"		/* VGA hardware */
#include "fb.h"
#include "xaa.h"

#include "xf86cmap.h"		/* xf86HandleColormaps */

#include "vm_basic_types.h"
#include "svga_reg.h"
#include "svga_struct.h"

#include "offscreen_manager.h"

/* Arbitrarily choose max cursor dimensions.  The emulation doesn't care. */
#define MAX_CURS        32

typedef struct {
    CARD32 svga_reg_enable;
    CARD32 svga_reg_width;
    CARD32 svga_reg_height;
    CARD32 svga_reg_bits_per_pixel;

    CARD32 svga_reg_cursor_on;
    CARD32 svga_reg_cursor_x;
    CARD32 svga_reg_cursor_y;
    CARD32 svga_reg_cursor_id;

    Bool svga_fifo_enabled;
} VMWARERegRec, *VMWARERegPtr;

typedef struct {
    EntityInfoPtr pEnt;
    pciVideoPtr PciInfo;
    PCITAG PciTag;
    Bool Primary;
    int depth;
    int bitsPerPixel;
    rgb weight;
    rgb offset;
    int defaultVisual;
    int videoRam;
    unsigned long memPhysBase;
    unsigned long fbOffset;
    unsigned long fbPitch;
    unsigned long ioBase;
    int maxWidth;
    int maxHeight;
    unsigned int vmwareCapability;

    unsigned char* FbBase;
    unsigned long FbSize;

    VMWARERegRec SavedReg;
    VMWARERegRec ModeReg;

    Bool* pvtSema;

    Bool noAccel;
    Bool hwCursor;
    Bool cursorDefined;
    int cursorSema;
    Bool cursorExcludedForUpdate;
    Bool cursorShouldBeHidden;

    unsigned int cursorRemoveFromFB;
    unsigned int cursorRestoreToFB;

#ifdef RENDER
    CompositeProcPtr Composite;
    void (*EnableDisableFBAccess)(int, Bool);
#endif /* RENDER */

    unsigned long mmioPhysBase;
    unsigned long mmioSize;

    unsigned char* mmioVirtBase;
    CARD32* vmwareFIFO;

    xf86CursorInfoPtr CursorInfoRec;
    struct {
        int bg, fg, x, y;
        BoxRec box;

        uint32 mask[SVGA_BITMAP_SIZE(MAX_CURS, MAX_CURS)];
        uint32 maskPixmap[SVGA_PIXMAP_SIZE(MAX_CURS, MAX_CURS, 32)];
        uint32 source[SVGA_BITMAP_SIZE(MAX_CURS, MAX_CURS)];
        uint32 sourcePixmap[SVGA_PIXMAP_SIZE(MAX_CURS, MAX_CURS, 32)];
    } hwcur;

    IOADDRESS indexReg, valueReg;

    ScreenRec ScrnFuncs;

    /*
     * XAA info rec and misc storage
     */
    XAAInfoRecPtr xaaInfo;
    int xaaFGColor;
    int xaaBGColor;
    int xaaRop;

    unsigned char* xaaColorExpScanLine[1];
    unsigned int xaaColorExpSize; /* size of current scan line in DWords */

    Heap* heap;
    SVGASurface* frontBuffer;

    SVGASurface* curPict;
    int op;

} VMWARERec, *VMWAREPtr;

#define VMWAREPTR(p) ((VMWAREPtr)((p)->driverPrivate))

static __inline ScrnInfoPtr infoFromScreen(ScreenPtr s) {
    return xf86Screens[s->myNum];
}

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define ABS(x) (((x) >= 0) ? (x) : -(x))

#define BOX_INTERSECT(a, b) \
		(ABS(((a).x1 + (a).x2) - ((b).x1 + (b).x2)) <= \
		((a).x2 - (a).x1) + ((b).x2 - (b).x1) && \
		ABS(((a).y1 + (a).y2) - ((b).y1 + (b).y2)) <= \
		((a).y2 - (a).y1) + ((b).y2 - (b).y1))

#define SVGA_GLYPH_SCANLINE_SIZE_DWORDS(w) (((w) + 31) >> 5)

#define PRE_OP_HIDE_CURSOR() \
    if (pVMWARE->cursorDefined && *pVMWARE->pvtSema) { \
        pVMWARE->cursorSema++; \
        if (pVMWARE->cursorSema == 1) { \
            vmwareWriteCursorRegs(pVMWARE, FALSE, FALSE); \
        } \
    }
#define POST_OP_SHOW_CURSOR() \
    if (pVMWARE->cursorDefined && *pVMWARE->pvtSema) { \
        pVMWARE->cursorSema--; \
        if (!pVMWARE->cursorSema && !pVMWARE->cursorShouldBeHidden) { \
            vmwareWriteCursorRegs(pVMWARE, TRUE, FALSE); \
        } \
    }

#define MOUSE_ID 1

extern const char *vmwareXaaSymbols[];

/*#define DEBUG_LOGGING*/
#ifdef DEBUG_LOGGING
# define VmwareLog(args) ErrorF args
# define TRACEPOINT VmwareLog((__FUNCTION__ ":" __FILE__ "\n"));
#else
# define VmwareLog(args)
# define TRACEPOINT
#endif

/* Undefine this to kill all acceleration */
#define ACCELERATE_OPS

void vmwareWriteReg(
   VMWAREPtr pVMWARE, int index, CARD32 value
   );

CARD32 vmwareReadReg(
    VMWAREPtr pVMWARE, int index
    );

void vmwareWriteWordToFIFO(
   VMWAREPtr pVMWARE, CARD32 value
   );

void vmwareWaitForFB(
   VMWAREPtr pVMWARE
   );

void vmwareSendSVGACmdUpdate(
   VMWAREPtr pVMWARE, BoxPtr pBB
   );

/* vmwarecurs.c */
Bool vmwareCursorInit(
   ScreenPtr pScr
   );

void vmwareCursorModeInit(
    ScrnInfoPtr pScrn,
    DisplayModePtr mode
   );

void vmwareCursorCloseScreen(
    ScreenPtr pScr
    );

void vmwareWriteCursorRegs(
   VMWAREPtr pVMWARE,
   Bool visible,
   Bool force
   );

void vmwareCursorHookWrappers(
   ScreenPtr pScreen
   );


/* vmwarexaa.c */
Bool vmwareXAAScreenInit(
   ScreenPtr pScreen
   );

Bool vmwareXAAModeInit(
    ScrnInfoPtr pScrn, DisplayModePtr mode
    );

void vmwareXAACloseScreen(
   ScreenPtr pScreen
   );

#endif
