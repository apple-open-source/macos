/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i128/i128reg.h,v 1.3 2002/01/25 21:56:02 tsi Exp $ */
/*
 * Copyright 1994 by Robin Cutshaw <robin@XFree86.Org>
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
 */
/* $XConsortium: i128reg.h /main/4 1996/05/12 20:56:19 kaleb $ */

#ifndef I128REG_H
#define I128REG_H

#include "xf86Pci.h"

struct i128pci {
    CARD32 devicevendor;
    CARD32 statuscommand;
    CARD32 classrev;
    CARD32 bhlc;
    CARD32 base0;
    CARD32 base1;
    CARD32 base2;
    CARD32 base3;
    CARD32 base4;
    CARD32 base5;
    CARD32 rsvd0;
    CARD32 rsvd1;
    CARD32 baserom;
    CARD32 rsvd2;
    CARD32 rsvd3;
    CARD32 lgii;
};

struct i128io {
    CARD32 rbase_g;
    CARD32 rbase_w;
    CARD32 rbase_a;
    CARD32 rbase_b;
    CARD32 rbase_i;
    CARD32 rbase_e;
    CARD32 id;
    CARD32 config1;
    CARD32 config2;
    CARD32 sgram;
    CARD32 soft_sw;
    CARD32 vga_ctl;
};

struct i128mem {
    unsigned char *mw0_ad;
    unsigned char *mw1_ad;
    unsigned char *xyw_ada;
    unsigned char *xyw_adb;
    CARD32 *rbase_g;
    CARD32 *rbase_w;
    CARD32 *rbase_a;
    CARD32 *rbase_b;
    CARD32 *rbase_i;
};

/* save the registers needed for restoration in this structure */
typedef struct {
	IOADDRESS iobase;		/* saved only for iobase indexing    */
	CARD32 config1;			/* iobase+0x1C register              */
	CARD32 config2;			/* iobase+0x20 register              */
	CARD32 sgram;			/* iobase+0x24 register              */
	CARD32 vga_ctl;			/* iobase+0x30 register              */
	CARD32 i128_base_g[0x60/4];	/* base g registers                  */
	CARD32 i128_base_w[0x28/4];	/* base w registers                  */
	CARD32 intm;			/* base a+0x04 register              */
	unsigned char Ti302X[0x40];	/* Ti302[05] registers               */
	unsigned char Ti3025[9];	/* Ti3025 N,M,P for PCLK, MCLK, LCLK */
	unsigned char IBMRGB[0x101];	/* IBMRGB registers                  */
} I128RegRec, *I128RegPtr;


/* display list processor instruction formats */
typedef union {
	struct {
		CARD8 aad;
		CARD8 bad;
		CARD8 cad;
		CARD8 control;
		CARD32 rad;
		CARD32 rbd;
		CARD32 rcd;
	} f0;
	struct {
		CARD32 xy0;
		CARD32 xy2;
		CARD32 xy3;
		CARD32 xy1;
	} f1;
	CARD32 f4[4];
} I128dlpu;

#define UNKNOWN_DAC        -1
#define TI3025_DAC          0
#define IBM524_DAC          1
#define IBM526_DAC          2
#define IBM528_DAC          3
#define SILVER_HAMMER_DAC   4

#define I128_MEMORY_UNKNOWN	0x01
#define I128_MEMORY_DRAM	0x02
#define I128_MEMORY_WRAM	0x04
#define I128_MEMORY_SGRAM	0x08

/* RBASE_I register offsets */

#define GINTP 0x0000
#define GINTM 0x0004
#define SGRAM 0x00A4

/* RBASE_G register offsets  (divided by four for double word indexing */

