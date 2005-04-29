
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
Copyright © 2002 David Dawes

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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/common.h,v 1.10 2004/01/03 02:11:53 dawes Exp $ */

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 *   David Dawes <dawes@xfree86.org>
 *
 */

#ifndef _INTEL_COMMON_H_
#define _INTEL_COMMON_H_

#ifdef __GNUC__
#define PFX __FILE__,__LINE__,__FUNCTION__
#define FUNCTION_NAME __FUNCTION__
#else
#define PFX __FILE__,__LINE__,""
#define FUNCTION_NAME ""
#endif

#ifdef I830DEBUG
#define MARKER() ErrorF("\n### %s:%d: >>> %s <<< ###\n\n", \
			 __FILE__, __LINE__,__FUNCTION__)
#define DPRINTF I830DPRINTF_stub
#else /* #ifdef I830DEBUG */
#define MARKER()
/* this is a real ugly hack to get the compiler to optimize the debugging statements into oblivion */
#define DPRINTF if(0) I830DPRINTF_stub
#endif /* #ifdef I830DEBUG */

#define KB(x) ((x) * 1024)
#define MB(x) ((x) * KB(1024))

/* Using usleep() makes things noticably slow. */
#if 0
#define DELAY(x) usleep(x)
#else
#define DELAY(x) do {;} while (0)
#endif

/* I830 hooks for the I810 driver setup/probe. */
extern const OptionInfoRec *I830BIOSAvailableOptions(int chipid, int busid);
extern void I830InitpScrn(ScrnInfoPtr pScrn);

/* Symbol lists shared by the i810 and i830 parts. */
extern const char *I810vgahwSymbols[];
extern const char *I810ramdacSymbols[];
extern const char *I810int10Symbols[];
extern const char *I810vbeSymbols[];
extern const char *I810ddcSymbols[];
extern const char *I810fbSymbols[];
extern const char *I810xaaSymbols[];
#ifdef XF86DRI
extern const char *I810driSymbols[];
extern const char *I810drmSymbols[];
extern const char *I810shadowSymbols[];
#endif

extern void I830DPRINTF_stub(const char *filename, int line,
			     const char *function, const char *fmt, ...);

#ifdef _I830_H_
#define PrintErrorState I830PrintErrorState
#define WaitRingFunc I830WaitLpRing
#define RecPtr pI830
#else
#define PrintErrorState I810PrintErrorState
#define WaitRingFunc I810WaitLpRing
#define RecPtr pI810
#endif

/* BIOS debug macro */
#define xf86ExecX86int10_wrapper(pInt, pScrn) do {			\
   if (I810_DEBUG & DEBUG_VERBOSE_BIOS) {				\
      ErrorF("\n\n\n\nExecuting (ax == 0x%x) BIOS call\n", pInt->ax);	\
      ErrorF("Checking Error state before execution\n");		\
      PrintErrorState(pScrn);						\
   }									\
   xf86ExecX86int10(pInt);						\
   if(I810_DEBUG & DEBUG_VERBOSE_BIOS) {				\
      ErrorF("Checking Error state after execution\n");			\
      usleep(50000);							\
      PrintErrorState(pScrn);						\
   }									\
} while (0)

#define OUT_RING(n) do {						\
   if (I810_DEBUG & DEBUG_VERBOSE_RING)					\
      ErrorF( "OUT_RING %lx: %x, (mask %x)\n",				\
		(unsigned long)(outring), (unsigned int)(n), ringmask);	\
   *(volatile unsigned int *)(virt + outring) = n;			\
   outring += 4;							\
   outring &= ringmask;							\
} while (0)

#if 1
#define ADVANCE_LP_RING() do {						\
   RecPtr->LpRing.tail = outring;					\
   if (outring & 0x07)							\
      ErrorF("ADVANCE_LP_RING: "					\
	     "outring (0x%x) isn't on a QWord boundary", outring);	\
   OUTREG(LP_RING + RING_TAIL, outring);				\
} while (0)
#else
#define ADVANCE_LP_RING() {						\
   RecPtr->LpRing.tail = outring;					\
   if (outring & 0x07)							\
      ErrorF("ADVANCE_LP_RING: "					\
	     "outring (0x%x) isn't on a QWord boundary", outring);	\
   ErrorF("head is %d, tail is %d [%d]\n", INREG(LP_RING + RING_HEAD), INREG(LP_RING + RING_TAIL), outring); \
   OUTREG(LP_RING + RING_TAIL, outring);				\
   ErrorF("head is %d, tail is %d [%d]\n", INREG(LP_RING + RING_HEAD), INREG(LP_RING + RING_TAIL), outring); \
}
#endif

