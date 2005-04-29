/*
 * Copyright 2000,2001 by Sven Luther <luther@dpt-info.u-strasbg.fr>.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Sven Luther not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission. Sven Luther makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * SVEN LUTHER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL SVEN LUTHER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: Sven Luther, <luther@dpt-info.u-strasbg.fr>
 *          Thomas Witzel, <twitzel@nmr.mgh.harvard.edu>
 *          Alan Hourihane, <alanh@fairlite.demon.co.uk>
 *
 * this work is sponsored by Appian Graphics.
 * 
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/glint/pm3_dac.c,v 1.34 2003/11/03 05:11:14 tsi Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86int10.h"

#include "glint_regs.h"
#include "pm3_regs.h"
#include "glint.h"

#define DEBUG 0

#if DEBUG
# define TRACE_ENTER(str)       ErrorF("glint: " str " %d\n",pScrn->scrnIndex)
# define TRACE_EXIT(str)        ErrorF("glint: " str " done\n")
# define TRACE(str)             ErrorF("glint trace: " str "\n")
#else
# define TRACE_ENTER(str)
# define TRACE_EXIT(str)
# define TRACE(str)
#endif

int
Permedia3MemorySizeDetect(ScrnInfoPtr pScrn)
{
    GLINTPtr pGlint = GLINTPTR (pScrn);
    CARD32 size = 0, temp, temp1, temp2, i;

    /* We can map 64MB, as that's the size of the Permedia3 aperture 
     * regardless of memory configuration */
    pGlint->FbMapSize = 64*1024*1024;

    /* Mark as VIDMEM_MMIO to avoid write-combining while detecting memory */
    pGlint->FbBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO,
			pGlint->PciTag, pGlint->FbAddress, pGlint->FbMapSize);

    if (pGlint->FbBase == NULL) 
	return 0;

    temp = GLINT_READ_REG(PM3MemBypassWriteMask);
    GLINT_SLOW_WRITE_REG(0xffffffff, PM3MemBypassWriteMask);

    /* The Permedia3 splits up memory, and even replicates it. Grrr.
     * So that each 32MB appears at offset 0, and offset 32, unless
     * there's really 64MB attached to the chip.
     * So, 16MB appears at offset 0, nothing between 16-32, then it re-appears
     * at offset 32.
     * This below is to detect the cases of memory combinations
     */

    /* Test first 32MB */
    for(i=0;i<32;i++) {
    	/* write test pattern */
	MMIO_OUT32(pGlint->FbBase, i*1024*1024, i*0x00345678);
	mem_barrier();
	temp1 = MMIO_IN32(pGlint->FbBase, i*1024*1024);
    	/* Let's check for wrapover, write will fail at 16MB boundary */
	if (temp1 == (i*0x00345678)) 
	    size = i;
	else 
	    break;
    }

    /* Ok, we're satisfied we've got 32MB, let's test the second lot */
    if ((size + 1) == i) {
	for(i=0;i<32;i++) {
	    /* Clear first 32MB */
	    MMIO_OUT32(pGlint->FbBase, i*1024*1024, 0);
	    mem_barrier();
	}
        for(i=32;i<64;i++) {
    	    /* write test pattern */
	    MMIO_OUT32(pGlint->FbBase, i*1024*1024, i*0x00345678);
	    mem_barrier();
	    temp1 = MMIO_IN32(pGlint->FbBase, i*1024*1024);
	    temp2 = MMIO_IN32(pGlint->FbBase, (i-32)*1024*1024);
    	    /* Let's check for wrapover */
	    if ( (temp1 == (i*0x00345678)) && (temp2 == 0) )
	        size = i;
	    else 
	        break;
	}
    }

    GLINT_SLOW_WRITE_REG(temp, PM3MemBypassWriteMask);

    xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pGlint->FbBase, 
							pGlint->FbMapSize);

    pGlint->FbBase = NULL;
    pGlint->FbMapSize = 0;

    return ( (size+1) * 1024 );
}

