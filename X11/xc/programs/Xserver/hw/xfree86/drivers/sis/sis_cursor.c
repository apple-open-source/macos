/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_cursor.c,v 1.10 2003/01/30 21:43:33 tsi Exp $ */
/*
 * Copyright 1998,1999 by Alan Hourihane, Wigan, England.
 * Parts Copyright 2001, 2002 by Thomas Winischhofer, Vienna, Austria.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Alan Hourihane, alanh@fairlite.demon.co.uk
 *           Mike Chapman <mike@paranoia.com>,
 *           Juanjo Santamarta <santamarta@ctv.es>,
 *           Mitani Hiroshi <hmitani@drl.mei.co.jp>
 *           David Thomas <davtom@dream.org.uk>.
 *	     Thomas Winischhofer <thomas@winischhofer.net>:
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
#endif

extern void    SISWaitRetraceCRT1(ScrnInfoPtr pScrn);
extern void    SISWaitRetraceCRT2(ScrnInfoPtr pScrn);

static void
SiSShowCursor(ScrnInfoPtr pScrn)
{
    SISPtr        pSiS = SISPTR(pScrn);
    unsigned char sridx, cridx;

    /* TW: Backup current indices of SR and CR since we run async:ly
     *     and might be interrupting an on-going register read/write
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
		/* TW: Head 2 is always CRT1 */
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
		/* TW: Head 1 is always CRT2 */
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

/* TW: 310/325 series */
static void
SiS310ShowCursor(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
	if(pSiS->SecondHead) {
		/* TW: Head 2 is always CRT1 */
		if(pSiS->UseHWARGBCursor) {
		   sis310EnableHWARGBCursor()
		} else {
		   sis310EnableHWCursor()
		}
        } else {
		/* TW: Head 1 is always CRT2 */
		if(pSiS->UseHWARGBCursor) {
		   sis301EnableHWARGBCursor310()
		} else {
		   sis301EnableHWCursor310()
		}
	}
    } else {
#endif
        if(pSiS->UseHWARGBCursor) {
    	   sis310EnableHWARGBCursor()
    	   if(pSiS->VBFlags & CRT2_ENABLE)  {
        	sis301EnableHWARGBCursor310()
      	   }
	} else {
	   sis310EnableHWCursor()
    	   if(pSiS->VBFlags & CRT2_ENABLE)  {
        	sis301EnableHWCursor310()
      	   }
	}
#ifdef SISDUALHEAD
    }
#endif
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
		/* TW: Head 2 is always CRT1 */
		sis300DisableHWCursor()
		sis300SetCursorPositionY(2000, 0)
        } else {
		/* TW: Head 1 is always CRT2 */
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

/* TW: 310/325 series */
static void
SiS310HideCursor(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode && (!pSiS->ForceCursorOff)) {
	if(pSiS->SecondHead) {
		/* TW: Head 2 is always CRT1 */
		sis310DisableHWCursor()
		sis310SetCursorPositionY(2000, 0)
        } else {
		/* TW: Head 1 is always CRT2 */
		sis301DisableHWCursor310()
		sis301SetCursorPositionY310(2000, 0)
	}
    } else {
#endif
    	sis310DisableHWCursor()
	sis310SetCursorPositionY(2000, 0)
    	if(pSiS->VBFlags & CRT2_ENABLE) {
        	sis301DisableHWCursor310()
		sis301SetCursorPositionY310(2000, 0)
    	}
#ifdef SISDUALHEAD
    }
#endif
}

static void
SiSSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    SISPtr        pSiS     = SISPTR(pScrn);
    unsigned char x_preset = 0;
    unsigned char y_preset = 0;
    int           temp;
    unsigned char sridx, cridx;

    sridx = inSISREG(SISSR); cridx = inSISREG(SISCR);

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    if (x < 0) {
        x_preset = (-x);
        x = 0;
    }

    if (y < 0) {
        y_preset = (-y);
        y = 0;
    }

    /* are we in interlaced/doublescan mode? */
    if (pScrn->currentMode->Flags & V_INTERLACE)
        y /= 2;
    else if (pScrn->currentMode->Flags & V_DBLSCAN)
        y *= 2;

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

