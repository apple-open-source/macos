/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_vb.c,v 1.45 2004/02/25 23:22:20 twini Exp $ */
/*
 * Video bridge detection and configuration for 300, 315 and 330 series
 *
 * Copyright (C) 2001-2004 by Thomas Winischhofer, Vienna, Austria
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
 * Author: 	Thomas Winischhofer <thomas@winischhofer.net>
 *		
 */

#include "xf86.h"
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86PciInfo.h"

#include "sis.h"
#include "sis_regs.h"
#include "sis_vb.h"
#include "sis_dac.h"

extern void    	     SISWaitRetraceCRT1(ScrnInfoPtr pScrn);
extern unsigned char SiS_GetSetBIOSScratch(ScrnInfoPtr pScrn, USHORT offset, unsigned char value);

static const SiS_LCD_StStruct SiS300_LCD_Type[]=
{
        { VB_LCD_1024x768, 1024,  768, LCD_1024x768 },  /* 0 - invalid */
	{ VB_LCD_800x600,   800,  600, LCD_800x600  },  /* 1 */
	{ VB_LCD_1024x768, 1024,  768, LCD_1024x768 },  /* 2 */
	{ VB_LCD_1280x1024,1280, 1024, LCD_1280x1024},  /* 3 */
	{ VB_LCD_1280x960, 1280,  960, LCD_1280x960 },  /* 4 */
	{ VB_LCD_640x480,   640,  480, LCD_640x480  },  /* 5 */
	{ VB_LCD_1024x600, 1024,  600, LCD_1024x600 },  /* 6 */
	{ VB_LCD_1152x768, 1152,  768, LCD_1152x768 },  /* 7 */
	{ VB_LCD_1024x768, 1024,  768, LCD_1024x768 },  /* 8 */
	{ VB_LCD_1024x768, 1024,  768, LCD_1024x768 },  /* 9 */
	{ VB_LCD_1280x768, 1280,  768, LCD_1280x768 },  /* a */
	{ VB_LCD_1024x768, 1024,  768, LCD_1024x768 },  /* b */
	{ VB_LCD_1024x768, 1024,  768, LCD_1024x768 },  /* c */
	{ VB_LCD_1024x768, 1024,  768, LCD_1024x768 },  /* d */
	{ VB_LCD_320x480,   320,  480, LCD_320x480  },  /* e */
	{ VB_LCD_CUSTOM,      0,    0, LCD_CUSTOM   }   /* f */
};

static const SiS_LCD_StStruct SiS315_LCD_Type[]=
{
        { VB_LCD_1024x768, 1024,  768, LCD_1024x768  },  /* 0 - invalid */
	{ VB_LCD_800x600,   800,  600, LCD_800x600   },  /* 1 */
	{ VB_LCD_1024x768, 1024,  768, LCD_1024x768  },  /* 2 */
	{ VB_LCD_1280x1024,1280, 1024, LCD_1280x1024 },  /* 3 */
	{ VB_LCD_640x480,   640,  480, LCD_640x480   },  /* 4 */
	{ VB_LCD_1024x600, 1024,  600, LCD_1024x600  },  /* 5 */
	{ VB_LCD_1152x864, 1152,  864, LCD_1152x864  },  /* 6 */
	{ VB_LCD_1280x960, 1280,  960, LCD_1280x960  },  /* 7 */
	{ VB_LCD_1152x768, 1152,  768, LCD_1152x768  },  /* 8 */
	{ VB_LCD_1400x1050,1400, 1050, LCD_1400x1050 },  /* 9 */
	{ VB_LCD_1280x768, 1280,  768, LCD_1280x768  },  /* a */
	{ VB_LCD_1600x1200,1600, 1200, LCD_1600x1200 },  /* b */
	{ VB_LCD_640x480_2, 640,  480, LCD_640x480_2 },  /* c DSTN/FSTN */
	{ VB_LCD_640x480_3, 640,  480, LCD_640x480_3 },  /* d DSTN/FSTN */
	{ VB_LCD_320x480,   320,  480, LCD_320x480   },  /* e */
	{ VB_LCD_CUSTOM,      0,    0, LCD_CUSTOM,   }   /* f */
};

