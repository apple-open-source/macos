/*
 * Copyright 1997,1998 by Alan Hourihane <alanh@fairlite.demon.co.uk>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Alan Hourihane not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Alan Hourihane makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ALAN HOURIHANE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ALAN HOURIHANE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Alan Hourihane, <alanh@fairlite.demon.co.uk>
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tga/tga_dac.c,v 1.14 2001/10/28 03:33:50 tsi Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "BT.h"
#include "tga_regs.h"
#include "tga.h"

static void ICS1562ClockSelect(ScrnInfoPtr pScrn, int freq);
static void ICS9110ClockSelect(ScrnInfoPtr pScrn, int freq);
extern void ICS1562_CalcClockBits(long f, unsigned char *bits);

static void
ICS1562ClockSelect(ScrnInfoPtr pScrn, int freq)
{
  TGAPtr pTga = TGAPTR(pScrn);
  unsigned char pll_bits[7];
  unsigned long temp;
  int i, j;

  /* There lies an ICS1562 Clock Generator. */
  ICS1562_CalcClockBits(freq, pll_bits);

  /*
   * For the DEC 21030 TGA:
   * This requires the 55 clock bits be written in a serial manner to
   * bit 0 of the CLOCK register and on the 56th bit set the hold flag.
   */
  for (i = 0;i <= 6; i++) {
    for (j = 0; j <= 7; j++) {
      temp = (pll_bits[i] >> (7-j)) & 1;
      if (i == 6 && j == 7)
	temp |= 2;
      TGA_WRITE_REG(temp, TGA_CLOCK_REG);
    }
  }
}

