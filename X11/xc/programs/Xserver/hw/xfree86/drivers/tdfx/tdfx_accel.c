/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tdfx/tdfx_accel.c,v 1.20 2001/10/28 03:33:49 tsi Exp $ */

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "compiler.h"

/* Drivers that need to access the PCI config space directly need this */
#include "xf86Pci.h"

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"

/* Drivers that use XAA need this */
#include "xaa.h"
#include "xaalocal.h"
#include "xf86fbman.h"

#include "miline.h"

#include "tdfx.h"

#ifdef TDFX_DEBUG_CMDS
static int cmdCnt=0;
static int lastAddr=0;
#endif

static int TDFXROPCvt[] = {0x00, 0x88, 0x44, 0xCC, 0x22, 0xAA, 0x66, 0xEE,
			   0x11, 0x99, 0x55, 0xDD, 0x33, 0xBB, 0x77, 0xFF,
			   0x00, 0xA0, 0x50, 0xF0, 0x0A, 0xAA, 0x5A, 0xFA,
			   0x05, 0xA5, 0x55, 0xF5, 0x0F, 0xAF, 0x5F, 0xFF};
#define ROP_PATTERN_OFFSET 16

static void TDFXSetClippingRectangle(ScrnInfoPtr pScrn, int left, int top, 
				     int right, int bottom);
static void TDFXDisableClipping(ScrnInfoPtr pScrn);
static void TDFXSetupForMono8x8PatternFill(ScrnInfoPtr pScrn, int patx, 
					   int paty, int fg, int bg, int rop, 
					   unsigned int planemask);
static void TDFXSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, int patx, 
						 int pay, int x, int y, 
						 int w, int h);
static void TDFXSetupForSolidLine(ScrnInfoPtr pScrn, int color, int rop, 
				  unsigned int planemask);
static void TDFXSubsequentSolidTwoPointLine(ScrnInfoPtr pScrn, int srcx, 
					    int srcy, int dstx, int dsty, 
					    int flags);
static void TDFXSubsequentSolidHorVertLine(ScrnInfoPtr pScrn, int x, int y, 
					   int len, int dir);
static void TDFXNonTEGlyphRenderer(ScrnInfoPtr pScrn, int x, int y, int n, 
				   NonTEGlyphPtr glyphs, BoxPtr pbox, int fg, 
				   int rop, unsigned int planemask);
static void TDFXSetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, int rop,
                                   unsigned int planemask, int length,
		                   unsigned char *pattern);
static void TDFXSubsequentDashedTwoPointLine(ScrnInfoPtr pScrn, int x1, int y1,
                                             int x2, int y2, int flags,
                                             int phase);
#ifdef ENABLE_SS_COLOR_EXPAND_FILL
static void TDFXSetupForScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                      int fg, int bg, int rop,
                                                      unsigned int planemask);
static void TDFXSubsequentScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                        int x, int y, int w,
                                                        int h, int srcx,
                                                        int srcy, int offset);
#endif
static void TDFXSetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                   int fg, int bg, int rop,
                                                   unsigned int planemask);
static void TDFXSubsequentCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                     int x, int y,
                                                     int w, int h,
                                                     int skipleft);
static void TDFXSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno);

void
TDFXNeedSync(ScrnInfoPtr pScrn) {
  TDFXPtr pTDFX = TDFXPTR(pScrn);
  pTDFX->syncDone=FALSE;
  pTDFX->AccelInfoRec->NeedToSync = TRUE;
}

void
TDFXFirstSync(ScrnInfoPtr pScrn) {
  TDFXPtr pTDFX = TDFXPTR(pScrn);

  if (!pTDFX->syncDone) {
#ifdef XF86DRI
    if (pTDFX->directRenderingEnabled) {
      DRILock(screenInfo.screens[pScrn->scrnIndex], 0);
      TDFXSwapContextFifo(screenInfo.screens[pScrn->scrnIndex]);
    }
#endif
    pTDFX->syncDone=TRUE;
    pTDFX->sync(pScrn);
  }
}

void
TDFXCheckSync(ScrnInfoPtr pScrn) {
  TDFXPtr pTDFX = TDFXPTR(pScrn);

  if (pTDFX->syncDone) {
    pTDFX->sync(pScrn);
    pTDFX->syncDone=FALSE;
#ifdef XF86DRI
    if (pTDFX->directRenderingEnabled) {
      DRIUnlock(screenInfo.screens[pScrn->scrnIndex]);
    }
#endif
  }
}

void
TDFXSelectBuffer(TDFXPtr pTDFX, int which) {
  int fmt;

  TDFXMakeRoom(pTDFX, 4);
  DECLARE(SSTCP_SRCBASEADDR|SSTCP_DSTBASEADDR|SSTCP_SRCFORMAT|SSTCP_DSTFORMAT);
  switch (which) {
  case TDFX_FRONT:
    if (pTDFX->cpp==1) fmt=pTDFX->stride|(1<<16);
    else fmt=pTDFX->stride|((pTDFX->cpp+1)<<16);
    TDFXWriteLong(pTDFX, SST_2D_DSTBASEADDR, pTDFX->fbOffset);
    TDFXWriteLong(pTDFX, SST_2D_DSTFORMAT, fmt);
    pTDFX->sst2DDstFmtShadow = fmt;
    TDFXWriteLong(pTDFX, SST_2D_SRCBASEADDR, pTDFX->fbOffset);
    TDFXWriteLong(pTDFX, SST_2D_SRCFORMAT, fmt);
    pTDFX->sst2DSrcFmtShadow = fmt;
    break;
  case TDFX_BACK:
    if (pTDFX->cpp==2)
      fmt=((pTDFX->stride+127)/128)|(3<<16); /* Tiled 16bpp */
    else
      fmt=((pTDFX->stride+127)/128)|(5<<16); /* Tiled 32bpp */
    TDFXWriteLong(pTDFX, SST_2D_DSTBASEADDR, pTDFX->backOffset|BIT(31));
    TDFXWriteLong(pTDFX, SST_2D_DSTFORMAT, fmt);
    pTDFX->sst2DDstFmtShadow = fmt;
    TDFXWriteLong(pTDFX, SST_2D_SRCBASEADDR, pTDFX->backOffset|BIT(31));
    TDFXWriteLong(pTDFX, SST_2D_SRCFORMAT, fmt);
    pTDFX->sst2DSrcFmtShadow = fmt;
    break;
  case TDFX_DEPTH:
    if (pTDFX->cpp==2)
      fmt=((pTDFX->stride+127)/128)|(3<<16); /* Tiled 16bpp */
    else
      fmt=((pTDFX->stride+127)/128)|(5<<16); /* Tiled 32bpp */
    TDFXWriteLong(pTDFX, SST_2D_DSTBASEADDR, pTDFX->depthOffset|BIT(31));
    TDFXWriteLong(pTDFX, SST_2D_DSTFORMAT, fmt);
    pTDFX->sst2DDstFmtShadow = fmt;
    TDFXWriteLong(pTDFX, SST_2D_SRCBASEADDR, pTDFX->depthOffset|BIT(31));
    TDFXWriteLong(pTDFX, SST_2D_SRCFORMAT, fmt);
    pTDFX->sst2DSrcFmtShadow = fmt;
    break;
  default:
    ;
  }
}

