/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/mga/mga_dac3026.c,v 1.56 2001/04/05 21:29:14 dawes Exp $ */
/*
 * Copyright 1994 by Robin Cutshaw <robin@XFree86.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Robin Cutshaw not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Robin Cutshaw makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ROBIN CUTSHAW DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ROBIN CUTSHAW BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 *
 * Modified for TVP3026 by Harald Koenig <koenig@tat.physik.uni-tuebingen.de>
 * 
 * Modified for MGA Millennium by Xavier Ducoin <xavier@rd.lectra.fr>
 *
 * Doug Merritt <doug@netcom.com>
 * 24bpp: fixed high res stripe glitches, clock glitches on all res
 *
 */


/*
 * This is a first cut at a non-accelerated version to work with the
 * new server design (DHD).
 */                     

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
 * Only change these bits in the Option register.  Make sure that the
 * vgaioen bit is never in this mask because it is controlled elsewhere
 */
#define OPTION_MASK 0xFFEFFEFF	/* ~(eepromwt | vgaioen) */

static void MGA3026LoadPalette(ScrnInfoPtr, int, int*, LOCO*, VisualPtr);
static void MGA3026SavePalette(ScrnInfoPtr, unsigned char*);
static void MGA3026RestorePalette(ScrnInfoPtr, unsigned char*);
static void MGA3026RamdacInit(ScrnInfoPtr);
static void MGA3026Save(ScrnInfoPtr, vgaRegPtr, MGARegPtr, Bool);
static void MGA3026Restore(ScrnInfoPtr, vgaRegPtr, MGARegPtr, Bool);
static Bool MGA3026Init(ScrnInfoPtr, DisplayModePtr);
static Bool MGA3026_i2cInit(ScrnInfoPtr pScrn);


/*
 * implementation
 */
 
/*
 * indexes to ti3026 registers (the order is important)
 */
const static unsigned char MGADACregs[] = {
	0x0F, 0x18, 0x19, 0x1A, 0x1C,   0x1D, 0x1E, 0x2A, 0x2B, 0x30,
	0x31, 0x32, 0x33, 0x34, 0x35,   0x36, 0x37, 0x38, 0x39, 0x3A,
	0x06
};

/* note: to fix a cursor hw glitch, register 0x37 (blue color key) needs
   to be set to magic numbers, even though they are "never" used because 
   blue keying disabled in 0x38.

   Matrox sez:

   ...The more precise statement of the software workaround is to insure
   that bits 7-5 of register 0x37 (Blue Color Key High) and bits 7-5 of
   register 0x38 (HZOOM)are the same...
*/

/* also note: the values of the MUX control register 0x19 (index [2]) can be
   found in table 2-17 of the 3026 manual. If interlace is set, the values
   listed here are incremented by one. 
*/
   
#define DACREGSIZE sizeof(MGADACregs)
/*
 * initial values of ti3026 registers
 */
const static unsigned char MGADACbpp8[DACREGSIZE] = {
	0x06, 0x80, 0x4b, 0x25, 0x00,   0x00, 0x0C, 0x00, 0x1E, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   0xFF, 0x00, 0x00,    0, 0x00,
	0x00
};
const static unsigned char MGADACbpp16[DACREGSIZE] = {
	0x07, 0x45, 0x53, 0x15, 0x00,   0x00, 0x2C, 0x00, 0x1E, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   0xFF, 0x00, 0x10,    0, 0x00,
	0x00
};
/*
 * [0] value was 0x07, but changed to 0x06 by Doug Merrit to fix high res
 * stripe glitches and clock glitches at 24bpp.
 */
/* [0] value is now set inside of MGA3026Init, based on the silicon revision
   It is still set to 7 or 6 based on the revision, though, since setting to
   8 as in the documentation makes (some?) revB chips get the colors wrong...
   maybe BGR instead of RGB? This only applies to 24bpp, since it is the only
   one documented as depending on revision.
 */

const static unsigned char MGADACbpp24[DACREGSIZE] = {
	0x06, 0x56, 0x5b, 0x25, 0x00,   0x00, 0x2C, 0x00, 0x1E, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   0xFF, 0x00, 0x10,    0, 0x00,
	0x00
};
const static unsigned char MGADACbpp32[DACREGSIZE] = {
	0x07, 0x46, 0x5b, 0x05, 0x00,   0x00, 0x2C, 0x00, 0x1E, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   0xFF, 0x00, 0x10,    0, 0x00,
	0x00
};

/* on at least some 2064Ws, the PSEL line flips at 4MB or so, so PSEL keying
   has to be off in register 0x1e -> bit4 clear */
   
const static unsigned char MGADACbpp8plus24[DACREGSIZE] = {
	0x07, 0x06, 0x5b, 0x05, 0x00,   0x00, 0x2C, 0x00, 0x1E, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   0xFF, 0x00, 0x01, 0x00, 0x00, 
	0x00
};
    
/*
 * Read/write to the DAC via MMIO 
 */

/*
 * These were functions.  Use macros instead to avoid the need to
 * pass pMga to them.
 */

#define inTi3026dreg(reg) INREG8(RAMDAC_OFFSET + (reg))

#define outTi3026dreg(reg, val) OUTREG8(RAMDAC_OFFSET + (reg), val)

#define inTi3026(reg) \
	(outTi3026dreg(TVP3026_INDEX, reg), inTi3026dreg(TVP3026_DATA))

#define outTi3026(reg, mask, val) \
	do { /* note: mask and reg may get evaluated twice */ \
	    unsigned char tmp = (mask) ? (inTi3026(reg) & (mask)) : 0; \
	    outTi3026dreg(TVP3026_INDEX, reg); \
	    outTi3026dreg(TVP3026_DATA, tmp | (val)); \
	} while (0)


