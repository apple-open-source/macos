/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_cursor.h,v 1.5 2003/02/06 13:14:04 eich Exp $ */
/*
 * Copyright 1998,1999 by Alan Hourihane, Wigan, England.
 * Parts Copyright 2001, 2002 by Thomas Winischhofer, Vienna, Austria.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Alan Hourihane, alanh@fairlite.demon.co.uk
 *           Mike Chapman <mike@paranoia.com>,
 *           Juanjo Santamarta <santamarta@ctv.es>,
 *           Mitani Hiroshi <hmitani@drl.mei.co.jp>
 *           David Thomas <davtom@dream.org.uk>.
 *	     Thomas Winischhofer <thomas@winischhofer.net>:
 */

#define CS(x)   (0x8500+(x<<2))

/* 300 series, CRT1 */

/* 80000000 = RGB(1) - MONO(0)
 * 40000000 = enable(1) - disable(0)
 * 20000000 = 32(1) / 16(1) bit RGB
 * 10000000 = "ghost"(1) - [other effect](0)
 */

#define sis300GetCursorStatus \
  MMIO_IN32(pSiS->IOBase, CS(0)) & 0x40000000;
  
#define sis300SetCursorStatus(status) \
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(0)); \
  temp &= 0xbfffffff; \
  temp |= status; \
  MMIO_OUT32(pSiS->IOBase, CS(0), temp); \
  }

#define sis300EnableHWCursor() \
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(0)); \
  temp &= 0x0fffffff; \
  temp |= 0x40000000; \
  MMIO_OUT32(pSiS->IOBase, CS(0), temp); \
  }
  
#define sis300EnableHWARGBCursor() \
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(0)); \
  temp |= 0xF0000000; \
  MMIO_OUT32(pSiS->IOBase, CS(0), temp); \
  }

#define sis300EnableHWARGB16Cursor() \
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(0)); \
  temp &= 0x0fffffff; \
  temp |= 0xD0000000; \
  MMIO_OUT32(pSiS->IOBase, CS(0), temp); \
  }

#define sis300SwitchToMONOCursor() \
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(0)); \
  temp &= 0x4fffffff; \
  MMIO_OUT32(pSiS->IOBase, CS(0), temp); \
  }
  
#define sis300SwitchToRGBCursor() \
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(0)); \
  temp |= 0xB0000000; \
  MMIO_OUT32(pSiS->IOBase, CS(0), temp); \
  }
  
#define sis300DisableHWCursor()\
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(0)); \
  temp &= 0xbFFFFFFF; \
  MMIO_OUT32(pSiS->IOBase, CS(0), temp); \
  }

#define sis300SetCursorBGColor(color)\
  MMIO_OUT32(pSiS->IOBase, CS(1), (color));
#define sis300SetCursorFGColor(color)\
  MMIO_OUT32(pSiS->IOBase, CS(2), (color));

#define sis300SetCursorPositionX(x,preset)\
  MMIO_OUT32(pSiS->IOBase, CS(3), ((x) | ((preset) << 16)));
#define sis300SetCursorPositionY(y,preset)\
  MMIO_OUT32(pSiS->IOBase, CS(4), ((y) | ((preset) << 16)));

#define sis300SetCursorAddress(address)\
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(0)); \
  temp &= 0xFFFF0000; \
  temp |= address; \
  MMIO_OUT32(pSiS->IOBase,CS(0),temp); \
  }

#define sis300SetCursorPatternSelect(pat_id)\
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(0)); \
  temp &= 0xF0FFFFFF; \
  temp |= (pat_id) << 24; \
  MMIO_OUT32(pSiS->IOBase,CS(0),temp); \
  }

/* 300 series, CRT2 */

/* 80000000 = RGB(1) - MONO(0)
 * 40000000 = enable(1) - disable(0)
 * 20000000 = 32(1) / 16(1) bit RGB
 * 10000000 = unused (always "ghosting")
 */

#define sis301GetCursorStatus \
  MMIO_IN32(pSiS->IOBase, CS(8)) & 0x40000000;
  
#define sis301SetCursorStatus(status) \
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(8)); \
  temp &= 0xbfffffff; \
  temp |= status; \
  MMIO_OUT32(pSiS->IOBase, CS(8), temp); \
  }
  
#define sis301EnableHWCursor()\
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(8)); \
  temp &= 0x0fffffff; \
  temp |= 0x40000000; \
  MMIO_OUT32(pSiS->IOBase, CS(8), temp); \
  }
  