void
TDFXSetLFBConfig(TDFXPtr pTDFX) {
  if (pTDFX->ChipType<=PCI_CHIP_VOODOO3) {
#if X_BYTE_ORDER == X_BIG_ENDIAN
    unsigned int lfbmode;  
    lfbmode=TDFXReadLongMMIO(pTDFX, SST_3D_LFBMODE);

    lfbmode&=~BIT(12); /* 0 bit 12 is byte swizzle */
    lfbmode|=BIT(11); /* 1 bit 11 is word swizzle */
    lfbmode&=~BIT(10); /* 0 bit 10  ARGB or ABGR */
    lfbmode&=~BIT(9); /* 0 bit 9 if bit10 = 0:  ARGB else ABGR */

    TDFXWriteLongMMIO(pTDFX, SST_3D_LFBMODE, lfbmode);
#endif
    TDFXWriteLongMMIO(pTDFX, LFBMEMORYCONFIG, (pTDFX->backOffset>>12) |
		      SST_RAW_LFB_ADDR_STRIDE_4K | 
		      ((pTDFX->stride+127)/128)<<SST_RAW_LFB_TILE_STRIDE_SHIFT);
  } else {
    int chip;
    int stride, bits;
    int TileAperturePitch, lg2TileAperturePitch;
    if (pTDFX->cpp==2) stride=pTDFX->stride;
    else stride=4*pTDFX->stride/pTDFX->cpp;
    bits=pTDFX->backOffset>>12;
    for (lg2TileAperturePitch = 0, TileAperturePitch = 1024;
         (lg2TileAperturePitch < 5) &&
             TileAperturePitch < stride;
         lg2TileAperturePitch += 1, TileAperturePitch <<= 1);
#if	0
    fprintf(stderr, "Using %d (== lg2(%d)-10) for tile aperture pitch\n",
            lg2TileAperturePitch, TileAperturePitch);
    fprintf(stderr, "stride == %d\n", stride);
#endif
    for (chip=0; chip<pTDFX->numChips; chip++) {
      TDFXWriteChipLongMMIO(pTDFX, chip, LFBMEMORYCONFIG, (bits&0x1FFF) |
			    SST_RAW_LFB_ADDR_STRIDE(lg2TileAperturePitch) | 
			    ((bits&0x6000)<<10) |
			    ((stride+127)/128)<<SST_RAW_LFB_TILE_STRIDE_SHIFT);
    }
  }
}