struct monitor_data tga_crystal_table[] = {
{
/*  Option 0 Monitor Info  130.8            */
1024,                        /* rows                         */
1280,                        /* columns                      */
130,                         /* 130.8 Mhz                    */
72,                          /* refresh rate                 */
1024,                        /* v scanlines                  */
3,                           /* v front porch                */
3,                           /* v sync                       */
33,                          /* v back porch                 */
1280,                        /* h pixels                     */
32,                          /* h front porch                */
160,                         /* h sync                       */
232,                         /* h back porch                 */
/* AV9110 clock serial load information    130.808     */
0x40,                           /* 0:6  VCO frequency divider  N         */
0x7,                            /* 7:13 Reference frequency divide  M   */
0x0,                            /* 14 VCO pre-scale divide V (0=div.by 1,1=by 8)
*/
0x1,                            /* 15:16 CLK/X output divide X          */
0x1,                            /* 17:18 VCO output divide R            */
1,                              /* 19 CLK Output enable. */
1,                              /* 20 CLK/X Output enable */
0,                              /* reserved, should be set to 0         */
0,                              /* Reference clock select on CLK 1=ref  */
1,                              /* reserved, should be set to 1         */
/* IBM561 PLL setup data 130.808 */
0xC8,                           /* VCO Div: PFR=0x3, M-65=49 */
0x8                             /* REF: N=0x8 */
},
{
/*  Option 3 Monitor Info 104.00 Mhz                   */
900,                         /* rows                         */
1152,                        /* columns                      */
104,                         /* 104 Mhz                      */
72,                          /* refresh rate                 */
900,                         /* v scanlines                  */
6,                           /* v front porch                */
10,                          /* v sync                       */
44,                          /* v back porch                 */
1152,                        /* h pixels                     */
64,                          /* h front porch                */
112,                         /* h sync                       */
176,                         /* h back porch                 */
/* 103.994 MHz av9110 clock serial load information         */
0x6d,                           /* 0:6  VCO frequency divider  N         */
0xf,                            /* 7:13 Reference frequency divide  M   */
0x0,                            /* 14 VCO pre-scale divide V (0=div.by 1,1=by 8)
*/
0x1,                            /* 15:16 CLK/X output divide X          */
0x1,                            /* 17:18 VCO output divide R            */
1,                              /* 19 CLK Output enable. */
1,                              /* 20 CLK/X Output enable */
0,                              /* reserved, should be set to 0         */
0,                              /* Reference clock select on CLK 1=ref  */
1,                              /* reserved, should be set to 1         */
/* 104.00 MHz IBM561 PLL setup data */
0x96,                           /* VCO Div: PFR=2, M=57 */
0x6                             /* REF: N=6 */
},
#if 1
{
/*  Option 6 Monitor Info 74.00 Mhz                    */
768,                         /* rows                         */
1024,                        /* columns                      */
74,                          /* 74 Mhz                       */
72,                          /* refresh rate                 */
768,                         /* v scanlines                  */
1,                           /* v front porch                */
6,                           /* v sync                       */
22,                          /* v back porch                 */
1024,                        /* h pixels                     */
16,                          /* h front porch                */
128,                         /* h sync                       */
128,                         /* h back porch                 */
/* 74.00 MHz AV9110 clock serial load information         */
0x2a,                           /* 0:6  VCO frequency divider  N         */
0x41,                           /* 7:13 Reference frequency divide  M   */
0x1,                            /* 14 VCO pre-scale divide V (0=div.by 1,1=by 8)
*/
0x1,                            /* 15:16 CLK/X output divide X          */
0x1,                            /* 17:18 VCO output divide R            */
1,                              /* 19 CLK Output enable. */
1,                              /* 20 CLK/X Output enable */
0,                              /* reserved, should be set to 0         */
0,                              /* Reference clock select on CLK 1=ref  */
1,                              /* reserved, should be set to 1         */
/* 74.00 MHz IBM561 PLL setup data */
0x9C,                           /* VCO Div: PFR=2, M=0x5D*/
0x9                             /* REF: N=0x9 */
},
#else
{
/*  Option 5 Monitor Info 75.00 Mhz                    */
768,                         /* rows                         */
1024,                        /* columns                      */
75,                          /* 74 Mhz                       */
70,                          /* refresh rate                 */
768,                         /* v scanlines                  */
3,                           /* v front porch                */
6,                           /* v sync                       */
29,                          /* v back porch                 */
1024,                        /* h pixels                     */
24,                          /* h front porch                */
136,                         /* h sync                       */
144,                         /* h back porch                 */
/* 75.00 MHz AV9110 clock serial load information         */
0x6e,                           /* 0:6  VCO frequency divider  N         */
0x15,                           /* 7:13 Reference frequency divide  M   */
0x0,                            /* 14 VCO pre-scale divide V (0=div.by 1,1=by 8)
*/
0x1,                            /* 15:16 CLK/X output divide X          */
0x1,                            /* 17:18 VCO output divide R            */
1,                              /* 19 CLK Output enable. */
1,                              /* 20 CLK/X Output enable */
0,                              /* reserved, should be set to 0         */
0,                              /* Reference clock select on CLK 1=ref  */
1,                              /* reserved, should be set to 1         */
/* 75.00 MHz IBM561 PLL setup data */
0x93,                           /* VCO Div: PFR=2, M=0x54 */
0x8                             /* REF: N=0x8 */
},
#endif
{
/*  Option 9 Monitor Info 50 Mhz ergo SVGA             */
600,                         /* rows                         */
800,                         /* columns                      */
50,                          /* 50 Mhz                       */
72,                          /* refresh rate                 */
600,                         /* v scanlines                  */
37,                          /*(31 tga)v front porch                */
6,                           /* v sync                       */
23,                          /*(29 tga)v back porch                 */
800,                         /* h pixels                     */
56,                          /* h front porch                */
120,                         /* h sync                       */
64,                          /* h back porch                 */
/*50.00 Mhz AV9110 clock serial load information         */
0x37,                           /* 0:6  VCO frequency divider  N         */
0x3f,                           /* 7:13 Reference frequency divide  M   */
0x1,                            /* 14 VCO pre-scale divide V (0=div.by 1,1=by 8)
*/
0x1,                            /* 15:16 CLK/X output divide X          */
0x2,                            /* 17:18 VCO output divide R            */
1,                              /* 19 CLK Output enable. */
1,                              /* 20 CLK/X Output enable */
0,                              /* reserved, should be set to 0         */
0,                              /* Reference clock select on CLK 1=ref  */
1,                              /* reserved, should be set to 1         */
/* 50.00 MHz IBM561 PLL setup data */
0x45,                           /* VCO Div: PFR=1, M=46*/
0x5                             /* REF: N=5 */
},
{
/*  Option B Monitor Info 31.5 Mhz ergo VGA            */
480,                         /* rows                         */
640,                         /* columns                      */
32,                          /* 32 Mhz                       */
72,                          /* refresh rate                 */
480,                         /* v scanlines                  */
9,                           /* v front porch                */
3,                           /* v sync                       */
28,                          /* v back porch                 */
640,                         /* h pixels                     */
24,                          /* h front porch                */
40,                          /* h sync                       */
128,                         /* h back porch                 */
/* 31.50 MHz AV9110 clock serial load information         */
0x16,                           /* 0:6  VCO frequency divider  N         */
0x05,                           /* 7:13 Reference frequency divide  M   */
0x0,                            /* 14 VCO pre-scale divide V (0=div.by 1,1=by 8)
*/
0x1,                            /* 15:16 CLK/X output divide X          */
0x2,                            /* 17:18 VCO output divide R            */
1,                              /* 19 CLK Output enable. */
1,                              /* 20 CLK/X Output enable */
0,                              /* reserved, should be set to 0         */
0,                              /* Reference clock select on CLK 1=ref  */
1,                              /* reserved, should be set to 1         */
/* 31.50 MHz IBM561 PLL setup data  */
0x17,                           /* VCO Div: PFR=0, M=0x58 */
0x5                             /* REF: N=0x5 */
},
#ifdef ALLOW_LT_72_HZ
{
/*  Option 1 Monitor Info 119.84 Mhz                   */
1024,                        /* rows                         */
1280,                        /* columns                      */
119,                         /* 119 Mhz                      */
66,                          /* refresh rate                 */
1024,                        /* v scanlines                  */
3,                           /* v front porch                */
3,                           /* v sync                       */
33,                          /* v back porch                 */
1280,                        /* h pixels                     */
32,                          /* h front porch                */
160,                         /* h sync                       */
232,                         /* h back porch                 */
/* 119.84MHz AV9110 clock serial load information         */
0x2d,                           /* 0:6  VCO frequency divider  N         */
0x2b,                           /* 7:13 Reference frequency divide  M   */
0x1,                            /* 14 VCO pre-scale divide V (0=div.by 1,1=by 8) */
0x1,                            /* 15:16 CLK/X output divide X          */
0x1,                            /* 17:18 VCO output divide R            */
1,                              /* 19 CLK Output enable. */
1,                              /* 20 CLK/X Output enable */
0,                              /* reserved, should be set to 0         */
0,                              /* Reference clock select on CLK 1=ref  */
1,                              /* reserved, should be set to 1         */
/* IBM561 PLL setup data 119.84 */
0x82,                           /* VCO Div: PFR=0x2, M=0x43 */
0x4                             /* REF: N=0x4 */
},
{
/*  Option 2 Monitor Info 108.18 Mhz                   */
1024,                        /* rows                         */
1280,                        /* columns                      */
108,                         /* 108 Mhz                      */
60,                          /* refresh rate                 */
1024,                        /* v scanlines                  */
3,                           /* v front porch                */
3,                           /* v sync                       */
26,                          /* v back porch                 */
1280,                        /* h pixels                     */
44,                          /* h front porch                */
184,                         /* h sync                       */
200,                         /* h back porch                 */
/* 108.18 MHz av9110 Clk serial load information         */
0x11,                           /* 0:6  VCO frequency divider  N         */
0x9,                           /* 7:13 Reference frequency divide  M   */
0x1,                            /* 14 VCO pre-scale divide V (0=div.by 1,1=by 8)
*/
0x1,                            /* 15:16 CLK/X output divide X          */
0x2,                            /* 17:18 VCO output divide R            */
1,                              /* 19 CLK Output enable. */
1,                              /* 20 CLK/X Output enable */
0,                              /* reserved, should be set to 0         */
0,                              /* Reference clock select on CLK 1=ref  */
1,                              /* reserved, should be set to 1         */
/* 108.18 MHz IBM561 PLL setup data */
0xB8,                           /* VCO Div: PFR=2, M=79 */
0x8                             /* REF: N=0x8 */
},
{
/*  Option 5 Monitor Info 75.00 Mhz                    */
768,                         /* rows                         */
1024,                        /* columns                      */
75,                          /* 74 Mhz                       */
70,                          /* refresh rate                 */
768,                         /* v scanlines                  */
3,                           /* v front porch                */
6,                           /* v sync                       */
29,                          /* v back porch                 */
1024,                        /* h pixels                     */
24,                          /* h front porch                */
136,                         /* h sync                       */
144,                         /* h back porch                 */
/* 75.00 MHz AV9110 clock serial load information         */
0x6e,                           /* 0:6  VCO frequency divider  N         */
0x15,                           /* 7:13 Reference frequency divide  M   */
0x0,                            /* 14 VCO pre-scale divide V (0=div.by 1,1=by 8)
*/
0x1,                            /* 15:16 CLK/X output divide X          */
0x1,                            /* 17:18 VCO output divide R            */
1,                              /* 19 CLK Output enable. */
1,                              /* 20 CLK/X Output enable */
0,                              /* reserved, should be set to 0         */
0,                              /* Reference clock select on CLK 1=ref  */
1,                              /* reserved, should be set to 1         */
/* 75.00 MHz IBM561 PLL setup data */
0x93,                           /* VCO Div: PFR=2, M=0x54 */
0x8                             /* REF: N=0x8 */
},
{
/*  Option 7 Monitor Info 69 Mhz DEC 72 Hz             */
864,                         /* rows                         */
1024,                        /* columns                      */
69,                          /* 69.x Mhz                     */
60,                          /* refresh rate                 */
864,                         /* v scanlines                  */
0,                           /* v front porch                */
3,                           /* v sync                       */
34,                          /* v back porch                 */
1024,                        /* h pixels                     */
12,                          /* h front porch                */
128,                         /* h sync                       */
116,                         /* h back porch                 */
/* 69.00 Mhz AV9110 clock serial load information         */
0x35,                           /* 0:6  VCO frequency divider  N         */
0xb,                            /* 7:13 Reference frequency divide  M   */
0x0,                            /* 14 VCO pre-scale divide V (0=div.by 1,1=by 8)
*/
0x1,                            /* 15:16 CLK/X output divide X          */
0x1,                            /* 17:18 VCO output divide R            */
1,                              /* 19 CLK Output enable. */
1,                              /* 20 CLK/X Output enable */
0,                              /* reserved, should be set to 0         */
0,                              /* Reference clock select on CLK 1=ref  */
1,                              /* reserved, should be set to 1         */
/* 69.00 MHz IBM561 PLL setup data */
0xA9,                           /* VCO Div: PFR=2, M=0x6A */
0xB                             /* REF: N=0xB */
},
{
/*  Option 8 Monitor Info 65 Mhz                       */
768,                         /* rows                         */
1024,                        /* columns                      */
65,                          /* 65 Mhz                       */
60,                          /* refresh rate                 */
768,                         /* v scanlines                  */
7,                           /* v front porch                */
9,                           /* v sync                       */
26,                          /* v back porch                 */
1024,                        /* h pixels                     */
56,                          /* h front porch                */
64,                          /* h sync                       */
200,                         /* h back porch                 */
/* 65.00 MHz AV9110 clock serial load information         */
0x6d,                           /* 0:6  VCO frequency divider  N         */
0x0c,                           /* 7:13 Reference frequency divide  M   */
0x0,                            /* 14 VCO pre-scale divide V (0=div.by 1,1=by 8)
*/
0x1,                            /* 15:16 CLK/X output divide X          */
0x2,                            /* 17:18 VCO output divide R            */
1,                              /* 19 CLK Output enable. */
1,                              /* 20 CLK/X Output enable */
0,                              /* reserved, should be set to 0         */
0,                              /* Reference clock select on CLK 1=ref  */
1,                              /* reserved, should be set to 1         */
/* 65.00 MHz IBM561 PLL setup data */
0xAC,                           /* VCO Div: PFR=2, M=0x6D */
0xC                             /* REF: N=0xC */
},
{
/*  Option A Monitor Info 40  Mhz SVGA                 */
600,                         /* rows                         */
800,                         /* columns                      */
40,                          /* 40 Mhz                       */
60,                          /* refresh rate                 */
600,                         /* v scanlines                  */
1,                           /* v front porch                */
4,                           /* v sync                       */
23,                          /* v back porch                 */
800,                         /* h pixels                     */
40,                          /* h front porch                */
128,                         /* h sync                       */
88,                          /* h back porch                 */
/* 40.00 MHz AV9110 clock serial load information         */
0x5f,                           /* 0:6  VCO frequency divider  N         */
0x11,                           /* 7:13 Reference frequency divide  M   */
0x0,                            /* 14 VCO pre-scale divide V (0=div.by 1,1=by 8)
*/
0x1,                            /* 15:16 CLK/X output divide X          */
0x2,                            /* 17:18 VCO output divide R            */
1,                              /* 19 CLK Output enable. */
1,                              /* 20 CLK/X Output enable */
0,                              /* reserved, should be set to 0         */
0,                              /* Reference clock select on CLK 1=ref  */
1,                              /* reserved, should be set to 1         */
/* 40.00 MHz IBM561 PLL setup data  */
0x42,                           /* VCO Div: PFR=1, M=43 */
0x6                             /* REF: N=0x6 */
},
{
/*  Option C Monitor Info 25.175 Mhz VGA                      */
480,                         /* rows                         */
640,                         /* columns                      */
25,                          /* 25.175 Mhz                   */
60,                          /* refresh rate                 */
480,                         /* v scanlines                  */
10,                          /* v front porch                */
2,                           /* v sync                       */
33,                          /* v back porch                 */
640,                         /* h pixels                     */
16,                          /* h front porch                */
96,                          /* h sync                       */
48,                          /* h back porch                 */
/* 25.175 MHz  AV9110 clock serial load information         */
0x66,                           /* 0:6  VCO frequency divider  N         */
0x1d,                           /* 7:13 Reference frequency divide  M   */
0x0,                            /* 14 VCO pre-scale divide V (0=div.by 1,1=by 8)
*/
0x1,                            /* 15:16 CLK/X output divide X          */
0x2,                            /* 17:18 VCO output divide R            */
1,                              /* 19 CLK Output enable. */
1,                              /* 20 CLK/X Output enable */
0,                              /* reserved, should be set to 0         */
0,                              /* Reference clock select on CLK 1=ref  */
1,                              /* reserved, should be set to 1         */
/* 25.175 MHz IBM561 PLL setup data  */
0x3E,                           /* VCO Div: PFR=0, M=0x7F */
0x9                             /* REF: N=0x9 */
},
{
/*  Option E Monitor Info 110 Mhz                  */
1024,                        /* rows                         */
1280,                        /* columns                      */
110,
60,                          /* refresh rate                 */
1024,                        /* v scanlines                  */
6,                           /* v front porch                */
7,                           /* v sync                       */
44,                          /* v back porch                 */
1280,                        /* h pixels                     */
19,                          /* h front porch                */
163,                         /* h sync                       */
234,                         /* h back porch                 */
/* 110.0 MHz AV9110 clock serial load information         */
0x60,                           /* 0:6  VCO frequency divider  N         */
0x32,                           /* 7:13 Reference frequency divide  M   */
0x1,                            /* 14 VCO pre-scale divide V (0=div.by 1,1=by 8) */
0x1,                            /* 15:16 CLK/X output divide X          */
0x2,                            /* 17:18 VCO output divide R            */
1,                              /* 19 CLK Output enable. */
1,                              /* 20 CLK/X Output enable */
0,                              /* reserved, should be set to 0         */
0,                              /* Reference clock select on CLK 1=ref  */
1,                              /* reserved, should be set to 1         */
/* 110.0 MHz IBM561 PLL setup data  */
0xBA,                           /* VCO Div: PFR=0x2, M=0x7B */
0x8                             /* REF: N=0x8 */
},
#endif /* ALLOW_LT_72_HZ */
#ifdef ALLOW_GT_72_HZ
{
/*  Option D Monitor Info 135 Mhz                  */
1024,                        /* rows                         */
1280,                        /* columns                      */
135,                         /* 135 Mhz                      */
75,                          /* refresh rate                 */
1024,                        /* v scanlines                  */
1,                           /* v front porch                */
3,                           /* v sync                       */
38,                          /* v back porch                 */
1280,                        /* h pixels                     */
16,                          /* h front porch                */
144,                         /* h sync                       */
248,                         /* h back porch                 */
/* 135.0 MHz AV9110 clock serial load information         */
0x42,                           /* 0:6  VCO frequency divider  N         */
0x07,                           /* 7:13 Reference frequency divide  M   */
0x0,                            /* 14 VCO pre-scale divide V (0=div.by 1,1=by 8) */
0x1,                            /* 15:16 CLK/X output divide X          */
0x1,                            /* 17:18 VCO output divide R            */
1,                              /* 19 CLK Output enable. */
1,                              /* 20 CLK/X Output enable */
0,                              /* reserved, should be set to 0         */
0,                              /* Reference clock select on CLK 1=ref  */
1,                              /* reserved, should be set to 1         */
/* 135.0 MHz IBM561 PLL setup data  */
0xC1,                           /* VCO Div: PFR=0x3, M=0x42 */
0x7                             /* REF: N=0x7 */
},
#ifdef ALLOW_GT_1280x1024
{
/*  Option 4 Monitor Info 175.5 Mhz (8-plane)                */
1200,                        /* rows                         */
1600,                        /* columns                      */
175,                         /* clock */
65,                          /* refresh rate                 */
1200,                        /* v scanlines                  */
1,                           /* v front porch                */
3,                           /* v sync                       */
46,                          /* v back porch                 */
1600,                        /* h pixels                     */
32,                          /* h front porch                */
192,                         /* h sync                       */
336,                         /* h back porch                 */
/* 110.0 MHz AV9110 clock serial load information         */
0x5F,                           /* 0:6  VCO frequency divider  N         */
0x3E,                           /* 7:13 Reference frequency divide  M   */
0x1,                            /* 14 VCO pre-scale divide V (0=div.by 1,1=by 8)
*/
0x1,                            /* 15:16 CLK/X output divide X          */
0x1,                            /* 17:18 VCO output divide R            */
1,                              /* 19 CLK Output enable. */
1,                              /* 20 CLK/X Output enable */
0,                              /* reserved, should be set to 0         */
0,                              /* Reference clock select on CLK 1=ref  */
1,                              /* reserved, should be set to 1         */
/* 110.0 MHz IBM561 PLL setup data  */
0xE1,                           /* VCO Div: PFR=0x3, M-65=0x21 */
0x8                             /* REF: N=0x8 */
},
{
/*  Option F Monitor Info  (24-plane)                        */
1200,                        /* rows                         */
1600,                        /* columns                      */
202.5,                       /* 130.8 Mhz                    */
75,                          /* refresh rate                 */
1200,                        /* v scanlines                  */
1,                           /* v front porch                */
3,                           /* v sync                       */
46,                          /* v back porch                 */
1600,                        /* h pixels                     */
32,                          /* h front porch                */
192,                         /* h sync                       */
336,                         /* h back porch                 */
/* AV9110 clock serial load information    130.808     */
0x60,                           /* 0:6  VCO frequency divider  N         */
0x32,                           /* 7:13 Reference frequency divide  M   */
0x1,                            /* 14 VCO pre-scale divide V (0=div.by 1,1=by 8) */
0x1,                            /* 15:16 CLK/X output divide X          */
0x2,                            /* 17:18 VCO output divide R            */
1,                              /* 19 CLK Output enable. */
1,                              /* 20 CLK/X Output enable */
0,                              /* reserved, should be set to 0         */
0,                              /* Reference clock select on CLK 1=ref  */
1,                              /* reserved, should be set to 1         */
/* 110.0 MHz IBM561 PLL setup data  */
0xE2,                           /* bogus VCO Div: PFR=0x2, M=0x7B */
0x7                             /* bogus REF: N=0x8 */
}
#endif /* ALLOW_GT_1280x1024 */
#endif /* ALLOW_GT_72_HZ */
};

