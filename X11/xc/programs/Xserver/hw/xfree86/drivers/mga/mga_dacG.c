/*
 * MGA-1064, MGA-G100, MGA-G200, MGA-G400, MGA-G550 RAMDAC driver
 */

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/mga/mga_dacG.c,v 1.51 2002/09/16 18:05:55 eich Exp $ */

/*
 * This is a first cut at a non-accelerated version to work with the
 * new server design (DHD).
 */                     

#include "colormapst.h"

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h" 

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"

/* Drivers that need to access the PCI config space directly need this */
#include "xf86Pci.h"

#include "mga_bios.h"
#include "mga_reg.h"
#include "mga.h"
#include "mga_macros.h"

#include "xf86DDC.h"


/*
 * implementation
 */
 
#define DACREGSIZE 0x50
    
/*
 * Only change bits shown in this mask.  Ideally reserved bits should be
 * zeroed here.  Also, don't change the vgaioen bit here since it is
 * controlled elsewhere.
 *
 * XXX These settings need to be checked.
 */
#define OPTION1_MASK	0xFFFFFEFF
#define OPTION2_MASK	0xFFFFFFFF
#define OPTION3_MASK	0xFFFFFFFF

#define OPTION1_MASK_PRIMARY	0xFFFC0FF

static void MGAGRamdacInit(ScrnInfoPtr);
static void MGAGSave(ScrnInfoPtr, vgaRegPtr, MGARegPtr, Bool);
static void MGAGRestore(ScrnInfoPtr, vgaRegPtr, MGARegPtr, Bool);
static Bool MGAGInit(ScrnInfoPtr, DisplayModePtr);
static void MGAGLoadPalette(ScrnInfoPtr, int, int*, LOCO*, VisualPtr);
static Bool MGAG_i2cInit(ScrnInfoPtr pScrn);

/*
 * MGAGCalcClock - Calculate the PLL settings (m, n, p, s).
 *
 * DESCRIPTION
 *   For more information, refer to the Matrox
 *   "MGA1064SG Developer Specification (document 10524-MS-0100).
 *     chapter 5.7.8. "PLLs Clocks Generators"
 *
 * PARAMETERS
 *   f_out		IN	Desired clock frequency.
 *   f_max		IN	Maximum allowed clock frequency.
 *   m			OUT	Value of PLL 'm' register.
 *   n			OUT	Value of PLL 'n' register.
 *   p			OUT	Value of PLL 'p' register.
 *   s			OUT	Value of PLL 's' filter register 
 *                              (pix pll clock only).
 *
 * HISTORY
 *   August 18, 1998 - Radoslaw Kapitan
 *   Adapted for G200 DAC
 *
 *   February 28, 1997 - Guy DESBIEF 
 *   Adapted for MGA1064SG DAC.
 *   based on MGACalcClock  written by [aem] Andrew E. Mileski
 */

/* The following values are in kHz */
#define MGA_MIN_VCO_FREQ     50000
#define MGA_MAX_VCO_FREQ    310000

static double
MGAGCalcClock ( ScrnInfoPtr pScrn, long f_out,
		int *best_m, int *best_n, int *p, int *s )
{
	MGAPtr pMga = MGAPTR(pScrn);
	int m, n;
	double f_pll, f_vco;
	double m_err, calc_f;
	double ref_freq;
	int feed_div_min, feed_div_max;
	int in_div_min, in_div_max;
	int post_div_max;
	
	switch( pMga->Chipset )
	{
	case PCI_CHIP_MGA1064:
		ref_freq     = 14318.18;
		feed_div_min = 100;
		feed_div_max = 127;
		in_div_min   = 1;
		in_div_max   = 31;
		post_div_max = 7;
		break;
	case PCI_CHIP_MGAG400:
	case PCI_CHIP_MGAG550:
		ref_freq     = 27050.5;
		feed_div_min = 7;
		feed_div_max = 127;
		in_div_min   = 1;
		in_div_max   = 31;
		post_div_max = 7;
		break;
	case PCI_CHIP_MGAG100:
	case PCI_CHIP_MGAG100_PCI:
	case PCI_CHIP_MGAG200:
	case PCI_CHIP_MGAG200_PCI:
	default:
		if (pMga->Bios2.PinID && (pMga->Bios2.VidCtrl & 0x20))
			ref_freq = 14318.18;
		else
			ref_freq = 27050.5;
		feed_div_min = 7;
		feed_div_max = 127;
		in_div_min   = 1;
		in_div_max   = 6;
		post_div_max = 7;
		break;
	}

	/* Make sure that f_min <= f_out */
	if ( f_out < ( MGA_MIN_VCO_FREQ / 8))
		f_out = MGA_MIN_VCO_FREQ / 8;

	/*
	 * f_pll = f_vco / (p+1)
	 * Choose p so that MGA_MIN_VCO_FREQ   <= f_vco <= MGA_MAX_VCO_FREQ  
	 * we don't have to bother checking for this maximum limit.
	 */
	f_vco = ( double ) f_out;
	for ( *p = 0; *p <= post_div_max && f_vco < MGA_MIN_VCO_FREQ;
		*p = *p * 2 + 1, f_vco *= 2.0);

	/* Initial amount of error for frequency maximum */
	m_err = f_out;

	/* Search for the different values of ( m ) */
	for ( m = in_div_min ; m <= in_div_max ; m++ )
	{
		/* see values of ( n ) which we can't use */
		for ( n = feed_div_min; n <= feed_div_max; n++ )
		{ 
			calc_f = ref_freq * (n + 1) / (m + 1) ;

			/*
			 * Pick the closest frequency.
			 */
			if ( abs(calc_f - f_vco) < m_err ) {
				m_err = abs(calc_f - f_vco);
				*best_m = m;
				*best_n = n;
			}
		}
	}
	
	/* Now all the calculations can be completed */
	f_vco = ref_freq * (*best_n + 1) / (*best_m + 1);

	/* Adjustments for filtering pll feed back */
	if ( (50000.0 <= f_vco)
	&& (f_vco < 100000.0) )
		*s = 0;	
	if ( (100000.0 <= f_vco)
	&& (f_vco < 140000.0) )
		*s = 1;	
	if ( (140000.0 <= f_vco)
	&& (f_vco < 180000.0) )
		*s = 2;	
	if ( (180000.0 <= f_vco) )
		*s = 3;	

	f_pll = f_vco / ( *p + 1 );

#ifdef DEBUG
	ErrorF( "f_out_requ =%ld f_pll_real=%.1f f_vco=%.1f n=0x%x m=0x%x p=0x%x s=0x%x\n",
		f_out, f_pll, f_vco, *best_n, *best_m, *p, *s );
#endif

	return f_pll;
}