Bool
TDFXAccelInit(ScreenPtr pScreen)
{
  XAAInfoRecPtr infoPtr;
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  TDFXPtr pTDFX = TDFXPTR(pScrn);
  CARD32 commonFlags;

  pTDFX->AccelInfoRec = infoPtr = XAACreateInfoRec();
  if (!infoPtr) return FALSE;

  infoPtr->Flags = PIXMAP_CACHE | OFFSCREEN_PIXMAPS | LINEAR_FRAMEBUFFER;

  infoPtr->Sync = pTDFX->sync;

  infoPtr->SetClippingRectangle = TDFXSetClippingRectangle;
  infoPtr->DisableClipping = TDFXDisableClipping;
  infoPtr->ClippingFlags = HARDWARE_CLIP_SCREEN_TO_SCREEN_COLOR_EXPAND |
    HARDWARE_CLIP_SCREEN_TO_SCREEN_COPY |
    HARDWARE_CLIP_MONO_8x8_FILL |
    HARDWARE_CLIP_COLOR_8x8_FILL |
    HARDWARE_CLIP_SOLID_FILL |
    HARDWARE_CLIP_DASHED_LINE |
    HARDWARE_CLIP_SOLID_LINE;

  miSetZeroLineBias(pScreen, OCTANT2 | OCTANT5 | OCTANT7 | OCTANT8);
  
  commonFlags = BIT_ORDER_IN_BYTE_MSBFIRST | NO_PLANEMASK;

  infoPtr->SetupForSolidFill = TDFXSetupForSolidFill;
  infoPtr->SubsequentSolidFillRect = TDFXSubsequentSolidFillRect;
  infoPtr->SolidFillFlags = commonFlags;

  infoPtr->SetupForSolidLine = TDFXSetupForSolidLine;
  infoPtr->SubsequentSolidTwoPointLine = TDFXSubsequentSolidTwoPointLine;
  infoPtr->SubsequentSolidHorVertLine = TDFXSubsequentSolidHorVertLine;
  infoPtr->SolidLineFlags = commonFlags;

  infoPtr->SetupForDashedLine = TDFXSetupForDashedLine;
  infoPtr->SubsequentDashedTwoPointLine = TDFXSubsequentDashedTwoPointLine;
  infoPtr->DashedLineFlags = commonFlags | LINE_PATTERN_LSBFIRST_LSBJUSTIFIED;
  infoPtr->DashPatternMaxLength = 32;

  infoPtr->NonTEGlyphRenderer = TDFXNonTEGlyphRenderer;
  infoPtr->NonTEGlyphRendererFlags = commonFlags;

  infoPtr->SetupForScreenToScreenCopy = TDFXSetupForScreenToScreenCopy;
  infoPtr->SubsequentScreenToScreenCopy = TDFXSubsequentScreenToScreenCopy;
  infoPtr->ScreenToScreenCopyFlags = commonFlags;

  /* When we're using the fifo we have to use indirect expansion */
  pTDFX->scanlineColorExpandBuffers[0] = xalloc((pScrn->virtualX+62)/32*4);
  pTDFX->scanlineColorExpandBuffers[1] = xalloc((pScrn->virtualX+62)/32*4);
  infoPtr->NumScanlineColorExpandBuffers=2;
  infoPtr->ScanlineColorExpandBuffers=pTDFX->scanlineColorExpandBuffers;
  infoPtr->SetupForScanlineCPUToScreenColorExpandFill =
    TDFXSetupForCPUToScreenColorExpandFill;
  infoPtr->SubsequentScanlineCPUToScreenColorExpandFill =
    TDFXSubsequentCPUToScreenColorExpandFill;
  infoPtr->SubsequentColorExpandScanline =
    TDFXSubsequentColorExpandScanline;
  infoPtr->ScanlineCPUToScreenColorExpandFillFlags = NO_PLANEMASK |
#if X_BYTE_ORDER == X_LITTLE_ENDIAN
    BIT_ORDER_IN_BYTE_MSBFIRST |
#endif
    CPU_TRANSFER_PAD_DWORD | SCANLINE_PAD_DWORD |
    LEFT_EDGE_CLIPPING; /* | LEFT_EDGE_CLIPPING_NEGATIVE_X; */

  infoPtr->SetupForMono8x8PatternFill = TDFXSetupForMono8x8PatternFill;
  infoPtr->SubsequentMono8x8PatternFillRect =
    TDFXSubsequentMono8x8PatternFillRect;
  infoPtr->Mono8x8PatternFillFlags = commonFlags |
    HARDWARE_PATTERN_PROGRAMMED_BITS |
    HARDWARE_PATTERN_PROGRAMMED_ORIGIN |
    HARDWARE_PATTERN_SCREEN_ORIGIN;

#ifdef ENABLE_SS_COLOR_EXPAND_FILL
  /* This causes us to fail compliance */
  /* I suspect 1bpp pixmaps are getting written to cache incorrectly */
  infoPtr->SetupForScreenToScreenColorExpandFill =
    TDFXSetupForScreenToScreenColorExpandFill;
  infoPtr->SubsequentScreenToScreenColorExpandFill =
    TDFXSubsequentScreenToScreenColorExpandFill;
  infoPtr->ScreenToScreenColorExpandFillFlags = commonFlags;
#endif

  pTDFX->PciCnt=TDFXReadLongMMIO(pTDFX, 0)&0x1F;
  pTDFX->PrevDrawState=pTDFX->DrawState=0;

  pTDFX->ModeReg.srcbaseaddr=pTDFX->fbOffset;
  TDFXWriteLongMMIO(pTDFX, SST_2D_SRCBASEADDR, pTDFX->ModeReg.srcbaseaddr);
  pTDFX->ModeReg.dstbaseaddr=pTDFX->fbOffset;
  TDFXWriteLongMMIO(pTDFX, SST_2D_DSTBASEADDR, pTDFX->ModeReg.dstbaseaddr);

  pTDFX->sst2DSrcFmtShadow = TDFXReadLongMMIO(pTDFX, SST_2D_SRCFORMAT);
  pTDFX->sst2DDstFmtShadow = TDFXReadLongMMIO(pTDFX, SST_2D_DSTFORMAT);

  /* Fill in acceleration functions */
  return XAAInit(pScreen, infoPtr);
}

static void TDFXMakeRoomNoProp(TDFXPtr pTDFX, int size) {
  int stat;

  pTDFX->PciCnt-=size;
  if (pTDFX->PciCnt<1) {
    do {
      stat=TDFXReadLongMMIO(pTDFX, 0);
      pTDFX->PciCnt=stat&0x1F;
    } while (pTDFX->PciCnt<size);
  }
}

static void TDFXSendNOPNoProp(ScrnInfoPtr pScrn)
{
  TDFXPtr pTDFX;

  pTDFX=TDFXPTR(pScrn);
  TDFXMakeRoomNoProp(pTDFX, 1);
  TDFXWriteLongMMIO(pTDFX, SST_2D_COMMAND, SST_2D_NOP);
}  

void TDFXSync(ScrnInfoPtr pScrn)
{
  TDFXPtr pTDFX;
  int i;
  int stat;

  TDFXTRACEACCEL("TDFXSync\n");
  pTDFX=TDFXPTR(pScrn);

  TDFXSendNOPNoProp(pScrn);
  i=0;
  do {
    stat=TDFXReadLongMMIO(pTDFX, 0);
    if (stat&SST_BUSY) i=0; else i++;
  } while (i<3);
  pTDFX->PciCnt=stat&0x1F;
}

