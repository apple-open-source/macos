/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3virge/s3v_accel.c,v 1.23 2001/12/13 18:01:50 eich Exp $ */

/*
Copyright (C) 1994-1999 The XFree86 Project, Inc.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the XFree86 Project shall not
be used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the XFree86 Project.
*/

#include "s3v.h"

#include "miline.h"
	/* cfb includes are in s3v.h */
#include "xaalocal.h"
#include "xaarop.h"

#include "servermd.h" /* LOG2_BYTES_PER_SCANLINE_PAD */

static void S3VWriteMask(CARD32*, int);

static void S3VEngineReset(ScrnInfoPtr pScrn);
/* s3v.h - static void S3VAccelSync(ScrnInfoPtr); */
static void S3VSetupForSolidFill(ScrnInfoPtr, int, int, unsigned);
static void S3VSubsequentSolidFillRect(ScrnInfoPtr, int, int, int, int);
static void S3VSubsequentSolidFillRectPlaneMask(ScrnInfoPtr, int, int, 
				int, int);
static void S3VSetupForMono8x8PatternFill(ScrnInfoPtr,int, int, int, int,
                                int, unsigned int);
static void S3VSubsequentMono8x8PatternFillRect(ScrnInfoPtr,int, int,
                                int, int, int, int);
static void S3VSubsequentMono8x8PatternFillRectPlaneMask(ScrnInfoPtr,int, int,
                                int, int, int, int);
static void S3VSetupForScreenToScreenCopy(ScrnInfoPtr, int, int, int, 
				unsigned int, int); 
static void S3VSubsequentScreenToScreenCopy(ScrnInfoPtr, int, int, int, int, 
				int, int);
static void S3VSetupForCPUToScreenColorExpand(ScrnInfoPtr, int, int, int, 
				unsigned int);
static void S3VSubsequentCPUToScreenColorExpand(ScrnInfoPtr, int, int, int, 
				int, int);
static void S3VSetupForImageWrite(ScrnInfoPtr, int, unsigned int, int, 
				int, int);
static void S3VSubsequentImageWriteRect(ScrnInfoPtr, int, int, int, int, int);
static void S3VSubsequentSolidHorVertLine(ScrnInfoPtr, int, int, int, int);
static void S3VSubsequentSolidHorVertLinePlaneMask(ScrnInfoPtr, int, int, 
				int, int);
#if 0
static void S3VSubsequentSolidBresenhamLine(ScrnInfoPtr, int, int, int, 
				int, int, int, int);
static void S3VPolylinesThinSolidWrapper(DrawablePtr, GCPtr, int, int, 
				DDXPointPtr);
static void S3VPolySegmentThinSolidWrapper(DrawablePtr, GCPtr, int, xSegment*);
#endif
static void S3VNopAllCmdSets(ScrnInfoPtr pScrn);


