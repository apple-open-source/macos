/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3virge/regs3v.h,v 1.10 2004/02/13 23:58:43 dawes Exp $ */

/*
 * Copyright (C) 1994-1999 The XFree86 Project, Inc.  All Rights Reserved.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * regs3v.h
 *
 * Port to 4.0 design level
 *
 * S3 ViRGE driver
 *
 * Portions based on code containing the following notices:
 **********************************************************
 *
 * Written by Jake Richter Copyright (c) 1989, 1990 Panacea Inc., Londonderry,
 * NH - All Rights Reserved
 *
 * This code may be freely incorporated in any program without royalty, as long
 * as the copyright notice stays intact.
 *
 * Additions by Kevin E. Martin (martin@cs.unc.edu)
 *
 * KEVIN E. MARTIN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEVIN E. MARTIN BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* Taken from accel/s3_virge code */
/* 23/03/97 S. Marineau: fixed bug with first Doubleword Offset macros 
 * and added macro CommandWaitIdle to wait for the command FIFO to empty 
 */


#ifndef _REGS3V_H
#define _REGS3V_H

#define S3_ViRGE_SERIES(chip)     ((chip&0xfff0)==0x31e0)
#define S3_ViRGE_GX2_SERIES(chip) (chip == S3_ViRGE_GX2 || chip == S3_TRIO_3D_2X)
#define S3_ViRGE_MX_SERIES(chip)  (chip == S3_ViRGE_MX || chip == S3_ViRGE_MXP)
#define S3_ViRGE_MXP_SERIES(chip) (chip == S3_ViRGE_MXP)
#define S3_ViRGE_VX_SERIES(chip)  ((chip&0xfff0)==0x3de0)
#define S3_TRIO_3D_SERIES(chip)		(chip == S3_TRIO_3D)
#define S3_TRIO_3D_2X_SERIES(chip)	(chip == S3_TRIO_3D_2X)

/* Chip tags */
#define PCI_S3_VENDOR_ID	PCI_VENDOR_S3
#define S3_UNKNOWN		 0
#define S3_ViRGE		 PCI_CHIP_VIRGE
#define S3_ViRGE_VX		 PCI_CHIP_VIRGE_VX
#define S3_ViRGE_DXGX	 PCI_CHIP_VIRGE_DXGX
#define S3_ViRGE_GX2	 PCI_CHIP_VIRGE_GX2
#define S3_ViRGE_MX		 PCI_CHIP_VIRGE_MX
#define S3_ViRGE_MXP	 PCI_CHIP_VIRGE_MXP
#define S3_TRIO_3D	PCI_CHIP_Trio3D
#define S3_TRIO_3D_2X	PCI_CHIP_Trio3D_2X

/* Subsystem Control Register */
#define	GPCTRL_NC	0x0000
#define	GPCTRL_ENAB	0x4000
#define	GPCTRL_RESET	0x8000


/* Command Register */
#define	CMD_OP_MSK	(0xf << 27)
#define	CMD_BITBLT	(0x0 << 27)
#define	CMD_RECT       ((0x2 << 27) | 0x0100)
#define	CMD_LINE	(0x3 << 27)
#define	CMD_POLYFILL	(0x5 << 27)
#define	CMD_NOP		(0xf << 27)

#define	BYTSEQ		0
#define	_16BIT		0
#define	PCDATA		0x80
#define	INC_Y		CMD_YP
#define	YMAJAXIS	0
#define	INC_X		CMD_XP
#define	DRAW		0x0020
#define	LINETYPE	0x0008
#define	LASTPIX		0
#define	PLANAR		0 /* MIX_MONO_SRC */
#define	WRTDATA		0

/*
 * Short Stroke Vector Transfer Register (The angular Defs also apply to the
 * Command Register
 */
#define	VECDIR_000	0x0000
#define	VECDIR_045	0x0020
#define	VECDIR_090	0x0040
#define	VECDIR_135	0x0060
#define	VECDIR_180	0x0080
#define	VECDIR_225	0x00a0
#define	VECDIR_270	0x00c0
#define	VECDIR_315	0x00e0
#define	SSVDRAW		0x0010

/* Command AutoExecute */
#define CMD_AUTOEXEC	0x01

/* Command Hardware Clipping Enable */
#define CMD_HWCLIP	0x02

/* Destination Color Format */
#define DST_8BPP	0x00
#define DST_16BPP	0x04
#define DST_24BPP	0x08

/* BLT Mix modes */
#define	MIX_BITBLT	0x0000
#define	MIX_MONO_SRC	0x0040
#define	MIX_CPUDATA	0x0080
#define	MIX_MONO_PATT	0x0100
#define MIX_COLOR_PATT  0x0000
#define	MIX_MONO_TRANSP	0x0200

