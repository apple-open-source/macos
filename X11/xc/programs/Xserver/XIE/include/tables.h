/* $Xorg: tables.h,v 1.4 2001/02/09 02:04:23 xorgcvs Exp $ */
/**** module tables.h ****/
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

	tables.h: entry points etc.

	Dean Verheiden, Robert NC Shelley -- AGE Logic, Inc. April 1993

******************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/include/tables.h,v 1.5 2001/12/14 19:58:15 dawes Exp $ */

#ifndef _XIEH_TABLES
#define _XIEH_TABLES

#include <flostr.h>
#include <protoflo.h>

#define DDAnalyzeIndex		0
#define DDServerChoiceIndex	1

#ifndef _XIEC_TABLES

extern	peDefPtr	(*MakeTable[])();
extern  xieVoidProc 	DDInterface[];

#endif /* _XIEC_TABLES */

	/* dd entry points for di */

extern int DAGalyze(floDefPtr);
extern xieBoolProc GetServerChoice(floDefPtr, peDefPtr);	/* FIXME: mixie */

	/* lut.c */
extern	int		ProcCreateLUT(ClientPtr);
extern	int		ProcDestroyLUT(ClientPtr);
extern	int		SProcCreateLUT(ClientPtr);
extern	int		SProcDestroyLUT(ClientPtr);
	/* photomap.c */
extern	int		ProcCreatePhotomap(ClientPtr);
extern	int		ProcDestroyPhotomap(ClientPtr);
extern	int		ProcQueryPhotomap(ClientPtr);
extern	int		SProcCreatePhotomap(ClientPtr);
extern	int		SProcDestroyPhotomap(ClientPtr);
extern	int		SProcQueryPhotomap(ClientPtr);
	/* technq.c */
extern	int		ProcQueryTechniques(ClientPtr);
extern	int		SProcQueryTechniques(ClientPtr);

#if XIE_FULL
	/* colorlst.c */
extern	int		ProcCreateColorList(ClientPtr);
extern	int		ProcDestroyColorList(ClientPtr);
extern	int		ProcPurgeColorList(ClientPtr);
extern	int		ProcQueryColorList(ClientPtr);
extern	int		SProcCreateColorList(ClientPtr);
extern	int		SProcDestroyColorList(ClientPtr);
extern	int		SProcPurgeColorList(ClientPtr);
extern	int		SProcQueryColorList(ClientPtr);
	/* roi.c */
extern	int		ProcCreateROI(ClientPtr);
extern	int		ProcDestroyROI(ClientPtr);
extern	int		SProcCreateROI(ClientPtr);
extern	int		SProcDestroyROI(ClientPtr);
#endif

/* elements */
#include <dixie_i.h>
#include <dixie_e.h>
#include <dixie_p.h>

extern	void		init_proc_tables(
				CARD16 minorVersion,
				int (**ptable[])(ClientPtr),
				int (**sptable[])(ClientPtr));

#endif /* _XIEH_TABLES */