Bool 
S3VAccelInit(ScreenPtr pScreen)
{
    XAAInfoRecPtr infoPtr;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    S3VPtr ps3v = S3VPTR(pScrn);
    BoxRec AvailFBArea;

    ps3v->AccelInfoRec = infoPtr = XAACreateInfoRec();
    if(!infoPtr) return FALSE;

    switch(ps3v->Chipset) 
      {
      case S3_ViRGE:
      case S3_ViRGE_VX:
	ps3v->AccelFlags = BLT_BUG;
	break;
      default:
	ps3v->AccelFlags = 0;
	break;
      }

    ps3v->AccelFlags |= MONO_TRANS_BUG; /* which aren't broken ? */


    infoPtr->Flags = PIXMAP_CACHE |
		     LINEAR_FRAMEBUFFER |
		     OFFSCREEN_PIXMAPS;

    infoPtr->Sync = S3VAccelSync;

    /* Problem reports with solid fill on trio3d */
    if(!S3_TRIO_3D_SERIES(ps3v->Chipset)) 
      {
	/* Solid filled rects */
	infoPtr->SetupForSolidFill = 
	  S3VSetupForSolidFill;
	infoPtr->SubsequentSolidFillRect = 
	  S3VSubsequentSolidFillRect;
      }

    /* Screen to screen copies */
    infoPtr->SetupForScreenToScreenCopy =
        S3VSetupForScreenToScreenCopy;
    infoPtr->SubsequentScreenToScreenCopy =
        S3VSubsequentScreenToScreenCopy;
    infoPtr->ScreenToScreenCopyFlags = NO_TRANSPARENCY;

    /* Mono 8x8 patterns */
    infoPtr->SetupForMono8x8PatternFill =
        S3VSetupForMono8x8PatternFill;
    infoPtr->SubsequentMono8x8PatternFillRect =
        S3VSubsequentMono8x8PatternFillRect;
    infoPtr->Mono8x8PatternFillFlags = NO_TRANSPARENCY |
				HARDWARE_PATTERN_PROGRAMMED_BITS |
				HARDWARE_PATTERN_SCREEN_ORIGIN |
				BIT_ORDER_IN_BYTE_MSBFIRST;


#ifndef __alpha__

    /* disable color expand on GX2 until we trace down */
    /* lockups.  locate 'html'  in an xterm is a good  */
    /* test case for an AGP GX2. */
    if (!S3_ViRGE_GX2_SERIES(ps3v->Chipset)) 
      {

	
	/* CPU to screen color expansion */
	infoPtr->CPUToScreenColorExpandFillFlags =  ROP_NEEDS_SOURCE |
					CPU_TRANSFER_PAD_DWORD |
                                        SCANLINE_PAD_DWORD |
                                        BIT_ORDER_IN_BYTE_MSBFIRST |
                                  	LEFT_EDGE_CLIPPING;

	if(ps3v->AccelFlags & MONO_TRANS_BUG)
	  infoPtr->CPUToScreenColorExpandFillFlags |=  NO_TRANSPARENCY;

	infoPtr->ColorExpandRange = 0x8000;
	infoPtr->ColorExpandBase = ps3v->MapBaseDense;
	infoPtr->SetupForCPUToScreenColorExpandFill =
                S3VSetupForCPUToScreenColorExpand;
	infoPtr->SubsequentCPUToScreenColorExpandFill =
                S3VSubsequentCPUToScreenColorExpand;

      } /* if(!GX2...) */


    /* Image Writes */
    infoPtr->ImageWriteFlags =  	ROP_NEEDS_SOURCE |
					NO_TRANSPARENCY |
					CPU_TRANSFER_PAD_DWORD |
					SCANLINE_PAD_DWORD |
					NO_GXCOPY |  /* added - kjb */
					LEFT_EDGE_CLIPPING; 

    infoPtr->ImageWriteRange = 0x8000;
    infoPtr->ImageWriteBase = ps3v->MapBaseDense;
    infoPtr->SetupForImageWrite = S3VSetupForImageWrite;
    infoPtr->SubsequentImageWriteRect = S3VSubsequentImageWriteRect;
    
    /* on alpha, I see corruption in the xscreensaver program "hypercube"
       as the line acceleration is just stubs, it loses us nothing to
       disable it on alphas */
    
    /* Lines */
#if 0
    /* Bresenham lines are broken when passed through fb to xaa
       so I pulled all the line functions.  This shouldn't hurt us
       a whole lot, since the Subsequent..Bresen stuff doesn't have
       any hardware accel yet anyway...  And xaa will do horiz/vert
       lines with the rect fill (like we are doing here) anyway.
       KJB 9/11/00
    */
    infoPtr->SetupForSolidLine = S3VSetupForSolidFill;
    infoPtr->SubsequentSolidHorVertLine = S3VSubsequentSolidHorVertLine;
    infoPtr->SubsequentSolidBresenhamLine = S3VSubsequentSolidBresenhamLine;
    infoPtr->PolySegmentThinSolid = S3VPolySegmentThinSolidWrapper;
    infoPtr->PolylinesThinSolid = S3VPolylinesThinSolidWrapper;
#endif

#endif /* !__alpha__ */
    
    /* And these are screen parameters used to setup the GE */

     ps3v->Width = pScrn->displayWidth;
     					/* Bytes per pixel */
     ps3v->Bpp = pScrn->bitsPerPixel / 8;
     					/* Bytes per line */
     ps3v->Bpl = ps3v->Width * ps3v->Bpp;
     					/* ScissB is max height, minus 1k */
					/* for hwcursor?, then limited by */
					/* ViRGE max height register of   */
					/* 2047 */
     ps3v->ScissB = (pScrn->videoRam * 1024 - 1024) / ps3v->Bpl;
     if (ps3v->ScissB > 2047)
         ps3v->ScissB = 2047;


    S3VEngineReset(pScrn);


    AvailFBArea.x1 = 0;
    AvailFBArea.y1 = 0;
    AvailFBArea.x2 = pScrn->displayWidth;
    AvailFBArea.y2 = (pScrn->videoRam * 1024 - 1024) / 
		     (pScrn->displayWidth * pScrn->bitsPerPixel / 8);

    xf86InitFBManager(pScreen, &AvailFBArea);

    /* make sure offscreen pixmaps aren't bigger than our address space */
    infoPtr->maxOffPixWidth  = 2048;
    infoPtr->maxOffPixHeight = 2048;

    return (XAAInit(pScreen, infoPtr));
} 


Bool 
S3VAccelInit32(ScreenPtr pScreen) 
{
   return FALSE; 
}

