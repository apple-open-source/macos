/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/gfx/vip_1400.c,v 1.1 2002/12/10 15:12:28 alanh Exp $ */
/*-----------------------------------------------------------------------------
 * VIP_1400.C
 *
 * Version 2.0 - February 21, 2000.
 *
 * This file routines to control the SC1400 video input port (VIP) hardware.
 *
 * History:
 *    Versions 0.1 through 2.0 by Brian Falardeau.
 *
 * Copyright (c) 1999-2000 National Semiconductor.
 *-----------------------------------------------------------------------------
 */

/*-----------------------------------------------------------------------------
 * gfx_set_vip_enable
 *
 * This routine enables or disables the writes to memory from the video port.  
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
sc1400_set_vip_enable(int enable)
#else
int
gfx_set_vip_enable(int enable)
#endif
{
   unsigned long mcr, value;

   value = READ_VIP32(SC1400_VIP_CONTROL);

   if (enable) {
      /* CONFIGURE MCR FOR VIDEO INPUT MODE */

      mcr = IND(SC1400_CB_BASE_ADDR + SC1400_CB_MISC_CONFIG);
      mcr |= (SC1400_MCR_VPOUT_CK_SELECT | SC1400_MCR_VPOUT_CK_SOURCE);
      mcr &= ~SC1400_MCR_VPOUT_MODE;
      mcr |= SC1400_MCR_VPIN_CCIR656;
      mcr &= ~SC1400_MCR_GENLOCK_CONTINUE;
      OUTD(SC1400_CB_BASE_ADDR + SC1400_CB_MISC_CONFIG, mcr);

      /* ENABLE CAPTURE */
      /* Hardcode config values for now. */

      WRITE_VIP32(SC1400_VIP_CONFIG, 0x30012);
      value |= 0x103;
   } else {
      value &= ~(0x102);
   }

   WRITE_VIP32(SC1400_VIP_CONTROL, value);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vip_base
 *
 * This routine sets the odd and even base address values for the VIP memory
 * buffer.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
sc1400_set_vip_base(unsigned long even, unsigned long odd)
#else
int
gfx_set_vip_base(unsigned long even, unsigned long odd)
#endif
{
   // TRUE OFFSET IS SPECIFIED, NEED TO SET BIT 23 FOR HARDWARE

   WRITE_VIP32(SC1400_VIP_EVEN_BASE, even | 0x00800000);
   WRITE_VIP32(SC1400_VIP_ODD_BASE, odd | 0x00800000);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vip_pitch
 *
 * This routine sets the number of bytes between scanlines for the VIP data.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
sc1400_set_vip_pitch(unsigned long pitch)
#else
int
gfx_set_vip_pitch(unsigned long pitch)
#endif
{
   WRITE_VIP32(SC1400_VIP_PITCH, pitch & 0x0000FFFC);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vbi_enable
 *
 * This routine enables or disables the VBI data capture.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
sc1400_set_vbi_enable(int enable)
#else
int
gfx_set_vbi_enable(int enable)
#endif
{
   unsigned long value;

   value = READ_VIP32(SC1400_VIP_CONTROL);
   if (enable)
      value |= SC1400_VIP_VBI_CAPTURE_EN;
   else
      value &= ~SC1400_VIP_VBI_CAPTURE_EN;
   WRITE_VIP32(SC1400_VIP_CONTROL, value);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vbi_base
 *
 * This routine sets the odd and even base address values for VBI capture.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
sc1400_set_vbi_base(unsigned long even, unsigned long odd)
#else
int
gfx_set_vbi_base(unsigned long even, unsigned long odd)
#endif
{
   // TRUE OFFSET IS SPECIFIED, NEED TO SET BIT 23 FOR HARDWARE

   WRITE_VIP32(SC1400_VBI_EVEN_BASE, even | 0x00800000);
   WRITE_VIP32(SC1400_VBI_ODD_BASE, odd | 0x00800000);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vbi_pitch
 *
 * This routine sets the number of bytes between scanlines for VBI capture.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
sc1400_set_vbi_pitch(unsigned long pitch)
#else
int
gfx_set_vbi_pitch(unsigned long pitch)
#endif
{
   WRITE_VIP32(SC1400_VBI_PITCH, pitch & 0x0000FFFC);
   return (0);
}

/*************************************************************/
/*  READ ROUTINES  |  INCLUDED FOR DIAGNOSTIC PURPOSES ONLY  */
/*************************************************************/

#if GFX_READ_ROUTINES

/*-----------------------------------------------------------------------------
 * gfx_get_vip_enable
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
sc1400_get_vip_enable(void)
#else
int
gfx_get_vip_enable(void)
#endif
{
   if (READ_VIP32(SC1400_VIP_CONTROL) & 0x00000100)
      return (1);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vip_base
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
sc1400_get_vip_base(int odd)
#else
unsigned long
gfx_get_vip_base(int odd)
#endif
{
   // MASK BIT 23 AND ABOVE TO MAKE IT A TRUE OFFSET

   if (odd)
      return (READ_VIP32(SC1400_VIP_ODD_BASE) & 0x007FFFFF);
   return (READ_VIP32(SC1400_VIP_EVEN_BASE) & 0x007FFFFF);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vip_pitch
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
sc1400_get_vip_pitch(void)
#else
unsigned long
gfx_get_vip_pitch(void)
#endif
{
   return (READ_VIP32(SC1400_VIP_PITCH) & 0x0000FFFF);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vbi_enable
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
sc1400_get_vbi_enable(void)
#else
int
gfx_get_vbi_enable(void)
#endif
{
   if (READ_VIP32(SC1400_VIP_CONTROL) & 0x00000200)
      return (1);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vbi_base
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
sc1400_get_vbi_base(int odd)
#else
unsigned long
gfx_get_vbi_base(int odd)
#endif
{
   // MASK BIT 23 AND ABOVE TO MAKE IT A TRUE OFFSET

   if (odd)
      return (READ_VIP32(SC1400_VBI_ODD_BASE) & 0x007FFFFF);
   return (READ_VIP32(SC1400_VBI_EVEN_BASE) & 0x007FFFFF);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vbi_pitch
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
sc1400_get_vbi_pitch(void)
#else
unsigned long
gfx_get_vbi_pitch(void)
#endif
{
   return (READ_VIP32(SC1400_VBI_PITCH) & 0x0000FFFF);
}

#endif /* GFX_READ_ROUTINES */

/* END OF FILE */
