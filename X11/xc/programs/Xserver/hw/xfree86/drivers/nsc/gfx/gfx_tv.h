/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/gfx/gfx_tv.h,v 1.1 2002/12/10 15:12:26 alanh Exp $ */

typedef struct tagTVDISPLAYMODE
{
   /* DISPLAY MODE FLAGS */
   /* Specify valid color depths and the refresh rate. */

   unsigned short flags;

   /* TIMINGS */

   unsigned short hactive;
   unsigned short hblankstart;
   unsigned short hsyncstart;
   unsigned short hsyncend;
   unsigned short hblankend;
   unsigned short htotal;

   unsigned short vactive;
   unsigned short vblankstart;
   unsigned short vsyncstart;
   unsigned short vsyncend;
   unsigned short vblankend;
   unsigned short vtotal;

   /* CLOCK FREQUENCY */

   unsigned long frequency;

}
TVDISPLAYMODE;

TVDISPLAYMODE TVTimings[] = {

/* NTSC resolution    */
   {
    0x3,				/* negative syncs     */
    0x0280, 0x0280, 0x0290, 0x02E8, 0x0318, 0x0318,	/* horizontal timings */
    0x01E0, 0x01E0, 0x01EA, 0x01EC, 0x0205, 0x020D,	/* vertical timings   */
    0x0018EC4D,				/* freq = 24.923052 MHz  */
    },

/* PAL resolution     */
   {
    0x3,				/* positive syncs     */
    0x0300, 0x0300, 0x0320, 0x0350, 0x0360, 0x0360,	/* horizontal timings */
    0x0240, 0x0240, 0x024A, 0x024C, 0x0271, 0x0271,	/* vertical timings   */
    0x001B0000,				/* freq = 27.00 MHz   */
    },

/* PAL resolution non-square pixels */
   {
    0x3,				/* positive syncs     */
    0x02C0, 0x02C0, 0x02F0, 0x0330, 0x0360, 0x0360,	/* horizontal timings */
    0x0240, 0x0240, 0x024A, 0x024C, 0x0271, 0x0271,	/* vertical timings   */
    0x001B0000,				/* freq = 27.00 MHz   */
    }
};

#define NUM_TV_MODES sizeof(TVTimings)/sizeof(TVDISPLAYMODE)
