/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_cursor.c,v 1.29 2004/02/25 17:45:13 twini Exp $ */
/*
 * SiS hardware cursor handling
 *
 * Copyright (C) 2001-2004 by Thomas Winischhofer, Vienna, Austria.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1) Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3) The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESSED OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author:   Thomas Winischhofer <thomas@winischhofer.net>
 *
 * Idea based on code by Can-Ru Yeou, SiS Inc.
 *
 */

#include "xf86.h"
#include "xf86PciInfo.h"
#include "cursorstr.h"
#include "vgaHW.h"

#include "sis.h"
#include "sis_regs.h"
#include "sis_cursor.h"

#if 0
#define SIS300_USE_ARGB16
#else
#undef  SIS300_USE_ARGB16
#endif

extern void    SISWaitRetraceCRT1(ScrnInfoPtr pScrn);
extern void    SISWaitRetraceCRT2(ScrnInfoPtr pScrn);

/* Helper function for Xabre to convert mono image to ARGB */
/* The Xabre's cursor engine for CRT2 is buggy and can't
 * handle mono cursors. We therefore convert the mono image
 * to ARGB
 */
static void
SiSXConvertMono2ARGB(SISPtr pSiS)
{
   unsigned char *src = pSiS->CurMonoSrc;
   CARD32 *dest = pSiS->CurARGBDest;
   CARD8  chunk, mask;
   CARD32 fg = pSiS->CurFGCol | 0xff000000;
   CARD32 bg = pSiS->CurBGCol | 0xff000000;
   int i,j,k;

   if(!dest || !src) return;

   for(i = 0; i < 64; i++) {
      for(j = 0; j < 8; j++) {
         chunk = *(src + 8); mask = *src++;
	 for(k = 128; k != 0; k >>= 1) {
	    if(mask & k)       *dest++ = 0x00000000;
	    else if(chunk & k) *dest++ = fg;
	    else               *dest++ = bg;
	 }
      }
      src += 8;
   }
}

static void
SiSHideCursor(ScrnInfoPtr pScrn)
{
    SISPtr        pSiS = SISPTR(pScrn);
    unsigned char sridx, cridx;

    sridx = inSISREG(SISSR); cridx = inSISREG(SISCR);

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    andSISIDXREG(SISSR, 0x06, 0xBF);

    outSISREG(SISSR, sridx); outSISREG(SISCR, cridx);
}

static void
SiS300HideCursor(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode && (!pSiS->ForceCursorOff)) {
       if(pSiS->SecondHead) {
	  /* Head 2 is always CRT1 */
	  sis300DisableHWCursor()
	  sis300SetCursorPositionY(2000, 0)
       } else {
	  /* Head 1 is always CRT2 */
	  sis301DisableHWCursor()
	  sis301SetCursorPositionY(2000, 0)
       }
    } else {
#endif
       sis300DisableHWCursor()
       sis300SetCursorPositionY(2000, 0)
       if(pSiS->VBFlags & CRT2_ENABLE)  {
          sis301DisableHWCursor()
	  sis301SetCursorPositionY(2000, 0)
       }
#ifdef SISDUALHEAD
    }
#endif
}

static void
SiS310HideCursor(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);

    pSiS->HWCursorIsVisible = FALSE;

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode && (!pSiS->ForceCursorOff)) {
       if(pSiS->SecondHead) {
	  /* Head 2 is always CRT1 */
   	  sis310DisableHWCursor()
  	  sis310SetCursorPositionY(2000, 0)
       } else {
	  /* Head 1 is always CRT2 */
	  sis301DisableHWCursor310()
	  sis301SetCursorPositionY310(2000, 0)
       }
    } else {
#endif
       sis310DisableHWCursor()
       sis310SetCursorPositionY(2000, 0)
       if(pSiS->VBFlags & VB_VIDEOBRIDGE) {
	  sis301DisableHWCursor310()
	  sis301SetCursorPositionY310(2000, 0)
       }
#ifdef SISDUALHEAD
    }
#endif
}

static void
SiSShowCursor(ScrnInfoPtr pScrn)
{
    SISPtr        pSiS = SISPTR(pScrn);
    unsigned char sridx, cridx;

    /* Backup current indices of SR and CR since we run async:ly
     * and might be interrupting an on-going register read/write
     */
    sridx = inSISREG(SISSR); cridx = inSISREG(SISCR);

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    orSISIDXREG(SISSR, 0x06, 0x40);

    outSISREG(SISSR, sridx); outSISREG(SISCR, cridx);
}

static void
SiS300ShowCursor(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
       if(pSiS->SecondHead) {
	  /* Head 2 is always CRT1 */
	  if(pSiS->UseHWARGBCursor) {
#ifdef SIS300_USE_ARGB16
	     sis300EnableHWARGB16Cursor()
#else
	     sis300EnableHWARGBCursor()
#endif
	  } else {
	     sis300EnableHWCursor()
	  }
       } else {
	  /* Head 1 is always CRT2 */
	  if(pSiS->UseHWARGBCursor) {
#ifdef SIS300_USE_ARGB16
	     sis301EnableHWARGB16Cursor()
#else
	     sis301EnableHWARGBCursor()
#endif
	  } else {
	     sis301EnableHWCursor()
	  }
       }
    } else {
#endif
       if(pSiS->UseHWARGBCursor) {
#ifdef SIS300_USE_ARGB16
	  sis300EnableHWARGB16Cursor()
#else
	  sis300EnableHWARGBCursor()
#endif
    	  if(pSiS->VBFlags & CRT2_ENABLE)  {
#ifdef SIS300_USE_ARGB16
             sis301EnableHWARGB16Cursor()
#else
	     sis301EnableHWARGBCursor()
#endif
 	  }
       } else {
    	  sis300EnableHWCursor()
    	  if(pSiS->VBFlags & CRT2_ENABLE)  {
             sis301EnableHWCursor()
 	  }
       }
#ifdef SISDUALHEAD
    }
#endif
}