void
S3VNopAllCmdSets(ScrnInfoPtr pScrn)
{
  int i;
  int max_it=1000;
  S3VPtr ps3v = S3VPTR(pScrn);

  if (xf86GetVerbosity() > 1) {
     ErrorF("\tTrio3D -- S3VNopAllCmdSets: SubsysStats#1 = 0x%08lx\n",
        IN_SUBSYS_STAT());
  }

  mem_barrier();
  for(i=0;i<max_it;i++) {
    if( (IN_SUBSYS_STAT() & 0x3f802000 & 0x20002000) == 0x20002000) {
      break;
    }
  }

  if(i!=max_it) {
    if (xf86GetVerbosity() > 1) ErrorF("\tTrio3D -- S3VNopAllCmdSets: state changed after %d iterations\n",i);
  } else {
    if (xf86GetVerbosity() > 1) ErrorF("\tTrio3D -- S3VNopAllCmdSets: state DIDN'T changed after %d iterations\n",max_it);
  }

  WaitQueue(5);

  OUTREG(CMD_SET, CMD_NOP);

  if (xf86GetVerbosity() > 1) {
     ErrorF("\tTrio3D -- S3VNopAllCmdSets: SubsysStats#2 = 0x%08lx\n",
        IN_SUBSYS_STAT());
  }
}