/*
 * MGATi3026CalcClock - Calculate the PLL settings (m, n, p).
 *
 * DESCRIPTION
 *   For more information, refer to the Texas Instruments
 *   "TVP3026 Data Manual" (document SLAS098B).
 *     Section 2.4 "PLL Clock Generators"
 *     Appendix A "Frequency Synthesis PLL Register Settings"
 *     Appendix B "PLL Programming Examples"
 *
 * PARAMETERS
 *   f_out		IN	Desired clock frequency.
 *   f_max		IN	Maximum allowed clock frequency.
 *   m			OUT	Value of PLL 'm' register.
 *   n			OUT	Value of PLL 'n' register.
 *   p			OUT	Value of PLL 'p' register.
 *
 * HISTORY
 *   January 11, 1997 - [aem] Andrew E. Mileski
 *   Split off from MGATi3026SetClock.
 */

/* The following values are in kHz */
#define TI_MIN_VCO_FREQ  110000
#define TI_MAX_VCO_FREQ  220000
#define TI_MAX_MCLK_FREQ 100000
#define TI_REF_FREQ      14318.18

static double
MGATi3026CalcClock (
   long f_out, long f_max,
   int *m, int *n, int *p
){
	int best_m = 0, best_n = 0;
	double f_pll, f_vco;
	double m_err, inc_m, calc_m;

	/* Make sure that f_min <= f_out <= f_max */
	if ( f_out < ( TI_MIN_VCO_FREQ / 8 ))
		f_out = TI_MIN_VCO_FREQ / 8;
	if ( f_out > f_max )
		f_out = f_max;

	/*
	 * f_pll = f_vco / 2 ^ p
	 * Choose p so that TI_MIN_VCO_FREQ <= f_vco <= TI_MAX_VCO_FREQ
	 * Note that since TI_MAX_VCO_FREQ = 2 * TI_MIN_VCO_FREQ
	 * we don't have to bother checking for this maximum limit.
	 */
	f_vco = ( double ) f_out;
	for ( *p = 0; *p < 3 && f_vco < TI_MIN_VCO_FREQ; ( *p )++ )
		f_vco *= 2.0;

	/*
	 * We avoid doing multiplications by ( 65 - n ),
	 * and add an increment instead - this keeps any error small.
	 */
	inc_m = f_vco / ( TI_REF_FREQ * 8.0 );

	/* Initial value of calc_m for the loop */
	calc_m = inc_m + inc_m + inc_m;

	/* Initial amount of error for an integer - impossibly large */
	m_err = 2.0;

	/* Search for the closest INTEGER value of ( 65 - m ) */
	for ( *n = 3; *n <= 25; ( *n )++, calc_m += inc_m ) {

		/* Ignore values of ( 65 - m ) which we can't use */
		if ( calc_m < 3.0 || calc_m > 64.0 )
			continue;

		/*
		 * Pick the closest INTEGER (has smallest fractional part).
		 * The optimizer should clean this up for us.
		 */
		if (( calc_m - ( int ) calc_m ) < m_err ) {
			m_err = calc_m - ( int ) calc_m;
			best_m = ( int ) calc_m;
			best_n = *n;
		}
	}
	
	/* 65 - ( 65 - x ) = x */
	*m = 65 - best_m;
	*n = 65 - best_n;

	/* Now all the calculations can be completed */
	f_vco = 8.0 * TI_REF_FREQ * best_m / best_n;
	f_pll = f_vco / ( 1 << *p );

#ifdef DEBUG
	ErrorF( "f_out=%ld f_pll=%.1f f_vco=%.1f n=%d m=%d p=%d\n",
		f_out, f_pll, f_vco, *n, *m, *p );
#endif

	return f_pll;
}

/*
 * MGATi3026SetMCLK - Set the memory clock (MCLK) PLL.
 *
 * HISTORY
 *   January 11, 1997 - [aem] Andrew E. Mileski
 *   Written and tested.
 */
static void
MGATi3026SetMCLK( ScrnInfoPtr pScrn, long f_out )
{
	double f_pll;
	int mclk_m, mclk_n, mclk_p;
	int pclk_m, pclk_n, pclk_p;
	int mclk_ctl;
	MGAPtr pMga = MGAPTR(pScrn);

	f_pll = MGATi3026CalcClock(
		f_out, TI_MAX_MCLK_FREQ,
		& mclk_m, & mclk_n, & mclk_p
	);

	/* Save PCLK settings */
	outTi3026( TVP3026_PLL_ADDR, 0, 0xfc );
	pclk_n = inTi3026( TVP3026_PIX_CLK_DATA );
	outTi3026( TVP3026_PLL_ADDR, 0, 0xfd );
	pclk_m = inTi3026( TVP3026_PIX_CLK_DATA );
	outTi3026( TVP3026_PLL_ADDR, 0, 0xfe );
	pclk_p = inTi3026( TVP3026_PIX_CLK_DATA );
	
	/* Stop PCLK (PLLEN = 0, PCLKEN = 0) */
	outTi3026( TVP3026_PLL_ADDR, 0, 0xfe );
	outTi3026( TVP3026_PIX_CLK_DATA, 0, 0x00 );
	
	/* Set PCLK to the new MCLK frequency (PLLEN = 1, PCLKEN = 0 ) */
	outTi3026( TVP3026_PLL_ADDR, 0, 0xfc );
	outTi3026( TVP3026_PIX_CLK_DATA, 0, ( mclk_n & 0x3f ) | 0xc0 );
	outTi3026( TVP3026_PIX_CLK_DATA, 0, mclk_m & 0x3f );
	outTi3026( TVP3026_PIX_CLK_DATA, 0, ( mclk_p & 0x03 ) | 0xb0 );
	
	/* Wait for PCLK PLL to lock on frequency */
	while (( inTi3026( TVP3026_PIX_CLK_DATA ) & 0x40 ) == 0 ) {
		;
	}
	
	/* Output PCLK on MCLK pin */
	mclk_ctl = inTi3026( TVP3026_MCLK_CTL );
	outTi3026( TVP3026_MCLK_CTL, 0, mclk_ctl & 0xe7 ); 
	outTi3026( TVP3026_MCLK_CTL, 0, ( mclk_ctl & 0xe7 ) | 0x08 );
	
	/* Stop MCLK (PLLEN = 0 ) */
	outTi3026( TVP3026_PLL_ADDR, 0, 0xfb );
	outTi3026( TVP3026_MEM_CLK_DATA, 0, 0x00 );
	
	/* Set MCLK to the new frequency (PLLEN = 1) */
	outTi3026( TVP3026_PLL_ADDR, 0, 0xf3 );
	outTi3026( TVP3026_MEM_CLK_DATA, 0, ( mclk_n & 0x3f ) | 0xc0 );
	outTi3026( TVP3026_MEM_CLK_DATA, 0, mclk_m & 0x3f );
	outTi3026( TVP3026_MEM_CLK_DATA, 0, ( mclk_p & 0x03 ) | 0xb0 );
	
	/* Wait for MCLK PLL to lock on frequency */
	while (( inTi3026( TVP3026_MEM_CLK_DATA ) & 0x40 ) == 0 ) {
		;
	}

	/* Output MCLK PLL on MCLK pin */
	outTi3026( TVP3026_MCLK_CTL, 0, ( mclk_ctl & 0xe7 ) | 0x10 );
	outTi3026( TVP3026_MCLK_CTL, 0, ( mclk_ctl & 0xe7 ) | 0x18 );
	
	/* Stop PCLK (PLLEN = 0, PCLKEN = 0 ) */
	outTi3026( TVP3026_PLL_ADDR, 0, 0xfe );
	outTi3026( TVP3026_PIX_CLK_DATA, 0, 0x00 );
	
	/* Restore PCLK (PLLEN = ?, PCLKEN = ?) */
	outTi3026( TVP3026_PLL_ADDR, 0, 0xfc );
	outTi3026( TVP3026_PIX_CLK_DATA, 0, pclk_n );
	outTi3026( TVP3026_PIX_CLK_DATA, 0, pclk_m );
	outTi3026( TVP3026_PIX_CLK_DATA, 0, pclk_p );
	
	/* Wait for PCLK PLL to lock on frequency */
	while (( inTi3026( TVP3026_PIX_CLK_DATA ) & 0x40 ) == 0 ) {
		;
	}
}