static void
TDFXMatchState(TDFXPtr pTDFX)
{
  if (pTDFX->PrevDrawState==pTDFX->DrawState) return;

  /* Do we need to set a clipping rectangle? */
  if (pTDFX->DrawState&DRAW_STATE_CLIPPING)
    pTDFX->Cmd |= SST_2D_USECLIP1;
  else
    pTDFX->Cmd &= ~SST_2D_USECLIP1;

  /* Do we need to set transparency? */
  TDFXMakeRoom(pTDFX, 1);
  DECLARE(SSTCP_COMMANDEXTRA);
  if (pTDFX->DrawState&DRAW_STATE_TRANSPARENT) {
    TDFXWriteLong(pTDFX, SST_2D_COMMANDEXTRA, SST_2D_SRC_COLORKEY_EX);
  } else {
    TDFXWriteLong(pTDFX, SST_2D_COMMANDEXTRA, 0);
  }

  /* Has the previous routine left clip1 changed? Reset it. */
  if (pTDFX->DrawState&DRAW_STATE_CLIP1CHANGED) {
    TDFXMakeRoom(pTDFX, 2);
    DECLARE(SSTCP_CLIP1MIN|SSTCP_CLIP1MAX);
    TDFXWriteLong(pTDFX, SST_2D_CLIP1MIN, pTDFX->ModeReg.clip1min);
    TDFXWriteLong(pTDFX, SST_2D_CLIP1MAX, pTDFX->ModeReg.clip1max);
    pTDFX->DrawState&=~DRAW_STATE_CLIP1CHANGED;
  }

  pTDFX->PrevDrawState=pTDFX->DrawState;
}

static void
TDFXClearState(ScrnInfoPtr pScrn)
{
  TDFXPtr pTDFX;

  pTDFX=TDFXPTR(pScrn);
  pTDFX->Cmd=0;
  pTDFX->DrawState&=~DRAW_STATE_TRANSPARENT;
  /* Make sure we've done a sync */
  TDFXFirstSync(pScrn);
}

static void
TDFXSetClippingRectangle(ScrnInfoPtr pScrn, int left, int top, int right, 
			 int bottom)
{
  TDFXPtr pTDFX;

  TDFXTRACEACCEL("TDFXSetClippingRectangle %d,%d to %d,%d\n", left, top,
		 right, bottom);
  pTDFX=TDFXPTR(pScrn);

  pTDFX->ModeReg.clip1min=(top&0xFFF)<<16 | (left&0xFFF);
  pTDFX->ModeReg.clip1max=((bottom+1)&0xFFF)<<16 | ((right+1)&0xFFF);

  pTDFX->DrawState|=DRAW_STATE_CLIPPING|DRAW_STATE_CLIP1CHANGED;
}

static void
TDFXDisableClipping(ScrnInfoPtr pScrn)
{
  TDFXPtr pTDFX;

  TDFXTRACEACCEL("TDFXDisableClippingRectangle\n");
  pTDFX=TDFXPTR(pScrn);

  pTDFX->DrawState&=~DRAW_STATE_CLIPPING;
}

void
TDFXSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir, int rop,
			       unsigned int planemask, int trans_color)
{
  TDFXPtr pTDFX;
  int fmt;

  TDFXTRACEACCEL("TDFXSetupForScreenToScreenCopy\n xdir=%d ydir=%d "
		 "rop=%d planemask=%d trans_color=%d\n", 
		 xdir, ydir, rop, planemask, trans_color);
  pTDFX=TDFXPTR(pScrn);
  TDFXClearState(pScrn);

  if (trans_color!=-1) {
    TDFXMakeRoom(pTDFX, 3);
    DECLARE(SSTCP_SRCCOLORKEYMIN|SSTCP_SRCCOLORKEYMAX|SSTCP_ROP);
    TDFXWriteLong(pTDFX, SST_2D_SRCCOLORKEYMIN, trans_color);
    TDFXWriteLong(pTDFX, SST_2D_SRCCOLORKEYMAX, trans_color);
    TDFXWriteLong(pTDFX, SST_2D_ROP, TDFXROPCvt[GXnoop]<<8);
    pTDFX->DrawState|=DRAW_STATE_TRANSPARENT;
  }
  pTDFX->Cmd = (TDFXROPCvt[rop]<<24) | SST_2D_SCRNTOSCRNBLIT;
  if (xdir==-1) pTDFX->Cmd |= SST_2D_X_RIGHT_TO_LEFT;
  if (ydir==-1) pTDFX->Cmd |= SST_2D_Y_BOTTOM_TO_TOP;
  if (pTDFX->cpp==1) fmt=pTDFX->stride|(1<<16); 
  else fmt=pTDFX->stride|((pTDFX->cpp+1)<<16);

  TDFXMakeRoom(pTDFX, 2);
  DECLARE(SSTCP_SRCFORMAT|SSTCP_DSTFORMAT);
  TDFXWriteLong(pTDFX, SST_2D_DSTFORMAT, fmt);
  pTDFX->sst2DDstFmtShadow = fmt;
  TDFXWriteLong(pTDFX, SST_2D_SRCFORMAT, fmt);
  pTDFX->sst2DSrcFmtShadow = fmt;
}  

void
TDFXSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int srcX, int srcY, 
				 int dstX, int dstY, int w, int h) 
{
  TDFXPtr pTDFX;

  TDFXTRACEACCEL("TDFXSubsequentScreenToScreenCopy\n srcX=%d srcY=%d"
                 " dstX=%d dstY=%d w=%d h=%d\n", srcX, srcY, dstX, dstY, w, h);
  pTDFX=TDFXPTR(pScrn);
  TDFXMatchState(pTDFX);

  if (pTDFX->Cmd&SST_2D_Y_BOTTOM_TO_TOP) {
    srcY += h-1;
    dstY += h-1;
  } 
  if (pTDFX->Cmd&SST_2D_X_RIGHT_TO_LEFT) {
    srcX += w-1;
    dstX += w-1;
  }
  if ((srcY>=dstY-32 && srcY<=dstY)||
      (srcY>=pTDFX->prevBlitDest.y1-32 && srcY<=pTDFX->prevBlitDest.y1)) {
    TDFXSendNOP(pScrn);
  }
  pTDFX->sync(pScrn);

  TDFXMakeRoom(pTDFX, 4);
  DECLARE(SSTCP_DSTSIZE|SSTCP_DSTXY|SSTCP_SRCXY|SSTCP_COMMAND);
  TDFXWriteLong(pTDFX, SST_2D_SRCXY, (srcX&0x1FFF) | ((srcY&0x1FFF)<<16));
  TDFXWriteLong(pTDFX, SST_2D_DSTSIZE, (w&0x1FFF) | ((h&0x1FFF)<<16));
  TDFXWriteLong(pTDFX, SST_2D_DSTXY, (dstX&0x1FFF) | ((dstY&0x1FFF)<<16));
  TDFXWriteLong(pTDFX, SST_2D_COMMAND, pTDFX->Cmd|SST_2D_GO);

  pTDFX->prevBlitDest.y1=dstY;
}

