/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/mga/mga_merge.c,v 1.2 2002/09/18 21:25:45 tsi Exp $ */

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"

/* All drivers need this */
#include "xf86_ansic.h"

#include "compiler.h"

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"
#include "mga.h"
#include "mga_macros.h"
#include "mga_reg.h"
#include "mga_merge.h"

#include "fbdevhw.h"

static int
StrToRanges(range* r, char* s) {
    float num=0.0;
    int rangenum=0;
    Bool gotdash = FALSE;
    Bool nextdash = FALSE;
    char* strnum=NULL;
    do {
        switch(*s) {
            case '0': case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9': case '.':
                if(strnum == NULL) {
                    strnum = s;
                    gotdash = nextdash;
                    nextdash = FALSE;
                 }
  
                break;
            case '-':
            case ' ': case 0:
                if(strnum == NULL) break; /*is extra seperator */
                if(strnum != NULL) sscanf(strnum,"%f",&num);
                if(gotdash) /*if wasn't singlet: correct. */
                    r[rangenum-1].hi = num;
                else { /*first, assume singlet */
                    r[rangenum].lo = num;
                    r[rangenum].hi = num;
                    rangenum++;
                }
                strnum = NULL;
                if(*s == '-')
                    nextdash = (rangenum != 0); /*ignore dash if before any number.*/
                break;
            default :
                return 0;        
        }
    } while(*(s++) != 0); /* run loop for every char including null terminator.*/
       
    return rangenum;
}            


/* Copys mode i, links the result to dest, and returns it.
 * Links i and j in Private record.
 * if dest is NULL, return value is copy of i linked to itself.
 */
static DisplayModePtr
CopyModeNLink(ScrnInfoPtr pScrn, DisplayModePtr dest, DisplayModePtr i, DisplayModePtr j, MgaScrn2Rel srel) {
    DisplayModePtr mode;
    int dx = 0,dy = 0;
    /* start with first node */
    mode = xalloc(sizeof(DisplayModeRec));
    memcpy(mode,i, sizeof(DisplayModeRec));
    mode->Private = xalloc(sizeof(MergedDisplayModeRec));
    ((MergedDisplayModePtr)mode->Private)->Monitor1 = i;
    ((MergedDisplayModePtr)mode->Private)->Monitor2 = j;
    ((MergedDisplayModePtr)mode->Private)->Monitor2Pos = srel;
    mode->PrivSize = 0;
    
        switch(srel) {
            case mgaLeftOf: 
            case mgaRightOf:
                dx = min(pScrn->virtualX,i->HDisplay + j->HDisplay) -  mode->HDisplay;
                dy = min(pScrn->virtualY,  max(i->VDisplay,j->VDisplay)) - mode->VDisplay;
                break;
            case mgaAbove: 
            case mgaBelow:
                dy = min(pScrn->virtualY,i->VDisplay + j->VDisplay) - mode->VDisplay;
                dx = min(pScrn->virtualX, max(i->HDisplay,j->HDisplay)) - mode->HDisplay;
                break;
            case mgaClone:
                dx = min(pScrn->virtualX, max(i->HDisplay,j->HDisplay)) - mode->HDisplay;
                dy = min(pScrn->virtualY, max(i->VDisplay,j->VDisplay)) - mode->VDisplay;
                break;
        }
    mode->HDisplay += dx;
    mode->HSyncStart += dx;
    mode->HSyncEnd += dx;
    mode->HTotal += dx;
    mode->VDisplay += dy;
    mode->VSyncStart += dy;
    mode->VSyncEnd += dy;
    mode->VTotal += dy;
    mode->Clock = 0; /* Shows we're in Merge mode. */
   
    mode->next = mode;
    mode->prev = mode;

    if(dest) {
        /* Insert node after "dest" */
        mode->next = dest->next; 
        dest->next->prev = mode;
        mode->prev = dest;
        dest->next = mode;
    } 

    return mode;
}

static DisplayModePtr
GetModeFromName(char* str, DisplayModePtr i)
{
    DisplayModePtr c = i;
    if(!i) return NULL;
    do {
        if(strcmp(str,c->name) == 0) return c;
        c = c->next;
    } while(c != i);
    return NULL;
}