/*
 * MGATi3026SetPCLK - Set the pixel (PCLK) and loop (LCLK) clocks.
 *
 * PARAMETERS
 *   f_pll			IN	Pixel clock PLL frequencly in kHz.
 *   bpp			IN	Bytes per pixel.
 *
 * HISTORY
 *   January 11, 1997 - [aem] Andrew E. Mileski
 *   Split to simplify code for MCLK (=GCLK) setting.
 *
 *   December 14, 1996 - [aem] Andrew E. Mileski
 *   Fixed loop clock to be based on the calculated, not requested,
 *   pixel clock. Added f_max = maximum f_vco frequency.
 *
 *   October 19, 1996 - [aem] Andrew E. Mileski
 *   Commented the loop clock code (wow, I understand everything now),
 *   and simplified it a bit. This should really be two separate functions.
 *
 *   October 1, 1996 - [aem] Andrew E. Mileski
 *   Optimized the m & n picking algorithm. Added maxClock detection.
 *   Low speed pixel clock fix (per the docs). Documented what I understand.
 *
 *   ?????, ??, ???? - [???] ????????????
 *   Based on the TVP3026 code in the S3 driver.
 */

static void 
MGATi3026SetPCLK( ScrnInfoPtr pScrn, long f_out, int bpp )
{
	/* Pixel clock values */
	int m, n, p;

	/* Loop clock values */
	int lm, ln, lp, lq;
	double z;

	/* The actual frequency output by the clock */
	double f_pll;

	long f_max = TI_MAX_VCO_FREQ;

	MGAPtr pMga = MGAPTR(pScrn);
	MGARegPtr pReg = &pMga->ModeReg;

	/* Get the maximum pixel clock frequency */
	if ( pMga->MaxClock > TI_MAX_VCO_FREQ )
		f_max = pMga->MaxClock;

	/* Do the calculations for m, n, and p */
	f_pll = MGATi3026CalcClock( f_out, f_max, & m, & n, & p );

	/* Values for the pixel clock PLL registers */
	pReg->DacClk[ 0 ] = ( n & 0x3f ) | 0xc0;
	pReg->DacClk[ 1 ] = ( m & 0x3f );
	pReg->DacClk[ 2 ] = ( p & 0x03 ) | 0xb0;

	/*
	 * Now that the pixel clock PLL is setup,
	 * the loop clock PLL must be setup.
	 */

	/*
	 * First we figure out lm, ln, and z.
	 * Things are different in packed pixel mode (24bpp) though.
	 */
	 if ( pMga->CurrentLayout.bitsPerPixel == 24 ) {

		/* ln:lm = ln:3 */
		lm = 65 - 3;

		/* Check for interleaved mode */
		if ( bpp == 2 )
			/* ln:lm = 4:3 */
			ln = 65 - 4;
		else
			/* ln:lm = 8:3 */
			ln = 65 - 8;

		/* Note: this is actually 100 * z for more precision */
		z = ( 11000 * ( 65 - ln )) / (( f_pll / 1000 ) * ( 65 - lm ));
	}
	else {
		/* ln:lm = ln:4 */
		lm = 65 - 4;

		/* Note: bpp = bytes per pixel */
		ln = 65 - 4 * ( 64 / 8 ) / bpp;

		/* Note: this is actually 100 * z for more precision */
		z = (( 11000 / 4 ) * ( 65 - ln )) / ( f_pll / 1000 );
	}

	/*
	 * Now we choose dividers lp and lq so that the VCO frequency
	 * is within the operating range of 110 MHz to 220 MHz.
	 */

	/* Assume no lq divider */
	lq = 0;

	/* Note: z is actually 100 * z for more precision */
	if ( z <= 200.0 )
		lp = 0;
	else if ( z <= 400.0 )
		lp = 1;
	else if ( z <= 800.0 )
		lp = 2;
	else if ( z <= 1600.0 )
		lp = 3;
	else {
		lp = 3;
		lq = ( int )( z / 1600.0 );
	}
 
	/* Values for the loop clock PLL registers */
	if ( pMga->CurrentLayout.bitsPerPixel == 24 ) {
		/* Packed pixel mode values */
		pReg->DacClk[ 3 ] = ( ln & 0x3f ) | 0x80;
		pReg->DacClk[ 4 ] = ( lm & 0x3f ) | 0x80;
		pReg->DacClk[ 5 ] = ( lp & 0x03 ) | 0xf8;
 	} else {
		/* Non-packed pixel mode values */
		pReg->DacClk[ 3 ] = ( ln & 0x3f ) | 0xc0;
		pReg->DacClk[ 4 ] = ( lm & 0x3f );
		pReg->DacClk[ 5 ] = ( lp & 0x03 ) | 0xf0;
	}
	pReg->DacRegs[ 18 ] = lq | 0x38;

#ifdef DEBUG
	ErrorF( "bpp=%d z=%.1f ln=%d lm=%d lp=%d lq=%d\n",
		bpp, z, ln, lm, lp, lq );
#endif
}