void
TDFXSetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop, 
		      unsigned int planemask)
{
  TDFXPtr pTDFX;
  int fmt;

  TDFXTRACEACCEL("TDFXSetupForSolidFill color=%d rop=%d planemask=%d\n",
                 color, rop, planemask);
  pTDFX=TDFXPTR(pScrn);
  TDFXClearState(pScrn);

  pTDFX->Cmd=TDFXROPCvt[rop]<<24;
  if (pTDFX->cpp==1) fmt=(1<<16)|pTDFX->stride; 
  else fmt=((pTDFX->cpp+1)<<16)|pTDFX->stride;

  TDFXMakeRoom(pTDFX, 3);
  DECLARE(SSTCP_DSTFORMAT|SSTCP_COLORFORE|
		 SSTCP_COLORBACK);
  TDFXWriteLong(pTDFX, SST_2D_DSTFORMAT, fmt);
  pTDFX->sst2DDstFmtShadow = fmt;
  TDFXWriteLong(pTDFX, SST_2D_COLORBACK, color);
  TDFXWriteLong(pTDFX, SST_2D_COLORFORE, color);
}

void
TDFXSubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
  /* Also called by TDFXSubsequentMono8x8PatternFillRect */
  TDFXPtr pTDFX;

  TDFXTRACEACCEL("TDFXSubsequentSolidFillRect x=%d y=%d w=%d h=%d\n", 
		 x, y, w, h);
  pTDFX=TDFXPTR(pScrn);
  TDFXMatchState(pTDFX);

  TDFXMakeRoom(pTDFX, 3);
  DECLARE(SSTCP_DSTSIZE|SSTCP_DSTXY|SSTCP_COMMAND);
  TDFXWriteLong(pTDFX, SST_2D_DSTSIZE, ((h&0x1FFF)<<16) | (w&0x1FFF));
  TDFXWriteLong(pTDFX, SST_2D_DSTXY, ((y&0x1FFF)<<16) | (x&0x1FFF));
  TDFXWriteLong(pTDFX, SST_2D_COMMAND, pTDFX->Cmd | SST_2D_RECTANGLEFILL |
		SST_2D_GO);
}

static void
TDFXSetupForMono8x8PatternFill(ScrnInfoPtr pScrn, int patx, int paty,
			       int fg, int bg, int rop, unsigned int planemask)
{
  TDFXPtr pTDFX;
  int fmt;

  TDFXTRACEACCEL("TDFXSetupForMono8x8PatternFill patx=%x paty=%x fg=%d"
                 " bg=%d rop=%d planemask=%d\n", patx, paty, fg, bg, rop,
		 planemask);
  pTDFX=TDFXPTR(pScrn);
  TDFXClearState(pScrn);

  pTDFX->Cmd = (TDFXROPCvt[rop+ROP_PATTERN_OFFSET]<<24) |
    SST_2D_MONOCHROME_PATTERN;
  if (bg==-1) {
    pTDFX->Cmd |= SST_2D_TRANSPARENT_MONOCHROME;
  }
  if (pTDFX->cpp==1) fmt=(1<<16)|pTDFX->stride; 
  else fmt=((pTDFX->cpp+1)<<16)|pTDFX->stride;

  TDFXMakeRoom(pTDFX, 5);
  DECLARE(SSTCP_DSTFORMAT|SSTCP_PATTERN0ALIAS
		  |SSTCP_PATTERN1ALIAS|SSTCP_COLORFORE|
		  SSTCP_COLORBACK);
  TDFXWriteLong(pTDFX, SST_2D_DSTFORMAT, fmt);
  pTDFX->sst2DDstFmtShadow = fmt;
  TDFXWriteLong(pTDFX, SST_2D_PATTERN0, patx);
  TDFXWriteLong(pTDFX, SST_2D_PATTERN1, paty);
  TDFXWriteLong(pTDFX, SST_2D_COLORBACK, bg);
  TDFXWriteLong(pTDFX, SST_2D_COLORFORE, fg);
}

static void
TDFXSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, int patx, int paty,
				     int x, int y, int w, int h)
{
  TDFXPtr pTDFX;

  TDFXTRACEACCEL("TDFXSubsequentMono8x8PatternFillRect patx=%x paty=%x"
                 " x=%d y=%d w=%d h=%d\n", patx, paty, x, y, w, h);
  pTDFX=TDFXPTR(pScrn);

  pTDFX->Cmd |= ((patx&0x7)<<SST_2D_X_PATOFFSET_SHIFT) |
    ((paty&0x7)<<SST_2D_Y_PATOFFSET_SHIFT);

  TDFXSubsequentSolidFillRect(pScrn, x, y, w, h);
}

static void
TDFXSetupForSolidLine(ScrnInfoPtr pScrn, int color, int rop, 
		      unsigned int planemask)
{
  TDFXPtr pTDFX;

  TDFXTRACEACCEL("TDFXSetupForSolidLine\n");
  pTDFX=TDFXPTR(pScrn);
  TDFXClearState(pScrn);

  pTDFX->Cmd = (TDFXROPCvt[rop]<<24);

  TDFXMakeRoom(pTDFX, 2);
  DECLARE(SSTCP_COLORFORE|SSTCP_COLORBACK);
  TDFXWriteLong(pTDFX, SST_2D_COLORBACK, color);
  TDFXWriteLong(pTDFX, SST_2D_COLORFORE, color);
}