/* takes a config file string of MetaModes and generates a MetaModeList */
static DisplayModePtr
GenerateModeList(ScrnInfoPtr pScrn, char* str,
		 DisplayModePtr i, DisplayModePtr j, MgaScrn2Rel srel) {
    char* strmode = str;
    char modename[256];
    Bool gotdash = FALSE;
    MgaScrn2Rel sr;
    
    DisplayModePtr mode1 = NULL;
    DisplayModePtr mode2 = NULL;
    DisplayModePtr result = NULL;
    do {
        switch(*str) {
            case 0:
            case '-':
            case ' ':
                if((strmode != str)) {/*we got a mode */
                    /* read new entry */
                    strncpy(modename,strmode,str - strmode);
                    modename[str - strmode] = 0;
                    
                    if(gotdash) {
                        if(mode1 == NULL) return NULL;
                        mode2 = GetModeFromName(modename,j);
                        if(!mode2) {
                            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                                "Mode: \"%s\" is not a supported mode for monitor 2\n",modename);
                            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                                "Skipping metamode \"%s-%s\".\n",mode1->name,modename);
                            mode1 = NULL;
                        } 
                    } else {
                        mode1 = GetModeFromName(modename,i);
                        if(!mode1) {
                            char* tmps = str;
                            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                                "Mode: \"%s\" is not a supported mode for monitor 1\n",modename);
                            /* find if a monitor2 mode follows */
                            gotdash = FALSE;
                            while(*tmps == ' ') tmps++;
                            if(*tmps == '-') { /* skip the next mode */
                                tmps++;
                                while((*tmps == ' ') && (*tmps != 0)) tmps++; /*skip spaces */
                                while((*tmps != ' ') && (*tmps != '-') && (*tmps != 0)) tmps++; /*skip modename */
                                /* for error message */
                                strncpy(modename,strmode,tmps - strmode);
                                modename[tmps - strmode] = 0;
                                str = tmps-1;
                            }
                            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                                "Skipping metamode \"%s\".\n",modename);
                            mode1 = NULL;
                        } 
                    }
                    gotdash = FALSE;
                }
                strmode = str+1; /* number starts on next char */
                gotdash |= (*str == '-');
                
                if(*str != 0) break; /* if end of string, we wont get a chance to catch a char and run the
                                        default case. do it now */
                
            default:
                if(!gotdash && mode1) { /* complete previous pair */
                    sr = srel;
                    if(!mode2) { 
                        mode2 = GetModeFromName(mode1->name,j);
                        sr = mgaClone;
                    }
                    if(!mode2) { 
                        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
                            "Mode: \"%s\" is not a supported mode for monitor 2\n",mode1->name);
                        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                            "Skipping clone mode \"%s\".\n");
                        mode1 = NULL;
                    } else {
                        result = CopyModeNLink(pScrn,result,mode1,mode2,sr);
                        mode1 = NULL;
                        mode2 = NULL;
                    }
                }
                break;
                
        }
    } while(*(str++) != 0);
    return result;
}
    

/* second CRTC init funcitons. Used to check monitor timings and refreshes. 
 * this function looses lots of maintainability points due to redundancy, 
 * but it still was the cleanest and least-intrusive way I found. */

