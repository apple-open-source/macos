/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/apm/apm_funcs.c,v 1.19 2003/11/03 05:11:04 tsi Exp $ */

#define FASTER
#ifndef PSZ
#define PSZ	8
#endif
#ifdef IOP_ACCESS
#  define APM_SUFF_IOP	"_IOP"
#  undef	RDXB
#  undef	RDXW
#  undef	RDXL
#  undef	WRXB
#  undef	WRXW
#  undef	WRXL
#  undef	ApmWriteSeq
#  define RDXB	RDXB_IOP
#  define RDXW	RDXW_IOP
#  define RDXL	RDXL_IOP
#  define WRXB	WRXB_IOP
#  define WRXW	WRXW_IOP
#  define WRXL	WRXL_IOP
#  define ApmWriteSeq(i, v)	wrinx(pApm->xport, i, v)
#else
#  define APM_SUFF_IOP	""
#endif
#if PSZ == 24
#  define APM_SUFF_24	"24"
#  ifdef IOP_ACCESS
#    define A(s)		Apm##s##24##_IOP
#  else
#    define A(s)		Apm##s##24
#  endif
#else
#  define APM_SUFF_24	""
#  ifdef IOP_ACCESS
#    define A(s)		Apm##s##_IOP
#  else
#    define A(s)		Apm##s
#  endif
#endif
#define	DPRINTNAME(s)	do { xf86DrvMsgVerb(pScrn->pScreen->myNum, X_NOTICE, 6, "Apm" #s APM_SUFF_24 APM_SUFF_IOP "\n"); } while (0)

#if PSZ == 24
#undef SETSOURCEXY
#undef SETDESTXY
#undef SETWIDTH
#undef SETWIDTHHEIGHT
#undef UPDATEDEST
#define SETSOURCEXY(x,y)	do { int off = ((((y) & 0xFFFF) * pApm->CurrentLayout.displayWidth + ((x) & 0x3FFF)) * 3); SETSOURCEOFF(((off & 0xFFF000) << 4) | (off & 0xFFF)); break;} while(1)
#define SETDESTXY(x,y)	do { int off = ((((y) & 0xFFFF) * pApm->CurrentLayout.displayWidth + ((x) & 0x3FFF)) * 3); SETDESTOFF(((off & 0xFFF000) << 4) | (off & 0xFFF)); break;} while(1)
#define SETWIDTH(w)		WRXW(0x58, ((w) & 0x3FFF) * 3)
#define SETWIDTHHEIGHT(w,h)	WRXL(0x58, ((h) << 16) | (((w) & 0x3FFF) * 3))
#define UPDATEDEST(x,y)		(void)(curr32[0x54 / 4] = ((((y) & 0xFFFF) * pApm->CurrentLayout.displayWidth + ((x) & 0xFFFF)) * 3))
#endif

/* Defines */
#define MAXLOOP 1000000


/* Local functions */
static void A(Sync)(ScrnInfoPtr pScrn);
static void A(SetupForSolidFill)(ScrnInfoPtr pScrn, int color, int rop,
					unsigned int planemask);
static void A(SubsequentSolidFillRect)(ScrnInfoPtr pScrn, int x, int y,
				       int w, int h);
static void A(SetupForScreenToScreenCopy)(ScrnInfoPtr pScrn, int xdir, int ydir,
					  int rop, unsigned int planemask,
                                          int transparency_color);
static void A(SubsequentScreenToScreenCopy)(ScrnInfoPtr pScrn, int x1, int y1,
					    int x2, int y2, int w, int h);
#if PSZ != 24
static void A(Sync6422)(ScrnInfoPtr pScrn);
static void A(WriteBitmap)(ScrnInfoPtr pScrn, int x, int y, int w, int h,
			    unsigned char *src, int srcwidth, int skipleft,
			    int fg, int bg, int rop, unsigned int planemask);
static void A(TEGlyphRenderer)(ScrnInfoPtr pScrn, int x, int y, int w, int h,
				int skipleft, int startline, 
				unsigned int **glyphs, int glyphWidth,
				int fg, int bg, int rop, unsigned planemask);
static void A(SetupForMono8x8PatternFill)(ScrnInfoPtr pScrn, int patx, int paty,
				          int fg, int bg, int rop,
				          unsigned int planemask);
static void A(SubsequentMono8x8PatternFillRect)(ScrnInfoPtr pScrn, int patx,
					        int paty, int x, int y,
					        int w, int h);
#if 0
static void A(SetupForCPUToScreenColorExpandFill)(ScrnInfoPtr pScrn, int bg, int fg, int rop, unsigned int planemask);
static void A(SubsequentCPUToScreenColorExpandFill)(ScrnInfoPtr pScrn, int x, int y, int w, int h, int skipleft);
#endif
static void A(SetupForScreenToScreenColorExpandFill)(ScrnInfoPtr pScrn,
						     int fg, int bg, int rop,
						     unsigned int planemask);
static void A(SetupForImageWrite)(ScrnInfoPtr pScrn, int rop,
				  unsigned int planemask, int trans_color,
				  int bpp, int depth);
static void A(SubsequentImageWriteRect)(ScrnInfoPtr pScrn, int x, int y,
					int w, int h, int skipleft);
static void A(SubsequentScreenToScreenColorExpandFill)(ScrnInfoPtr pScrn,
						       int x, int y,
						       int w, int h,
						       int srcx, int srcy,
						       int offset);
static void A(SubsequentSolidBresenhamLine)(ScrnInfoPtr pScrn, int x1, int y1, int octant, int err, int e1, int e2, int length);
static void A(SubsequentSolidBresenhamLine6422)(ScrnInfoPtr pScrn, int x1, int y1, int octant, int err, int e1, int e2, int length);
static void A(SetClippingRectangle)(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2);
static void A(WritePixmap)(ScrnInfoPtr pScrn, int x, int y, int w, int h,
			   unsigned char *src, int srcwidth, int rop,
			   unsigned int planemask, int trans, int bpp,
			   int depth);
static void A(FillImageWriteRects)(ScrnInfoPtr pScrn, int rop,
				    unsigned int planemask,
				    int nBox, BoxPtr pBox, int xorg, int yorg,
				    PixmapPtr pPix);
static void A(SetupForColor8x8PatternFill)(ScrnInfoPtr pScrn,int patx,int paty,
				           int rop, unsigned int planemask,
					   int transparency_color);
static void A(SubsequentColor8x8PatternFillRect)(ScrnInfoPtr pScrn, int patx,
					         int paty, int x, int y,
					         int w, int h);
#endif

/* Inline functions */
static __inline__ void
A(WaitForFifo)(ApmPtr pApm, int slots)
{
  if (!pApm->UsePCIRetry) {
    volatile int i;

    for(i = 0; i < MAXLOOP; i++) {
      if ((STATUS() & STATUS_FIFO) >= slots)
	break;
    }
    if (i == MAXLOOP) {
      unsigned int status = STATUS();

      WRXB(0x1FF, 0);
      if (!xf86ServerIsExiting())
	  FatalError("Hung in WaitForFifo() (Status = 0x%08X)\n", status);
    }
  }
}