/*
 * MGA3026Init -- for mga2064 with ti3026
 *
 * The 'mode' parameter describes the video mode.	The 'mode' structure 
 * as well as the 'vga256InfoRec' structure can be dereferenced for
 * information that is needed to initialize the mode.	The 'new' macro
 * (see definition above) is used to simply fill in the structure.
 */
static Bool
MGA3026Init(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
	int hd, hs, he, ht, vd, vs, ve, vt, wd;
	int i, BppShift, index_1d = 0;
	const unsigned char* initDAC;
	MGAPtr pMga = MGAPTR(pScrn);
	MGARamdacPtr MGAdac = &pMga->Dac;
	MGAFBLayout *pLayout = &pMga->CurrentLayout;
	MGARegPtr pReg = &pMga->ModeReg;
	vgaRegPtr pVga = &VGAHWPTR(pScrn)->ModeReg;

	BppShift = pMga->BppShifts[(pLayout->bitsPerPixel >> 3) - 1];

	switch(pLayout->bitsPerPixel)
	{
	case 8:
		initDAC = MGADACbpp8;
		break;
	case 16:
		initDAC = MGADACbpp16;
		break;
	case 24:
		initDAC = MGADACbpp24;
		break;
	case 32:
		if(pLayout->Overlay8Plus24)
		    initDAC = MGADACbpp8plus24;
		else
		    initDAC = MGADACbpp32;
		break;
	default:
		FatalError("MGA: unsupported bits per pixel\n");
	}
	
	/* Allocate the DacRegs space if not done already */
	if (pReg->DacRegs == NULL) {
		pReg->DacRegs = xnfcalloc(DACREGSIZE, 1);
	}
	for (i = 0; i < DACREGSIZE; i++) {
	    pReg->DacRegs[i] = initDAC[i]; 
	    if (MGADACregs[i] == 0x1D)
		index_1d = i;
	}

        if((pLayout->bitsPerPixel == 32) && pLayout->Overlay8Plus24) {
	    pReg->DacRegs[9] = pMga->colorKey;
	    pReg->DacRegs[10] = pMga->colorKey;
	}

	if ( (pLayout->bitsPerPixel == 16) && (pLayout->weight.red == 5)
	    && (pLayout->weight.green == 5) && (pLayout->weight.blue == 5) ) {
	    pReg->DacRegs[1] &= ~0x01;
	}
	if (pMga->Interleave ) 	pReg->DacRegs[2] += 1;


	if ( pLayout->bitsPerPixel == 24 ) {
	  int silicon_rev;
	  /* we need to set DacRegs[0] differently based on the silicon
	   * revision of the 3026 RAMDAC, as per page 2-14 of tvp3026.pdf.
	   * If we have rev A silicon, we want 0x07; rev B silicon wants
	   * 0x06.
	   */
	  silicon_rev = inTi3026(TVP3026_SILICON_REV);
	  
#ifdef DEBUG
	    ErrorF("TVP3026 revision 0x%x (rev %s)\n",
		   silicon_rev, (silicon_rev <= 0x20) ? "A" : "B");
#endif
	    
	    if(silicon_rev <= 0x20) {
	      /* rev A */
	      pReg->DacRegs[0] = 0x07;
	    } else {
	      /* rev B */
	      pReg->DacRegs[0] = 0x06;
	    }
	}

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
		
		/* enable interlaced cursor */
		pReg->DacRegs[20] |= 0x20;
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

	/* Set viddelay (CRTCEXT3 Bits 3-4). */
	pReg->ExtVga[3] |= (pScrn->videoRam == 8192 ? 0x10
			     : pScrn->videoRam == 2048 ? 0x08 : 0x00);

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

	if (mode->Flags & V_DBLSCAN)
		pVga->CRTC[9] |= 0x80;
    
	/* Per DDK vid.c line 75, sync polarity should be controlled
	 * via the TVP3026 RAMDAC register 1D and so MISC Output Register
	 * should always have bits 6 and 7 set. */

	pVga->MiscOutReg |= 0xC0;
	if ((mode->Flags & (V_PHSYNC | V_NHSYNC)) &&
	    (mode->Flags & (V_PVSYNC | V_NVSYNC)))
	{
	    if (mode->Flags & V_PHSYNC)
		pReg->DacRegs[index_1d] |= 0x01;
	    if (mode->Flags & V_PVSYNC)
		pReg->DacRegs[index_1d] |= 0x02;
	}
	else
	{
	  int VDisplay = mode->VDisplay;
	  if (mode->Flags & V_DBLSCAN)
	    VDisplay *= 2;
	  if      (VDisplay < 400)
		  pReg->DacRegs[index_1d] |= 0x01; /* +hsync -vsync */
	  else if (VDisplay < 480)
		  pReg->DacRegs[index_1d] |= 0x02; /* -hsync +vsync */
	  else if (VDisplay < 768)
		  pReg->DacRegs[index_1d] |= 0x00; /* -hsync -vsync */
	  else
		  pReg->DacRegs[index_1d] |= 0x03; /* +hsync +vsync */
	}
	
	if (pMga->SyncOnGreen)
	    pReg->DacRegs[index_1d] |= 0x20;

	pReg->Option = 0x402C0100;  /* fine for 2064 and 2164 */

	if (pMga->Interleave)
	  pReg->Option |= 0x1000;
	else
	  pReg->Option &= ~0x1000;

	/* must always have the pci retries on but rely on 
	   polling to keep them from occuring */
	pReg->Option &= ~0x20000000;

	pVga->MiscOutReg |= 0x0C; 
	/* XXX Need to check the first argument */
	MGATi3026SetPCLK( pScrn, mode->Clock, 1 << BppShift );

	/* this one writes registers rather than writing to the 
	   mgaReg->ModeReg and letting Restore write to the hardware
	   but that's no big deal since we will Restore right after
	   this function */

	MGA_NOT_HAL(MGATi3026SetMCLK(pScrn, MGAdac->MemoryClock));

#ifdef DEBUG		
	ErrorF("%6ld: %02X %02X %02X	%02X %02X %02X	%08lX\n", mode->Clock,
		pReg->DacClk[0], pReg->DacClk[1], pReg->DacClk[2], pReg->DacClk[3], pReg->DacClk[4], pReg->DacClk[5], pReg->Option);
	for (i=0; i<sizeof(MGADACregs); i++) ErrorF("%02X ", pReg->DacRegs[i]);
	for (i=0; i<6; i++) ErrorF(" %02X", pReg->ExtVga[i]);
	ErrorF("\n");
#endif

	/* This disables the VGA memory aperture */
	pVga->MiscOutReg &= ~0x02;
	return(TRUE);
}