int tga_crystal_table_entries = sizeof(tga_crystal_table)/sizeof(struct monitor_data);

struct monitor_data *tga_c_table;

/* ICS av9110 is only used on TGA2 */

void
write_av9110(ScrnInfoPtr pScrn, unsigned int *temp)
{
    TGAPtr pTga = TGAPTR(pScrn);

    /* the following is based on write_av9110() from the
       TRU64 kernel TGA driver */

    TGA2_WRITE_CLOCK_REG(0x0, 0xf800);
    TGA2_WRITE_CLOCK_REG(0x0, 0xf000);

    TGA2_WRITE_CLOCK_REG(temp[0], 0x0000);
    TGA2_WRITE_CLOCK_REG(temp[1], 0x0000);
    TGA2_WRITE_CLOCK_REG(temp[2], 0x0000);
    TGA2_WRITE_CLOCK_REG(temp[3], 0x0000);
    TGA2_WRITE_CLOCK_REG(temp[4], 0x0000);
    TGA2_WRITE_CLOCK_REG(temp[5], 0x0000);

    TGA2_WRITE_CLOCK_REG(0x0, 0xf800);
}

void TGA2SetupMode(ScrnInfoPtr pScrn)
{
  int i;

  /*
   * HACK HACK HACK
   *
   * We do not know how to generate arbitrary clocks, so we search
   * the crystal_table above for a match. Sigh...
   */
  tga_c_table = tga_crystal_table;
  for (i = 0; i < tga_crystal_table_entries; i++, tga_c_table++) {
    if ((tga_c_table->max_rows == pScrn->currentMode->VDisplay) &&
	(tga_c_table->max_cols == pScrn->currentMode->HDisplay)) {
      ErrorF("Found a matching mode (%d)!\n", i);
      break;
    }
  }
  if (i == tga_crystal_table_entries) {
#ifdef FOR_NOW
    FatalError("Unable to find a workable mode");
#else
    ErrorF("Unable to find a matching mode!\n");
    /* tga_c_table = &tga_crystal_table[4]; *//* 640x480 @ 72 */
    tga_c_table = &tga_crystal_table[2]; /* 1024x768 @ 72 */
#endif
  }
  return;
}