/*
 * MGAGSetPCLK - Set the pixel (PCLK) clock.
 */
static void 
MGAGSetPCLK( ScrnInfoPtr pScrn, long f_out )
{
	MGAPtr pMga = MGAPTR(pScrn);
	MGARegPtr pReg = &pMga->ModeReg;

	/* Pixel clock values */
	int m, n, p, s;

	/* The actual frequency output by the clock */
	double f_pll;

	if(MGAISGx50(pMga)) {
	    pReg->Clock = f_out;
	    return;
	}

	/* Do the calculations for m, n, p and s */
	f_pll = MGAGCalcClock( pScrn, f_out, &m, &n, &p, &s );

	/* Values for the pixel clock PLL registers */
	pReg->DacRegs[ MGA1064_PIX_PLLC_M ] = m & 0x1F;
	pReg->DacRegs[ MGA1064_PIX_PLLC_N ] = n & 0x7F;
	pReg->DacRegs[ MGA1064_PIX_PLLC_P ] = (p & 0x07) | ((s & 0x03) << 3);
}

/*
 * MGAGInit 
 */
static Bool
MGAGInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
	/*
	 * initial values of the DAC registers
	 */
	const static unsigned char initDAC[] = {
	/* 0x00: */	   0,    0,    0,    0,    0,    0, 0x00,    0,
	/* 0x08: */	   0,    0,    0,    0,    0,    0,    0,    0,
	/* 0x10: */	   0,    0,    0,    0,    0,    0,    0,    0,
	/* 0x18: */	0x00,    0, 0xC9, 0xFF, 0xBF, 0x20, 0x1F, 0x20,
	/* 0x20: */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x28: */	0x00, 0x00, 0x00, 0x00,    0,    0,    0, 0x40,
	/* 0x30: */	0x00, 0xB0, 0x00, 0xC2, 0x34, 0x14, 0x02, 0x83,
	/* 0x38: */	0x00, 0x93, 0x00, 0x77, 0x00, 0x00, 0x00, 0x3A,
	/* 0x40: */	   0,    0,    0,    0,    0,    0,    0,    0,
	/* 0x48: */	   0,    0,    0,    0,    0,    0,    0,    0
	};

	int i, weight555 = FALSE;
	int hd, hs, he, ht, vd, vs, ve, vt, wd;
	int BppShift;
	MGAPtr pMga;
	MGARegPtr pReg;
	vgaRegPtr pVga;
	MGAFBLayout *pLayout;
	xMODEINFO ModeInfo;

	ModeInfo.ulDispWidth = mode->HDisplay;
        ModeInfo.ulDispHeight = mode->VDisplay;
        ModeInfo.ulFBPitch = mode->HDisplay; 
        ModeInfo.ulBpp = pScrn->bitsPerPixel;    
        ModeInfo.flSignalMode = 0;
        ModeInfo.ulPixClock = mode->Clock;
        ModeInfo.ulHFPorch = mode->HSyncStart - mode->HDisplay;
        ModeInfo.ulHSync = mode->HSyncEnd - mode->HSyncStart;
        ModeInfo.ulHBPorch = mode->HTotal - mode->HSyncEnd;
        ModeInfo.ulVFPorch = mode->VSyncStart - mode->VDisplay;
        ModeInfo.ulVSync = mode->VSyncEnd - mode->VSyncStart;
        ModeInfo.ulVBPorch = mode->VTotal - mode->VSyncEnd;

	pMga = MGAPTR(pScrn);
	pReg = &pMga->ModeReg;
	pVga = &VGAHWPTR(pScrn)->ModeReg;
	pLayout = &pMga->CurrentLayout;

	BppShift = pMga->BppShifts[(pLayout->bitsPerPixel >> 3) - 1];

	MGA_NOT_HAL(
	/* Allocate the DacRegs space if not done already */
	if (pReg->DacRegs == NULL) {
		pReg->DacRegs = xnfcalloc(DACREGSIZE, 1);
	}
	for (i = 0; i < DACREGSIZE; i++) {
	    pReg->DacRegs[i] = initDAC[i]; 
	}
	);	/* MGA_NOT_HAL */
	    
	switch(pMga->Chipset)
	{
	case PCI_CHIP_MGA1064:
		pReg->DacRegs[ MGA1064_SYS_PLL_M ] = 0x04;
		pReg->DacRegs[ MGA1064_SYS_PLL_N ] = 0x44;
		pReg->DacRegs[ MGA1064_SYS_PLL_P ] = 0x18;
		pReg->Option  = 0x5F094F21;
		pReg->Option2 = 0x00000000;
		break;
	case PCI_CHIP_MGAG100:
	case PCI_CHIP_MGAG100_PCI:
		pReg->DacRegs[ MGAGDAC_XVREFCTRL ] = 0x03;
		if(pMga->HasSDRAM) {
		    if(pMga->OverclockMem) {
                        /* 220 Mhz */
			pReg->DacRegs[ MGA1064_SYS_PLL_M ] = 0x06;
			pReg->DacRegs[ MGA1064_SYS_PLL_N ] = 0x38;
			pReg->DacRegs[ MGA1064_SYS_PLL_P ] = 0x18;
		    } else {
                        /* 203 Mhz */
			pReg->DacRegs[ MGA1064_SYS_PLL_M ] = 0x01;
			pReg->DacRegs[ MGA1064_SYS_PLL_N ] = 0x0E;
			pReg->DacRegs[ MGA1064_SYS_PLL_P ] = 0x18;
		    }
		    pReg->Option = 0x404991a9;
		} else {
		    if(pMga->OverclockMem) {
                        /* 143 Mhz */
			pReg->DacRegs[ MGA1064_SYS_PLL_M ] = 0x06;
			pReg->DacRegs[ MGA1064_SYS_PLL_N ] = 0x24;
			pReg->DacRegs[ MGA1064_SYS_PLL_P ] = 0x10;
		    } else {
		        /* 124 Mhz */
			pReg->DacRegs[ MGA1064_SYS_PLL_M ] = 0x04;
			pReg->DacRegs[ MGA1064_SYS_PLL_N ] = 0x16;
			pReg->DacRegs[ MGA1064_SYS_PLL_P ] = 0x08;
		    }
		    pReg->Option = 0x4049d121;
		}
		pReg->Option2 = 0x0000007;
		break;
	case PCI_CHIP_MGAG400:
	case PCI_CHIP_MGAG550:
#ifdef USEMGAHAL
	       MGA_HAL(break;);
#endif
	       if (MGAISGx50(pMga))
		       break;

	       if(pMga->Dac.maxPixelClock == 360000) {  /* G400 MAX */
	           if(pMga->OverclockMem) {
			/* 150/200  */
			pReg->DacRegs[ MGA1064_SYS_PLL_M ] = 0x05;
			pReg->DacRegs[ MGA1064_SYS_PLL_N ] = 0x42;
			pReg->DacRegs[ MGA1064_SYS_PLL_P ] = 0x18;
			pReg->Option3 = 0x019B8419;
			pReg->Option = 0x50574120;
		   } else {
			/* 125/166  */
			pReg->DacRegs[ MGA1064_SYS_PLL_M ] = 0x02;
			pReg->DacRegs[ MGA1064_SYS_PLL_N ] = 0x1B;
			pReg->DacRegs[ MGA1064_SYS_PLL_P ] = 0x18;
			pReg->Option3 = 0x019B8419;
			pReg->Option = 0x5053C120;
		   } 
		} else {
	           if(pMga->OverclockMem) {
			/* 125/166  */
			pReg->DacRegs[ MGA1064_SYS_PLL_M ] = 0x02;
			pReg->DacRegs[ MGA1064_SYS_PLL_N ] = 0x1B;
			pReg->DacRegs[ MGA1064_SYS_PLL_P ] = 0x18;
			pReg->Option3 = 0x019B8419;
			pReg->Option = 0x5053C120;
		   } else {
			/* 110/166  */
			pReg->DacRegs[ MGA1064_SYS_PLL_M ] = 0x13;
			pReg->DacRegs[ MGA1064_SYS_PLL_N ] = 0x7A;
			pReg->DacRegs[ MGA1064_SYS_PLL_P ] = 0x08;
			pReg->Option3 = 0x0190a421;
			pReg->Option = 0x50044120;
		   } 
		}
		if(pMga->HasSDRAM)
		   pReg->Option &= ~(1 << 14);
		pReg->Option2 = 0x01003000;
		break;
	case PCI_CHIP_MGAG200:
	case PCI_CHIP_MGAG200_PCI:
	default:
#ifdef USEMGAHAL
		MGA_HAL(break;);
#endif
		if(pMga->OverclockMem) {
                     /* 143 Mhz */
		    pReg->DacRegs[ MGA1064_SYS_PLL_M ] = 0x06;
		    pReg->DacRegs[ MGA1064_SYS_PLL_N ] = 0x24;
		    pReg->DacRegs[ MGA1064_SYS_PLL_P ] = 0x10;
		} else {
		    /* 124 Mhz */
		    pReg->DacRegs[ MGA1064_SYS_PLL_M ] = 0x04;
		    pReg->DacRegs[ MGA1064_SYS_PLL_N ] = 0x2D;
		    pReg->DacRegs[ MGA1064_SYS_PLL_P ] = 0x19;
		}
	        pReg->Option2 = 0x00008000;
		if(pMga->HasSDRAM)
		    pReg->Option = 0x40499121;
		else
		    pReg->Option = 0x4049cd21;
		break;
	}

	MGA_NOT_HAL(
	/* must always have the pci retries on but rely on 
	   polling to keep them from occuring */
	pReg->Option &= ~0x20000000;

	switch(pLayout->bitsPerPixel)
	{
	case 8:
		pReg->DacRegs[ MGA1064_MUL_CTL ] = MGA1064_MUL_CTL_8bits;
		break;
	case 16:
		pReg->DacRegs[ MGA1064_MUL_CTL ] = MGA1064_MUL_CTL_16bits;
		if ( (pLayout->weight.red == 5) && (pLayout->weight.green == 5)
					&& (pLayout->weight.blue == 5) ) {
		    weight555 = TRUE;
		    pReg->DacRegs[ MGA1064_MUL_CTL ] = MGA1064_MUL_CTL_15bits;
		}
		break;
	case 24:
		pReg->DacRegs[ MGA1064_MUL_CTL ] = MGA1064_MUL_CTL_24bits;
		break;
	case 32:
		if(pLayout->Overlay8Plus24) {
		   pReg->DacRegs[ MGA1064_MUL_CTL ] = MGA1064_MUL_CTL_32bits;
		   pReg->DacRegs[ MGA1064_COL_KEY_MSK_LSB ] = 0xFF;
		   pReg->DacRegs[ MGA1064_COL_KEY_LSB ] = pMga->colorKey;
		} else 
		   pReg->DacRegs[ MGA1064_MUL_CTL ] = MGA1064_MUL_CTL_32_24bits;
		break;
	default:
		FatalError("MGA: unsupported depth\n");
	}
	);	/* MGA_NOT_HAL */
		
	/*
	 * This will initialize all of the generic VGA registers.
	 */
	if (!vgaHWInit(pScrn, mode))
		return(FALSE);

	/*
	 * Here all of the MGA registers get filled in.
	 */
	hd = (mode->CrtcHDisplay	>> 3)	- 1;
	hs = (mode->CrtcHSyncStart	>> 3)	- 1;
	he = (mode->CrtcHSyncEnd	>> 3)	- 1;
	ht = (mode->CrtcHTotal		>> 3)	- 1;
	vd = mode->CrtcVDisplay			- 1;
	vs = mode->CrtcVSyncStart		- 1;
	ve = mode->CrtcVSyncEnd			- 1;
	vt = mode->CrtcVTotal			- 2;
	
	/* HTOTAL & 0x7 equal to 0x6 in 8bpp or 0x4 in 24bpp causes strange
	 * vertical stripes
	 */  
	if((ht & 0x07) == 0x06 || (ht & 0x07) == 0x04)
		ht++;
		
	if (pLayout->bitsPerPixel == 24)
		wd = (pLayout->displayWidth * 3) >> (4 - BppShift);
	else
		wd = pLayout->displayWidth >> (4 - BppShift);

	pReg->ExtVga[0] = 0;
	pReg->ExtVga[5] = 0;
	
	if (mode->Flags & V_INTERLACE)
	{
		pReg->ExtVga[0] = 0x80;
		pReg->ExtVga[5] = (hs + he - ht) >> 1;
		wd <<= 1;
		vt &= 0xFFFE;
	}

	pReg->ExtVga[0]	|= (wd & 0x300) >> 4;
	pReg->ExtVga[1]	= (((ht - 4) & 0x100) >> 8) |
				((hd & 0x100) >> 7) |
				((hs & 0x100) >> 6) |
				(ht & 0x40);
	pReg->ExtVga[2]	= ((vt & 0xc00) >> 10) |
				((vd & 0x400) >> 8) |
				((vd & 0xc00) >> 7) |
				((vs & 0xc00) >> 5);
	if (pLayout->bitsPerPixel == 24)
		pReg->ExtVga[3]	= (((1 << BppShift) * 3) - 1) | 0x80;
	else
		pReg->ExtVga[3]	= ((1 << BppShift) - 1) | 0x80;

	pReg->ExtVga[4]	= 0;
		
	pVga->CRTC[0]	= ht - 4;
	pVga->CRTC[1]	= hd;
	pVga->CRTC[2]	= hd;
	pVga->CRTC[3]	= (ht & 0x1F) | 0x80;
	pVga->CRTC[4]	= hs;
	pVga->CRTC[5]	= ((ht & 0x20) << 2) | (he & 0x1F);
	pVga->CRTC[6]	= vt & 0xFF;
	pVga->CRTC[7]	= ((vt & 0x100) >> 8 ) |
				((vd & 0x100) >> 7 ) |
				((vs & 0x100) >> 6 ) |
				((vd & 0x100) >> 5 ) |
				0x10 |
				((vt & 0x200) >> 4 ) |
				((vd & 0x200) >> 3 ) |
				((vs & 0x200) >> 2 );
	pVga->CRTC[9]	= ((vd & 0x200) >> 4) | 0x40; 
	pVga->CRTC[16] = vs & 0xFF;
	pVga->CRTC[17] = (ve & 0x0F) | 0x20;
	pVga->CRTC[18] = vd & 0xFF;
	pVga->CRTC[19] = wd & 0xFF;
	pVga->CRTC[21] = vd & 0xFF;
	pVga->CRTC[22] = (vt + 1) & 0xFF;

	MGA_NOT_HAL(pReg->DacRegs[MGA1064_CURSOR_BASE_ADR_LOW] = pMga->FbCursorOffset >> 10);
	MGA_NOT_HAL(pReg->DacRegs[MGA1064_CURSOR_BASE_ADR_HI] = pMga->FbCursorOffset >> 18);
	
	if (pMga->SyncOnGreen) {
	    MGA_NOT_HAL(pReg->DacRegs[MGA1064_GEN_CTL] &= ~0x20);
	    pReg->ExtVga[3] |= 0x40;
	}

	/* select external clock */
	pVga->MiscOutReg |= 0x0C; 

	MGA_NOT_HAL(
	if (mode->Flags & V_DBLSCAN)
		pVga->CRTC[9] |= 0x80;

	if(MGAISGx50(pMga)) {
		OUTREG(MGAREG_ZORG, 0);
	}

  	MGAGSetPCLK(pScrn, mode->Clock);
	);	/* MGA_NOT_HAL */

	/* This disables the VGA memory aperture */
	pVga->MiscOutReg &= ~0x02;

	/* Urgh. Why do we define our own xMODEINFO structure instead 
	 * of just passing the blinkin' DisplayModePtr? If we're going to
	 * just cut'n'paste routines from the HALlib, it would be better
	 * just to strip the MacroVision stuff out of the HALlib and release
	 * that, surely?
	 */
        /*********************  Second Crtc programming **************/
        /* Writing values to crtc2[] array */
        if (pMga->SecondCrtc)
        {
            MGACRTC2Get(pScrn, &ModeInfo); 
            MGACRTC2GetPitch(pScrn, &ModeInfo); 
            MGACRTC2GetDisplayStart(pScrn, &ModeInfo,0,0,0);
        }
	return(TRUE);
}