void
S3VGEReset(ScrnInfoPtr pScrn, int from_timeout, int line, char *file)
{
    unsigned long gs1, gs2;   /* -- debug info for graphics state -- */
    unsigned char tmp, sr1, resetidx=0x66;  /* FIXME */
    int r;
    int ge_was_on = 0;
    CARD32 fifo_control = 0, miu_control = 0;
    CARD32 streams_timeout = 0, misc_timeout = 0;
    vgaHWPtr hwp = VGAHWPTR(pScrn);
	S3VPtr ps3v = S3VPTR(pScrn);
  	int vgaCRIndex, vgaCRReg, vgaIOBase;
  	vgaIOBase = hwp->IOBase;
  	vgaCRIndex = vgaIOBase + 4;
  	vgaCRReg = vgaIOBase + 5;  	  


    if (S3_TRIO_3D_SERIES(ps3v->Chipset)) {
      VGAOUT8(0x3c4,0x01);
      sr1 = VGAIN8(0x3c5);

      if (sr1 & 0x20) {
        if (xf86GetVerbosity() > 1)
          ErrorF("\tTrio3D -- Display is on...turning off\n");
        VGAOUT8(0x3c5,sr1 & ~0x20);
        VerticalRetraceWait();
      }     
    }

    if (from_timeout) {
      if (ps3v->GEResetCnt++ < 10 || xf86GetVerbosity() > 1)
	ErrorF("\tS3VGEReset called from %s line %d\n",file,line);
    }
    else {
      if (S3_TRIO_3D_SERIES(ps3v->Chipset))
        S3VNopAllCmdSets(pScrn);
      WaitIdleEmpty();
    }


    if (from_timeout && (ps3v->Chipset == S3_ViRGE || ps3v->Chipset == S3_ViRGE_VX || ps3v->Chipset == S3_ViRGE_DXGX)) {
      /* reset will trash these registers, so save them */
      fifo_control    = INREG(FIFO_CONTROL_REG);
      miu_control     = INREG(MIU_CONTROL_REG);
      streams_timeout = INREG(STREAMS_TIMEOUT_REG);
      misc_timeout    = INREG(MISC_TIMEOUT_REG);
    }

    if(ps3v->Chipset == S3_ViRGE_VX){
        VGAOUT8(vgaCRIndex, 0x63);
        }
    else {
        VGAOUT8(vgaCRIndex, 0x66);
        }
  if (!S3_TRIO_3D_SERIES(ps3v->Chipset)) {
    tmp = VGAIN8(vgaCRReg);
    
    usleep(10000);
    for (r=1; r<10; r++) {  /* try multiple times to avoid lockup of ViRGE/MX */
      VGAOUT8(vgaCRReg, tmp | 0x02);
      usleep(10000);
      VGAOUT8(vgaCRReg, tmp & ~0x02);
      usleep(10000);

      xf86ErrorFVerb(VERBLEV, "	S3VGEReset sub_stat=%x \n", 
   	IN_SUBSYS_STAT()
	);

      if (!from_timeout) 
        WaitIdleEmpty();

      OUTREG(DEST_SRC_STR, ps3v->Bpl << 16 | ps3v->Bpl);
      
      usleep(10000);
      if (((IN_SUBSYS_STAT() & 0x3f00) != 0x3000)) 
	xf86ErrorFVerb(VERBLEV, "restarting S3 graphics engine reset %2d ...\n",r);
      else
	break;
    } 
    } else {
    usleep(10000);

    for (r=1; r<10; r++) {
      VerticalRetraceWait();
      VGAOUT8(vgaCRIndex,resetidx);
      tmp = VGAIN8(vgaCRReg);

      VGAOUT8(0x3c4,0x01);
      sr1 = VGAIN8(0x3c5);

      if(sr1 & 0x20) {
        if(xf86GetVerbosity() > 1) {
          ErrorF("\tTrio3D -- Upps Display is on again ...turning off\n");
        }
        VGAOUT8(0x3c4,0x01);
        VerticalRetraceWait();
        VGAOUT8(0x3c5,sr1 & ~0x20);
      }

      VerticalRetraceWait();
      gs1   = (long) IN_SUBSYS_STAT();

      /* turn off the GE */

      VGAOUT8(vgaCRIndex,resetidx);
      if(tmp & 0x01) {
	/*        tmp &= ~0x01; */
        VGAOUT8(vgaCRReg, tmp);
        ge_was_on = 1;
        usleep(10000);
      }

      gs2   = (long) IN_SUBSYS_STAT();
      VGAOUT8(vgaCRReg, (tmp | 0x02));
      usleep(10000);

      VerticalRetraceWait();
      VGAOUT8(vgaCRIndex,resetidx);
      VGAOUT8(vgaCRReg, (tmp & ~0x02));
      usleep(10000);

      if(ge_was_on) {
        tmp |= 0x01;
        VGAOUT8(vgaCRReg, tmp);
        usleep(10000);
      }

      if (xf86GetVerbosity() > 2) {
          ErrorF("\tTrio3D -- GE was %s ST#1: 0x%08lx ST#2: 0x%08lx\n",
		 (ge_was_on) ? "on" : "off", gs1, gs2);
      }

      VerticalRetraceWait();

      if (!from_timeout) {
	S3VNopAllCmdSets(pScrn);
        WaitIdleEmpty();
      }

      OUTREG(DEST_SRC_STR, ps3v->Bpl << 16 | ps3v->Bpl);
      usleep(10000);

      if((IN_SUBSYS_STAT() & 0x3f802000 & 0x20002000) != 0x20002000) {
        if(xf86GetVerbosity() > 1)
          ErrorF("restarting S3 graphics engine reset %2d ..."
                 "%lx\n",r,IN_SUBSYS_STAT());
      }
        else
          break;
    }
    }
    
    if (from_timeout && (ps3v->Chipset == S3_ViRGE || ps3v->Chipset == S3_ViRGE_VX
			 || ps3v->Chipset == S3_ViRGE_DXGX)) {
      /* restore trashed registers */
      OUTREG(FIFO_CONTROL_REG, fifo_control);
      OUTREG(MIU_CONTROL_REG, miu_control);
      OUTREG(STREAMS_TIMEOUT_REG, streams_timeout);
      OUTREG(MISC_TIMEOUT_REG, misc_timeout);
    }

    WAITFIFO(2);
/*      SETB_SRC_BASE(0); */
/*      SETB_DEST_BASE(0);    */
    OUTREG(SRC_BASE, 0);
    OUTREG(DEST_BASE, 0);

  	WAITFIFO(4);
    OUTREG(CLIP_L_R, ((0) << 16) | ps3v->Width);
    OUTREG(CLIP_T_B, ((0) << 16) | ps3v->ScissB);
    OUTREG(MONO_PAT_0, ~0);
    OUTREG(MONO_PAT_1, ~0);

    if (!from_timeout && S3_TRIO_3D_SERIES(ps3v->Chipset))
      S3VNopAllCmdSets(pScrn);
}

/* The sync function for the GE */
void
S3VAccelSync(ScrnInfoPtr pScrn)
{
    S3VPtr ps3v = S3VPTR(pScrn);

    WAITIDLE();
}


static void
S3VEngineReset(ScrnInfoPtr pScrn)
{
    S3VPtr ps3v = S3VPTR(pScrn);

    ps3v->SrcBaseY = 0;
    ps3v->DestBaseY = 0; 
    ps3v->Stride = pScrn->displayWidth * pScrn->bitsPerPixel >> 3;

    switch(pScrn->bitsPerPixel) {
    case 8: 	ps3v->CommonCmd = DRAW | DST_8BPP;
		ps3v->FullPlaneMask = 0x000000ff;
		ps3v->bltbug_width1 = 51;
		ps3v->bltbug_width2 = 64;
		break;
    case 16: 	ps3v->CommonCmd = DRAW | DST_16BPP;
		ps3v->FullPlaneMask = 0x0000ffff;
		ps3v->bltbug_width1 = 26;
		ps3v->bltbug_width2 = 32;
		break;
    case 24: 	ps3v->CommonCmd = DRAW | DST_24BPP;
		ps3v->FullPlaneMask = 0x00ffffff;
		ps3v->bltbug_width1 = 16;
		ps3v->bltbug_width2 = 22;
		break;
    }


    WAITFIFO(5);
    OUTREG(SRC_BASE, 0);
    OUTREG(DEST_BASE, 0);
    OUTREG(DEST_SRC_STR, ps3v->Stride | (ps3v->Stride << 16));

    OUTREG(CLIP_L_R, ((0) << 16) | ps3v->Width);
    OUTREG(CLIP_T_B, ((0) << 16) | ps3v->ScissB);
}


