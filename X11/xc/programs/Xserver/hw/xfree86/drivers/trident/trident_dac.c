/*
 * Copyright 1992-2003 by Alan Hourihane, North Wales, UK.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/trident/trident_dac.c,v 1.80 2004/01/21 22:31:54 alanh Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Version.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "vgaHW.h"

#include "trident.h"
#include "trident_regs.h"


static biosMode bios1[] = { 
    { 640, 480, 0x11 }
};

static biosMode bios4[] = {
    { 320, 200, 0xd },
    { 640, 200, 0xe },
    { 640, 350, 0x11 },
    { 640, 480, 0x12 },
    { 800, 600, 0x5b },
    { 1024, 768 , 0x5f },
    { 1280, 1024, 0x63 },
    { 1600, 1200, 0x65 }
};

static biosMode bios8[] = {	
    { 320, 200, 0x13 },
    { 640, 400, 0x5c },
    { 640, 480, 0x5d },
    { 720, 480, 0x60 },
    { 800, 600, 0x5e },
    { 1024, 768, 0x62 },
    { 1280, 1024, 0x64 },
    { 1600, 1200, 0x66 }
};

static biosMode bios15[] = {
    { 640, 400, 0x72 },
    { 640, 480, 0x74 },
    { 720, 480, 0x70 },
    { 800, 600, 0x76 },
    { 1024, 768, 0x78 },
    { 1280, 1024, 0x7a },
    { 1600, 1200, 0x7c }
};

static biosMode bios16[] = {
    { 640, 400, 0x73 },
    { 640, 480, 0x75 },
    { 720, 480, 0x71 },
    { 800, 600, 0x77 },
    { 1024, 768, 0x79 },
    { 1280, 1024, 0x7b },
    { 1600, 1200, 0x7d }
};

static biosMode bios24[] = {
    { 640, 400, 0x6b },
    { 640, 480, 0x6c },
    { 720, 480, 0x61 },
    { 800, 600, 0x6d },
    { 1024, 768, 0x6e }
};

static newModes newModeRegs [] = {
  { 320, 200, 0x13, 0x30 },
  { 640, 480, 0x13, 0x61 },
  { 800, 600, 0x13, 0x61 },
  { 1024, 768, 0x3b, 0x63 },
  { 1280, 1024, 0x7b, 0x64 },
  { 1400, 1050, 0x11, 0x7b } 
};

int
TridentFindMode(int xres, int yres, int depth)
{
    int xres_s;
    int i, size;
    biosMode *mode;

    switch (depth) {
    case 1:
	size = sizeof(bios1) / sizeof(biosMode);
	mode = bios1;
	break;
    case 4:
	size = sizeof(bios4) / sizeof(biosMode);
	mode = bios4;
	break;
    case 8:
	size = sizeof(bios8) / sizeof(biosMode);
	mode = bios8;
	break;
    case 15:
	size = sizeof(bios15) / sizeof(biosMode);
	mode = bios15;
	break;
    case 16:
	size = sizeof(bios16) / sizeof(biosMode);
	mode = bios16;
	break;
    case 24:
	size = sizeof(bios24) / sizeof(biosMode);
	mode = bios24;
	break;
    default:
	return 0;
    }

    for (i = 0; i < size; i++) {
	if (xres <= mode[i].x_res) {
	    xres_s = mode[i].x_res;
	    for (; i < size; i++) {
		if (mode[i].x_res != xres_s)
		    return mode[i-1].mode;
		if (yres <= mode[i].y_res)
		    return mode[i].mode;
	    }
	}
    }
    return mode[size - 1].mode;
}

static void
TridentFindNewMode(int xres, int yres, CARD8 *gr5a, CARD8 *gr5c)
{
    int xres_s;
    int i, size;
    
    size = sizeof(newModeRegs) / sizeof(newModes);

    for (i = 0; i < size; i++) {
	if (xres <= newModeRegs[i].x_res) {
	    xres_s = newModeRegs[i].x_res;
	    for (; i < size; i++) {
	        if (newModeRegs[i].x_res != xres_s 
		    || yres <= newModeRegs[i].y_res) {
		  *gr5a = newModeRegs[i].GR5a;
		  *gr5c = newModeRegs[i].GR5c;
		  return;
		}
	    }
	}
    }
    *gr5a = newModeRegs[size - 1].GR5a;
    *gr5c = newModeRegs[size - 1].GR5c;
    return;
}

static void
tridentSetBrightnessAndGamma(TRIDENTRegPtr tridentReg,
			     Bool on, double exp,int brightness)
{
    int pivots[] = {0,3,15,63,255};

    double slope;
    double y_0;
    double x, x_prev = 0, y, y_prev = 0;
    int i;
    CARD8 i_slopes[4];
    CARD8 intercepts[4];

    if (!on) {
	tridentReg->tridentRegs3C4[0xB4] &= ~0x80;
	return;
    }

    for (i = 0; i < 4; i++) {
	x = pivots[i + 1] / 255.0;
	y = pow(x,exp);
	slope = (y - y_prev) / (x - x_prev);
	y_0 = y - x * slope;
	{
#define RND(x) ((((x) - (int) (x)) < 0.5) ? (int)(x) : (int)(x) + 1)
	    int val = slope;
	    if (val > 7) 
		i_slopes[i] = (3 << 4) | (RND(slope) & 0xf);
	    else if (val > 3) 
		i_slopes[i] = (2 << 4) | (RND(slope * 2) & 0xf);
	    else if (val > 1) 
		i_slopes[i] = (1 << 4) | (RND(slope * 4) & 0xf);
	    else 
		i_slopes[i] = (RND(slope * 8) & 0xf);
#undef RND
	}
	intercepts[i] = (char)(y_0 * 256 / 4);
	x_prev = x;
	y_prev = y;
    }
    
    tridentReg->tridentRegs3C4[0xB4] = 0x80 | i_slopes[0];
    tridentReg->tridentRegs3C4[0xB5] = i_slopes[1];
    tridentReg->tridentRegs3C4[0xB6] = i_slopes[2];
    tridentReg->tridentRegs3C4[0xB7] = i_slopes[3];
    tridentReg->tridentRegs3C4[0xB8] = (intercepts[0] + brightness);
    tridentReg->tridentRegs3C4[0xB9] = (intercepts[1] + brightness);
    tridentReg->tridentRegs3C4[0xBA] = (intercepts[2] + brightness);
    tridentReg->tridentRegs3C4[0xBB] = (intercepts[3] + brightness);
}

Bool
TridentInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    TRIDENTRegPtr pReg = &pTrident->ModeReg;

    int vgaIOBase;
    int offset = 0;
    int clock = pTrident->currentClock;
    CARD8 protect = 0;
    Bool fullSize = FALSE;

    vgaHWPtr hwp = VGAHWPTR(pScrn);
    vgaRegPtr regp = &hwp->ModeReg;
    vgaRegPtr vgaReg = &hwp->ModeReg;
    vgaIOBase = VGAHWPTR(pScrn)->IOBase;

    /* Unprotect */
    if (pTrident->Chipset > PROVIDIA9685) {
    	OUTB(0x3C4, Protection);
    	protect = INB(0x3C5);
    	OUTB(0x3C5, 0x92);
    }

    OUTB(0x3C4, 0x0B); INB(0x3C5); /* Ensure we are in New Mode */

    pReg->tridentRegs3x4[PixelBusReg] = 0x00;
    pReg->tridentRegsDAC[0x00] = 0x00;
    pReg->tridentRegs3C4[NewMode2] = 0x20;
    OUTB(0x3CE, MiscExtFunc);
    pReg->tridentRegs3CE[MiscExtFunc] = INB(0x3CF) & 0xF0;
    pReg->tridentRegs3x4[GraphEngReg] = 0x00; 
    pReg->tridentRegs3x4[PreEndControl] = 0;
    pReg->tridentRegs3x4[PreEndFetch] = 0;

    pReg->tridentRegs3x4[CRTHiOrd] = (((mode->CrtcVBlankEnd-1) & 0x400)>>4) |
 				     (((mode->CrtcVTotal - 2) & 0x400) >> 3) |
 				     ((mode->CrtcVSyncStart & 0x400) >> 5) |
 				     (((mode->CrtcVDisplay - 1) & 0x400) >> 6)|
 				     0x08;

    pReg->tridentRegs3x4[HorizOverflow] = ((mode->CrtcHTotal & 0x800) >> 11) |
	    				  ((mode->CrtcHBlankStart & 0x800)>>7);

    if (pTrident->IsCyber) {
	Bool LCDActive;
#ifdef READOUT
	Bool ShadowModeActive;
#endif
	int i = pTrident->lcdMode;
#ifdef READOUT
	OUTB(0x3CE, CyberControl);
	ShadowModeActive = ((INB(0x3CF) & 0x81) == 0x81);
#endif
	OUTB(0x3CE, FPConfig);
	pReg->tridentRegs3CE[FPConfig] = INB(0x3CF);
	if (pTrident->dspOverride) {
	    if (pTrident->dspOverride & LCD_ACTIVE) {
		pReg->tridentRegs3CE[FPConfig] |= 0x10;
		    LCDActive = TRUE;
	    } else {
		pReg->tridentRegs3CE[FPConfig] &= ~0x10;
		    LCDActive = FALSE;
	    }
	    if (pTrident->dspOverride & CRT_ACTIVE)
		pReg->tridentRegs3CE[FPConfig] |= 0x20;
	    else
		pReg->tridentRegs3CE[FPConfig] &= ~0x20;
	} else {
	    LCDActive = (pReg->tridentRegs3CE[FPConfig] & 0x10);
	}

	OUTB(0x3CE, CyberEnhance); 
#if 0
	pReg->tridentRegs3CE[CyberEnhance] = INB(0x3CF);
#else
	pReg->tridentRegs3CE[CyberEnhance] = INB(0x3CF) & 0x8F;
 	if (mode->CrtcVDisplay > 1024)
	    pReg->tridentRegs3CE[CyberEnhance] |= 0x50;
	else
	if (mode->CrtcVDisplay > 768)
	    pReg->tridentRegs3CE[CyberEnhance] |= 0x30;
	else
	if (mode->CrtcVDisplay > 600)
	    pReg->tridentRegs3CE[CyberEnhance] |= 0x20;
	else
	if (mode->CrtcVDisplay > 480)
	    pReg->tridentRegs3CE[CyberEnhance] |= 0x10;
#endif
	OUTB(0x3CE, CyberControl);
	pReg->tridentRegs3CE[CyberControl] = INB(0x3CF);

	OUTB(0x3CE,HorStretch);
	pReg->tridentRegs3CE[HorStretch] = INB(0x3CF);
	OUTB(0x3CE,VertStretch);
	pReg->tridentRegs3CE[VertStretch] = INB(0x3CF);

#ifdef READOUT
	if ((!((pReg->tridentRegs3CE[VertStretch] & 1) ||
	       (pReg->tridentRegs3CE[HorStretch] & 1)))
	    && (!LCDActive || ShadowModeActive)) 
	  {
	    unsigned char tmp;
	    
	    SHADOW_ENABLE(tmp);
	    OUTB(vgaIOBase + 4,0);
	    pReg->tridentRegs3x4[0x0] = INB(vgaIOBase + 5);
	    OUTB(vgaIOBase + 4,3);
	    pReg->tridentRegs3x4[0x3] = INB(vgaIOBase + 5);
	    OUTB(vgaIOBase + 4,4);
	    pReg->tridentRegs3x4[0x4] = INB(vgaIOBase + 5);
	    OUTB(vgaIOBase + 4,5);
  	    pReg->tridentRegs3x4[0x5] = INB(vgaIOBase + 5);
  	    OUTB(vgaIOBase + 4,0x6);
  	    pReg->tridentRegs3x4[0x6] = INB(vgaIOBase + 5);
  	    SHADOW_RESTORE(tmp);
 	} else
#endif
 	{
 	    if (i != 0xff) {
  		pReg->tridentRegs3x4[0x0] = LCD[i].shadow_0;
  		pReg->tridentRegs3x4[0x1] = regp->CRTC[1];
  		pReg->tridentRegs3x4[0x2] = regp->CRTC[2];
  		pReg->tridentRegs3x4[0x3] = LCD[i].shadow_3;
  		pReg->tridentRegs3x4[0x4] = LCD[i].shadow_4;
  		pReg->tridentRegs3x4[0x5] = LCD[i].shadow_5;
  		pReg->tridentRegs3x4[0x6] = LCD[i].shadow_6;
 		xf86DrvMsgVerb(pScrn->scrnIndex,X_INFO,1,
 			       "Overriding Horizontal timings.\n");
  	    }
  	}
 
 	if (i != 0xff) {
 	    pReg->tridentRegs3x4[0x7] = LCD[i].shadow_7;
 	    pReg->tridentRegs3x4[0x10] = LCD[i].shadow_10;
 	    pReg->tridentRegs3x4[0x11] = LCD[i].shadow_11;
 	    pReg->tridentRegs3x4[0x12] = regp->CRTC[0x12];
 	    pReg->tridentRegs3x4[0x15] = regp->CRTC[0x15];
 	    pReg->tridentRegs3x4[0x16] = LCD[i].shadow_16;
 	    if (LCDActive) {
 		pReg->tridentRegs3x4[CRTHiOrd] = LCD[i].shadow_HiOrd;
	    }
	    
	    fullSize = (mode->HDisplay == LCD[i].display_x) 
	        && (mode->VDisplay == LCD[i].display_y);
 	}
 	
  	/* copy over common bits from normal VGA */
  	
  	pReg->tridentRegs3x4[0x7] &= ~0x4A;
	pReg->tridentRegs3x4[0x7] |= (vgaReg->CRTC[0x7] & 0x4A);

	if (LCDActive && fullSize) {	
	    regp->CRTC[0] = pReg->tridentRegs3x4[0];
	    regp->CRTC[3] = pReg->tridentRegs3x4[3];
	    regp->CRTC[4] = pReg->tridentRegs3x4[4];
	    regp->CRTC[5] = pReg->tridentRegs3x4[5];
	    regp->CRTC[6] = pReg->tridentRegs3x4[6];
	    regp->CRTC[7] = pReg->tridentRegs3x4[7];
	    regp->CRTC[0x10] = pReg->tridentRegs3x4[0x10];
	    regp->CRTC[0x11] = pReg->tridentRegs3x4[0x11];
	    regp->CRTC[0x16] = pReg->tridentRegs3x4[0x16];
	}
	if (LCDActive && !fullSize) {
	    /*
	     * Set negative h/vsync polarity to center display nicely
	     * Seems to work on several systems.
	     */
	    regp->MiscOutReg |= 0xC0;
	  /* 
	   * If the LCD is active and we don't fill the entire screen
	   * and the previous mode was stretched we may need help from
	   * the BIOS to set all registers for the unstreched mode.
	   */
	    pTrident->doInit =  ((pReg->tridentRegs3CE[HorStretch] & 1)
				|| (pReg->tridentRegs3CE[VertStretch] & 1));
	    pReg->tridentRegs3CE[CyberControl] |= 0x81;
	    xf86DrvMsgVerb(pScrn->scrnIndex,X_INFO,1,"Shadow on\n");
	} else {
	    pReg->tridentRegs3CE[CyberControl] &= 0x7E;
	    xf86DrvMsgVerb(pScrn->scrnIndex,X_INFO,1,"Shadow off\n");
	}
	if (pTrident->FPDelay < 6) {
	    pReg->tridentRegs3CE[CyberControl] &= 0xC7;
	    pReg->tridentRegs3CE[CyberControl] |= (pTrident->FPDelay + 2) << 3;
	}
	
	if (pTrident->CyberShadow) {
	    pReg->tridentRegs3CE[CyberControl] &= 0x7E;
	    xf86DrvMsgVerb(pScrn->scrnIndex,X_INFO,1,"Forcing Shadow off\n");
	}

 	xf86DrvMsgVerb(pScrn->scrnIndex,X_INFO,1,"H-timing shadow registers:"
 		       " 0x%2.2x           0x%2.2x 0x%2.2x 0x%2.2x\n",
 		       pReg->tridentRegs3x4[0], pReg->tridentRegs3x4[3],
 		       pReg->tridentRegs3x4[4], pReg->tridentRegs3x4[5]);
 	xf86DrvMsgVerb(pScrn->scrnIndex,X_INFO,1,"H-timing registers:       "
 		       " 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x\n",
 		       regp->CRTC[0], regp->CRTC[1], regp->CRTC[2],
		       regp->CRTC[3], regp->CRTC[4], regp->CRTC[5]);
 	xf86DrvMsgVerb(pScrn->scrnIndex,X_INFO,1,"V-timing shadow registers: "
 		       "0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x"
		       "           0x%2.2x (0x%2.2x)\n",
		       pReg->tridentRegs3x4[6], pReg->tridentRegs3x4[7],
 		       pReg->tridentRegs3x4[0x10],pReg->tridentRegs3x4[0x11],
 		       pReg->tridentRegs3x4[0x16],
 		       pReg->tridentRegs3x4[CRTHiOrd]);
 	xf86DrvMsgVerb(pScrn->scrnIndex,X_INFO,1,"V-timing registers:        "
 		       "0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x "
		       "0x%2.2x 0x%2.2x 0x%2.2x\n",
 		       regp->CRTC[6], regp->CRTC[7], regp->CRTC[0x10],
		       regp->CRTC[0x11],regp->CRTC[0x12],
 		       regp->CRTC[0x14],regp->CRTC[0x16]);
 	
	
	/* disable stretching, enable centering */
	pReg->tridentRegs3CE[VertStretch] &= 0xFC;
	pReg->tridentRegs3CE[VertStretch] |= 0x80;
	pReg->tridentRegs3CE[HorStretch] &= 0xFC;
	pReg->tridentRegs3CE[HorStretch] |= 0x80;
#if 1
	{
  	    int mul = pScrn->bitsPerPixel >> 3; 
	    int val;
	    
	    if (!mul) mul = 1;
	    
	    /* this is what my BIOS does */ 
	    val = (mode->HDisplay * mul / 8) + 16;

	    pReg->tridentRegs3x4[PreEndControl] = ((val >> 8) < 2 ? 2 :0)
	      | ((val >> 8) & 0x01);
	    pReg->tridentRegs3x4[PreEndFetch] = val & 0xff;
	}
#else
	OUTB(vgaIOBase + 4,PreEndControl);
	pReg->tridentRegs3x4[PreEndControl] = INB(vgaIOBase + 5);
	OUTB(vgaIOBase + 4,PreEndFetch);
	pReg->tridentRegs3x4[PreEndFetch] = INB(vgaIOBase + 5);
#endif
	/* set mode */
	if (pTrident->Chipset < BLADEXP) {
	  pReg->tridentRegs3CE[BiosMode] = TridentFindMode(
					   mode->HDisplay,
					   mode->VDisplay,
					   pScrn->depth);
	  xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 1, 
			 "Setting BIOS Mode: %x for: %ix%i\n",
			 pReg->tridentRegs3CE[BiosMode],
			 mode->HDisplay,
			 mode->VDisplay);
	} else {
	  TridentFindNewMode(mode->HDisplay,
			     mode->VDisplay,
			     &pReg->tridentRegs3CE[BiosNewMode1],
			     &pReg->tridentRegs3CE[BiosNewMode2]);
	  xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 1, 
			 "Setting BIOS Mode Regs: %x %x\n",
			 pReg->tridentRegs3CE[BiosNewMode1],
			 pReg->tridentRegs3CE[BiosNewMode2]);
	};
	
	/* no stretch */
	if (pTrident->Chipset != CYBERBLADEXPAI1)
	    pReg->tridentRegs3CE[BiosReg] = 0;
	else
	    pReg->tridentRegs3CE[BiosReg] = 8;

	if (pTrident->CyberStretch) {
	    pReg->tridentRegs3CE[VertStretch] |= 0x01;
	    pReg->tridentRegs3CE[HorStretch] |= 0x01;
	    xf86DrvMsgVerb(pScrn->scrnIndex,X_INFO,1,"Enabling StretchMode\n");
	}
    }

    /* Enable Chipset specific options */
    switch (pTrident->Chipset) {
	case CYBERBLADEXP4:
	case CYBERBLADEXPAI1:
	case BLADEXP:
	case CYBERBLADEI7:
	case CYBERBLADEI7D:
	case CYBERBLADEI1:
	case CYBERBLADEI1D:
	case CYBERBLADEAI1:
	case CYBERBLADEAI1D:
	case CYBERBLADEE4:
	case BLADE3D:
	    OUTB(vgaIOBase + 4, RAMDACTiming);
	    pReg->tridentRegs3x4[RAMDACTiming] = INB(vgaIOBase + 5) | 0x0F;
	    /* Fall Through */
	case CYBER9520:
	case CYBER9525DVD:
	case CYBER9397DVD:
	case CYBER9397:
	case IMAGE975:
	case IMAGE985:
	case CYBER9388:
	    if (pScrn->bitsPerPixel >= 8)
    	    	pReg->tridentRegs3CE[MiscExtFunc] |= 0x10;
	    else
    	    	pReg->tridentRegs3CE[MiscExtFunc] &= ~0x10;
	    if (!pReg->tridentRegs3x4[PreEndControl])
	    	pReg->tridentRegs3x4[PreEndControl] = 0x01;
	    if (!pReg->tridentRegs3x4[PreEndFetch])
	    	pReg->tridentRegs3x4[PreEndFetch] = 0xFF;
	    /* Fall Through */
	case PROVIDIA9685:
	case CYBER9385:
	    pReg->tridentRegs3x4[Enhancement0] = 0x40;
	    /* Fall Through */
	case PROVIDIA9682:
	case CYBER9382:
	    if (pTrident->UsePCIRetry) 
	    	pReg->tridentRegs3x4[PCIRetry] = 0xDF;
	    else
	    	pReg->tridentRegs3x4[PCIRetry] = 0x1F;
	    /* Fall Through */
	case TGUI9660:
	case TGUI9680:
	    if (pTrident->MUX && pScrn->bitsPerPixel == 8) {
	    	pReg->tridentRegs3x4[PixelBusReg] |= 0x01; /* 16bit bus */
	    	pReg->tridentRegs3C4[NewMode2] |= 0x02; /* half clock */
    		pReg->tridentRegsDAC[0x00] |= 0x20;	/* mux mode */
	    }	
    }

    /* Defaults for all trident chipsets follows */
    switch (pScrn->bitsPerPixel) {
	case 1:
	case 4:
    	    offset = pScrn->displayWidth >> 4;
	    break;
	case 8:
	    pReg->tridentRegs3CE[MiscExtFunc] |= 0x02;
    	    offset = pScrn->displayWidth >> 3;
	    break;
	case 16:
	    pReg->tridentRegs3CE[MiscExtFunc] |= 0x02;
    	    offset = pScrn->displayWidth >> 2;
	    if (pScrn->depth == 15)
    	    	pReg->tridentRegsDAC[0x00] = 0x10;
	    else
	    	pReg->tridentRegsDAC[0x00] = 0x30;
    	    pReg->tridentRegs3x4[PixelBusReg] = 0x04;
	    /* Reload with any chipset specific stuff here */
	    if (pTrident->Chipset >= TGUI9660) 
		pReg->tridentRegs3x4[PixelBusReg] |= 0x01;
	    if (pTrident->Chipset == TGUI9440AGi) {
    	        pReg->tridentRegs3CE[MiscExtFunc] |= 0x08;/*Clock Division / 2*/
	        clock *= 2;	/* Double the clock */
	    }
	    break;
	case 24:
	    pReg->tridentRegs3CE[MiscExtFunc] |= 0x02;
    	    offset = (pScrn->displayWidth * 3) >> 3;
    	    pReg->tridentRegs3x4[PixelBusReg] = 0x29;
	    pReg->tridentRegsDAC[0x00] = 0xD0;
	    if (pTrident->Chipset == CYBERBLADEXP4 ||
	        pTrident->Chipset == CYBERBLADEE4) {
    		OUTB(vgaIOBase+ 4, New32);
		pReg->tridentRegs3x4[New32] = INB(vgaIOBase + 5) & 0x7F;
	    }
	    break;
	case 32:
	    pReg->tridentRegs3CE[MiscExtFunc] |= 0x02;
	    if (pTrident->Chipset != CYBERBLADEXP4
	        && pTrident->Chipset != CYBERBLADEE4
		&& pTrident->Chipset != CYBERBLADEXPAI1) {
	        /* Clock Division by 2*/
	        pReg->tridentRegs3CE[MiscExtFunc] |= 0x08; 
		clock *= 2;	/* Double the clock */
	    }
    	    offset = pScrn->displayWidth >> 1;
    	    pReg->tridentRegs3x4[PixelBusReg] = 0x09;
	    pReg->tridentRegsDAC[0x00] = 0xD0;
	    if (pTrident->Chipset == CYBERBLADEXP4
	        || pTrident->Chipset == CYBERBLADEE4
		|| pTrident->Chipset == CYBERBLADEXPAI1) {
    		OUTB(vgaIOBase+ 4, New32);
		pReg->tridentRegs3x4[New32] = INB(vgaIOBase + 5) | 0x80;
		/* With new mode 32bpp we set the packed flag */
      	    	pReg->tridentRegs3x4[PixelBusReg] |= 0x20; 
	    }
	    break;
    }
    pReg->tridentRegs3x4[Offset] = offset & 0xFF;

    {
	CARD8 a, b;
	TGUISetClock(pScrn, clock, &a, &b);
	pReg->tridentRegsClock[0x00] = (regp->MiscOutReg & 0xF3) | 0x08;
	pReg->tridentRegsClock[0x01] = a;
	pReg->tridentRegsClock[0x02] = b;
	if (pTrident->MCLK > 0) {
	    TGUISetMCLK(pScrn, pTrident->MCLK, &a, &b);
	    pReg->tridentRegsClock[0x03] = a;
	    pReg->tridentRegsClock[0x04] = b;
	}
    }

    pReg->tridentRegs3C4[NewMode1] = 0xC0;
    pReg->tridentRegs3C4[Protection] = 0x92;

    pReg->tridentRegs3x4[LinearAddReg] = 0;
    if (pTrident->Linear) {
	/* This is used for VLB, when we support it again in 4.0 */
	if (pTrident->Chipset < CYBER9385)
    	    pReg->tridentRegs3x4[LinearAddReg] |=
					((pTrident->FbAddress >> 24) << 6)|
					((pTrident->FbAddress >> 20) & 0x0F);
	/* Turn on linear mapping */
    	pReg->tridentRegs3x4[LinearAddReg] |= 0x20; 
    } else {
	pReg->tridentRegs3CE[MiscExtFunc] |= 0x04;
    }
    
    pReg->tridentRegs3x4[CRTCModuleTest] = 
				(mode->Flags & V_INTERLACE ? 0x84 : 0x80);

    OUTB(vgaIOBase+ 4, InterfaceSel);
    pReg->tridentRegs3x4[InterfaceSel] = INB(vgaIOBase + 5) | 0x40;

    OUTB(vgaIOBase+ 4, Performance);
    pReg->tridentRegs3x4[Performance] = INB(vgaIOBase + 5);
    if (pTrident->Chipset < BLADEXP)
	pReg->tridentRegs3x4[Performance] |= 0x10;

    OUTB(vgaIOBase+ 4, DRAMControl);
    if (pTrident->Chipset >= CYBER9388)
    	pReg->tridentRegs3x4[DRAMControl] = INB(vgaIOBase + 5) | 0x10;

    if (pTrident->IsCyber && !pTrident->MMIOonly)
	pReg->tridentRegs3x4[DRAMControl] |= 0x20;

    if (pTrident->NewClockCode && pTrident->Chipset <= CYBER9397DVD) {
	    OUTB(vgaIOBase + 4, ClockControl);
	    pReg->tridentRegs3x4[ClockControl] = INB(vgaIOBase + 5) | 0x01;
    }

    OUTB(vgaIOBase+ 4, AddColReg);
    pReg->tridentRegs3x4[AddColReg] = INB(vgaIOBase + 5) & 0xEF;
    pReg->tridentRegs3x4[AddColReg] |= (offset & 0x100) >> 4;

    if (pTrident->Chipset >= TGUI9660) {
    	pReg->tridentRegs3x4[AddColReg] &= 0xDF;
    	pReg->tridentRegs3x4[AddColReg] |= (offset & 0x200) >> 4;
    }
   
    if (IsPciCard && UseMMIO) {
    	if (!pTrident->NoAccel)
	    pReg->tridentRegs3x4[GraphEngReg] |= 0x80; 
    } else {
    	if (!pTrident->NoAccel)
	    pReg->tridentRegs3x4[GraphEngReg] |= 0x82; 
    }

    OUTB(0x3CE, MiscIntContReg);
    pReg->tridentRegs3CE[MiscIntContReg] = INB(0x3CF) | 0x04;

    /* Fix hashing problem in > 8bpp on 9320 chipset */
    if (pTrident->Chipset == CYBER9320 && pScrn->bitsPerPixel > 8) 
    	pReg->tridentRegs3CE[MiscIntContReg] &= ~0x80;

    OUTB(vgaIOBase+ 4, PCIReg);
    if (IsPciCard && UseMMIO)
    	pReg->tridentRegs3x4[PCIReg] = INB(vgaIOBase + 5) & 0xF9; 
    else
    	pReg->tridentRegs3x4[PCIReg] = INB(vgaIOBase + 5) & 0xF8; 

    /* Enable PCI Bursting on capable chips */
    if (pTrident->Chipset >= TGUI9660) {
	if(pTrident->UsePCIBurst) {
	    pReg->tridentRegs3x4[PCIReg] |= 0x06;
	} else {
	    pReg->tridentRegs3x4[PCIReg] &= 0xF9;
	}
    }

    if (pTrident->Chipset >= CYBER9388) {
	if (pTrident->GammaBrightnessOn)
	    xf86DrvMsgVerb(pScrn->scrnIndex,X_INFO,1,
			   "Setting Gamma: %f Brightness: %i\n",
			   pTrident->gamma, pTrident->brightness);
	tridentSetBrightnessAndGamma(pReg,
				     pTrident->GammaBrightnessOn,
				     pTrident->gamma, pTrident->brightness);
    }
    
    /* Video */
    OUTB(0x3C4,0x20);
    pReg->tridentRegs3C4[SSetup] = INB(0x3C5) | 0x4;
    pReg->tridentRegs3C4[SKey] = 0x00;
    pReg->tridentRegs3C4[SPKey] = 0xC0;
    OUTB(0x3C4,0x12);
    pReg->tridentRegs3C4[Threshold] = INB(0x3C5);
    if (pScrn->bitsPerPixel > 16)
	pReg->tridentRegs3C4[Threshold] =
	    (pReg->tridentRegs3C4[Threshold] & 0xf0) | 0x2;
    
     /* restore */
    if (pTrident->Chipset > PROVIDIA9685) {
    	OUTB(0x3C4, Protection);
    	OUTB(0x3C5, protect);
    }
 
    if (pTrident->Chipset == CYBERBLADEXP4)
    	pReg->tridentRegs3CE[DisplayEngCont] = 0x08;

    /* Avoid lockup on Blade3D, PCI Retry is permanently on */
    if (pTrident->Chipset == BLADE3D)
    	pReg->tridentRegs3x4[PCIRetry] = 0x9F;
   
    return(TRUE);
}