/*
 * MGAGLoadPalette
 */

static void
MGAPaletteLoadCallback(ScrnInfoPtr pScrn)
{
    MGAPtr pMga = MGAPTR(pScrn);
    MGAPaletteInfo *pal = pMga->palinfo;
    int i;

    while (!(INREG8(0x1FDA) & 0x08)); 

    for(i = 0; i < 256; i++) {
	if(pal->update) {
	    outMGAdreg(MGA1064_WADR_PAL, i);
            outMGAdreg(MGA1064_COL_PAL, pal->red);
            outMGAdreg(MGA1064_COL_PAL, pal->green);
            outMGAdreg(MGA1064_COL_PAL, pal->blue);
	    pal->update = FALSE;
	}
	pal++;
    }
    pMga->PaletteLoadCallback = NULL;
}

void MGAGLoadPalette(
    ScrnInfoPtr pScrn, 
    int numColors, 
    int *indices,
    LOCO *colors,
    VisualPtr pVisual
){
    MGAPtr pMga = MGAPTR(pScrn);

    if((pMga->CurrentLayout.Overlay8Plus24) && (pVisual->nplanes != 8)) 
	return;

     if(pMga->Chipset == PCI_CHIP_MGAG400 || pMga->Chipset == PCI_CHIP_MGAG550){ 
	 /* load them at the retrace in the block handler instead to 
	    work around some problems with static on the screen */
	while(numColors--) {
	    pMga->palinfo[*indices].update = TRUE;
	    pMga->palinfo[*indices].red   = colors[*indices].red;
	    pMga->palinfo[*indices].green = colors[*indices].green;
	    pMga->palinfo[*indices].blue  = colors[*indices].blue;
	    indices++;
	}
	pMga->PaletteLoadCallback = MGAPaletteLoadCallback;
	return;
    } else {
	while(numColors--) {
            outMGAdreg(MGA1064_WADR_PAL, *indices);
            outMGAdreg(MGA1064_COL_PAL, colors[*indices].red);
            outMGAdreg(MGA1064_COL_PAL, colors[*indices].green);
            outMGAdreg(MGA1064_COL_PAL, colors[*indices].blue);
	    indices++;
	}
    }
}