static void
ICS9110ClockSelect(ScrnInfoPtr pScrn, int freq)
{
    unsigned int temp, temp1[6];

    /* There lies an ICS9110 Clock Generator. */
    /* ICS9110_CalcClockBits(freq, pll_bits); */

    /* the following is based on munge_ics() from the
       TRU64 kernel TGA driver */

    temp = (unsigned int)(tga_c_table->vco_div |
			  (tga_c_table->ref_div << 7) |
			  (tga_c_table->vco_pre << 14) |
			  (tga_c_table->clk_div << 15) |
			  (tga_c_table->vco_out_div << 17) |
			  (tga_c_table->clk_out_en << 19) |
			  (tga_c_table->clk_out_enX << 20) |
			  (tga_c_table->res0 << 21) |
			  (tga_c_table->clk_sel << 22) |
			  (tga_c_table->res1 << 23));

    temp1[0] = (temp & 0x00000001)         | ((temp & 0x00000002) << 7) |
      ((temp & 0x00000004) << 14) | ((temp & 0x00000008) << 21);

    temp1[1] = ((temp & 0x00000010) >> 4)  | ((temp & 0x00000020) << 3) |
      ((temp & 0x00000040) << 10) | ((temp & 0x00000080) << 17);

    temp1[2] = ((temp & 0x00000100) >> 8)  | ((temp & 0x00000200) >> 1) |
      ((temp & 0x00000400) << 6)  | ((temp & 0x00000800) << 13);

    temp1[3] = ((temp & 0x00001000) >> 12) | ((temp & 0x00002000) >> 5) |
      ((temp & 0x00004000) << 2)  | ((temp & 0x00008000) << 9);

    temp1[4] = ((temp & 0x00010000) >> 16) | ((temp & 0x00020000) >> 9) |
      ((temp & 0x00040000) >> 2)  | ((temp & 0x00080000) << 5);

    temp1[5] = ((temp & 0x00100000) >> 20) | ((temp & 0x00200000) >> 13) |
      ((temp & 0x00400000) >> 6)  | ((temp & 0x00800000) << 1);

    write_av9110(pScrn, temp1);

}

