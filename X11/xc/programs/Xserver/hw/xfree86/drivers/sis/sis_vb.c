/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_vb.c,v 1.10 2003/01/29 15:42:17 eich Exp $ */
/*
 * Video bridge detection and configuration for 300 and 310/325 series
 *
 * Copyright 2002 by Thomas Winischhofer, Vienna, Austria
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Thomas Winischhofer not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Thomas Winischhofer makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THOMAS WINISCHHOFER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THOMAS WINISCHHOFER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: 	Thomas Winischhofer <thomas@winischhofer.net>
 *		(Completely rewritten)
 */

#include "xf86.h"
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86PciInfo.h"

#include "sis.h"
#include "sis_regs.h"
#include "sis_vb.h"

static const SiS_LCD_StStruct SiS300_LCD_Type[]=
{
        { VB_LCD_1024x768, 1024,  768, LCD_1024x768,  1},  /* 0 - invalid */
	{ VB_LCD_800x600,   800,  600, LCD_800x600,   0},  /* 1 */
	{ VB_LCD_1024x768, 1024,  768, LCD_1024x768,  1},  /* 2 */
	{ VB_LCD_1280x1024,1280, 1024, LCD_1280x1024, 2},  /* 3 */
	{ VB_LCD_1280x960, 1280,  960, LCD_1280x960,  3},  /* 4 */
	{ VB_LCD_640x480,   640,  480, LCD_640x480,   4},  /* 5 */
	{ VB_LCD_1024x600, 1024,  600, LCD_1024x600, 10},  /* 6 */
	{ VB_LCD_1152x768, 1152,  768, LCD_1152x768,  7},  /* 7 */
	{ VB_LCD_320x480,   320,  480, LCD_320x480,   6},  /* 8 */
	{ VB_LCD_1024x768, 1024,  768, LCD_1024x768,  1},  /* 9 */
	{ VB_LCD_1024x768, 1024,  768, LCD_1024x768,  1},  /* a */
	{ VB_LCD_1024x768, 1024,  768, LCD_1024x768,  1},  /* b */
	{ VB_LCD_1024x768, 1024,  768, LCD_1024x768,  1},  /* c */
	{ VB_LCD_1024x768, 1024,  768, LCD_1024x768,  1},  /* d */
	{ VB_LCD_1024x768, 1024,  768, LCD_1024x768,  1},  /* e */
	{ VB_LCD_1024x768, 1024,  768, LCD_1024x768,  1},  /* f */
};

static const SiS_LCD_StStruct SiS310_LCD_Type[]=
{
        { VB_LCD_1024x768,  1024, 768, LCD_1024x768,  1},  /* 0 - invalid */
	{ VB_LCD_800x600,    800, 600, LCD_800x600,   0},  /* 1 */
	{ VB_LCD_1024x768,  1024, 768, LCD_1024x768,  1},  /* 2 */
	{ VB_LCD_1280x1024, 1280,1024, LCD_1280x1024, 2},  /* 3 */
	{ VB_LCD_640x480,    640, 480, LCD_640x480,   4},  /* 4 */
	{ VB_LCD_1024x600,  1024, 600, LCD_1024x600, 10},  /* 5 */
	{ VB_LCD_1152x864,  1152, 864, LCD_1152x864, 11},  /* 6 */
	{ VB_LCD_1280x960,  1280, 960, LCD_1280x960,  3},  /* 7 */
	{ VB_LCD_1152x768,  1152, 768, LCD_1152x768,  7},  /* 8 */
	{ VB_LCD_1400x1050, 1400,1050, LCD_1400x1050, 8},  /* 9 */
	{ VB_LCD_1280x768,  1280, 768, LCD_1280x768,  9},  /* a */
	{ VB_LCD_1600x1200, 1600,1200, LCD_1600x1200, 5},  /* b */
	{ VB_LCD_320x480,    320, 480, LCD_320x480,   6},  /* c */
	{ VB_LCD_1024x768,  1024, 768, LCD_1024x768,  1},  /* d */
	{ VB_LCD_1024x768,  1024, 768, LCD_1024x768,  1},  /* e */
	{ VB_LCD_1024x768,  1024, 768, LCD_1024x768,  1}   /* f */
};

