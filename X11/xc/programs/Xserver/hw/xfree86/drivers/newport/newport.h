/* 
 * Id: newport.h,v 1.4 2000/11/29 20:58:10 agx Exp $
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/newport/newport.h,v 1.10 2002/12/10 04:03:00 dawes Exp $ */

#ifndef __NEWPORT_H__
#define __NEWPORT_H__

/*
 * All drivers should include these:
 */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86Resources.h"

#include "xf86cmap.h"

/* xaa & hardware cursor */
#include "xaa.h"
#include "xf86Cursor.h"

/* register definitions of the Newport card */
#include "newport_regs.h"

#define NEWPORT_BASE_ADDR0  0x1f0f0000
#define NEWPORT_BASE_OFFSET 0x00400000
#define NEWPORT_MAX_BOARDS 4

#if 0
# define DEBUG 1
#endif

#ifdef DEBUG
# define TRACE_ENTER(str)       ErrorF("newport: " str " %d\n",pScrn->scrnIndex)
# define TRACE_EXIT(str)        ErrorF("newport: " str " done\n")
# define TRACE(str)             ErrorF("newport trace: " str "\n")
#else
# define TRACE_ENTER(str)
# define TRACE_EXIT(str)
# define TRACE(str)
#endif

typedef struct {
	unsigned busID;
	int bitplanes; 
	/* revision numbers of the various pieces of silicon */
	unsigned int board_rev, cmap_rev, rex3_rev, xmap9_rev, bt445_rev;
	/* shadow copies of frequently used registers */
	NewportRegsPtr pNewportRegs;	/* Pointer to REX3 registers */
	npireg_t drawmode1;		/* REX3 drawmode1 common to all drawing operations */
	CARD16 vc2ctrl;                 /* VC2 control register */

	/* ShadowFB stuff: */
	CARD32* ShadowPtr;
	unsigned long int ShadowPitch;
	unsigned int Bpp;		/* Bytes per pixel */

	/* HWCursour stuff: */
	Bool hwCursor;
	xf86CursorInfoPtr CursorInfoRec;
	CARD16 curs_cmap_base;          /* MSB of the cursor's cmap */

	/* wrapped funtions: */
	CloseScreenProcPtr  CloseScreen;

	/* newport register backups: */
	npireg_t txt_drawmode0;		/* Rex3 drawmode0 register */
	npireg_t txt_drawmode1;		/* Rex3 drawmode1 register */
	npireg_t txt_wrmask;		/* Rex3 write mask register */
	npireg_t txt_smask1x;		/* Rex3 screen mask 1 registers */
	npireg_t txt_smask1y;
	npireg_t txt_smask2x;		/* Rex3 screen mask 2 registers */
	npireg_t txt_smask2y;
	npireg_t txt_clipmode;		/* Rex3 clip mode register */

	CARD16 txt_vc2ctrl;             /* VC2 control register */
	CARD16 txt_vc2cur_x;            /* VC2 hw cursor x location */
	CARD16 txt_vc2cur_y;            /* VC2 hw cursor x location */
	CARD32  txt_vc2cur_data[64];    /* VC2 hw cursor glyph data */

	CARD8  txt_xmap9_cfg0;		/* 0. Xmap9's control register */
	CARD8  txt_xmap9_cfg1;		/* 1. Xmap9's control register */
	CARD8  txt_xmap9_ccmsb;         /* cursor cmap msb */
	CARD8  txt_xmap9_mi;		/* Xmap9s' mode index register */
	CARD32 txt_xmap9_mod0;		/* Xmap9s' mode 0 register */

	LOCO txt_colormap[256];

	OptionInfoPtr Options;
} NewportRec, *NewportPtr;

#define NEWPORTPTR(p) ((NewportPtr)((p)->driverPrivate))
#define NEWPORTREGSPTR(p) ((NEWPORTPTR(p))->pNewportRegs)

/* Newport_regs.c */
unsigned short NewportVc2Get(NewportRegsPtr, unsigned char vc2Ireg);
void NewportVc2Set(NewportRegsPtr pNewportRegs, unsigned char vc2Ireg, unsigned short val);
void NewportWait(NewportRegsPtr pNewportRegs);
void NewportBfwait(NewportRegsPtr pNewportRegs);
void NewportXmap9SetModeRegister(NewportRegsPtr pNewportRegs, CARD8 address, CARD32 mode);
CARD32 NewportXmap9GetModeRegister(NewportRegsPtr pNewportRegs, unsigned chip, CARD8 address);
void NewportBackupRex3( ScrnInfoPtr pScrn);
void NewportRestoreRex3( ScrnInfoPtr pScrn);
void NewportBackupXmap9s( ScrnInfoPtr pScrn);
void NewportRestoreXmap9s( ScrnInfoPtr pScrn);
void NewportBackupVc2( ScrnInfoPtr pScrn);
void NewportRestoreVc2( ScrnInfoPtr pScrn);
void NewportBackupVc2Cursor( ScrnInfoPtr pScrn);
void NewportRestoreVc2Cursor( ScrnInfoPtr pScrn);

/* newort_cmap.c */
void NewportLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices, 
			LOCO* colors, VisualPtr pVisual);
void NewportRestorePalette(ScrnInfoPtr pScrn);
void NewportBackupPalette(ScrnInfoPtr pScrn);
void NewportCmapSetRGB( NewportRegsPtr pNewportRegs, unsigned short addr, LOCO color);

/* newport_shadow.c */
void NewportRefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void NewportRefreshArea24(ScrnInfoPtr pScrn, int num, BoxPtr pbox);

/* newport_cursor.c */
Bool NewportHWCursorInit(ScreenPtr pScreen);
void NewportLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *bits);

#endif /* __NEWPORT_H__ */