static const SiS_LCD_StStruct SiS661_LCD_Type[]=
{
        { VB_LCD_1024x768, 1024,  768, LCD_1024x768  },  /* 0 - invalid */
	{ VB_LCD_800x600,   800,  600, LCD_800x600   },  /* 1 */
	{ VB_LCD_1024x768, 1024,  768, LCD_1024x768  },  /* 2 */
	{ VB_LCD_1280x1024,1280, 1024, LCD_1280x1024 },  /* 3 */
	{ VB_LCD_640x480,   640,  480, LCD_640x480   },  /* 4 */
	{ VB_LCD_1024x600, 1024,  600, LCD_1024x600  },  /* 5 */
	{ VB_LCD_1152x864, 1152,  864, LCD_1152x864  },  /* 6 */
	{ VB_LCD_1280x960, 1280,  960, LCD_1280x960  },  /* 7 */
	{ VB_LCD_1152x768, 1152,  768, LCD_1152x768  },  /* 8 */
	{ VB_LCD_1400x1050,1400, 1050, LCD_1400x1050 },  /* 9 */
	{ VB_LCD_1280x768, 1280,  768, LCD_1280x768  },  /* a */
	{ VB_LCD_1600x1200,1600, 1200, LCD_1600x1200 },  /* b */
	{ VB_LCD_1280x800, 1280,  800, LCD_1280x800  },  /* c */
	{ VB_LCD_1680x1050,1680, 1050, LCD_1680x1050 },  /* d */
	{ VB_LCD_1280x720, 1280,  720, LCD_1280x720  },  /* e */
	{ VB_LCD_CUSTOM,      0,    0, LCD_CUSTOM,   }   /* f */
};

static Bool
TestDDC1(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);
    unsigned short old;
    int count = 48;

    old = SiS_ReadDDC1Bit(pSiS->SiS_Pr);
    do {
       if(old != SiS_ReadDDC1Bit(pSiS->SiS_Pr)) break;
    } while(count--);
    return (count == -1) ? FALSE : TRUE;
}

static int
SiS_SISDetectCRT1(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);
    unsigned short temp = 0xffff;
    unsigned char SR1F, CR63=0, CR17;
    int i, ret = 0;
    Bool mustwait = FALSE;

    inSISIDXREG(SISSR,0x1F,SR1F);
    orSISIDXREG(SISSR,0x1F,0x04);
    andSISIDXREG(SISSR,0x1F,0x3F);
    if(SR1F & 0xc0) mustwait = TRUE;

    if(pSiS->VGAEngine == SIS_315_VGA) {
       inSISIDXREG(SISCR,pSiS->myCR63,CR63);
       CR63 &= 0x40;
       andSISIDXREG(SISCR,pSiS->myCR63,0xBF);
    }

    inSISIDXREG(SISCR,0x17,CR17);
    CR17 &= 0x80;
    if(!CR17) {
       orSISIDXREG(SISCR,0x17,0x80);
       mustwait = TRUE;
       outSISIDXREG(SISSR, 0x00, 0x01);
       outSISIDXREG(SISSR, 0x00, 0x03);
    }

    if(mustwait) {
       for(i=0; i < 10; i++) SISWaitRetraceCRT1(pScrn);
    }

    i = 3;
    do {
       temp = SiS_HandleDDC(pSiS->SiS_Pr, pSiS->VBFlags, pSiS->VGAEngine, 0, 0, NULL);
    } while(((temp == 0) || (temp == 0xffff)) && i--);

    if((temp == 0) || (temp == 0xffff)) {
       if(TestDDC1(pScrn)) temp = 1;
    }

    if((temp) && (temp != 0xffff)) {
       orSISIDXREG(SISCR,0x32,0x20);
       ret = 1;
    }

    if(pSiS->VGAEngine == SIS_315_VGA) {
       setSISIDXREG(SISCR,pSiS->myCR63,0xBF,CR63);
    }

    setSISIDXREG(SISCR,0x17,0x7F,CR17);

    outSISIDXREG(SISSR,0x1F,SR1F);

    return ret;
}