static void
A(SetupForSolidFill)(ScrnInfoPtr pScrn, int color, int rop,
			unsigned int planemask)
{
  APMDECL(pScrn);

  DPRINTNAME(SetupForSolidFill);
#ifdef FASTER
  A(WaitForFifo)(pApm, 3 + pApm->apmClip);
  SETDEC(DEC_QUICKSTART_ONDIMX | DEC_OP_RECT | DEC_DEST_UPD_TRCORNER |
	  pApm->CurrentLayout.Setup_DEC);
#else
  A(WaitForFifo)(pApm, 2 + pApm->apmClip);
#endif
#if PSZ == 2
  pApm->color = ((color & 0xFF0000) << 8) | ((color & 0xFF0000) >> 16) |
		  ((color & 0xFF00) << 8) | ((color & 0xFF) << 16);
#else
  SETFOREGROUNDCOLOR(color);
#endif

  if (pApm->apmClip)  {
    SETCLIP_CTRL(0);
    pApm->apmClip = FALSE;
  }

  SETROP(apmROP[rop]);
}

static void
A(SubsequentSolidFillRect)(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
  APMDECL(pScrn);

  DPRINTNAME(SubsequentSolidFillRect);
#if PSZ == 24
#  ifndef FASTER
  A(WaitForFifo)(pApm, 5);
#  else
  A(WaitForFifo)(pApm, 4);
#  endif
  SETOFFSET(3*(pApm->CurrentLayout.displayWidth - w));
#if 0
  switch ((((y * pApm->CurrentLayout.displayWidth + x)* 3) / 8) % 3) {
  case 0:
      SETFOREGROUNDCOLOR(pApm->color);
      break;
  case 1:
      SETFOREGROUNDCOLOR((pApm->color << 8) | (pApm->color >> 16));
      break;
  case 2:
      SETFOREGROUNDCOLOR(pApm->color >> 8);
      break;
  }
#endif
#else
#  ifndef FASTER
  A(WaitForFifo)(pApm, 3);
#  else
  A(WaitForFifo)(pApm, 2);
#  endif
#endif
  SETDESTXY(x, y);
  SETWIDTHHEIGHT(w, h);
  UPDATEDEST(x + w + 1, y);
#ifndef FASTER
  SETDEC(DEC_START | DEC_OP_RECT | DEC_DEST_UPD_TRCORNER | pApm->CurrentLayout.Setup_DEC);
#endif
}

static void
A(SetupForScreenToScreenCopy)(ScrnInfoPtr pScrn, int xdir, int ydir, int rop,
				unsigned int planemask, int transparency_color)
{
  unsigned char tmp;

  APMDECL(pScrn);

  DPRINTNAME(SetupForScreenToScreenCopy);

  if (pApm->apmLock) {
    /*
     * This is just an attempt, because Daryll is tampering with MY registers.
     */
    tmp = (RDXB(0xDB) & 0xF4) |  0x0A;
    WRXB(0xDB, tmp);
    ApmWriteSeq(0x1B, 0x20);
    ApmWriteSeq(0x1C, 0x2F);
    pApm->apmLock = FALSE;
  }

  pApm->blitxdir = xdir;
  pApm->blitydir = ydir;

  pApm->apmTransparency = (transparency_color != -1);

#ifdef FASTER
  A(WaitForFifo)(pApm, 2 + (transparency_color != -1));
  SETDEC(DEC_QUICKSTART_ONDIMX | DEC_OP_BLT | DEC_DEST_UPD_TRCORNER |
	  (pApm->apmTransparency ? DEC_SOURCE_TRANSPARENCY : 0) | pApm->CurrentLayout.Setup_DEC |
	  ((xdir < 0) ? DEC_DIR_X_NEG : DEC_DIR_X_POS) |
	  ((ydir < 0) ? DEC_DIR_Y_NEG : DEC_DIR_Y_POS));
#else
  A(WaitForFifo)(pApm, 1 + (transparency_color != -1));
#endif

  if (transparency_color != -1)
    SETBACKGROUNDCOLOR(transparency_color);

  SETROP(apmROP[rop]);
}

static void
A(SubsequentScreenToScreenCopy)(ScrnInfoPtr pScrn, int x1, int y1,
				    int x2, int y2, int w, int h)
{
  APMDECL(pScrn);
#ifndef FASTER
  u32		c = pApm->apmTransparency ? DEC_SOURCE_TRANSPARENCY : 0;
#endif
  u32		sx, dx, sy, dy;
  int		i = y1 / pApm->CurrentLayout.Scanlines;

  DPRINTNAME(SubsequentScreenToScreenCopy);
  if (i && pApm->pixelStride) {
#ifdef FASTER
    A(WaitForFifo)(pApm, 1);
    SETDEC(curr32[0x40 / 4] | (DEC_SOURCE_CONTIG | DEC_SOURCE_LINEAR));
#else
    c |= DEC_SOURCE_LINEAR | DEC_SOURCE_CONTIG;
#endif
    pApm->apmClip = TRUE;
    A(WaitForFifo)(pApm, 3);
    SETCLIP_LEFTTOP(x2, y2);
    SETCLIP_RIGHTBOT(x2 + w - 1, y2 + h - 1);
    SETCLIP_CTRL(1);
    w = (pApm->pixelStride * 8) / pApm->CurrentLayout.bitsPerPixel;
  }
  else {
#ifdef FASTER
    A(WaitForFifo)(pApm, 1 + pApm->apmClip);
    SETDEC(curr32[0x40 / 4] & ~(DEC_SOURCE_CONTIG | DEC_SOURCE_LINEAR));
    if (pApm->apmClip)
	SETCLIP_CTRL(0);
    pApm->apmClip = FALSE;
#else
    if (pApm->apmClip) {
	A(WaitForFifo)(pApm, 1);
	SETCLIP_CTRL(0);
	pApm->apmClip = FALSE;
    }
#endif
  }
  if (i) {
      if (pApm->pixelStride) {
	  x1 += (((y1 % pApm->CurrentLayout.Scanlines) - pApm->RushY[i - 1]) * pApm->pixelStride * 8) / pApm->CurrentLayout.bitsPerPixel;
	  y1 = pApm->RushY[i - 1];
      }
      else
	  y1 -= i * pApm->CurrentLayout.Scanlines;
  }
  if (pApm->blitxdir < 0)
  {
#ifndef FASTER
    c |= DEC_DIR_X_NEG;
#endif
    sx = x1+w-1;
    dx = x2+w-1;
  }
  else
  {
#ifndef FASTER
    c |= DEC_DIR_X_POS;
#endif
    sx = x1;
    dx = x2;
  }

  if (pApm->blitydir < 0)
  {
#ifndef FASTER
    c |= DEC_DIR_Y_NEG | DEC_START | DEC_OP_BLT | DEC_DEST_UPD_TRCORNER |
	    pApm->CurrentLayout.Setup_DEC;
#endif
    sy = y1+h-1;
    dy = y2+h-1;
  }
  else
  {
#ifndef FASTER
    c |= DEC_DIR_Y_POS | DEC_START | DEC_OP_BLT | DEC_DEST_UPD_TRCORNER |
	    pApm->CurrentLayout.Setup_DEC;
#endif
    sy = y1;
    dy = y2;
  }

#if PSZ == 24
#  ifndef FASTER
  A(WaitForFifo)(pApm, 5);
#  else
  A(WaitForFifo)(pApm, 4);
#  endif
  if (pApm->blitxdir == pApm->blitydir)
    SETOFFSET(3 * (pApm->CurrentLayout.displayWidth - w));
  else
    SETOFFSET(3 * (pApm->CurrentLayout.displayWidth + w));
#else
#  ifndef FASTER
  A(WaitForFifo)(pApm, 4);
#  else
  A(WaitForFifo)(pApm, 3);
#  endif
#endif

  if (i && pApm->pixelStride) {
    register unsigned int off = sx + sy * pApm->CurrentLayout.displayWidth;

    SETSOURCEOFF(((off & 0xFFF000) << 4) | (off & 0xFFF));
  }
  else
    SETSOURCEXY(sx,sy);
  SETDESTXY(dx,dy);
  SETWIDTHHEIGHT(w,h);
  UPDATEDEST(dx + (w + 1)*pApm->blitxdir, dy);

#ifndef FASTER
  SETDEC(c);
#endif
  if (i) A(Sync)(pScrn);	/* Only for AT3D */
}


