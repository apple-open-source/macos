/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i128/Ti302X.h,v 1.2 2001/03/03 22:26:11 tsi Exp $ */
/*
 * Copyright 1994-2000 by Robin Cutshaw <robin@XFree86.Org>
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
/* $XConsortium: $ */

#include "compiler.h"

/* Indirect indexed registers */

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