/* Detect CRT1 */
void SISCRT1PreInit(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);
    unsigned char CR32;
    unsigned char CRT1Detected = 0;
    unsigned char OtherDevices = 0;

    if(!(pSiS->VBFlags & VB_VIDEOBRIDGE)) {
       pSiS->CRT1off = 0;
       return;
    }

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
       pSiS->CRT1off = 0;
       return;
    }
#endif

#ifdef SISMERGED
    if((pSiS->MergedFB) && (!(pSiS->MergedFBAuto))) {
       pSiS->CRT1off = 0;
       return;
    }
#endif

    inSISIDXREG(SISCR, 0x32, CR32);

    if(CR32 & 0x20)  CRT1Detected = 1;
    else CRT1Detected = SiS_SISDetectCRT1(pScrn);

    if(CR32 & 0x5F)  OtherDevices = 1;

    if(pSiS->CRT1off == -1) {
       if(!CRT1Detected) {

          /* No CRT1 detected. */
	  /* If other devices exist, switch it off */
	  if(OtherDevices) pSiS->CRT1off = 1;
	  else             pSiS->CRT1off = 0;

       } else {

          /* CRT1 detected, leave/switch it on */
	  pSiS->CRT1off = 0;

       }
    }

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
    		"%sCRT1 (VGA) connection detected\n",
		CRT1Detected ? "" : "No ");
}

/* Detect CRT2-LCD and LCD size */
void SISLCDPreInit(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);
    unsigned char CR32, CR36, CR37, CR7D=0, tmp;

    pSiS->LCDwidth = 0;

    if(!(pSiS->VBFlags & VB_VIDEOBRIDGE)) return;

    inSISIDXREG(SISCR, 0x32, CR32);
   
    if(CR32 & 0x08) pSiS->VBFlags |= CRT2_LCD;
    
    /* If no panel has been detected by the BIOS during booting,
     * we try to detect it ourselves at this point. We do that
     * if forcecrt2redetection was given, too.
     * This is useful on machines with DVI connectors where the
     * panel was connected after booting. This is only supported
     * on the 315/330 series and the 301/30xB/C bridge (because the
     * 30xLV don't seem to have a DDC port and operate only LVDS
     * panels which mostly don't support DDC). We only do this if
     * there was no secondary VGA detected by the BIOS, because LCD
     * and VGA2 share the same DDC channel and might be misdetected
     * as the wrong type (especially if the LCD panel only supports
     * EDID Version 1).
     *
     * By default, CRT2 redetection is forced since 12/09/2003, as
     * I encountered numerous panels which deliver more or less
     * bogus DDC data confusing the BIOS. Since our DDC detection
     * is waaaay better, we prefer it instead of the primitive
     * and buggy BIOS method.
     */
#ifdef SISDUALHEAD
    if((!pSiS->DualHeadMode) || (!pSiS->SecondHead)) {
#endif
       if((pSiS->VGAEngine == SIS_315_VGA) &&
          (pSiS->VBFlags & (VB_301|VB_301B|VB_301C|VB_302B)) &&
          (!(pSiS->VBFlags & VB_30xBDH))) {

          if(pSiS->forcecrt2redetection) {
             pSiS->VBFlags &= ~CRT2_LCD;
          }

          if(!(pSiS->nocrt2ddcdetection)) {
             if((!(pSiS->VBFlags & CRT2_LCD)) && (!(CR32 & 0x10))) {
	        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	             "%s LCD/plasma panel, sensing via DDC\n",
		     pSiS->forcecrt2redetection ?
		        "Forced re-detection of" : "BIOS detected no");
                if(SiS_SenseLCDDDC(pSiS->SiS_Pr, pSiS)) {
    	           xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	              "DDC error during LCD panel detection\n");
	        } else {
	           inSISIDXREG(SISCR, 0x32, CR32);
	           if(CR32 & 0x08) {
	              pSiS->VBFlags |= CRT2_LCD;
		      pSiS->postVBCR32 |= 0x08;
	           } else {
	              xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	        	   "No LCD/plasma panel detected\n");
	           }
	        }
             }
          }

       }
