
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i810_wmark.c,v 1.8 2002/10/30 12:52:18 alanh Exp $ */

/*
 * Reformatted with GNU indent (2.2.8), using the following options:
 *
 *    -bad -bap -c41 -cd0 -ncdb -ci6 -cli0 -cp0 -ncs -d0 -di3 -i3 -ip3 -l78
 *    -lp -npcs -psl -sob -ss -br -ce -sc -hnl
 *
 * This provides a good match with the original i810 code and preferred
 * XFree86 formatting conventions.
 *
 * When editing this driver, please follow the existing formatting, and edit
 * with <TAB> characters expanded at 8-column intervals.
 */

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 */

#include "xf86.h"
#include "i810.h"

struct wm_info {
   double freq;
   unsigned int wm;
};

struct wm_info i810_wm_8_100[] = {
   {0, 0x22003000},
   {25.2, 0x22003000},
   {28.0, 0x22003000},
   {31.5, 0x22003000},
   {36.0, 0x22007000},
   {40.0, 0x22007000},
   {45.0, 0x22007000},
   {49.5, 0x22008000},
   {50.0, 0x22008000},
   {56.3, 0x22008000},
   {65.0, 0x22008000},
   {75.0, 0x22008000},
   {78.8, 0x22008000},
   {80.0, 0x22008000},
   {94.0, 0x22008000},
   {96.0, 0x22107000},
   {99.0, 0x22107000},
   {108.0, 0x22107000},
   {121.0, 0x22107000},
   {128.9, 0x22107000},
   {132.0, 0x22109000},
   {135.0, 0x22109000},
   {157.5, 0x2210b000},
   {162.0, 0x2210b000},
   {175.5, 0x2210b000},
   {189.0, 0x2220e000},
   {202.5, 0x2220e000}
};

struct wm_info i810_wm_16_100[] = {
   {0, 0x22004000},
   {25.2, 0x22006000},
   {28.0, 0x22006000},
   {31.5, 0x22007000},
   {36.0, 0x22007000},
   {40.0, 0x22007000},
   {45.0, 0x22007000},
   {49.5, 0x22009000},
   {50.0, 0x22009000},
   {56.3, 0x22108000},
   {65.0, 0x2210e000},
   {75.0, 0x2210e000},
   {78.8, 0x2210e000},
   {80.0, 0x22210000},
   {94.5, 0x22210000},
   {96.0, 0x22210000},
   {99.0, 0x22210000},
   {108.0, 0x22210000},
   {121.0, 0x22210000},
   {128.9, 0x22210000},
   {132.0, 0x22314000},
   {135.0, 0x22314000},
   {157.5, 0x22415000},
   {162.0, 0x22416000},
   {175.5, 0x22416000},
   {189.0, 0x22416000},
   {195.0, 0x22416000},
   {202.5, 0x22416000}
};

struct wm_info i810_wm_24_100[] = {
   {0, 0x22006000},
   {25.2, 0x22009000},
   {28.0, 0x22009000},
   {31.5, 0x2200a000},
   {36.0, 0x2210c000},
   {40.0, 0x2210c000},
   {45.0, 0x2210c000},
   {49.5, 0x22111000},
   {50.0, 0x22111000},
   {56.3, 0x22111000},
   {65.0, 0x22214000},
   {75.0, 0x22214000},
   {78.8, 0x22215000},
   {80.0, 0x22216000},
   {94.5, 0x22218000},
   {96.0, 0x22418000},
   {99.0, 0x22418000},
   {108.0, 0x22418000},
   {121.0, 0x22418000},
   {128.9, 0x22419000},
   {132.0, 0x22519000},
   {135.0, 0x4441d000},
   {157.5, 0x44419000},
   {162.0, 0x44419000},
   {175.5, 0x44419000},
   {189.0, 0x44419000},
   {195.0, 0x44419000},
   {202.5, 0x44419000}
};

struct wm_info i810_wm_32_100[] = {
   {0, 0x2210b000},
   {60, 0x22415000},			/* 0x314000 works too */
   {80, 0x22419000}			/* 0x518000 works too */
};

struct wm_info i810_wm_8_133[] = {
   {0, 0x22003000},
   {25.2, 0x22003000},
   {28.0, 0x22003000},
   {31.5, 0x22003000},
   {36.0, 0x22007000},
   {40.0, 0x22007000},
   {45.0, 0x22007000},
   {49.5, 0x22008000},
   {50.0, 0x22008000},
   {56.3, 0x22008000},
   {65.0, 0x22008000},
   {75.0, 0x22008000},
   {78.8, 0x22008000},
   {80.0, 0x22008000},
   {94.0, 0x22008000},
   {96.0, 0x22107000},
   {99.0, 0x22107000},
   {108.0, 0x22107000},
   {121.0, 0x22107000},
   {128.9, 0x22107000},
   {132.0, 0x22109000},
   {135.0, 0x22109000},
   {157.5, 0x2210b000},
   {162.0, 0x2210b000},
   {175.5, 0x2210b000},
   {189.0, 0x2220e000},
   {202.5, 0x2220e000}
};