#if PSZ != 24
static void
A(SetupForScreenToScreenColorExpandFill)(ScrnInfoPtr pScrn, int fg, int bg,
					 int rop, unsigned int planemask)
{
  APMDECL(pScrn);

  DPRINTNAME(SetupForScreenToScreenColorExpandFill);
  A(WaitForFifo)(pApm, 3 + pApm->apmClip);
  if (bg == -1)
  {
    SETFOREGROUNDCOLOR(fg);
    SETBACKGROUNDCOLOR(fg+1);
    pApm->apmTransparency = TRUE;
  }
  else
  {
    SETFOREGROUNDCOLOR(fg);
    SETBACKGROUNDCOLOR(bg);
    pApm->apmTransparency = FALSE;
  }

  SETROP(apmROP[rop]);
}

static void
A(WriteBitmap)(ScrnInfoPtr pScrn, int x, int y, int w, int h,
		unsigned char *src, int srcwidth, int skipleft,
		int fg, int bg, int rop, unsigned int planemask)
{
    APMDECL(pScrn);
    Bool	beCareful, apmClip = FALSE;
    int		wc, n, nc, wr, wrd;
    CARD32	*dstPtr;
#ifndef FASTER
    int		c;
#endif

    DPRINTNAME(WriteBitmap);

    if (w <= 0 && h <= 0)
	return;

    /*
     * The function is a bit long, but the spirit is simple : put the monochrome
     * data in scratch memory and color-expand it using the
     * ScreenToScreenColorExpand techniques.
     */

    w += skipleft;
    x -= skipleft;
    wc = pApm->ScratchMemSize * 8;
    wrd = (w + 31) >> 5;
    wr = wrd << 5;
    nc = wc / wr;
    if (nc > h)
	nc = h;
    if (wr / 8 > srcwidth)
	beCareful = TRUE;
    else
	beCareful = FALSE;
    srcwidth -= wr / 8;

    if (skipleft || w != wr) {
	apmClip = TRUE;
	A(WaitForFifo)(pApm, 3);
	SETCLIP_LEFTTOP(x + skipleft, y);
	SETCLIP_RIGHTBOT(x + w - 1, y + h - 1);
	SETCLIP_CTRL(1);
    }
    else if (pApm->apmClip) {
	A(WaitForFifo)(pApm, 1);
	SETCLIP_CTRL(0);
    }
    pApm->apmClip = FALSE;

    A(SetupForScreenToScreenColorExpandFill)(pScrn, fg, bg, rop, planemask);
#ifdef FASTER
    A(WaitForFifo)(pApm, 2);
    if (pApm->apmTransparency)
    SETDEC(DEC_OP_BLT | DEC_DIR_X_POS | DEC_DIR_Y_POS | DEC_SOURCE_MONOCHROME |
        DEC_QUICKSTART_ONDIMX | DEC_DEST_UPD_BLCORNER | DEC_SOURCE_LINEAR |
        DEC_SOURCE_CONTIG | DEC_SOURCE_TRANSPARENCY | pApm->CurrentLayout.Setup_DEC);
    else
    SETDEC(DEC_OP_BLT | DEC_DIR_X_POS | DEC_DIR_Y_POS | DEC_SOURCE_MONOCHROME |
        DEC_QUICKSTART_ONDIMX | DEC_DEST_UPD_BLCORNER | DEC_SOURCE_LINEAR |
        DEC_SOURCE_CONTIG | pApm->CurrentLayout.Setup_DEC);
#else
    A(WaitForFifo)(pApm, 1);
    c = DEC_OP_BLT | DEC_DIR_X_POS | DEC_DIR_Y_POS | DEC_SOURCE_MONOCHROME |
        DEC_START | DEC_DEST_UPD_BLCORNER | DEC_SOURCE_LINEAR |
        DEC_SOURCE_CONTIG | pApm->CurrentLayout.Setup_DEC;
    if (pApm->apmTransparency)
      c |= DEC_SOURCE_TRANSPARENCY;
#endif

    SETDESTXY(x, y);

    if (!beCareful || h % nc > 3 || (w > 16 && h % nc)) {
#ifndef FASTER
	if (h / nc)
	    SETWIDTHHEIGHT(wr, nc);
#endif
	for (n = h / nc; n-- > 0; ) {
	    int i, j;

	    if (pApm->ScratchMemPtr + nc * wrd * 4 < pApm->ScratchMemEnd) {
#define		d	((memType)dstPtr - (memType)pApm->FbBase)
		A(WaitForFifo)(pApm, 1);
		dstPtr = (CARD32 *)pApm->ScratchMemPtr;
		switch(pApm->CurrentLayout.bitsPerPixel) {
		case 8: case 24:
		    SETSOURCEOFF((d & 0xFFF000) << 4 |
				(d & 0xFFF));
		    break;
		case 16:
		    SETSOURCEOFF((d & 0xFFE000) << 3 |
				((d & 0x1FFE) >> 1));
		    break;
		case 32:
		    SETSOURCEOFF((d & 0xFFC000) << 2 |
				((d & 0x3FFC) >> 2));
		    break;
		}
#undef		d
	    }
	    else {
		(*pApm->AccelInfoRec->Sync)(pScrn);
		dstPtr = (CARD32 *)pApm->ScratchMemOffset;
		SETSOURCEOFF(pApm->ScratchMem);
	    }
	    pApm->ScratchMemPtr = ((memType)(dstPtr + wrd * nc) + 4) 
	      & ~(memType)7;
	    for (i = nc; i-- > 0; ) {
		for (j = wrd; j-- > 0; ) {
		    *dstPtr++ = XAAReverseBitOrder(*(CARD32 *)src);
		    src += 4;
		}
		src += srcwidth;
	    }
	    A(WaitForFifo)(pApm, 1);
#ifdef FASTER
	    SETWIDTHHEIGHT(wr, nc);
#else
	    SETDEC(c);
#endif
	}
    }
    else {
#ifndef FASTER
	if (h / nc)
	    SETWIDTHHEIGHT(wr, nc);
#endif
	for (n = h / nc; n-- > 0; ) {
	    int i, j;

	    if (pApm->ScratchMemPtr + nc * wrd * 4 < pApm->ScratchMemEnd) {
#define		d	((memType)dstPtr - (memType)pApm->FbBase)
		A(WaitForFifo)(pApm, 1);
		dstPtr = (CARD32 *)pApm->ScratchMemPtr;
		switch(pApm->CurrentLayout.bitsPerPixel) {
		case 8: case 24:
		    SETSOURCEOFF((d & 0xFFF000) << 4 |
				(d & 0xFFF));
		    break;
		case 16:
		    SETSOURCEOFF((d & 0xFFE000) << 3 |
				((d & 0x1FFE) >> 1));
		    break;
		case 32:
		    SETSOURCEOFF((d & 0xFFC000) << 2 |
				((d & 0x3FFC) >> 2));
		    break;
		}
#undef		d
	    }
	    else {
		(*pApm->AccelInfoRec->Sync)(pScrn);
		dstPtr = (CARD32 *)pApm->ScratchMemOffset;
		SETSOURCEOFF(pApm->ScratchMem);
	    }
	    pApm->ScratchMemPtr = ((memType)(dstPtr + wrd * nc * 4) + 4) & ~7;
	    for (i = nc; i-- > 0; ) {
		for (j = wrd; j-- > 0; ) {
		    if (i || j || n)
			*dstPtr++ = XAAReverseBitOrder(*(CARD32 *)src);
		    else if (srcwidth > -8) {
			((CARD8 *)dstPtr)[0] = byte_reversed[((CARD8 *)src)[2]];
			((CARD8 *)dstPtr)[1] = byte_reversed[((CARD8 *)src)[1]];
			((CARD8 *)dstPtr)[2] = byte_reversed[((CARD8 *)src)[0]];
			dstPtr = (CARD32 *)(3 + (CARD8 *)dstPtr);
		    }
		    else if (srcwidth > -16) {
			((CARD8 *)dstPtr)[0] = byte_reversed[((CARD8 *)src)[1]];
			((CARD8 *)dstPtr)[1] = byte_reversed[((CARD8 *)src)[0]];
			dstPtr = (CARD32 *)(2 + (CARD8 *)dstPtr);
		    }
		    else {
			*(CARD8 *)dstPtr = byte_reversed[*(CARD8 *)src];
			dstPtr = (CARD32 *)(1 + (CARD8 *)dstPtr);
		    }
		    src += 4;
		}
		src += srcwidth;
	    }
	    A(WaitForFifo)(pApm, 1);
#ifdef FASTER
	    SETWIDTHHEIGHT(wr, nc);
#else
	    SETDEC(c);
#endif
	}
    }

    /*
     * Same thing for the remnant
     */
    UPDATEDEST(x, y + h + 1);
    h %= nc;
    if (h) {
	if (!beCareful) {
	    int i, j;

#ifndef FASTER
	    SETWIDTHHEIGHT(wr, h);
#endif
	    if (pApm->ScratchMemPtr + h * wrd * 4 < pApm->ScratchMemEnd) {
#define		d	((memType)dstPtr - (memType)pApm->FbBase)
		A(WaitForFifo)(pApm, 1);
		dstPtr = (CARD32 *)pApm->ScratchMemPtr;
		switch(pApm->CurrentLayout.bitsPerPixel) {
		case 8: case 24:
		    SETSOURCEOFF((d & 0xFFF000) << 4 |
				(d & 0xFFF));
		    break;
		case 16:
		    SETSOURCEOFF((d & 0xFFE000) << 3 |
				((d & 0x1FFE) >> 1));
		    break;
		case 32:
		    SETSOURCEOFF((d & 0xFFC000) << 2 |
				((d & 0x3FFC) >> 2));
		    break;
		}
#undef		d
	    }
	    else {
		(*pApm->AccelInfoRec->Sync)(pScrn);
		dstPtr = (CARD32 *)pApm->ScratchMemOffset;
		SETSOURCEOFF(pApm->ScratchMem);
	    }
	    pApm->ScratchMemPtr = ((memType)(dstPtr + wrd * h) + 4) & ~7;
	    for (i = h; i-- > 0; ) {
		for (j = wrd; j-- > 0; ) {
		    *dstPtr++ = XAAReverseBitOrder(*(CARD32 *)src);
		    src += 4;
		}
		src += srcwidth;
	    }
	    A(WaitForFifo)(pApm, 1);
#ifdef FASTER
	    SETWIDTHHEIGHT(wr, h);
#else
	    SETDEC(c);
#endif
	}
	else {
	    int i, j;

#ifndef FASTER
	    SETWIDTHHEIGHT(w, h);
#endif
	    if (pApm->ScratchMemPtr + h * wrd * 4 < pApm->ScratchMemEnd) {
#define		d	((memType)dstPtr - (memType)pApm->FbBase)
		A(WaitForFifo)(pApm, 1);
		dstPtr = (CARD32 *)pApm->ScratchMemPtr;
		switch(pApm->CurrentLayout.bitsPerPixel) {
		case 8: case 24:
		    SETSOURCEOFF((d & 0xFFF000) << 4 |
				(d & 0xFFF));
		    break;
		case 16:
		    SETSOURCEOFF((d & 0xFFE000) << 3 |
				((d & 0x1FFE) >> 1));
		    break;
		case 32:
		    SETSOURCEOFF((d & 0xFFC000) << 2 |
				((d & 0x3FFC) >> 2));
		    break;
		}
#undef		d
	    }
	    else {
		(*pApm->AccelInfoRec->Sync)(pScrn);
		dstPtr = (CARD32 *)pApm->ScratchMemOffset;
		SETSOURCEOFF(pApm->ScratchMem);
	    }
	    pApm->ScratchMemPtr = ((memType)(dstPtr + wrd * h) + 4) & ~7;
	    for (i = h; i-- > 0; ) {
		for (j = wrd; j-- > 0; ) {
		    if (i || j)
			*dstPtr++ = XAAReverseBitOrder(*(CARD32 *)src);
		    else if (srcwidth > -8) {
			((CARD8 *)dstPtr)[0] = byte_reversed[((CARD8 *)src)[2]];
			((CARD8 *)dstPtr)[1] = byte_reversed[((CARD8 *)src)[1]];
			((CARD8 *)dstPtr)[2] = byte_reversed[((CARD8 *)src)[0]];
			dstPtr = (CARD32 *)(3 + (CARD8 *)dstPtr);
		    }
		    else if (srcwidth > -16) {
			((CARD8 *)dstPtr)[0] = byte_reversed[((CARD8 *)src)[1]];
			((CARD8 *)dstPtr)[1] = byte_reversed[((CARD8 *)src)[0]];
			dstPtr = (CARD32 *)(2 + (CARD8 *)dstPtr);
		    }
		    else {
			*(CARD8 *)dstPtr = byte_reversed[*(CARD8 *)src];
			dstPtr = (CARD32 *)(1 + (CARD8 *)dstPtr);
		    }
		    src += 4;
		}
		src += srcwidth;
	    }
	    A(WaitForFifo)(pApm, 1);
#ifdef FASTER
	    SETWIDTHHEIGHT(w, h);
#else
	    SETDEC(c);
#endif
	}
    }
    pApm->apmClip = apmClip;
}