#define WR_ADR   0x0000/4
#define PAL_DAT  0x0004/4
#define PEL_MASK 0x0008/4
#define RD_ADR   0x000C/4
#define INDEX_TI 0x0018/4   /* TI  ramdac */
#define DATA_TI  0x001C/4   /* TI  ramdac */
#define IDXL_I   0x0010/4   /* IBM ramdac */
#define IDXH_I   0x0014/4   /* IBM ramdac */
#define DATA_I   0x0018/4   /* IBM ramdac */
#define IDXCTL_I 0x001C/4   /* IBM ramdac */
#define INT_VCNT 0x0020/4
#define INT_HCNT 0x0024/4
#define DB_ADR   0x0028/4
#define DB_PTCH  0x002C/4
#define CRT_HAC  0x0030/4
#define CRT_HBL  0x0034/4
#define CRT_HFP  0x0038/4
#define CRT_HS   0x003C/4
#define CRT_VAC  0x0040/4
#define CRT_VBL  0x0044/4
#define CRT_VFP  0x0048/4
#define CRT_VS   0x004C/4
#define CRT_LCNT 0x0050/4
#define CRT_ZOOM 0x0054/4
#define CRT_1CON 0x0058/4
#define CRT_2CON 0x005C/4


/* RBASE_W register offsets  (divided by four for double word indexing */

#define MW0_CTRL 0x0000/4
#define MW0_AD   0x0004/4
#define MW0_SZ   0x0008/4   /* 2MB = 0x9, 4MB = 0xA, 8MB = 0xB */
#define MW0_PGE  0x000C/4
#define MW0_ORG  0x0010/4
#define MW0_MSRC 0x0018/4
#define MW0_WKEY 0x001C/4
#define MW0_KDAT 0x0020/4
#define MW0_MASK 0x0024/4


/* RBASE_[AB] register offsets  (divided by four for double word indexing */