Bool
MGAPreInitMergedFB(ScrnInfoPtr pScrn1, int flags)
{
    ScrnInfoPtr pScrn;
    MGAPtr pMga;
    MGAPtr pMga1;
    MessageType from;
    int i;
    char* s;
    ClockRangePtr clockRanges;
#ifdef USEMGAHAL
    ULONG status;
#endif
    MgaScrn2Rel Monitor2Pos;

    xf86DrvMsg(pScrn1->scrnIndex, X_INFO, "==== Start of second screen initialization ====\n");
    pScrn = xalloc(sizeof(ScrnInfoRec));
    memcpy(pScrn,pScrn1,sizeof(ScrnInfoRec));
   
    pScrn->driverPrivate = NULL; 
    /* Allocate the MGARec driverPrivate */
    if (!MGAGetRec(pScrn)) {
	return FALSE;
    }

    pMga = MGAPTR(pScrn);
#ifdef USEMGAHAL
    pMga->pMgaModeInfo = NULL; /*will be allocated later if NULL*/ 
#endif
    pMga1 = MGAPTR(pScrn1);
    pMga1->pScrn2 = pScrn;
  
    /* Get the entity, and make sure it is PCI. */
    pMga->pEnt = pMga1->pEnt;
    
    /* Set pMga->device to the relevant Device section */
    pMga->device = pMga1->device;
    
    if (flags & PROBE_DETECT) {
	MGAProbeDDC(pScrn, pMga->pEnt->index); /*FIXME make shure this probes second monitor */
	return TRUE;
    }

    pMga->PciTag = pMga1->PciTag;
   
    pMga->Primary = pMga1->Primary;

    /* Set pScrn->monitor */
    {
        pScrn->monitor = xalloc(sizeof(MonRec));
        /* copy everything we don't care about */
        memcpy(pScrn->monitor,pScrn1->monitor,sizeof(MonRec));
        pScrn->monitor->DDC = NULL;   /*FIXME:have to try this */ 
        if ((s = xf86GetOptValString(pMga1->Options, OPTION_HSYNC2))) { 
            pScrn->monitor->nHsync = StrToRanges(pScrn->monitor->hsync,s);
        }
        if ((s = xf86GetOptValString(pMga1->Options, OPTION_VREFRESH2))) { 
            pScrn->monitor->nVrefresh = StrToRanges(pScrn->monitor->vrefresh,s);
        }
    
    
    
   }
  
    pMga->SecondCrtc = TRUE;
    pMga->HWCursor = FALSE;
    pScrn->AdjustFrame = MGAAdjustMergeFrames;
    pScrn1->AdjustFrame = MGAAdjustMergeFrames;
    
/*    if (!xf86SetDepthBpp(pScrn, 8, 8, 8, flags24))   FIXME:have to copy result form scrn1 
  if (!xf86SetWeight(pScrn, zeros, zeros)) {
*/

    /* We use a programamble clock */
    pScrn->progClock = TRUE;

    /* Collect all of the relevant option flags (fill in pScrn->options) */
    pScrn->options = pScrn1->options;

/*   xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pMga->Options);*/
    pMga->Options = pMga1->Options;

    
    /* Set the bits per RGB for 8bpp mode */
    if (pScrn->depth == 8)
	pScrn->rgbBits = 8;

    /*
     * Set the Chipset and ChipRev, allowing config file entries to
     * override.
     */
    pScrn->chipset = pScrn1->chipset;
    pMga->Chipset = pMga1->Chipset;
    pMga->ChipRev = pMga1->ChipRev;

#ifdef XF86DRI
    pMga->agpMode = pMga1->agpMode;
#endif

    pMga->NoAccel = pMga1->NoAccel;
    pMga->UsePCIRetry = pMga1->UsePCIRetry;
    pMga->SyncOnGreen = pMga1->SyncOnGreen;
    pMga->ShowCache = pMga1->ShowCache;
    pMga->HasSDRAM = pMga1->HasSDRAM;
    pMga->MemClk = pMga1->MemClk;
    pMga->Overlay8Plus24 = pMga1->Overlay8Plus24;
    pMga->colorKey = pMga1->colorKey;
    pScrn->colorKey = pScrn1->colorKey;
    pScrn->overlayFlags = pScrn1->overlayFlags;
    pMga->videoKey = pMga1->videoKey;
    /* unsupported options */
    pMga->HWCursor = FALSE;
    pMga->ShadowFB = FALSE;
    pMga->FBDev = FALSE;
    
    pMga->OverclockMem = pMga1->OverclockMem;
    pMga->TexturedVideo = pMga1->TexturedVideo;
    pMga->MergedFB = TRUE;

    pMga->Rotate = 0;

    switch (pMga->Chipset) {
    case PCI_CHIP_MGA2064:
    case PCI_CHIP_MGA2164:
    case PCI_CHIP_MGA2164_AGP:
	MGA2064SetupFuncs(pScrn);
	break;
    case PCI_CHIP_MGA1064:
    case PCI_CHIP_MGAG100:
    case PCI_CHIP_MGAG100_PCI:
    case PCI_CHIP_MGAG200:
    case PCI_CHIP_MGAG200_PCI:
    case PCI_CHIP_MGAG400:
    case PCI_CHIP_MGAG550:
	MGAGSetupFuncs(pScrn);
	break;
    }

    pMga->FbAddress = pMga1->FbAddress;
    pMga->FbBaseReg = pMga1->FbBaseReg;
    pMga->PciInfo = pMga1->PciInfo;
    pMga->IOAddress = pMga1->IOAddress;
    pMga->ILOADAddress = pMga1->ILOADAddress;
    pMga->BiosFrom = pMga1->BiosFrom;
    pMga->BiosAddress = pMga1->BiosAddress;

    /*
     * Read the BIOS data struct
     */

    MGAReadBios(pScrn);

    /* HW bpp matches reported bpp */
    pMga->HwBpp = pMga1->HwBpp;

    /*
     * Reset card if it isn't primary one
     */
    if ( (!pMga->Primary && !pMga->FBDev) || xf86IsPc98() )
        MGASoftReset(pScrn);

    
    pScrn->videoRam = pScrn1->videoRam;
    pMga->FbMapSize = pMga1->FbMapSize;
    pMga->SrcOrg = pMga1->SrcOrg;
    pMga->DstOrg = pMga1->DstOrg;

   /* Set the bpp shift value */
    pMga->BppShifts[0] = 0;
    pMga->BppShifts[1] = 1;
    pMga->BppShifts[2] = 0;
    pMga->BppShifts[3] = 2;

    /*
     * fill MGAdac struct
     * Warning: currently, it should be after RAM counting
     */
    (*pMga->PreInit)(pScrn);

#if !defined(__powerpc__)

    /* Read and print the Monitor DDC info */
/*    pScrn->monitor->DDC = MGAdoDDC(pScrn);*/ /*FIXME: have to try this*/
#endif /* !__powerpc__ */

    /*
     * If the driver can do gamma correction, it should call xf86SetGamma()
     * here.
     */
    {
	Gamma zeros = {0.0, 0.0, 0.0};

	if (!xf86SetGamma(pScrn, zeros)) {
	    return FALSE;
	}
    }


    /* Set the min pixel clock */
    pMga->MinClock = pMga1->MinClock;	/* XXX Guess, need to check this */
    xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "CRTC2: Min pixel clock is %d MHz\n",
	       pMga->MinClock / 1000);
   /* Override on 2nd crtc */

    if (pMga->ChipRev >= 0x80) {	/* G450 */
        pMga->MaxClock = 234000;
    } else {
        pMga->MaxClock = 135000;
    }
    xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "CRTC2: Max pixel clock is %d MHz\n",
	       pMga->MaxClock / 1000);
    /*
     * Setup the ClockRanges, which describe what clock ranges are available,
     * and what sort of modes they can be used for.
     */
    clockRanges = xnfcalloc(sizeof(ClockRange), 1);
    clockRanges->next = NULL;
    clockRanges->minClock = pMga->MinClock;
    clockRanges->maxClock = pMga->MaxClock;
    clockRanges->clockIndex = -1;		/* programmable */
    clockRanges->interlaceAllowed = TRUE;
    clockRanges->doubleScanAllowed = TRUE;