static void
SiS310ShowCursor(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);

    if(pSiS->HideHWCursor) {
       SiS310HideCursor(pScrn);
       pSiS->HWCursorIsVisible = TRUE;
       return;
    }

    pSiS->HWCursorIsVisible = TRUE;

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
       if(pSiS->SecondHead) {
	  /* Head 2 is always CRT1 */
	  if(pSiS->UseHWARGBCursor) {
	     sis310EnableHWARGBCursor()
	  } else {
	     sis310EnableHWCursor()
	  }
       } else {
	  /* Head 1 is always CRT2 */
	  if(pSiS->ChipFlags & SiSCF_CRT2HWCKaputt) {
	     sis301EnableHWCursor330()
	  } else {
	     if(pSiS->UseHWARGBCursor) {
	        sis301EnableHWARGBCursor310()
	     } else {
	        sis301EnableHWCursor310()
	     }
	  }
       }
    } else {
#endif
       if(pSiS->ChipFlags & SiSCF_CRT2HWCKaputt) {
	  if(pSiS->UseHWARGBCursor) {
	     sis310EnableHWARGBCursor()
	  } else {
	     sis310EnableHWCursor()
	  }
	  if(pSiS->VBFlags & CRT2_ENABLE) {
	     sis301EnableHWCursor330()
	  }
       } else {
          if(pSiS->UseHWARGBCursor) {
    	     sis310EnableHWARGBCursor()
    	     if(pSiS->VBFlags & CRT2_ENABLE)  {
	        sis301EnableHWARGBCursor310()
	     }
      	  } else {
	     sis310EnableHWCursor()
    	     if(pSiS->VBFlags & CRT2_ENABLE) {
	        sis301EnableHWCursor310()
	     }
      	  }
       }
#ifdef SISDUALHEAD
    }
#endif
}

static void
SiSSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    SISPtr         pSiS     = SISPTR(pScrn);
    DisplayModePtr mode = pSiS->CurrentLayout.mode;
    unsigned char  x_preset = 0;
    unsigned char  y_preset = 0;
    int            temp;
    unsigned char  sridx, cridx;

    sridx = inSISREG(SISSR); cridx = inSISREG(SISCR);

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    if(x < 0) {
       x_preset = (-x);
       x = 0;
    }

    if(y < 0) {
       y_preset = (-y);
       y = 0;
    }

    if(mode->Flags & V_INTERLACE)     y /= 2;
    else if(mode->Flags & V_DBLSCAN)  y *= 2;

    outSISIDXREG(SISSR, 0x1A, x & 0xff);
    outSISIDXREG(SISSR, 0x1B, (x & 0xff00) >> 8);
    outSISIDXREG(SISSR, 0x1D, y & 0xff);

    inSISIDXREG(SISSR, 0x1E, temp);
    temp &= 0xF8;
    outSISIDXREG(SISSR, 0x1E, temp | ((y >> 8) & 0x07));

    outSISIDXREG(SISSR, 0x1C, x_preset);
    outSISIDXREG(SISSR, 0x1F, y_preset);

    outSISREG(SISSR, sridx); outSISREG(SISCR, cridx);
}

#ifdef SISMERGED
static void
SiSSetCursorPositionMerged(ScrnInfoPtr pScrn1, int x, int y)
{
    SISPtr         pSiS = SISPTR(pScrn1);
    ScrnInfoPtr    pScrn2 = pSiS->CRT2pScrn;
    DisplayModePtr mode1 = CDMPTR->CRT1;
    DisplayModePtr mode2 = CDMPTR->CRT2;
    unsigned short x1_preset = 0, x2_preset = 0;
    unsigned short y1_preset = 0, y2_preset = 0;
    unsigned short maxpreset;
    int            x1, y1, x2, y2;

    x += pScrn1->frameX0;
    y += pScrn1->frameY0;

    x1 = x - pSiS->CRT1frameX0;
    y1 = y - pSiS->CRT1frameY0;

    x2 = x - pScrn2->frameX0;
    y2 = y - pScrn2->frameY0;

    maxpreset = 63;
    if((pSiS->VGAEngine == SIS_300_VGA) && (pSiS->UseHWARGBCursor)) maxpreset = 31;

    if(x1 < 0) {
       x1_preset = (-x1);
       if(x1_preset > maxpreset) x1_preset = maxpreset;
       x1 = 0;
    }
    if(y1 < 0) {
       y1_preset = (-y1);
       if(y1_preset > maxpreset) y1_preset = maxpreset;
       y1 = 0;
    }
    if(x2 < 0) {
       x2_preset = (-x2);
       if(x2_preset > maxpreset) x2_preset = maxpreset;
       x2 = 0;
    }
    if(y2 < 0) {
       y2_preset = (-y2);
       if(y2_preset > maxpreset) y2_preset = maxpreset;
       y2 = 0;
    }

    if(mode1->Flags & V_INTERLACE)     { y1 /= 2; y1_preset /= 2; }
    else if(mode1->Flags & V_DBLSCAN)  { y1 *= 2; y1_preset *= 2; }

    if(mode2->Flags & V_INTERLACE)     { y2 /= 2; y2_preset /= 2; }
    else if(mode2->Flags & V_DBLSCAN)  { y2 *= 2; y2_preset *= 2; }

    /* Work around bug in Cursor engine */
    if(x1 > mode1->HDisplay) { y1 = 2000; y1_preset = 0; }
    if(x2 > mode2->HDisplay) { y2 = 2000; y2_preset = 0; }

    if(pSiS->VGAEngine == SIS_300_VGA) {
       sis300SetCursorPositionX(x1, x1_preset)
       sis300SetCursorPositionY(y1, y1_preset)
       sis301SetCursorPositionX(x2 + 13, x2_preset)
       sis301SetCursorPositionY(y2, y2_preset)
    } else {
       sis310SetCursorPositionX(x1, x1_preset)
       sis310SetCursorPositionY(y1, y1_preset)
       sis301SetCursorPositionX310(x2 + 17, x2_preset)
       sis301SetCursorPositionY310(y2, y2_preset)
    }
}
#endif