/* Image Transfer Alignments */
#define CMD_ITA_BYTE	0x0000
#define CMD_ITA_WORD	0x0400
#define CMD_ITA_DWORD	0x0800

/* First Doubleword Offset (Image Transfer) */
#define CMD_FDO_BYTE0	0x00000
#define CMD_FDO_BYTE1	0x01000
#define CMD_FDO_BYTE2	0x02000
#define CMD_FDO_BYTE3	0x03000

/* X Positive, Y Positive (Bit BLT) */
#define CMD_XP		0x2000000
#define CMD_YP		0x4000000

/* 2D or 3D Select */
#define CMD_2D		0x00000000
#define CMD_3D		0x80000000

/* The Mix ROPs (selected ones, not all 256)  */
#if 0

#define	ROP_0				(0x00<<17)
#define	ROP_DSon			(0x11<<17)
#define	ROP_DSna			(0x22<<17)
#define	ROP_Sn				(0x33<<17)
#define	ROP_SDna			(0x44<<17)
#define	ROP_Dn				(0x55<<17)
#define	ROP_DSx				(0x66<<17)
#define	ROP_DSan			(0x77<<17)
#define	ROP_DSa				(0x88<<17)
#define	ROP_DSxn			(0x99<<17)
#define	ROP_D				(0xaa<<17)
#define	ROP_DSno			(0xbb<<17)
#define	ROP_S				(0xcc<<17)
#define	ROP_SDno			(0xdd<<17)
#define	ROP_DSo				(0xee<<17)
#define	ROP_1				(0xff<<17)

/* ROP  ->  (ROP & P) | (D & ~P) */
#define	ROP_0_PaDPnao    /* DPna     */	(0x0a<<17)
#define	ROP_DSon_PaDPnao /* PDSPaox  */	(0x1a<<17)
#define	ROP_DSna_PaDPnao /* DPSana   */	(0x2a<<17)
#define	ROP_Sn_PaDPnao   /* SPDSxox  */	(0x3a<<17)
#define	ROP_SDna_PaDPnao /* DPSDoax  */	(0x4a<<17)
#define	ROP_Dn_PaDPnao   /* DPx      */	(0x5a<<17)
#define	ROP_DSx_PaDPnao  /* DPSax    */	(0x6a<<17)
#define	ROP_DSan_PaDPnao /* DPSDnoax */	(0x7a<<17)
#define	ROP_DSa_PaDPnao  /* DSPnoa   */	(0x8a<<17)
#define	ROP_DSxn_PaDPnao /* DPSnax   */	(0x9a<<17)
#define	ROP_D_PaDPnao    /* D        */	(0xaa<<17)
#define	ROP_DSno_PaDPnao /* DPSnao   */	(0xba<<17)
#define	ROP_S_PaDPnao    /* DPSDxax  */	(0xca<<17)
#define	ROP_SDno_PaDPnao /* DPSDanax */	(0xda<<17)
#define	ROP_DSo_PaDPnao  /* DPSao    */ (0xea<<17)
#define	ROP_1_PaDPnao    /* DPo      */	(0xfa<<17)


/* S -> P */
#define	ROP_DPon			(0x05<<17)
#define	ROP_DPna			(0x0a<<17)
#define	ROP_Pn				(0x0f<<17)
#define	ROP_PDna			(0x50<<17)
#define	ROP_DPx				(0x5a<<17)
#define	ROP_DPan			(0x5f<<17)
#define	ROP_DPa				(0xa0<<17)
#define	ROP_DPxn			(0xa5<<17)
#define	ROP_DPno			(0xaf<<17)
#define	ROP_P				(0xf0<<17)
#define	ROP_PDno			(0xf5<<17)
#define	ROP_DPo				(0xfa<<17)

/* ROP -> (ROP & S) | (~ROP & D) */
#define ROP_DPSDxax			(0xca<<17)
#define ROP_DSPnoa			(0x8a<<17)
#define ROP_DPSao			(0xea<<17)
#define ROP_DPSoa			(0xa8<<17)
#define ROP_DSa				(0x88<<17)
#define ROP_SSPxDSxax			(0xe8<<17)
#define ROP_SDPoa			(0xc8<<17)
#define ROP_DSPnao			(0xae<<17)
#define ROP_SSDxPDxax			(0x8e<<17)
#define ROP_DSo				(0xee<<17)
#define ROP_SDPnao			(0xce<<17)
#define ROP_SPDSxax			(0xac<<17)
#define ROP_SDPnoa			(0x8c<<17)
#define ROP_SDPao			(0xec<<17)

