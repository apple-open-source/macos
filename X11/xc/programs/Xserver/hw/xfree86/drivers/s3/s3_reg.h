/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3/s3_reg.h,v 1.1 2001/07/02 10:46:04 alanh Exp $ */

#ifndef _S3_REG_H
#define _S3_REG_H

#include "compiler.h"

extern short s3alu[16];

#define S3_NEWMMIO_REGBASE      0x1000000  /* 16MB */   
#define S3_NEWMMIO_REGSIZE        0x10000  /* 64KB */

#define ADVFUNC_CNTL	0x4ae8
#define SUBSYS_STAT	0x42e8
#define SUBSYS_CNTL	0x42e8
#define CUR_Y		0x82e8
#define CUR_X		0x86e8
#define CUR_Y2		0x82ea
#define CUR_X2		0x86ea
#define DESTY_AXSTP	0x8ae8
#define DESTX_DIASTP	0x8ee8
#define DESTY_AXSTP2	0x8aea
#define DESTX_DIASTP2	0x8eea
#define ERR_TERM        0x92e8
#define ERR_TERM2       0x92ea   
#define MAJ_AXIS_PCNT	0x96e8
#define MAJ_AXIS_PCNT2	0x96ea
#define GP_STAT		0x9ae8
#define CMD		0x9ae8
#define CMD2		0x9aea
#define BKGD_COLOR	0xa2e8
#define FRGD_COLOR	0xa6e8
#define WRT_MASK	0xaae8
#define RD_MASK		0xaee8
#define COLOR_CMP	0xb2e8
#define BKGD_MIX	0xb6e8
#define FRGD_MIX	0xbae8
#define MULTIFUNC_CNTL	0xbee8
#define PIX_TRANS	0xe2e8
#define PIX_TRANS_EXT	0xe2ea

/* Graphics Processor Status Register */
#define GPBUSY		0x0200

/* Command Register */
#define CMD_NOP         0x0000
#define CMD_LINE        0x2000
#define CMD_RECT        0x4000
#define CMD_RECTV1      0x6000 
#define CMD_RECTV2      0x8000  
#define CMD_LINEAF      0xa000
#define CMD_BITBLT      0xc000
#define CMD_PFILL       0xe000
#define CMD_OP_MSK      0xf000
#define BYTSEQ          0x1000
#define _16BIT          0x0200
#define _32BIT          0x0400
#define PCDATA          0x0100
#define INC_Y           0x0080
#define YMAJAXIS        0x0040
#define INC_X           0x0020 
#define DRAW            0x0010
#define LINETYPE        0x0008
#define LASTPIX         0x0004
#define PLANAR          0x0002
#define WRTDATA         0x0001


/* Background Mix Register */
#define BSS_BKGDCOL	0x0000
#define BSS_FRGDCOL	0x0020
#define BSS_PCDATA	0x0040
#define BSS_BITBLT	0x0060

/* Foreground Mix Register */
#define FSS_BKGDCOL	0x0000
#define FSS_FRGDCOL	0x0020
#define FSS_PCDATA	0x0040
#define FSS_BITBLT	0x0060

#define PIX_CNTL	0xa000
#define MIN_AXIS_PCNT	0x0000

/* Pixel Control Register */
#define MIXSEL_EXPPC	0x0080

#define SCISSORS_T	0x1000
#define SCISSORS_L	0x2000
#define SCISSORS_B	0x3000
#define SCISSORS_R	0x4000
#define MULT_MISC2	0xd000
#define MULT_MISC	0xe000


#define	MIX_MASK			0x001f

#define MIX_NOT_DST			0x0000
#define MIX_0				0x0001
#define MIX_1				0x0002
#define MIX_DST				0x0003
#define MIX_NOT_SRC			0x0004
#define MIX_XOR				0x0005
#define MIX_XNOR			0x0006
#define MIX_SRC				0x0007
#define MIX_NAND			0x0008
#define MIX_NOT_SRC_OR_DST		0x0009
#define MIX_SRC_OR_NOT_DST		0x000a
#define MIX_OR				0x000b
#define MIX_AND				0x000c
#define MIX_SRC_AND_NOT_DST		0x000d
#define MIX_NOT_SRC_AND_DST		0x000e
#define MIX_NOR				0x000f

