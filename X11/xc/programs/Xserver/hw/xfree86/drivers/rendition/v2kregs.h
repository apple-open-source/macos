/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/rendition/v2kregs.h,v 1.4 1999/11/19 13:54:46 hohndel Exp $ */
#ifndef __V2KREGS_H__
#define __V2KREGS_H__

/* New registers and values found from V2K and on */
#define STATUS          0x4A /* specifies which blocks of the V2000 are busy */
#define XBUSCTL         0x4B /* XBus control register */

#define SCLKPLL         0x68 /* system clock PLL control register */
#define SCRATCH         0x70 /* 16-bit BIOS scratch space */

#define MEMDIAG         0xa4 /* Memory diagnostic register #1 */
#define CURSORBASE      0xac /* cursor base address bits [23:10] aligne to 1024 byte boundary */

#define PCLKPLL         0xc0 /* external device 0 */
#define VINEVENBASE     0xd0 /* video input even field base address */
#define VINODDBASE      0xd4 /* video input odd field base address */
#define WRITEINTR0ADDR  0xd8 /* Memory write interrupt address0 */
#define WRITEINTR1ADDR  0xdc /* Memory write interrupt address1 */
#define DEVICE0_V2x000  0xf0 /* external device 1 (PLL) */

/* 
 * PCLKPLL/SCLKPLL register bit defn
 */
#define PCLKPLLPMASK	0xffffe1ff
#define SCLKPLLPMASK	0xffffe1ff
#define MCLKPLLPMASK	0xfffe1fff
#define PLLPCLKP	9
#define PLLSCLKP	9
#define PLLMCLKP	13
#define PLLPCLKN	13
#define PLLSCLKN	17
#define PLLPCLKDOUBLE	26	/* bit 26 in PClkPLL register */

#define DIRECTPCLKMASK	0x00400000
#define DIRECTMCLKMASK	0x00800000

#define VGASTDCLOCK     0x100000
#define EXTRADIV2       0x200000 

#define PLLINCLKFREQ	14318 /* PLL input clk freq in KHz */

/*
 * memory controller 
 */
#define MCLK_BYPASSEDGEFREQ 90000 /* in KHz */

/*
 * Microcode commands
 */
#define	CMD_SETPALETTE	0x21

/*
 * MMIO registers
 */

#define MMIO_FIFOINFREE	    0x20040
#define MMIO_COMM           0x20042
#define MMIO_FIFOOUTVALID   0x20041
#define MMIO_INTR           0x20044 /* which interrupts occurred */
#define MMIO_DMACMDPTR      0x20050
#define MMIO_CRTCHORZ       0x20088 /* CRTC horizontal timing */
#define MMIO_CRTCVERT       0x2008c /* CRTC vertical timing */
#define MMIO_CRTCSTATUS	    0x2009c 
#define MMIO_DACRAMWRITEADR 0x200b0 /* Palette Write Index */
#define MMIO_DACRAMDATA     0x200b1 /* Palette Data */
#define MMIO_VINEVENBASE    0x200d0 /* video input even field base address */
#define MMIO_VINODDBASE     0x200d4 /* video input odd field base address */
#define MMIO_WRITEINTR0ADDR 0x200d8 /* Memory write interrupt address0 */
#define MMIO_WRITEINTR1ADDR 0x200dc /* Memory write interrupt address1 */

#define HOST_INTERRUPT_MUTEX 1

