#ifndef __CMD2D_H__
#define __CMD2D_H__

/*
 * commands the 2d microcode offers
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/rendition/cmd2d.h,v 1.2 1999/10/14 04:43:14 dawes Exp $ */

#define CMD_SETUP					32
#define CMD_SET_PIX_REG				2
#define CMD_RECT_SOLID				1
#define CMD_RECT_SOLID_ROP			41
#define CMD_SET_FGCOLOR				50
#define CMD_DD_RECT_SOLID			38
#define CMD_SCREEN_BLT				12
#define CMD_DD_SCREEN_BLT			37
#define CMD_BITBLT_MS_COLOR			7
#define CMD_BITBLT_MS_MONO			22
#define CMD_LOAD_COLOR_BRUSH		5
#define CMD_RECT_COLOR_BRUSH		6 
#define CMD_RECT_COLOR_BRUSH_ROP	24
#define CMD_LOAD_MONO_BRUSH			4
#define CMD_RECT_MONO_BRUSH			3
#define CMD_RECT_MONO_BRUSH_ROP		42
#define CMD_SET_PIXEL				34
#define CMD_GET_PIXEL				9
#define CMD_LINE_SOLID				20
#define CMD_LINE_STYLE				21
#define CMD_SCAN_LINE_SOLID			15
#define CMD_BSCAN_SOLID				19
#define CMD_BSCAN_COLOR				14
#define CMD_BSCAN_MONO				13
#define CMD_ENDSCAN					18
#define CMD_SCANLINE_COLOR_BRUSH	17
#define CMD_SCANLINE_MONO_BRUSH		16
#define CMD_STRETCH_BLT				23
#define CMD_DD_STRETCH_BLT			39
#define CMD_LOAD_COLOR_TABLE		25
#define CMD_BLTDIB1					26
#define CMD_BLTDIB4					27
#define CMD_BLTDIB8					28
#define CMD_BLTRLE4					30
#define CMD_BLTRLE8					31
#define CMD_SET_PALETTE				33
#define CMD_SETCLIPPING				36
#define CMD_DRAWGLYPHS				35
#define CMD_DD_YUV2RGB				43
#define CMD_DD_YUV2RGBCk			44



/*
 * pixel formats (R: red, G: green, B: blue, I: intensity/index, A: alpha)
 *
 * <ml>: this is redundant; one should use the enums in vtypes.h.
 */

#define FMT_SOURCE					0
#define FMT_8_332_RGB				1
#define FMT_8_I						2
#define FMT_8_A						3
#define FMT_16_565_RGB				4
#define FMT_16_4444_ARGB			5
#define FMT_16_1555_ARGB			6
#define FMT_4I_565_RGB				8
#define FMT_4I_444_ARGB				9
#define FMT_4I_1555_ARGB			0xa
#define FMT_32_8888_ARGB			0xc
#define FMT_32_888_Y0CrY1Cb			0xd



/*
 * stride values for address calculation
 */

#define STRIDE0_0					0
#define STRIDE0_8					1
#define STRIDE0_9					2
#define STRIDE0_10					3
#define STRIDE0_2					4
#define STRIDE1_0					0
#define STRIDE1_4					1
#define STRIDE1_5					2
#define STRIDE1_6					3
#define STRIDE1_7					4
#define STRIDE1_10					5
#define STRIDE1_11					6
#define STRIDE1_12					7



/*
 * raster operations (S: source, D: destination, N: not)
 */

#define ROP_ALLBITS0				0
#define ROP_NOR_SD					1
#define ROP_AND_NSD					2
#define ROP_NOT_S					3
#define ROP_AND_SND					4
#define ROP_NOT_D					5
#define ROP_XOR_SD					6
#define ROP_NAND_SD					7
#define ROP_AND_SD					8
#define ROP_XNOR_SD					9
#define ROP_D						0xa
#define ROP_OR_NSD					0xb
#define ROP_S						0xc
#define ROP_OR_SND					0xd
#define ROP_OR_SD					0xe
#define ROP_ALLBITS1				0xf



/*
 * ... modes
 */

#define MODE_TRANSPARENT			0
#define MODE_OPAQUE					1



/*
 * 8x8 predefined monochrome brushes
 */

#define HS_HORIZONTAL				0
#define HS_VERTICAL					1
#define HS_FDIAGONAL				2
#define HS_BDIAGONAL				3
#define HS_CROSS					4
#define HS_DIAGCROSS				5



/*
 * pen styles for styled lined
 */

#define PS_DASH						1
#define PS_DOT						2
#define PS_DASHDOT					3
#define PS_DASHDOTDOT				4

/* end of __CMD2D_H__ */
#endif