static void
A(TEGlyphRenderer)(ScrnInfoPtr pScrn, int x, int y, int w, int h,
		    int skipleft, int startline, 
		    unsigned int **glyphs, int glyphWidth,
		    int fg, int bg, int rop, unsigned planemask)
{
    CARD32 *base, *base0;
    GlyphScanlineFuncPtr GlyphFunc = XAAGlyphScanlineFuncLSBFirst[glyphWidth - 1];
    int w2, h2, dwords;

    w2 = w + skipleft;
    h2 = h;
    dwords = (w2 + 31) >> 5;
    dwords <<= 2;

    base0 = base = (CARD32*)xalloc(dwords * h);
    if (!base)
	return;		/* Should not happen : it's rather small... */

    while(h--) {
	base = (*GlyphFunc)(base, glyphs, startline++, w2, glyphWidth);
    }

    A(WriteBitmap)(pScrn, x, y, w, h2, (unsigned char *)base0, dwords,
		    skipleft, fg, bg, rop, planemask);

    xfree(base0);
}

static void A(SetupForMono8x8PatternFill)(ScrnInfoPtr pScrn, int patx, int paty,
				          int fg, int bg, int rop,
				          unsigned int planemask)
{
    APMDECL(pScrn);

    DPRINTNAME(SetupForMono8x8PatternFill);
    pApm->apmTransparency = (pApm->Chipset >= AT3D) && (bg == -1);
    pApm->Bg8x8 = bg;
    pApm->Fg8x8 = fg;
    pApm->rop = apmROP[rop];
    A(WaitForFifo)(pApm, 3 + pApm->apmClip);
    if (bg == -1)
	SETBACKGROUNDCOLOR(fg + 1);
    else
	SETBACKGROUNDCOLOR(bg);
    SETFOREGROUNDCOLOR(fg);
    if (pApm->Chipset >= AT3D)
	SETROP(apmROP[rop] & 0xF0);
    else
	SETROP((apmROP[rop] & 0xF0) | 0x0A);
    if (pApm->apmClip) {
	SETCLIP_CTRL(0);
	pApm->apmClip = FALSE;
    }
}

