/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tdfx/tdfx_sli.c,v 1.7 2001/03/21 17:02:26 dawes Exp $ */

#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86_OSproc.h"
#include "xf86Pci.h"
#include "tdfx.h"

#define AACLKOUTDEL 0x2
#define CFGSWAPALGORITHM 0x1

/* #define RD_ABORT_ERROR */
#define H3VDD

Bool TDFXDisableSLI(TDFXPtr pTDFX)
{
  int i;
  int v;

  for (i=0; i<pTDFX->numChips; i++) {
    v=pciReadLong(pTDFX->PciTag[i], CFG_INIT_ENABLE);
    pciWriteLong(pTDFX->PciTag[i], CFG_INIT_ENABLE,
		 v&~(CFG_SNOOP_MEMBASE0 | CFG_SNOOP_EN | CFG_SNOOP_MEMBASE0_EN |
		     CFG_SNOOP_MEMBASE1_EN | CFG_SNOOP_SLAVE |
		     CFG_SNOOP_FBIINIT_WR_EN | CFG_SWAP_ALGORITHM |
		     CFG_SWAP_QUICK));
    v=pciReadLong(pTDFX->PciTag[i], CFG_SLI_LFB_CTRL);
    pciWriteLong(pTDFX->PciTag[i], CFG_SLI_LFB_CTRL, 
		 v&~(CFG_SLI_LFB_CPU_WR_EN | CFG_SLI_LFB_DPTCH_WR_EN | 
		     CFG_SLI_RD_EN));
#ifdef H3VDD
    pTDFX->writeChipLong(pTDFX, i, SST_3D_SLICTRL, 0);
    pTDFX->writeChipLong(pTDFX, i, SST_3D_AACTRL, 0);
#endif
    v=pciReadLong(pTDFX->PciTag[i], CFG_AA_LFB_CTRL);
    pciWriteLong(pTDFX->PciTag[i], CFG_AA_LFB_CTRL,
		 v&~(CFG_AA_LFB_CPU_WR_EN | CFG_AA_LFB_DPTCH_WR_EN |
		     CFG_AA_LFB_RD_EN));
    v=pciReadLong(pTDFX->PciTag[i], CFG_SLI_AA_MISC);
    pciWriteLong(pTDFX->PciTag[i], CFG_SLI_AA_MISC,
		 (v&~CFG_VGA_VSYNC_OFFSET) |
		 (0 << CFG_VGA_VSYNC_OFFSET_PIXELS_SHIFT) |
		 (0 << CFG_VGA_VSYNC_OFFSET_CHARS_SHIFT) |
		 (0 << CFG_VGA_VSYNC_OFFSET_HXTRA_SHIFT));
    pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0, 0);
    pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1, 0);
    pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2, 0);

    if (pTDFX->numChips>1) {
      v=pTDFX->readChipLong(pTDFX, i, PCIINIT0);
      pTDFX->writeChipLong(pTDFX, i, PCIINIT0,
			   (v&~(SST_PCI_DISABLE_IO|SST_PCI_DISABLE_MEM|
				SST_PCI_RETRY_INTERVAL)) |
			   (0<<SST_PCI_RETRY_INTERVAL_SHIFT) |
			   SST_PCI_FORCE_FB_HIGH);
    } else {
      v=pTDFX->readChipLong(pTDFX, i, PCIINIT0);
      pTDFX->writeChipLong(pTDFX, i, PCIINIT0,
			   (v&~(SST_PCI_DISABLE_IO|SST_PCI_DISABLE_MEM|
				SST_PCI_RETRY_INTERVAL)) |
			   (0<<SST_PCI_RETRY_INTERVAL_SHIFT));
    }

#if 0
    if (i>0) {
      pTDFX->writeChipLong(pTDFX, i, DACMODE, 
			   SST_DAC_DPMS_ON_VSYNC | SST_DAC_DPMS_ON_HSYNC);
      v=pTDFX->readChipLong(pTDFX, i, VIDPROCCFG);
      pTDFX->writeChipLong(pTDFX, i, VIDPROCCFG, v&~SST_VIDEO_PROCESSOR_EN);
    }
#endif
  }
  return TRUE;
}