static void
S3VWriteMask(
   CARD32 *dstBase,
   int dwords
){
  /* on alphas, be sure to call this with MapBaseDense, not MapBase! */
   int numLeft;
   CARD32 *dst = dstBase;

   while(dwords >= 8192) {
	numLeft = 8192;
	while(numLeft) {
	  dst[0] = ~0; dst[1] = ~0;
	  dst[2] = ~0; dst[3] = ~0;
	  dst += 4;
	  numLeft -= 4;
	}
	dwords -= 8192;
	dst = dstBase;
   }
   while(dwords >= 4) {
	dst[0] = ~0; dst[1] = ~0;
	dst[2] = ~0; dst[3] = ~0;
	dst += 4;
	dwords -= 4;
   }
   if(!dwords) return;
   dst[0] = ~0;
   if(dwords == 1) return;
   dst[1] = ~0;
   if(dwords == 2) return;
   dst[2] = ~0;

   return;
}


	/************************\
	|  Solid Filled Rects    |
	\************************/

static void 
S3VSetupForSolidFill(
   ScrnInfoPtr pScrn, 
   int color, int rop, 
   unsigned int planemask
){
    S3VPtr ps3v = S3VPTR(pScrn);
    int mix;

    mix = XAAHelpSolidROP(pScrn, &color, planemask, &rop);

    ps3v->AccelCmd = ps3v->CommonCmd | (rop << 17) |
			CMD_XP | CMD_YP | CMD_AUTOEXEC | CMD_BITBLT;
 
    if(mix & ROP_SRC) {
	ps3v->AccelCmd |= MIX_CPUDATA | CMD_ITA_DWORD | MIX_MONO_SRC;
	ps3v->AccelInfoRec->SubsequentSolidFillRect = 
		S3VSubsequentSolidFillRectPlaneMask;
	ps3v->AccelInfoRec->SubsequentSolidHorVertLine =
		S3VSubsequentSolidHorVertLinePlaneMask;	
	WAITFIFO(5);
	OUTREG(SRC_FG_CLR, planemask);
    } else {
	ps3v->AccelInfoRec->SubsequentSolidFillRect =
		S3VSubsequentSolidFillRect;	
	ps3v->AccelInfoRec->SubsequentSolidHorVertLine =
		S3VSubsequentSolidHorVertLine;	
	WAITFIFO(4);
    }

    if(mix & ROP_PAT) {
	ps3v->AccelCmd |= MIX_MONO_PATT;
	OUTREG(PAT_FG_CLR, color);
	OUTREG(MONO_PAT_0, ~0);
	OUTREG(MONO_PAT_1, ~0);
    }

    OUTREG(CMD_SET, ps3v->AccelCmd);
}


void 
S3VSubsequentSolidFillRect(
   ScrnInfoPtr pScrn, 
   int x, int y, 
   int w, int h
){
    S3VPtr ps3v = S3VPTR(pScrn);

    CHECK_DEST_BASE(y,h);

    WAITFIFO(2);
    OUTREG(RWIDTH_HEIGHT, ((w - 1) << 16) | h);
    WAITCMD();
    OUTREG(RDEST_XY, (x << 16) | y);
}


void 
S3VSubsequentSolidFillRectPlaneMask(
   ScrnInfoPtr pScrn, 
   int x, int y, 
   int w, int h
){
    S3VPtr ps3v = S3VPTR(pScrn);
    int dwords;

    CHECK_DEST_BASE(y,h);

    dwords = ((w + 31) >> 5) * h;

    WAITFIFO(2);
    OUTREG(RWIDTH_HEIGHT, ((w - 1) << 16) | h);
    WAITCMD();
    OUTREG(RDEST_XY, (x << 16) | y);
    S3VWriteMask((CARD32*)ps3v->MapBaseDense, dwords);
}


	/**************************\
	|  Screen to Screen Copies |
	\**************************/