static void A(SubsequentMono8x8PatternFillRect)(ScrnInfoPtr pScrn, int patx,
					        int paty, int x, int y,
					        int w, int h)
{
    APMDECL(pScrn);

    DPRINTNAME(SubsequentMono8x8PatternFillRect);
    SETDESTXY(x, y);
    UPDATEDEST(x, y + h + 1);
    A(WaitForFifo)(pApm, 6);
    if (pApm->Chipset == AT24 && pApm->Bg8x8 != -1) {
	SETROP(pApm->rop);
	SETFOREGROUNDCOLOR(pApm->Bg8x8);
#ifdef FASTER
	SETDEC(pApm->CurrentLayout.Setup_DEC | ((h == 1) ? DEC_OP_STRIP : DEC_OP_RECT) |
		DEC_DEST_XY | DEC_QUICKSTART_ONDIMX);
	SETWIDTHHEIGHT(w, h);
#else
	SETWIDTHHEIGHT(w, h);
	SETDEC(pApm->CurrentLayout.Setup_DEC | ((h == 1) ? DEC_OP_STRIP : DEC_OP_RECT) |
		DEC_DEST_XY | DEC_START);
#endif
	A(WaitForFifo)(pApm, 6);
	SETROP((pApm->rop & 0xF0) | 0x0A);
	SETFOREGROUNDCOLOR(pApm->Fg8x8);
    }
    SETPATTERN(patx, paty);
#ifdef FASTER
    SETDEC(pApm->CurrentLayout.Setup_DEC | ((h == 1) ? DEC_OP_STRIP : DEC_OP_RECT) |
	    DEC_DEST_XY | DEC_PATTERN_88_1bMONO | DEC_DEST_UPD_TRCORNER |
	    (pApm->apmTransparency ? DEC_SOURCE_TRANSPARENCY : 0) |
	    DEC_QUICKSTART_ONDIMX);
    SETWIDTHHEIGHT(w, h);
#else
    SETWIDTHHEIGHT(w, h);
    SETDEC(pApm->CurrentLayout.Setup_DEC | ((h == 1) ? DEC_OP_STRIP : DEC_OP_RECT) |
	    DEC_DEST_XY | DEC_PATTERN_88_1bMONO | DEC_DEST_UPD_TRCORNER |
	    (pApm->apmTransparency ? DEC_SOURCE_TRANSPARENCY : 0) |
	    DEC_START);
#endif
}

#if 0
static void
A(SetupForCPUToScreenColorExpandFill)(ScrnInfoPtr pScrn, int fg, int bg,
					int rop, unsigned int planemask)
{
  APMDECL(pScrn);

  DPRINTNAME(SetupForCPUToScreenColorExpandFill);
  if (bg == -1)
  {
#ifndef FASTER
    pApm->apmTransparency = TRUE;
    A(WaitForFifo)(pApm, 3);
#else
    A(WaitForFifo)(pApm, 4);
    SETDEC(DEC_OP_HOSTBLT_HOST2SCREEN | DEC_SOURCE_LINEAR | DEC_SOURCE_CONTIG |
      DEC_SOURCE_TRANSPARENCY | DEC_SOURCE_MONOCHROME | DEC_QUICKSTART_ONDIMX |
      DEC_DEST_UPD_TRCORNER | pApm->CurrentLayout.Setup_DEC);
#endif
    SETFOREGROUNDCOLOR(fg);
    SETBACKGROUNDCOLOR(fg+1);
  }
  else
  {
#ifndef FASTER
    pApm->apmTransparency = FALSE;
    A(WaitForFifo)(pApm, 3);
#else
    A(WaitForFifo)(pApm, 4);
    SETDEC(DEC_OP_HOSTBLT_HOST2SCREEN | DEC_SOURCE_LINEAR | DEC_SOURCE_CONTIG |
	   DEC_DEST_UPD_TRCORNER | DEC_SOURCE_MONOCHROME |
	   DEC_QUICKSTART_ONDIMX | pApm->CurrentLayout.Setup_DEC);
#endif
    SETFOREGROUNDCOLOR(fg);
    SETBACKGROUNDCOLOR(bg);
  }
  SETROP(apmROP[rop]);
}

static void
A(SubsequentCPUToScreenColorExpandFill)(ScrnInfoPtr pScrn, int x, int y,
					int w, int h, int skipleft)
{
  APMDECL(pScrn);
#ifndef FASTER
  u32 c;
#endif

  DPRINTNAME(SubsequentCPUToScreenColorExpandFill);
#ifndef FASTER
  c = DEC_OP_HOSTBLT_HOST2SCREEN | DEC_SOURCE_LINEAR | DEC_SOURCE_CONTIG |
      DEC_SOURCE_MONOCHROME | DEC_START | DEC_DEST_UPD_TRCORNER |
      pApm->CurrentLayout.Setup_DEC;

  if (pApm->apmTransparency)
    c |= DEC_SOURCE_TRANSPARENCY;

  A(WaitForFifo)(pApm, 7);
#else
  A(WaitForFifo)(pApm, 6);
#endif

  SETCLIP_LEFTTOP(x+skipleft, y);
  SETCLIP_RIGHTBOT(x+w-1, y+h-1);
  SETCLIP_CTRL(0x01);
  pApm->apmClip = TRUE;
  SETSOURCEX(0); /* According to manual, it just has to be zero */
  SETDESTXY(x, y);
  SETWIDTHHEIGHT((w + 31) & ~31, h);
  UPDATEDEST(x + ((w + 31) & ~31), y);

#ifndef FASTER
  SETDEC(c);
#endif
}
#endif

static void
A(SetupForImageWrite)(ScrnInfoPtr pScrn, int rop, unsigned int planemask,
		      int trans_color, int bpp, int depth)
{
  APMDECL(pScrn);

  DPRINTNAME(SetupForImageWrite);
  if (trans_color != -1)
  {
#ifndef FASTER
    pApm->apmTransparency = TRUE;
    A(WaitForFifo)(pApm, 3);
#else
    A(WaitForFifo)(pApm, 4);
    SETDEC(DEC_OP_HOSTBLT_HOST2SCREEN | DEC_SOURCE_LINEAR | DEC_SOURCE_CONTIG |
	  DEC_SOURCE_TRANSPARENCY | DEC_QUICKSTART_ONDIMX | pApm->CurrentLayout.Setup_DEC);
#endif
    SETBACKGROUNDCOLOR(trans_color);
  }
  else {
#ifndef FASTER
    pApm->apmTransparency = FALSE;
    A(WaitForFifo)(pApm, 2);
#else
    A(WaitForFifo)(pApm, 3);
    SETDEC(DEC_OP_HOSTBLT_HOST2SCREEN | DEC_SOURCE_LINEAR | DEC_SOURCE_CONTIG |
	  DEC_QUICKSTART_ONDIMX | pApm->CurrentLayout.Setup_DEC);
#endif
  }

  SETROP(apmROP[rop]);
}