#define sis301EnableHWARGBCursor()\
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(8)); \
  temp |= 0xF0000000; \
  MMIO_OUT32(pSiS->IOBase, CS(8), temp); \
  }
  
#define sis301EnableHWARGB16Cursor()\
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(8)); \
  temp &= 0x0FFFFFFF; \
  temp |= 0xD0000000; \
  MMIO_OUT32(pSiS->IOBase, CS(8), temp); \
  }
  
#define sis301SwitchToRGBCursor() \
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(8)); \
  temp |= 0xB0000000; \
  MMIO_OUT32(pSiS->IOBase, CS(8), temp); \
  }
  
#define sis301SwitchToMONOCursor() \
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(8)); \
  temp &= 0x4fffffff; \
  MMIO_OUT32(pSiS->IOBase, CS(8), temp); \
  }
 
#define sis301DisableHWCursor()\
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(8)); \
  temp &= 0xbFFFFFFF; \
  MMIO_OUT32(pSiS->IOBase, CS(8), temp); \
  }
  
#define sis301SetCursorBGColor(color)\
  MMIO_OUT32(pSiS->IOBase, CS(9), (color));
#define sis301SetCursorFGColor(color)\
  MMIO_OUT32(pSiS->IOBase, CS(10), (color));

#define sis301SetCursorPositionX(x,preset)\
  MMIO_OUT32(pSiS->IOBase, CS(11), ((x) | ((preset) << 16)));
#define sis301SetCursorPositionY(y,preset)\
  MMIO_OUT32(pSiS->IOBase, CS(12), ((y) | ((preset) << 16)));

#define sis301SetCursorAddress(address)\
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(8)); \
  temp &= 0xFFFF0000; \
  temp |= address; \
  MMIO_OUT32(pSiS->IOBase,CS(8),temp); \
  }

#define sis301SetCursorPatternSelect(pat_id)\
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(8)); \
  temp &= 0xF0FFFFFF; \
  temp |= (pat_id) << 24; \
  MMIO_OUT32(pSiS->IOBase,CS(8),temp); \
  }

/* 310/325/330 series CRT1 */

/* 80000000 = RGB(1) - MONO(0)
 * 40000000 = enable(1) - disable(0)
 * 20000000 = 32(1) / 16(1) bit RGB
 * 10000000 = "ghost"(1) - Alpha Blend(0)
 */
 
#define sis310GetCursorStatus \
  MMIO_IN32(pSiS->IOBase, CS(0)) & 0x40000000;
  
#define sis310SetCursorStatus(status) \
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(0)); \
  temp &= 0xbfffffff; \
  temp |= status; \
  MMIO_OUT32(pSiS->IOBase, CS(0), temp); \
  }
  
#define sis310EnableHWCursor()\
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(0)); \
  temp &= 0x0fffffff; \
  temp |= 0x40000000; \
  MMIO_OUT32(pSiS->IOBase, CS(0), temp); \
  }
  
#define sis310EnableHWARGBCursor()\
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(0)); \
  temp &= 0x0FFFFFFF; \
  temp |= 0xE0000000; \
  MMIO_OUT32(pSiS->IOBase, CS(0), temp); \
  }

#define sis310SwitchToMONOCursor() \
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(0)); \
  temp &= 0x4fffffff; \
  MMIO_OUT32(pSiS->IOBase, CS(0), temp); \
  }   
  
#define sis310SwitchToRGBCursor() \
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(0)); \
  temp &= 0xBFFFFFFF; \
  temp |= 0xA0000000; \
  MMIO_OUT32(pSiS->IOBase, CS(0), temp); \
  }
  
#define sis310DisableHWCursor()\
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(0)); \
  temp &= 0xBFFFFFFF; \
  MMIO_OUT32(pSiS->IOBase, CS(0), temp); \
  }
  
#define sis310SetCursorBGColor(color)\
  MMIO_OUT32(pSiS->IOBase, CS(1), (color));
#define sis310SetCursorFGColor(color)\
  MMIO_OUT32(pSiS->IOBase, CS(2), (color));

#define sis310SetCursorPositionX(x,preset)\
  MMIO_OUT32(pSiS->IOBase, CS(3), ((x) | ((preset) << 16)));
#define sis310SetCursorPositionY(y,preset)\
  MMIO_OUT32(pSiS->IOBase, CS(4), ((y) | ((preset) << 16)));

#define sis310SetCursorAddress(address)\
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(0)); \
  temp &= 0xFFF00000; \
  temp |= address; \
  MMIO_OUT32(pSiS->IOBase,CS(0),temp); \
  }