void
Ibm561Init(TGAPtr pTga)
{
    unsigned char *Ibm561 = pTga->Ibm561modeReg;
    int i, j;
 
/* ?? FIXME FIXME FIXME FIXME */

    /* Command registers */
    Ibm561[0] = 0x40;  Ibm561[1] = 0x08;
    Ibm561[2] = (pTga->SyncOnGreen ? 0x80 : 0x00);
	
    /* Read mask */
    Ibm561[3] = 0xff;  Ibm561[4] = 0xff;  Ibm561[5] = 0xff;  Ibm561[6] = 0x0f;

    /* Blink mask */
    Ibm561[7] = 0x00;  Ibm561[8] = 0x00;  Ibm561[9] = 0x00; Ibm561[10] = 0x00;

    /* Window attributes */
    for (i = 0, j=11; i < 16; i++) {
        Ibm561[j++] = 0x00;  Ibm561[j++] = 0x01;  Ibm561[j++] = 0x80;
    }
}

void
Bt463Init(TGAPtr pTga)
{
    unsigned char *Bt463 = pTga->Bt463modeReg;
    int i, j;

    /* Command registers */
    Bt463[0] = 0x40;  Bt463[1] = 0x08;
    Bt463[2] = (pTga->SyncOnGreen ? 0x80 : 0x00);
	
    /* Read mask */
    Bt463[3] = 0xff;  Bt463[4] = 0xff;  Bt463[5] = 0xff;  Bt463[6] = 0x0f;

    /* Blink mask */
    Bt463[7] = 0x00;  Bt463[8] = 0x00;  Bt463[9] = 0x00; Bt463[10] = 0x00;

    /* Window attributes */
    for (i = 0, j=11; i < 16; i++) {
        Bt463[j++] = 0x00;  Bt463[j++] = 0x01;  Bt463[j++] = 0x80;
    }
}