static void
SiS300SetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    SISPtr         pSiS = SISPTR(pScrn);
    DisplayModePtr mode = pSiS->CurrentLayout.mode; /* pScrn->currentMode; */
    unsigned short x_preset = 0;
    unsigned short y_preset = 0;

#ifdef SISMERGED
    if(pSiS->MergedFB) {
       SiSSetCursorPositionMerged(pScrn, x, y);
       return;
    }
#endif

    if(x < 0) {
       x_preset = (-x);
       x = 0;
    }
    if(y < 0) {
       y_preset = (-y);
       y = 0;
    }

    if(mode->Flags & V_INTERLACE)     y /= 2;
    else if(mode->Flags & V_DBLSCAN)  y *= 2;

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
       if(pSiS->SecondHead) {
	  /* Head 2 is always CRT1 */
	  sis300SetCursorPositionX(x, x_preset)
    	  sis300SetCursorPositionY(y, y_preset)
       } else {
	  /* Head 1 is always CRT2 */
	  sis301SetCursorPositionX(x + 13, x_preset)
      	  sis301SetCursorPositionY(y, y_preset)
       }
    } else {
#endif
       sis300SetCursorPositionX(x, x_preset)
       sis300SetCursorPositionY(y, y_preset)
       if(pSiS->VBFlags & CRT2_ENABLE) {
      	  sis301SetCursorPositionX(x + 13, x_preset)
      	  sis301SetCursorPositionY(y, y_preset)
       }
#ifdef SISDUALHEAD
    }
#endif
}

static void
SiS310SetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    SISPtr         pSiS = SISPTR(pScrn);
    DisplayModePtr mode = pSiS->CurrentLayout.mode;
    unsigned short x_preset = 0;
    unsigned short y_preset = 0;

#ifdef SISMERGED
    if(pSiS->MergedFB) {
       SiSSetCursorPositionMerged(pScrn, x, y);
       return;
    }
#endif

    if(x < 0) {
       x_preset = (-x);
       x = 0;
    }
    if(y < 0) {
       y_preset = (-y);
       y = 0;
    }

    if(mode->Flags & V_INTERLACE)     y /= 2;
    else if(mode->Flags & V_DBLSCAN)  y *= 2;

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
       if(pSiS->SecondHead) {
	  /* Head 2 is always CRT1 */
	  sis310SetCursorPositionX(x, x_preset)
    	  sis310SetCursorPositionY(y, y_preset)
       } else {
	  /* Head 1 is always CRT2 */
	  sis301SetCursorPositionX310(x + 17, x_preset)
      	  sis301SetCursorPositionY310(y, y_preset)
       }
    } else {
#endif
    	sis310SetCursorPositionX(x, x_preset)
    	sis310SetCursorPositionY(y, y_preset)
    	if(pSiS->VBFlags & CRT2_ENABLE) {
	   sis301SetCursorPositionX310(x + 17, x_preset)
      	   sis301SetCursorPositionY310(y, y_preset)
    	}
#ifdef SISDUALHEAD
    }
#endif
}


static void
SiSSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    SISPtr        pSiS = SISPTR(pScrn);
    unsigned char f_red, f_green, f_blue;
    unsigned char b_red, b_green, b_blue;
    unsigned char sridx, cridx;

    sridx = inSISREG(SISSR); cridx = inSISREG(SISCR);

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    f_red   = (fg & 0x00FF0000) >> (16+2);
    f_green = (fg & 0x0000FF00) >> (8+2);
    f_blue  = (fg & 0x000000FF) >> 2;
    b_red   = (bg & 0x00FF0000) >> (16+2);
    b_green = (bg & 0x0000FF00) >> (8+2);
    b_blue  = (bg & 0x000000FF) >> 2;

    outSISIDXREG(SISSR, 0x14, b_red);
    outSISIDXREG(SISSR, 0x15, b_green);
    outSISIDXREG(SISSR, 0x16, b_blue);
    outSISIDXREG(SISSR, 0x17, f_red);
    outSISIDXREG(SISSR, 0x18, f_green);
    outSISIDXREG(SISSR, 0x19, f_blue);

    outSISREG(SISSR, sridx); outSISREG(SISCR, cridx);
}

static void
SiS300SetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    SISPtr pSiS = SISPTR(pScrn);

    if(pSiS->UseHWARGBCursor) return;

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
       if(pSiS->SecondHead) {
	  /* Head 2 is always CRT1 */
    	  sis300SetCursorBGColor(bg)
    	  sis300SetCursorFGColor(fg)
       } else {
	  /* Head 1 is always CRT2 */
          sis301SetCursorBGColor(bg)
          sis301SetCursorFGColor(fg)
       }
    } else {
#endif
       sis300SetCursorBGColor(bg)
       sis300SetCursorFGColor(fg)
       if(pSiS->VBFlags & CRT2_ENABLE)  {
          sis301SetCursorBGColor(bg)
          sis301SetCursorFGColor(fg)
       }
#ifdef SISDUALHEAD
    }
#endif
}

static void
SiS310SetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    SISPtr pSiS = SISPTR(pScrn);

    if(pSiS->UseHWARGBCursor) return;

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
	if(pSiS->SecondHead) {
	   /* Head 2 is always CRT1 */
    	   sis310SetCursorBGColor(bg)
    	   sis310SetCursorFGColor(fg)
        } else {
	   /* Head 1 is always CRT2 */
	   if(pSiS->ChipFlags & SiSCF_CRT2HWCKaputt) {
	      if((fg != pSiS->CurFGCol) || (bg != pSiS->CurBGCol)) {
      	         pSiS->CurFGCol = fg;
    	         pSiS->CurBGCol = bg;
                 SiSXConvertMono2ARGB(pSiS);
	      }
	   } else {
	      sis301SetCursorBGColor310(bg)
              sis301SetCursorFGColor310(fg)
	   }
       }
    } else {
#endif
       sis310SetCursorBGColor(bg)
       sis310SetCursorFGColor(fg)

       if(pSiS->VBFlags & CRT2_ENABLE)  {
	  if(pSiS->ChipFlags & SiSCF_CRT2HWCKaputt) {
	     if((fg != pSiS->CurFGCol) || (bg != pSiS->CurBGCol)) {
	        pSiS->CurFGCol = fg;
                pSiS->CurBGCol = bg;
	        SiSXConvertMono2ARGB(pSiS);
	     }
	  } else {
	     sis301SetCursorBGColor310(bg)
             sis301SetCursorFGColor310(fg)
	  }
       }
#ifdef SISDUALHEAD
    }
