/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_setup.c,v 1.9 2003/02/04 02:44:29 dawes Exp $ */
/*
 * Basic hardware and memory detection
 *
 * Copyright 1998,1999 by Alan Hourihane, Wigan, England.
 * Parts Copyright 2001, 2002 by Thomas Winischhofer, Vienna, Austria.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holder not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The copyright holder makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
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
 *	     Thomas Winischhofer <thomas@winischhofer.net>
 */
 
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86.h"
#include "fb.h"
#include "xf1bpp.h"
#include "xf4bpp.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "xf86Version.h"

#include "xf86pciBus.h"
#include "xf86cmap.h"

#include "sis.h"
#include "sis_regs.h"
#include "sis_dac.h"

#define _XF86DGA_SERVER_
#include "extensions/xf86dgastr.h"

#include "globals.h"
#define DPMS_SERVER
#include "extensions/dpms.h"

static const char *dramTypeStr[] = {
        "Fast Page DRAM",
        "2 cycle EDO RAM",
        "1 cycle EDO RAM",
        "SDRAM/SGRAM",
        "SDRAM",
        "SGRAM",
        "ESDRAM",
	"DDR RAM",  /* for 550/650 */
	"DDR RAM",  /* for 550/650 */
        "" };

/* TW: MCLK tables for SiS6326 */
static const int SiS6326MCLKIndex[4][8] = {
       { 10, 12, 14, 16, 17, 18, 19,  7 },  /* SGRAM */
       {  4,  6,  8, 10, 11, 12, 13,  3 },  /* Fast Page */
       {  9, 11, 12, 13, 15, 16,  5,  7 },  /* 2 cycle EDO */
       { 10, 12, 14, 16, 17, 18, 19,  7 }   /* ? (Not 1 cycle EDO) */
};

static const struct _sis6326mclk {
    CARD16 mclk;
    unsigned char sr13;
    unsigned char sr28;
    unsigned char sr29;
} SiS6326MCLK[] = {
	{  0, 0,    0,    0 },
	{  0, 0,    0,    0 },
	{  0, 0,    0,    0 },
	{ 45, 0, 0x2b, 0x26 },
	{ 53, 0, 0x49, 0xe4 },
	{ 55, 0, 0x7c, 0xe7 },
	{ 56, 0, 0x7c, 0xe7 },
	{ 60, 0, 0x42, 0xe3 },
	{ 61, 0, 0x21, 0xe1 },
	{ 65, 0, 0x5a, 0xe4 },
	{ 66, 0, 0x5a, 0xe4 },
	{ 70, 0, 0x61, 0xe4 },
	{ 75, 0, 0x3e, 0xe2 },
	{ 80, 0, 0x42, 0xe2 },
	{ 83, 0, 0xb3, 0xc5 },
	{ 85, 0, 0x5e, 0xe3 },
	{ 90, 0, 0xae, 0xc4 },
	{100, 0, 0x37, 0xe1 },
	{115, 0, 0x78, 0x0e },
	{134, 0, 0x4a, 0xa3 }
};

/* For 5597, 6326, 530/620 */
static  void
sisOldSetup(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);
    int     ramsize[8]  = { 1,  2,  4, 0, 0,  2,  4,  8};
    int     buswidth[8] = {32, 64, 64, 0, 0, 32, 32, 64 };
    int     clockTable[4] = { 66, 75, 83, 100 };
    int     ramtype[4]  = { 5, 0, 1, 3 };
    int     config;
    int     temp, i;
    unsigned char sr23, sr33, sr34, sr37;
#if 0
    unsigned char newsr13, newsr28, newsr29;
#endif
    pciConfigPtr pdptr, *systemPCIdevices = NULL;

    if(pSiS->Chipset == PCI_CHIP_SIS5597) {
        inSISIDXREG(SISSR, FBSize, temp);
	pScrn->videoRam = ((temp & 0x07) + 1) * 256;
	inSISIDXREG(SISSR, Mode64, temp);
	if(temp & 0x06) {
		pScrn->videoRam *= 2;
		pSiS->BusWidth = 64;
	} else  pSiS->BusWidth = 32;
    } else {
        inSISIDXREG(SISSR, RAMSize, temp);
        config = ((temp & 0x10) >> 2 ) | ((temp & 0x6) >> 1);
        pScrn->videoRam = ramsize[config] * 1024;
        pSiS->BusWidth = buswidth[config];
    }

    if(pSiS->Chipset == PCI_CHIP_SIS530)  {

        inSISIDXREG(SISSR, 0x0D, temp);
	pSiS->Flags &= ~(UMA);
	if(temp & 0x01) {
		pSiS->Flags |= UMA;  		/* TW: Shared fb mode */
        	inSISIDXREG(SISSR, 0x10, temp);
        	pSiS->MemClock = clockTable[temp & 0x03] * 1000;
	} else  pSiS->MemClock = SiSMclk(pSiS); /* TW: Local fb mode */

    } else if(pSiS->Chipset == PCI_CHIP_SIS6326) {

       inSISIDXREG(SISSR,0x0e,temp);
       i = temp & 0x03;

       xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
            "Detected DRAM type: %s\n",
	    dramTypeStr[ramtype[i]]);

       temp = (temp >> 5) & 0x07;
       i = SiS6326MCLKIndex[i][temp];
       pSiS->MemClock = SiS6326MCLK[i].mclk;