void
TridentRestore(ScrnInfoPtr pScrn, TRIDENTRegPtr tridentReg)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int vgaIOBase;
    vgaIOBase = VGAHWPTR(pScrn)->IOBase;

    if (pTrident->Chipset > PROVIDIA9685) {
    	OUTB(0x3C4, Protection);
    	OUTB(0x3C5, 0x92);
    }
#if 0
    if (pTrident->doInit && pTrident->Int10) {
        OUTW_3CE(BiosReg);	
    }
#endif
    /* Goto New Mode */
    OUTB(0x3C4, 0x0B);
    (void) INB(0x3C5);

    /* Unprotect registers */
    OUTW(0x3C4, ((0xC0 ^ 0x02) << 8) | NewMode1);
    
    (void) INB(0x3C8);
    (void) INB(0x3C6);
    (void) INB(0x3C6);
    (void) INB(0x3C6);
    (void) INB(0x3C6);
    OUTB(0x3C6, tridentReg->tridentRegsDAC[0x00]);
    (void) INB(0x3C8);

    OUTW_3x4(CRTCModuleTest);
    OUTW_3x4(LinearAddReg);
    OUTW_3C4(NewMode2);
    OUTW_3x4(CursorControl);
    OUTW_3x4(CRTHiOrd);
    OUTW_3x4(HorizOverflow);
    OUTW_3x4(AddColReg);
    OUTW_3x4(GraphEngReg);
    OUTW_3x4(Performance);
    OUTW_3x4(InterfaceSel);
    OUTW_3x4(DRAMControl);
    OUTW_3x4(PixelBusReg);
    OUTW_3x4(PCIReg);
    OUTW_3x4(PCIRetry);
    OUTW_3CE(MiscIntContReg);
    OUTW_3CE(MiscExtFunc);
    OUTW_3x4(Offset);
    if (pTrident->NewClockCode && pTrident->Chipset <= CYBER9397DVD)
	OUTW_3x4(ClockControl);
    if (pTrident->Chipset >= CYBER9388) {
	OUTW_3C4(Threshold);
	OUTW_3C4(SSetup);
	OUTW_3C4(SKey);
	OUTW_3C4(SPKey);
	OUTW_3x4(PreEndControl);
	OUTW_3x4(PreEndFetch);
	OUTW_3C4(GBslope1);
	OUTW_3C4(GBslope2);
	OUTW_3C4(GBslope3);
	OUTW_3C4(GBslope4);
	OUTW_3C4(GBintercept1);
	OUTW_3C4(GBintercept2);
	OUTW_3C4(GBintercept3);
	OUTW_3C4(GBintercept4);
    }
    if (pTrident->Chipset >= CYBER9385)    OUTW_3x4(Enhancement0);
    if (pTrident->Chipset >= BLADE3D)      OUTW_3x4(RAMDACTiming);
    if (pTrident->Chipset == CYBERBLADEXP4 ||
        pTrident->Chipset == CYBERBLADEE4) OUTW_3x4(New32);
    if (pTrident->Chipset == CYBERBLADEXP4) OUTW_3CE(DisplayEngCont);
    if (pTrident->IsCyber) {
	CARD8 tmp;

	OUTW_3CE(VertStretch);
	OUTW_3CE(HorStretch);
	if (pTrident->Chipset < BLADEXP) {
	    OUTW_3CE(BiosMode);
	} else {
	    OUTW_3CE(BiosNewMode1);
	    OUTW_3CE(BiosNewMode2);
	};
	OUTW_3CE(BiosReg);
	OUTW_3CE(FPConfig);
    	OUTW_3CE(CyberControl);
    	OUTW_3CE(CyberEnhance);
	SHADOW_ENABLE(tmp);
	OUTW_3x4(0x0);
	if (pTrident->shadowNew) {
	    OUTW_3x4(0x1);
	    OUTW_3x4(0x2);	    
	}
	OUTW_3x4(0x3);
	OUTW_3x4(0x4);
	OUTW_3x4(0x5);
	OUTW_3x4(0x6);
	OUTW_3x4(0x7);	
	OUTW_3x4(0x10);
	OUTW_3x4(0x11); 
	if (pTrident->shadowNew) {
	    OUTW_3x4(0x12);
	    OUTW_3x4(0x15);
	}
	OUTW_3x4(0x16);
	SHADOW_RESTORE(tmp);
    }
 
    if (Is3Dchip) {
#ifdef READOUT
	if (!pTrident->DontSetClock)
#endif
	{
	    OUTW(0x3C4, (tridentReg->tridentRegsClock[0x01])<<8 | ClockLow);
	    OUTW(0x3C4, (tridentReg->tridentRegsClock[0x02])<<8 | ClockHigh);
	}
	if (pTrident->MCLK > 0) {
	    OUTW(0x3C4,(tridentReg->tridentRegsClock[0x03])<<8 | MCLKLow);
	    OUTW(0x3C4,(tridentReg->tridentRegsClock[0x04])<<8 | MCLKHigh);
	}
    } else {
#ifdef READOUT
	if (!pTrident->DontSetClock)
#endif
	{
	    OUTB(0x43C8, tridentReg->tridentRegsClock[0x01]);
	    OUTB(0x43C9, tridentReg->tridentRegsClock[0x02]);
	}
	if (pTrident->MCLK > 0) {
	    OUTB(0x43C6, tridentReg->tridentRegsClock[0x03]);
	    OUTB(0x43C7, tridentReg->tridentRegsClock[0x04]);
	}
    }