#endif
}

static void
SiSLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
    SISPtr         pSiS = SISPTR(pScrn);
    DisplayModePtr mode = pSiS->CurrentLayout.mode;
    int            cursor_addr;
    unsigned char  temp;
    unsigned char  sridx, cridx;

    sridx = inSISREG(SISSR); cridx = inSISREG(SISCR);

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    cursor_addr = pScrn->videoRam - 1;
    if(mode->Flags & V_DBLSCAN) {
      int i;
      for(i = 0; i < 32; i++) {
         memcpy((unsigned char *)pSiS->FbBase + (cursor_addr * 1024) + (32 * i),
	            src + (16 * i), 16);
         memcpy((unsigned char *)pSiS->FbBase + (cursor_addr * 1024) + (32 * i) + 16,
	 	    src + (16 * i), 16);
      }
    } else {
      memcpy((unsigned char *)pSiS->FbBase + (cursor_addr * 1024), src, 1024);
    }

    /* copy bits [21:18] into the top bits of SR38 */
    inSISIDXREG(SISSR, 0x38, temp);
    temp &= 0x0F;
    outSISIDXREG(SISSR, 0x38, temp | ((cursor_addr & 0xF00) >> 4));

    if(pSiS->Chipset == PCI_CHIP_SIS530) {
       /* store the bit [22] to SR3E */
       if(cursor_addr & 0x1000) {
          orSISIDXREG(SISSR, 0x3E, 0x04);
       } else {
          andSISIDXREG(SISSR, 0x3E, ~0x04);
       }
    }

    /* set HW cursor pattern, use pattern 0xF */
    orSISIDXREG(SISSR, 0x1E, 0xF0);

    /* disable the hardware cursor side pattern */
    andSISIDXREG(SISSR, 0x1E, 0xF7);

    outSISREG(SISSR, sridx); outSISREG(SISCR, cridx);
}

static void
SiS300LoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
    SISPtr pSiS = SISPTR(pScrn);
    int cursor_addr;
    CARD32 status1 = 0, status2 = 0;
    unsigned char *dest = pSiS->FbBase;
    BOOLEAN sizedouble = FALSE;
#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

#ifdef SISMERGED
    if(pSiS->MergedFB) {
       if((CDMPTR->CRT1->Flags & V_DBLSCAN) && (CDMPTR->CRT2->Flags & V_DBLSCAN)) {
          sizedouble = TRUE;
       }
    } else
#endif
           if(pSiS->CurrentLayout.mode->Flags & V_DBLSCAN) {
       sizedouble = TRUE;
    }

    cursor_addr = pScrn->videoRam - pSiS->cursorOffset - (pSiS->CursorSize/1024);  /* 1K boundary */

#ifdef SISDUALHEAD
    /* TW: Use the global (real) FbBase in DHM */
    if(pSiS->DualHeadMode) dest = pSiSEnt->FbBase;
#endif

    if(sizedouble) {
       int i;
       for(i = 0; i < 32; i++) {
          memcpy((unsigned char *)dest + (cursor_addr * 1024) + (32 * i),
	           src + (16 * i), 16);
          memcpy((unsigned char *)dest + (cursor_addr * 1024) + (32 * i) + 16,
	           src + (16 * i), 16);
       }
    } else {
       memcpy((unsigned char *)dest + (cursor_addr * 1024), src, 1024);
    }

    if(pSiS->UseHWARGBCursor) {
       if(pSiS->VBFlags & DISPTYPE_CRT1) {
	  status1 = sis300GetCursorStatus;
	  sis300DisableHWCursor()
	  if(pSiS->VBFlags & CRT2_ENABLE) {
	     status2 = sis301GetCursorStatus;
	     sis301DisableHWCursor()
	  }
	  SISWaitRetraceCRT1(pScrn);
	  sis300SwitchToMONOCursor();
	  if(pSiS->VBFlags & CRT2_ENABLE) {
	     SISWaitRetraceCRT2(pScrn);
	     sis301SwitchToMONOCursor();
	  }
       }
    }
    sis300SetCursorAddress(cursor_addr);
    if(status1) sis300SetCursorStatus(status1)
    
    if(pSiS->VBFlags & CRT2_ENABLE) {  
       if((pSiS->UseHWARGBCursor) && (!pSiS->VBFlags & DISPTYPE_CRT1)) {
	  status2 = sis301GetCursorStatus;
	  sis301DisableHWCursor()
	  SISWaitRetraceCRT2(pScrn);
	  sis301SwitchToMONOCursor();
       }
       sis301SetCursorAddress(cursor_addr)
       if(status2) sis301SetCursorStatus(status2)
    }

    pSiS->UseHWARGBCursor = FALSE;
}

static void
SiS310LoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
    SISPtr pSiS = SISPTR(pScrn);
    unsigned long cursor_addr, cursor_addr2 = 0;
    CARD32 status1 = 0, status2 = 0;
    unsigned char *dest = pSiS->FbBase;
    BOOLEAN sizedouble = FALSE;
    int bufnum;
#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = pSiS->entityPrivate;

    if(pSiS->DualHeadMode) {
       pSiSEnt->HWCursorMBufNum ^= 1;
       bufnum = 1 << pSiSEnt->HWCursorMBufNum;
    } else {
#endif
       pSiS->HWCursorMBufNum ^= 1;
       bufnum = 1 << pSiS->HWCursorMBufNum;
#ifdef SISDUALHEAD
    }