/* ROP_sp -> (ROP_sp & S) | (D & ~S) */
#define	ROP_0_SaDSnao    /* DSna     */	(0x22<<17)
#define	ROP_DPa_SaDSnao  /* DPSnoa   */	(0xa2<<17)
#define	ROP_PDna_SaDSnao /* DSPDoax  */	(0x62<<17)
#define	ROP_P_SaDSnao    /* DSPDxax  */	(0xe2<<17)
#define	ROP_DPna_SaDSnao /* DPSana   */	(0x2a<<17)
#define	ROP_D_SaDSnao    /* D        */	(0xaa<<17)
#define	ROP_DPx_SaDSnao  /* DPSax    */	(0x6a<<17)
#define	ROP_DPo_SaDSnao  /* DPSao    */	(0xea<<17)
#define	ROP_DPon_SaDSnao /* SDPSaox  */	(0x26<<17)
#define	ROP_DPxn_SaDSnao /* DSPnax   */	(0xa6<<17)
#define	ROP_Dn_SaDSnao   /* DSx      */	(0x66<<17)
#define	ROP_PDno_SaDSnao /* SDPSanax */	(0xe6<<17)
#define	ROP_Pn_SaDSnao   /* PSDPxox  */	(0x2e<<17)
#define	ROP_DPno_SaDSnao /* DSPnao   */	(0xae<<17)
#define	ROP_DPan_SaDSnao /* SDPSnoax */	(0x6e<<17)
#define	ROP_1_SaDSnao    /* DSo      */	(0xee<<17)

#endif


#define MAXLOOP 0x0fffff /* timeout value for engine waits, 0.5 secs */

/* Wait until "v" queue entries are free */
#define	WaitQueue(v) \
  if (ps3v->NoPCIRetry) { \
    do { int loop=0; mem_barrier(); \
         while ((((IN_SUBSYS_STAT()) & 0x1f00) < (((v)+2) << 8)) && (loop++<MAXLOOP)); \
         if (loop >= MAXLOOP) S3VGEReset(pScrn,1,__LINE__,__FILE__); \
    } while (0); }

/* Wait until GP is idle and queue is empty */
#define	WaitIdleEmpty()  \
  do { int loop=0; mem_barrier(); \
    if(S3_TRIO_3D_SERIES(ps3v->Chipset)) \
       while (((IN_SUBSYS_STAT() & 0x3f802000 & 0x20002000) != 0x20002000) && \
             (loop++<MAXLOOP)); \
    else \
       while (((IN_SUBSYS_STAT() & 0x3f00) != 0x3000) && (loop++<MAXLOOP)); \
       if (loop >= MAXLOOP) S3VGEReset(pScrn,1,__LINE__,__FILE__); \
  } while (0)

/* Wait until GP is idle */
#define WaitIdle() \
  do { int loop=0; mem_barrier(); \
       while ((!(IN_SUBSYS_STAT() & 0x2000)) && (loop++<MAXLOOP)); \
         if (loop >= MAXLOOP) S3VGEReset(pScrn,1,__LINE__,__FILE__); \
  } while (0)


/* Wait until Command FIFO is empty */
#define WaitCommandEmpty()       do { int loop=0; mem_barrier(); 			\
	if (S3_ViRGE_GX2_SERIES(S3_ViRGE_GX2) || S3_ViRGE_MX_SERIES(ps3v->Chipset)) 		\
	     while ((!(((((mmtr)s3vMmioMem)->subsys_regs.regs.adv_func_cntl)) & 0x400)) && (loop++<MAXLOOP));	\
	else if (S3_TRIO_3D_SERIES(ps3v->Chipset)) \
	     while (((IN_SUBSYS_STAT() & 0x5f00) != 0x5f00) && (loop++<MAXLOOP)); \
	  else 										\
	     while ((!(((((mmtr)s3vMmioMem)->subsys_regs.regs.adv_func_cntl)) & 0x200)) && (loop++<MAXLOOP));	\
          if (loop >= MAXLOOP) S3VGEReset(pScrn,1,__LINE__,__FILE__); \
	} while (0)

/* Wait until a DMA transfer is done */ 
#define WaitDMAEmpty() \
  do { int loop=0; mem_barrier(); \
       while  (((((mmtr)s3vMmioMem)->dma_regs.regs.cmd.write_pointer) != (((mmtr)s3vMmioMem)->dma_regs.regs.cmd.read_pointer)) && (loop++<MAXLOOP)); \
       if (loop >= MAXLOOP) S3VGEReset(pScrn,1,__LINE__,__FILE__); \
  } while(0)

      



#define RGB8_PSEUDO      (-1)
#define RGB16_565         0
#define RGB16_555         1
#define RGB32_888         2

#endif /* _REGS3V_H */