#define MIX_MIN				0x0010
#define MIX_DST_MINUS_SRC		0x0011
#define MIX_SRC_MINUS_DST		0x0012
#define MIX_PLUS			0x0013
#define MIX_MAX				0x0014
#define MIX_HALF__DST_MINUS_SRC		0x0015
#define MIX_HALF__SRC_MINUS_DST		0x0016
#define MIX_AVERAGE			0x0017
#define MIX_DST_MINUS_SRC_SAT		0x0018
#define MIX_SRC_MINUS_DST_SAT		0x001a
#define MIX_HALF__DST_MINUS_SRC_SAT	0x001c
#define MIX_HALF__SRC_MINUS_DST_SAT	0x001e
#define MIX_AVERAGE_SAT			0x001f

/*
 * Short Stroke Vector Transfer Register (The angular Defs also apply to
the
 * Command Register
 */
#define VECDIR_000      0x0000
#define VECDIR_045      0x0020
#define VECDIR_090      0x0040
#define VECDIR_135      0x0060
#define VECDIR_180      0x0080
#define VECDIR_225      0x00a0  
#define VECDIR_270      0x00c0
#define VECDIR_315      0x00e0
#define SSVDRAW         0x0010


#define S3_OUTW(p,n)		outw(p, n)
#define S3_OUTL(p,n)		outl(p, n)
#define S3_OUTW32(p,n)		if (pS3->s3Bpp > 2) {		\
					outw(p, n);		\
					outw(p, (n) >> 16);	\
				} else outw(p, n)


#define WaitIdle()	do {			\
		mem_barrier();			\
		while(inw(GP_STAT) & GPBUSY);	\
	} while(0)


#ifdef S3_NEWMMIO
#include "newmmio.h"

/*
 * streams regs
 */
#define SET_BLEND_CNTL(val)     ((mmtr)s3MmioMem)->streams_regs.regs.blend_cntl = (val)
#define SET_PSTREAM_CNTL(val)   ((mmtr)s3MmioMem)->streams_regs.regs.prim_stream_cntl = (val)
#define SET_PSTREAM_FBADDR(val) ((mmtr)s3MmioMem)->streams_regs.regs.prim_fbaddr0 = (val)
#define SET_PSTREAM_STRIDE(val) ((mmtr)s3MmioMem)->streams_regs.regs.prim_stream_stride = (val)
#define SET_PSTREAM_START(val)  ((mmtr)s3MmioMem)->streams_regs.regs.prim_start_coord = (val)
#define SET_PSTREAM_WIND(val)   ((mmtr)s3MmioMem)->streams_regs.regs.prim_window_size = (val)
#define SET_SSTREAM_CNTL(val)   ((mmtr)s3MmioMem)->streams_regs.regs.second_stream_cntl = (val)
#define SET_SSTRETCH(val)       ((mmtr)s3MmioMem)->streams_regs.regs.second_stream_stretch = (val)
#define SET_SSTREAM_FBADDR(val) ((mmtr)s3MmioMem)->streams_regs.regs.second_fbaddr0 = (val)
#define SET_SSTREAM_STRIDE(val) ((mmtr)s3MmioMem)->streams_regs.regs.second_stream_stride = (val)
#define SET_SSTREAM_START(val)  ((mmtr)s3MmioMem)->streams_regs.regs.second_start_coord = (val)
#define SET_SSTREAM_WIND(val)   ((mmtr)s3MmioMem)->streams_regs.regs.second_window_size = (val)
#define SET_K1_VSCALE(val)      ((mmtr)s3MmioMem)->streams_regs.regs.k1 = (val)
#define SET_K2_VSCALE(val)      ((mmtr)s3MmioMem)->streams_regs.regs.k2 = (val)
#define SET_DDA_VERT(val)       ((mmtr)s3MmioMem)->streams_regs.regs.dda_vert = (val)
#define SET_CHROMA_KEY(val)     ((mmtr)s3MmioMem)->streams_regs.regs.col_chroma_key_cntl = (val)
#define SET_DOUBLE_BUFFER(val)  ((mmtr)s3MmioMem)->streams_regs.regs.double_buffer = (val)
#define SET_OPAQUE_OVERLAY(val) ((mmtr)s3MmioMem)->streams_regs.regs.opaq_overlay_cntl = (val)