#ifdef USEMGAHAL
    MGA_HAL(clockRanges->interlaceAllowed = FALSE);
    MGA_HAL(clockRanges->doubleScanAllowed = FALSE);
#endif
    clockRanges->interlaceAllowed = FALSE; /*no interlace on CRTC2 */

    clockRanges->ClockMulFactor = 1;
    clockRanges->ClockDivFactor = 1;
    /* Only set MemClk if appropriate for the ramdac */
    if (pMga->Dac.SetMemClk) {
	if (pMga->MemClk == 0) {
	    pMga->MemClk = pMga->Dac.MemoryClock;
	    from = pMga->Dac.MemClkFrom;
	} else
	    from = X_CONFIG;
	xf86DrvMsg(pScrn->scrnIndex, from, "CRTC2: MCLK used is %.1f MHz\n",
		   pMga->MemClk / 1000.0);
    }

    /*
     * xf86ValidateModes will check that the mode HTotal and VTotal values
     * don't exceed the chipset's limit if pScrn->maxHValue and
     * pScrn->maxVValue are set.  Since our MGAValidMode() already takes
     * care of this, we don't worry about setting them here.
     */
    {
	int Pitches1[] =
	  {640, 768, 800, 960, 1024, 1152, 1280, 1600, 1920, 2048, 0};
	int Pitches2[] =
	  {512, 640, 768, 800, 832, 960, 1024, 1152, 1280, 1600, 1664,
		1920, 2048, 0};
	int *linePitches = NULL;
	int minPitch = 256;
	int maxPitch = 2048;

        switch(pMga->Chipset) {
	case PCI_CHIP_MGA2064:
	   if (!pMga->NoAccel) {
		linePitches = xalloc(sizeof(Pitches1));
		memcpy(linePitches, Pitches1, sizeof(Pitches1));
		minPitch = maxPitch = 0;
	   }
	   break;
	case PCI_CHIP_MGA2164:
	case PCI_CHIP_MGA2164_AGP:
	case PCI_CHIP_MGA1064:
	   if (!pMga->NoAccel) {
		linePitches = xalloc(sizeof(Pitches2));
		memcpy(linePitches, Pitches2, sizeof(Pitches2));
		minPitch = maxPitch = 0;
	   }
	   break;
	case PCI_CHIP_MGAG100:
	case PCI_CHIP_MGAG100_PCI:
	   maxPitch = 2048;
	   break;
	case PCI_CHIP_MGAG200:
	case PCI_CHIP_MGAG200_PCI:
	case PCI_CHIP_MGAG400:
	case PCI_CHIP_MGAG550:
	   maxPitch = 4096;
	   break;
	}

        pScrn->modePool=NULL;
        pScrn->modes = NULL;
	i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
			      pScrn->display->modes, clockRanges,
			      linePitches, minPitch, maxPitch,
			      pMga->Roundings[(pScrn->bitsPerPixel >> 3) - 1] *
					pScrn->bitsPerPixel, 128, 2048,
			      pScrn->display->virtualX,
			      pScrn->display->virtualY,
			      pMga->FbMapSize,
			      LOOKUP_BEST_REFRESH);
        
	if (linePitches)
	   xfree(linePitches);
    }


    if (i < 1 && pMga->FBDev) {
	fbdevHWUseBuildinMode(pScrn);
	pScrn->displayWidth = pScrn->virtualX; /* FIXME: might be wrong */
	i = 1;
    }
    if (i == -1) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "CRTC2: Validate Modes Failed\n");
	MGAFreeRec(pScrn);
	return FALSE;
    }

    /* Prune the modes marked as invalid */
    xf86PruneDriverModes(pScrn);

    if (i == 0 || pScrn->modes == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "CRTC2: No valid modes found\n");
	MGAFreeRec(pScrn);
	return FALSE;
    }