#ifdef SISDUALHEAD
    }
#endif

    if(pSiS->VBFlags & CRT2_LCD) {
       inSISIDXREG(SISCR, 0x36, CR36);
       inSISIDXREG(SISCR, 0x37, CR37);
       inSISIDXREG(SISCR, 0x7D, CR7D);
       if(pSiS->SiS_Pr->SiS_CustomT == CUT_BARCO1366) {
          pSiS->VBLCDFlags |= VB_LCD_BARCO1366;
	  pSiS->LCDwidth = 1360;
	  pSiS->LCDheight = 1024;
	  if(CR37 & 0x10) pSiS->VBLCDFlags |= VB_LCD_EXPANDING;
	  xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		"Detected LCD panel (%dx%d, type %d, %sexpanding, RGB%d)\n",
		pSiS->LCDwidth, pSiS->LCDheight,
		((CR36 & 0xf0) >> 4),
		(CR37 & 0x10) ? "" : "non-",
		(CR37 & 0x01) ? 18 : 24);
       } else if(pSiS->SiS_Pr->SiS_CustomT == CUT_PANEL848) {
          pSiS->VBLCDFlags |= VB_LCD_848x480;
	  pSiS->LCDwidth = pSiS->SiS_Pr->CP_MaxX = 848;
	  pSiS->LCDheight = pSiS->SiS_Pr->CP_MaxY = 480;
	  pSiS->VBLCDFlags |= VB_LCD_EXPANDING;
	  xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	  	"Assuming LCD/plasma panel (848x480, expanding, RGB24)\n");
       } else {
	  if((pSiS->VGAEngine == SIS_315_VGA) && (!CR36)) {
	     /* Old 650/301LV BIOS version "forgot" to set CR36, CR37 */
	     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	        "BIOS-provided LCD information invalid, probing myself...\n");
	     if(pSiS->VBFlags & VB_LVDS) pSiS->SiS_Pr->SiS_IF_DEF_LVDS = 1;
	     else pSiS->SiS_Pr->SiS_IF_DEF_LVDS = 0;
	     SiS_GetPanelID(pSiS->SiS_Pr, &pSiS->sishw_ext);
	     inSISIDXREG(SISCR, 0x36, CR36);
	     inSISIDXREG(SISCR, 0x37, CR37);
	  }
	  if(((CR36 & 0x0f) == 0x0f) && (pSiS->SiS_Pr->CP_HaveCustomData)) {
	     pSiS->VBLCDFlags |= VB_LCD_CUSTOM;
             pSiS->LCDheight = pSiS->SiS_Pr->CP_MaxY;
	     pSiS->LCDwidth = pSiS->SiS_Pr->CP_MaxX;
	     if(CR37 & 0x10) pSiS->VBLCDFlags |= VB_LCD_EXPANDING;
	     xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		"Detected non-standard LCD/Plasma panel (max. X %d Y %d, preferred %dx%d, RGB%d)\n",
 		pSiS->SiS_Pr->CP_MaxX, pSiS->SiS_Pr->CP_MaxY,
		pSiS->SiS_Pr->CP_PreferredX, pSiS->SiS_Pr->CP_PreferredY,
		(CR37 & 0x01) ? 18 : 24);
	  } else {
	     if(pSiS->VGAEngine == SIS_300_VGA) {
	        pSiS->VBLCDFlags |= SiS300_LCD_Type[(CR36 & 0x0f)].VBLCD_lcdflag;
                pSiS->LCDheight = SiS300_LCD_Type[(CR36 & 0x0f)].LCDheight;
	        pSiS->LCDwidth = SiS300_LCD_Type[(CR36 & 0x0f)].LCDwidth;
	        if(CR37 & 0x10) pSiS->VBLCDFlags |= VB_LCD_EXPANDING;
	     } else if((pSiS->sishw_ext.jChipType >= SIS_661) || (pSiS->ROM661New)) {
	        pSiS->VBLCDFlags |= SiS661_LCD_Type[(CR36 & 0x0f)].VBLCD_lcdflag;
                pSiS->LCDheight = SiS661_LCD_Type[(CR36 & 0x0f)].LCDheight;
	        pSiS->LCDwidth = SiS661_LCD_Type[(CR36 & 0x0f)].LCDwidth;
	        if(CR37 & 0x10) pSiS->VBLCDFlags |= VB_LCD_EXPANDING;
		if(pSiS->sishw_ext.jChipType < SIS_661) {
		   if(!(pSiS->SiS_Pr->PanelSelfDetected)) {
		      inSISIDXREG(SISCR,0x35,tmp);
		      CR37 &= 0xfc;
		      CR37 |= (tmp & 0x01);
		   }
		}
 	     } else {
	        pSiS->VBLCDFlags |= SiS315_LCD_Type[(CR36 & 0x0f)].VBLCD_lcdflag;
                pSiS->LCDheight = SiS315_LCD_Type[(CR36 & 0x0f)].LCDheight;
	        pSiS->LCDwidth = SiS315_LCD_Type[(CR36 & 0x0f)].LCDwidth;
	        if(CR37 & 0x10) pSiS->VBLCDFlags |= VB_LCD_EXPANDING;
	     }
	     xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"Detected LCD/plasma panel (%dx%d, %d, %sexp., RGB%d [%02x%02x%02x])\n",
			pSiS->LCDwidth, pSiS->LCDheight,
			((pSiS->VGAEngine == SIS_315_VGA) &&
			 (pSiS->Chipset != PCI_CHIP_SIS660)) ?
			 	((CR36 & 0x0f) - 1) : ((CR36 & 0xf0) >> 4),
			(CR37 & 0x10) ? "" : "non-",
			(CR37 & 0x01) ? 18 : 24,
			CR36, CR37, CR7D);
	  }
       }
    }

}

