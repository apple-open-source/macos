/* $Xorg: uconstrain.c,v 1.4 2001/02/09 02:05:48 xorgcvs Exp $ */

/**** module uconstrain.c ****/
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
  
	uconstrain.c -- unconstrain flo element test 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/uconstrain.c,v 1.6 2001/12/14 20:01:52 dawes Exp $ */


#include "xieperf.h"
#include <stdio.h>

static XiePhotomap XIEPhotomap;
static XieLut XIELut;
static int flo_notify;
static XiePhotoElement *flograph;
static XiePhotoflo flo;
static XieClipScaleParam *parms;
static int flo_elements;

int 
InitUnconstrain(XParms xp, Parms p, int reps)
{
	int decode_notify = 0;
	XieLTriplet levels;
	XieProcessDomain domain;
	XieConstrainTechnique tech = ( XieConstrainTechnique ) NULL;
	char *tech_parms=NULL;
        XieConstant in_low,in_high;
        XieLTriplet out_low,out_high;
	XIEimage *image;
	Bool pointFlag = False;
	int	cclass;
	int	idx = 0;

	image = p->finfo.image1;

#if     defined(__cplusplus) || defined(c_plusplus)
    	cclass = xp->vinfo.c_class;
#else
    	cclass = xp->vinfo.class;
#endif

	flo = ( XiePhotoflo ) NULL;
	flograph = ( XiePhotoElement * ) NULL;
	parms = ( XieClipScaleParam * ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;

	if ( ( XIEPhotomap = GetXIEPhotomap( xp, p, 1 ) ) == ( XiePhotomap ) NULL )
		reps = 0;
	else
	{
		tech = ( ( UnconstrainParms * )p->ts )->constrain;
		decode_notify = False;

		if ( xp->screenDepth != image->depth[ 0 ] )
		{
			pointFlag = True;
		}
		
		levels[ 0 ] = 1 << image->depth[ 0 ];
		levels[ 1 ] = levels[ 2 ] = levels[ 0 ];

		in_low[ 0 ] = 0.0;
		in_low[ 1 ] = 0.0;
		in_low[ 2 ] = 0.0;
		in_high[ 0 ] = (levels[ 0 ]) - 1.0;
		in_high[ 1 ] = (levels[ 1 ]) - 1.0; 
		in_high[ 2 ] = (levels[ 2 ]) - 1.0; 
		out_low[ 0 ] = 0;
		out_low[ 1 ] = 0;
		out_low[ 2 ] = 0;
		out_high[ 0 ] = levels[ 0 ] - 1;
		out_high[ 1 ] = levels[ 1 ] - 1;
		out_high[ 2 ] = levels[ 2 ] - 1;

		if ( tech == xieValConstrainHardClip )
		{
			tech_parms = ( char * ) NULL;
		}
		else
		{
			parms = XieTecClipScale( in_low, in_high, out_low, out_high);
			tech_parms = ( char * ) parms;
			if ( tech_parms == ( char * ) NULL )
			{
				fprintf( stderr, 
					"Trouble loading clipscale technique parameters\n" );
				reps = 0;
			}
		}
	}
	if ( reps )
	{
		if ( pointFlag )
			flo_elements = 6;
		else
			flo_elements = 4;

		flograph = XieAllocatePhotofloGraph(flo_elements);	
		if ( flograph == ( XiePhotoElement * ) NULL )
		{
			fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
			reps = 0;
		}
	}
	if ( reps )
	{
		XieFloImportPhotomap(&flograph[idx],XIEPhotomap,decode_notify);
		idx++;

		XieFloUnconstrain(&flograph[idx], idx );
		idx++;	

	}
	if ( reps )
	{
		XieFloConstrain( &flograph[idx], 
			idx,
			levels,
			tech,
			tech_parms
		);
		idx++;

		if ( pointFlag )
		{
			if ((XIELut = CreatePointLut(xp, p, levels[ 0 ],
               			1 << xp->screenDepth, False)) == (XieLut) NULL)
			{
				reps = 0;
			}
			else
			{
				XieFloImportLUT(&flograph[idx], XIELut );
				idx++;

				domain.phototag = 0;
				domain.offset_x = 0;
				domain.offset_y = 0;
				XieFloPoint(&flograph[idx],
					idx - 1,
					&domain,
					idx,
					0x7
				);
				idx++;
			}
		}
	}
	
	if ( reps )
	{
		XieFloExportDrawable(&flograph[idx],
			idx,     /* source phototag number */
			xp->w,
			xp->fggc,
			0,       /* x offset in window */
			0        /* y offset in window */
		);
		idx++;

		flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );

		flo_notify = False;
	}
	if ( !reps )
		FreeUnconstrainStuff( xp, p );
	return( reps );
}

void 
DoUnconstrain(XParms xp, Parms p, int reps)
{
    	int     i;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
    	}
}

void 
EndUnconstrain(XParms xp, Parms p)
{
	FreeUnconstrainStuff( xp, p );
}

void
FreeUnconstrainStuff(XParms xp, Parms p)
{
	if ( parms )
	{
		XFree( parms );
		parms = ( XieClipScaleParam * ) NULL;
	}
	if ( flo )
	{
		XieDestroyPhotoflo( xp->d, flo );
		flo = ( XiePhotoflo ) NULL;
	}	
	if ( flograph )
	{
		XieFreePhotofloGraph(flograph,flo_elements);	
		flograph = ( XiePhotoElement * ) NULL;
	}
	if ( XIELut ) 
	{
		XieDestroyLUT(xp->d, XIELut);
		XIELut = ( XieLut ) NULL;
	}
	if ( XIEPhotomap && IsPhotomapInCache( XIEPhotomap ) == False )
	{
		XieDestroyPhotomap(xp->d, XIEPhotomap);
		XIEPhotomap = ( XiePhotomap ) NULL;
	}
}