/*
 * MGAGRestorePalette
 */

static void
MGAGRestorePalette(ScrnInfoPtr pScrn, unsigned char* pntr)
{
    MGAPtr pMga = MGAPTR(pScrn);
    int i = 768;

    outMGAdreg(MGA1064_WADR_PAL, 0x00);
    while(i--)
	outMGAdreg(MGA1064_COL_PAL, *(pntr++));
}

/*
 * MGAGSavePalette
 */
static void
MGAGSavePalette(ScrnInfoPtr pScrn, unsigned char* pntr)
{
    MGAPtr pMga = MGAPTR(pScrn);
    int i = 768;

    outMGAdreg(MGA1064_RADR_PAL, 0x00);
    while(i--) 
	*(pntr++) = inMGAdreg(MGA1064_COL_PAL);
}

/*
 * MGAGRestore
 *
 * This function restores a video mode.	 It basically writes out all of
 * the registers that have previously been saved.
 */
static void 
MGAGRestore(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, MGARegPtr mgaReg,
	       Bool restoreFonts)
{
	int i;
	MGAPtr pMga = MGAPTR(pScrn);
	CARD32 optionMask;

	/*
	 * Pixel Clock needs to be restored regardless if we use
	 * HALLib or not. HALlib doesn't do a good job restoring
	 * VESA modes. MATROX: hint, hint.
	 */
	if (MGAISGx50(pMga) && mgaReg->Clock) {
	    /* 
	     * With HALlib program only when restoring to console!
	     * To test this we check for Clock == 0.
	     */
	    MGAG450SetPLLFreq(pScrn, mgaReg->Clock);
	    mgaReg->PIXPLLCSaved = FALSE;
	}

        if(!pMga->SecondCrtc) {

MGA_NOT_HAL(
	   /*
	    * Code is needed to get things back to bank zero.
	    */
	   
	   /* restore DAC registers 
	    * according to the docs we shouldn't write to reserved regs*/
	   for (i = 0; i < DACREGSIZE; i++) {
	      if( (i <= 0x03) ||
		  (i == 0x07) ||
		  (i == 0x0b) ||
		  (i == 0x0f) ||
		  ((i >= 0x13) && (i <= 0x17)) ||
		  (i == 0x1b) ||
		  (i == 0x1c) ||
		  ((i >= 0x1f) && (i <= 0x29)) ||
		  ((i >= 0x30) && (i <= 0x37)) ||
                  (MGAISGx50(pMga) && !mgaReg->PIXPLLCSaved &&
		   ((i == 0x2c) || (i == 0x2d) || (i == 0x2e) ||
		    (i == 0x4c) || (i == 0x4d) || (i == 0x4e))))
		 continue; 
	      outMGAdac(i, mgaReg->DacRegs[i]);
	   }
	   
	   /* Do not set the memory config for primary cards as it
	      should be correct already */
	   optionMask = (pMga->Primary) ? OPTION1_MASK_PRIMARY : OPTION1_MASK; 
	   
	   if (!MGAISGx50(pMga)) {
	      /* restore pci_option register */
	      pciSetBitsLong(pMga->PciTag, PCI_OPTION_REG, optionMask,
			     mgaReg->Option);
	      if (pMga->Chipset != PCI_CHIP_MGA1064)
		 pciSetBitsLong(pMga->PciTag, PCI_MGA_OPTION2, OPTION2_MASK,
				mgaReg->Option2);
	      if (pMga->Chipset == PCI_CHIP_MGAG400 || pMga->Chipset == PCI_CHIP_MGAG550)
		 pciSetBitsLong(pMga->PciTag, PCI_MGA_OPTION3, OPTION3_MASK,
				mgaReg->Option3);
	   }
);	/* MGA_NOT_HAL */
#ifdef USEMGAHAL
          /* 
	   * Work around another bug in HALlib: it doesn't restore the
	   * DAC width register correctly. MATROX: hint, hint.
	   */
           MGA_HAL(	 
    	       outMGAdac(MGA1064_MUL_CTL,mgaReg->DacRegs[0]);
  	       outMGAdac(MGA1064_MISC_CTL,mgaReg->DacRegs[1]); 
	       if (!MGAISGx50(pMga)) {
		   outMGAdac(MGA1064_PIX_PLLC_M,mgaReg->DacRegs[2]);
		   outMGAdac(MGA1064_PIX_PLLC_N,mgaReg->DacRegs[3]);
		   outMGAdac(MGA1064_PIX_PLLC_P,mgaReg->DacRegs[4]);
	       } 
	       ); 
#endif
	   /* restore CRTCEXT regs */
           for (i = 0; i < 6; i++)
	      OUTREG16(0x1FDE, (mgaReg->ExtVga[i] << 8) | i);

	   /*
	    * This function handles restoring the generic VGA registers.
	    */
	   vgaHWRestore(pScrn, vgaReg,
			VGA_SR_MODE | (restoreFonts ? VGA_SR_FONTS : 0));
  	   MGAGRestorePalette(pScrn, vgaReg->DAC); 
	   
	   /*
	    * this is needed to properly restore start address
	    */
	   OUTREG16(0x1FDE, (mgaReg->ExtVga[0] << 8) | 0);
	} else {
	   /* Second Crtc */
	   xMODEINFO ModeInfo;

MGA_NOT_HAL(
	   /* Enable Dual Head */
	   MGACRTC2Set(pScrn, &ModeInfo); 
	   MGAEnableSecondOutPut(pScrn, &ModeInfo); 
	   MGACRTC2SetPitch(pScrn, &ModeInfo); 
	   MGACRTC2SetDisplayStart(pScrn, &ModeInfo,0,0,0);
            
	   for (i = 0x80; i <= 0xa0; i ++) {
                if (i== 0x8d) {
		   i = 0x8f;
		   continue;
		}
                outMGAdac(i,   mgaReg->dac2[ i - 0x80]);
	   }
); /* MGA_NOT_HAL */

        } 

#ifdef DEBUG		
	ErrorF("Setting DAC:");
	for (i=0; i<DACREGSIZE; i++) {
#if 1
		if(!(i%16)) ErrorF("\n%02X: ",i);
		ErrorF("%02X ", mgaReg->DacRegs[i]);
#else
		if(!(i%8)) ErrorF("\n%02X: ",i);
		ErrorF("0x%02X, ", mgaReg->DacRegs[i]);
#endif
	}
	ErrorF("\nOPTION  = %08lX\n", mgaReg->Option);
	ErrorF("OPTION2 = %08lX\n", mgaReg->Option2);
	ErrorF("CRTCEXT:");
	for (i=0; i<6; i++) ErrorF(" %02X", mgaReg->ExtVga[i]);
	ErrorF("\n");
#endif
	
}