static unsigned long
PM3DAC_CalculateClock
(
 unsigned long ReqClock,		/* In kHz units */
 unsigned long RefClock,		/* In kHz units */
 unsigned char *prescale,		/* ClkPreScale */
 unsigned char *feedback,		/* ClkFeedBackScale */
 unsigned char *postscale		/* ClkPostScale */
 )
{
  unsigned long fMinVCO = 2000000; /* min fVCO is 200MHz (in 100Hz units) */
  unsigned long fMaxVCO = 6220000; /* max fVCO is 622MHz (in 100Hz units) */
  unsigned long fMinINTREF = 10000;/* min fINTREF is 1MHz (in 100Hz units) */
  unsigned long fMaxINTREF = 20000;/* max fINTREF is 2MHz (in 100Hz units) */
  unsigned long	M, N, P; /* M=feedback, N=prescale, P=postscale */
  unsigned long	fINTREF;
  unsigned long	fVCO;
  unsigned long	ActualClock;
  long		Error;
  unsigned long LowestError = 1000000;
  unsigned int	bFoundFreq = FALSE;
  int		cInnerLoopIterations = 0;
  int		LoopCount;
  unsigned long	ClosestClock = 0;

  ReqClock*=10; /* convert into 100Hz units */
  RefClock*=10; /* convert into 100Hz units */

  for(P = 0; P <= 5; ++P)
    {
      unsigned long fVCOLowest, fVCOHighest;

      /* it is pointless going through the main loop if all values of
         N produce an fVCO outside the acceptable range */
      N = 1;
      M = (N * (1UL << P) * ReqClock) / (2 * RefClock);
      fVCOLowest = (2 * RefClock * M) / N;
      N = 255;
      M = (N * (1UL << P) * ReqClock) / (2 * RefClock);
      fVCOHighest = (2 * RefClock * M) / N;

      if(fVCOHighest < fMinVCO || fVCOLowest > fMaxVCO)
	{
	  continue;
	}

      for(N = 1; N <= 255; ++N, ++cInnerLoopIterations)
	{
	  fINTREF = RefClock / N;
	  if(fINTREF < fMinINTREF || fINTREF > fMaxINTREF)
	    {
	      if(fINTREF > fMaxINTREF)
		{
		  /* hopefully we will get into range as the prescale
		     value increases */
		  continue;
		}
	      else
		{
		  /* already below minimum and it will only get worse:
		     move to the next postscale value */
		  break;
		}
	    }

	  M = (N * (1UL << P) * ReqClock) / (2 * RefClock);
	  if(M > 255)
	    {
	      /* M, N & P registers are only 8 bits wide */
	      break;
	    }

	  /* we can expect rounding errors in calculating M, which
	     will always be rounded down. So we will checkout our
	     calculated value of M along with (M+1) */
	  for(LoopCount = (M == 255) ? 1 : 2; --LoopCount >= 0; ++M)
	    {
	      fVCO = (2 * RefClock * M) / N;
	      if(fVCO >= fMinVCO && fVCO <= fMaxVCO)
		{
		  ActualClock = fVCO / (1UL << P);
		  Error = ActualClock - ReqClock;
		  if(Error < 0)
		    Error = -Error;
		  if(Error < LowestError)
		    {
		      bFoundFreq = TRUE;
		      LowestError = Error;
		      ClosestClock = ActualClock;
		      *prescale = N;
		      *feedback = M;
		      *postscale = P;
		      if(Error == 0)
			goto Done;
		    }
		}
	    }
	}
    }

Done:
	
  if(bFoundFreq)
    ActualClock = ClosestClock;
  else
    ActualClock = 0;
	
#if 0
  ErrorF("PM3DAC_CalculateClock: Got prescale=%d, feedback=%d, postscale=%d, WantedClock = %d00 ActualClock = %d00 (Error %d00)\n",
	 *prescale, *feedback, *postscale, ReqClock, ActualClock, LowestError);
#endif

    return(ActualClock);
}