#ifdef READOUT
    if (!pTrident->DontSetClock)
#endif
    {
	OUTB(0x3C2, tridentReg->tridentRegsClock[0x00]);
    }
    
    if (pTrident->Chipset > PROVIDIA9685) {
    	OUTB(0x3C4, Protection);
    	OUTB(0x3C5, tridentReg->tridentRegs3C4[Protection]);
    }

    OUTW(0x3C4, ((tridentReg->tridentRegs3C4[NewMode1] ^ 0x02) << 8)| NewMode1);    
}

void
TridentSave(ScrnInfoPtr pScrn, TRIDENTRegPtr tridentReg)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int vgaIOBase;
    vgaIOBase = VGAHWPTR(pScrn)->IOBase;

    /* Goto New Mode */
    OUTB(0x3C4, 0x0B);
    (void) INB(0x3C5);

    INB_3C4(NewMode1);
    if (pTrident->Chipset > PROVIDIA9685)
    	INB_3C4(Protection);
    
    /* Unprotect registers */
    OUTW(0x3C4, ((0xC0 ^ 0x02) << 8) | NewMode1);
    if (pTrident->Chipset > PROVIDIA9685)
    	OUTW(0x3C4, (0x92 << 8) | Protection);

    INB_3x4(Offset);
    INB_3x4(LinearAddReg);
    INB_3x4(CRTCModuleTest);
    INB_3x4(CRTHiOrd);
    INB_3x4(HorizOverflow);
    INB_3x4(Performance);
    INB_3x4(InterfaceSel);
    INB_3x4(DRAMControl);
    INB_3x4(AddColReg);
    INB_3x4(PixelBusReg);
    INB_3x4(GraphEngReg);
    INB_3x4(PCIReg);
    INB_3x4(PCIRetry);
    if (pTrident->NewClockCode && pTrident->Chipset <= CYBER9397DVD)
	INB_3x4(ClockControl);
    if (pTrident->Chipset >= CYBER9388) {
	INB_3C4(Threshold);
	INB_3C4(SSetup);
	INB_3C4(SKey);
	INB_3C4(SPKey);
	INB_3x4(PreEndControl);
	INB_3x4(PreEndFetch);
	INB_3C4(GBslope1);
	INB_3C4(GBslope2);
	INB_3C4(GBslope3);
	INB_3C4(GBslope4);
	INB_3C4(GBintercept1);
	INB_3C4(GBintercept2);
	INB_3C4(GBintercept3);
	INB_3C4(GBintercept4);
    }
    if (pTrident->Chipset >= CYBER9385)    INB_3x4(Enhancement0);
    if (pTrident->Chipset >= BLADE3D)      INB_3x4(RAMDACTiming);
    if (pTrident->Chipset == CYBERBLADEXP4 ||
        pTrident->Chipset == CYBERBLADEE4) INB_3x4(New32);
    if (pTrident->Chipset == CYBERBLADEXP4) INB_3CE(DisplayEngCont);
    if (pTrident->IsCyber) {
	CARD8 tmp;
	INB_3CE(VertStretch);
	INB_3CE(HorStretch);
	if (pTrident->Chipset < BLADEXP) {
    	    INB_3CE(BiosMode);
	} else {
	INB_3CE(BiosNewMode1);
	INB_3CE(BiosNewMode2);
	}
	INB_3CE(BiosReg);	
	INB_3CE(FPConfig);
    	INB_3CE(CyberControl);
    	INB_3CE(CyberEnhance);
	SHADOW_ENABLE(tmp);
	INB_3x4(0x0);
	if (pTrident->shadowNew) {
	    INB_3x4(0x1);
	    INB_3x4(0x2);	    
	}
	INB_3x4(0x3);
	INB_3x4(0x4);
	INB_3x4(0x5);
	INB_3x4(0x6);
	INB_3x4(0x7);
	INB_3x4(0x10);
	INB_3x4(0x11);
	if (pTrident->shadowNew) {
	    INB_3x4(0x12);
	    INB_3x4(0x15);
	}
	INB_3x4(0x16);
	SHADOW_RESTORE(tmp);
    }

    /* save cursor registers */
    INB_3x4(CursorControl);

    INB_3CE(MiscExtFunc);
    INB_3CE(MiscIntContReg);

    (void) INB(0x3C8);
    (void) INB(0x3C6);
    (void) INB(0x3C6);
    (void) INB(0x3C6);
    (void) INB(0x3C6);
    tridentReg->tridentRegsDAC[0x00] = INB(0x3C6);
    (void) INB(0x3C8);

    tridentReg->tridentRegsClock[0x00] = INB(0x3CC);
    if (Is3Dchip) {
	OUTB(0x3C4, ClockLow);
	tridentReg->tridentRegsClock[0x01] = INB(0x3C5);
	OUTB(0x3C4, ClockHigh);
	tridentReg->tridentRegsClock[0x02] = INB(0x3C5);
	if (pTrident->MCLK > 0) {
	    OUTB(0x3C4, MCLKLow);
	    tridentReg->tridentRegsClock[0x03] = INB(0x3C5);
	    OUTB(0x3C4, MCLKHigh);
	    tridentReg->tridentRegsClock[0x04] = INB(0x3C5);
	}
    } else {
	tridentReg->tridentRegsClock[0x01] = INB(0x43C8);
	tridentReg->tridentRegsClock[0x02] = INB(0x43C9);
	if (pTrident->MCLK > 0) {
	    tridentReg->tridentRegsClock[0x03] = INB(0x43C6);
	    tridentReg->tridentRegsClock[0x04] = INB(0x43C7);
	}
    }

    INB_3C4(NewMode2);

    /* Protect registers */
    OUTW_3C4(NewMode1);
}

