/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/rendition/commonregs.h,v 1.1 1999/11/19 13:54:44 hohndel Exp $ */

#ifndef __COMMONREGS_H__
#define __COMMONREGS_H__


#define FIFO_SIZE	0x1f

/* IO register offsets. */
#define FIFO_SWAP_NO    0x00 /* FIFO. No byte swap. */
#define FIFO_SWAP_END   0x04 /* FIFO. Swap bytes 3<>0, 2<>1. */
#define FIFO_SWAP_INHW  0x08 /* FIFO. Swap bytes 3<>2, 1<>0. */
#define FIFO_SWAP_HW    0x0c /* FIFO. Swap half-words. */
#define FIFOINFREE      0x40 /* Input FIFO free entry count. */
#define FIFOOUTVALID    0x41 /* Output FIFO valid entry count. */
#define COMM            0x42 /* dual 4 bit communications ports */
#define MEMENDIAN       0x43 /* set byte swapping on PCI mem accesses */
#define INTR            0x44 /* which interrupts occurred */
#define INTREN          0x46 /* enable different interrupts */
#define DEBUGREG        0x48 /* soft resets, RISC hold/single step */
#define LOWWATERMARK    0x49 /* Input FIFO low water mark for interrupt */
#define PCITEST         0x4C /* PCI test */
#define DMACMDPTR       0x50 /* DMA command list pointer */
#define DMA_ADDRESS     0x54 /* DMA data address */
#define DMA_COUNT       0x58 /* DMA remaining transfer count */
#define STATEINDEX      0x60 /* state index info */
#define STATEDATA       0x64 /* state data info */
#define SCRATCH         0x70 /* 16-bit BIOS scratch space */
#define MODEREG         0x72 /* Mode -- to differentiate from old MODE */
#define MODE_           MODEREG
#define	MODE		MODEREG
#define BANKSELECT      0x74 /* Local memory to A0000 mapping */
#define	BANKSELECT_PHYSADDR	((unsigned long)(0xA0000))
#define CRTCTEST        0x80 /* CRTC test register */
#define CRTCCTL         0x84 /* CRTC mode */
#define CRTCHORZ        0x88 /* CRTC horizontal timing */
#define CRTCVERT        0x8c /* CRTC vertical timing */
#define FRAMEBASEB      0x90 /* Stereoscopic frame base b address */
#define FRAMEBASEA      0x94 /* Frame base A address */
#define CRTCOFFSET      0x98 /* CRTC StrideOffset */
#define CRTCSTATUS      0x9c /* CRTC video scan position */
#define DRAMCTL         0xa0 /* DRAM timing */
#define PALETTE         0xb0 /* Access to DAC */
#define RAMDACBASEADDR  0xb0 /* Access to DAC */
#define DEVICE0         0xc0 /* external device 0 (PLL) */
#define DEVICE1         0xd0 /* external device 1 */

/* IO register flag bits */
/* _MASK defined for multi-bit values */
/* _ADDR defined for registers accessible from RISC */

/* COMM */
#define SYSSTATUS_MASK  0x0f /* host->RISC comm */
#define SYSSTATUS_SHIFT 0
#define RISCSTATUS_MASK 0xf0 /* RISC->host comm r/o */
#define RISCSTATUS_SHIFT 4
 
/* MEMENDIAN */
#define MEMENDIAN_NO    0       /* No byte swap. */
#define MEMENDIAN_END   1       /* Swap bytes 3<>0, 2<>1. */
#define MEMENDIAN_INHW  2       /* Swap bytes 3<>2, 1<>0. */
#define MEMENDIAN_HW    3       /* Swap half-words. */
#define MEMENDIAN_MASK  3
#define MEMENDIAN_SHIFT 0

#define DMABUSY         0x80    /* DMA busy r/o */
#define DMACMDPTR_DMABUSY       0x1     /* corresponding bit in other reg */

/* INTR */
#define VERTINTR        0x01 /* vert retrace */
#define FIFOLOWINTR     0x02 /* free entries rose above low water */
#define RISCINTR        0x04 /* RISC firmware interrupt */
#define HALTINTR        0x08 /* RISC halted */
#define FIFOERRORINTR   0x10 /* FIFO under/over flow */
#define DMAERRORINTR    0x20 /* PCI error during DMA */
#define DMAINTR         0x40 /* DMA done interrupt */
#define XINTR           0x80 /* external device pass thru intr */

/* INTREN */
#define VERTINTREN      0x01 /* vert retrace */
#define FIFOLOWINTREN   0x02 /* free entries rose above low water */
#define RISCINTREN      0x04 /* RISC firmware interrupt */
#define HALTINTREN      0x08 /* RISC halted */
#define FIFOERRORINTREN 0x10 /* FIFO under/over flow */
#define DMAERRORINTREN  0x20 /* PCI error during DMA */
#define DMAINTREN       0x40 /* DMA done interrupt */
#define XINTREN         0x80 /* external device pass thru intr */

/* DEBUG */
#define SOFTRESET       0x01 /* soft reset chip */
#define HOLDRISC        0x02 /* stop RISC when set */
#define STEPRISC        0x04 /* single step RISC */
#define DIRECTSCLK      0x08 /* disable internal divide by 2 for sys clk */
#define SOFTVGARESET    0x10 /* assert VGA reset */
#define SOFTXRESET      0x20 /* assert XReset output to ext devices */

/* MODE_ register */
#define VESA_MODE       0x01 /* enable 0xA0000 in native mode */
#define VGA_MODE        0x02 /* VGA mode if set else native mode */
#define VGA_32          0x04 /* enable VGA 32 bit accesses */
#define DMA_EN          0x08 /* enable DMA accesses */