static unsigned long
PM4DAC_CalculateClock
(
 unsigned long req_clock,		/* In kHz units */
 unsigned long ref_clock,		/* In kHz units */
 unsigned char *param_m,		/* ClkPreScale */
 unsigned char *param_n, 		/* ClkFeedBackScale */
 unsigned char *param_p			/* ClkPostScale */
 )
{
#define INITIALFREQERR 10000

  long fMinVCO = 200000;	/* min fVCO is 200MHz (in 10000Hz units) */
  long fMaxVCO = 400000;	/* max fVCO is 400MHz (in 10000Hz units) */
  unsigned long int	M, N, P;
  unsigned long int	fVCO;
  unsigned long int	ActualClock;
  int		Error;
  int		LowestError = INITIALFREQERR;
  short		bFoundFreq = FALSE;
  int		cInnerLoopIterations = 0;
  int		LoopCount;

  /*
   * Actual Equations:
   *		fVCO = (ref_clock * M)/(N+1)
   *		PIXELCLOCK = fVCO/(1<<p)
   *		200 <= fVCO <= 400
   *		24 <= N <= 80
   *		1 <= M <= 15
   *		0 <= P <= 3
   *		1Mhz < ref_clock/(N+1) <= 2Mhz - not used
   * For refclk == 14.318 we have the tighter equations:
   *		32 <= N <= 80
   *		3 <= M <= 12
   * Notes:
   *		The spec says that the PLLs will only do 260Mhz, but I have assumed 300Mhz 'cos
   *		260Mhz is a crap limit.
   */

#define	P4RD_PLL_MIN_P	0
#define	P4RD_PLL_MAX_P	3
#define	P4RD_PLL_MIN_M	1
#define	P4RD_PLL_MAX_M	12
#define	P4RD_PLL_MIN_N	24
#define	P4RD_PLL_MAX_N	80

  for(P = P4RD_PLL_MIN_P; P <= P4RD_PLL_MAX_P; ++P) {
      unsigned long int fVCOLowest, fVCOHighest;

      /* it's pointless going through the main loop if all values of 
       * N produce an fVCO outside the acceptable range */ 

      M = P4RD_PLL_MIN_M;
      N = ((M + 1) * (1 << P) * req_clock) / ref_clock;

      fVCOLowest = (ref_clock * N) / (M + 1);

      M = P4RD_PLL_MAX_M;
      N = ((M + 1) * (1 << P) * req_clock) / ref_clock;

      fVCOHighest = (ref_clock * N) / (M + 1);

      if(fVCOHighest < fMinVCO || fVCOLowest > fMaxVCO)
	  continue;

      for(M = P4RD_PLL_MIN_M; M <= P4RD_PLL_MAX_M; ++M, ++cInnerLoopIterations)
	{
	  N = ((M + 1) * (1 << P) * req_clock) / ref_clock;

	  if(N > P4RD_PLL_MAX_N || N < P4RD_PLL_MIN_N)
	      continue;

	  /*  we can expect rounding errors in calculating M, which will always be rounded down. */
	  /*  So we'll checkout our calculated value of M along with (M+1) */

	  for(LoopCount = (N == P4RD_PLL_MAX_N) ? 1 : 2; --LoopCount >= 0; ++N)
	    {
	      fVCO = (ref_clock * N) / (M + 1);
	      
	      if( (fVCO >= fMinVCO) && (fVCO <= fMaxVCO) )
		{
		  ActualClock = (fVCO / (1 << P));

		  Error = ActualClock - req_clock;

		  if(Error < 0)
		      Error = -Error;

		  /*  It is desirable that we use the lowest value of M if the*/
		  /*  frequencies are the same.*/
		  if(Error < LowestError || (Error == LowestError && M < *param_m))
		    {
		      bFoundFreq = TRUE;
		      LowestError = Error;
		      *param_m = M;
		      *param_n = N;
		      *param_p = P;
		      if(Error == 0)
			goto Done;
		    }
		}
	    }
	}
    }

Done:
  if(bFoundFreq)
    ActualClock = (ref_clock * (*param_n)) / (((*param_m) + 1) * (1 << (*param_p)));
  else
    ActualClock = 0;
  
  return(ActualClock);
}