/*
 * MGA3026Restore -- for mga2064 with ti3026
 *
 * This function restores a video mode.	 It basically writes out all of
 * the registers that have previously been saved in the vgaMGARec data 
 * structure.
 */
static void 
MGA3026Restore(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, MGARegPtr mgaReg,
	       Bool restoreFonts)
{
	int i;
	MGAPtr pMga = MGAPTR(pScrn);

	/*
	 * Code is needed to get things back to bank zero.
	 */
	for (i = 0; i < 6; i++)
		OUTREG16(0x1FDE, (mgaReg->ExtVga[i] << 8) | i);

	pciSetBitsLong(pMga->PciTag, PCI_OPTION_REG, OPTION_MASK,
		       mgaReg->Option);

	MGA_NOT_HAL(
	/* select pixel clock PLL as clock source */
	outTi3026(TVP3026_CLK_SEL, 0, mgaReg->DacRegs[3]);
	
	/* set loop and pixel clock PLL PLLEN bits to 0 */
	outTi3026(TVP3026_PLL_ADDR, 0, 0x2A);
	outTi3026(TVP3026_LOAD_CLK_DATA, 0, 0);
	outTi3026(TVP3026_PIX_CLK_DATA, 0, 0);
	);	/* MGA_NOT_HAL */
	 
	/*
	 * This function handles restoring the generic VGA registers.
	 */
	vgaHWRestore(pScrn, vgaReg,
			VGA_SR_MODE | (restoreFonts ? VGA_SR_FONTS : 0));
	MGA3026RestorePalette(pScrn, vgaReg->DAC);

	/*
	 * Code to restore SVGA registers that have been saved/modified
	 * goes here. 
	 */

	MGA_NOT_HAL(
	/* program pixel clock PLL */
	outTi3026(TVP3026_PLL_ADDR, 0, 0x00);
	for (i = 0; i < 3; i++)
		outTi3026(TVP3026_PIX_CLK_DATA, 0, mgaReg->DacClk[i]);

	if (vgaReg->MiscOutReg & 0x08) {
		/* poll until pixel clock PLL LOCK bit is set */
		outTi3026(TVP3026_PLL_ADDR, 0, 0x3F);
		while ( ! (inTi3026(TVP3026_PIX_CLK_DATA) & 0x40) );
	}

	/* set Q divider for loop clock PLL */
	outTi3026(TVP3026_MCLK_CTL, 0, mgaReg->DacRegs[18]);
	);	/* MGA_NOT_HAL */
	
	/* program loop PLL */
	outTi3026(TVP3026_PLL_ADDR, 0, 0x00);
	for (i = 3; i < 6; i++)
		outTi3026(TVP3026_LOAD_CLK_DATA, 0, mgaReg->DacClk[i]);

	MGA_NOT_HAL(
	if ((vgaReg->MiscOutReg & 0x08) && ((mgaReg->DacClk[3] & 0xC0) == 0xC0) ) {
		/* poll until loop PLL LOCK bit is set */
		outTi3026(TVP3026_PLL_ADDR, 0, 0x3F);
		while ( ! (inTi3026(TVP3026_LOAD_CLK_DATA) & 0x40) );
	}
	);	/* MGA_NOT_HAL */

	/*
	 * restore other DAC registers
	 */
	for (i = 0; i < DACREGSIZE; i++)
		outTi3026(MGADACregs[i], 0, mgaReg->DacRegs[i]);

#ifdef DEBUG
	ErrorF("PCI retry (0-enabled / 1-disabled): %d\n",
		!!(mgaReg->Option & 0x20000000));
#endif		 
}

/*
 * MGA3026Save -- for mga2064 with ti3026
 *
 * This function saves the video state.
 */