/*
 * MGAGSave
 *
 * This function saves the video state.
 */
static void
MGAGSave(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, MGARegPtr mgaReg,
	    Bool saveFonts)
{
	int i;
	MGAPtr pMga = MGAPTR(pScrn);

	/*
	 * Pixel Clock needs to be restored regardless if we use
	 * HALLib or not. HALlib doesn't do a good job restoring
	 * VESA modes (s.o.). MATROX: hint, hint.
	 */
	if (MGAISGx50(pMga)) {
	    mgaReg->Clock = MGAG450SavePLLFreq(pScrn);
	}

	if(pMga->SecondCrtc == TRUE) {
	   for(i = 0x80; i < 0xa0; i++)
	      mgaReg->dac2[i-0x80] = inMGAdac(i);

	   return;
	}

	MGA_NOT_HAL(
	/* Allocate the DacRegs space if not done already */
	if (mgaReg->DacRegs == NULL) {
		mgaReg->DacRegs = xnfcalloc(DACREGSIZE, 1);
	}
	);	/* MGA_NOT_HAL */

	/*
	 * Code is needed to get back to bank zero.
	 */
	OUTREG16(0x1FDE, 0x0004);
	
	/*
	 * This function will handle creating the data structure and filling
	 * in the generic VGA portion.
	 */
	vgaHWSave(pScrn, vgaReg, VGA_SR_MODE | (saveFonts ? VGA_SR_FONTS : 0));
	MGAGSavePalette(pScrn, vgaReg->DAC);
	/* 
	 * Work around another bug in HALlib: it doesn't restore the
	 * DAC width register correctly.
	 */

#ifdef USEMGAHAL
	/* 
	 * Work around another bug in HALlib: it doesn't restore the
	 * DAC width register correctly (s.o.). MATROX: hint, hint.
	 */
  	MGA_HAL(
  	    if (mgaReg->DacRegs == NULL) {
  		mgaReg->DacRegs = xnfcalloc(MGAISGx50(pMga) ? 2 : 5, 1);
  	    }
    	    mgaReg->DacRegs[0] = inMGAdac(MGA1064_MUL_CTL);
  	    mgaReg->DacRegs[1] = inMGAdac(MGA1064_MISC_CTL);
	    if (!MGAISGx50(pMga)) {
		mgaReg->DacRegs[2] = inMGAdac(MGA1064_PIX_PLLC_M);
		mgaReg->DacRegs[3] = inMGAdac(MGA1064_PIX_PLLC_N);
		mgaReg->DacRegs[4] = inMGAdac(MGA1064_PIX_PLLC_P);
	    } 
  	);
#endif
	MGA_NOT_HAL(
	/*
	 * The port I/O code necessary to read in the extended registers.
	 */
	for (i = 0; i < DACREGSIZE; i++)
		mgaReg->DacRegs[i] = inMGAdac(i);

        mgaReg->PIXPLLCSaved = TRUE;

	mgaReg->Option = pciReadLong(pMga->PciTag, PCI_OPTION_REG);

	mgaReg->Option2 = pciReadLong(pMga->PciTag, PCI_MGA_OPTION2);
	if (pMga->Chipset == PCI_CHIP_MGAG400 || pMga->Chipset == PCI_CHIP_MGAG550)
	    mgaReg->Option3 = pciReadLong(pMga->PciTag, PCI_MGA_OPTION3);
	);	/* MGA_NOT_HAL */

	for (i = 0; i < 6; i++)
	{
		OUTREG8(0x1FDE, i);
		mgaReg->ExtVga[i] = INREG8(0x1FDF);
	}
#ifdef DEBUG		
	ErrorF("Saved values:\nDAC:");
	for (i=0; i<DACREGSIZE; i++) {
#if 1
		if(!(i%16)) ErrorF("\n%02X: ",i);
		ErrorF("%02X ", mgaReg->DacRegs[i]);
#else
		if(!(i%8)) ErrorF("\n%02X: ",i);
		ErrorF("0x%02X, ", mgaReg->DacRegs[i]);
#endif
	}
	ErrorF("\nOPTION  = %08lX\n:", mgaReg->Option);
	ErrorF("OPTION2 = %08lX\nCRTCEXT:", mgaReg->Option2);
	for (i=0; i<6; i++) ErrorF(" %02X", mgaReg->ExtVga[i]);
	ErrorF("\n");
#endif
}