void
Permedia3PreInit(ScrnInfoPtr pScrn)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    CARD32 LocalMemCaps;

    TRACE_ENTER("Permedia3PreInit");

    if (IS_J2000) {
    	unsigned char m,n,p;

	if (pGlint->Chipset == PCI_VENDOR_3DLABS_CHIP_GAMMA)
	    GLINT_SLOW_WRITE_REG(GCSRSecondaryGLINTMapEn, GCSRAperture);

	/* Memory timings for the Appian J2000 board.
	 * This is needed for the second head which is left un-initialized
	 * by the bios, thus freezing the machine. */
	GLINT_SLOW_WRITE_REG(0x02e311B8, PM3LocalMemCaps);
	GLINT_SLOW_WRITE_REG(0x07424905, PM3LocalMemTimings);
	GLINT_SLOW_WRITE_REG(0x0c000003, PM3LocalMemControl);
	GLINT_SLOW_WRITE_REG(0x00000061, PM3LocalMemRefresh);
	GLINT_SLOW_WRITE_REG(0x00000000, PM3LocalMemPowerDown);
	
	/* Let's program the K, M and S Clocks to the same values as the bios
	 * does for first head :
	 *   - KClk and MClk are 105Mhz.
	 *   - S Clock is set to PClk.
	 * Note 1 : pGlint->RefClock is not set yet, so use 14318 instead.
	 * Note 2 : KClk gets internally halved, so we need to double it.
	 */
	(void) PM3DAC_CalculateClock(2*105000, 14318, &m,&n,&p);
        Permedia2vOutIndReg(pScrn, PM3RD_KClkPreScale, 0x00, m);
        Permedia2vOutIndReg(pScrn, PM3RD_KClkFeedbackScale, 0x00, n);
        Permedia2vOutIndReg(pScrn, PM3RD_KClkPostScale, 0x00, p);
        Permedia2vOutIndReg(pScrn, PM3RD_KClkControl, 0x00,
	    PM3RD_KClkControl_STATE_RUN |
	    PM3RD_KClkControl_SOURCE_PLL |
	    PM3RD_KClkControl_ENABLE);
        Permedia2vOutIndReg(pScrn, PM3RD_MClkControl, 0x00,
	    PM3RD_MClkControl_STATE_RUN |
	    PM3RD_MClkControl_SOURCE_KCLK |
	    PM3RD_MClkControl_ENABLE);
        Permedia2vOutIndReg(pScrn, PM3RD_SClkControl, 0x00,
	    PM3RD_SClkControl_STATE_RUN |
	    PM3RD_SClkControl_SOURCE_PCLK |
	    PM3RD_SClkControl_ENABLE);
    }

#if defined(__alpha__)
    /*
     * On Alpha, we have to "int10" secondary VX1 cards early;
     * otherwise, some information taken from registers, like
     * memory size, is incorrect.
     */
    if (!xf86IsPrimaryPci(pGlint->PciInfo)) {
        if ( IS_QVX1 ) {

	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "VX1 secondary enabling VGA before int10\n");

	    /* Enable VGA on the current card. */
	    pciWriteByte( pGlint->PciTag, 0xf8, 0 );
	    pciWriteByte( pGlint->PciTag, 0xf4, 0 );
	    pciWriteByte( pGlint->PciTag, 0xfc, 0 );

	    /* The card we are on should be VGA-enabled now, so run int10. */
	    if (xf86LoadSubModule(pScrn, "int10")) {
	        xf86Int10InfoPtr pInt;

	        xf86LoaderReqSymLists(GLINTint10Symbols, NULL);
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Initializing int10\n");
		pInt = xf86InitInt10(pGlint->pEnt->index);
		xf86FreeInt10(pInt);
	    }

	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "VX1 secondary disabling VGA after int10\n");

	    /* Finally, disable VGA on the current card. */
	    pciWriteByte( pGlint->PciTag, 0xf8, 0x70 );
	    pciWriteByte( pGlint->PciTag, 0xf4, 0x01 );
	    pciWriteByte( pGlint->PciTag, 0xfc, 0x00 );
	}
    }
#endif /* __alpha__ */

    /* If we have SDRAM instead of SGRAM, we have to do some things
       differently in the FillRectSolid code. */       
    LocalMemCaps = GLINT_READ_REG(PM3LocalMemCaps);
    pGlint->PM3_UsingSGRAM = !(LocalMemCaps & PM3LocalMemCaps_NoWriteMask);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using %s memory\n", 
	       pGlint->PM3_UsingSGRAM ? "SGRAM" : "SDRAM");

    TRACE_EXIT("Permedia3PreInit");
}