#ifdef USEMGAHAL
    MGA_HAL(

    pMga->pBoard = pMga1->pBoard;
    pMga->pClientStruct = pMga1->pClientStruct;
    pMga->pMgaHwInfo = pMga1->pMgaHwInfo;


    MGAFillModeInfoStruct(pScrn,NULL);
    /* Fields usually handled by MGAFillModeInfoStruct, but are unavailable
     * because no mode is given
     */
    pMga->pMgaModeInfo->ulDispWidth = pScrn->virtualX;
    pMga->pMgaModeInfo->ulDispHeight = pScrn->virtualY;
    
    if((status = MGAValidateMode(pMga->pBoard,pMga->pMgaModeInfo)) != 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "MGAValidateMode from HALlib found the mode to be invalid.\n"
		   "\tError: 0x%lx\n", status);
        return FALSE;
    }
    pScrn->displayWidth = pMga->pMgaModeInfo->ulFBPitch;
    );	/* MGA_HAL */
#endif

    /*
     * Set the CRTC parameters for all of the modes based on the type
     * of mode, and the chipset's interlace requirements.
     *
     * Calling this is required if the mode->Crtc* values are used by the
     * driver and if the driver doesn't provide code to set them.  They
     * are not pre-initialised at all.
     */
#ifdef USEMGAHAL
    MGA_HAL(xf86SetCrtcForModes(pScrn, 0));
#endif
    MGA_NOT_HAL(xf86SetCrtcForModes(pScrn, INTERLACE_HALVE_V));

    /* Set the current mode to the first in the list */
    pScrn->currentMode = pScrn->modes;

    /* Print the list of modes being used */
    xf86PrintModes(pScrn);

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);

    /*
     * Compute the byte offset into the linear frame buffer where the
     * frame buffer data should actually begin.  According to DDK misc.c
     * line 1023, if more than 4MB is to be displayed, YDSTORG must be set
     * appropriately to align memory bank switching, and this requires a
     * corresponding offset on linear frame buffer access.
     * This is only needed for WRAM.
     */

    pMga->YDstOrg = pMga1->YDstOrg; 
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "CRTC2: YDstOrg is set to %d\n",
		   pMga->YDstOrg);
    pMga->FbUsableSize = pMga1->FbUsableSize;
    pMga->FbCursorOffset = pMga1->FbCursorOffset;

    pMga->CurrentLayout.bitsPerPixel = pScrn->bitsPerPixel;
    pMga->CurrentLayout.depth = pScrn->depth;
    pMga->CurrentLayout.displayWidth = pScrn->displayWidth;
    pMga->CurrentLayout.weight.red = pScrn->weight.red;
    pMga->CurrentLayout.weight.green = pScrn->weight.green;
    pMga->CurrentLayout.weight.blue = pScrn->weight.blue;
    pMga->CurrentLayout.Overlay8Plus24 = pMga->Overlay8Plus24;
    pMga->CurrentLayout.mode = pScrn->currentMode;


    Monitor2Pos = mgaRightOf;
    if ((s = xf86GetOptValString(pMga1->Options, OPTION_MONITOR2POS))) {
        switch(s[0]) {
            case 'L': case 'l': case 'G': case 'g':
                Monitor2Pos = mgaLeftOf;
                break;
            case 'R': case 'r': case 'D': case 'd':
                Monitor2Pos = mgaRightOf;
                break;

            case 'A': case 'a': case 'H': case 'h':
                Monitor2Pos = mgaAbove;
                break;

            case 'B': case 'b':
                Monitor2Pos = mgaBelow;
                break;

            case 'C': case 'c':
                Monitor2Pos = mgaClone;
                break;
            default:
                Monitor2Pos = mgaRightOf;
                break;
        } 
    }

    /* Fool xf86 into thinking we have huge modes */
    /* Keep the original values somewhere */
    pMga1->M1modes = pScrn1->modes;
    pMga1->M1currentMode = pScrn1->currentMode;
    /* make a copy of the mode list, so we can modify it. */
    if ((s = xf86GetOptValString(pMga1->Options, OPTION_METAMODES))) {
        pScrn1->modes = GenerateModeList(pScrn,s,pMga1->M1modes,pScrn->modes,Monitor2Pos); /*FIXME: free this list*/
        if(!pScrn1->modes) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Parse Error reading MetaModes, or No modes left.\n");
            return FALSE;
        }
            
        pScrn1->modes = pScrn1->modes->next;
        pScrn1->currentMode = pScrn1->modes;
    } else {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "MetaModes option missing.\n");
        return FALSE;
    }
    xf86DrvMsg(pScrn1->scrnIndex, X_INFO, "==== End of second screen initialization ====\n");
    return TRUE;
}