/* Detect CRT2-TV connector type and PAL/NTSC flag */
void SISTVPreInit(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);
    unsigned char SR16, SR38, CR32, CR35=0, CR38=0, CR79, CR39;
    int temp = 0;

    if(!(pSiS->VBFlags & VB_VIDEOBRIDGE)) return;

    inSISIDXREG(SISCR, 0x32, CR32);
    inSISIDXREG(SISCR, 0x35, CR35);
    inSISIDXREG(SISSR, 0x16, SR16);
    inSISIDXREG(SISSR, 0x38, SR38);
    switch(pSiS->VGAEngine) {
    case SIS_300_VGA: 
       if(pSiS->Chipset == PCI_CHIP_SIS630) temp = 0x35;
       break;
    case SIS_315_VGA:
       temp = 0x38;
       break;
    }
    if(temp) {
       inSISIDXREG(SISCR, temp, CR38);
    }

#ifdef TWDEBUG
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, 
    	"(vb.c: CR32=%02x SR16=%02x SR38=%02x)\n", 
	CR32, SR16, SR38);
#endif

    if(CR32 & 0x47) pSiS->VBFlags |= CRT2_TV;

    if(pSiS->SiS_SD_Flags & SiS_SD_SUPPORTYPBPR) {
       if(CR32 & 0x80) pSiS->VBFlags |= CRT2_TV;
    } else {
       CR32 &= 0x7f;
    }

    if(CR32 & 0x01)
       pSiS->VBFlags |= TV_AVIDEO;
    else if(CR32 & 0x02)
       pSiS->VBFlags |= TV_SVIDEO;
    else if(CR32 & 0x04)
       pSiS->VBFlags |= TV_SCART;
    else if((CR32 & 0x40) && (pSiS->SiS_SD_Flags & SiS_SD_SUPPORTHIVISION))
       pSiS->VBFlags |= (TV_HIVISION | TV_PAL);
    else if((CR32 & 0x80) && (pSiS->SiS_SD_Flags & SiS_SD_SUPPORTYPBPR)) {
       pSiS->VBFlags |= TV_YPBPR;
       if(pSiS->Chipset == PCI_CHIP_SIS660) {
          if(CR38 & 0x04) {
             switch(CR35 & 0xE0) {
             case 0x20: pSiS->VBFlags |= TV_YPBPR525P; break;
	     case 0x40: pSiS->VBFlags |= TV_YPBPR750P; break;
	     case 0x60: pSiS->VBFlags |= TV_YPBPR1080I; break;
	     default:   pSiS->VBFlags |= TV_YPBPR525I;
	     }
          } else        pSiS->VBFlags |= TV_YPBPR525I;
          inSISIDXREG(SISCR,0x39,CR39);
	  CR39 &= 0x03;
	  if(CR39 == 0x00)      pSiS->VBFlags |= TV_YPBPR43LB;
	  else if(CR39 == 0x01) pSiS->VBFlags |= TV_YPBPR43;
	  else if(CR39 == 0x02) pSiS->VBFlags |= TV_YPBPR169;
	  else			pSiS->VBFlags |= TV_YPBPR43;
       } else if(pSiS->SiS_SD_Flags & SiS_SD_SUPPORTYPBPR) {
          if(CR38 & 0x08) {
	     switch(CR38 & 0x30) {
	     case 0x10: pSiS->VBFlags |= TV_YPBPR525P; break;
	     case 0x20: pSiS->VBFlags |= TV_YPBPR750P; break;
	     case 0x30: pSiS->VBFlags |= TV_YPBPR1080I; break;
	     default:   pSiS->VBFlags |= TV_YPBPR525I;
	     }
	  } else        pSiS->VBFlags |= TV_YPBPR525I;
	  if(pSiS->SiS_SD_Flags & SiS_SD_SUPPORTYPBPRAR) {
             inSISIDXREG(SISCR,0x3B,CR39);
	     CR39 &= 0x03;
	     if(CR39 == 0x00)      pSiS->VBFlags |= TV_YPBPR43LB;
	     else if(CR39 == 0x01) pSiS->VBFlags |= TV_YPBPR169;
	     else if(CR39 == 0x03) pSiS->VBFlags |= TV_YPBPR43;
	  }
       }
    } else if((CR38 & 0x04) && (pSiS->VBFlags & VB_CHRONTEL))
       pSiS->VBFlags |= (TV_CHSCART | TV_PAL);
    else if((CR38 & 0x08) && (pSiS->VBFlags & VB_CHRONTEL))
       pSiS->VBFlags |= (TV_CHYPBPR525I | TV_NTSC);

    if(pSiS->VBFlags & (TV_SCART | TV_SVIDEO | TV_AVIDEO)) {
       if(pSiS->VGAEngine == SIS_300_VGA) {
	  /* Should be SR38, but this does not work. */
	  if(SR16 & 0x20)
	     pSiS->VBFlags |= TV_PAL;
          else
	     pSiS->VBFlags |= TV_NTSC;
       } else if(pSiS->Chipset == PCI_CHIP_SIS550) {
          inSISIDXREG(SISCR, 0x7a, CR79);
	  if(CR79 & 0x08) {
             inSISIDXREG(SISCR, 0x79, CR79);
	     CR79 >>= 5;
	  }
	  if(CR79 & 0x01) {
             pSiS->VBFlags |= TV_PAL;
	     if(CR38 & 0x40)      pSiS->VBFlags |= TV_PALM;
	     else if(CR38 & 0x80) pSiS->VBFlags |= TV_PALN;
 	  } else
	     pSiS->VBFlags |= TV_NTSC;
       } else if(pSiS->Chipset == PCI_CHIP_SIS650) {
	  inSISIDXREG(SISCR, 0x79, CR79);
	  if(CR79 & 0x20) {
             pSiS->VBFlags |= TV_PAL;
	     if(CR38 & 0x40)      pSiS->VBFlags |= TV_PALM;
	     else if(CR38 & 0x80) pSiS->VBFlags |= TV_PALN;
 	  } else
	     pSiS->VBFlags |= TV_NTSC;
       } else if(pSiS->Chipset == PCI_CHIP_SIS660) {
          if(SR38 & 0x01) {
	     pSiS->VBFlags |= TV_PAL;
	     if(CR35 & 0x04)      pSiS->VBFlags |= TV_PALM;
	     else if(CR35 & 0x08) pSiS->VBFlags |= TV_PALN;
	  } else {
	     pSiS->VBFlags |= TV_NTSC;
	     if(CR35 & 0x02)      pSiS->VBFlags |= TV_NTSCJ;
	  }
       } else {	/* 315, 330 */
	  if(SR38 & 0x01) {
             pSiS->VBFlags |= TV_PAL;
	     if(CR38 & 0x40)      pSiS->VBFlags |= TV_PALM;
	     else if(CR38 & 0x80) pSiS->VBFlags |= TV_PALN;
 	  } else
	     pSiS->VBFlags |= TV_NTSC;
       }
    }

    if(pSiS->VBFlags & (TV_SCART|TV_SVIDEO|TV_AVIDEO)) {
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Detected default TV standard %s\n",
          (pSiS->VBFlags & TV_NTSC) ?
	     ((pSiS->VBFlags & TV_NTSCJ) ? "NTSCJ" : "NTSC") :
	         ((pSiS->VBFlags & TV_PALM) ? "PALM" :
		     ((pSiS->VBFlags & TV_PALN) ? "PALN" : "PAL")));
    }

    if(pSiS->VBFlags & TV_HIVISION) {
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "BIOS reports HiVision TV\n");
    }

    if((pSiS->VBFlags & VB_CHRONTEL) && (pSiS->VBFlags & (TV_CHSCART|TV_CHYPBPR525I))) {
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Chrontel: %s forced\n",
       	(pSiS->VBFlags & TV_CHSCART) ? "SCART (PAL)" : "YPbPr (480i)");
    }

    if(pSiS->VBFlags & TV_YPBPR) {
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Detected YPbPr TV (by default %s)\n",
         (pSiS->VBFlags & TV_YPBPR525I) ? "480i" :
	     ((pSiS->VBFlags & TV_YPBPR525P) ? "480p" :
	        ((pSiS->VBFlags & TV_YPBPR750P) ? "720p" : "1080i")));
    }
}