#endif

#ifdef SISMERGED
    if(pSiS->MergedFB) {
       if((CDMPTR->CRT1->Flags & V_DBLSCAN) && (CDMPTR->CRT2->Flags & V_DBLSCAN)) {
          sizedouble = TRUE;
       }
    } else
#endif
           if(pSiS->CurrentLayout.mode->Flags & V_DBLSCAN) {
       sizedouble = TRUE;
    }

#ifdef SISDUALHEAD
    /* Use the global (real) FbBase in DHM */
    if(pSiS->DualHeadMode) dest = pSiSEnt->FbBase;
#endif

    if(pSiS->ChipFlags & SiSCF_CRT2HWCKaputt) {
       cursor_addr = pScrn->videoRam - pSiS->cursorOffset - (pSiS->CursorSize/1024);
    } else {
       cursor_addr = pScrn->videoRam - pSiS->cursorOffset - ((pSiS->CursorSize/1024) * bufnum);
    }

    if(sizedouble) {
       int i;
       for(i = 0; i < 32; i++) {
          memcpy((unsigned char *)dest + (cursor_addr * 1024) + (32 * i),
	           src + (16 * i), 16);
          memcpy((unsigned char *)dest + (cursor_addr * 1024) + (32 * i) + 16,
	           src + (16 * i), 16);
       }
    } else {
       memcpy((unsigned char *)dest + (cursor_addr * 1024), src, 1024);
    }

    if(pSiS->ChipFlags & SiSCF_CRT2HWCKaputt) {

       /* Convert Mono image to color image */

       cursor_addr2 = pScrn->videoRam - pSiS->cursorOffset - ((pSiS->CursorSize/1024) * 2);

       pSiS->CurMonoSrc = (unsigned char *)dest + (cursor_addr * 1024);
       pSiS->CurARGBDest = (CARD32 *)((unsigned char *)dest + (cursor_addr2 * 1024));

       SiSXConvertMono2ARGB(pSiS);

       if(pSiS->UseHWARGBCursor) {
          if(pSiS->VBFlags & DISPTYPE_CRT1) {
	     status1 = sis310GetCursorStatus;
	     sis310DisableHWCursor();
	     SISWaitRetraceCRT1(pScrn);
	     sis310SwitchToMONOCursor();
	  }
       }

    } else {

       if(pSiS->UseHWARGBCursor) {
          if(pSiS->VBFlags & DISPTYPE_CRT1) {
	     status1 = sis310GetCursorStatus;
	     sis310DisableHWCursor()
	     if(pSiS->VBFlags & CRT2_ENABLE) {
	        status2 = sis301GetCursorStatus310;
	        sis301DisableHWCursor310()
	     }
	     SISWaitRetraceCRT1(pScrn);
	     sis310SwitchToMONOCursor();
	     if(pSiS->VBFlags & CRT2_ENABLE)  {
	        SISWaitRetraceCRT2(pScrn);
	        sis301SwitchToMONOCursor310();
	     }
	  }
       } else if(pSiS->Chipset == PCI_CHIP_SIS315H) {
	  if(pSiS->VBFlags & DISPTYPE_CRT1) {
	     SISWaitRetraceCRT1(pScrn);
	  }
       }
    }

    sis310SetCursorAddress(cursor_addr);
    if(status1) sis310SetCursorStatus(status1)

    if(pSiS->VBFlags & CRT2_ENABLE) {
       if(pSiS->ChipFlags & SiSCF_CRT2HWCKaputt) {
          sis301SetCursorAddress310(cursor_addr2)
       } else {
          if((pSiS->UseHWARGBCursor) && (!pSiS->VBFlags & DISPTYPE_CRT1)) {
	     status2 = sis301GetCursorStatus310;
	     sis301DisableHWCursor310()
	     SISWaitRetraceCRT2(pScrn);
	     sis301SwitchToMONOCursor310();
	  }
          sis301SetCursorAddress310(cursor_addr)
	  if(status2) sis301SetCursorStatus310(status2)
       }
    }
    
    pSiS->UseHWARGBCursor = FALSE;
}

static Bool
SiSUseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    SISPtr  pSiS = SISPTR(pScrn);
    DisplayModePtr  mode = pSiS->CurrentLayout.mode;

    if(pSiS->Chipset != PCI_CHIP_SIS6326) return TRUE;
    if(!(pSiS->SiS6326Flags & SIS6326_TVDETECTED)) return TRUE;
    if((strcmp(mode->name, "PAL800x600U") == 0) ||
       (strcmp(mode->name, "NTSC640x480U") == 0))
       return FALSE;
    else
       return TRUE;
}

static Bool
SiS300UseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    SISPtr  pSiS = SISPTR(pScrn);
    DisplayModePtr  mode = pSiS->CurrentLayout.mode;
#ifdef SISMERGED
    DisplayModePtr  mode2 = NULL;

    if(pSiS->MergedFB) {
       mode = CDMPTR->CRT1;
       mode2 = CDMPTR->CRT2;
    }