static void
SiS300SetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    SISPtr        pSiS = SISPTR(pScrn);
    unsigned char x_preset = 0;
    unsigned char y_preset = 0;

    if (x < 0) {
        x_preset = (-x);
        x = 0;
    }
    if (y < 0) {
        y_preset = (-y);
        y = 0;
    }

    /* are we in interlaced/doublescan mode? */
    if(pScrn->currentMode->Flags & V_INTERLACE)
        y /= 2;
    else if(pScrn->currentMode->Flags & V_DBLSCAN)
        y *= 2;

#ifdef SISDUALHEAD
    if (pSiS->DualHeadMode) {
	if (pSiS->SecondHead) {
		/* TW: Head 2 is always CRT1 */
		sis300SetCursorPositionX(x, x_preset)
    		sis300SetCursorPositionY(y, y_preset)
        } else {
		/* TW: Head 1 is always CRT2 */
		sis301SetCursorPositionX(x+13, x_preset)
      		sis301SetCursorPositionY(y, y_preset)
	}
    } else {
#endif
    	sis300SetCursorPositionX(x, x_preset)
    	sis300SetCursorPositionY(y, y_preset)
    	if(pSiS->VBFlags & CRT2_ENABLE) {
      		sis301SetCursorPositionX(x+13, x_preset)
      		sis301SetCursorPositionY(y, y_preset)
    	}
#ifdef SISDUALHEAD
    }
#endif
}

static void
SiS310SetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    SISPtr        pSiS = SISPTR(pScrn);
    unsigned char x_preset = 0;
    unsigned char y_preset = 0;

    if (x < 0) {
        x_preset = (-x);
        x = 0;
    }
    if (y < 0) {
        y_preset = (-y);
        y = 0;
    }

    /* are we in interlaced/doublescan mode? */
    if(pScrn->currentMode->Flags & V_INTERLACE)
        y /= 2;
    else if(pScrn->currentMode->Flags & V_DBLSCAN)
        y *= 2;