#if 0
       /* TW: Correct invalid MCLK settings by old BIOSes */
       newsr13 = SiS6326MCLK[i].sr13;
       newsr28 = SiS6326MCLK[i].sr28;
       newsr29 = SiS6326MCLK[i].sr29;
       if((pSiS->ChipRev == 0x92) ||
          (pSiS->ChipRev == 0xd1) ||
	  (pSiS->ChipRev == 0xd2)) {
	  if(pSiS->MemClock == 60) {
	     newsr28 = 0xae;
	     newsr29 = 0xc4;
	  }
       }
#endif
       pSiS->MemClock *= 1000;
#if 0
       inSISIDXREG(SISSR, 0x13, temp);
       temp &= 0x80;
       temp |= (newsr13 & 0x80);
       outSISIDXREG(SISSR,0x13,temp);
       outSISIDXREG(SISSR,0x28,newsr28);
       outSISIDXREG(SISSR,0x29,newsr29);
#endif

    } else {

        pSiS->MemClock = SiSMclk(pSiS);

    }

    pSiS->Flags &= ~(SYNCDRAM | RAMFLAG);
    if(pSiS->oldChipset >= OC_SIS82204) {
       inSISIDXREG(SISSR, 0x23, sr23);
       inSISIDXREG(SISSR, 0x33, sr33);
       inSISIDXREG(SISSR, 0x34, sr34);
       if(sr33 & 0x09) {   	  			/* 5597: Sync DRAM timing | One cycle EDO ram;   */
       		pSiS->Flags |= (sr33 & SYNCDRAM);	/* 6326: Enable SGRam timing | One cycle EDO ram */
		pSiS->Flags |= RAMFLAG;			/* 530:  Enable SGRAM timing | reserved (0)      */
       } else if(sr23 & 0x20) {   			/* 5597, 6326: EDO DRAM enabled */
		pSiS->Flags |= SYNCDRAM;		/* 530/620:    reserved (0)     */
       }
    }

    pSiS->Flags &= ~(ESS137xPRESENT);
    if(pSiS->Chipset == PCI_CHIP_SIS530) {
       if(pSiS->oldChipset == OC_SIS530A) {
          if((systemPCIdevices = xf86GetPciConfigInfo())) {
	      i = 0;
              while((pdptr = systemPCIdevices[i])) {
	         if((pdptr->pci_vendor == 0x1274) &&
	            ((pdptr->pci_device == 0x5000) ||
	  	     ((pdptr->pci_device & 0xFFF0) == 0x1370))) {
		     pSiS->Flags |= ESS137xPRESENT;
		     break;
		 }
		 i++;
	      }
	  }
	  if(pSiS->Flags & ESS137xPRESENT) {
	     xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	         "SiS530/620: Found ESS device\n");
	  }
       }
    }

    pSiS->Flags &= ~(SECRETFLAG);
    if(pSiS->oldChipset >= OC_SIS5597) {
        inSISIDXREG(SISSR, 0x37, sr37);
	if(sr37 & 0x80) pSiS->Flags |= SECRETFLAG;
    }

    pSiS->Flags &= ~(A6326REVAB);
    if(pSiS->Chipset == PCI_CHIP_SIS6326) {
       if(((pSiS->ChipRev & 0x0f) == 0x0a) ||
          ((pSiS->ChipRev & 0x0f) == 0x0b)) {
	    pSiS->Flags |= A6326REVAB;
       }
    }

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
               "Detected memory clock: %3.3f MHz\n",
	       pSiS->MemClock/1000.0);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
               "Detected DRAM bus width: %d bit\n",
	       pSiS->BusWidth);
}