static void 
TridentShowCursor(ScrnInfoPtr pScrn) 
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int vgaIOBase;
    vgaIOBase = VGAHWPTR(pScrn)->IOBase;

    /* 64x64 */
    OUTW(vgaIOBase + 4, 0xC150);
}

static void 
TridentHideCursor(ScrnInfoPtr pScrn) {
    int vgaIOBase;
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    vgaIOBase = VGAHWPTR(pScrn)->IOBase;

    OUTW(vgaIOBase + 4, 0x4150);
}

static void 
TridentSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    int vgaIOBase;
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    vgaIOBase = VGAHWPTR(pScrn)->IOBase;
    
    if (x < 0) {
    	OUTW(vgaIOBase + 4, (-x)<<8 | 0x46);
	x = 0;
    } else
    	OUTW(vgaIOBase + 4, 0x0046);
 
    if (y < 0) {
    	OUTW(vgaIOBase + 4, (-y)<<8 | 0x47);
	y = 0;
    } else
    	OUTW(vgaIOBase + 4, 0x0047);

    OUTW(vgaIOBase + 4, (x&0xFF)<<8 | 0x40);
    OUTW(vgaIOBase + 4, (x&0x0F00)  | 0x41);
    OUTW(vgaIOBase + 4, (y&0xFF)<<8 | 0x42);
    OUTW(vgaIOBase + 4, (y&0x0F00)  | 0x43);
}