static void 
S3VSetupForScreenToScreenCopy(
   ScrnInfoPtr pScrn, 
   int xdir, int ydir, 
   int rop, 
   unsigned int planemask, 
   int trans
){
    S3VPtr ps3v = S3VPTR(pScrn);

    planemask &= ps3v->FullPlaneMask;
    ps3v->AccelCmd = ps3v->CommonCmd | CMD_AUTOEXEC | CMD_BITBLT;
 
    if(planemask != ps3v->FullPlaneMask) {     
        ps3v->AccelCmd |= (XAACopyROP_PM[rop] << 17) | MIX_MONO_PATT;
	WAITFIFO(4);
	OUTREG(PAT_FG_CLR, planemask);
	OUTREG(MONO_PAT_0, ~0);
	OUTREG(MONO_PAT_1, ~0);
        }
    else {
        ps3v->AccelCmd |= XAACopyROP[rop] << 17;
	WAITFIFO(1);
        }
    if(xdir == 1) ps3v->AccelCmd |= CMD_XP;
    if(ydir == 1) ps3v->AccelCmd |= CMD_YP;
   
    OUTREG(CMD_SET, ps3v->AccelCmd);
}


static void 
S3VSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1, 
        int x2, int y2, int w, int h)
{
    S3VPtr ps3v = S3VPTR(pScrn);

    CHECK_SRC_BASE(y1,h);
    CHECK_DEST_BASE(y2,h);

    w--;

    if(!(ps3v->AccelCmd & CMD_YP)) {
	y1 += h - 1; y2 += h - 1;
    }

    if(!(ps3v->AccelCmd & CMD_XP)) {
	x1 += w; x2 += w;
    }

    WAITFIFO(3);
    OUTREG(RWIDTH_HEIGHT, (w << 16) | h);
    OUTREG(RSRC_XY, (x1 << 16) | y1);
    WAITCMD();
    OUTREG(RDEST_XY, (x2 << 16) | y2);
}


	/*********************\
	|  8x8 Pattern fills  |
	\*********************/


static void 
S3VSetupForMono8x8PatternFill(
    ScrnInfoPtr pScrn,
    int patx, int paty, 
    int fg, int bg,
    int rop, unsigned int planemask
){
    S3VPtr ps3v = S3VPTR(pScrn);
    int mix;

    mix = XAAHelpPatternROP(pScrn, &fg, &bg, planemask, &rop);

    ps3v->AccelCmd = ps3v->CommonCmd | (rop << 17) |
			CMD_XP | CMD_YP | CMD_AUTOEXEC | CMD_BITBLT;

    if(mix & ROP_SRC) {
 	ps3v->AccelCmd |= MIX_CPUDATA | CMD_ITA_DWORD | MIX_MONO_SRC;
	ps3v->AccelInfoRec->SubsequentMono8x8PatternFillRect = 
		S3VSubsequentMono8x8PatternFillRectPlaneMask;
	WAITFIFO(6);
	OUTREG(SRC_FG_CLR, planemask);
    } else {
	ps3v->AccelInfoRec->SubsequentMono8x8PatternFillRect =
		S3VSubsequentMono8x8PatternFillRect;	
	WAITFIFO(5);
    }

    if(mix & ROP_PAT) {
	ps3v->AccelCmd |= MIX_MONO_PATT;
	OUTREG(PAT_FG_CLR, fg);
	OUTREG(PAT_BG_CLR, bg);
	OUTREG(MONO_PAT_0, patx);
	OUTREG(MONO_PAT_1, paty);
    }

    OUTREG(CMD_SET, ps3v->AccelCmd);
}


static void 
S3VSubsequentMono8x8PatternFillRect(
    ScrnInfoPtr pScrn,
    int patx, int paty,
    int x, int y, int w, int h 
){
    S3VPtr ps3v = S3VPTR(pScrn);

    CHECK_DEST_BASE(y,h);

    WAITFIFO(2);
    OUTREG(RWIDTH_HEIGHT, ((w - 1) << 16) | h);
    WAITCMD();
    OUTREG(RDEST_XY, (x << 16) | y);
}


static void 
S3VSubsequentMono8x8PatternFillRectPlaneMask(
    ScrnInfoPtr pScrn,
    int patx, int paty,
    int x, int y, int w, int h 
){
    S3VPtr ps3v = S3VPTR(pScrn);
    int dwords;

    CHECK_DEST_BASE(y,h);

    dwords = ((w + 31) >> 5) * h;

    WAITFIFO(2);
    OUTREG(RWIDTH_HEIGHT, ((w - 1) << 16) | h);
    WAITCMD();
    OUTREG(RDEST_XY, (x << 16) | y);

    S3VWriteMask((CARD32*)ps3v->MapBaseDense, dwords);
}

	/*********************************\
	|  CPU to Screen Color Expansion  |
	\*********************************/