void
MGADisplayPowerManagementSetMerged(ScrnInfoPtr pScrn, int PowerManagementMode,
				  int flags)
{
    MGADisplayPowerManagementSet(pScrn,PowerManagementMode,flags);
    MGADisplayPowerManagementSetCrtc2(pScrn,PowerManagementMode,flags);
}

typedef struct _region {
    int x0,x1,y0,y1;
    } region;

static Bool
InRegion(int x, int y, region r) {
    return (r.x0 <= x) && (x < r.x1) && (r.y0 <= y) && (y < r.y1);
}


#define BOUND(test,low,hi) { \
    if(test < low) test = low; \
    if(test > hi) test = hi; }
#define REBOUND(low,hi,test) { \
    if(test < low) { \
        hi += test-low; \
        low = test; } \
    if(test > hi) { \
        low += test-hi; \
        hi = test; } }
 void 
MGAMergePointerMoved(int scrnIndex, int x, int y)
{
  ScrnInfoPtr   pScr = xf86Screens[scrnIndex];
  MGAPtr        pMga = MGAPTR(pScr);
  ScrnInfoPtr   pScr2 = pMga->pScrn2;

  region out,in1,in2,f2,f1;

  int deltax,deltay;

  /* for ease. */
  f1.x0 = pMga->M1frameX0;
  f1.x1 = pMga->M1frameX1+1;
  f1.y0 = pMga->M1frameY0;
  f1.y1 = pMga->M1frameY1+1;
  f2.x0 = pScr2->frameX0;
  f2.x1 = pScr2->frameX1+1;
  f2.y0 = pScr2->frameY0;
  f2.y1 = pScr2->frameY1+1;


  /*specify outer clipping region. crossing this causes all frames to move*/
  out.x0 = pScr->frameX0;
  out.x1 = pScr->frameX1+1;
  out.y0 = pScr->frameY0;
  out.y1 = pScr->frameY1+1;

  /* 
   * specify inner sliding window. beeing outsize both frames, and inside 
   * the outer cliping window, causes corresponding frame to slide
   */
  in1 = out;
  in2 = out;
  switch(((MergedDisplayModePtr)pScr->currentMode->Private)->Monitor2Pos) {
      case mgaLeftOf : 
          in1.x0 = f1.x0; 
          in2.x1 = f2.x1; 
          break;
      case mgaRightOf : 
          in1.x1 = f1.x1; 
          in2.x0 = f2.x0; 
          break;
      case mgaBelow : 
          in1.y1 = f1.y1; 
          in2.y0 = f2.y0; 
          break;
      case mgaAbove : 
          in1.y0 = f1.y0; 
          in2.y1 = f2.y1; 
          break;
      case mgaClone : 
          break;
      }

  
    deltay = 0;
    deltax = 0;
  
    if(InRegion(x,y,out)) {
        if( InRegion(x,y, in1) && !InRegion(x,y, f1) ) {
            REBOUND(f1.x0,f1.x1,x);
            REBOUND(f1.y0,f1.y1,y);
            deltax = 1; /*force frame update */
        }
        if( InRegion(x,y, in2) && !InRegion(x,y, f2) ) {
            REBOUND(f2.x0,f2.x1,x);
            REBOUND(f2.y0,f2.y1,y);
            deltax = 1; /*force frame update */
        }
    }
    else {  /*outside outer clipping region*/
        if ( out.x0 > x) { 
            deltax = x - out.x0;
        }
        if ( out.x1 < x) { 
            deltax = x - out.x1;
        }
        f1.x0 += deltax;
        f1.x1 += deltax;
        f2.x0 += deltax;
        f2.x1 += deltax;
        pScr->frameX0 += deltax;
        pScr->frameX1 += deltax;
        

        if ( out.y0 > y) { 
            deltay = y - out.y0;
        }
        if ( out.y1 < y) { 
            deltay = y - out.y1;
        }
        f1.y0 += deltay;
        f1.y1 += deltay;
        f2.y0 += deltay;
        f2.y1 += deltay;
        pScr->frameY0 += deltay;
        pScr->frameY1 += deltay;
    } 

    
    if (deltax != 0 || deltay != 0) {
        /* back to reality. */
        pMga->M1frameX0 = f1.x0; 
        pMga->M1frameY0 = f1.y0;
        pScr2->frameX0 = f2.x0;
        pScr2->frameY0 = f2.y0;

        /*Adjust Granularity */
        MGAAdjustGranularity(pScr,&pMga->M1frameX0,&pMga->M1frameY0);
        MGAAdjustGranularity(pScr,&pScr2->frameX0,&pScr2->frameY0);
        MGAAdjustGranularity(pScr,&pScr->frameX0,&pScr->frameY0);
        
        pMga->M1frameX1 = pMga->M1frameX0 + MDMPTR(pScr)->Monitor1->HDisplay -1;
        pMga->M1frameY1 = pMga->M1frameY0 + MDMPTR(pScr)->Monitor1->VDisplay -1;
        pScr2->frameX1 = pScr2->frameX0 + MDMPTR(pScr)->Monitor2->HDisplay -1;
        pScr2->frameY1 = pScr2->frameY0 + MDMPTR(pScr)->Monitor2->VDisplay -1;
        pScr->frameX1 = pScr->frameX0 + pScr->currentMode->HDisplay -1;
        pScr->frameY1 = pScr->frameY0 + pScr->currentMode->VDisplay -1;
        
        MGAAdjustFrame(pScr->scrnIndex, pMga->M1frameX0, pMga->M1frameY0, 0);
        MGAAdjustFrameCrtc2(pScr->scrnIndex, pScr2->frameX0, pScr2->frameY0, 0);
    }

/*  if(pMga->PointerMoved)  
      (*pMga->PointerMoved)(scrnIndex, x, y);  FIXME: do I need to call old func?*/

}

   
void
MGAAdjustMergeFrames(int scrnIndex, int x, int y, int flags) {
    ScrnInfoPtr pScrn1 = xf86Screens[scrnIndex];
    MGAPtr pMga = MGAPTR(pScrn1); 
    ScrnInfoPtr pScrn2 = pMga->pScrn2;
    int VTotal = pScrn1->currentMode->VDisplay; 
    int HTotal = pScrn1->currentMode->HDisplay; 
    int VMax = VTotal;
    int HMax = HTotal;

    BOUND(x,0,pScrn1->virtualX-HTotal);
    BOUND(y,0,pScrn1->virtualY-VTotal); 
    switch(MDMPTR(pScrn1)->Monitor2Pos) {
        case mgaLeftOf:
            pScrn2->frameX0 = x;
            BOUND(pScrn2->frameY0,y,y + VMax - MDMPTR(pScrn1)->Monitor2->VDisplay);
            pMga->M1frameX0 = x+MDMPTR(pScrn1)->Monitor2->HDisplay;
            BOUND(pMga->M1frameY0,y,y + VMax - MDMPTR(pScrn1)->Monitor1->VDisplay);
            break;
        case mgaRightOf:
            pMga->M1frameX0 = x;
            BOUND(pMga->M1frameY0,y,y + VMax - MDMPTR(pScrn1)->Monitor1->VDisplay);
            pScrn2->frameX0 = x+MDMPTR(pScrn1)->Monitor1->HDisplay;
            BOUND(pScrn2->frameY0,y,y + VMax - MDMPTR(pScrn1)->Monitor2->VDisplay);
            break;
        case mgaAbove:
            BOUND(pScrn2->frameX0,x,x + HMax - MDMPTR(pScrn1)->Monitor2->HDisplay);
            pScrn2->frameY0 = y;
            BOUND(pMga->M1frameX0,x,x + HMax - MDMPTR(pScrn1)->Monitor1->HDisplay);
            pMga->M1frameY0 = y+MDMPTR(pScrn1)->Monitor2->VDisplay;
            break;
        case mgaBelow:
            BOUND(pMga->M1frameX0,x,x + HMax - MDMPTR(pScrn1)->Monitor1->HDisplay);
            pMga->M1frameY0 = y;
            BOUND(pScrn2->frameX0,x,x + HMax - MDMPTR(pScrn1)->Monitor2->HDisplay);
            pScrn2->frameY0 = y+MDMPTR(pScrn1)->Monitor1->VDisplay;
            break;
        case mgaClone:
            BOUND(pMga->M1frameX0,x,x + HMax - MDMPTR(pScrn1)->Monitor1->HDisplay);
            BOUND(pMga->M1frameY0,y,y + VMax - MDMPTR(pScrn1)->Monitor1->VDisplay);
            BOUND(pScrn2->frameX0,x,x + HMax - MDMPTR(pScrn1)->Monitor2->HDisplay);
            BOUND(pScrn2->frameY0,y,y + VMax - MDMPTR(pScrn1)->Monitor2->VDisplay);
            break;
    }
    /* sanity checks. Make shure were not out of bounds */
    BOUND(pMga->M1frameX0,0,pScrn1->virtualX -MDMPTR(pScrn1)->Monitor1->HDisplay);
    BOUND(pMga->M1frameY0,0,pScrn1->virtualY -MDMPTR(pScrn1)->Monitor1->VDisplay);
    BOUND(pScrn2->frameX0,0,pScrn2->virtualX -MDMPTR(pScrn1)->Monitor2->HDisplay);
    BOUND(pScrn2->frameY0,0,pScrn2->virtualY -MDMPTR(pScrn1)->Monitor2->VDisplay);
    
    pScrn1->frameX0 = x;
    pScrn1->frameY0 = y;
    
    /* check granularity */
    MGAAdjustGranularity(pScrn1,&pMga->M1frameX0,&pMga->M1frameY0);
    MGAAdjustGranularity(pScrn1,&pScrn2->frameX0,&pScrn2->frameY0);
    MGAAdjustGranularity(pScrn1,&pScrn1->frameX0,&pScrn1->frameY0);
    
    /* complete shitty redundant info */
    pMga->M1frameX1 = pMga->M1frameX0 + MDMPTR(pScrn1)->Monitor1->HDisplay -1;
    pMga->M1frameY1 = pMga->M1frameY0 + MDMPTR(pScrn1)->Monitor1->VDisplay -1;
    pScrn2->frameX1 = pScrn2->frameX0 + MDMPTR(pScrn1)->Monitor2->HDisplay -1;
    pScrn2->frameY1 = pScrn2->frameY0 + MDMPTR(pScrn1)->Monitor2->VDisplay -1;
    pScrn1->frameX1 = pScrn1->frameX0 + pScrn1->currentMode->HDisplay -1;
    pScrn1->frameY1 = pScrn1->frameY0 + pScrn1->currentMode->VDisplay -1;

    MGAAdjustFrame(scrnIndex, pMga->M1frameX0, pMga->M1frameY0, flags);
    MGAAdjustFrameCrtc2(scrnIndex, pScrn2->frameX0, pScrn2->frameY0, flags);
    return;
}