#else

#define CMD_REG_WIDTH	0x0000

#define WaitQueue(n)	do {				\
		mem_barrier();			\
		while(inb(GP_STAT) & (0x0100 >> (n)));	\
	} while (0)

#define WaitQueue16_32(n16,n32)				\
	if (pS3->s3Bpp <= 2) {				\
		WaitQueue(n16);				\
	} else {					\
		WaitQueue(n32);				\
	}

#define VerticalRetraceWait()						 \
	{								 \
		outb(vgaCRIndex, 0x17);					 \
		if (inb(vgaCRReg) & 0x80) {				 \
			while ((inb(vgaCRIndex-4+0x0a) & 0x08) == 0x00); \
			while ((inb(vgaCRIndex-4+0x0a) & 0x08) == 0x08); \
		}							 \
	}

/* accel commands */

#define SET_PIX_CNTL(val)	S3_OUTW(MULTIFUNC_CNTL, PIX_CNTL | (val))

#define SET_FRGD_COLOR(col)	S3_OUTW32(FRGD_COLOR, col)
#define SET_BKGD_COLOR(col)	S3_OUTW32(BKGD_COLOR, col)

#define SET_FRGD_MIX(fmix)	S3_OUTW(FRGD_MIX, (fmix))
#define SET_WRT_MASK(mask)	S3_OUTW32(WRT_MASK, mask)

#define SET_CUR_X(cur_x)	S3_OUTW(CUR_X, cur_x)
#define SET_CUR_Y(cur_y)	S3_OUTW(CUR_Y, cur_y)
#define SET_CUR_X2(cur_x)	S3_OUTW(CUR_X2, cur_x)
#define SET_CUR_Y2(cur_y)	S3_OUTW(CUR_Y2, cur_y)

#define SET_CURPT(cur_x, cur_y)	{					\
	SET_CUR_X(cur_x);						\
	SET_CUR_Y(cur_y);						\
	}	

#define SET_DESTSTP(x,y) {						\
	S3_OUTW(DESTX_DIASTP, x);					\
	S3_OUTW(DESTY_AXSTP, y);					\
	}

#define SET_AXIS_PCNT(maj, min)	{					\
	S3_OUTW(MAJ_AXIS_PCNT, maj);					\
	S3_OUTW(MULTIFUNC_CNTL, MIN_AXIS_PCNT | (min));			\
	}

#define SET_CMD(cmd)		S3_OUTW(CMD, cmd)

#define SET_SCISSORS(x1,y1,x2,y2) {					\
	S3_OUTW(MULTIFUNC_CNTL, SCISSORS_T | (y1));			\
	S3_OUTW(MULTIFUNC_CNTL, SCISSORS_L | (x1));			\
	S3_OUTW(MULTIFUNC_CNTL, SCISSORS_R | (x2));			\
	S3_OUTW(MULTIFUNC_CNTL, SCISSORS_B | (y2));			\
	}

#define SET_MULT_MISC(val)	S3_OUTW(MULTIFUNC_CNTL, MULT_MISC | (val))

#define SET_COLOR_CMP(color)	S3_OUTW32(COLOR_CMP, color)

#define SET_PIX_TRANS_W(val)	S3_OUTW(PIX_TRANS, val)

#define SET_PIX_TRANS_L(val)	outl(PIX_TRANS, val)

#define SET_ERR_TERM(err)	S3_OUTW(ERR_TERM, err)
#define SET_ERR_TERM2(err)	S3_OUTW(ERR_TERM2, err)

#define SET_MAJ_AXIS_PCNT(maj)	S3_OUTW(MAJ_AXIS_PCNT, maj)
#endif


#endif /* _S3_REG_H */
