/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i128/IBMRGB.h,v 1.1 2000/10/04 23:34:58 robin Exp $ */



/* $XConsortium: $ */

#define IBMRGB_REF_FREQ_1       14.31818
#define IBMRGB_REF_FREQ_2       50.00000

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