Bool
DEC21030Init(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    TGAPtr pTga = TGAPTR(pScrn);
    TGARegPtr pReg = &pTga->ModeReg;

    if (pTga->RamDac != NULL) { /* this really means 8-bit and BT485 */
        RamDacHWRecPtr pBT = RAMDACHWPTR(pScrn);
	RamDacRegRecPtr ramdacReg = &pBT->ModeReg;

	ramdacReg->DacRegs[BT_COMMAND_REG_0] = 0xA0 |
	    (!pTga->Dac6Bit ? 0x2 : 0x0) | (pTga->SyncOnGreen ? 0x8 : 0x0);
#if 1
	ramdacReg->DacRegs[BT_COMMAND_REG_2] = 0x20;
#else
	ramdacReg->DacRegs[BT_COMMAND_REG_2] = 0x27; /* ?? was 0x20 */
#endif
	ramdacReg->DacRegs[BT_STATUS_REG] = 0x14;
	(*pTga->RamDac->SetBpp)(pScrn, ramdacReg);

    } else {
	switch (pTga->Chipset) {
	case PCI_CHIP_DEC21030: /* always BT463 */
	    Bt463Init(pTga);
	    break;
	case PCI_CHIP_TGA2:	/* always IBM 561 */
	    Ibm561Init(pTga);
	    break;
	}
    }

    pReg->tgaRegs[0x00] = mode->CrtcHDisplay;
    pReg->tgaRegs[0x01] = mode->CrtcHSyncStart - mode->CrtcHDisplay;
    pReg->tgaRegs[0x02] = (mode->CrtcHSyncEnd - mode->CrtcHSyncStart) / 4;
    pReg->tgaRegs[0x03] = (mode->CrtcHTotal - mode->CrtcHSyncEnd) / 4;
    pReg->tgaRegs[0x04] = mode->CrtcVDisplay;
    pReg->tgaRegs[0x05] = mode->CrtcVSyncStart - mode->CrtcVDisplay;
    pReg->tgaRegs[0x06] = mode->CrtcVSyncEnd - mode->CrtcVSyncStart;
    pReg->tgaRegs[0x07] = mode->CrtcVTotal - mode->CrtcVSyncEnd;

    /*
     * We do polarity the Step B way of the 21030 
     * Tell me how I can detect a Step A, and I'll support that too. 
     * But I think that the Step B's are most common 
     */
    if (mode->Flags & V_PHSYNC)
	pReg->tgaRegs[0x08] = 1; /* Horizontal Polarity */
    else
	pReg->tgaRegs[0x08] = 0;

    if (mode->Flags & V_PVSYNC)
	pReg->tgaRegs[0x09] = 1; /* Vertical Polarity */
    else
	pReg->tgaRegs[0x09] = 0;

    pReg->tgaRegs[0x0A] = mode->Clock;

    pReg->tgaRegs[0x10] = (((pReg->tgaRegs[0x00]) / 4) & 0x1FF) |
                ((((pReg->tgaRegs[0x00]) / 4) & 0x600) << 19) |
		(((pReg->tgaRegs[0x01]) / 4) << 9) |
		(pReg->tgaRegs[0x02] << 14) |
		(pReg->tgaRegs[0x03] << 21) |
#if 0
      (1 << 31) | /* ?? */
#endif
		(pReg->tgaRegs[0x08] << 30);
    pReg->tgaRegs[0x11] = pReg->tgaRegs[0x04] |
		(pReg->tgaRegs[0x05] << 11) | 
		(pReg->tgaRegs[0x06] << 16) |
		(pReg->tgaRegs[0x07] << 22) |
		(pReg->tgaRegs[0x09] << 30);

    pReg->tgaRegs[0x12] = 0x01;

    pReg->tgaRegs[0x13] = 0x0000;
    return TRUE;
}