static void
TDFXSubsequentSolidTwoPointLine(ScrnInfoPtr pScrn, int srcx, int srcy,
				int dstx, int dsty, int flags)
{
  /* Also used by TDFXSubsequentDashedTwoPointLine */
  TDFXPtr pTDFX;

  TDFXTRACEACCEL("TDFXSubsequentSolidTwoPointLine "
		 "srcx=%d srcy=%d dstx=%d dsty=%d flags=%d\n",
		 srcx, srcy, dstx, dsty, flags);
  pTDFX=TDFXPTR(pScrn);
  TDFXMatchState(pTDFX);

  if (flags&OMIT_LAST) pTDFX->Cmd|=SST_2D_POLYLINE;
  else pTDFX->Cmd|=SST_2D_LINE;

  TDFXMakeRoom(pTDFX, 3);
  DECLARE(SSTCP_SRCXY|SSTCP_DSTXY|SSTCP_COMMAND);
  TDFXWriteLong(pTDFX, SST_2D_SRCXY, (srcy&0x1FFF)<<16 | (srcx&0x1FFF));
  TDFXWriteLong(pTDFX, SST_2D_DSTXY, (dsty&0x1FFF)<<16 | (dstx&0x1FFF));
  TDFXWriteLong(pTDFX, SST_2D_COMMAND, pTDFX->Cmd|SST_2D_GO);
}

static void
TDFXSubsequentSolidHorVertLine(ScrnInfoPtr pScrn, int x, int y, int len, 
			       int dir)
{
  TDFXPtr pTDFX;

  TDFXTRACEACCEL("TDFXSubsequentSolidHorVertLine\n");
  pTDFX=TDFXPTR(pScrn);
  TDFXMatchState(pTDFX);

  TDFXMakeRoom(pTDFX, 3);
  DECLARE(SSTCP_SRCXY|SSTCP_DSTXY|SSTCP_COMMAND);
  TDFXWriteLong(pTDFX, SST_2D_SRCXY, (y&0x1FFF)<<16 | (x&0x1FFF));
  if (dir == DEGREES_0)
    TDFXWriteLong(pTDFX, SST_2D_DSTXY, (y&0x1FFF)<<16 | ((x+len)&0x1FFF));
  else
    TDFXWriteLong(pTDFX, SST_2D_DSTXY, ((y+len)&0x1FFF)<<16 | (x&0x1FFF));
  TDFXWriteLong(pTDFX, SST_2D_COMMAND, pTDFX->Cmd|SST_2D_POLYLINE|SST_2D_GO);
}

static void
TDFXNonTEGlyphRenderer(ScrnInfoPtr pScrn, int x, int y, int n, 
		       NonTEGlyphPtr glyphs, BoxPtr pbox, int fg, int rop,
		       unsigned int planemask)
{
  TDFXPtr pTDFX;
  int ndwords;
  int g;
  NonTEGlyphPtr glyph;

  TDFXTRACEACCEL("TDFXNonTEGlyphRenderer\n");
  pTDFX=TDFXPTR(pScrn);
  TDFXClearState(pScrn);
  /* Don't bother fixing clip1, we're going to change it anyway */
  pTDFX->DrawState&=~DRAW_STATE_CLIP1CHANGED;
  TDFXMatchState(pTDFX);
  /* We're changing clip1 so make sure we use it and flag it */
  pTDFX->Cmd|=SST_2D_USECLIP1;
  pTDFX->DrawState|=DRAW_STATE_CLIP1CHANGED;

  pTDFX->Cmd|=(TDFXROPCvt[rop]<<24)|SST_2D_TRANSPARENT_MONOCHROME;
  pTDFX->Cmd|=SST_2D_HOSTTOSCRNBLIT;

  TDFXMakeRoom(pTDFX, 6);
  DECLARE(SSTCP_CLIP1MIN|SSTCP_CLIP1MAX|SSTCP_SRCFORMAT|
	  SSTCP_SRCXY|SSTCP_COLORFORE|SSTCP_COMMAND);
  TDFXWriteLong(pTDFX, SST_2D_CLIP1MIN, ((pbox->y1&0x1FFF)<<16) |
		(pbox->x1&0x1FFF));
  TDFXWriteLong(pTDFX, SST_2D_CLIP1MAX, ((pbox->y2&0x1FFF)<<16) |
		(pbox->x2&0x1FFF));
#if X_BYTE_ORDER == X_BIG_ENDIAN
  TDFXWriteLong(pTDFX, SST_2D_SRCFORMAT, SST_2D_PIXFMT_1BPP |
		SST_2D_SOURCE_PACKING_DWORD | BIT(20));
#else
  TDFXWriteLong(pTDFX, SST_2D_SRCFORMAT, SST_2D_PIXFMT_1BPP |
		SST_2D_SOURCE_PACKING_DWORD);
#endif
  pTDFX->sst2DSrcFmtShadow = SST_2D_PIXFMT_1BPP | SST_2D_SOURCE_PACKING_DWORD;
  TDFXWriteLong(pTDFX, SST_2D_SRCXY, 0);
  TDFXWriteLong(pTDFX, SST_2D_COLORFORE, fg);
  TDFXWriteLong(pTDFX, SST_2D_COMMAND, pTDFX->Cmd);

  for (g=0, glyph=glyphs; g<n; g++, glyph++) {
    int dx = x+glyph->start;
    int dy = y-glyph->yoff;
    int w = glyph->end - glyph->start;
    int *glyph_data = (int*)glyph->bits;

    if (!glyph->srcwidth) continue;
    ndwords = (glyph->srcwidth+3)>>2;
    ndwords *= glyph->height;

    TDFXMakeRoom(pTDFX, 2);
    DECLARE(SSTCP_DSTSIZE|SSTCP_DSTXY);
    TDFXWriteLong(pTDFX, SST_2D_DSTSIZE, ((glyph->height&0x1FFF)<<16) |
		  (w&0x1FFF));
    TDFXWriteLong(pTDFX, SST_2D_DSTXY, ((dy&0x1FFF)<<16) | (dx&0x1FFF));

    do {
      int i = ndwords;
      int j;

      if (i>30) i=30;
      TDFXMakeRoom(pTDFX, i);
      DECLARE_LAUNCH(i, 0);
      for (j=0; j<i; j++) {
#if X_BYTE_ORDER == X_BIG_ENDIAN
	TDFXWriteLong(pTDFX, SST_2D_LAUNCH, *glyph_data);
#else
	TDFXWriteLong(pTDFX, SST_2D_LAUNCH, XAAReverseBitOrder(*glyph_data));
#endif
	glyph_data++;
      }
      ndwords -= i;
    } while (ndwords);
  }
}