#define INTP     0x0000/4
#define  INTP_DD_INT 0x01	/* drawing op completed  */
#define  INTP_CL_INT 0x02
#define INTM     0x0004/4
#define  INTM_DD_MSK 0x01
#define  INTM_CL_MSK 0x02
#define FLOW     0x0008/4
#define  FLOW_DEB    0x01	/* drawing engine busy   */
#define  FLOW_MCB    0x02	/* mem controller busy   */
#define  FLOW_CLP    0x04
#define  FLOW_PRV    0x08	/* prev cmd still running or cache ready */
#define BUSY     0x000C/4
#define  BUSY_BUSY   0x01	/* command pipeline busy */
#define XYW_AD   0x0010/4
#define Z_CTRL   0x0018/4
#define BUF_CTRL 0x0020/4
#define  BC_AMV      0x02
#define  BC_MP       0x04
#define  BC_AMD      0x08
#define  BC_SEN_MSK  0x0300
#define  BC_SEN_DB   0x0000
#define  BC_SEN_VB   0x0100
#define  BC_SEN_MB   0x0200
#define  BC_SEN_CB   0x0300
#define  BC_DEN_MSK  0x0C00
#define  BC_DEN_DB   0x0000
#define  BC_DEN_VB   0x0400
#define  BC_DEN_MB   0x0800
#define  BC_DEN_CB   0x0C00
#define  BC_DSE      0x1000
#define  BC_VSE      0x2000
#define  BC_MSE      0x4000
#define  BC_PS_MSK   0x001F0000
#define  BC_MDM_MSK  0x00600000
#define  BC_MDM_KEY  0x00200000
#define  BC_MDM_PLN  0x00400000
#define  BC_BLK_ENA  0x00800000
#define  BC_PSIZ_MSK 0x03000000
#define  BC_PSIZ_8B  0x00000000
#define  BC_PSIZ_16B 0x01000000
#define  BC_PSIZ_32B 0x02000000
#define  BC_PSIZ_NOB 0x03000000
#define  BC_CO       0x40000000
#define  BC_CR       0x80000000
#define DE_PGE   0x0024/4
#define  DP_DVP_MSK  0x0000001F
#define  DP_MP_MSK   0x000F0000
#define DE_SORG   0x0028/4
#define DE_DORG   0x002C/4
#define DE_MSRC   0x0030/4
#define DE_WKEY   0x0038/4
#define DE_KYDAT  0x003C/4
#define DE_ZPTCH  0x003C/4
#define DE_SPTCH  0x0040/4
#define DE_DPTCH  0x0044/4
#define CMD       0x0048/4
#define  CMD_OPC_MSK 0x000000FF
#define  CMD_ROP_MSK 0x0000FF00
#define  CMD_STL_MSK 0x001F0000
#define  CMD_CLP_MSK 0x00E00000
#define  CMD_PAT_MSK 0x0F000000
#define  CMD_HDF_MSK 0x70000000
#define CMD_OPC   0x0050/4
#define  CO_NOOP     0x00
#define  CO_BITBLT   0x01
#define  CO_LINE     0x02
#define  CO_ELINE    0x03
#define  CO_TRIAN    0x04
#define  CO_RXFER    0x06
#define  CO_WXFER    0x07
#define CMD_ROP   0x0054/4
#define  CR_CLEAR    0x00
#define  CR_NOR      0x01
#define  CR_AND_INV  0x02
#define  CR_COPY_INV 0x03
#define  CR_AND_REV  0x04
#define  CR_INVERT   0x05
#define  CR_XOR      0x06
#define  CR_NAND     0x07
#define  CR_AND      0x08
#define  CR_EQUIV    0x09
#define  CR_NOOP     0x0A
#define  CR_OR_INV   0x0B
#define  CR_COPY     0x0C
#define  CR_OR_REV   0x0D
#define  CR_OR       0x0E
#define  CR_SET      0x0F
#define CMD_STYLE 0x0058/4
#define  CS_SOLID    0x01
#define  CS_TRNSP    0x02
#define  CS_STP_NO   0x00
#define  CS_STP_PL   0x04
#define  CS_STP_PA32 0x08
#define  CS_STP_PA8  0x0C
#define  CS_EDI      0x10
#define CMD_PATRN 0x005C/4
#define  CP_APAT_NO  0x00
#define  CP_APAT_8X  0x01
#define  CP_APAT_32X 0x02
#define  CP_NLST     0x04
#define  CP_PRST     0x08
#define CMD_CLP   0x0060/4
#define  CC_NOCLP    0x00
#define  CC_CLPRECI  0x02
#define  CC_CLPRECO  0x03
#define  CC_CLPSTOP  0x04
#define CMD_HDF   0x0064/4
#define  CH_BIT_SWP  0x01
#define  CH_BYT_SWP  0x02
#define  CH_WRD_SWP  0x04
#define FORE      0x0068/4
#define BACK      0x006C/4
#define MASK      0x0070/4
#define RMSK      0x0074/4
#define LPAT      0x0078/4
#define PCTRL     0x007C/4
#define  PC_PLEN_MSK  0x0000001F
#define  PC_PSCL_MSK  0x000000E0
#define  PC_SPTR_MSK  0x00001F00
#define  PC_SSCL_MSK  0x0000E000
#define  PC_STATE_MSK 0xFFFF0000
#define CLPTL     0x0080/4
#define  CLPTLY_MSK   0x0000FFFF
#define  CLPTLX_MSK   0xFFFF0000
#define CLPBR     0x0084/4
#define  CLPBRY_MSK   0x0000FFFF
#define  CLPBRX_MSK   0xFFFF0000
#define XY0_SRC   0x0088/4
#define XY1_DST   0x008C/4      /* trigger */
#define XY2_WH    0x0090/4
#define XY3_DIR   0x0094/4
#define  DIR_LR_TB    0x00000000
#define  DIR_LR_BT    0x00000001
#define  DIR_RL_TB    0x00000002
#define  DIR_RL_BT    0x00000003
#define XY4_ZM    0x0098/4
#define  ZOOM_NONE    0x00000000
#define  XY_Y_DATA    0x0000FFFF
#define  XY_X_DATA    0xFFFF0000
#define  XY_I_DATA1   0x0000FFFF
#define  XY_I_DATA2   0xFFFF0000
#define DL_ADR    0x00F8/4
#define DL_CNTRL  0x00FC/4
#define ACNTRL    0x016C/4