static  void
sis300Setup(ScrnInfoPtr pScrn)
{
    SISPtr    pSiS = SISPTR(pScrn);
    const int bus[4] = {32, 64, 128, 32};
    const int adaptermclk[8]    = {  66,  83, 100, 133,
                                    100, 100, 100, 100};
    const int adaptermclk300[8] = { 125, 125, 125, 100,
                                    100, 100, 100, 100};
    unsigned int    config;
    unsigned char   temp;
    int		    cpubuswidth;
    int 	    from = X_PROBED;

    pSiS->MemClock = SiSMclk(pSiS);

    inSISIDXREG(SISSR, 0x14, config);
    pScrn->videoRam = ((config & 0x3F) + 1) * 1024;
    cpubuswidth = bus[config >> 6];
    
    switch(pSiS->Chipset) {
    case PCI_CHIP_SIS300:
    	pSiS->BusWidth = cpubuswidth;
	break;
    case PCI_CHIP_SIS540:
    	pSiS->BusWidth = 64;
	from = X_INFO;
	break;
    case PCI_CHIP_SIS630:
    	pSiS->BusWidth = 64;
	from = X_INFO;
	break;
    default:
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"Internal error: sis300setup() called with invalid chipset!\n");
	pSiS->BusWidth = 64;
	from = X_INFO;
    }

    inSISIDXREG(SISSR, 0x3A, config);
    config &= 0x03;
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
            "Detected DRAM type: %s\n",
	    dramTypeStr[config+4]);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
            "Detected memory clock: %3.3f MHz\n",
            pSiS->MemClock/1000.0);

    if(pSiS->Chipset == PCI_CHIP_SIS300) {
       if(pSiS->ChipRev > 0x13) {
          inSISIDXREG(SISSR, 0x3A, temp);
          xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
             "(Adapter assumes MCLK being %d Mhz)\n",
	     adaptermclk300[(temp & 0x07)]);
       }
    } else {
       inSISIDXREG(SISSR, 0x1A, temp);
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
            "(Adapter assumes MCLK being %d Mhz)\n",
	    adaptermclk[(temp & 0x07)]);
    }
    
    xf86DrvMsg(pScrn->scrnIndex, from,
            "%s DRAM bus width: %d bit\n",
	    (from == X_PROBED) ? "Detected" : "Assuming",
	    pSiS->BusWidth);
}

/* TW: for 315, 315H, 315PRO, 330 */
static  void
sis310Setup(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);
    int     busSDR[4]  = {64, 64, 128, 128};
    int     busDDR[4]  = {32, 32,  64,  64};
    int     busDDRA[4] = {64+32, 64+32 , (64+32)*2, (64+32)*2};
    unsigned int config, config1, config2;
    char    *dramTypeStr310[] = {
        "Single Channel 1 rank SDR SDRAM",
        "Single Channel 1 rank SDR SGRAM",
        "Single Channel 1 rank DDR SDRAM",
        "Single Channel 1 rank DDR SGRAM",
        "Single Channel 2 rank SDR SDRAM",
        "Single Channel 2 rank SDR SGRAM",
        "Single Channel 2 rank DDR SDRAM",
        "Single Channel 2 rank DDR SGRAM",
	"Asymmetric SDR SDRAM",
	"Asymmetric SDR SGRAM",
	"Asymmetric DDR SDRAM",
	"Asymmetric DDR SGRAM",
	"Dual channel SDR SDRAM",
	"Dual channel SDR SGRAM",
	"Dual channel DDR SDRAM",
	"Dual channel DDR SGRAM"};
    char    *dramTypeStr330[] = {
        "Single Channel SDR SDRAM",
        "",
        "Single Channel DDR SDRAM",
        "",
        "--unknown--",
        "",
        "--unknown--",
        "",
	"Asymetric Dual Channel SDR SDRAM",
	"",
	"Asymetric Dual Channel DDR SDRAM",
	"",
	"Dual channel SDR SDRAM",
	"",
	"Dual channel DDR SDRAM",
	""};

    inSISIDXREG(SISSR, 0x14, config);
    config1 = (config & 0x0C) >> 2;
    inSISIDXREG(SISSR, 0x3A, config2);
    config2 &= 0x03;

    pScrn->videoRam = (1 << ((config & 0xF0) >> 4)) * 1024;

    if(pSiS->Chipset == PCI_CHIP_SIS330) {

       if(config1) pScrn->videoRam <<= 1;

    } else {

       /* If SINGLE_CHANNEL_2_RANK or DUAL_CHANNEL_1_RANK -> mem * 2 */
       if((config1 == 0x01) || (config1 == 0x03))
           pScrn->videoRam <<= 1;

       /* If DDR asymetric -> mem * 1,5 */
       if(config1 == 0x02)
           pScrn->videoRam += pScrn->videoRam/2;

    }

    pSiS->MemClock = SiSMclk(pSiS);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
            "Detected DRAM type: %s\n",
	    (pSiS->Chipset == PCI_CHIP_SIS330) ?
	        dramTypeStr330[(config1 * 4) + (config2 & 0x02)] :
	           dramTypeStr310[(config1 * 4) + config2]);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
            "Detected memory clock: %3.3f MHz\n",
            pSiS->MemClock/1000.0);

    /* TW: DDR -> mclk * 2 - needed for bandwidth calculation */
    if(pSiS->Chipset == PCI_CHIP_SIS330) {
       if(config2 & 0x02) {
       	  pSiS->MemClock *= 2;
	  if(config1 == 0x02) {
	     pSiS->BusWidth = busDDRA[0];
	  } else {
	     pSiS->BusWidth = busDDR[(config & 0x02)];
	  }
       } else {
          if(config1 == 0x02) {
	     pSiS->BusWidth = busDDRA[2];
	  } else {
             pSiS->BusWidth = busSDR[(config & 0x02)];
	  }
       }
    } else {
       if(config2 & 0x02) pSiS->MemClock *= 2;
       if(config1 == 0x02)
          pSiS->BusWidth = busDDRA[(config & 0x03)];
       else if(config2 & 0x02)
          pSiS->BusWidth = busDDR[(config & 0x03)];
       else
          pSiS->BusWidth = busSDR[(config & 0x03)];
    }

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
            "Detected DRAM bus width: %d bit\n",
	    pSiS->BusWidth);
}