/* memory mapped IO register structure */
typedef struct _v_mem_io {
    vu32	fifo_swap_no[0x2000];	/* 0x0 */
    vu32	fifo_swap_end[0x2000];	/* 0x4000 */
    vu32	fifo_swap_inhw[0x2000];	/* 0x8000 */
    vu32	fifo_swap_hw[0x2000];	/* 0xC000 */
    vu32	reserved0[0x10];	/* 0x10000 */
    vu8 	fifoinFree;		/* 0x10040 */
    vu8 	fifooutvalid;		/* 0x10041 */
    vu8 	comm;			/* 0x10042 */
    vu8 	memendian;		/* 0x10043 */
    vu8 	intr;			/* 0x10044 */
    vu8 	reserved1;		/* 0x10045 */
    vu8 	intren;			/* 0x10046 */
    vu8 	reserved2;		/* 0x10047 */
    vu8 	debugreg;		/* 0x10048 */
    vu8 	lowwatermark;		/* 0x10049 */
    vu8 	status;			/* 0x1004a */
    vu8 	xbusctl;		/* 0x1004b */
    vu8 	pcitest;		/* 0x1004c */
    vu8 	reserved3[3];		/* 0x1004d */
    vu32	dmacmdptr;		/* 0x10050 */
    vu32	dma_address;		/* 0x10054 */
    vu32	dma_count;		/* 0x10058 */
    vu8 	vga_extend;		/* 0x1005c */
    vu8 	reserved4;		/* 0x1005d */
    vu8 	membase;		/* 0x1005e */
    vu8 	reserved5;		/* 0x1005f */
    vu32	stateindex;		/* 0x10060 */
    vu32	statedata;		/* 0x10064 */
    vu8 	sclkpll;		/* 0x10068 */
    vu8 	reserved6[7];		/* 0x10069 */
    vu16	scratch;		/* 0x10070 */
    vu8 	mode;			/* 0x10072 */
    vu8 	scratch1;		/* 0x10073 */
    vu8 	bankselect;		/* 0x10074 */
    vu8 	reserved7[11];		/* 0x10075 */
    vu32	crtctest;		/* 0x10080 */
    vu32	crtcctl;		/* 0x10084 */
    vu32	crtchorz;		/* 0x10088 */
    vu32	crtcvert;		/* 0x1008c */
    vu32	framebaseb;		/* 0x10090 */
    vu32	framebasea;		/* 0x10094 */
    vu32	crtcoffset;		/* 0x10098 */
    vu32	crtcstatus;		/* 0x1009c */
    vu32	memctl;			/* 0x100a0 */
    vu32	memdiag;		/* 0x100a4 */
    vu32	memcmd;			/* 0x100a8 */
    vu32	cursorbase;		/* 0x100ac */
    vu8 	dacramwriteadr;		/* 0x100b0 */
    vu8 	dacramdata;		/* 0x100b1 */
    vu8 	dacpixelmsk;		/* 0x100b2 */
    vu8 	dacramreadadr;		/* 0x100b3 */
    vu8 	dacovswriteadr;		/* 0x100b4 */
    vu8 	dacovsdata;		/* 0x100b5 */
    vu8 	daccommand0;		/* 0x100b6 */
    vu8 	dacovsreadadr;		/* 0x100b7 */
    vu8 	daccommand1;		/* 0x100b8 */
    vu8 	daccommand2;		/* 0x100b9 */
    vu8 	daccommand3;		/* 0x100ba */
    vu8 	daccursordata;		/* 0x100bb */
    vu8 	daccursorxlow;		/* 0x100bc */
    vu8 	daccursorxhigh;		/* 0x100bd */
    vu8 	daccursorylow;		/* 0x100be */
    vu8 	daccursoryhigh;		/* 0x100bf */
    vu8 	pclkpll;		/* 0x100c0 */
} v_mem_io;

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
#define VIDEOINEVENINTR	0x100 /* Video input even field interrupt */
#define VIDEOINODDINTR	0x100 /* Video input even field interrupt */

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
#define DRAMCTL_ADDR    0xffe00500

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
#define CRTCCTL_VIDEOFIFOSIZE256        0x800000

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
#define CRTCVERT_FRONTPORCH_MASK    0x03F00000

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

#define BT_CO_COLORWR_ADDR	DACOVSWRITEADR
#define BT_CO_COLORDATA		DACOVSDATA
#define BT_PTR_ROWOFFSET	32
#define BT_PTR_COLUMNOFFSET	32

/* PCLKPLL register */
#define PLLDEV          DEVICE0
#define VOUTEN			0x00080000L	/* bit 19 */
#define VGASCLKOVER2	0x00100000L	/* bit 20 */
#define PCLKSTARTEN		0x00800000L	/* bit 23 */

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
#ifndef ICACHE_ONOFF_MASK
#define ICACHE_ONOFF_MASK  (((v_u32)1<<17)|(1<<3))
#define ICACHE_ON          ((0<<17)|(0<<3))
#define ICACHE_OFF         (((v_u32)1<<17)|(1<<3))
#endif

/* Video registers */
#define BT829_DEV                 DEVICE0
#define VIDEO_DECODER_DEV_ENABLE  0x4
#define VIDEO_DECODER_DEV_DISABLE 0x0

#define VINBASE_MASK     0x1FFFFFL
#define VINMAXVERT_SHIFT 24
#define VINSTRIDE_SHIFT  27
#define VINQSIZE_SHIFT   30

#define VINORDER_SHIFT   24
#define ACTIVE_LOW        0
#define ACTIVE_HI         1L
#define VINHSYNCHI_SHIFT 26
#define VINVSYNCHI_SHIFT 27
#define VINACTIVE_SHIFT  28
#define VINNOODD_SHIFT   29
#define VINENABLE_SHIFT  30

#endif /* __V2KREGS_H__ */