/****
 ***  HW Cursor
 */
static void
MGAGLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
    MGAPtr pMga = MGAPTR(pScrn);
    CARD32 *dst = (CARD32*)(pMga->FbBase + pMga->FbCursorOffset);
    int i = 128;
    
    /* swap bytes in each line */
    while( i-- ) {
        *dst++ = (src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7];
        *dst++ = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
        src += 8;
    }
}

static void 
MGAGShowCursor(ScrnInfoPtr pScrn)
{
    MGAPtr pMga = MGAPTR(pScrn);
    /* Enable cursor - X-Windows mode */
    outMGAdac(MGA1064_CURSOR_CTL, 0x03);
}

static void 
MGAGShowCursorG100(ScrnInfoPtr pScrn)
{
    MGAPtr pMga = MGAPTR(pScrn);
    /* Enable cursor - X-Windows mode */
    outMGAdac(MGA1064_CURSOR_CTL, 0x01);
}

static void
MGAGHideCursor(ScrnInfoPtr pScrn)
{
    MGAPtr pMga = MGAPTR(pScrn);
    /* Disable cursor */
    outMGAdac(MGA1064_CURSOR_CTL, 0x00);
}

static void
MGAGSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    MGAPtr pMga = MGAPTR(pScrn);
    x += 64;
    y += 64;