static void
S3VSetupForCPUToScreenColorExpand(
    ScrnInfoPtr pScrn, 
    int fg, int bg, 
    int rop, 
    unsigned int planemask
){
    S3VPtr ps3v = S3VPTR(pScrn);

    planemask &= ps3v->FullPlaneMask;
    ps3v->AccelCmd = ps3v->CommonCmd | CMD_AUTOEXEC | CMD_BITBLT | 
		CMD_XP | CMD_YP | CMD_ITA_DWORD | CMD_HWCLIP |
		MIX_CPUDATA | MIX_MONO_SRC;


    if(planemask == ps3v->FullPlaneMask) { 
        ps3v->AccelCmd |= XAACopyROP[rop] << 17;
	WAITFIFO(3);
    } else {
        ps3v->AccelCmd |= (XAACopyROP_PM[rop] << 17) | MIX_MONO_PATT;
	WAITFIFO(6);
	OUTREG(MONO_PAT_0, ~0);
	OUTREG(MONO_PAT_1, ~0);
        OUTREG(PAT_FG_CLR, planemask);
    }

    if(bg == -1)
	ps3v->AccelCmd |= MIX_MONO_TRANSP;
    else
	OUTREG(SRC_BG_CLR, bg);

    OUTREG(SRC_FG_CLR, fg);
    OUTREG(CMD_SET, ps3v->AccelCmd);
}


void
S3VSubsequentCPUToScreenColorExpand(
    ScrnInfoPtr pScrn, 
    int x, int y, 
    int w, int h, 
    int skipleft
){
    S3VPtr ps3v = S3VPTR(pScrn);

    CHECK_DEST_BASE(y,h);
    WAITFIFO(3);
    OUTREG(CLIP_L_R, ((x + skipleft) << 16) | 0xffff);
    OUTREG(RWIDTH_HEIGHT, ((w - 1) << 16) | h);
    WAITCMD();
    OUTREG(RDEST_XY, (x << 16) | y);
}


    /****************\
    |  Image Writes  |
    \****************/


static void
S3VSetupForImageWrite(
   ScrnInfoPtr pScrn, 
   int rop, unsigned int planemask,
   int trans_color, int bpp, int depth
){
    S3VPtr ps3v = S3VPTR(pScrn);

    planemask &= ps3v->FullPlaneMask;
    ps3v->AccelCmd = ps3v->CommonCmd | CMD_AUTOEXEC | CMD_BITBLT | 
		MIX_CPUDATA | CMD_ITA_DWORD | CMD_HWCLIP | CMD_XP | CMD_YP;
 
    if(planemask != ps3v->FullPlaneMask) {     
        ps3v->AccelCmd |= (XAACopyROP_PM[rop] << 17) | MIX_MONO_PATT;
	WAITFIFO(4);
	OUTREG(PAT_FG_CLR, planemask);
	OUTREG(MONO_PAT_0, ~0);
	OUTREG(MONO_PAT_1, ~0);
    } else {
        ps3v->AccelCmd |= XAACopyROP[rop] << 17;
	WAITFIFO(1);
    }
   
    OUTREG(CMD_SET, ps3v->AccelCmd);
}


static void
S3VSubsequentImageWriteRect(
    ScrnInfoPtr pScrn, 
    int x, int y, 
    int w, int h, 
    int skipleft
){
    S3VPtr ps3v = S3VPTR(pScrn);

    CHECK_DEST_BASE(y,h);

    WAITFIFO(3);
    OUTREG(CLIP_L_R, ((x + skipleft) << 16) | 0xffff);
    OUTREG(RWIDTH_HEIGHT, ((w - 1) << 16) | h);
    WAITCMD();
    OUTREG(RDEST_XY, (x << 16) | y);
}


	/***********\
	|   Lines   |
	\***********/


#if 0   /* Some line funcs are disabled at the moment */

static void 
S3VPolylinesThinSolidWrapper(
   DrawablePtr     pDraw,
   GCPtr           pGC,
   int             mode,
   int             npt,
   DDXPointPtr     pPts
){
    XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_GC(pGC);
    S3VPtr ps3v = S3VPTR(infoRec->pScrn);
    ps3v->CurrentGC = pGC;
    /* fb support */
    ps3v->CurrentDrawable = pDraw;
    if(infoRec->NeedToSync) 
	S3VAccelSync(infoRec->pScrn);
    XAAPolyLines(pDraw, pGC, mode, npt, pPts);
}

static void 
S3VPolySegmentThinSolidWrapper(
   DrawablePtr     pDraw,
   GCPtr           pGC,
   int             nseg,
   xSegment        *pSeg
){
    XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_GC(pGC);
    S3VPtr ps3v = S3VPTR(infoRec->pScrn);
    ps3v->CurrentGC = pGC;
    /* fb support */
    ps3v->CurrentDrawable = pDraw;
    if(infoRec->NeedToSync) 
	S3VAccelSync(infoRec->pScrn);
    XAAPolySegment(pDraw, pGC, nseg, pSeg);
}

#endif