#define I128_WAIT_READY 1
#define I128_WAIT_DONE  2

typedef struct {
	unsigned char r, b, g;
} LUTENTRY;

#define RGB8_PSEUDO      (-1)
#define RGB16_565         0
#define RGB16_555         1
#define RGB32_888         2

#define MB	mem_barrier()


/* TI ramdac indirect indexed registers */

#define TI_CURS_X_LOW		0x00
#define TI_CURS_X_HIGH		0x01    /* only lower 4 bits are used */
#define TI_CURS_Y_LOW		0x02
#define TI_CURS_Y_HIGH		0x03    /* only lower 4 bits are used */
#define TI_SPRITE_ORIGIN_X	0x04
#define TI_SPRITE_ORIGIN_Y	0x05
#define TI_CURS_CONTROL		0x06
#define   TI_PLANAR_ACCESS	0x80    /* 3025 only - 80 == BT485 mode */
#define   TI_CURS_SPRITE_ENABLE 0x40
#define   TI_CURS_X_WINDOW_MODE 0x10
#define   TI_CURS_CTRL_MASK     (TI_CURS_SPRITE_ENABLE | TI_CURS_X_WINDOW_MODE)
#define TI_CURS_RAM_ADDR_LOW	0x08
#define TI_CURS_RAM_ADDR_HIGH	0x09
#define TI_CURS_RAM_DATA	0x0A
#define TI_TRUE_COLOR_CONTROL	0x0E    /* 3025 only */
#define   TI_TC_BTMODE		0x04    /* on = BT485 mode, off = TI3020 mode */
#define   TI_TC_NONVGAMODE	0x02    /* on = nonvgamode, off = vgamode */
#define   TI_TC_8BIT		0x01    /* on = 8/4bit, off = 16/32bit */
#define TI_VGA_SWITCH_CONTROL	0x0F    /* 3025 only */
#define TI_LATCH_CONTROL	0x0F    /* 3026 only */
#define TI_WINDOW_START_X_LOW	0x10
#define TI_WINDOW_START_X_HIGH	0x11
#define TI_WINDOW_STOP_X_LOW	0x12
#define TI_WINDOW_STOP_X_HIGH	0x13
#define TI_WINDOW_START_Y_LOW	0x14
#define TI_WINDOW_START_Y_HIGH	0x15
#define TI_WINDOW_STOP_Y_LOW	0x16
#define TI_WINDOW_STOP_Y_HIGH	0x17
#define TI_MUX_CONTROL_1	0x18
#define   TI_MUX1_PSEUDO_COLOR	0x80
#define   TI_MUX1_DIRECT_888	0x06
#define   TI_MUX1_DIRECT_565	0x05
#define   TI_MUX1_DIRECT_555	0x04
#define   TI_MUX1_DIRECT_664	0x03
#define   TI_MUX1_TRUE_888	0x46
#define   TI_MUX1_TRUE_565	0x45
#define   TI_MUX1_TRUE_555	0x44
#define   TI_MUX1_TRUE_664	0x43
#define   TI_MUX1_3025D_888	0x0E     /* 3025 only */
#define   TI_MUX1_3025D_565	0x0D     /* 3025 only */
#define   TI_MUX1_3025D_555	0x0C     /* 3025 only */
#define   TI_MUX1_3025T_888	0x4E     /* 3025 only */
#define   TI_MUX1_3025T_565	0x4D     /* 3025 only */
#define   TI_MUX1_3025T_555	0x4C     /* 3025 only */
#define   TI_MUX1_3026D_888	0x06     /* 3026 only */
#define   TI_MUX1_3026D_565	0x05     /* 3026 only */
#define   TI_MUX1_3026D_555	0x04     /* 3026 only */
#define   TI_MUX1_3026D_888_P8	0x16     /* 3026 only */
#define   TI_MUX1_3026D_888_P5	0x1e     /* 3026 only */
#define   TI_MUX1_3026T_888	0x46     /* 3026 only */
#define   TI_MUX1_3026T_565	0x45     /* 3026 only */
#define   TI_MUX1_3026T_555	0x44     /* 3026 only */
#define   TI_MUX1_3026T_888_P8	0x56     /* 3026 only */
#define   TI_MUX1_3026T_888_P5	0x5e     /* 3026 only */
#define TI_MUX_CONTROL_2	0x19
#define   TI_MUX2_BUS_VGA	0x98
#define   TI_MUX2_BUS_PC_D8P64	0x1C
#define   TI_MUX2_BUS_DC_D24P64	0x1C
#define   TI_MUX2_BUS_DC_D16P64	0x04
#define   TI_MUX2_BUS_DC_D15P64	0x04
#define   TI_MUX2_BUS_TC_D24P64	0x04
#define   TI_MUX2_BUS_TC_D16P64	0x04
#define   TI_MUX2_BUS_TC_D15P64	0x04
#define   TI_MUX2_BUS_3026PC_D8P64	0x4C
#define   TI_MUX2_BUS_3026DC_D24P64	0x5C
#define   TI_MUX2_BUS_3026DC_D16P64	0x54
#define   TI_MUX2_BUS_3026DC_D15P64	0x54
#define   TI_MUX2_BUS_3026TC_D24P64	0x5c
#define   TI_MUX2_BUS_3026TC_D16P64	0x54
#define   TI_MUX2_BUS_3026TC_D15P64	0x54
#define   TI_MUX2_BUS_3030PC_D8P128	0x4d
#define   TI_MUX2_BUS_3030DC_D24P128	0x5d
#define   TI_MUX2_BUS_3030DC_D16P128	0x55
#define   TI_MUX2_BUS_3030DC_D15P128	0x55
#define   TI_MUX2_BUS_3030TC_D24P128	0x5d
#define   TI_MUX2_BUS_3030TC_D16P128	0x55
#define   TI_MUX2_BUS_3030TC_D15P128	0x55
#define TI_INPUT_CLOCK_SELECT	0x1A
#define   TI_ICLK_CLK0		0x00
#define   TI_ICLK_CLK0_DOUBLE	0x10
#define   TI_ICLK_CLK1		0x01
#define   TI_ICLK_CLK1_DOUBLE	0x11
#define   TI_ICLK_CLK2		0x02     /* 3025 only */
#define   TI_ICLK_CLK2_DOUBLE	0x12     /* 3025 only */
#define   TI_ICLK_CLK2_I	0x03     /* 3025 only */
#define   TI_ICLK_CLK2_I_DOUBLE	0x13     /* 3025 only */
#define   TI_ICLK_CLK2_E	0x04     /* 3025 only */
#define   TI_ICLK_CLK2_E_DOUBLE	0x14     /* 3025 only */
#define   TI_ICLK_PLL		0x05     /* 3025 only */
#define TI_OUTPUT_CLOCK_SELECT	0x1B
#define   TI_OCLK_VGA		0x3E
#define   TI_OCLK_S		0x40
#define   TI_OCLK_NS		0x80     /* 3025 only */
#define   TI_OCLK_V1		0x00
#define   TI_OCLK_V2		0x08
#define   TI_OCLK_V4		0x10
#define   TI_OCLK_V8		0x18
#define   TI_OCLK_R1		0x00
#define   TI_OCLK_R2		0x01
#define   TI_OCLK_R4		0x02
#define   TI_OCLK_R8		0x03
#define   TI_OCLK_S_V1_R8	(TI_OCLK_S | TI_OCLK_V1 | TI_OCLK_R8)
#define   TI_OCLK_S_V2_R8	(TI_OCLK_S | TI_OCLK_V2 | TI_OCLK_R8)
#define   TI_OCLK_S_V4_R8	(TI_OCLK_S | TI_OCLK_V4 | TI_OCLK_R8)
#define   TI_OCLK_S_V8_R8	(TI_OCLK_S | TI_OCLK_V8 | TI_OCLK_R8)
#define   TI_OCLK_S_V2_R4	(TI_OCLK_S | TI_OCLK_V2 | TI_OCLK_R4)
#define   TI_OCLK_S_V4_R4	(TI_OCLK_S | TI_OCLK_V4 | TI_OCLK_R4)
#define   TI_OCLK_S_V1_R2	(TI_OCLK_S | TI_OCLK_V1 | TI_OCLK_R2)
#define   TI_OCLK_S_V2_R2	(TI_OCLK_S | TI_OCLK_V2 | TI_OCLK_R2)
#define   TI_OCLK_NS_V1_R1	(TI_OCLK_NS | TI_OCLK_V1 | TI_OCLK_R1)
#define   TI_OCLK_NS_V2_R2	(TI_OCLK_NS | TI_OCLK_V2 | TI_OCLK_R2)
#define   TI_OCLK_NS_V4_R4	(TI_OCLK_NS | TI_OCLK_V4 | TI_OCLK_R4)
#define TI_PALETTE_PAGE		0x1C
#define TI_GENERAL_CONTROL	0x1D
#define TI_MISC_CONTROL		0x1E     /* 3025 only */
#define   TI_MC_POWER_DOWN	0x01
#define   TI_MC_DOTCLK_DISABLE	0x02
#define   TI_MC_INT_6_8_CONTROL	0x04     /* 00 == external 6/8 pin */
#define   TI_MC_8_BPP		0x08     /* 00 == 6bpp */
#define   TI_MC_PSEL_POLARITY	0x20	 /* 3026 only, PSEL polarity select */
#define   TI_MC_VCLK_POLARITY	0x20
#define   TI_MC_LCLK_LATCH	0x40     /* VCLK == 00, default */
#define   TI_MC_LOOP_PLL_RCLK	0x80
#define TI_OVERSCAN_COLOR_RED	0x20
#define TI_OVERSCAN_COLOR_GREEN	0x21
#define TI_OVERSCAN_COLOR_BLUE	0x22
#define TI_CURSOR_COLOR_0_RED	0x23
#define TI_CURSOR_COLOR_0_GREEN	0x24
#define TI_CURSOR_COLOR_0_BLUE	0x25
#define TI_CURSOR_COLOR_1_RED	0x26
#define TI_CURSOR_COLOR_1_GREEN	0x27
#define TI_CURSOR_COLOR_1_BLUE	0x28
#define TI_AUXILIARY_CONTROL	0x29
#define   TI_AUX_SELF_CLOCK	0x08
#define   TI_AUX_W_CMPL		0x01
#define TI_GENERAL_IO_CONTROL	0x2A
#define   TI_GIC_ALL_BITS	0x1F
#define TI_GENERAL_IO_DATA	0x2B
#define   TI_GID_W2000_6BIT     0x00
#define   TI_GID_N9_964		0x01
#define   TI_GID_ELSA_SOG	0x04
#define   TI_GID_W2000_8BIT     0x08
#define   TI_GID_S3_DAC_6BIT	0x1C
#define   TI_GID_S3_DAC_8BIT	0x1E
#define   TI_GID_TI_DAC_6BIT	0x1D
#define   TI_GID_TI_DAC_8BIT	0x1F
#define TI_PLL_CONTROL		0x2C    /* 3025 only */
#define TI_PIXEL_CLOCK_PLL_DATA	0x2D    /* 3025 only */
#define   TI_PLL_ENABLE		0x08    /* 3025 only */
#define TI_MCLK_PLL_DATA	0x2E    /* 3025 only */
#define TI_LOOP_CLOCK_PLL_DATA	0x2F    /* 3025 only */
#define TI_COLOR_KEY_OLVGA_LOW	0x30
#define TI_COLOR_KEY_OLVGA_HIGH	0x31
#define TI_COLOR_KEY_RED_LOW	0x32
#define TI_COLOR_KEY_RED_HIGH	0x33
#define TI_COLOR_KEY_GREEN_LOW	0x34
#define TI_COLOR_KEY_GREEN_HIGH	0x35
#define TI_COLOR_KEY_BLUE_LOW	0x36
#define TI_COLOR_KEY_BLUE_HIGH	0x37
#define TI_COLOR_KEY_CONTROL	0x38
#define   TI_COLOR_KEY_CMPL	0x10
#define TI_MCLK_DCLK_CONTROL	0x39    /* 3025 only */
#define TI_MCLK_LCLK_CONTROL	0x39    /* 3026 only */
#define TI_SENSE_TEST		0x3A
#define TI_TEST_DATA		0x3B
#define TI_CRC_LOW		0x3C
#define TI_CRC_HIGH		0x3D
#define TI_CRC_CONTROL		0x3E
#define TI_ID			0x3F
#define   TI_VIEWPOINT20_ID	0x20
#define   TI_VIEWPOINT25_ID	0x25
#define TI_MODE_85_CONTROL	0xD5    /* 3025 only */