void
DEC21030Save(ScrnInfoPtr pScrn, TGARegPtr tgaReg)
{
    TGAPtr pTga = TGAPTR(pScrn);

    tgaReg->tgaRegs[0x10] = TGA_READ_REG(TGA_HORIZ_REG);
    tgaReg->tgaRegs[0x11] = TGA_READ_REG(TGA_VERT_REG);
    tgaReg->tgaRegs[0x12] = TGA_READ_REG(TGA_VALID_REG);
    tgaReg->tgaRegs[0x13] = TGA_READ_REG(TGA_BASE_ADDR_REG);
    
    return;
}

void
DEC21030Restore(ScrnInfoPtr pScrn, TGARegPtr tgaReg)
{
    TGAPtr pTga = TGAPTR(pScrn);

    TGA_WRITE_REG(0x00, TGA_VALID_REG); /* Disable Video */

    switch (pTga->Chipset) {
    case PCI_CHIP_DEC21030:
        ICS1562ClockSelect(pScrn, tgaReg->tgaRegs[0x0A]);
	break;
    case PCI_CHIP_TGA2:
        ICS9110ClockSelect(pScrn, tgaReg->tgaRegs[0x0A]);
	break;
    }

    TGA_WRITE_REG(tgaReg->tgaRegs[0x10], TGA_HORIZ_REG);
    TGA_WRITE_REG(tgaReg->tgaRegs[0x11], TGA_VERT_REG);
    TGA_WRITE_REG(tgaReg->tgaRegs[0x13], TGA_BASE_ADDR_REG);

    TGA_WRITE_REG(tgaReg->tgaRegs[0x12], TGA_VALID_REG); /* Re-enable Video */

    return;
}