static void
S3VSubsequentSolidHorVertLine(
    ScrnInfoPtr pScrn,
    int x, int y, 
    int len, int dir
){
    S3VPtr ps3v = S3VPTR(pScrn);
    int w, h;

    if(dir == DEGREES_0) {
	w = len; h = 1;
    } else {
	w = 1; h = len;
    }

    CHECK_DEST_BASE(y,h);

    WAITFIFO(2);
    OUTREG(RWIDTH_HEIGHT, ((w - 1) << 16) | h);
    WAITCMD();
    OUTREG(RDEST_XY, (x << 16) | y);
}

static void
S3VSubsequentSolidHorVertLinePlaneMask(
    ScrnInfoPtr pScrn,
    int x, int y, 
    int len, int dir
){
    S3VPtr ps3v = S3VPTR(pScrn);
    int w, h, dwords;

    if(dir == DEGREES_0) {
	w = len; h = 1; dwords = (len + 31) >> 5;
    } else {
	w = 1; h = len; dwords = len;
    }
    
    CHECK_DEST_BASE(y,h);

    WAITFIFO(2);
    OUTREG(RWIDTH_HEIGHT, ((w - 1) << 16) | h);
    WAITCMD();
    OUTREG(RDEST_XY, (x << 16) | y);

    S3VWriteMask((CARD32*)ps3v->MapBaseDense, dwords);
}


#if 0   /* Line funcs are disabled at the moment */

static void (*LineFuncs[3])() = {
  cfbBresS,
  cfb16BresS,
  cfb24BresS
};

static void 
S3VSubsequentSolidBresenhamLine( 
   ScrnInfoPtr pScrn,
   int x, int y, 
   int dmaj, int dmin, 
   int e, int len, int octant
){
    S3VPtr ps3v = S3VPTR(pScrn);
    cfbPrivGCPtr devPriv;
    int Bpp = pScrn->bitsPerPixel >> 3;

    if( ps3v->UseFB )
      {
#if 1
	/*
void
fbBres (DrawablePtr     pDrawable,
        GCPtr           pGC,
        int             dashOffset,
        int             signdx,
        int             signdy,
        int             axis,
        int             x1,
        int             y1,
        int             e,
        int             e1,
        int             e3,
        int             len)

	from cfb, e3 = e2-e1.
	for the cfb call e2 = dmin - dmaj
	e1 = dmin
	e3 = e2-e1 = dmin-dmaj-dmin
	*/


	fbBres(ps3v->CurrentDrawable, ps3v->CurrentGC, 0,
           (octant & XDECREASING) ? -1 : 1,
           (octant & YDECREASING) ? -1 : 1,
           (octant & YMAJOR) ? Y_AXIS : X_AXIS,
	       /*   x, y, dmin + e, dmin, dmin-dmaj, len); */
	 x, y, dmin + e, dmin, -dmaj, len); 
#endif
#if 0
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, VERBLEV, 
		       "Bresenham dmaj=%i, -dmaj=%i, -1*dmaj=%i.\n", 
		       dmaj, -dmaj, -1*dmaj );
#endif

      }
    else
      {
	devPriv = cfbGetGCPrivate(ps3v->CurrentGC);

	/* you could trap for lines you could do here and accelerate them */

	/*
	 * void
	 * cfbBresS(rop, and, xor, addrl, nlwidth, signdx, signdy, axis, 
	 * x1, y1, e, e1, e2, len)
	 */

	(*LineFuncs[Bpp - 1])
		(devPriv->rop, devPriv->and, devPriv->xor, 
                (unsigned long*)ps3v->FBBase,
		 (pScrn->displayWidth * Bpp) >> LOG2_BYTES_PER_SCANLINE_PAD, 
                (octant & XDECREASING) ? -1 : 1, 
                (octant & YDECREASING) ? -1 : 1, 
                (octant & YMAJOR) ? Y_AXIS : X_AXIS,
                x, y, dmin + e, dmin, dmin - dmaj, len);
      } /*if(fb)*/
}

#endif


void
S3VWaitFifoGX2(S3VPtr ps3v, int slots )
{
  if(ps3v->NoPCIRetry)
    while(((INREG(SUBSYS_STAT_REG) >> 9) & 0x60) < slots){}
}



void
S3VWaitFifoMain(S3VPtr ps3v, int slots )
{
  if(ps3v->NoPCIRetry) 
    while(((INREG(SUBSYS_STAT_REG) >> 8) & 0x1f) < slots){}
}


void
S3VWaitCmdGX2(S3VPtr ps3v)
{
  while(((INREG(ADV_FUNC_CNTR) >> 6) & 0x1f) != 16){}
}


void
S3VWaitDummy(S3VPtr ps3v)
{
  /* do nothing */
}

/*EOF*/