#endif

    switch (pSiS->Chipset)  {
      case PCI_CHIP_SIS300:
      case PCI_CHIP_SIS630:
      case PCI_CHIP_SIS540:
        if(mode->Flags & V_INTERLACE)
           return FALSE;
	if((mode->Flags & V_DBLSCAN) && (pCurs->bits->height > 32))
	   return FALSE;
#ifdef SISMERGED
        if(pSiS->MergedFB) {
	   if(mode2->Flags & V_INTERLACE)
              return FALSE;
	   if((mode2->Flags & V_DBLSCAN) && (pCurs->bits->height > 32))
	      return FALSE;
	}
#endif
        break;
      case PCI_CHIP_SIS550:
      case PCI_CHIP_SIS650:
      case PCI_CHIP_SIS315:
      case PCI_CHIP_SIS315H:
      case PCI_CHIP_SIS315PRO:
      case PCI_CHIP_SIS660:
      case PCI_CHIP_SIS330:
        if(mode->Flags & V_INTERLACE)
           return FALSE;
	if((mode->Flags & V_DBLSCAN) && (pCurs->bits->height > 32))
	   return FALSE;
#ifdef SISMERGED
        if(pSiS->MergedFB) {
	   if(mode2->Flags & V_INTERLACE)
              return FALSE;
	   if((mode2->Flags & V_DBLSCAN) && (pCurs->bits->height > 32))
	      return FALSE;
	}
#endif
        break;
      default:
        if(mode->Flags & V_INTERLACE)
           return FALSE;
        if((mode->Flags & V_DBLSCAN) && (pCurs->bits->height > 32))
	   return FALSE;
	break;
    }
    return TRUE;
}

#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,2,99,0,0)
#ifdef ARGB_CURSOR
#ifdef SIS_ARGB_CURSOR
static Bool
SiSUseHWCursorARGB(ScreenPtr pScreen, CursorPtr pCurs)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    SISPtr  pSiS = SISPTR(pScrn);
    DisplayModePtr  mode = pSiS->CurrentLayout.mode; 
#ifdef SISMERGED
    DisplayModePtr  mode2 = NULL;

    if(pSiS->MergedFB) {
       mode = CDMPTR->CRT1;
       mode2 = CDMPTR->CRT2;
    }
#endif

    switch (pSiS->Chipset)  {
      case PCI_CHIP_SIS300:
      case PCI_CHIP_SIS630:
      case PCI_CHIP_SIS540:
        if(mode->Flags & V_INTERLACE)
           return FALSE;
	if((pCurs->bits->height > 32) || (pCurs->bits->width > 32))
	   return FALSE;
	if((mode->Flags & V_DBLSCAN) && (pCurs->bits->height > 16))
	   return FALSE;
#ifdef SISMERGED
        if(pSiS->MergedFB) {
	   if(mode2->Flags & V_INTERLACE)
              return FALSE;
	   if((mode2->Flags & V_DBLSCAN) && (pCurs->bits->height > 16))
	      return FALSE;
	}
#endif
        break;
      case PCI_CHIP_SIS550:
      case PCI_CHIP_SIS650:
      case PCI_CHIP_SIS315:
      case PCI_CHIP_SIS315H:
      case PCI_CHIP_SIS315PRO:
      case PCI_CHIP_SIS330:
      case PCI_CHIP_SIS660:
        if(mode->Flags & V_INTERLACE)
           return FALSE;
	if((pCurs->bits->height > 64) || (pCurs->bits->width > 64))
	   return FALSE;
	if((mode->Flags & V_DBLSCAN) && (pCurs->bits->height > 32))
	   return FALSE;
	if((pSiS->CurrentLayout.bitsPerPixel == 8) && (pSiS->VBFlags & CRT2_ENABLE))
	   return FALSE;
#ifdef SISMERGED
        if(pSiS->MergedFB) {
	   if(mode2->Flags & V_INTERLACE)
              return FALSE;
	   if((mode->Flags & V_DBLSCAN) && (pCurs->bits->height > 32))
	      return FALSE;
	}
#endif
        break;
      default:
        return FALSE;
    }
    return TRUE;
}