static void
TridentSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    int vgaIOBase;
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    vgaIOBase = VGAHWPTR(pScrn)->IOBase;
    OUTW(vgaIOBase + 4, (fg & 0x000000FF)<<8  | 0x48);
    OUTW(vgaIOBase + 4, (fg & 0x0000FF00)     | 0x49);
    OUTW(vgaIOBase + 4, (fg & 0x00FF0000)>>8  | 0x4A);
    OUTW(vgaIOBase + 4, (fg & 0xFF000000)>>16 | 0x4B);
    OUTW(vgaIOBase + 4, (bg & 0x000000FF)<<8  | 0x4C);
    OUTW(vgaIOBase + 4, (bg & 0x0000FF00)     | 0x4D);
    OUTW(vgaIOBase + 4, (bg & 0x00FF0000)>>8  | 0x4E);
    OUTW(vgaIOBase + 4, (bg & 0xFF000000)>>16 | 0x4F);
}

static void
TridentLoadCursorImage(
    ScrnInfoPtr pScrn, 
    CARD8 *src
)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int vgaIOBase;
    int programmed_offset = pTrident->CursorOffset / 1024;
    vgaIOBase = VGAHWPTR(pScrn)->IOBase;

    memcpy((CARD8 *)pTrident->FbBase + pTrident->CursorOffset,
			src, pTrident->CursorInfoRec->MaxWidth * 
			pTrident->CursorInfoRec->MaxHeight / 4);

    OUTW(vgaIOBase + 4, ((programmed_offset & 0xFF) << 8) | 0x44);
    OUTW(vgaIOBase + 4, (programmed_offset & 0xFF00) | 0x45);
}