static void
A(SubsequentImageWriteRect)(ScrnInfoPtr pScrn, int x, int y, int w, int h,
			    int skipleft)
{
  APMDECL(pScrn);
#ifndef FASTER
  u32 c;
#endif

  DPRINTNAME(SubsequentImageWriteRect);
#ifndef FASTER
  c = DEC_OP_HOSTBLT_HOST2SCREEN | DEC_SOURCE_LINEAR | DEC_SOURCE_CONTIG |
      DEC_START | pApm->CurrentLayout.Setup_DEC;

  if (pApm->apmTransparency)
    c |= DEC_SOURCE_TRANSPARENCY;

  if (pApm->Chipset >= AT24)
      A(WaitForFifo)(pApm, 7);
  else
      A(WaitForFifo)(pApm, 3);
#else
  if (pApm->Chipset >= AT24)
      A(WaitForFifo)(pApm, 6);
  else
      A(WaitForFifo)(pApm, 3);
#endif

  SETCLIP_LEFTTOP(x+skipleft, y);
  SETCLIP_RIGHTBOT(x+w-1, y+h-1);
  SETCLIP_CTRL(0x01);
  pApm->apmClip = TRUE;
  if (pApm->Chipset < AT24)
      A(WaitForFifo)(pApm, 4);
  SETSOURCEX(0); /* According to manual, it just has to be zero */
  SETDESTXY(x, y);
  SETWIDTHHEIGHT((w + 3) & ~3, h);

#ifndef FASTER
  SETDEC(c);
#endif
}

static void
A(SubsequentScreenToScreenColorExpandFill)(ScrnInfoPtr pScrn, int x, int y,
					   int w, int h, int srcx, int srcy,
					   int offset)
{
  APMDECL(pScrn);
  u32 c;

  DPRINTNAME(SubsequentScreenToScreenColorExpandFill);
#ifdef FASTER
  c = DEC_OP_BLT | DEC_DIR_X_POS | DEC_DIR_Y_POS | DEC_SOURCE_MONOCHROME |
      DEC_QUICKSTART_ONDIMX | DEC_DEST_UPD_TRCORNER | pApm->CurrentLayout.Setup_DEC;
#else
  c = DEC_OP_BLT | DEC_DIR_X_POS | DEC_DIR_Y_POS | DEC_SOURCE_MONOCHROME |
      DEC_START | DEC_DEST_UPD_TRCORNER | pApm->CurrentLayout.Setup_DEC;
#endif

  if (pApm->apmTransparency)
    c |= DEC_SOURCE_TRANSPARENCY;

  if (srcy >= pApm->CurrentLayout.Scanlines) {
      struct ApmStippleCacheRec *pCache;
      CARD32	dist;

      /*
       * Offscreen linear stipple
       */
      pCache = &pApm->apmCache[srcy / pApm->CurrentLayout.Scanlines - 1];
      if (w != pCache->apmStippleCache.w * pApm->CurrentLayout.bitsPerPixel) {
	  A(WaitForFifo)(pApm, 3);
	  SETCLIP_LEFTTOP(x, y);
	  SETCLIP_RIGHTBOT(x + w - 1, y + h - 1);
	  SETCLIP_CTRL(0x01);
	  pApm->apmClip = TRUE;
	  w = pCache->apmStippleCache.w * pApm->CurrentLayout.bitsPerPixel;
	  x -= srcx - pCache->apmStippleCache.x + offset;
	  srcx = (srcy - pCache->apmStippleCache.y) & 7;
	  srcy -= srcx;
	  y -= srcx;
	  h += srcx;
	  srcx = pCache->apmStippleCache.x;
      }
      else if (pApm->apmClip) {
	  A(WaitForFifo)(pApm, 1);
	  SETCLIP_CTRL(0x00);
	  pApm->apmClip = FALSE;
      }
      srcx += (srcy - pCache->apmStippleCache.y) * pCache->apmStippleCache.w;
      srcy = pCache->apmStippleCache.y % pApm->CurrentLayout.Scanlines;
      dist = srcx + srcy * pApm->CurrentLayout.displayWidth;
      srcx = dist & 0xFFF;
      srcy = dist >> 12;
      c |= DEC_SOURCE_CONTIG | DEC_SOURCE_LINEAR;
  }
  else if (offset) {
      A(WaitForFifo)(pApm, 3);
      SETCLIP_LEFTTOP(x, y);
      SETCLIP_RIGHTBOT(x + w, y + h);
      SETCLIP_CTRL(0x01);
      pApm->apmClip = TRUE;
      w += offset;
      x -= offset;
  }
  else if (pApm->apmClip) {
      A(WaitForFifo)(pApm, 1);
      SETCLIP_CTRL(0x00);
      pApm->apmClip = FALSE;
  }

  A(WaitForFifo)(pApm, 4);

  SETSOURCEXY(srcx, srcy);
  SETDESTXY(x, y);

#ifdef FASTER
  SETDEC(c);
  SETWIDTHHEIGHT(w, h);
#else
  SETWIDTHHEIGHT(w, h);
  SETDEC(c);
#endif
  UPDATEDEST(x + w + 1, h);
}

static void
A(SubsequentSolidBresenhamLine)(ScrnInfoPtr pScrn, int x1, int y1, int e1,
				int e2, int err, int length, int octant)
{
  APMDECL(pScrn);
#ifdef FASTER
  u32 c = DEC_QUICKSTART_ONDIMX | DEC_OP_VECT_ENDP | DEC_DEST_UPD_LASTPIX |
	  pApm->CurrentLayout.Setup_DEC;
#else
  u32 c = DEC_START | DEC_OP_VECT_ENDP | DEC_DEST_UPD_LASTPIX | pApm->CurrentLayout.Setup_DEC;
#endif
  int	tmp;

  DPRINTNAME(SubsequentSolidBresenhamLine);

  A(WaitForFifo)(pApm, 5);
  SETDESTXY(x1,y1);
  SETDDA_ERRORTERM(err);
  SETDDA_ADSTEP(e1, e2);

  if (octant & YMAJOR) {
    c |= DEC_MAJORAXIS_Y;
    tmp = e1; e1 = e2; e2 = tmp;
  }
  else
    c |= DEC_MAJORAXIS_X;

  if (octant & XDECREASING) {
    c |= DEC_DIR_X_NEG;
    e1 = -e1;
  }
  else
    c |= DEC_DIR_X_POS;

  if (octant & YDECREASING) {
    c |= DEC_DIR_Y_NEG;
    e2 = -e2;
  }
  else
    c |= DEC_DIR_Y_POS;

#ifdef FASTER
  SETDEC(c);
  SETWIDTH(length);
#else
  SETWIDTH(length);
  SETDEC(c);
#endif

  if (octant & YMAJOR)
    UPDATEDEST(x1 + e1 / 2, y1 + e2 / 2);
  else
    UPDATEDEST(x1 + e2 / 2, y1 + e1 / 2);
  if (pApm->apmClip)
  {
    pApm->apmClip = FALSE;
    A(WaitForFifo)(pApm, 1);
    SETCLIP_CTRL(0);
  }
}