Bool
Permedia3Init(ScrnInfoPtr pScrn, DisplayModePtr mode, GLINTRegPtr pReg)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    CARD32 temp1, temp2, temp3, temp4;

    if ((pGlint->numMultiDevices == 2) || (IS_J2000)) {
	STOREREG(GCSRAperture, GCSRSecondaryGLINTMapEn);
    }

    if (pGlint->MultiAperture) {
	STOREREG(GMultGLINTAperture, pGlint->realWidth);
	STOREREG(GMultGLINT1, 
			pGlint->MultiPciInfo[0]->memBase[2] & 0xFF800000);
	STOREREG(GMultGLINT2,
			pGlint->MultiPciInfo[1]->memBase[2] & 0xFF800000);
    }

    STOREREG(PM3MemBypassWriteMask, 	0xffffffff);
    STOREREG(Aperture0,		 	0x00000000);
    STOREREG(Aperture1,		 	0x00000000);

    if (pGlint->Chipset == PCI_VENDOR_3DLABS_CHIP_GAMMA)
    	STOREREG(DFIFODis,			0x00000001);

    STOREREG(FIFODis,			0x00000007);

    temp1 = mode->CrtcHSyncStart - mode->CrtcHDisplay;
    temp2 = mode->CrtcVSyncStart - mode->CrtcVDisplay;
    temp3 = mode->CrtcHSyncEnd - mode->CrtcHSyncStart;
    temp4 = mode->CrtcVSyncEnd - mode->CrtcVSyncStart;

    STOREREG(PMHTotal,	Shiftbpp(pScrn, mode->CrtcHTotal - 1));
    STOREREG(PMHsEnd,	Shiftbpp(pScrn, temp1 + temp3));
    STOREREG(PMHsStart,	Shiftbpp(pScrn, temp1));
    STOREREG(PMHbEnd,	Shiftbpp(pScrn, mode->CrtcHTotal - mode->CrtcHDisplay));
    STOREREG(PMHgEnd,	Shiftbpp(pScrn, mode->CrtcHTotal - mode->CrtcHDisplay));
    STOREREG(PMScreenStride, Shiftbpp(pScrn, pScrn->displayWidth));

    STOREREG(PMVTotal,	mode->CrtcVTotal - 1);
    STOREREG(PMVsEnd,	temp2 + temp4 - 1);
    STOREREG(PMVsStart,	temp2 - 1);
    STOREREG(PMVbEnd,	mode->CrtcVTotal - mode->CrtcVDisplay);

    switch (pScrn->bitsPerPixel)
    {
	case 8:
	    STOREREG(PM3ByAperture1Mode, PM3ByApertureMode_PIXELSIZE_8BIT);
	    STOREREG(PM3ByAperture2Mode, PM3ByApertureMode_PIXELSIZE_8BIT);
	    STOREREG(PMVideoControl,	 1 | (1 << 3) | (1 << 5) | (0 << 19));
	    break;
	case 16:
#if X_BYTE_ORDER != X_BIG_ENDIAN
	    STOREREG(PM3ByAperture1Mode, PM3ByApertureMode_PIXELSIZE_16BIT);
	    STOREREG(PM3ByAperture2Mode, PM3ByApertureMode_PIXELSIZE_16BIT);
#else
	    STOREREG(PM3ByAperture1Mode, PM3ByApertureMode_PIXELSIZE_16BIT |
					 PM3ByApertureMode_BYTESWAP_BADC);
	    STOREREG(PM3ByAperture2Mode, PM3ByApertureMode_PIXELSIZE_16BIT |
					 PM3ByApertureMode_BYTESWAP_BADC);
#endif
	    STOREREG(PMVideoControl,	 1 | (1 << 3) | (1 << 5) | (1 << 19));
	    break;
	case 32:
#if X_BYTE_ORDER != X_BIG_ENDIAN
	    STOREREG(PM3ByAperture1Mode, PM3ByApertureMode_PIXELSIZE_32BIT);
	    STOREREG(PM3ByAperture2Mode, PM3ByApertureMode_PIXELSIZE_32BIT);
#else
	    STOREREG(PM3ByAperture1Mode, PM3ByApertureMode_PIXELSIZE_32BIT |
					 PM3ByApertureMode_BYTESWAP_DCBA);
	    STOREREG(PM3ByAperture2Mode, PM3ByApertureMode_PIXELSIZE_32BIT |
					 PM3ByApertureMode_BYTESWAP_DCBA);
#endif
	    STOREREG(PMVideoControl,	 1 | (1 << 3) | (1 << 5) | (2 << 19));
	    break;
    }

    STOREREG(VClkCtl, GLINT_READ_REG(VClkCtl) & 0xFFFFFFFC);
    STOREREG(PMScreenBase, 0x00000000);
    STOREREG(ChipConfig, GLINT_READ_REG(ChipConfig) & 0xFFFFFFFD);

    {
	/* Get the programmable clock values */
    	unsigned char m,n,p;
	
	/* Let's program the dot clock */
	switch (pGlint->Chipset) {
	case PCI_VENDOR_3DLABS_CHIP_PERMEDIA4:
	case PCI_VENDOR_3DLABS_CHIP_R4:
	  (void) PM4DAC_CalculateClock(mode->Clock, pGlint->RefClock, &m,&n,&p);
	  break;
	case PCI_VENDOR_3DLABS_CHIP_PERMEDIA3:
	  (void) PM3DAC_CalculateClock(mode->Clock, pGlint->RefClock, &m,&n,&p);
	  break;
	case PCI_VENDOR_3DLABS_CHIP_GAMMA:
	  switch (pGlint->MultiChip) {
	    case PCI_CHIP_PERMEDIA3:
		(void) PM3DAC_CalculateClock(mode->Clock,
					    pGlint->RefClock, &m,&n,&p);
		break;
	    case PCI_CHIP_R4:
		(void) PM4DAC_CalculateClock(mode->Clock,
					    pGlint->RefClock, &m,&n,&p);
		break;
	  }
	  break;
	}
	STOREDAC(PM3RD_DClk0PreScale, m);
	STOREDAC(PM3RD_DClk0FeedbackScale, n);
	STOREDAC(PM3RD_DClk0PostScale, p);
    }

    temp1 = 0;
    temp2 = 0;
    temp3 = 0;

    if (pGlint->UseFlatPanel) {
    	temp2 |= PM3RD_DACControl_BLANK_PEDESTAL_ENABLE;
    	temp3 |= PM3RD_MiscControl_VSB_OUTPUT_ENABLE;
    	STOREREG(VSConfiguration, 0x06); 
    	STOREREG(VSBBase, 1<<14);
    }

    if (mode->Flags & V_PHSYNC) temp1 |= PM3RD_SyncControl_HSYNC_ACTIVE_HIGH;
    if (mode->Flags & V_PVSYNC) temp1 |= PM3RD_SyncControl_VSYNC_ACTIVE_HIGH;

    STOREREG(PM2VDACRDIndexControl, 0x00);
    STOREDAC(PM2VDACRDSyncControl, temp1);
    STOREDAC(PM2VDACRDDACControl, temp2);

    if (pScrn->rgbBits == 8)
	temp3 |= 0x01; /* 8bit DAC */

    switch (pScrn->bitsPerPixel)
    {
    case 8:
	STOREDAC(PM2VDACRDPixelSize, 0x00);
	STOREDAC(PM2VDACRDColorFormat, 0x2E);
    	break;
    case 16:
    	temp3 |= PM3RD_MiscControl_DIRECTCOLOR_ENABLE;
	STOREDAC(PM2VDACRDPixelSize, 0x01);
	if (pScrn->depth == 15) {
	    STOREDAC(PM2VDACRDColorFormat, 0x61);
	} else {
	    STOREDAC(PM2VDACRDColorFormat, 0x70);
	}
    	break;
    case 24:
    	temp3 |= PM3RD_MiscControl_DIRECTCOLOR_ENABLE;
	STOREDAC(PM2VDACRDPixelSize, 0x04);
	STOREDAC(PM2VDACRDColorFormat, 0x20);
    	break;
    case 32:
    	temp3 |= PM3RD_MiscControl_DIRECTCOLOR_ENABLE;
	if (pScrn->overlayFlags & OVERLAY_8_32_PLANAR) {
	    temp3 |= 0x18;
	    STOREDAC(PM2VDACRDOverlayKey, pScrn->colorKey);
	}
	STOREDAC(PM2VDACRDPixelSize, 0x02);
	STOREDAC(PM2VDACRDColorFormat, 0x20);
    	break;
    }
    STOREDAC(PM2VDACRDMiscControl, temp3);

    STOREREG(PM3FifoControl, 0x905); /* Lower the default fifo threshold */

    return(TRUE);
}