#ifdef USEMGAHAL
    MGA_HAL(
	    x += pMga->HALGranularityOffX;
	    y += pMga->HALGranularityOffY;
    );
#endif
    /* cursor update must never occurs during a retrace period (pp 4-160) */
    while( INREG( MGAREG_Status ) & 0x08 );
    
    /* Output position - "only" 12 bits of location documented */
    OUTREG8( RAMDAC_OFFSET + MGA1064_CUR_XLOW, (x & 0xFF));
    OUTREG8( RAMDAC_OFFSET + MGA1064_CUR_XHI, (x & 0xF00) >> 8);
    OUTREG8( RAMDAC_OFFSET + MGA1064_CUR_YLOW, (y & 0xFF));
    OUTREG8( RAMDAC_OFFSET + MGA1064_CUR_YHI, (y & 0xF00) >> 8);
}


static void
MGAGSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    MGAPtr pMga = MGAPTR(pScrn);

    /* Background color */
    outMGAdac(MGA1064_CURSOR_COL0_RED,   (bg & 0x00FF0000) >> 16);
    outMGAdac(MGA1064_CURSOR_COL0_GREEN, (bg & 0x0000FF00) >> 8);
    outMGAdac(MGA1064_CURSOR_COL0_BLUE,  (bg & 0x000000FF));

    /* Foreground color */
    outMGAdac(MGA1064_CURSOR_COL1_RED,   (fg & 0x00FF0000) >> 16);
    outMGAdac(MGA1064_CURSOR_COL1_GREEN, (fg & 0x0000FF00) >> 8);
    outMGAdac(MGA1064_CURSOR_COL1_BLUE,  (fg & 0x000000FF));
}

static void
MGAGSetCursorColorsG100(ScrnInfoPtr pScrn, int bg, int fg)
{
    MGAPtr pMga = MGAPTR(pScrn);

    /* Background color */
    outMGAdac(MGA1064_CURSOR_COL1_RED,   (bg & 0x00FF0000) >> 16);
    outMGAdac(MGA1064_CURSOR_COL1_GREEN, (bg & 0x0000FF00) >> 8);
    outMGAdac(MGA1064_CURSOR_COL1_BLUE,  (bg & 0x000000FF));

    /* Foreground color */
    outMGAdac(MGA1064_CURSOR_COL2_RED,   (fg & 0x00FF0000) >> 16);
    outMGAdac(MGA1064_CURSOR_COL2_GREEN, (fg & 0x0000FF00) >> 8);
    outMGAdac(MGA1064_CURSOR_COL2_BLUE,  (fg & 0x000000FF));
}

static Bool 
MGAGUseHWCursor(ScreenPtr pScrn, CursorPtr pCurs)
{
    MGAPtr pMga = MGAPTR(xf86Screens[pScrn->myNum]);
   /* This needs to detect if its on the second dac */
    if( XF86SCRNINFO(pScrn)->currentMode->Flags & V_DBLSCAN )
    	return FALSE;
    if( pMga->SecondCrtc == TRUE )
     	return FALSE;
    return TRUE;
}


/*
 * According to mga-1064g.pdf pp215-216 (4-179 & 4-180) the low bits of
 * XGENIODATA and XGENIOCTL are connected to the 4 DDC pins, but don't say
 * which VGA line is connected to each DDC pin, so I've had to guess.
 *
 * DDC1 support only requires DDC_SDA_MASK,
 * DDC2 support reuqiers DDC_SDA_MASK and DDC_SCL_MASK
 */
static const int DDC_SDA_MASK = 1 << 1;
static const int DDC_SCL_MASK = 1 << 3;

static unsigned int
MGAG_ddc1Read(ScrnInfoPtr pScrn)
{
  MGAPtr pMga = MGAPTR(pScrn);
  unsigned char val;
  
  /* Define the SDA as an input */
  outMGAdacmsk(MGA1064_GEN_IO_CTL, ~(DDC_SCL_MASK | DDC_SDA_MASK), 0);

  /* wait for Vsync */
  while( INREG( MGAREG_Status ) & 0x08 );
  while( ! (INREG( MGAREG_Status ) & 0x08) );

  /* Get the result */
  val = (inMGAdac(MGA1064_GEN_IO_DATA) & DDC_SDA_MASK);
  return val;
}

