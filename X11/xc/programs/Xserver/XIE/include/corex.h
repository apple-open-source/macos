/* $Xorg: corex.h,v 1.4 2001/02/09 02:04:23 xorgcvs Exp $ */
/**** module corex.h ****/
/******************************************************************************

Copyright 1993, 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


				NOTICE
                              
This software is being provided by AGE Logic, Inc. under the
following license.  By obtaining, using and/or copying this software,
you agree that you have read, understood, and will comply with these
terms and conditions:

     Permission to use, copy, modify, distribute and sell this
     software and its documentation for any purpose and without
     fee or royalty and to grant others any or all rights granted
     herein is hereby granted, provided that you agree to comply
     with the following copyright notice and statements, including
     the disclaimer, and that the same appears on all copies and
     derivative works of the software and documentation you make.
     
     "Copyright 1993, 1994 by AGE Logic, Inc."
     
     THIS SOFTWARE IS PROVIDED "AS IS".  AGE LOGIC MAKES NO
     REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.  By way of
     example, but not limitation, AGE LOGIC MAKE NO
     REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS
     FOR ANY PARTICULAR PURPOSE OR THAT THE SOFTWARE DOES NOT
     INFRINGE THIRD-PARTY PROPRIETARY RIGHTS.  AGE LOGIC 
     SHALL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE.  IN NO
     EVENT SHALL EITHER PARTY BE LIABLE FOR ANY INDIRECT,
     INCIDENTAL, SPECIAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS
     OF PROFITS, REVENUE, DATA OR USE, INCURRED BY EITHER PARTY OR
     ANY THIRD PARTY, WHETHER IN AN ACTION IN CONTRACT OR TORT OR
     BASED ON A WARRANTY, EVEN IF AGE LOGIC LICENSEES
     HEREUNDER HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH
     DAMAGES.
    
     The name of AGE Logic, Inc. may not be used in
     advertising or publicity pertaining to this software without
     specific, written prior permission from AGE Logic.

     Title to this software shall at all times remain with AGE
     Logic, Inc.
*******************************************************************************

	corex.h - Xie server interface (for hooking to Core X)

		Dean Verheiden -- AGE Logic, Inc. April, 1993

******************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/include/corex.h,v 1.5 2001/12/14 19:58:12 dawes Exp $ */

#ifndef _XIEH_COREX
#define _XIEH_COREX

#include <extnsionst.h>

/* temporary hack:
 *
 * XIE is suppose to support single-band images up to the depth
 * of the deepest drawable supported by the server -- but not less than 16.
 * Triple-band data should never be more than 16 bits.
 *
 * Later releases will compute the correct value, but it will not exceed 24
 * in the sample implementation.  (eeeck, sounds like a global variable..)
 */

#define MAX_SINGLE 24
#define MAX_TRIPLE 16
#define MAX_DEPTH(nbands)  (nbands > 1 ? MAX_TRIPLE : MAX_SINGLE)
#define MAX_LEVELS(nbands) (nbands > 1 ? (1<<MAX_TRIPLE) : (1<<MAX_SINGLE))

#define ALIGNMENT  xieValArbitrary	

#ifdef LONG64
#define PITCH_MOD 64			/* default scanline pitch          */
#else
#define PITCH_MOD 32			/* default scanline pitch	   */
#endif

#if defined(_XIEC_SESSION)
/*
 * Define constants relating to unconstrained computation
 * See QueryImageExtension for description
 * Return values for Single Precision IEEE floats for now
 */
#define UNCONSTRAINED_MANTISSA		  24
#define UNCONSTRAINED_MAX_EXPONENT	 128	
#define UNCONSTRAINED_MIN_EXPONENT	-126	

CARD32       Preferred_levels[] = 
			{ 
			1<<1, 		/* Bit pixels */
			1<<8,		/* Byte pixels */
			1<<16,		/* Pair pixels */	
			1<<24		/* Quad pixels */
			};
#endif

/*
 * Interface to Core X 
 */
extern Bool         AddResource(XID id, RESTYPE type, pointer value);
extern void         FreeResource(XID id, RESTYPE type);

#ifndef _XIEC_SESSION
extern ExtensionEntry 	*extEntry;

extern RESTYPE	RC_XIE;			/* XIE Resource Class		*/
#if XIE_FULL
extern RESTYPE	RT_COLORLIST;		/* ColorList resource type	*/
#endif
extern RESTYPE	RT_LUT;			/* Lookup table resource type	*/
extern RESTYPE	RT_PHOTOFLO;		/* Photoflo   resource type	*/
extern RESTYPE	RT_PHOTOMAP;		/* Photomap   resource type	*/
extern RESTYPE	RT_PHOTOSPACE;		/* Photospace resource type	*/
#if XIE_FULL
extern RESTYPE	RT_ROI;			/* Region Of Interest type	*/
#endif
extern RESTYPE	RT_XIE_CLIENT;		/* XIE Type for Shutdown Notice */
#endif

#if !defined(_XIEC_COLORLST) && (XIE_FULL)
extern	int		DeleteColorList();
#endif
extern	int		DeleteLUT();
#ifndef _XIEC_PHOTOMAP
extern	int		DeletePhotomap();
#endif
#ifndef _XIEC_PROTOFLO
extern	int		DeletePhotoflo();
extern	int		DeletePhotospace();
#endif
#if !defined(_XIEC_ROI) && (XIE_FULL)
extern	int		DeleteROI();
#endif
#ifndef _XIEC_SESSION
extern	int		DeleteXieClient();
#endif

#endif /* end _XIEH_COREX */