#define NATIVE_MODE     0    /* not VESA and not VGA */

/* DRAM register */
#define DRAMCTL_ADDR		0xffe00500
#define DRAMCTL_SLOWPRECHARGE	0x140010
#define DRAMCTL_NORMAL		0x140000

/* CRTC registers */
#define CRTCTEST_ADDR   0xffe00400
#define CRTCCTL_ADDR    0xffe00420
#define CRTCHORZ_ADDR   0xffe00440
#define CRTCVERT_ADDR   0xffe00460
#define FRAMEBASEB_ADDR 0xffe00480
#define FRAMEBASEA_ADDR 0xffe004a0
#define CRTCOFFSET_ADDR 0xffe004c0
#define CRTCSTATUS_ADDR 0xffe004e0

#define CRTCTEST_VIDEOLATENCY_MASK      0x1F
#define CRTCTEST_NOTVBLANK      0x10000
#define CRTCTEST_VBLANK         0x40000

#define CRTCCTL_SCRNFMT_MASK    0xF
#define CRTCCTL_VIDEOFIFOSIZE128        0x10
#define CRTCCTL_ENABLEDDC       0x20
#define CRTCCTL_DDCOUTPUT       0x40
#define CRTCCTL_DDCDATA         0x80
#define CRTCCTL_VSYNCHI         0x100
#define CRTCCTL_HSYNCHI         0x200
#define CRTCCTL_VSYNCENABLE     0x400
#define CRTCCTL_HSYNCENABLE     0x800
#define CRTCCTL_VIDEOENABLE     0x1000
#define CRTCCTL_STEREOSCOPIC    0x2000
#define CRTCCTL_FRAMEDISPLAYED  0x4000
#define CRTCCTL_FRAMEBUFFERBGR  0x8000
#define CRTCCTL_EVENFRAME       0x10000
#define CRTCCTL_LINEDOUBLE      0x20000
#define CRTCCTL_FRAMESWITCHED   0x40000

#define CRTCHORZ_ACTIVE_MASK    	0xFF
#define CRTCHORZ_ACTIVE_SHIFT    	0
#define CRTCHORZ_BACKPORCH_MASK 	0x7E00
#define CRTCHORZ_BACKPORCH_SHIFT 	11
#define CRTCHORZ_SYNC_MASK      	0x1F0000L
#define CRTCHORZ_SYNC_SHIFT      	16
#define CRTCHORZ_FRONTPORCH_MASK    0xE00000L
#define CRTCHORZ_FRONTPORCH_SHIFT   20

#define CRTCVERT_ACTIVE_MASK    	0x7FF
#define CRTCVERT_BACKPORCH_MASK 	0x1F800
#define CRTCVERT_SYNC_MASK      	0xE0000
#define CRTCVERT_FRONTPORCH_MASK        0x03F00000

#define CRTCOFFSET_MASK         0xFFFF

#define CRTCSTATUS_HORZCLOCKS_MASK      0xFF
#define CRTCSTATUS_HORZ_MASK    0x600
#define CRTCSTATUS_HORZ_FPORCH  0x200
#define CRTCSTATUS_HORZ_SYNC    0x600
#define CRTCSTATUS_HORZ_BPORCH  0x400
#define CRTCSTATUS_HORZ_ACTIVE  0x000
#define CRTCSTATUS_SCANLINESLEFT_MASK   0x003FF800
#define CRTCSTATUS_VERT_MASK    0xC00000
#define CRTCSTATUS_VERT_FPORCH  0x400000
#define CRTCSTATUS_VERT_SYNC    0xC00000
#define CRTCSTATUS_VERT_BPORCH  0x800000
#define CRTCSTATUS_VERT_ACTIVE  0x000000

/* RAMDAC registers - avail through I/O space */

#define DACRAMWRITEADR  0xb0
#define DACRAMDATA      0xb1
#define DACPIXELMSK     0xb2
#define DACRAMREADADR   0xb3
#define DACOVSWRITEADR  0xb4
#define DACOVSDATA      0xb5
#define DACCOMMAND0     0xb6
#define DACOVSREADADR   0xb7
#define DACCOMMAND1     0xb8
#define DACCOMMAND2     0xb9
#define DACSTATUS       0xba
#define DACCOMMAND3     0xba    /* accessed via unlocking/indexing */
#define DACCURSORDATA   0xbb
#define DACCURSORXLOW   0xbc
#define DACCURSORXHIGH  0xbd
#define DACCURSORYLOW   0xbe
#define DACCURSORYHIGH  0xbf

/* values for DACCOMMAND3 */
#define DACCOMMAND3_INIT 0x00
#define DAC_CLK_DOUBLER 0x8

#define PLLDEV          DEVICE0

/* Some state indices */
#define STATEINDEX_IR   128
#define STATEINDEX_PC   129
#define STATEINDEX_S1   130

/* PCI configuration registers. */
#define CONFIGIOREG        0xE0000014
#define CONFIGENABLE       0xE0000004
#ifdef USEROM
#define CONFIGROMREG       0xE0000030
#endif

/* Cache parameters. */
#define ICACHESIZE       2048 /* I cache size. */
#define ICACHELINESIZE     32 /* I cache line size. */
#define ICACHE_ONOFF_MASK  (((vu32)1<<17)|(1<<3))
#define ICACHE_ON          ((0<<17)|(0<<3))
#define ICACHE_OFF         (((vu32)1<<17)|(1<<3))



#endif /* __COMMONREGS_H__  */