Bool TDFXSetupSLI(ScrnInfoPtr pScrn, Bool sliEnable, int aaSamples)
{
  TDFXPtr pTDFX;
  int i, sliLines, sliLinesLog2, nChipsLog2, v;
  int sli_renderMask, sli_compareMask, sli_scanMask;
  int sliAnalog, dwFormat;

  pTDFX=TDFXPTR(pScrn);
  if (pScrn->depth == 24 || pScrn->depth==32) {
    if ((aaSamples == 4) && (pTDFX->numChips>1)) {
      pTDFX->pixelFormat=GR_PIXFMT_AA_4_ARGB_8888;
    } else if (aaSamples >= 2) {
      pTDFX->pixelFormat=GR_PIXFMT_AA_2_ARGB_8888;
    } else {
      pTDFX->pixelFormat=GR_PIXFMT_ARGB_8888;
    }
  } else if (pScrn->depth == 16) {
    if ((aaSamples == 4) && (pTDFX->numChips>1)) {
      pTDFX->pixelFormat=GR_PIXFMT_AA_4_RGB_565;
    } else if (aaSamples >= 2) {
      pTDFX->pixelFormat=GR_PIXFMT_AA_2_RGB_565;
    } else {
      pTDFX->pixelFormat=GR_PIXFMT_RGB_565;
    }
  } else if (pScrn->depth == 8) {
    pTDFX->pixelFormat=GR_PIXFMT_I_8;
  }
  if (!sliEnable && !aaSamples) { /* Turn off */
    return TDFXDisableSLI(pTDFX);
  }

  if (pScrn->virtualY>768) sliLinesLog2=5;
  else sliLinesLog2=4;
  sliLines=1<<sliLinesLog2;
  if (pScrn->virtualY*pScrn->virtualX>1600*1024) sliAnalog=1;
  else sliAnalog=0;
  /* XXX We need to avoid SLI in double scan modes somehow */

  switch (pTDFX->numChips) {
  case 1:
    nChipsLog2=0;
    break;
  case 2:
    nChipsLog2=1;
    break;
  case 4:
    nChipsLog2=2;
    break;
  default:
    return FALSE;
    /* XXX Huh? Unsupported configuration */
  }

  for (i=0; i<pTDFX->numChips; i++) {
    /* Do we want to set these differently for a VIA board? */
    v=pTDFX->readChipLong(pTDFX, i, PCIINIT0);
    v=(v&~(SST_PCI_RETRY_INTERVAL|SST_PCI_FORCE_FB_HIGH)) |
		     SST_PCI_READ_WS | SST_PCI_WRITE_WS |
		     SST_PCI_DISABLE_IO | SST_PCI_DISABLE_MEM |
		     (5<<SST_PCI_RETRY_INTERVAL_SHIFT);
    pTDFX->writeChipLong(pTDFX, i, PCIINIT0,
			 (v&~(SST_PCI_RETRY_INTERVAL|SST_PCI_FORCE_FB_HIGH)) |
			 SST_PCI_READ_WS | SST_PCI_WRITE_WS |
			 SST_PCI_DISABLE_IO | SST_PCI_DISABLE_MEM |
			 (5<<SST_PCI_RETRY_INTERVAL_SHIFT));
    v=pTDFX->readChipLong(pTDFX, i, TMUGBEINIT);
    pTDFX->writeChipLong(pTDFX, i, TMUGBEINIT, 
			 (v&~(SST_AA_CLK_DELAY | SST_AA_CLK_INVERT)) |
			 (AACLKOUTDEL<<SST_AA_CLK_DELAY_SHIFT) | 
			 SST_AA_CLK_INVERT);

    if (pTDFX->numChips>1) {
      v=pciReadLong(pTDFX->PciTag[i], CFG_INIT_ENABLE);
      pciWriteLong(pTDFX->PciTag[i], CFG_INIT_ENABLE, v |
		   (CFGSWAPALGORITHM << CFG_SWAPBUFFER_ALGORITHM_SHIFT) |
		   CFG_SWAP_ALGORITHM | ((!i)? CFG_SWAP_MASTER : 0));
      if (!i) {
	v=pciReadLong(pTDFX->PciTag[i], CFG_INIT_ENABLE);
	pciWriteLong(pTDFX->PciTag[i], CFG_INIT_ENABLE, 
			 v | CFG_SNOOP_EN);
	v=pciReadLong(pTDFX->PciTag[i], CFG_PCI_DECODE);
      } else {
	v=pciReadLong(pTDFX->PciTag[i], CFG_INIT_ENABLE);
	v=(v & ~CFG_SNOOP_MEMBASE0) | CFG_SNOOP_EN | 
	  CFG_SNOOP_MEMBASE0_EN | CFG_SNOOP_MEMBASE1_EN |
	  CFG_SNOOP_SLAVE | CFG_SNOOP_FBIINIT_WR_EN |
	  (((pTDFX->MMIOAddr[0]>>22)&0x3ff)<<CFG_SNOOP_MEMBASE0_SHIFT) |
	  ((pTDFX->numChips>2)? CFG_SWAP_QUICK : 0);
	pciWriteLong(pTDFX->PciTag[i], CFG_INIT_ENABLE, v);
	v=pciReadLong(pTDFX->PciTag[i], CFG_PCI_DECODE);
	v=(v & ~CFG_SNOOP_MEMBASE1) |
	  ((pTDFX->LinearAddr[0]>>22)&0x3ff)<<CFG_SNOOP_MEMBASE1_SHIFT;
	pciWriteLong(pTDFX->PciTag[i], CFG_PCI_DECODE, v);
      }
    }

    if (sliEnable && aaSamples<4) {
      /* SLI is on and we're using less than 4 AA samples */
      sli_renderMask = (pTDFX->numChips-1) << sliLinesLog2;
      sli_compareMask = i << sliLinesLog2;
      sli_scanMask = sliLines - 1;
      v = (sli_renderMask << CFG_SLI_LFB_RENDERMASK_SHIFT) |
	(sli_compareMask << CFG_SLI_LFB_COMPAREMASK_SHIFT) |
	(sli_scanMask << CFG_SLI_LFB_SCANMASK_SHIFT) |
	(nChipsLog2 << CFG_SLI_LFB_NUMCHIPS_LOG2_SHIFT) |
	CFG_SLI_LFB_CPU_WR_EN | CFG_SLI_LFB_DPTCH_WR_EN;
#ifndef RD_ABORT_ERROR
      v|=CFG_SLI_RD_EN;
#endif
      pciWriteLong(pTDFX->PciTag[i], CFG_SLI_LFB_CTRL, v);

#ifdef H3VDD
      pTDFX->writeChipLong(pTDFX, i, SST_3D_SLICTRL,
			   (sli_renderMask << SLICTL_3D_RENDERMASK_SHIFT) |
			   (sli_compareMask << SLICTL_3D_COMPAREMASK_SHIFT) |
			   (sli_scanMask << SLICTL_3D_SCANMASK_SHIFT) |
			   (nChipsLog2 << SLICTL_3D_NUMCHIPS_LOG2_SHIFT) |
			   SLICTL_3D_EN);
#endif
    } else if (!sliEnable && aaSamples) {
      /* SLI is off and AA is on */
      sli_renderMask = 0;
      sli_compareMask = 0;
      sli_scanMask = 0;
      pciWriteLong(pTDFX->PciTag[i], CFG_SLI_LFB_CTRL,
		   (sli_renderMask << CFG_SLI_LFB_RENDERMASK_SHIFT) |
		   (sli_compareMask << CFG_SLI_LFB_COMPAREMASK_SHIFT) |
		   (sli_scanMask << CFG_SLI_LFB_SCANMASK_SHIFT) |
		   (0x0 << CFG_SLI_LFB_NUMCHIPS_LOG2_SHIFT));
#ifdef H3VDD
      pTDFX->writeChipLong(pTDFX, i, SST_3D_SLICTRL,
			   (sli_renderMask << SLICTL_3D_RENDERMASK_SHIFT) |
			   (sli_compareMask << SLICTL_3D_COMPAREMASK_SHIFT) |
			   (sli_scanMask << SLICTL_3D_SCANMASK_SHIFT) |
			   (0 << SLICTL_3D_NUMCHIPS_LOG2_SHIFT));
#endif
    } else {
      /* SLI is on && aaSamples=4 */
      sli_renderMask = ((pTDFX->numChips>>1)-1) << sliLinesLog2;
      sli_compareMask = (i>>1) << sliLinesLog2;
      sli_scanMask = sliLines - 1;
      v = (sli_renderMask << CFG_SLI_LFB_RENDERMASK_SHIFT) |
	(sli_compareMask << CFG_SLI_LFB_COMPAREMASK_SHIFT) |
	(sli_scanMask << CFG_SLI_LFB_SCANMASK_SHIFT) |
	((nChipsLog2-1) << CFG_SLI_LFB_NUMCHIPS_LOG2_SHIFT) |
	CFG_SLI_LFB_CPU_WR_EN | CFG_SLI_LFB_DPTCH_WR_EN;
#ifndef RD_ABORT_ERROR
      v|=CFG_SLI_RD_EN;
#endif
      pciWriteLong(pTDFX->PciTag[i], CFG_SLI_LFB_CTRL, v);
#ifdef H3VDD
      pTDFX->writeChipLong(pTDFX, i, SST_3D_SLICTRL,
			   (sli_renderMask << SLICTL_3D_RENDERMASK_SHIFT) |
			   (sli_compareMask << SLICTL_3D_COMPAREMASK_SHIFT) |
			   (sli_scanMask << SLICTL_3D_SCANMASK_SHIFT) |
			   ((nChipsLog2-1) << SLICTL_3D_NUMCHIPS_LOG2_SHIFT) |
			   SLICTL_3D_EN);
#endif
    }

    TDFXSetLFBConfig(pTDFX);
    if (pTDFX->cpp==2) dwFormat = CFG_AA_LFB_RD_FORMAT_16BPP;
    else dwFormat = CFG_AA_LFB_RD_FORMAT_32BPP;
    if (pTDFX->numChips==2 && !sliEnable && aaSamples==2)
      dwFormat|=CFG_AA_LFB_RD_DIVIDE_BY_4;
    /* Thess are wrong, because we don't know where the secondary buffers
       are located */
    pTDFX->writeChipLong(pTDFX, i, CFG_AA_LFB_CTRL, 
			 (pScrn->videoRam<<10 /* 2nd buf */ << CFG_AA_BASEADDR_SHIFT) |
			 CFG_AA_LFB_CPU_WR_EN | CFG_AA_LFB_DPTCH_WR_EN |
			 CFG_AA_LFB_RD_EN | dwFormat |
			 ((aaSamples==4)?CFG_AA_LFB_RD_DIVIDE_BY_4:0));
    pTDFX->writeChipLong(pTDFX, i, CFG_AA_ZBUFF_APERTURE,
			 ((pTDFX->depthOffset>>12)<<CFG_AA_DEPTH_BUFFER_BEG_SHIFT) |
			 ((pScrn->videoRam>>2)<<CFG_AA_DEPTH_BUFFER_END_SHIFT));

    if (pTDFX->numChips>1 && i && (aaSamples || sliEnable)) {
      int vsyncOffsetPixels, vsyncOffsetChars, vsyncOffsetHXtra;

      if (aaSamples || (pTDFX->numChips==4 && sliEnable && aaSamples==4 &&
			sliAnalog && i==3)) {
	vsyncOffsetPixels=7;
	vsyncOffsetChars=4;
	vsyncOffsetHXtra=0;
      } else {
	vsyncOffsetPixels=7;
	vsyncOffsetChars=5;
	vsyncOffsetHXtra=0;
      }
      v=pciReadLong(pTDFX->PciTag[i], CFG_SLI_AA_MISC);
      pciWriteLong(pTDFX->PciTag[i], CFG_SLI_AA_MISC, 
		   (v&~CFG_VGA_VSYNC_OFFSET) |
		   (vsyncOffsetPixels << CFG_VGA_VSYNC_OFFSET_PIXELS_SHIFT) |
		   (vsyncOffsetChars << CFG_VGA_VSYNC_OFFSET_CHARS_SHIFT) |
		   (vsyncOffsetHXtra <<
		    CFG_VGA_VSYNC_OFFSET_HXTRA_SHIFT));
    }
    if (pTDFX->numChips==1 && aaSamples) {
      /* 1 chip 2 AA */
      pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		   CFG_ENHANCED_VIDEO_EN | 
		   CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
		   CFG_VIDEO_OTHERMUX_SEL_PIPE<<CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT |
		   CFG_DIVIDE_VIDEO_BY_2);
      pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		   0x0 << CFG_SLI_RENDERMASK_FETCH_SHIFT |
		   0x0 << CFG_SLI_COMPAREMASK_FETCH_SHIFT |
		   0x0 << CFG_SLI_RENDERMASK_CRT_SHIFT |
		   0x0 << CFG_SLI_COMPAREMASK_CRT_SHIFT);
      pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		   0x0 << CFG_SLI_RENDERMASK_AAFIFO_SHIFT |
		   0xff << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT);
    } else if (pTDFX->numChips==2 && !sliEnable && aaSamples==4 &&
	       !sliAnalog) {
      /* 2 chips 4 digital AA */
      if (!i) {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN | 
		     CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE_PLUS_AAFIFO <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_4);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     (0x0 << CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (0x0 << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      } else {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     (CFG_ENHANCED_VIDEO_EN |
		      CFG_ENHANCED_VIDEO_SLV |
		      CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
		      (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		       CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		      CFG_DIVIDE_VIDEO_BY_1));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     (0x0 << CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     (0xff << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (0x0 << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      }
    } else if (pTDFX->numChips==2 && !sliEnable && aaSamples==4 && sliAnalog) {
      /* 2 chips 4 analog AA */
      if (!i) {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     (CFG_ENHANCED_VIDEO_EN |
		      CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
		      (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		       CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		      CFG_DIVIDE_VIDEO_BY_4));
      } else {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_ENHANCED_VIDEO_SLV |
		     CFG_DAC_HSYNC_TRISTATE |
		     CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_4);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     (0x0 << CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (0x0 << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      }
    } else if (pTDFX->numChips==2 && sliEnable && !aaSamples && !sliAnalog) {
      /* 2 chips 2 digital SLI */
      if (!i) {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     (CFG_ENHANCED_VIDEO_EN |
		      (CFG_VIDEO_OTHERMUX_SEL_AAFIFO <<
		       CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		      (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		       CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
		      CFG_DIVIDE_VIDEO_BY_1));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     ((0x1<<sliLinesLog2) << CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     ((0x0<<sliLinesLog2) << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     ((0x1<<sliLinesLog2) << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      } else {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_ENHANCED_VIDEO_SLV |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_1);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     (((pTDFX->numChips-1)<<sliLinesLog2) <<
		      CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     ((i<<sliLinesLog2) << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     (0xff << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (((pTDFX->numChips-1)<<sliLinesLog2) <<
		      CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     ((i<<sliLinesLog2) <<
		      CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      }
    } else if (pTDFX->numChips>=2 && sliEnable && !aaSamples && sliAnalog) {
      /* 2 or 4 chips 2/4 analog SLI */
      if (!i) {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_1);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     (((pTDFX->numChips-1)<<sliLinesLog2) <<
		      CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     (((pTDFX->numChips-1)<<sliLinesLog2) << 
		      CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (0x0 << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     (0xff << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      } else {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_ENHANCED_VIDEO_SLV |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_1);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     (((pTDFX->numChips-1)<<sliLinesLog2) <<
		      CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     ((i<<sliLinesLog2) << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     (((pTDFX->numChips-1)<<sliLinesLog2) << 
		      CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     ((i<<sliLinesLog2) <<
		      CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (0x0 << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     (0xff << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      }
    } else if (pTDFX->numChips==2 && sliEnable && aaSamples==2 && !sliAnalog) {
      /* 2 chips 2 AA 2 digital SLI */
      if (!i) {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
		     (CFG_VIDEO_OTHERMUX_SEL_AAFIFO <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_2);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     ((0x1<<sliLinesLog2) << CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     ((0x1<<sliLinesLog2) << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     ((0x1<<sliLinesLog2) <<
		      CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      } else {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_ENHANCED_VIDEO_SLV |
		     CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_1);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     (((pTDFX->numChips-1)<<sliLinesLog2) <<
		      CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     ((i<<sliLinesLog2) << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     (0xff << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (((pTDFX->numChips-1)<<sliLinesLog2) <<
		      CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     ((i<<sliLinesLog2) <<
		      CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      }
    } else if (pTDFX->numChips==2 && !sliEnable && aaSamples==2 && !sliAnalog) {
      /* 2 chips 2 digital AA */
      if (!i) {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE_PLUS_AAFIFO <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_2);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     (0x0 << CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (0x0 << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      } else {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_ENHANCED_VIDEO_SLV |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_1);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     (0x0 << CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     (0xff << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (0x0 << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      }
    } else if (pTDFX->numChips==2 && !sliEnable && aaSamples==2 && sliAnalog) {
      /* 2 chips 2 analog AA */
      if (!i) {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_2);
      } else {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_ENHANCED_VIDEO_SLV |
		     CFG_DAC_HSYNC_TRISTATE |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_2);
      }
      pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		   (0x0 << CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		   (0x0 << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		   (0x0 << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		   (0x0 << CFG_SLI_COMPAREMASK_CRT_SHIFT));
      pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		   (0x0 << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		   (0x0 << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
    } else if (pTDFX->numChips>=2 && sliEnable && aaSamples==2 && sliAnalog) {
      /* 2 or 4 chips 2 AA 2 or 4 analog SLI */
      if (!i) {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_2);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     (((pTDFX->numChips-1)<<sliLinesLog2) <<
		      CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     (((pTDFX->numChips-1)<<sliLinesLog2) << 
		      CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (0x0 << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     (0xff << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      } else {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_ENHANCED_VIDEO_SLV |
		     CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_2);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     (((pTDFX->numChips-1)<<sliLinesLog2) <<
		      CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     ((i<<sliLinesLog2) << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     (((pTDFX->numChips-1)<<sliLinesLog2) << 
		      CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     ((i<<sliLinesLog2) << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (0x0 << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     (0xff << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      }
    } else if (pTDFX->numChips==4 && sliEnable && !aaSamples && !sliAnalog) {
      /* 4 chips 4 digital SLI */
      if (!i) {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     (CFG_VIDEO_OTHERMUX_SEL_AAFIFO <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
		     CFG_SLI_AAFIFO_COMPARE_INV |
		     CFG_DIVIDE_VIDEO_BY_1);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     (((pTDFX->numChips-1)<<sliLinesLog2) <<
		      CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (((pTDFX->numChips-1)<<sliLinesLog2) <<
		      CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     ((0x0<<sliLinesLog2) <<
		      CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      } else {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_ENHANCED_VIDEO_SLV |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_1);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     (((pTDFX->numChips-1)<<sliLinesLog2) <<
		      CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     ((i<<sliLinesLog2) << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     (0xff << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (((pTDFX->numChips-1)<<sliLinesLog2) <<
		      CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     ((i<<sliLinesLog2) << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      }
    } else if (pTDFX->numChips==4 && sliEnable && aaSamples==2 && !sliAnalog) {
      /* 4 chips 2 AA 4 digital SLI */
      if (!i) {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
		     (CFG_VIDEO_OTHERMUX_SEL_AAFIFO <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
		     CFG_SLI_AAFIFO_COMPARE_INV |
		     CFG_DIVIDE_VIDEO_BY_2);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     (((pTDFX->numChips-1)<<sliLinesLog2) <<
		      CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (((pTDFX->numChips-1)<<sliLinesLog2) <<
		      CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     ((0x0<<sliLinesLog2) << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      } else {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_ENHANCED_VIDEO_SLV |
		     CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_1);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     (((pTDFX->numChips-1)<<sliLinesLog2) <<
		      CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     ((i<<sliLinesLog2) << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     (0x0 << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     (0xff << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (((pTDFX->numChips-1)<<sliLinesLog2) <<
		      CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     ((i<<sliLinesLog2) << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      }
    } else if (pTDFX->numChips==4 && sliEnable && aaSamples==4 && !sliAnalog) {
      /* 4 chips 4 AA 2 digital SLI */
      if (!i) {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE_PLUS_AAFIFO <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_4);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     ((0x1<<sliLinesLog2) << CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     ((0x0<<sliLinesLog2) << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     ((0x1<<sliLinesLog2) << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     ((0x0<<sliLinesLog2) << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (0x0 << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      } else if (i==1 || i==3) {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_ENHANCED_VIDEO_SLV |
		     CFG_DAC_HSYNC_TRISTATE |
		     CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_1);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     ((0x1<<sliLinesLog2) << CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     ((((i+1)>>2)<<sliLinesLog2) << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     ((0x0<<sliLinesLog2) << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     ((0xff<<sliLinesLog2) << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (0x0 << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      } else {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_ENHANCED_VIDEO_SLV |
		     CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE_PLUS_AAFIFO <<
		      CFG_VIDEO_OTHERMUX_SEL_FALSE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_4);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     ((0x1<<sliLinesLog2) << CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     ((0x1<<sliLinesLog2) << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     ((0x1<<sliLinesLog2) << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     ((0x1<<sliLinesLog2) << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (0x0 << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     (0xff << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      }
    } else if (pTDFX->numChips==4 && sliEnable && aaSamples==4 && sliAnalog) {
      /* 4 chips 4 AA 2 analog SLI */
      if (!i) {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_4);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     ((0x1<<sliLinesLog2) << CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     ((0x0<<sliLinesLog2) << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     ((0x1<<sliLinesLog2) << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     ((0x0<<sliLinesLog2) << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (0x0 << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      } else if (i==1 || i==3) {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_ENHANCED_VIDEO_SLV |
		     CFG_DAC_HSYNC_TRISTATE |
		     CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_4);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     ((0x1<<sliLinesLog2) << CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     ((((i+1)>>2)<<sliLinesLog2) << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     ((0x1<<sliLinesLog2) << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     ((((i+1)>>2)<<sliLinesLog2) << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (0x0 << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      } else {
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		     CFG_ENHANCED_VIDEO_EN |
		     CFG_ENHANCED_VIDEO_SLV |
		     CFG_VIDEO_LOCALMUX_DESKTOP_PLUS_OVERLAY |
		     (CFG_VIDEO_OTHERMUX_SEL_PIPE <<
		      CFG_VIDEO_OTHERMUX_SEL_TRUE_SHIFT) |
		     CFG_DIVIDE_VIDEO_BY_4);
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL1,
		     ((0x1<<sliLinesLog2) << CFG_SLI_RENDERMASK_FETCH_SHIFT) |
		     ((0x1<<sliLinesLog2) << CFG_SLI_COMPAREMASK_FETCH_SHIFT) |
		     ((0x1<<sliLinesLog2) << CFG_SLI_RENDERMASK_CRT_SHIFT) |
		     ((0x1<<sliLinesLog2) << CFG_SLI_COMPAREMASK_CRT_SHIFT));
	pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL2,
		     (0x0 << CFG_SLI_RENDERMASK_AAFIFO_SHIFT) |
		     (0x0 << CFG_SLI_COMPAREMASK_AAFIFO_SHIFT));
      }
    }
    if (pTDFX->numChips==4 && sliEnable && aaSamples==4 && i==3) {
      v=pciReadLong(pTDFX->PciTag[i], CFG_SLI_AA_MISC);
      pciWriteLong(pTDFX->PciTag[i], CFG_SLI_AA_MISC, 
		   v | CFG_AA_LFB_RD_SLV_WAIT);
    }
    if (i) {
      v=pciReadLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0);
      pciWriteLong(pTDFX->PciTag[i], CFG_VIDEO_CTRL0,
		   v|CFG_VIDPLL_SEL);
      v=pTDFX->readChipLong(pTDFX, i, MISCINIT1);
      pTDFX->writeChipLong(pTDFX, i, MISCINIT1, v|SST_POWERDOWN_DAC);
    }
  }
  return TRUE;
}