static void
MGA3026Save(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, MGARegPtr mgaReg,
	    Bool saveFonts)
{
	int i;
	MGAPtr pMga = MGAPTR(pScrn);
	
	/* Allocate the DacRegs space if not done already */
	if (mgaReg->DacRegs == NULL) {
		mgaReg->DacRegs = xnfcalloc(DACREGSIZE, 1);
	}

	/*
	 * Code is needed to get back to bank zero.
	 */
	OUTREG16(0x1FDE, 0x0004);
	
	/*
	 * This function will handle creating the data structure and filling
	 * in the generic VGA portion.
	 */
	vgaHWSave(pScrn, vgaReg, VGA_SR_MODE | (saveFonts ? VGA_SR_FONTS : 0));
	MGA3026SavePalette(pScrn, vgaReg->DAC);

	/*
	 * The port I/O code necessary to read in the extended registers 
	 * into the fields of the vgaMGARec structure.
	 */
	for (i = 0; i < 6; i++)
	{
		OUTREG8(0x1FDE, i);
		mgaReg->ExtVga[i] = INREG8(0x1FDF);
	}

	MGA_NOT_HAL(
	outTi3026(TVP3026_PLL_ADDR, 0, 0x00);
	for (i = 0; i < 3; i++)
		outTi3026(TVP3026_PIX_CLK_DATA, 0, mgaReg->DacClk[i] =
					inTi3026(TVP3026_PIX_CLK_DATA));

	outTi3026(TVP3026_PLL_ADDR, 0, 0x00);
	for (i = 3; i < 6; i++)
		outTi3026(TVP3026_LOAD_CLK_DATA, 0, mgaReg->DacClk[i] =
					inTi3026(TVP3026_LOAD_CLK_DATA));
	);	/* MGA_NOT_HAL */
	
	for (i = 0; i < DACREGSIZE; i++)
		mgaReg->DacRegs[i]	 = inTi3026(MGADACregs[i]);
	
	mgaReg->Option = pciReadLong(pMga->PciTag, PCI_OPTION_REG);
	
#ifdef DEBUG		
	ErrorF("read: %02X %02X %02X	%02X %02X %02X	%08lX\n",
		mgaReg->DacClk[0], mgaReg->DacClk[1], mgaReg->DacClk[2], mgaReg->DacClk[3], mgaReg->DacClk[4], mgaReg->DacClk[5], mgaReg->Option);
	for (i=0; i<sizeof(MGADACregs); i++) ErrorF("%02X ", mgaReg->DacRegs[i]);
	for (i=0; i<6; i++) ErrorF(" %02X", mgaReg->ExtVga[i]);
	ErrorF("\n");
#endif
}


static void
MGA3026LoadCursorImage(
    ScrnInfoPtr pScrn, 
    unsigned char *src
)
{
    MGAPtr pMga = MGAPTR(pScrn);
    int i = 1024;
       
    outTi3026(TVP3026_CURSOR_CTL, 0xf3, 0x00); /* reset A9,A8 */
    /* reset cursor RAM load address A7..A0 */
    outTi3026dreg(TVP3026_WADR_PAL, 0x00); 

    while(i--) {
	while (INREG8(0x1FDA) & 0x01);
	while (!(INREG8(0x1FDA) & 0x01));
        outTi3026dreg(TVP3026_CUR_RAM, *(src++));    
    }
}


static void 
MGA3026ShowCursor(ScrnInfoPtr pScrn)
{
    MGAPtr pMga = MGAPTR(pScrn);
    /* Enable cursor - X11 mode */
    outTi3026(TVP3026_CURSOR_CTL, 0x6c, 0x13);
}

static void
MGA3026HideCursor(ScrnInfoPtr pScrn)
{
    MGAPtr pMga = MGAPTR(pScrn);
    /* Disable cursor */
    outTi3026(TVP3026_CURSOR_CTL, 0xfc, 0x00);
}

static void
MGA3026SetCursorPosition(
   ScrnInfoPtr pScrn, 
   int x, int y
)
{
    MGAPtr pMga = MGAPTR(pScrn);
    x += 64;
    y += 64;

    /* Output position - "only" 12 bits of location documented */
   
    outTi3026dreg(TVP3026_CUR_XLOW, x & 0xFF);
    outTi3026dreg(TVP3026_CUR_XHI, (x >> 8) & 0x0F);
    outTi3026dreg(TVP3026_CUR_YLOW, y & 0xFF);
    outTi3026dreg(TVP3026_CUR_YHI, (y >> 8) & 0x0F);
}

static void
MGA3026SetCursorColors(
   ScrnInfoPtr pScrn, 
   int bg, int fg
)
{
    MGAPtr pMga = MGAPTR(pScrn);
    /* The TI 3026 cursor is always 8 bits so shift 8, not 10 */

    /* Background color */
    outTi3026dreg(TVP3026_CUR_COL_ADDR, 1);
    outTi3026dreg(TVP3026_CUR_COL_DATA, (bg & 0x00FF0000) >> 16);
    outTi3026dreg(TVP3026_CUR_COL_DATA, (bg & 0x0000FF00) >> 8);
    outTi3026dreg(TVP3026_CUR_COL_DATA, (bg & 0x000000FF));

    /* Foreground color */
    outTi3026dreg(TVP3026_CUR_COL_ADDR, 2);
    outTi3026dreg(TVP3026_CUR_COL_DATA, (fg & 0x00FF0000) >> 16);
    outTi3026dreg(TVP3026_CUR_COL_DATA, (fg & 0x0000FF00) >> 8);
    outTi3026dreg(TVP3026_CUR_COL_DATA, (fg & 0x000000FF));
}

static Bool 
MGA3026UseHWCursor(ScreenPtr pScrn, CursorPtr pCurs)
{
    if( XF86SCRNINFO(pScrn)->currentMode->Flags & V_DBLSCAN )
    	return FALSE;
    return TRUE;
}

static const int DDC_SDA_MASK = 1 << 2;
static const int DDC_SCL_MASK = 1 << 4;

static unsigned int
MGA3026_ddc1Read(ScrnInfoPtr pScrn)
{
  MGAPtr pMga = MGAPTR(pScrn);

  /* Define the SDA as an input */
  outTi3026(TVP3026_GEN_IO_CTL, 0xfb, 0);

  /* wait for Vsync */
  while( INREG( MGAREG_Status ) & 0x08 );
  while( ! (INREG( MGAREG_Status ) & 0x08) );

  /* Get the result */
  return (inTi3026(TVP3026_GEN_IO_DATA) & DDC_SDA_MASK) >> 2 ;
}

static void
MGA3026_I2CGetBits(I2CBusPtr b, int *clock, int *data) 
{
  MGAPtr pMga = MGAPTR(xf86Screens[b->scrnIndex]);
  unsigned char val;

  /* Get the result. */
  val = inTi3026(TVP3026_GEN_IO_DATA); 
  *clock = (val & DDC_SCL_MASK) != 0;
  *data  = (val & DDC_SDA_MASK) != 0;

#ifdef DEBUG
	 ErrorF("MGA3026_I2CGetBits(%p,...) val=0x%x, returns clock %d, data %d\n", b, val, *clock, *data);
#endif
}