Bool
MGACloseScreenMerged(int scrnIndex, ScreenPtr pScreen) {
    ScrnInfoPtr pScrn1 = xf86Screens[scrnIndex];
    MGAPtr pMga = MGAPTR(pScrn1); 
    ScrnInfoPtr pScrn2 = pMga->pScrn2;

    if(pScrn2) {
        xfree(pScrn2->monitor);
        pScrn2->monitor = NULL;

        xfree(pScrn2);
        pMga->pScrn2 = NULL;
    }

    if(pScrn1->modes) {
        pScrn1->currentMode = pScrn1->modes;
        do {
            DisplayModePtr p = pScrn1->currentMode->next; 
            if(pScrn1->currentMode->Private) 
                xfree(pScrn1->currentMode->Private);
            xfree(pScrn1->currentMode);
            pScrn1->currentMode = p;
        }while( pScrn1->currentMode != pScrn1->modes);
    }
 
    pScrn1->currentMode = pMga->M1currentMode;
    pScrn1->modes = pMga->M1modes;
        
    return TRUE;       
}

Bool
MGASaveScreenMerged(ScreenPtr pScreen, int mode)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    MGAPtr pMga = MGAPTR(pScrn); 
    BOOL on = xf86IsUnblank(mode);
    CARD8 reg;
    
    if (on) {
/*        SetTimdeSinceLastInputEvent();*/
       
        /* power on Dac1 */
        reg = inMGAdac(0x1E);
	outMGAdac(0x1E, reg | 1);
        /* power on Dac2 */
        reg = inMGAdac(0xA0);
	outMGAdac(0xA0, reg | 1);
        
    } else {
        /* power off Dac1 */
        reg = inMGAdac(0x1E);
	outMGAdac(0x1E, reg & ~1);
        /* power off Dac2 */
        reg = inMGAdac(0xA0);
	outMGAdac(0xA0, reg & ~1);

    }
    return TRUE;
}