static void
TDFXSetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, int rop,
                       unsigned int planemask, int length,
		       unsigned char *pattern)
{
  TDFXPtr pTDFX;
#if X_BYTE_ORDER == X_BIG_ENDIAN
  unsigned int pat = *(unsigned int *)pattern;
#endif

  TDFXTRACEACCEL("TDFXSetupForDashedLine\n");
  pTDFX=TDFXPTR(pScrn);

#if X_BYTE_ORDER == X_BIG_ENDIAN
  pat=((pat & 0xAAAAAAAA) >> 1) | ((pat & 0x55555555) << 1);
  pat=((pat & 0xCCCCCCCC) >> 2) | ((pat & 0x33333333) << 2);
  pat=((pat & 0xF0F0F0F0) >> 4) | ((pat & 0x0F0F0F0F) << 4);
  pat=((pat & 0xFF00FF00) >> 8) | ((pat & 0x00FF00FF) << 8);
  pat=((pat & 0xFFFF0000) >> 16) | ((pat & 0x0000FFFF) << 16);
#endif

  TDFXClearState(pScrn);

  pTDFX->Cmd = (TDFXROPCvt[rop]<<24) | SST_2D_STIPPLE_LINE;
  if(bg == -1) {
    pTDFX->Cmd |= SST_2D_TRANSPARENT_MONOCHROME;
  }
  pTDFX->DashedLineSize = ((length-1)&0xFF)+1;

  TDFXMakeRoom(pTDFX, 3);
  DECLARE(SSTCP_COLORFORE|SSTCP_COLORBACK|SSTCP_LINESTIPPLE);
#if X_BYTE_ORDER == X_BIG_ENDIAN
  TDFXWriteLong(pTDFX, SST_2D_LINESTIPPLE, pat);
#else
  TDFXWriteLong(pTDFX, SST_2D_LINESTIPPLE, *(int *)pattern);
#endif
  TDFXWriteLong(pTDFX, SST_2D_COLORBACK, bg);
  TDFXWriteLong(pTDFX, SST_2D_COLORFORE, fg);
}

static void
TDFXSubsequentDashedTwoPointLine(ScrnInfoPtr pScrn, int x1, int y1,
                                 int x2, int y2, int flags, int phase)
{
  TDFXPtr pTDFX;
  int linestyle;

  TDFXTRACEACCEL("TDFXSubsequentDashedTwoPointLine\n");
  pTDFX=TDFXPTR(pScrn);

  linestyle = ((pTDFX->DashedLineSize-1)<<8) |
              (((phase%pTDFX->DashedLineSize)&0x1F)<<24);
  
  TDFXMakeRoom(pTDFX, 1);
  DECLARE(SSTCP_LINESTYLE);
  TDFXWriteLong(pTDFX, SST_2D_LINESTYLE, linestyle);
  
  TDFXSubsequentSolidTwoPointLine(pScrn, x1, y1, x2, y2, flags);
}

#ifdef ENABLE_SS_COLOR_EXPAND_FILL
static void
TDFXSetupForScreenToScreenColorExpandFill(ScrnInfoPtr pScrn, int fg, int bg,
                                          int rop, unsigned int planemask)
{
  TDFXPtr pTDFX;

  TDFXTRACEACCEL("TDFXSetupForScreenToScreenColorExpandFill\n");
  pTDFX=TDFXPTR(pScrn);
  TDFXClearState(pScrn);
  
  TDFXMatchState(pTDFX);
  pTDFX->Cmd|=SST_2D_SCRNTOSCRNBLIT|(TDFXROPCvt[rop]<<24);
  
  if (bg==-1) {
    pTDFX->Cmd |= SST_2D_TRANSPARENT_MONOCHROME;
  }
  TDFXMakeRoom(pTDFX, 2);
  DECLARE(SSTCP_COLORFORE|SSTCP_COLORBACK);
  TDFXWriteLong(pTDFX, SST_2D_COLORBACK, bg);
  TDFXWriteLong(pTDFX, SST_2D_COLORFORE, fg);
}

static void
TDFXSubsequentScreenToScreenColorExpandFill(ScrnInfoPtr pScrn, int x, int y,
                                            int w, int h, int srcx, int srcy,
                                            int offset)
{
  TDFXPtr pTDFX;
  int fmt;

  TDFXTRACEACCEL("TDFXSubsequentScreenToScreenColorExpandFill "
		 "x=%d y=%d w=%d h=%d srcx=%d srcy=%d offset=%d\n", 
		 x, y, w, h, srcx, srcy, offset);
  pTDFX=TDFXPTR(pScrn);
  /* Don't bother resetting clip1 since we're changing it anyway */
  pTDFX->DrawState&=~DRAW_STATE_CLIP1CHANGED;
  TDFXMatchState(pTDFX);
  /* We're changing clip1 so make sure we use it and flag it */
  pTDFX->Cmd|=SST_2D_USECLIP1;
  pTDFX->DrawState|=DRAW_STATE_CLIP1CHANGED;

  if (srcy>=pTDFX->prevBlitDest.y1-8 && srcy<=pTDFX->prevBlitDest.y1) {
    TDFXSendNOP(pScrn);
  }

  if (pTDFX->cpp==1) fmt=(1<<16)|pTDFX->stride; 
  else fmt=(pTDFX->cpp+1)<<16|pTDFX->stride;

  TDFXMakeRoom(pTDFX, 8);
  DECLARE(SSTCP_SRCFORMAT|SSTCP_SRCXY|SSTCP_DSTFORMAT |
	  SSTCP_DSTSIZE|SSTCP_DSTXY|SSTCP_COMMAND |
	  SSTCP_CLIP1MIN|SSTCP_CLIP1MAX);
  TDFXWriteLong(pTDFX,SST_2D_DSTFORMAT, fmt);
  pTDFX->sst2DDstFmtShadow = fmt;
  TDFXWriteLong(pTDFX,SST_2D_CLIP1MIN, (x&0x1FFF) | ((y&0x1FFF)<<16));
  TDFXWriteLong(pTDFX,SST_2D_CLIP1MAX, ((x+w)&0x1FFF) | (((y+h)&0x1FFF)<<16));
  TDFXWriteLong(pTDFX,SST_2D_SRCFORMAT, pTDFX->stride);
  pTDFX->sst2DSrcFmtShadow = pTDFX->stride;
  TDFXWriteLong(pTDFX,SST_2D_SRCXY, (srcx&0x1FFF) | ((srcy&0x1FFF)<<16));
  TDFXWriteLong(pTDFX,SST_2D_DSTSIZE, ((w+offset)&0x1FFF) | ((h&0x1FFF)<<16));
  TDFXWriteLong(pTDFX,SST_2D_DSTXY, ((x-offset)&0x1FFF) | ((y&0x1FFF)<<16));
  TDFXWriteLong(pTDFX,SST_2D_COMMAND, pTDFX->Cmd|SST_2D_GO);

  pTDFX->prevBlitDest.y1=y;
}
#endif