static void SiS300LoadCursorImageARGB(ScrnInfoPtr pScrn, CursorPtr pCurs)
{
    SISPtr pSiS = SISPTR(pScrn);
    int cursor_addr, i, j, maxheight = 32;
    CARD32 *src = pCurs->bits->argb, *p;
#ifdef SIS300_USE_ARGB16    
    CARD16 *dest, *pb;
    CARD16 temp1;
#define MYSISPTRTYPE CARD16    
#else    
    CARD32 *pb, *dest;
#define MYSISPTRTYPE CARD32    
#endif    
    int srcwidth = pCurs->bits->width;
    int srcheight = pCurs->bits->height;
    CARD32 temp, status1 = 0, status2 = 0;
    BOOLEAN sizedouble = FALSE;
#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

#ifdef SISMERGED
    if(pSiS->MergedFB) {
       if((CDMPTR->CRT1->Flags & V_DBLSCAN) && (CDMPTR->CRT2->Flags & V_DBLSCAN)) {
          sizedouble = TRUE;
       }
    } else
#endif
           if(pSiS->CurrentLayout.mode->Flags & V_DBLSCAN) {
       sizedouble = TRUE;
    }

    cursor_addr = pScrn->videoRam - pSiS->cursorOffset - ((pSiS->CursorSize/1024) * 2);

    if(srcwidth > 32)  srcwidth = 32;
    if(srcheight > 32) srcheight = 32;

#ifdef SISDUALHEAD
    if (pSiS->DualHeadMode)
    	/* TW: Use the global (real) FbBase in DHM */
	dest = (MYSISPTRTYPE *)((unsigned char *)pSiSEnt->FbBase + (cursor_addr * 1024));
    else
#endif
        dest = (MYSISPTRTYPE *)((unsigned char *)pSiS->FbBase + (cursor_addr * 1024));

    if(sizedouble) {
       if(srcheight > 16) srcheight = 16;
       maxheight = 16;
    }

#ifdef SIS300_USE_ARGB16  /* Use 16 Bit RGB pointer */
    for(i = 0; i < srcheight; i++) {
	    p = src;
	    pb = dest;
	    src += pCurs->bits->width;
	    for(j = 0; j < srcwidth; j++) {
	       temp = *p++;
	       if(temp & 0xffffff) {
	          temp1 = ((temp & 0xff) >> 3) |
	                  ((((temp & 0xff00) >> (8 + 3)) << 5) & 0x03e0) |
		          ((((temp & 0xff0000) >> (16 + 3)) << 10) & 0x7c00);
	       } else temp1 = 0x8000;
	       *dest++ = temp1;
	    }
	    if(srcwidth < 32) {
	      for(; j < 32; j++) {
	        *dest++ = 0x8000; 
	      }
	    }
    }	    
    if(srcheight < maxheight) {
	for(; i < maxheight; i++)
	   for(j = 0; j < 32; j++) {
	      *dest++ = 0x8000;
	   }
	   if(sizedouble) {
	      for(j = 0; j < 32; j++) 
	      	*dest++ = 0x0000;
	   }
    }	    
#else    /* Use 32bit RGB pointer - preferred, saves us from the conversion */	       
    for(i = 0; i < srcheight; i++) {
	    p = src;
	    pb = dest;
	    src += pCurs->bits->width;
	    for(j = 0; j < srcwidth; j++) {
	       temp = *p++;
/*	       *dest1++ = ((temp ^ 0xff000000) << 4) | (((temp ^ 0xff000000) & 0xf0000000) >> 28);  */
	       if(pSiS->OptUseColorCursorBlend) {
	          if(temp & 0xffffff) {
	             if((temp & 0xff000000) > pSiS->OptColorCursorBlendThreshold) {
		        temp &= 0x00ffffff;
		     } else {
		  	temp = 0xff111111;
		     }
	          } else temp = 0xff000000;
	       } else {
	           if(temp & 0xffffff) temp &= 0x00ffffff;
	           else temp = 0xff000000;
	       }
	       *dest++ = temp;
	    }
	    if(srcwidth < 32) {
	       for(; j < 32; j++) {
	          *dest++ = 0xff000000;
	       }
	    }
	    if(sizedouble) {
	       for(j = 0; j < 32; j++) {
	          *dest++ = *pb++;
	       }
	    }
   
    }
    if(srcheight < maxheight) {
	for(; i < maxheight; i++) {
	   for(j = 0; j < 32; j++) {
	      *dest++ = 0xff000000;
	   }
	   if(sizedouble) {
	      for(j = 0; j < 32; j++) {
	      	*dest++ = 0xff000000;
	      }
	   }
	}
    }
#endif

    if(!pSiS->UseHWARGBCursor) {
       if(pSiS->VBFlags & DISPTYPE_CRT1) {
	  status1 = sis300GetCursorStatus;
	  sis300DisableHWCursor()
	  if(pSiS->VBFlags & CRT2_ENABLE)  {
	     status2 = sis301GetCursorStatus;
	     sis301DisableHWCursor()
	  }
	  SISWaitRetraceCRT1(pScrn);
	  sis300SwitchToRGBCursor();
	  if(pSiS->VBFlags & CRT2_ENABLE)  {
	     SISWaitRetraceCRT2(pScrn);
	     sis301SwitchToRGBCursor();
	  }
       }
    }

    sis300SetCursorAddress(cursor_addr);
    if(status1) sis300SetCursorStatus(status1)
    
    if(pSiS->VBFlags & CRT2_ENABLE) {
       if((!pSiS->UseHWARGBCursor) && (!pSiS->VBFlags & DISPTYPE_CRT1)) {
	  status2 = sis301GetCursorStatus;
	  sis301DisableHWCursor()
	  SISWaitRetraceCRT2(pScrn);
	  sis301SwitchToRGBCursor();
       }
       sis301SetCursorAddress(cursor_addr)
       if(status2) sis301SetCursorStatus(status2)
    }

    pSiS->UseHWARGBCursor = TRUE;
}

static void SiS310LoadCursorImageARGB(ScrnInfoPtr pScrn, CursorPtr pCurs)
{
    SISPtr pSiS = SISPTR(pScrn);
    int cursor_addr, i, j, maxheight = 64;
    CARD32 *src = pCurs->bits->argb, *p, *pb, *dest;
    int srcwidth = pCurs->bits->width;
    int srcheight = pCurs->bits->height;
    CARD32 status1 = 0, status2 = 0;
    BOOLEAN sizedouble = FALSE;
    int bufnum;
#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

#ifdef SISMERGED
    if(pSiS->MergedFB) {
       if((CDMPTR->CRT1->Flags & V_DBLSCAN) && (CDMPTR->CRT2->Flags & V_DBLSCAN)) {
          sizedouble = TRUE;
       }
    } else
#endif
           if(pSiS->CurrentLayout.mode->Flags & V_DBLSCAN) {
       sizedouble = TRUE;
    }

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
       pSiSEnt->HWCursorCBufNum ^= 1;
       bufnum = 1 << pSiSEnt->HWCursorCBufNum;
    } else {
#endif
       pSiS->HWCursorCBufNum ^= 1;
       bufnum = 1 << pSiS->HWCursorCBufNum;
#ifdef SISDUALHEAD
    }
#endif

    if(pSiS->ChipFlags & SiSCF_CRT2HWCKaputt) {
       cursor_addr = pScrn->videoRam - pSiS->cursorOffset - ((pSiS->CursorSize/1024) * 2);
    } else {
       cursor_addr = pScrn->videoRam - pSiS->cursorOffset - ((pSiS->CursorSize/1024) * (2 + bufnum));
    }

    if(srcwidth > 64)  srcwidth = 64;
    if(srcheight > 64) srcheight = 64;

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode)
       /* Use the global (real) FbBase in DHM */
       dest = (CARD32 *)((unsigned char *)pSiSEnt->FbBase + (cursor_addr * 1024));
    else