static void
MGAG_I2CGetBits(I2CBusPtr b, int *clock, int *data) 
{
  MGAPtr pMga = MGAPTR(xf86Screens[b->scrnIndex]);
  unsigned char val;
  
   /* Get the result. */
   val = inMGAdac(MGA1064_GEN_IO_DATA);
   
   *clock = (val & DDC_SCL_MASK) != 0;
   *data  = (val & DDC_SDA_MASK) != 0;
#ifdef DEBUG
  ErrorF("MGAG_I2CGetBits(%p,...) val=0x%x, returns clock %d, data %d\n", b, val, *clock, *data);
#endif
}

/*
 * ATTENTION! - the DATA and CLOCK lines need to be tri-stated when
 * high. Therefore turn off output driver for the line to set line
 * to high. High signal is maintained by a 15k Ohm pll-up resistor.
 */
static void
MGAG_I2CPutBits(I2CBusPtr b, int clock, int data)
{
  MGAPtr pMga = MGAPTR(xf86Screens[b->scrnIndex]);
  unsigned char drv, val;

  val = (clock ? DDC_SCL_MASK : 0) | (data ? DDC_SDA_MASK : 0);
  drv = ((!clock) ? DDC_SCL_MASK : 0) | ((!data) ? DDC_SDA_MASK : 0);

  /* Write the values */
  outMGAdacmsk(MGA1064_GEN_IO_CTL, ~(DDC_SCL_MASK | DDC_SDA_MASK) , drv);
  outMGAdacmsk(MGA1064_GEN_IO_DATA, ~(DDC_SCL_MASK | DDC_SDA_MASK) , val);
#ifdef DEBUG
  ErrorF("MGAG_I2CPutBits(%p, %d, %d) val=0x%x\n", b, clock, data, val);
#endif
}


Bool
MGAG_i2cInit(ScrnInfoPtr pScrn)
{
    MGAPtr pMga = MGAPTR(pScrn);
    I2CBusPtr I2CPtr;

    I2CPtr = xf86CreateI2CBusRec();
    if(!I2CPtr) return FALSE;

    pMga->I2C = I2CPtr;

    I2CPtr->BusName    = "DDC";
    I2CPtr->scrnIndex  = pScrn->scrnIndex;
    I2CPtr->I2CPutBits = MGAG_I2CPutBits;
    I2CPtr->I2CGetBits = MGAG_I2CGetBits;
    I2CPtr->AcknTimeout = 5;

    if (!xf86I2CBusInit(I2CPtr)) {
	return FALSE;
    }
    return TRUE;
}


/*
 * MGAGRamdacInit
 * Handle broken G100 special.
 */
static void
MGAGRamdacInit(ScrnInfoPtr pScrn)
{
    MGAPtr pMga = MGAPTR(pScrn);
    MGARamdacPtr MGAdac = &pMga->Dac;

    MGAdac->isHwCursor             = TRUE;
    MGAdac->CursorOffscreenMemSize = 1024;
    MGAdac->CursorMaxWidth         = 64;
    MGAdac->CursorMaxHeight        = 64;
    MGAdac->SetCursorPosition      = MGAGSetCursorPosition;
    MGAdac->LoadCursorImage        = MGAGLoadCursorImage;
    MGAdac->HideCursor             = MGAGHideCursor;
    if ((pMga->Chipset == PCI_CHIP_MGAG100) 
	|| (pMga->Chipset == PCI_CHIP_MGAG100)) {
      MGAdac->SetCursorColors        = MGAGSetCursorColorsG100;
      MGAdac->ShowCursor             = MGAGShowCursorG100;
    } else {
      MGAdac->SetCursorColors        = MGAGSetCursorColors;
      MGAdac->ShowCursor             = MGAGShowCursor;
    }
    MGAdac->UseHWCursor            = MGAGUseHWCursor;
    MGAdac->CursorFlags            =
#if X_BYTE_ORDER == X_LITTLE_ENDIAN
    				HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
#endif
    				HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_64 |
    				HARDWARE_CURSOR_TRUECOLOR_AT_8BPP;

    MGAdac->LoadPalette 	   = MGAGLoadPalette;

    if ( pMga->Bios2.PinID && pMga->Bios2.PclkMax != 0xFF )
    {
	MGAdac->maxPixelClock = (pMga->Bios2.PclkMax + 100) * 1000;
	MGAdac->ClockFrom = X_PROBED;
    }
    else
    {
    	switch( pMga->Chipset )
    	{
    	case PCI_CHIP_MGA1064:
	    if ( pMga->ChipRev < 3 )
	    	MGAdac->maxPixelClock = 170000;
	    else
	        MGAdac->maxPixelClock = 220000;
	    break;
    	case PCI_CHIP_MGAG400:
    	case PCI_CHIP_MGAG550:
	    /* We don't know the new pins format but we know that
	       the maxclock / 4 is where the RamdacType was in the
	       old pins format */
	    MGAdac->maxPixelClock = pMga->Bios2.RamdacType * 4000;
	    if(MGAdac->maxPixelClock < 300000)
		MGAdac->maxPixelClock = 300000;
	    break;
	default:
	    MGAdac->maxPixelClock = 250000;
	}
	MGAdac->ClockFrom = X_DEFAULT;
    }
    
    /* Disable interleaving and set the rounding value */
    pMga->Interleave = FALSE;

    pMga->Roundings[0] = 64;
    pMga->Roundings[1] = 32;
    pMga->Roundings[2] = 64;
    pMga->Roundings[3] = 32;

    /* Clear Fast bitblt flag */
    pMga->HasFBitBlt = FALSE;
}

void MGAGSetupFuncs(ScrnInfoPtr pScrn)
{
    MGAPtr pMga = MGAPTR(pScrn);

    pMga->PreInit = MGAGRamdacInit;
    pMga->Save = MGAGSave;
    pMga->Restore = MGAGRestore;
    pMga->ModeInit = MGAGInit;
    pMga->ddc1Read = MGAG_ddc1Read;
    /* vgaHWddc1SetSpeed will only work if the card is in VGA mode */
    pMga->DDC1SetSpeed = vgaHWddc1SetSpeed;
    pMga->i2cInit = MGAG_i2cInit;
}