static const char  *panelres[] = {
	"800x600",
	"1024x768",
	"1280x1024",
	"1280x960",
	"640x480",
	"1600x1200",
	"320x480",
	"1152x768",
	"1400x1050",
	"1280x768",
	"1024x600",
	"1152x864"
};

/* Detect CRT1 */
void SISCRT1PreInit(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);
    unsigned char CR32, SR17;
    unsigned char CRT1Detected = 0;
    unsigned char OtherDevices = 0;

    if(!(pSiS->VBFlags & VB_VIDEOBRIDGE)) {
        pSiS->CRT1off = 0;
        return;
    }

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode && pSiS->SecondHead) {
        pSiS->CRT1off = 0;
    	return;
    }
#endif

    inSISIDXREG(SISCR, 0x32, CR32);
    inSISIDXREG(SISSR, 0x17, SR17);

    if ( (pSiS->VGAEngine == SIS_300_VGA) &&
         (pSiS->Chipset != PCI_CHIP_SIS300) &&
         (SR17 & 0x0F) ) {

        if(SR17 & 0x01)  CRT1Detected = 1;
	if(SR17 & 0x0E)  OtherDevices = 1;

    } else {

        if(CR32 & 0x20)  CRT1Detected = 1;
	if(CR32 & 0x5F)  OtherDevices = 1;

    }

    if(pSiS->CRT1off == -1) {
            if(!CRT1Detected) {

                /* BIOS detected no CRT1. */
	        /* If other devices exist, switch it off */
	        if(OtherDevices) pSiS->CRT1off = 1;
		else             pSiS->CRT1off = 0;

    	    } else {

    	        /* BIOS detected CRT1, leave/switch it on */
	        pSiS->CRT1off = 0;

	    }
    }

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
    		"%sCRT1 connection detected\n",
		CRT1Detected ? "" : "No ");
}