/*
 * ATTENTION! - the DATA and CLOCK lines need to be tri-stated when
 * high. Therefore turn off output driver for the line to set line
 * to high. High signal is maintained by a 15k Ohm pll-up resistor.
 */
static void
MGA3026_I2CPutBits(I2CBusPtr b, int clock, int data)
{
  MGAPtr pMga = MGAPTR(xf86Screens[b->scrnIndex]);
  unsigned char val,drv;

  /* Write the values */
  val = (clock ? DDC_SCL_MASK : 0) | (data ? DDC_SDA_MASK : 0);
  drv = ((!clock) ? DDC_SCL_MASK : 0) | ((!data) ? DDC_SDA_MASK : 0);
  /* Define the SDA (Data) and SCL (clock) as outputs */
  outTi3026(TVP3026_GEN_IO_CTL, ~(DDC_SDA_MASK | DDC_SCL_MASK), drv);
  outTi3026(TVP3026_GEN_IO_DATA, ~(DDC_SDA_MASK | DDC_SCL_MASK), val); 

#ifdef DEBUG
  ErrorF("MGA3026_I2CPutBits(%p, %d, %d) val=0x%x\n", b, clock, data, val);
#endif

}


Bool
MGA3026_i2cInit(ScrnInfoPtr pScrn)
{
    MGAPtr pMga = MGAPTR(pScrn);
    I2CBusPtr I2CPtr;

    I2CPtr = xf86CreateI2CBusRec();
    if(!I2CPtr) return FALSE;

    pMga->I2C = I2CPtr;

    I2CPtr->BusName    = "DDC";
    I2CPtr->scrnIndex  = pScrn->scrnIndex;
    I2CPtr->I2CPutBits = MGA3026_I2CPutBits;
    I2CPtr->I2CGetBits = MGA3026_I2CGetBits;

    /* I2CPutByte is timing out, experimenting with AcknTimeout 
     * default is 2CPtr->AcknTimeout = 5; 
     */
    /* I2CPtr->AcknTimeout = 10; */

    if (!xf86I2CBusInit(I2CPtr)) {
	return FALSE;
    }
    return TRUE;
}

static void
MGA3026RamdacInit(ScrnInfoPtr pScrn)
{
    MGAPtr pMga;
    MGARamdacPtr MGAdac;

    pMga = MGAPTR(pScrn);
    MGAdac = &pMga->Dac;

    MGAdac->isHwCursor		= TRUE;
    MGAdac->CursorMaxWidth	= 64;
    MGAdac->CursorMaxHeight	= 64;
    MGAdac->SetCursorColors	= MGA3026SetCursorColors;
    MGAdac->SetCursorPosition	= MGA3026SetCursorPosition;
    MGAdac->LoadCursorImage	= MGA3026LoadCursorImage;
    MGAdac->HideCursor		= MGA3026HideCursor;
    MGAdac->ShowCursor		= MGA3026ShowCursor;
    MGAdac->UseHWCursor		= MGA3026UseHWCursor;
    MGAdac->CursorFlags		= 
#if X_BYTE_ORDER == X_LITTLE_ENDIAN
				HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
#endif
				HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
				HARDWARE_CURSOR_SOURCE_MASK_NOT_INTERLEAVED;

    MGAdac->LoadPalette 	= MGA3026LoadPalette;

    MGAdac->ClockFrom = X_PROBED;
    if ( pMga->Chipset == PCI_CHIP_MGA2064 && pMga->Bios2.PinID == 0 )
    {
	switch( pMga->Bios.RamdacType & 0xff )
	{
	case 1: MGAdac->maxPixelClock = 220000;
	    break;
	case 2: MGAdac->maxPixelClock = 250000;
	    break;
	default:
	    MGAdac->maxPixelClock = 175000;
	    MGAdac->ClockFrom = X_DEFAULT;
	    break;
	}
	/* Set MCLK based on amount of memory */
	if(pMga->OverclockMem) {
	  if ( pScrn->videoRam < 4096 )
            MGAdac->MemoryClock = pMga->Bios.ClkBase * 12;
	  else if ( pScrn->videoRam < 8192 )
            MGAdac->MemoryClock = pMga->Bios.Clk4MB * 12;
	  else
            MGAdac->MemoryClock = pMga->Bios.Clk8MB * 12;
	  MGAdac->MemClkFrom = X_CONFIG;
	  MGAdac->SetMemClk = TRUE;
#if 0
	  ErrorF("BIOS Memory clock settings: 2Mb %d, 4Mb %d, 8MB %d\n",
		 pMga->Bios.ClkBase, pMga->Bios.Clk4MB, pMga->Bios.Clk8MB);
#endif
	} else {
	  if ( pScrn->videoRam < 4096 )
            MGAdac->MemoryClock = pMga->Bios.ClkBase * 10;
	  else if ( pScrn->videoRam < 8192 )
            MGAdac->MemoryClock = pMga->Bios.Clk4MB * 10;
	  else
            MGAdac->MemoryClock = pMga->Bios.Clk8MB * 10;
	  MGAdac->MemClkFrom = X_PROBED;
	  MGAdac->SetMemClk = TRUE;
	}
    }
    else
    {
	if ( pMga->Bios2.PinID ) 	/* make sure BIOS is available */
	{
	    if ( pMga->Bios2.PclkMax != 0xff )
	    {
		MGAdac->maxPixelClock = (pMga->Bios2.PclkMax + 100) * 1000;
	    }
	    else
		MGAdac->maxPixelClock = 220000;

	    /* make sure we are not overdriving the GE for the amount of WRAM */
	    switch (  pScrn->videoRam )
	    {
		case 4096:
		    if (pMga->Bios2.Clk4MB != 0xff)
			pMga->Bios2.ClkGE = pMga->Bios2.Clk4MB;
		    break;
		case 8192:
		    if (pMga->Bios2.Clk8MB != 0xff)
			pMga->Bios2.ClkGE = pMga->Bios2.Clk8MB;
		    break;
		case 12288:
		    if (pMga->Bios2.Clk12MB != 0xff)
			pMga->Bios2.ClkGE = pMga->Bios2.Clk12MB;
		    break;
		case 16384:
		    if (pMga->Bios2.Clk16MB != 0xff)
			pMga->Bios2.ClkGE = pMga->Bios2.Clk16MB;
		    break;
		default:
		    break;
	    }

	    if ( pMga->Bios2.ClkGE != 0xff && pMga->Bios2.ClkMem == 0xff )
		pMga->Bios2.ClkMem = pMga->Bios2.ClkGE;
	    else if ( pMga->Bios2.ClkGE == 0xff && pMga->Bios2.ClkMem != 0xff )
		    ; /* don't need to do anything */
	    else if ( pMga->Bios2.ClkGE == pMga->Bios2.ClkMem && pMga->Bios2.ClkGE != 0xff )
		pMga->Bios2.ClkMem = pMga->Bios2.ClkGE;
	    else
		pMga->Bios2.ClkMem = 60;

	    MGAdac->MemoryClock = pMga->Bios2.ClkMem * 1000;
    	    MGAdac->MemClkFrom = X_PROBED;
    	    MGAdac->SetMemClk = TRUE;
	} /* BIOS enabled initialization */
	else
	{
		/* bios is not available, initialize to rational figures */
		MGAdac->MemoryClock = 60000;	/* 60 MHz WRAM */
		MGAdac->maxPixelClock = 220000;  /* 220 MHz */
	        MGAdac->ClockFrom = X_DEFAULT;
        }
     } /* 2164 specific initialization */

    /* safety check */
    if ( (MGAdac->MemoryClock < 40000) ||
         (MGAdac->MemoryClock > 70000) )
	MGAdac->MemoryClock = 50000; 

    /*
     * Should initialise a sane default when the probed value is
     * obviously garbage.
     */
     
    /* Check if interleaving can be used and set the rounding value */
    if (pScrn->videoRam > 2048)
        pMga->Interleave = TRUE;
    else {
        pMga->Interleave = FALSE;
        pMga->BppShifts[0]++;
        pMga->BppShifts[1]++;
        pMga->BppShifts[2]++;
        pMga->BppShifts[3]++;
    }

    pMga->Roundings[0] = 128 >> pMga->BppShifts[0];
    pMga->Roundings[1] = 128 >> pMga->BppShifts[1];
    pMga->Roundings[2] = 128 >> pMga->BppShifts[2];
    pMga->Roundings[3] = 128 >> pMga->BppShifts[3];

    /* Set Fast bitblt flag */
    pMga->HasFBitBlt = !(pMga->Bios.FeatFlag & 0x00000001);
}