#define sis310SetCursorPatternSelect(pat_id)\
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(0)); \
  temp &= 0xF0FFFFFF; \
  temp |= (pat_id) << 24; \
  MMIO_OUT32(pSiS->IOBase,CS(0),temp); \
  }

/* 310/325/330 series CRT2 */

/* 80000000 = RGB(1) - MONO(0)
 * 40000000 = enable(1) - disable(0)
 * 20000000 = 32(1) / 16(1) bit RGB
 * 10000000 = "ghost"(1) - Alpha Blend(0)  ?
 */
 
#define sis301GetCursorStatus310 \
  MMIO_IN32(pSiS->IOBase, CS(8)) & 0x40000000;
  
#define sis301SetCursorStatus310(status) \
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(8)); \
  temp &= 0xbfffffff; \
  temp |= status; \
  MMIO_OUT32(pSiS->IOBase, CS(8), temp); \
  }
  
#define sis301EnableHWCursor310()\
   { \
   unsigned long temp, temp1, temp2; \
   temp1 = MMIO_IN32(pSiS->IOBase, CS(11)); \
   temp2 = MMIO_IN32(pSiS->IOBase, CS(12)); \
   temp = MMIO_IN32(pSiS->IOBase, CS(8)); \
   temp &= 0x0fffffff; \
   temp |= 0x40000000; \
   MMIO_OUT32(pSiS->IOBase, CS(8), temp); \
   MMIO_OUT32(pSiS->IOBase, CS(11), temp1); \
   MMIO_OUT32(pSiS->IOBase, CS(12), temp2); \
   }

#define sis301EnableHWARGBCursor310()\
   { \
   unsigned long temp, temp1, temp2; \
   temp1 = MMIO_IN32(pSiS->IOBase, CS(11)); \
   temp2 = MMIO_IN32(pSiS->IOBase, CS(12)); \
   temp = MMIO_IN32(pSiS->IOBase, CS(8)); \
   temp &= 0x0FFFFFFF; \
   temp |= 0xE0000000; \
   MMIO_OUT32(pSiS->IOBase, CS(8), temp); \
   MMIO_OUT32(pSiS->IOBase, CS(11), temp1); \
   MMIO_OUT32(pSiS->IOBase, CS(12), temp2); \
   }

#define sis301SwitchToRGBCursor310() \
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(8)); \
  temp &= 0xBFFFFFFF; \
  temp |= 0xA0000000; \
  MMIO_OUT32(pSiS->IOBase, CS(8), temp); \
  }
  
#define sis301SwitchToMONOCursor310() \
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(8)); \
  temp &= 0x4fffffff; \
  MMIO_OUT32(pSiS->IOBase, CS(8), temp); \
  }
  
#define sis301DisableHWCursor310()\
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(8)); \
  temp &= 0xBFFFFFFF; \
  MMIO_OUT32(pSiS->IOBase, CS(8), temp); \
  }

#define sis301SetCursorBGColor310(color)\
  MMIO_OUT32(pSiS->IOBase, CS(9), (color));
#define sis301SetCursorFGColor310(color)\
  MMIO_OUT32(pSiS->IOBase, CS(10), (color));

#define sis301SetCursorPositionX310(x,preset)\
  MMIO_OUT32(pSiS->IOBase, CS(11), ((x) | ((preset) << 16)));
#define sis301SetCursorPositionY310(y,preset)\
  MMIO_OUT32(pSiS->IOBase, CS(12), ((y) | ((preset) << 16)));
  
#define sis301SetCursorAddress310(address)\
  { \
  unsigned long temp; \
  if(pSiS->sishw_ext.jChipType == SIS_315H) { \
     if(address & 0x10000) { \
        address &= ~0x10000; \
	orSISIDXREG(SISSR, 0x37, 0x80); \
     } else { \
        andSISIDXREG(SISSR, 0x37, 0x7f); \
     } \
  } \
  temp = MMIO_IN32(pSiS->IOBase, CS(8)); \
  temp &= 0xFFF00000; \
  temp |= address; \
  MMIO_OUT32(pSiS->IOBase,CS(8),temp); \
  }

#define sis301SetCursorPatternSelect310(pat_id)\
  { \
  unsigned long temp; \
  temp = MMIO_IN32(pSiS->IOBase, CS(8)); \
  temp &= 0xF0FFFFFF; \
  temp |= (pat_id) << 24; \
  MMIO_OUT32(pSiS->IOBase,CS(8),temp); \
  }