#define TI_REF_FREQ		14.31818  /* 3025 only */

/*
 * which clocks should be set (just flags...)
 */
#define TI_BOTH_CLOCKS	1
#define TI_LOOP_CLOCK	2

/* IBM ramdac registers */

#define IBMRGB_rev		0x00
#define IBMRGB_id		0x01
#define IBMRGB_misc_clock	0x02
#define IBMRGB_sync		0x03
#define IBMRGB_hsync_pos	0x04
#define IBMRGB_pwr_mgmt		0x05
#define IBMRGB_dac_op		0x06
#define IBMRGB_pal_ctrl		0x07
#define IBMRGB_sysclk		0x08  /* not RGB525 */
#define IBMRGB_pix_fmt		0x0a
#define IBMRGB_8bpp		0x0b
#define IBMRGB_16bpp		0x0c
#define IBMRGB_24bpp		0x0d
#define IBMRGB_32bpp		0x0e
#define IBMRGB_pll_ctrl1	0x10
#define IBMRGB_pll_ctrl2	0x11
#define IBMRGB_pll_ref_div_fix	0x14
#define IBMRGB_sysclk_ref_div	0x15  /* not RGB525 */
#define IBMRGB_sysclk_vco_div	0x16  /* not RGB525 */
#define IBMRGB_f0		0x20
#define IBMRGB_m0		0x20
#define IBMRGB_n0		0x21
#define IBMRGB_curs		0x30
#define IBMRGB_curs_xl		0x31
#define IBMRGB_curs_xh		0x32
#define IBMRGB_curs_yl		0x33
#define IBMRGB_curs_yh		0x34
#define IBMRGB_curs_hot_x	0x35
#define IBMRGB_curs_hot_y	0x36
#define IBMRGB_curs_col1_r	0x40
#define IBMRGB_curs_col1_g	0x41
#define IBMRGB_curs_col1_b	0x42
#define IBMRGB_curs_col2_r	0x43
#define IBMRGB_curs_col2_g	0x44
#define IBMRGB_curs_col2_b	0x45
#define IBMRGB_curs_col3_r	0x46
#define IBMRGB_curs_col3_g	0x47
#define IBMRGB_curs_col3_b	0x48
#define IBMRGB_border_col_r	0x60
#define IBMRGB_border_col_g	0x61
#define IBMRGB_botder_col_b	0x62
#define IBMRGB_misc1		0x70
#define IBMRGB_misc2		0x71
#define IBMRGB_misc3		0x72
#define IBMRGB_misc4		0x73  /* not RGB525 */
#define IBMRGB_dac_sense	0x82
#define IBMRGB_misr_r		0x84
#define IBMRGB_misr_g		0x86
#define IBMRGB_misr_b		0x88
#define IBMRGB_pll_vco_div_in	0x8e
#define IBMRGB_pll_ref_div_in	0x8f
#define IBMRGB_vram_mask_0	0x90
#define IBMRGB_vram_mask_1	0x91
#define IBMRGB_vram_mask_2	0x92
#define IBMRGB_vram_mask_3	0x93
#define IBMRGB_curs_array	0x100

#endif