static void
A(SubsequentSolidBresenhamLine6422)(ScrnInfoPtr pScrn, int x1, int y1, int e1,
				int e2, int err, int length, int octant)
{
  APMDECL(pScrn);
#ifdef FASTER
  u32 c = DEC_QUICKSTART_ONDIMX | DEC_OP_VECT_ENDP | DEC_DEST_UPD_LASTPIX |
	  pApm->CurrentLayout.Setup_DEC;
#else
  u32 c = DEC_START | DEC_OP_VECT_ENDP | DEC_DEST_UPD_LASTPIX | pApm->CurrentLayout.Setup_DEC;
#endif
  int	tmp;

  DPRINTNAME(SubsequentSolidBresenhamLine6422);

  A(WaitForFifo)(pApm, 1);
  SETDESTXY(x1,y1);
  A(WaitForFifo)(pApm, 4);
  SETDDA_ERRORTERM(err);
  SETDDA_ADSTEP(e1, e2);

  if (octant & YMAJOR) {
    c |= DEC_MAJORAXIS_Y;
    tmp = e1; e1 = e2; e2 = tmp;
  }
  else
    c |= DEC_MAJORAXIS_X;

  if (octant & XDECREASING) {
    c |= DEC_DIR_X_NEG;
    e1 = -e1;
  }
  else
    c |= DEC_DIR_X_POS;

  if (octant & YDECREASING) {
    c |= DEC_DIR_Y_NEG;
    e2 = -e2;
  }
  else
    c |= DEC_DIR_Y_POS;

#ifdef FASTER
  SETDEC(c);
  SETWIDTH(length);
#else
  SETWIDTH(length);
  SETDEC(c);
#endif

  if (octant & YMAJOR)
    UPDATEDEST(x1 + e1 / 2, y1 + e2 / 2);
  else
    UPDATEDEST(x1 + e2 / 2, y1 + e1 / 2);
  if (pApm->apmClip)
  {
    pApm->apmClip = FALSE;
    A(WaitForFifo)(pApm, 1);
    SETCLIP_CTRL(0);
  }
}

static void
A(SetClippingRectangle)(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2)
{
  APMDECL(pScrn);

  DPRINTNAME(SetClippingRectangle);
  A(WaitForFifo)(pApm, 3);
  SETCLIP_LEFTTOP(x1,y1);
  SETCLIP_RIGHTBOT(x2,y2);
  SETCLIP_CTRL(0x01);
  pApm->apmClip = TRUE;
}

static void
A(SyncBlt)(ApmPtr pApm)
{
    int again = (pApm->Chipset == AP6422);

    do {
       while (!(STATUS() & STATUS_HOSTBLTBUSY))
	   ;
    }
    while (again--);	/* See remark in Sync6422 */
}
static void
A(WritePixmap)(ScrnInfoPtr pScrn, int x, int y, int w, int h,
	       unsigned char *src, int srcwidth, int rop,
	       unsigned int planemask, int trans, int bpp, int depth)
{
    APMDECL(pScrn);
    int dwords, skipleft, Bpp = bpp >> 3;
    Bool beCareful = FALSE;
    unsigned char *dst = ((unsigned char *)pApm->FbBase) + x * Bpp + y * pApm->CurrentLayout.bytesPerScanline;
    int PlusOne = 0, mask, count;

    DPRINTNAME(WritePixmap);
    if (rop == GXnoop)
	return;
    /*
     * The function seems to crash more than it feels good. I hope that a
     * good sync will help. This sync is anyway needed for direct write.
     */
    (*pApm->AccelInfoRec->Sync)(pScrn);
    /*
     * First the fast case : source and dest have same alignment. Doc says
     * it's faster to do it here, which may be true since one has to read
     * the chip when CPU to screen-ing.
     */
    if ((skipleft = (long)src & 3L) == ((long)dst & 3L) && rop == GXcopy) {
	int skipright;

	if (skipleft)
	    skipleft = 4 - skipleft;
	dwords = (skipright = w * Bpp - skipleft) >> 2;
	skipright %= 4;
	if (!skipleft && !skipright)
	    while (h-- > 0) {
		CARD32 *src2 = (CARD32 *)src;
		CARD32 *dst2 = (CARD32 *)dst;

		for (count = dwords; count-- > 0; )
		    *dst2++ = *src2++;
		src += srcwidth;
		dst += pApm->CurrentLayout.bytesPerScanline;
	    }
	else if (!skipleft)
	    while (h-- > 0) {
		CARD32 *src2 = (CARD32 *)src;
		CARD32 *dst2 = (CARD32 *)dst;

		for (count = dwords; count-- > 0; )
		    *dst2++ = *src2++;
		for (count = skipright; count-- > 0; )
		    ((char *)dst2)[count] = ((char *)src2)[count];
		src += srcwidth;
		dst += pApm->CurrentLayout.bytesPerScanline;
	    }
	else if (!skipright)
	    while (h-- > 0) {
		CARD32 *src2 = (CARD32 *)(src + skipleft);
		CARD32 *dst2 = (CARD32 *)(dst + skipleft);

		for (count = skipleft; count-- > 0; )
		    dst[count] = src[count];
		for (count = dwords; count-- > 0; )
		    *dst2++ = *src2++;
		src += srcwidth;
		dst += pApm->CurrentLayout.bytesPerScanline;
	    }
	else
	    while (h-- > 0) {
		CARD32 *src2 = (CARD32 *)(src + skipleft);
		CARD32 *dst2 = (CARD32 *)(dst + skipleft);

		for (count = skipleft; count-- > 0; )
		    dst[count] = src[count];
		for (count = dwords; count-- > 0; )
		    *dst2++ = *src2++;
		for (count = skipright; count-- > 0; )
		    ((char *)dst2)[count] = ((char *)src2)[count];
		src += srcwidth;
		dst += pApm->CurrentLayout.bytesPerScanline;
	    }

	return;
    }

    if (skipleft) {
	if (Bpp == 3)
	   skipleft = 4 - skipleft;
	else
	   skipleft /= Bpp;

	if (x < skipleft) {
	   skipleft = 0;
	   beCareful = TRUE;
	   goto BAD_ALIGNMENT;
	}

	x -= skipleft;
	w += skipleft;

	if (Bpp == 3)
	   src -= 3 * skipleft;
	else   /* is this Alpha friendly ? */
	   src = (unsigned char*)((long)src & ~0x03L);
    }

BAD_ALIGNMENT:

    dwords = ((w * Bpp) + 3) >> 2;
    mask = (pApm->CurrentLayout.bitsPerPixel / 8) - 1;

    if (dwords & mask) {
	/*
	 * Experimental...
	 * It seems the AT3D needs a padding of scanline to a multiple of
	 * 4 pixels, not only bytes.
	 */
	PlusOne = mask - (dwords & mask) + 1;
    }

    A(SetupForImageWrite)(pScrn, rop, planemask, trans, bpp, depth);
    A(SubsequentImageWriteRect)(pScrn, x, y, w, h, skipleft);

    if (beCareful) {
	/* in cases with bad alignment we have to be careful not
	   to read beyond the end of the source */
	if (((x * Bpp) + (dwords << 2)) > srcwidth) h--;
	else beCareful = FALSE;
    }

    srcwidth -= (dwords << 2);

    while (h--) {
	for (count = dwords; count-- > 0; ) {
	    A(SyncBlt)(pApm);
	    *(CARD32*)pApm->BltMap = *(CARD32*)src;
	    src += 4;
	}
	src += srcwidth;
	for (count = PlusOne; count-- > 0; ) {
	    int	status;

	    while (!((status = STATUS()) & STATUS_HOSTBLTBUSY))
		if (!(status & STATUS_ENGINEBUSY))
		    break;
	    if (pApm->Chipset == AP6422)	/* See remark in Sync6422 */
		while (!((status = STATUS()) & STATUS_HOSTBLTBUSY))
		    if (!(status & STATUS_ENGINEBUSY))
			break;
	    if (status & STATUS_ENGINEBUSY)
		*(CARD32*)pApm->BltMap = 0x00000000;
	}
    }
    if (beCareful) {
       int shift = ((long)src & 0x03L) << 3;

       if (--dwords) {
	    for (count = dwords >> 2; count-- > 0; ) {
		A(SyncBlt)(pApm);
		*(CARD32*)pApm->BltMap = *(CARD32*)src;
		src += 4;
	    }
       }
       A(SyncBlt)(pApm);
       *((CARD32*)pApm->BltMap) = *((CARD32*)src) >> shift;
    }

    pApm->apmClip = FALSE;
    A(WaitForFifo)(pApm, 1);
    SETCLIP_CTRL(0);
}