struct wm_info i810_wm_16_133[] = {
   {0, 0x22004000},
   {25.2, 0x22006000},
   {28.0, 0x22006000},
   {31.5, 0x22007000},
   {36.0, 0x22007000},
   {40.0, 0x22007000},
   {45.0, 0x22007000},
   {49.5, 0x22009000},
   {50.0, 0x22009000},
   {56.3, 0x22108000},
   {65.0, 0x2210e000},
   {75.0, 0x2210e000},
   {78.8, 0x2210e000},
   {80.0, 0x22210000},
   {94.5, 0x22210000},
   {96.0, 0x22210000},
   {99.0, 0x22210000},
   {108.0, 0x22210000},
   {121.0, 0x22210000},
   {128.9, 0x22210000},
   {132.0, 0x22314000},
   {135.0, 0x22314000},
   {157.5, 0x22415000},
   {162.0, 0x22416000},
   {175.5, 0x22416000},
   {189.0, 0x22416000},
   {195.0, 0x22416000},
   {202.5, 0x22416000}
};

struct wm_info i810_wm_24_133[] = {
   {0, 0x22006000},
   {25.2, 0x22009000},
   {28.0, 0x22009000},
   {31.5, 0x2200a000},
   {36.0, 0x2210c000},
   {40.0, 0x2210c000},
   {45.0, 0x2210c000},
   {49.5, 0x22111000},
   {50.0, 0x22111000},
   {56.3, 0x22111000},
   {65.0, 0x22214000},
   {75.0, 0x22214000},
   {78.8, 0x22215000},
   {80.0, 0x22216000},
   {94.5, 0x22218000},
   {96.0, 0x22418000},
   {99.0, 0x22418000},
   {108.0, 0x22418000},
   {121.0, 0x22418000},
   {128.9, 0x22419000},
   {132.0, 0x22519000},
   {135.0, 0x4441d000},
   {157.5, 0x44419000},
   {162.0, 0x44419000},
   {175.5, 0x44419000},
   {189.0, 0x44419000},
   {195.0, 0x44419000},
   {202.5, 0x44419000}
};

#define Elements(x) (sizeof(x)/sizeof(*x))

/*
 * I810CalcFIFO --
 *
 * Calculate burst length and FIFO watermark.
 */

unsigned int
I810CalcWatermark(ScrnInfoPtr pScrn, double freq, Bool dcache)
{
   I810Ptr pI810 = I810PTR(pScrn);
   struct wm_info *tab;
   int nr;
   int i;

   if (pI810->LmFreqSel == 100) {
      switch (pScrn->bitsPerPixel) {
      case 8:
	 tab = i810_wm_8_100;
	 nr = Elements(i810_wm_8_100);
	 break;
      case 16:
	 tab = i810_wm_16_100;
	 nr = Elements(i810_wm_16_100);
	 break;
      case 24:
	 tab = i810_wm_24_100;
	 nr = Elements(i810_wm_24_100);
	 break;
      default:
	 return 0;
      }
   } else {
      switch (pScrn->bitsPerPixel) {
      case 8:
	 tab = i810_wm_8_133;
	 nr = Elements(i810_wm_8_133);
	 break;
      case 16:
	 tab = i810_wm_16_133;
	 nr = Elements(i810_wm_16_133);
	 break;
      case 24:
	 tab = i810_wm_24_133;
	 nr = Elements(i810_wm_24_133);
	 break;
      default:
	 return 0;
      }
   }

   for (i = 0; i < nr && tab[i].freq < freq; i++) ;

   if (i == nr)
      i--;

   xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
		  "chose watermark 0x%x: (tab.freq %.1f)\n",
		  tab[i].wm, tab[i].freq);

   /* None of these values (sourced from intel) have watermarks for
    * the dcache memory.  Fake it for now by using the same watermark
    * for both...  
    *
    * Update: this is probably because dcache isn't real useful as
    * framebuffer memory, so intel's drivers don't need watermarks
    * for that memory because they never use it to feed the ramdacs.
    * We do use it in the fallback mode, so keep the watermarks for
    * now.
    */
   if (dcache)
      return (tab[i].wm & ~0xffffff) | ((tab[i].wm >> 12) & 0xfff);
   else
      return tab[i].wm;
}