/* Detect CRT2-VGA */
void SISCRT2PreInit(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);
    unsigned char CR32; 

    if(!(pSiS->VBFlags & VB_VIDEOBRIDGE))  return;

    /* CRT2-VGA not supported on LVDS and 30xLV */
    if(pSiS->VBFlags & (VB_LVDS|VB_301LV|VB_302LV|VB_302ELV))
       return;

    inSISIDXREG(SISCR, 0x32, CR32);
    
    if(CR32 & 0x10)  pSiS->VBFlags |= CRT2_VGA;

#ifdef SISDUALHEAD
    if((!pSiS->DualHeadMode) || (!pSiS->SecondHead)) {
#endif

       if(pSiS->forcecrt2redetection) {
          pSiS->VBFlags &= ~CRT2_VGA;
       }

       /* We don't trust the normal sensing method for VGA2 since
        * it is performed by the BIOS during POST, and it is
        * impossible to sense VGA2 if the bridge is disabled.
        * Therefore, we try sensing VGA2 by DDC as well (if not
        * detected otherwise and only if there is no LCD panel
        * which is prone to be misdetected as a secondary VGA)
        */
       if(!(pSiS->nocrt2ddcdetection)) {
          if(pSiS->VBFlags & (VB_301|VB_301B|VB_301C|VB_302B)) {
             if(!(pSiS->VBFlags & (CRT2_VGA | CRT2_LCD))) {
	        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	           "%s secondary VGA, sensing via DDC\n",
	           pSiS->forcecrt2redetection ?
		      "Forced re-detection of" : "BIOS detected no");
                if(SiS_SenseVGA2DDC(pSiS->SiS_Pr, pSiS)) {
    	           xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	              "DDC error during secondary VGA detection\n");
	        } else {
	           inSISIDXREG(SISCR, 0x32, CR32);
	           if(CR32 & 0x10) {
	              pSiS->VBFlags |= CRT2_VGA;
	              pSiS->postVBCR32 |= 0x10;
		      xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		         "Detected secondary VGA connection\n");
	           } else {
	              xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		         "No secondary VGA connection detected\n");
	           }
	        }
             }
          }
       }
#ifdef SISDUALHEAD
    }
#endif    
}