static void
TDFXSetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScrn, int fg, int bg,
                                       int rop, unsigned int planemask)
{
  TDFXPtr pTDFX;
  
  TDFXTRACEACCEL("SetupForCPUToScreenColorExpandFill bg=%x fg=%x rop=%d\n",
                 bg, fg, rop);
  pTDFX=TDFXPTR(pScrn);
  TDFXClearState(pScrn);

  pTDFX->Cmd|=SST_2D_HOSTTOSCRNBLIT|(TDFXROPCvt[rop]<<24);
  
  if (bg == -1) {
    pTDFX->Cmd |= SST_2D_TRANSPARENT_MONOCHROME;
  }
  
  TDFXMakeRoom(pTDFX, 2);
  DECLARE(SSTCP_COLORBACK|SSTCP_COLORFORE);
  TDFXWriteLong(pTDFX, SST_2D_COLORBACK, bg);
  TDFXWriteLong(pTDFX, SST_2D_COLORFORE, fg);
}

static void
TDFXSubsequentCPUToScreenColorExpandFill(ScrnInfoPtr pScrn, int x, int y,
                                         int w, int h, int skipleft)
{
  TDFXPtr pTDFX;
  int fmt;
  
  TDFXTRACEACCEL("SubsequentCPUToScreenColorExpandFill x=%d y=%d w=%d h=%d"
                 " skipleft=%d\n", x, y, w, h, skipleft);
  pTDFX = TDFXPTR(pScrn);

  /* We're changing clip1 anyway, so don't bother to reset it */
  pTDFX->DrawState&=~DRAW_STATE_CLIP1CHANGED;
  TDFXMatchState(pTDFX);
  /* Make sure we use clip1 and flag it */
  pTDFX->Cmd|=SST_2D_USECLIP1;
  pTDFX->DrawState|=DRAW_STATE_CLIP1CHANGED;
  
  if (pTDFX->cpp==1) fmt=(1<<16)|pTDFX->stride; 
  else fmt=((pTDFX->cpp+1)<<16)|pTDFX->stride;
  pTDFX->scanlineWidth=w;

  TDFXMakeRoom(pTDFX, 8);
  DECLARE(SSTCP_CLIP1MIN|SSTCP_CLIP1MAX|SSTCP_SRCFORMAT|
	  SSTCP_DSTFORMAT|SSTCP_DSTSIZE|SSTCP_SRCXY|
	  SSTCP_DSTXY|SSTCP_COMMAND);
  TDFXWriteLong(pTDFX, SST_2D_DSTFORMAT, fmt);
  pTDFX->sst2DDstFmtShadow = fmt;
  TDFXWriteLong(pTDFX, SST_2D_CLIP1MIN, ((y&0x1FFF)<<16)|(x&0x1FFF));
  TDFXWriteLong(pTDFX, SST_2D_CLIP1MAX, (((y+h)&0x1FFF)<<16)|((x+w)&0x1FFF));
#if X_BYTE_ORDER == X_BIG_ENDIAN
  /* bit 20 byte swizzle */
  TDFXWriteLong(pTDFX, SST_2D_SRCFORMAT, ((((w+31)/32)*4) & 0x3FFF) | BIT(20));
#else
  TDFXWriteLong(pTDFX, SST_2D_SRCFORMAT,  (((w+31)/32)*4) & 0x3FFF);
#endif
  pTDFX->sst2DSrcFmtShadow = (((w+31)/32)*4) & 0x3FFF;
  TDFXWriteLong(pTDFX, SST_2D_SRCXY, skipleft&0x1F);
  TDFXWriteLong(pTDFX, SST_2D_DSTSIZE, ((w-skipleft)&0x1FFF)|((h&0x1FFF)<<16));
  TDFXWriteLong(pTDFX, SST_2D_DSTXY, ((x+skipleft)&0x1FFF) | ((y&0x1FFF)<<16));
  TDFXWriteLong(pTDFX, SST_2D_COMMAND, pTDFX->Cmd|SST_2D_GO);
}
  
static void TDFXSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno)
{
  TDFXPtr pTDFX;
  int i, size, cnt;
  CARD32 *pos;

  TDFXTRACEACCEL("SubsequentColorExpandScanline bufno=%d\n", bufno);
  pTDFX = TDFXPTR(pScrn);

  cnt=(pTDFX->scanlineWidth+31)/32;
  pos=(CARD32 *)pTDFX->scanlineColorExpandBuffers[bufno];
  while (cnt>0) {
    if (cnt>64) size=64;
    else size=cnt;
    TDFXMakeRoom(pTDFX, size);
    DECLARE_LAUNCH(size, 0);
    for (i=0; i<size; i++, pos++) {
      TDFXWriteLong(pTDFX, SST_2D_LAUNCH, *pos);
    }
    cnt-=size;
  }
}