static Bool 
TridentUseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    
    if (pTrident->MUX && pScrn->bitsPerPixel == 8) return FALSE;

    if (!pTrident->HWCursor) return FALSE;

    return TRUE;
}

#define CURSOR_WIDTH 64
#define CURSOR_HEIGHT 64
#define CURSOR_ALIGN(x,bytes) (((x) + ((bytes) - 1)) & ~((bytes) - 1))

Bool 
TridentHWCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    xf86CursorInfoPtr infoPtr;
    FBAreaPtr          fbarea;
    int                width;
    int		       width_bytes;
    int                height;
    int                size_bytes;

    size_bytes                = CURSOR_WIDTH * 4 * CURSOR_HEIGHT;
    width                     = pScrn->displayWidth;
    width_bytes		      = width * (pScrn->bitsPerPixel / 8);
    height                    = (size_bytes + width_bytes - 1) / width_bytes;
    fbarea                    = xf86AllocateOffscreenArea(pScreen,
							  width,
							  height,
							  1024,
							  NULL,
							  NULL,
							  NULL);

    if (!fbarea) {
	pTrident->CursorOffset = 0;
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Hardware cursor disabled"
		   " due to insufficient offscreen memory\n");
	return FALSE;
    } else {
	pTrident->CursorOffset = CURSOR_ALIGN((fbarea->box.x1 + 
					       fbarea->box.y1 * width) *
					       pScrn->bitsPerPixel / 8,
					       1024);
    }

    if ((pTrident->Chipset != CYBER9397DVD) &&
      			    (pTrident->Chipset < CYBERBLADEE4)) {
	/* Can't deal with an offset more than 4MB - 4096 bytes */
	if (pTrident->CursorOffset >= ((4096*1024) - 4096)) {
	    pTrident->CursorOffset = 0;
    	    xf86FreeOffscreenArea(fbarea);
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Hardware cursor disabled"
		   " due to cursor offset constraints.\n");
		return FALSE;
	}
    }

    infoPtr = xf86CreateCursorInfoRec();
    if(!infoPtr) return FALSE;
    
    pTrident->CursorInfoRec = infoPtr;

    infoPtr->MaxWidth = 64;
    infoPtr->MaxHeight = 64;
    infoPtr->Flags = HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
		HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK |
		HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_32 |
                ((pTrident->Chipset == CYBERBLADEXP4 ||
                  pTrident->Chipset == CYBERBLADEE4) ? 
                HARDWARE_CURSOR_TRUECOLOR_AT_8BPP : 0);
    infoPtr->SetCursorColors = TridentSetCursorColors;
    infoPtr->SetCursorPosition = TridentSetCursorPosition;
    infoPtr->LoadCursorImage = TridentLoadCursorImage;
    infoPtr->HideCursor = TridentHideCursor;
    infoPtr->ShowCursor = TridentShowCursor;
    infoPtr->UseHWCursor = TridentUseHWCursor;

    return(xf86InitCursor(pScreen, infoPtr));
}