#ifdef SISDUALHEAD
    if (pSiS->DualHeadMode) {
	if (pSiS->SecondHead) {
		/* TW: Head 2 is always CRT1 */
		sis310SetCursorPositionX(x, x_preset)
    		sis310SetCursorPositionY(y, y_preset)
        } else {
		/* TW: Head 1 is always CRT2 */
		sis301SetCursorPositionX310(x + 17, x_preset)
      		sis301SetCursorPositionY310(y, y_preset)
	}
    } else {
#endif
    	sis310SetCursorPositionX(x, x_preset)
    	sis310SetCursorPositionY(y, y_preset)
    	if (pSiS->VBFlags & CRT2_ENABLE) {
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

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
	if(pSiS->SecondHead) {
		/* TW: Head 2 is always CRT1 */
    		sis300SetCursorBGColor(bg)
    		sis300SetCursorFGColor(fg)
        } else {
		/* TW: Head 1 is always CRT2 */
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

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
	if(pSiS->SecondHead) {
		/* TW: Head 2 is always CRT1 */
    		sis310SetCursorBGColor(bg)
    		sis310SetCursorFGColor(fg)
        } else {
		/* TW: Head 1 is always CRT2 */
        	sis301SetCursorBGColor310(bg)
        	sis301SetCursorFGColor310(fg)
	}
    } else {
#endif
    	sis310SetCursorBGColor(bg)
    	sis310SetCursorFGColor(fg)
    	if(pSiS->VBFlags & CRT2_ENABLE)  {
        	sis301SetCursorBGColor310(bg)
        	sis301SetCursorFGColor310(fg)
    	}
#ifdef SISDUALHEAD
    }
#endif
}

static void
SiSLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
    SISPtr        pSiS = SISPTR(pScrn);
    int           cursor_addr;
    unsigned char temp;
    unsigned char sridx, cridx;

    sridx = inSISREG(SISSR); cridx = inSISREG(SISCR);

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    cursor_addr = pScrn->videoRam - 1;
    if(pSiS->CurrentLayout.mode->Flags & V_DBLSCAN) {
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

#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif
    
    cursor_addr = pScrn->videoRam - pSiS->cursorOffset - (pSiS->CursorSize/1024);  /* 1K boundary */

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
    	/* TW: Use the global (real) FbBase in DHM */
	if(pSiS->CurrentLayout.mode->Flags & V_DBLSCAN) {
           int i;
           for(i = 0; i < 32; i++) {
              memcpy((unsigned char *)pSiSEnt->FbBase + (cursor_addr * 1024) + (32 * i),
	               src + (16 * i), 16);
              memcpy((unsigned char *)pSiSEnt->FbBase + (cursor_addr * 1024) + (32 * i) + 16,
	 	       src + (16 * i), 16);
           }
        } else {
    	   memcpy((unsigned char *)pSiSEnt->FbBase + (cursor_addr * 1024), src, 1024);
	}
    } else 
#endif
      if(pSiS->CurrentLayout.mode->Flags & V_DBLSCAN) {
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
    sis300SetCursorPatternSelect(0);
    if(status1) sis300SetCursorStatus(status1)
    
    if(pSiS->VBFlags & CRT2_ENABLE) {  
    	if((pSiS->UseHWARGBCursor) && (!pSiS->VBFlags & DISPTYPE_CRT1)) {
	    status2 = sis301GetCursorStatus;
	    sis301DisableHWCursor()
	    SISWaitRetraceCRT2(pScrn); 
	    sis301SwitchToMONOCursor();   
	}
        sis301SetCursorAddress(cursor_addr)
        sis301SetCursorPatternSelect(0)
	if(status2) sis301SetCursorStatus(status2)
    }

    pSiS->UseHWARGBCursor = FALSE;
}

static void
SiS310LoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
    SISPtr pSiS = SISPTR(pScrn);
    int cursor_addr;
    CARD32 status1 = 0, status2 = 0;

#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

    cursor_addr = pScrn->videoRam - pSiS->cursorOffset - (pSiS->CursorSize/1024); /* 1K boundary */

#ifdef SISDUALHEAD
    if (pSiS->DualHeadMode) {
    	/* TW: Use the global (real) FbBase in DHM */
	if(pSiS->CurrentLayout.mode->Flags & V_DBLSCAN) {
           int i;
           for(i = 0; i < 32; i++) {
              memcpy((unsigned char *)pSiSEnt->FbBase + (cursor_addr * 1024) + (32 * i),
	               src + (16 * i), 16);
              memcpy((unsigned char *)pSiSEnt->FbBase + (cursor_addr * 1024) + (32 * i) + 16,
	 	       src + (16 * i), 16);
           }
        } else {
    	   memcpy((unsigned char *)pSiSEnt->FbBase + (cursor_addr * 1024), src, 1024);
	}
    } else
#endif
      if(pSiS->CurrentLayout.mode->Flags & V_DBLSCAN) {
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
    
    if(pSiS->UseHWARGBCursor) {
        if(pSiS->VBFlags & DISPTYPE_CRT1) {
	   status1 = sis310GetCursorStatus;
	   sis310DisableHWCursor()
	   if(pSiS->VBFlags & CRT2_ENABLE)  {
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
    }
    sis310SetCursorAddress(cursor_addr);
    sis310SetCursorPatternSelect(0);
    if(status1) sis310SetCursorStatus(status1)
    
    if(pSiS->VBFlags & CRT2_ENABLE)  {  
    	if((pSiS->UseHWARGBCursor) && (!pSiS->VBFlags & DISPTYPE_CRT1)) {
	    status2 = sis301GetCursorStatus310;
	    sis301DisableHWCursor310()
	    SISWaitRetraceCRT2(pScrn); 
	    sis301SwitchToMONOCursor310();   
	}
        sis301SetCursorAddress310(cursor_addr)
        sis301SetCursorPatternSelect310(0)
	if(status2) sis301SetCursorStatus310(status2)
    }
    
    pSiS->UseHWARGBCursor = FALSE;
}

static Bool
SiSUseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    DisplayModePtr  mode = pScrn->currentMode;
    SISPtr  pSiS = SISPTR(pScrn);

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
    DisplayModePtr  mode = pSiS->CurrentLayout.mode; /* pScrn->currentMode; */

    switch (pSiS->Chipset)  {
      case PCI_CHIP_SIS300:
      case PCI_CHIP_SIS630:
      case PCI_CHIP_SIS540:
        if(mode->Flags & V_INTERLACE)
            return FALSE;
	if((mode->Flags & V_DBLSCAN) && (pCurs->bits->height > 32))
	    return FALSE;
        break;
      case PCI_CHIP_SIS550:
      case PCI_CHIP_SIS650:
      case PCI_CHIP_SIS315:
      case PCI_CHIP_SIS315H:
      case PCI_CHIP_SIS315PRO:
      case PCI_CHIP_SIS330:
        if(mode->Flags & V_INTERLACE)
            return FALSE;
	if((mode->Flags & V_DBLSCAN) && (pCurs->bits->height > 32))
	    return FALSE;
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
    DisplayModePtr  mode = pScrn->currentMode;
    SISPtr  pSiS = SISPTR(pScrn);
    	
    switch (pSiS->Chipset)  {
      case PCI_CHIP_SIS300:
      case PCI_CHIP_SIS630:
      case PCI_CHIP_SIS540:
        if(mode->Flags & V_INTERLACE)
            return FALSE;
	if(pCurs->bits->height > 32 || pCurs->bits->width > 32)
	    return FALSE;
	if((mode->Flags & V_DBLSCAN) && (pCurs->bits->height > 16))
	    return FALSE;
        break;
      case PCI_CHIP_SIS550:
      case PCI_CHIP_SIS650:
      case PCI_CHIP_SIS315:
      case PCI_CHIP_SIS315H:
      case PCI_CHIP_SIS315PRO:
      case PCI_CHIP_SIS330:
        if(mode->Flags & V_INTERLACE)
            return FALSE;
	if(pCurs->bits->height > 64 || pCurs->bits->width > 64)
	    return FALSE;
	if((mode->Flags & V_DBLSCAN) && (pCurs->bits->height > 32))
	    return FALSE;
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
    BOOLEAN sizedouble = FALSE;
    CARD32 temp, status1 = 0, status2 = 0;

#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

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

    if(pSiS->CurrentLayout.mode->Flags & V_DBLSCAN) {
        sizedouble = TRUE;
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
    sis300SetCursorPatternSelect(0);
    if(status1) sis300SetCursorStatus(status1)
    
    if(pSiS->VBFlags & CRT2_ENABLE) {  
    	if((!pSiS->UseHWARGBCursor) && (!pSiS->VBFlags & DISPTYPE_CRT1)) {
	    status2 = sis301GetCursorStatus;
	    sis301DisableHWCursor()
	    SISWaitRetraceCRT2(pScrn); 
	    sis301SwitchToRGBCursor();   
	}
        sis301SetCursorAddress(cursor_addr)
        sis301SetCursorPatternSelect(0)
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
    BOOLEAN sizedouble = FALSE;
    CARD32 status1 = 0, status2 = 0;

#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif
    
    cursor_addr = pScrn->videoRam - pSiS->cursorOffset - ((pSiS->CursorSize/1024) * 2);
    
    if(srcwidth > 64)  srcwidth = 64;
    if(srcheight > 64) srcheight = 64;

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) 
    	/* TW: Use the global (real) FbBase in DHM */
	dest = (CARD32 *)((unsigned char *)pSiSEnt->FbBase + (cursor_addr * 1024)); 
    else
#endif
        dest = (CARD32 *)((unsigned char *)pSiS->FbBase + (cursor_addr * 1024)); 

    if(pSiS->CurrentLayout.mode->Flags & V_DBLSCAN) {
        sizedouble = TRUE;
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
	for(; i < maxheight; i++)
	   for(j = 0; j < 64; j++) *dest++ = 0;
	   if(sizedouble) {
	      for(j = 0; j < 64; j++) *dest++ = 0;
	   }
    }

    if(!pSiS->UseHWARGBCursor) {
        if(pSiS->VBFlags & DISPTYPE_CRT1) {
	   status1 = sis310GetCursorStatus;
	   sis310DisableHWCursor()
	   if(pSiS->VBFlags & CRT2_ENABLE)  {
	      status2 = sis301GetCursorStatus310;
	      sis301DisableHWCursor310()
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
    sis310SetCursorPatternSelect(0);
    if(status1) sis310SetCursorStatus(status1)
    
    if(pSiS->VBFlags & CRT2_ENABLE) {  
    	if((!pSiS->UseHWARGBCursor) && (!pSiS->VBFlags & DISPTYPE_CRT1)) {
	    status2 = sis301GetCursorStatus310;
	    sis301DisableHWCursor310()
	    SISWaitRetraceCRT2(pScrn); 
	    sis301SwitchToRGBCursor310();   
	}
        sis301SetCursorAddress310(cursor_addr)
        sis301SetCursorPatternSelect310(0)
	if(status2) sis301SetCursorStatus310(status2)
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

    PDEBUG(ErrorF("HW Cursor Init\n"));
    infoPtr = xf86CreateCursorInfoRec();
    if(!infoPtr) 
        return FALSE;

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