void MGA3026LoadPalette(
    ScrnInfoPtr pScrn, 
    int numColors, 
    int *indices,
    LOCO *colors,
    VisualPtr pVisual
){
    MGAPtr pMga = MGAPTR(pScrn);
    int i, index;

    if(pMga->CurrentLayout.Overlay8Plus24 && (pVisual->nplanes != 8))
	return;

    if (pVisual->nplanes == 16) {
	for(i = 0; i < numColors; i++) {
	    index = indices[i];
            outTi3026dreg(MGA1064_WADR_PAL, index << 2);
            outTi3026dreg(MGA1064_COL_PAL, colors[index >> 1].red);
            outTi3026dreg(MGA1064_COL_PAL, colors[index].green);
            outTi3026dreg(MGA1064_COL_PAL, colors[index >> 1].blue);

	/* we have to write 2 indices since the pixel X on the
	   TVP3026 has green colors at different locations from
	   the red and blue colors */
	    if(index <= 31) {
		outTi3026dreg(MGA1064_WADR_PAL, index << 3);
		outTi3026dreg(MGA1064_COL_PAL, colors[index].red);
		outTi3026dreg(MGA1064_COL_PAL, colors[(index << 1) + 1].green);
		outTi3026dreg(MGA1064_COL_PAL, colors[index].blue);
	    }
	}
    } else {
	int shift = (pVisual->nplanes == 15) ? 3 : 0;

	for(i = 0; i < numColors; i++) {
	    index = indices[i];
            outTi3026dreg(MGA1064_WADR_PAL, index << shift);
            outTi3026dreg(MGA1064_COL_PAL, colors[index].red);
            outTi3026dreg(MGA1064_COL_PAL, colors[index].green);
            outTi3026dreg(MGA1064_COL_PAL, colors[index].blue);
	}
    }
}


static void
MGA3026SavePalette(ScrnInfoPtr pScrn, unsigned char* pntr)
{
    MGAPtr pMga = MGAPTR(pScrn);
    int i = 768;

    outTi3026dreg(TVP3026_RADR_PAL, 0x00);
    while(i--)
        *(pntr++) = inTi3026dreg(TVP3026_COL_PAL);
}

static void
MGA3026RestorePalette(ScrnInfoPtr pScrn, unsigned char* pntr)
{
    MGAPtr pMga = MGAPTR(pScrn);
    int i = 768;

    outTi3026dreg(TVP3026_WADR_PAL, 0x00);
    while(i--) 
        outTi3026dreg(TVP3026_COL_PAL, *(pntr++));
}


void MGA2064SetupFuncs(ScrnInfoPtr pScrn)
{
    MGAPtr pMga = MGAPTR(pScrn);
    
    pMga->PreInit = MGA3026RamdacInit;
    pMga->Save = MGA3026Save;
    pMga->Restore = MGA3026Restore;
    pMga->ModeInit = MGA3026Init;
    pMga->ddc1Read = MGA3026_ddc1Read;
    /* vgaHWddc1SetSpeed will only work if the card is in VGA mode */
    pMga->DDC1SetSpeed = vgaHWddc1SetSpeed;
    pMga->i2cInit = MGA3026_i2cInit;
}