void
Permedia3Save(ScrnInfoPtr pScrn, GLINTRegPtr pReg)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int i;

    /* We can't rely on the vgahw layer copying the font information
     * back properly, due to problems with MMIO access to VGA space
     * so we memcpy the information using the slow routines */
    xf86SlowBcopy((CARD8*)pGlint->FbBase, (CARD8*)pGlint->VGAdata, 65536);

    if ((pGlint->numMultiDevices == 2) || (IS_J2000)) {
	SAVEREG(GCSRAperture);
    }

    if (pGlint->MultiAperture) {
	SAVEREG(GMultGLINTAperture);
	SAVEREG(GMultGLINT1);
	SAVEREG(GMultGLINT2);
    }

    /* Permedia 3 memory Timings */
    SAVEREG(PM3MemBypassWriteMask);
    SAVEREG(PM3ByAperture1Mode);
    SAVEREG(PM3ByAperture2Mode);
    SAVEREG(ChipConfig);
    SAVEREG(Aperture0);
    SAVEREG(Aperture1);
    SAVEREG(PM3FifoControl);

    if (pGlint->Chipset == PCI_VENDOR_3DLABS_CHIP_GAMMA)
   	SAVEREG(DFIFODis);
    SAVEREG(FIFODis);
    SAVEREG(PMHTotal);
    SAVEREG(PMHbEnd);
    SAVEREG(PMHgEnd);
    SAVEREG(PMScreenStride);
    SAVEREG(PMHsStart);
    SAVEREG(PMHsEnd);
    SAVEREG(PMVTotal);
    SAVEREG(PMVbEnd);
    SAVEREG(PMVsStart);
    SAVEREG(PMVsEnd);
    SAVEREG(PMScreenBase);
    SAVEREG(PMVideoControl);
    SAVEREG(VClkCtl);
    if (pGlint->UseFlatPanel) {
    	SAVEREG(VSConfiguration);
    	SAVEREG(VSBBase);
    }

    for (i=0;i<768;i++) {
    	Permedia2ReadAddress(pScrn, i);
	pReg->cmap[i] = Permedia2ReadData(pScrn);
    }

    SAVEREG(PM2VDACRDIndexControl);
    P2VIN(PM2VDACRDOverlayKey);
    P2VIN(PM2VDACRDSyncControl);
    P2VIN(PM2VDACRDMiscControl);
    P2VIN(PM2VDACRDDACControl);
    P2VIN(PM2VDACRDPixelSize);
    P2VIN(PM2VDACRDColorFormat);
    P2VIN(PM2VDACRDDClk0PreScale);
    P2VIN(PM2VDACRDDClk0FeedbackScale);
    P2VIN(PM2VDACRDDClk0PostScale);
}