#endif
       dest = (CARD32 *)((unsigned char *)pSiS->FbBase + (cursor_addr * 1024));

    if(sizedouble) {
       if(srcheight > 32) srcheight = 32;
       maxheight = 32;
    }

    for(i = 0; i < srcheight; i++) {
       p = src;
       pb = dest;
       src += pCurs->bits->width;
       for(j = 0; j < srcwidth; j++)  *dest++ = *p++;
       if(srcwidth < 64) {
          for(; j < 64; j++) *dest++ = 0;
       }
       if(sizedouble) {
          for(j = 0; j < 64; j++) {
             *dest++ = *pb++;
          }
       }
    }
    if(srcheight < maxheight) {
       for(; i < maxheight; i++) {
	  for(j = 0; j < 64; j++) *dest++ = 0;
	  if(sizedouble) {
	     for(j = 0; j < 64; j++) *dest++ = 0;
	  }
       }
    }

    if(pSiS->ChipFlags & SiSCF_CRT2HWCKaputt) {

       if(!pSiS->UseHWARGBCursor) {
          if(pSiS->VBFlags & DISPTYPE_CRT1) {
	     status1 = sis310GetCursorStatus;
	     sis310DisableHWCursor()
	  }
	  SISWaitRetraceCRT1(pScrn);
	  sis310SwitchToRGBCursor();
       }

    } else {

       if(!pSiS->UseHWARGBCursor) {
          if(pSiS->VBFlags & DISPTYPE_CRT1) {
	     status1 = sis310GetCursorStatus;
	     sis310DisableHWCursor()
	     if(pSiS->VBFlags & CRT2_ENABLE) {
	        status2 = sis301GetCursorStatus310;
	        sis301DisableHWCursor310()
	     }
	  }
	  SISWaitRetraceCRT1(pScrn);
	  sis310SwitchToRGBCursor();
	  if(pSiS->VBFlags & CRT2_ENABLE)  {
	     SISWaitRetraceCRT2(pScrn);
	     sis301SwitchToRGBCursor310();
	  }
       }
    }

    sis310SetCursorAddress(cursor_addr);
    if(status1) sis310SetCursorStatus(status1)
    
    if(pSiS->VBFlags & CRT2_ENABLE) {
       if(pSiS->ChipFlags & SiSCF_CRT2HWCKaputt) {
          sis301SetCursorAddress310(cursor_addr)
       } else {
          if((!pSiS->UseHWARGBCursor) && (!pSiS->VBFlags & DISPTYPE_CRT1)) {
	     status2 = sis301GetCursorStatus310;
	     sis301DisableHWCursor310()
	     SISWaitRetraceCRT2(pScrn);
	     sis301SwitchToRGBCursor310();
	  }
          sis301SetCursorAddress310(cursor_addr)
	  if(status2) sis301SetCursorStatus310(status2)
       }
    }
    
    pSiS->UseHWARGBCursor = TRUE;
}
#endif
#endif
#endif

Bool
SiSHWCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    SISPtr pSiS = SISPTR(pScrn);
    xf86CursorInfoPtr infoPtr;

    infoPtr = xf86CreateCursorInfoRec();
    if(!infoPtr) return FALSE;

    pSiS->CursorInfoPtr = infoPtr;
    pSiS->UseHWARGBCursor = FALSE;

    switch (pSiS->Chipset)  {
    
      case PCI_CHIP_SIS300:
      case PCI_CHIP_SIS630:
      case PCI_CHIP_SIS540:
        infoPtr->MaxWidth  = 64;
        infoPtr->MaxHeight = 64;
        infoPtr->ShowCursor = SiS300ShowCursor;
        infoPtr->HideCursor = SiS300HideCursor;
        infoPtr->SetCursorPosition = SiS300SetCursorPosition;
        infoPtr->SetCursorColors = SiS300SetCursorColors;
        infoPtr->LoadCursorImage = SiS300LoadCursorImage;
        infoPtr->UseHWCursor = SiS300UseHWCursor;	
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,2,99,0,0)
#ifdef ARGB_CURSOR
#ifdef SIS_ARGB_CURSOR
        if(pSiS->OptUseColorCursor) {
	   infoPtr->UseHWCursorARGB = SiSUseHWCursorARGB;
	   infoPtr->LoadCursorARGB = SiS300LoadCursorImageARGB;
	}
#endif	
#endif	
#endif
        infoPtr->Flags =
            HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
            HARDWARE_CURSOR_INVERT_MASK |
            HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
            HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
            HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK |
            HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_64;
        break;

      case PCI_CHIP_SIS550:
      case PCI_CHIP_SIS650:
      case PCI_CHIP_SIS315:
      case PCI_CHIP_SIS315H:
      case PCI_CHIP_SIS315PRO:
      case PCI_CHIP_SIS330:
      case PCI_CHIP_SIS660:
        infoPtr->MaxWidth  = 64;
        infoPtr->MaxHeight = 64;
        infoPtr->ShowCursor = SiS310ShowCursor;
        infoPtr->HideCursor = SiS310HideCursor;
        infoPtr->SetCursorPosition = SiS310SetCursorPosition;
        infoPtr->SetCursorColors = SiS310SetCursorColors;
        infoPtr->LoadCursorImage = SiS310LoadCursorImage;
        infoPtr->UseHWCursor = SiS300UseHWCursor;
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,2,99,0,0)
#ifdef ARGB_CURSOR
#ifdef SIS_ARGB_CURSOR
  	if(pSiS->OptUseColorCursor) {
	   infoPtr->UseHWCursorARGB = SiSUseHWCursorARGB;
	   infoPtr->LoadCursorARGB = SiS310LoadCursorImageARGB;
	}
#endif
#endif	
#endif
        infoPtr->Flags =
            HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
            HARDWARE_CURSOR_INVERT_MASK |
            HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
            HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
            HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK |
            HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_64;
        break;

      default:
        infoPtr->MaxWidth  = 64;
	infoPtr->MaxHeight = 64;
        infoPtr->SetCursorPosition = SiSSetCursorPosition;
        infoPtr->ShowCursor = SiSShowCursor;
        infoPtr->HideCursor = SiSHideCursor;
        infoPtr->SetCursorColors = SiSSetCursorColors;
        infoPtr->LoadCursorImage = SiSLoadCursorImage;
        infoPtr->UseHWCursor = SiSUseHWCursor;
        infoPtr->Flags =
            HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
            HARDWARE_CURSOR_INVERT_MASK |
            HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
            HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
            HARDWARE_CURSOR_NIBBLE_SWAPPED |
            HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_1;
        break;
    }

    return(xf86InitCursor(pScreen, infoPtr));
}