static void 
A(FillImageWriteRects)(ScrnInfoPtr pScrn, int rop, unsigned int planemask,
			int nBox, BoxPtr pBox, int xorg, int yorg,
			PixmapPtr pPix)
{
    XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_SCRNINFOPTR(pScrn);
    int x, y, phaseY, phaseX, height, width, blit_w;
    int pHeight = pPix->drawable.height;
    int pWidth = pPix->drawable.width;
    int depth = pPix->drawable.depth;
    int bpp = pPix->drawable.bitsPerPixel;
    unsigned char *pSrc;
    int srcwidth = pPix->devKind;

    while(nBox--) {
	x = pBox->x1;
	y = pBox->y1;
	phaseY = (pBox->y1 - yorg) % pHeight;
	if(phaseY < 0) phaseY += pHeight;
	phaseX = (x - xorg) % pWidth;
	pSrc = (unsigned char *)pPix->devPrivate.ptr +
				    phaseX * pPix->drawable.bitsPerPixel / 8;
	if(phaseX < 0) phaseX += pWidth;
	height = pBox->y2 - pBox->y1;
	width = pBox->x2 - x;
	
	while(1) {
	    int		ch = height, cp = phaseY, cy = y;

	    blit_w = pWidth - phaseX;
	    if(blit_w > width) blit_w = width;

	    while (ch > 0) {
		int	h = MIN(pHeight - cp, ch);

		A(WritePixmap)(pScrn, x, cy, blit_w, h, pSrc + cp * srcwidth,
				srcwidth, rop, planemask, FALSE, bpp, depth);
		cy += h;
		ch -= h;
		cp = 0;
	    }

	    width -= blit_w;
	    if(!width) break;
	    x += blit_w;
	    phaseX = (phaseX + blit_w) % pWidth;
	}
	pBox++;
    }

    SET_SYNC_FLAG(infoRec);
}

static void A(SetupForColor8x8PatternFill)(ScrnInfoPtr pScrn,int patx,int paty,
				           int rop, unsigned int planemask,
					   int transparency_color)
{
    APMDECL(pScrn);

    DPRINTNAME(SetupForColor8x8PatternFillRect);
    if (transparency_color != -1) {
#ifndef FASTER
	pApm->apmTransparency = TRUE;
	A(WaitForFifo)(pApm, 2 + pApm->apmClip);
#else
	A(WaitForFifo)(pApm, 3 + pApm->apmClip);
	SETDEC(pApm->CurrentLayout.Setup_DEC | DEC_OP_BLT |
	    DEC_DEST_XY | DEC_PATTERN_88_8bCOLOR | DEC_SOURCE_TRANSPARENCY |
	    DEC_QUICKSTART_ONDIMX);
#endif
	SETBACKGROUNDCOLOR(transparency_color);
    }
    else {
#ifndef FASTER
	pApm->apmTransparency = FALSE;
	A(WaitForFifo)(pApm, 1 + pApm->apmClip);
#else
	A(WaitForFifo)(pApm, 2 + pApm->apmClip);
	SETDEC(pApm->CurrentLayout.Setup_DEC | DEC_OP_BLT |
	    DEC_DEST_XY | DEC_PATTERN_88_8bCOLOR | DEC_QUICKSTART_ONDIMX);
#endif
    }
    if (pApm->apmClip) {
	SETCLIP_CTRL(0);
	pApm->apmClip = FALSE;
    }
    SETROP(apmROP[rop]);
}

static void A(SubsequentColor8x8PatternFillRect)(ScrnInfoPtr pScrn, int patx,
					         int paty, int x, int y,
					         int w, int h)
{
    APMDECL(pScrn);

    DPRINTNAME(SubsequentColor8x8PatternFillRect);
#ifndef FASTER
    A(WaitForFifo)(pApm, 5);
#else
    A(WaitForFifo)(pApm, 4);
#endif
    SETSOURCEXY(patx, paty);
    SETDESTXY(x, y);
    SETWIDTHHEIGHT(w, h);
    UPDATEDEST(x + w + 1, y);
#ifndef FASTER
    SETDEC(pApm->CurrentLayout.Setup_DEC | DEC_OP_BLT |
	    DEC_DEST_XY | (pApm->apmTransparency * DEC_SOURCE_TRANSPARENCY) |
	    DEC_PATTERN_88_8bCOLOR | DEC_START);
#endif
}
#endif

static void
A(Sync)(ScrnInfoPtr pScrn)
{
  APMDECL(pScrn);
  volatile u32 i, stat;

  for(i = 0; i < MAXLOOP; i++) {
    stat = STATUS();
    if ((!(stat & (STATUS_HOSTBLTBUSY | STATUS_ENGINEBUSY))) &&
        ((stat & STATUS_FIFO) >= 8))
      break;
  }
  if (i == MAXLOOP) {
    unsigned int status = STATUS();

    WRXB(0x1FF, 0);
    if (!xf86ServerIsExiting())
	FatalError("Hung in ApmSync" APM_SUFF_24 APM_SUFF_IOP "(%d) (Status = 0x%08X)\n", pScrn->pScreen->myNum, status);
  }
  if (pApm->apmClip) {
    SETCLIP_CTRL(0);
    pApm->apmClip = FALSE;
  }
}

#if PSZ != 24
static void
A(Sync6422)(ScrnInfoPtr pScrn)
{
  APMDECL(pScrn);
  volatile u32 i, j, stat;

  for (j = 0; j < 2; j++) {
      /*
       * From Henrik Harmsen :
       *
       * This is a kludge. We can't trust the status register. Don't
       * know why... We shouldn't be forced to read the status reg and get
       * a correct value more than once...
       */
      for(i = 0; i < MAXLOOP; i++) {
	stat = STATUS();
	if ((!(stat & (STATUS_HOSTBLTBUSY | STATUS_ENGINEBUSY))) &&
	    ((stat & STATUS_FIFO) >= 4))
	  break;
      }
  }
  if (i == MAXLOOP) {
    unsigned int status = STATUS();

    WRXB(0x1FF, 0);
    if (!xf86ServerIsExiting())
	FatalError("Hung in ApmSync6422() (Status = 0x%08X)\n", status);
  }
  if (pApm->apmClip) {
    SETCLIP_CTRL(0);
    pApm->apmClip = FALSE;
  }
}
#endif
#include "apm_video.c"


#undef	RDXB
#undef	RDXW
#undef	RDXL
#undef	WRXB
#undef	WRXW
#undef	WRXL
#undef	ApmWriteSeq
#define RDXB	RDXB_M
#define RDXW	RDXW_M
#define RDXL	RDXL_M
#define WRXB	WRXB_M
#define WRXW	WRXW_M
#define WRXL	WRXL_M
#define ApmWriteSeq(idx, val)	do { APMVGAB(0x3C4) = (idx); APMVGAB(0x3C5) = (val); break; } while(1)
#undef DPRINTNAME
#undef A
#undef DEPTH
#undef PSZ
#undef IOP_ACCESS
#undef APM_SUFF_24
#undef APM_SUFF_IOP