/*
 * XXX Note: the head/tail masks are different for 810 and i830.
 * If the i810 always sets the higher bits to 0, then this shouldn't be
 * a problem.  Check this!
 */
#define DO_RING_IDLE() do {						\
   int _head;								\
   int _tail;								\
   do {									\
      _head = INREG(LP_RING + RING_HEAD) & I830_HEAD_MASK;		\
      _tail = INREG(LP_RING + RING_TAIL) & I830_TAIL_MASK;		\
      DELAY(10);							\
   } while (_head != _tail);						\
} while( 0)

/*
 * This is for debugging a potential problem writing the tail pointer
 * close to the end of the ring buffer.
 */
#ifndef AVOID_TAIL_END
#define AVOID_TAIL_END 0
#endif
#ifndef AVOID_SIZE
#define AVOID_SIZE 64
#endif

#if AVOID_TAIL_END

#define BEGIN_LP_RING(n)						\
   unsigned int outring, ringmask;					\
   volatile unsigned char *virt;					\
   unsigned int needed;							\
   if ((n) & 1)								\
      ErrorF("BEGIN_LP_RING called with odd argument: %d\n", n);	\
   if ((n) > 2 && (I810_DEBUG&DEBUG_ALWAYS_SYNC))			\
      DO_RING_IDLE();							\
   needed = (n) * 4;							\
   if ((RecPtr->LpRing.tail > RecPtr->LpRing.tail_mask - AVOID_SIZE) ||	\
       (RecPtr->LpRing.tail + needed) >					\
	RecPtr->LpRing.tail_mask - AVOID_SIZE) {			\
      needed += RecPtr->LpRing.tail_mask + 1 - RecPtr->LpRing.tail;	\
      ErrorF("BEGIN_LP_RING: skipping last 64 bytes of "		\
	     "ring (%d vs %d)\n", needed, (n) * 4);			\
   }									\
   if (RecPtr->LpRing.space < needed)					\
      WaitRingFunc(pScrn, needed, 0);					\
   RecPtr->LpRing.space -= needed;					\
   outring = RecPtr->LpRing.tail;					\
   ringmask = RecPtr->LpRing.tail_mask;					\
   virt = RecPtr->LpRing.virtual_start;					\
   while (needed > (n) * 4) {						\
      ErrorF("BEGIN_LP_RING: putting MI_NOOP at 0x%x (remaining %d)\n",	\
	     outring, needed - (n) * 4);				\
      OUT_RING(MI_NOOP);						\
      needed -= 4;							\
   }									\
   if (I810_DEBUG & DEBUG_VERBOSE_RING)					\
      ErrorF( "BEGIN_LP_RING %d in %s\n", n, FUNCTION_NAME);

#else /* AVOID_TAIL_END */

#define BEGIN_LP_RING(n)						\
   unsigned int outring, ringmask;					\
   volatile unsigned char *virt;					\
   unsigned int needed;							\
   if ((n) & 1)								\
      ErrorF("BEGIN_LP_RING called with odd argument: %d\n", n);	\
   if ((n) > 2 && (I810_DEBUG&DEBUG_ALWAYS_SYNC))			\
      DO_RING_IDLE();							\
   needed = (n) * 4;							\
   if (RecPtr->LpRing.space < needed)					\
      WaitRingFunc(pScrn, needed, 0);					\
   RecPtr->LpRing.space -= needed;					\
   outring = RecPtr->LpRing.tail;					\
   ringmask = RecPtr->LpRing.tail_mask;					\
   virt = RecPtr->LpRing.virtual_start;					\
   if (I810_DEBUG & DEBUG_VERBOSE_RING)					\
      ErrorF( "BEGIN_LP_RING %d in %s\n", n, FUNCTION_NAME);

#endif /* AVOID_TAIL_END */


/* Memory mapped register access macros */
#define INREG8(addr)        *(volatile CARD8  *)(RecPtr->MMIOBase + (addr))
#define INREG16(addr)       *(volatile CARD16 *)(RecPtr->MMIOBase + (addr))
#define INREG(addr)         *(volatile CARD32 *)(RecPtr->MMIOBase + (addr))

#define OUTREG8(addr, val) do {						\
   *(volatile CARD8 *)(RecPtr->MMIOBase  + (addr)) = (val);		\
   if (I810_DEBUG&DEBUG_VERBOSE_OUTREG) {				\
      ErrorF("OUTREG8(0x%lx, 0x%lx) in %s\n", (unsigned long)(addr),	\
		(unsigned long)(val), FUNCTION_NAME);			\
   }									\
} while (0)