void
Permedia3Restore(ScrnInfoPtr pScrn, GLINTRegPtr pReg)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    CARD32 temp;
    int i;

    /* We can't rely on the vgahw layer copying the font information
     * back properly, due to problems with MMIO access to VGA space
     * so we memcpy the information using the slow routines */
    if (pGlint->STATE)
	xf86SlowBcopy((CARD8*)pGlint->VGAdata, (CARD8*)pGlint->FbBase, 65536);

    if ((pGlint->numMultiDevices == 2) || (IS_J2000)) {
	RESTOREREG(GCSRAperture);
    }

    if (pGlint->MultiAperture) {
	RESTOREREG(GMultGLINTAperture);
	RESTOREREG(GMultGLINT1);
	RESTOREREG(GMultGLINT2);
    }

    /* Permedia 3 memory Timings */
    RESTOREREG(PM3MemBypassWriteMask);
    RESTOREREG(PM3ByAperture1Mode);
    RESTOREREG(PM3ByAperture2Mode);
    RESTOREREG(ChipConfig);
    RESTOREREG(Aperture0);
    RESTOREREG(Aperture1);
    RESTOREREG(PM3FifoControl);
    if (pGlint->Chipset == PCI_VENDOR_3DLABS_CHIP_GAMMA)
    	RESTOREREG(DFIFODis);
    RESTOREREG(FIFODis);
    RESTOREREG(PMVideoControl);
    RESTOREREG(PMHbEnd);
    RESTOREREG(PMHgEnd);
    RESTOREREG(PMScreenBase);
    RESTOREREG(VClkCtl);
    RESTOREREG(PMScreenStride);
    RESTOREREG(PMHTotal);
    RESTOREREG(PMHsStart);
    RESTOREREG(PMHsEnd);
    RESTOREREG(PMVTotal);
    RESTOREREG(PMVbEnd);
    RESTOREREG(PMVsStart);
    RESTOREREG(PMVsEnd);

    if (pGlint->UseFlatPanel) {
    	RESTOREREG(VSConfiguration);
    	RESTOREREG(VSBBase);
    }

    RESTOREREG(PM2VDACRDIndexControl);
    P2VOUT(PM2VDACRDOverlayKey);
    P2VOUT(PM2VDACRDSyncControl);
    P2VOUT(PM2VDACRDMiscControl);
    P2VOUT(PM2VDACRDDACControl);
    P2VOUT(PM2VDACRDPixelSize);
    P2VOUT(PM2VDACRDColorFormat);

    for (i=0;i<768;i++) {
    	Permedia2WriteAddress(pScrn, i);
	Permedia2WriteData(pScrn, pReg->cmap[i]);
    }

    temp = Permedia2vInIndReg(pScrn, PM2VDACIndexClockControl) & 0xFC;
    P2VOUT(PM2VDACRDDClk0PreScale);
    P2VOUT(PM2VDACRDDClk0FeedbackScale);
    P2VOUT(PM2VDACRDDClk0PostScale);
    Permedia2vOutIndReg(pScrn, PM2VDACIndexClockControl, 0x00, temp|0x03);
}

