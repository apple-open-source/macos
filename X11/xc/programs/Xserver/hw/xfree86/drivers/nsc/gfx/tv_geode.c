/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/gfx/tv_geode.c,v 1.1 2002/12/10 15:12:27 alanh Exp $ */
/*-----------------------------------------------------------------------------
 * TV_GEODE.C
 *
 * Version 1.20 - February 9, 2000
 *
 * This file contains routines to program the TV encoder when it is 
 * integrated onto a Geode processor.
 *
 * History:
 *    Initial version ported from code by Ilia Stolov.
 *    Versions 0.1 through 1.20 by Brian Falardeau.
 *
 * Copyright (c) 1999-2000 National Semiconductor.
 *-----------------------------------------------------------------------------
 */

/*-----------------------------------------------------------------------------
 * gfx_set_tv_defaults
 *
 * This routine sets all of the TV encoder registers to default values for 
 * the specified format.  Currently only NTSC is supported.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
geode_set_tv_defaults(int format)
#else
int
gfx_set_tv_defaults(int format)
#endif
{
   /* SET DEFAULTS FOR NTSC */

   WRITE_VID32(SC1400_TVOUT_HORZ_TIM, 0x00790359);
   WRITE_VID32(SC1400_TVOUT_HORZ_SYNC, 0x03580350);
   WRITE_VID32(SC1400_TVOUT_VERT_SYNC, 0x0A002001);
   WRITE_VID32(SC1400_TVOUT_LINE_END, 0x039C00F0);
   WRITE_VID32(SC1400_TVOUT_VERT_DOWNSCALE, 0xFFFFFFFF);
   WRITE_VID32(SC1400_TVOUT_HORZ_SCALING, 0x10220700);
   WRITE_VID32(SC1400_TVOUT_EMMA_BYPASS, 0x0002D0F0);
   WRITE_VID32(SC1400_TVENC_TIM_CTRL_1, 0xA2E03000);
   WRITE_VID32(SC1400_TVENC_TIM_CTRL_2, 0x1FF20000);
   WRITE_VID32(SC1400_TVENC_TIM_CTRL_3, 0x00000000);
   WRITE_VID32(SC1400_TVENC_SUB_FREQ, 0x21F12000);
   WRITE_VID32(SC1400_TVENC_DISP_POS, 0x00030071);
   WRITE_VID32(SC1400_TVENC_DISP_SIZE, 0x00EF02CF);

   /* ### ADD ### DEFAULTS FOR PAL */
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_tv_enable
 *
 * This routine enables or disables the TV output.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
geode_set_tv_enable(int enable)
#else
int
gfx_set_tv_enable(int enable)
#endif
{
   unsigned long value;

   value = READ_VID32(SC1400_DISPLAY_CONFIG);
   if (enable)
      value |= SC1400_DCFG_TVOUT_EN;
   else
      value &= ~(SC1400_DCFG_TVOUT_EN);
   WRITE_VID32(SC1400_DISPLAY_CONFIG, value);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_tv_cc_enable
 *
 * This routine enables or disables the use of the hardware CC registers 
 * in the TV encoder.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
geode_set_tv_cc_enable(int enable)
#else
int
gfx_set_tv_cc_enable(int enable)
#endif
{
   unsigned long value;

   value = READ_VID32(SC1400_TVENC_CC_CONTROL);
   value &= ~(0x0005F);
   if (enable)
      value |= 0x51;
   WRITE_VID32(SC1400_TVENC_CC_CONTROL, value);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_tv_cc_data
 *
 * This routine writes the two specified characters to the CC data register 
 * of the TV encoder.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
geode_set_tv_cc_data(unsigned char data1, unsigned char data2)
#else
int
gfx_set_tv_cc_data(unsigned char data1, unsigned char data2)
#endif
{
   unsigned long value;

   value = data1 | (((unsigned long)data2) << 8);
   WRITE_VID32(SC1400_TVENC_CC_DATA, value);
   return (0);
}

/* END OF FILE */