#define OUTREG16(addr, val) do {					\
   *(volatile CARD16 *)(RecPtr->MMIOBase + (addr)) = (val);		\
   if (I810_DEBUG&DEBUG_VERBOSE_OUTREG) {				\
      ErrorF("OUTREG16(0x%lx, 0x%lx) in %s\n", (unsigned long)(addr),	\
		(unsigned long)(val), FUNCTION_NAME);			\
   }									\
} while (0)

#define OUTREG(addr, val) do {						\
   *(volatile CARD32 *)(RecPtr->MMIOBase + (addr)) = (val);		\
   if (I810_DEBUG&DEBUG_VERBOSE_OUTREG) {				\
      ErrorF("OUTREG(0x%lx, 0x%lx) in %s\n", (unsigned long)(addr),	\
		(unsigned long)(val), FUNCTION_NAME);			\
   }									\
} while (0)

/* To remove all debugging, make sure I810_DEBUG is defined as a
 * preprocessor symbol, and equal to zero.
 */
#if 1
#ifndef I810_DEBUG
#define I810_DEBUG 0
#endif
#endif
#ifndef I810_DEBUG
#warning "Debugging enabled - expect reduced performance"
extern int I810_DEBUG;
#endif

#define DEBUG_VERBOSE_ACCEL  0x1
#define DEBUG_VERBOSE_SYNC   0x2
#define DEBUG_VERBOSE_VGA    0x4
#define DEBUG_VERBOSE_RING   0x8
#define DEBUG_VERBOSE_OUTREG 0x10
#define DEBUG_VERBOSE_MEMORY 0x20
#define DEBUG_VERBOSE_CURSOR 0x40
#define DEBUG_ALWAYS_SYNC    0x80
#define DEBUG_VERBOSE_DRI    0x100
#define DEBUG_VERBOSE_BIOS   0x200

/* Size of the mmio region.
 */
#define I810_REG_SIZE 0x80000

#ifndef PCI_CHIP_I810
#define PCI_CHIP_I810              0x7121
#define PCI_CHIP_I810_DC100        0x7123
#define PCI_CHIP_I810_E            0x7125
#define PCI_CHIP_I815              0x1132
#define PCI_CHIP_I810_BRIDGE       0x7120
#define PCI_CHIP_I810_DC100_BRIDGE 0x7122
#define PCI_CHIP_I810_E_BRIDGE     0x7124
#define PCI_CHIP_I815_BRIDGE       0x1130
#endif

#ifndef PCI_CHIP_I855_GM
#define PCI_CHIP_I855_GM	   0x3582
#define PCI_CHIP_I855_GM_BRIDGE	   0x3580
#endif

#ifndef PCI_CHIP_I865_G
#define PCI_CHIP_I865_G		   0x2572
#define PCI_CHIP_I865_G_BRIDGE	   0x2570
#endif

#define IS_I810(pI810) (pI810->PciInfo->chipType == PCI_CHIP_I810 ||	\
			pI810->PciInfo->chipType == PCI_CHIP_I810_DC100 || \
			pI810->PciInfo->chipType == PCI_CHIP_I810_E)
#define IS_I815(pI810) (pI810->PciInfo->chipType == PCI_CHIP_I815)
#define IS_I830(pI810) (pI810->PciInfo->chipType == PCI_CHIP_I830_M)
#define IS_845G(pI810) (pI810->PciInfo->chipType == PCI_CHIP_845_G)
#define IS_I85X(pI810)  (pI810->PciInfo->chipType == PCI_CHIP_I855_GM)
#define IS_I865G(pI810) (pI810->PciInfo->chipType == PCI_CHIP_I865_G)

#define IS_MOBILE(pI810) (IS_I830(pI810) || IS_I85X(pI810))

#define GTT_PAGE_SIZE			KB(4)
#define ROUND_TO(x, y)			(((x) + (y) - 1) / (y) * (y))
#define ROUND_DOWN_TO(x, y)		((x) / (y) * (y))
#define ROUND_TO_PAGE(x)		ROUND_TO((x), GTT_PAGE_SIZE)
#define ROUND_TO_MB(x)			ROUND_TO((x), MB(1))
#define PRIMARY_RINGBUFFER_SIZE		KB(128)
#define MIN_SCRATCH_BUFFER_SIZE		KB(16)
#define MAX_SCRATCH_BUFFER_SIZE		KB(64)
#define HWCURSOR_SIZE			GTT_PAGE_SIZE
#define OVERLAY_SIZE			GTT_PAGE_SIZE

/* Use a 64x64 HW cursor */
#define I810_CURSOR_X			64
#define I810_CURSOR_Y			I810_CURSOR_X

/* XXX Need to check if these are reasonable. */
#define MAX_DISPLAY_PITCH		2048
#define MAX_DISPLAY_HEIGHT		2048

#define PIPE_NAME(n)			('A' + (n))

#endif /* _INTEL_COMMON_H_ */
