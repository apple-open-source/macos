/* $Xorg: dither.c,v 1.4 2001/02/09 02:05:47 xorgcvs Exp $ */

/**** module dither.c ****/
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
  
	dither.c -- dither flo element tests 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/dither.c,v 1.6 2001/12/14 20:01:48 dawes Exp $ */

#include "xieperf.h"
#include <stdio.h>

static XiePhotomap XIEPhotomap;
static int flo_notify, flo_id;
static XiePhotospace photospace;
static XiePhotoElement *flograph;
static int decode_notify;
static XieLTriplet levels;
static XieConstant in_low,in_high;
static XieLTriplet out_low,out_high;
static char *dithertech_parms=NULL;
static XieClipScaleParam *parms;
static int maxlevels;
static int flo_elements;

int 
InitDither(XParms xp, Parms p, int reps)
{
	int	threshold;

	photospace = ( XiePhotospace ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;
	parms = ( XieClipScaleParam * ) NULL; 
	dithertech_parms = ( char * ) NULL;
	flograph = ( XiePhotoElement * ) NULL; 
	maxlevels = DepthFromLevels( xp->vinfo.colormap_size ); 
	threshold = ( ( DitherParms * )p->ts )->threshold;

	if ( ( ( DitherParms * ) p->ts )->dither != xieValDitherDefault )
	{
		if ( TechniqueSupported( xp, xieValDither, 
			( ( DitherParms * ) p->ts )->dither ) == False )
		{
			fprintf( stderr, "Dither technique %d not supported\n", 
				( ( DitherParms * ) p->ts )->dither );
			reps = 0;
		}
	}
	if ( reps )
	{
        	if ( ( XIEPhotomap = GetXIEPhotomap( xp, p, 1 ) ) == 
			( XiePhotomap ) NULL )
		{
			fprintf( stderr, "Couldn't get photomap\n" );
			reps = 0;
		}
	}
	if ( reps )
	{
		photospace = XieCreatePhotospace(xp->d);
		decode_notify = False;
		levels[ 0 ] = levels[ 1 ] = levels[ 2 ] = 0;
		in_low[ 0 ] = in_low[ 1 ] = in_low[ 2 ] = 0.0;
		out_low[ 0 ] = out_low[ 1 ] = out_low[ 2 ] = 0;
		out_high[ 0 ] = out_high[ 1 ] = out_high[ 2 ] = 0;
		if ( ( ( DitherParms * )p->ts )->drawable == Drawable )
			out_high[ 0 ] = ( 1 << xp->screenDepth ) - 1;
		else
			out_high[ 0 ] = 1;
		flo_elements = 5;
		flograph = XieAllocatePhotofloGraph(flo_elements);	
		if ( flograph == ( XiePhotoElement * ) NULL )
		{
			fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
			reps = 0;
		}
	}
	if ( reps )
	{
		flo_notify = False;	
		XieFloImportPhotomap(&flograph[0],XIEPhotomap, decode_notify);

		if ( ( ( DitherParms * ) p->ts )->dither == xieValDitherOrdered )
		{	 
			dithertech_parms = ( char * ) 
				XieTecDitherOrderedParam(threshold); 
			if ( dithertech_parms == ( char * ) NULL )
			{
				fprintf( stderr, 
				"Trouble loading dither technique parameters\n" );
				reps = 0;
			}
		}
	}
	if ( !reps )
	{
		FreeDitherStuff( xp, p );
	}
        return( reps );
}

void 
DoDither(XParms xp, Parms p, int reps)
{
	int	i, j, idx;
	char    *tech_parms=NULL;
	XieLut	XIELut;
	XieProcessDomain domain;

	j = 0;
    	for (i = 0; i != reps; i++) {
		XIELut = ( XieLut ) NULL;
		idx = 1;
		flo_id = i + 1;		
		if ( ( ( DitherParms * ) p->ts )->drawable == Drawable )
			j += 2;
		else
			j = 2;
		if ( j >= maxlevels )
			j = 2;
		levels[ 0 ] = j;

		XieFloDither( &flograph[ idx ], 
			idx,
			( ( DitherParms * ) p->ts )->bandMask,
			levels,
			( ( DitherParms * ) p->ts )->dither,
			dithertech_parms
		);
		idx++;

		in_high[ 0 ] =  ( float ) j - 1.0;
		in_high[ 1 ] =  0.0;
		in_high[ 2 ] =  0.0;
		if ( tech_parms )
		{
			XFree( tech_parms );
			tech_parms = ( char * ) NULL;
		}
		parms = XieTecClipScale( in_low, in_high, out_low, out_high);
		tech_parms = ( char * ) parms;
		if ( tech_parms == ( char * ) NULL )
		{
			fprintf( stderr, 
				"Trouble loading clipscale technique parameters\n" );
			return;
		}

		if ( DepthFromLevels( levels[ 0 ] ) != xp->screenDepth && 
			( ( DitherParms * ) p->ts )->drawable == Drawable )
		{
			if ( ( XIELut = CreatePointLut( xp, p, levels[ 0 ],
				1 << xp->screenDepth, False ) )
				== ( XieLut ) NULL )
			{
				reps = 0;
			}

                        XieFloImportLUT(&flograph[idx], XIELut );
                        idx++;

                        domain.phototag = 0;
                        domain.offset_x = 0;
                        domain.offset_y = 0;
                        XieFloPoint(&flograph[idx],
                                idx-1,
                                &domain,
                                idx,
                                0x7
                        );
                        idx++;
		}

		if ( ( ( DitherParms * ) p->ts )->drawable == Drawable )
			XieFloExportDrawable(&flograph[idx],
				idx,       /* source phototag number */
				xp->w,
				xp->fggc,
				0,       /* x offset in window */
				0        /* y offset in window */
			);
		else
			XieFloExportDrawablePlane(&flograph[idx],
				idx,       /* source phototag number */
				xp->w,
				xp->fggc,
				0,       /* x offset in window */
				0        /* y offset in window */
			);
		idx++;
       		XieExecuteImmediate(xp->d, photospace,
               		flo_id,		
               		flo_notify,     
               		flograph,       /* photoflo specification */
               		idx    		/* number of elements */
       		);
		XSync( xp->d, 0 );
		if ( XIELut )
		{
			XieDestroyLUT( xp->d, XIELut );
		}
    	}
	if ( tech_parms )
	{
		XFree( tech_parms );
		tech_parms = ( char * ) NULL;
	}
}

void 
EndDither(XParms xp, Parms p)
{
	FreeDitherStuff( xp, p );
}

void
FreeDitherStuff(XParms xp, Parms p)
{
	if ( XIEPhotomap && IsPhotomapInCache( XIEPhotomap ) == False )
	{
		XieDestroyPhotomap(xp->d, XIEPhotomap);
		XIEPhotomap = ( XiePhotomap ) NULL;
	}
	if ( dithertech_parms )
	{
		XFree( dithertech_parms );
		dithertech_parms = ( char * ) NULL;
	}
	if ( flograph != (XiePhotoElement *) NULL )
	{
		XieFreePhotofloGraph(flograph,4);	
		flograph = ( XiePhotoElement * ) NULL; 
	}
	if ( photospace )
	{
		XieDestroyPhotospace( xp->d, photospace );
		photospace = ( XiePhotospace ) NULL;
	}
}

