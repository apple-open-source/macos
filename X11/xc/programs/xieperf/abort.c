/* $Xorg: abort.c,v 1.4 2001/02/09 02:05:46 xorgcvs Exp $ */

/**** module abort.c ****/
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
*****************************************************************************
  
	abort.c -- abort flo test 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/abort.c,v 1.7 2001/12/14 20:01:46 dawes Exp $ */

#include "xieperf.h"
#include <stdio.h>

static int AbortAndWaitForEvent(XParms xp, Parms p, unsigned long namespace, 
				unsigned long flo_id);

static XieLut XIELut;
static XiePhotoElement *flograph;
static XiePhotoflo flo;

int 
InitAbort(XParms xp, Parms p, int reps)
{
        XieDataClass    class;
        XieOrientation  band_order;
        XieLTriplet     length, levels;
        Bool    	merge;
        XieLTriplet     start;

	XIELut = ( XieLut ) NULL;
	flograph = ( XiePhotoElement * ) NULL;
	flo = ( XiePhotoflo ) NULL;
	
	if ( !(XIELut = XieCreateLUT( xp->d ) ) )
	{
		fprintf( stderr, "XieCreateLUT failed\n" );
		reps = 0;
	}

	if ( reps )
	{
		/* set up a flo to read a lut from client */

		flograph = XieAllocatePhotofloGraph(2);
		if ( flograph == ( XiePhotoElement * ) NULL )
		{
			fprintf(stderr,"XieAllocatePhotofloGraph failed\n");
			reps = 0;
		}

		class = xieValSingleBand;
		band_order = xieValLSFirst;
		length[ 0 ] = ( ( AbortParms * ) p->ts )->lutSize;
		length[ 1 ] = 0;
		length[ 2 ] = 0;
		levels[ 0 ] = ( ( AbortParms * ) p->ts )->lutLevels; 
		levels[ 1 ] = 0;
		levels[ 2 ] = 0;

		XieFloImportClientLUT(&flograph[0],
			class,
			band_order,
			length,
			levels
		);

		merge = False;
		start[ 0 ] = 0;
		start[ 1 ] = 0;
		start[ 2 ] = 0;

		XieFloExportLUT(&flograph[1],
			1,              /* source phototag number */
			XIELut,
			merge,
			start
		);

		flo = XieCreatePhotoflo( xp->d, flograph, 2 );
	}
	if ( !reps )
		FreeAbortStuff( xp, p );
	return( reps );
}

void 
DoAbort(XParms xp, Parms p, int reps)
{
	int	i;

    	for (i = 0; i != reps; i++) {

		XieExecutePhotoflo( xp->d, flo, True );

		/* now, the flo should be active and waiting for data */

		if ( !AbortAndWaitForEvent( xp, p, 0L, flo ) )
			break;
    	}
}

void 
EndAbort(XParms xp, Parms p)
{
        XieFreePhotofloGraph(flograph,2);
        XieDestroyPhotoflo( xp->d, flo );
	XieDestroyLUT( xp->d, XIELut );
}

static int
AbortAndWaitForEvent(XParms xp, Parms p, unsigned long namespace, 
		     unsigned long flo_id )
{
	XieAbort( xp->d, namespace, flo_id );
	return( WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo_id, 0, False ) );
}

void
FreeAbortStuff(XParms xp, Parms p)
{
	if ( XIELut )
	{
		XieDestroyLUT( xp->d, XIELut );
		XIELut = ( XieLut ) NULL;
	}
	if ( flograph )
	{
                XieFreePhotofloGraph(flograph,2);
		flograph = ( XiePhotoElement * ) NULL;
	}
	if ( flo )
	{
                XieDestroyPhotoflo( xp->d, flo );
		flo = ( XiePhotoflo ) NULL;
	}
}