unsigned int
Tridentddc1Read(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int vgaIOBase = VGAHWPTR(pScrn)->IOBase;
    CARD8 temp;

    /* New mode */
    OUTB(0x3C4, 0x0B); temp = INB(0x3C5);

    OUTB(0x3C4, NewMode1);
    temp = INB(0x3C5);
    OUTB(0x3C5, temp | 0x80);

    /* Define SDA as input */
    OUTW(vgaIOBase + 4, (0x04 << 8) | I2C);

    OUTW(0x3C4, (temp << 8) | NewMode1);

    /* Wait until vertical retrace is in progress. */
    while (INB(vgaIOBase + 0xA) & 0x08);
    while (!(INB(vgaIOBase + 0xA) & 0x08));

    /* Get the result */
    OUTB(vgaIOBase + 4, I2C);
    return ( INB(vgaIOBase + 5) & 0x01 );
}

void TridentSetOverscan(
    ScrnInfoPtr pScrn, 
    int overscan
){
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    if (overscan < 0 || overscan > 255)
	return;

    hwp->enablePalette(hwp);
    hwp->writeAttr(hwp, OVERSCAN, overscan);
    hwp->disablePalette(hwp);
}

void TridentLoadPalette(
    ScrnInfoPtr pScrn, 
    int numColors, 
    int *indicies,
    LOCO *colors,
    VisualPtr pVisual
){
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int i, index;
    for(i = 0; i < numColors; i++) {
	index = indicies[i];
    	OUTB(0x3C6, 0xFF);
	DACDelay(hwp);
        OUTB(0x3c8, index);
	DACDelay(hwp);
        OUTB(0x3c9, colors[index].red);
	DACDelay(hwp);
        OUTB(0x3c9, colors[index].green);
	DACDelay(hwp);
        OUTB(0x3c9, colors[index].blue);
	DACDelay(hwp);
    }
}