/* TW: for 550, 650, 740 */
static  void
sis550Setup(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);
    unsigned int    config;
    CARD8	    pcimemcode;

    /* TW: Some of the following is guessed; however,
       since our mode switching code is omniscient
       anyway, we only need some reasonable values
       to prevent X from deleting modes from the
       list
     */

    inSISIDXREG(SISSR, 0x14, config);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
            "Detected DRAM type: %s\n",
	    dramTypeStr[(((config & 0x80) >> 7) << 2) + 4]);

    pSiS->MemClock = SiSMclk(pSiS);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
            "Detected memory clock: %3.3f MHz\n",
            pSiS->MemClock/1000.0);

    /* TW: DDR -> Mclk * 2 - needed for bandwidth calculation */
    if(config & 0x80) pSiS->MemClock *= 2;

    pSiS->BusWidth = (config & 0x40) ? 128 : 64;

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
            "Detected DRAM bus width: %d bit\n",
	    pSiS->BusWidth);

    pScrn->videoRam = (((config & 0x3F) + 1) * 4) * 1024;

    /* TW: Some 550 BIOSes don't seem to set SR14 correctly. We have
     * to read PCI configuration in order to get a correct size.
     */
    if (pSiS->Chipset == PCI_CHIP_SIS550) {
      if((pScrn->videoRam != 4*1024) &&
         (pScrn->videoRam != 8*1024) &&
         (pScrn->videoRam != 16*1024) &&
         (pScrn->videoRam != 24*1024) &&
         (pScrn->videoRam != 32*1024) &&
         (pScrn->videoRam != 48*1024) &&
         (pScrn->videoRam != 64*1024) &&
         (pScrn->videoRam != 96*1024) &&
         (pScrn->videoRam != 128*1024) &&
         (pScrn->videoRam != 256*1024)) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"Invalid memory size (%d) encountered, reading PCI configuration\n",
		pScrn->videoRam);
	pcimemcode = pciReadByte(0x00000000, 0x63);
	pScrn->videoRam = (1 << (((pcimemcode & 0x70) >> 4) + 21)) / 1024;
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		"PCI config reported %dKB video RAM\n", pScrn->videoRam);
      }
    }
}

void
SiSSetup(ScrnInfoPtr pScrn)
{
    SISPtr pSiS = SISPTR(pScrn);

    pSiS->Flags = 0;
    pSiS->VBFlags = 0;

    switch  (SISPTR(pScrn)->Chipset)  {
    case    PCI_CHIP_SIS5597:
    case    PCI_CHIP_SIS6326:
    case    PCI_CHIP_SIS530:
        sisOldSetup(pScrn);
        break;
    case    PCI_CHIP_SIS300:
    case    PCI_CHIP_SIS630:  /* +730 */
    case    PCI_CHIP_SIS540:
        sis300Setup(pScrn);
        break;
    case    PCI_CHIP_SIS315:
    case    PCI_CHIP_SIS315H:
    case    PCI_CHIP_SIS315PRO:
    case    PCI_CHIP_SIS330:
    	sis310Setup(pScrn);
	break;
    case    PCI_CHIP_SIS550:
    case    PCI_CHIP_SIS650: /* + 740 */
        sis550Setup(pScrn);
	break;
    default:
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"Internal error: SiSSetup() called with invalid Chipset (0x%x)\n",
		pSiS->Chipset);
        break;
    }
}