void Permedia3LoadPalette(
    ScrnInfoPtr pScrn, 
    int numColors, 
    int *indices,
    LOCO *colors,
    VisualPtr pVisual
){
#if 0
    GLINTPtr pGlint = GLINTPTR(pScrn);
#endif
    int i, index, shift = 0, j, repeat = 1;

    if (pScrn->depth == 15) {
	repeat = 8;
	shift = 3;
    }

    for(i = 0; i < numColors; i++) {
	index = indices[i];
	for (j = 0; j < repeat; j++) {
	    Permedia2WriteAddress(pScrn, (index << shift)+j);
	    Permedia2WriteData(pScrn, colors[index].red);
	    Permedia2WriteData(pScrn, colors[index].green);
	    Permedia2WriteData(pScrn, colors[index].blue);
	}
#if 0
        GLINT_SLOW_WRITE_REG(index, PM3LUTIndex);
	GLINT_SLOW_WRITE_REG((colors[index].red & 0xFF) |
			     ((colors[index].green & 0xFF) << 8) |
			     ((colors[index].blue & 0xFF) << 16),
			     PM3LUTData);
#endif
    }
}

/* special one for 565 mode */
void Permedia3LoadPalette16(
    ScrnInfoPtr pScrn, 
    int numColors, 
    int *indices,
    LOCO *colors,
    VisualPtr pVisual
){
#if 0
    GLINTPtr pGlint = GLINTPTR(pScrn);
#endif
    int i, index, j;

    for(i = 0; i < numColors; i++) {
	index = indices[i];
	for (j = 0; j < 4; j++) {
	    Permedia2WriteAddress(pScrn, (index << 2)+j);
	    Permedia2WriteData(pScrn, colors[index >> 1].red);
	    Permedia2WriteData(pScrn, colors[index].green);
	    Permedia2WriteData(pScrn, colors[index >> 1].blue);
	}
#if 0
        GLINT_SLOW_WRITE_REG(index, PM3LUTIndex);
	GLINT_SLOW_WRITE_REG((colors[index].red & 0xFF) |
			     ((colors[index].green & 0xFF) << 8) |
			     ((colors[index].blue & 0xFF) << 16),
			     PM3LUTData);
#endif

	if(index <= 31) {
	    for (j = 0; j < 4; j++) {
	    	Permedia2WriteAddress(pScrn, (index << 3)+j);
	    	Permedia2WriteData(pScrn, colors[index].red);
	    	Permedia2WriteData(pScrn, colors[(index << 1) + 1].green);
	    	Permedia2WriteData(pScrn, colors[index].blue);
	    }
	}
    }
}