/* Detect CRT2-LCD and LCD size */
void SISLCDPreInit(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);
    unsigned char CR32, SR17, CR36, CR37;
    USHORT textindex;

    if(!(pSiS->VBFlags & VB_VIDEOBRIDGE)) {
        return;
    }

    inSISIDXREG(SISCR, 0x32, CR32);
    inSISIDXREG(SISSR, 0x17, SR17);

    if( (pSiS->VGAEngine == SIS_300_VGA) &&
        (pSiS->Chipset != PCI_CHIP_SIS300) &&
	(SR17 & 0x0F) ) {
	if(SR17 & 0x02)
	   pSiS->VBFlags |= CRT2_LCD;
    } else {
    	if(CR32 & 0x08)
           pSiS->VBFlags |= CRT2_LCD;
    }

    if(pSiS->VBFlags & CRT2_LCD) {
        inSISIDXREG(SISCR, 0x36, CR36);
	inSISIDXREG(SISCR, 0x37, CR37);
	if((pSiS->VGAEngine == SIS_315_VGA) && (!CR36)) {
	    /* TW: Old 650/301LV BIOS version "forgot" to set CR36, CR37 */
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	       "BIOS-provided LCD information invalid, probing myself...\n");
	    if(pSiS->VBFlags & VB_LVDS) pSiS->SiS_Pr->SiS_IF_DEF_LVDS = 1;
	    else pSiS->SiS_Pr->SiS_IF_DEF_LVDS = 0;
	    SiS_GetPanelID(pSiS->SiS_Pr, &pSiS->sishw_ext);
	    inSISIDXREG(SISCR, 0x36, CR36);
	    inSISIDXREG(SISCR, 0x37, CR37);
	}
	if(pSiS->VGAEngine == SIS_300_VGA) {
	    pSiS->VBLCDFlags |= SiS300_LCD_Type[(CR36 & 0x0f)].VBLCD_lcdflag;
            pSiS->LCDheight = SiS300_LCD_Type[(CR36 & 0x0f)].LCDheight;
	    pSiS->LCDwidth = SiS300_LCD_Type[(CR36 & 0x0f)].LCDwidth;
            pSiS->sishw_ext.ulCRT2LCDType = SiS300_LCD_Type[(CR36 & 0x0f)].LCDtype;
	    textindex = SiS300_LCD_Type[(CR36 & 0x0f)].LCDrestextindex;
	} else {
	    pSiS->VBLCDFlags |= SiS310_LCD_Type[(CR36 & 0x0f)].VBLCD_lcdflag;
            pSiS->LCDheight = SiS310_LCD_Type[(CR36 & 0x0f)].LCDheight;
	    pSiS->LCDwidth = SiS310_LCD_Type[(CR36 & 0x0f)].LCDwidth;
            pSiS->sishw_ext.ulCRT2LCDType = SiS310_LCD_Type[(CR36 & 0x0f)].LCDtype;
	    textindex = SiS310_LCD_Type[(CR36 & 0x0f)].LCDrestextindex;
	}
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"Detected LCD panel resolution %s (type %d, %s%s)\n",
			panelres[textindex],
			(pSiS->VGAEngine == SIS_315_VGA) ? ((CR36 & 0x0f) - 1) : ((CR36 & 0xf0) >> 4),
			(pSiS->VBFlags & VB_LVDS) ?
			      (CR37 & 0x10 ? "non-expanding, " : "expanding, ") :
			      ( ((pSiS->VBFlags & VB_301B) && (pSiS->VGAEngine == SIS_300_VGA)) ?
			            (CR37 & 0x10 ? "non-expanding, " : "expanding, ") :
			            (CR37 & 0x10 ? "self-scaling, " : "non-self-scaling, ") ),
			CR37 & 0x01 ? "RGB18" : "RGB24");
    }
}

/* Detect CRT2-TV connector type and PAL/NTSC flag */
void SISTVPreInit(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);
    unsigned char SR16, SR17, SR38, CR32, CR38=0, CR79;
    int temp = 0;

    if(!(pSiS->VBFlags & VB_VIDEOBRIDGE))
        return;

    inSISIDXREG(SISCR, 0x32, CR32);
    inSISIDXREG(SISSR, 0x17, SR17);
    inSISIDXREG(SISSR, 0x16, SR16);
    inSISIDXREG(SISSR, 0x38, SR38);
    switch(pSiS->VGAEngine) {
    case SIS_300_VGA: 
       if(pSiS->Chipset != PCI_CHIP_SIS300) temp = 0x35;
       break;
    case SIS_315_VGA:
       temp = 0x38;
       break;
    }
    if(temp) {
       inSISIDXREG(SISCR, temp, CR38);
    }

