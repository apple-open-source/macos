/**********************************************************************
Copyright 1998, 1999 by Precision Insight, Inc., Cedar Park, Texas.

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and
its documentation for any purpose is hereby granted without fee,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Precision Insight not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  Precision Insight
and its suppliers make no representations about the suitability of this
software for any purpose.  It is provided "as is" without express or 
implied warranty.

PRECISION INSIGHT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**********************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/neomagic/neo_macros.h,v 1.2 2002/10/30 12:52:22 alanh Exp $ */

/*
 * The original Precision Insight driver for
 * XFree86 v.3.3 has been sponsored by Red Hat.
 *
 * Authors:
 *   Jens Owen (jens@tungstengraphics.com)
 *   Kevin E. Martin (kevin@precisioninsight.com)
 *
 * Port to Xfree86 v.4.0
 *   1998, 1999 by Egbert Eich (Egbert.Eich@Physik.TU-Darmstadt.DE)
 */

#define WAIT_FB_FIFO_EMPTY() {                                              \
            while( INREG(NEOREG_BLTSTAT) & NEO_BS0_FIFO_PEND);              \
        }

#define WAIT_BLT_DONE() {                                                   \
            while( INREG(NEOREG_BLTSTAT) & NEO_BS0_BLT_BUSY);               \
        }

#define WAIT_ENGINE_IDLE() {                                                \
	    WAIT_BLT_DONE();                                                \
        }

#ifdef NOT_DONE
#define WAIT_FIFO(n) {                                                      \
	    while( NeoFifoCount < (n)) {                                    \
		NeoFifoCount = (INREG(NEOREG_BLTSTAT) >> 8);                \
	    }                                                               \
	    NeoFifoCount -= (n);                                            \
        }
#else
#define WAIT_FIFO(n) {                                                      \
	    WAIT_ENGINE_IDLE();                                             \
        }
#endif