#ifdef TWDEBUG
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "(vb.c: SR17=%02x CR32=%02x)\n", SR17, CR32);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "(vb.c: SR16=%02x SR38=%02x)\n", SR16, SR38);
#endif

    if( (pSiS->VGAEngine == SIS_300_VGA) &&
        (pSiS->Chipset != PCI_CHIP_SIS300) &&
        (SR17 & 0x0F) ) {

        if(SR17 & 0x04)
	    	pSiS->VBFlags |= CRT2_TV;

	if(SR17 & 0x20)
	        pSiS->VBFlags |= TV_SVIDEO;
        else if (SR17 & 0x10)
	        pSiS->VBFlags |= TV_AVIDEO;

	if(pSiS->VBFlags & (TV_SVIDEO | TV_AVIDEO)) {
	   if(SR16 & 0x20)
	     	pSiS->VBFlags |= TV_PAL;
           else
	        pSiS->VBFlags |= TV_NTSC;
	}

    } else {

        if(CR32 & 0x47)
	    	pSiS->VBFlags |= CRT2_TV;

     	if(CR32 & 0x04)
            	pSiS->VBFlags |= TV_SCART;
      	else if(CR32 & 0x02)
                pSiS->VBFlags |= TV_SVIDEO;
        else if(CR32 & 0x01)
                pSiS->VBFlags |= TV_AVIDEO;
        else if(CR32 & 0x40)
                pSiS->VBFlags |= (TV_SVIDEO | TV_HIVISION);
	else if((CR38 & 0x04) && (pSiS->VBFlags & VB_CHRONTEL)) 
		pSiS->VBFlags |= (TV_CHSCART | TV_PAL);
	else if((CR38 & 0x08) && (pSiS->VBFlags & VB_CHRONTEL))
		pSiS->VBFlags |= (TV_CHHDTV | TV_NTSC);
	        
	if(pSiS->VBFlags & (TV_SCART | TV_SVIDEO | TV_AVIDEO | TV_HIVISION)) {
	   if( (pSiS->Chipset == PCI_CHIP_SIS550) ||   /* TW: ? */
	       (pSiS->Chipset == PCI_CHIP_SIS650) ) {
	      inSISIDXREG(SISCR, 0x79, CR79);
	      if(CR79 & 0x20) {
                  pSiS->VBFlags |= TV_PAL;
		  if(CR38 & 0x40)      pSiS->VBFlags |= TV_PALM;
		  else if(CR38 & 0x80) pSiS->VBFlags |= TV_PALN;
 	      } else
	          pSiS->VBFlags |= TV_NTSC;
	   } else if(pSiS->VGAEngine == SIS_300_VGA) {
	      /* TW: Should be SR38 here as well, but this
	       *     does not work. Looks like a BIOS bug (2.04.5c).
	       */
	      if(SR16 & 0x20)
	     	  pSiS->VBFlags |= TV_PAL;
              else
	          pSiS->VBFlags |= TV_NTSC;
	   } else {	/* 315, 330 */
	      if(SR38 & 0x01) {
                  pSiS->VBFlags |= TV_PAL;
		  if(CR38 & 0x40)      pSiS->VBFlags |= TV_PALM;
		  else if(CR38 & 0x80) pSiS->VBFlags |= TV_PALN;
 	      } else
	          pSiS->VBFlags |= TV_NTSC;
	   }
	}
    }
    if(pSiS->VBFlags & (TV_SCART | TV_SVIDEO | TV_AVIDEO | TV_HIVISION | TV_CHSCART | TV_CHHDTV)) {
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"%sTV standard %s\n",
			(pSiS->VBFlags & (TV_CHSCART | TV_CHHDTV)) ? "Using " : "Detected default ",
			(pSiS->VBFlags & TV_NTSC) ? 
			   ((pSiS->VBFlags & TV_CHHDTV) ? "480i HDTV" : "NTSC") :
			   ((pSiS->VBFlags & TV_PALM) ? "PALM" :
			   	((pSiS->VBFlags & TV_PALN) ? "PALN" : "PAL")));
    }
}

/* Detect CRT2-VGA */
void SISCRT2PreInit(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);
    unsigned char SR17, CR32;

    if (!(pSiS->VBFlags & VB_VIDEOBRIDGE))
        return;

    /* CRT2-VGA not supported on LVDS and 30xLV(X) */
    if (pSiS->VBFlags & (VB_LVDS|VB_30xLV|VB_30xLVX))
        return;

    inSISIDXREG(SISCR, 0x32, CR32);
    inSISIDXREG(SISSR, 0x17, SR17);

    if( (pSiS->VGAEngine == SIS_300_VGA) &&
        (pSiS->Chipset != PCI_CHIP_SIS300) &&
	(SR17 & 0x0F) ) {

	 if(SR17 & 0x08)
	     pSiS->VBFlags |= CRT2_VGA;

    } else {

         if(CR32 & 0x10)
             pSiS->VBFlags |= CRT2_VGA;
	     
    }
}



