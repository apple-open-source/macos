/* $Xorg: math.c,v 1.4 2001/02/09 02:05:47 xorgcvs Exp $ */

/**** module math.c ****/
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
  
	math.c -- math flo element tests 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/math.c,v 1.6 2001/12/14 20:01:50 dawes Exp $ */

#include "xieperf.h"
#include <stdio.h>

static XiePhotomap XIEPhotomap;
static XiePhotomap ControlPlane;
static XieRoi XIERoi;
static XieLut XIELut;

static int flo_notify;
static XiePhotoElement *flograph;
static XiePhotoflo flo;
static int flo_elements;

static XieClipScaleParam *parms;
static int monoflag;

int 
InitMath(XParms xp, Parms p, int reps)
{
	XieProcessDomain domain;
	XieArithmeticOp	*ops;
	int		nops;
	unsigned int	bandMask;
	Bool		useDomain;
	Bool		constrain;
	XieConstrainTechnique constrainTech;
	int		idx;
	XieRectangle	rect;
	int		i;
        XieLTriplet levels;
        XieConstant in_low,in_high;
        XieLTriplet out_low,out_high;
	XIEimage *image;
        int     cclass;

#if     defined(__cplusplus) || defined(c_plusplus)
        cclass = xp->vinfo.c_class;
#else
        cclass = xp->vinfo.class;
#endif

        monoflag = 0;

	useDomain = ( ( MathParms * )p->ts )->useDomain;
	constrain = ( ( MathParms * )p->ts )->constrain;
	constrainTech = ( ( MathParms * )p->ts )->constrainTech;
	ops = ( ( MathParms * )p->ts )->ops;
	nops = ( ( MathParms * )p->ts )->nops;
	bandMask = ( ( MathParms * )p->ts )->bandMask;
	in_low[ 0 ] = ( ( MathParms * )p->ts )->inLow;
	in_high[ 0 ] = ( ( MathParms * )p->ts )->inHigh;  
	in_low[ 1 ] = in_low[ 2 ] = 0.0;
	in_high[ 1 ] = in_high[ 2 ] = 0.0;

	XIELut = ( XieLut ) NULL;
	XIERoi = ( XieRoi ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;
	ControlPlane = ( XiePhotomap ) NULL;
	flo = ( XiePhotoflo ) NULL;
	flograph = ( XiePhotoElement * ) NULL;
	parms = ( XieClipScaleParam * ) NULL;

	for ( i = 0; i < nops; i++ )
	{
		switch( ops[ i ] )
		{
			case xieValExp:
			case xieValLn:
			case xieValLog2:
			case xieValLog10:
			case xieValSquare:
			case xieValSqrt:
				break;
			default:
				fprintf( stderr, "Invalid math op\n" );
				reps = 0;
				break;
		}
	}

	if ( !reps )
		return( reps );	

	image = p->finfo.image1;

	if ( !image )
		return ( 0 );

	if ( ( constrain == False && xp->screenDepth != image->depth[ 0 ] ) ) 
	{
                monoflag = 1;
                if ( ( XIELut = CreatePointLut( xp, p,
                        1 << image->depth[ 0 ], 1 << xp->screenDepth, False ) )
                        == ( XieLut ) NULL )
                {
                        reps = 0;
                }
	}
        else if ( IsTrueColorOrDirectColor( cclass ) && constrain == True )
        {
                monoflag = 1;
                if ( ( XIELut = CreatePointLut( xp, p,
                        xp->vinfo.colormap_size, 1 << xp->screenDepth, False ) )
                        == ( XieLut ) NULL )
                {
                        reps = 0;
                }
        }

	flo_elements = 2 + nops;
	if ( useDomain == DomainROI || useDomain == DomainCtlPlane )
		flo_elements++;
	if ( constrain == True )
		flo_elements += 2;
	if ( monoflag )
		flo_elements += 2;

	flograph = XieAllocatePhotofloGraph( flo_elements );	
	if ( flograph == ( XiePhotoElement * ) NULL )
	{
		fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
		reps = 0;
	}
	else if ( ( XIEPhotomap = 
		GetXIEPhotomap( xp, p, 1 ) ) == ( XiePhotomap ) NULL )
	{
		reps = 0;
	}
	else if ( useDomain == DomainROI )
	{
		rect.x = ( ( MathParms * )p->ts )->x;
		rect.y = ( ( MathParms * )p->ts )->y;
		rect.width = ( ( MathParms * )p->ts )->width;
		rect.height = ( ( MathParms * )p->ts )->height;

		if ( ( XIERoi = GetXIERoi( xp, p, &rect, 1 ) ) == 
			( XieRoi ) NULL )
		{
			reps = 0;
		}
	}
	else if ( useDomain == DomainCtlPlane )
	{
		ControlPlane = GetControlPlane( xp, 13 );
		if ( ControlPlane == ( XiePhotomap ) NULL )
			reps = 0;
	}

	if ( reps )
	{
		idx = 0;

		domain.offset_x = 0;
		domain.offset_y = 0;

		if ( useDomain == DomainROI )
		{
	        	XieFloImportROI(&flograph[idx], XIERoi);
			idx++;
			domain.phototag = idx;
		}
                else if ( useDomain == DomainCtlPlane )
                {
                        XieFloImportPhotomap(&flograph[idx], ControlPlane, 
				False);
                        idx++;
                        domain.phototag = idx;
                }
		else
		{
			domain.phototag = 0;
		}

		XieFloImportPhotomap(&flograph[idx], XIEPhotomap, False );
		idx++;

		if ( constrain == True )
		{
			XieFloUnconstrain( &flograph[idx], idx );
			idx++;
		}

		for ( i = 0; i < nops; i++ )
		{ 
			XieFloMath(&flograph[idx], 
				idx,
				&domain,
				ops[ i ],
				bandMask
			);
			idx++;
		}

		if ( constrain == True )
		{
		        levels[ 0 ] = xp->vinfo.colormap_size;
			levels[ 1 ] = 0;
			levels[ 2 ] = 0;

			if ( constrainTech == xieValConstrainHardClip )
				parms = ( XieClipScaleParam * ) NULL;
			else if ( constrainTech == xieValConstrainClipScale )
			{
				out_low[ 0 ] = 0;
				out_high[ 0 ] = xp->vinfo.colormap_size - 1;

				parms = ( XieClipScaleParam * ) 
					XieTecClipScale( in_low, 
					in_high, out_low, out_high);
				if ( parms == ( XieClipScaleParam * ) NULL )
				{
	                                fprintf( stderr, "Trouble loading clipscale technique parameters\n" );
					reps = 0;
				}
			} 
			else
			{
				fprintf( stderr, 
					"Invalid technique for constrain\n" );
				reps = 0;
			}	
			if ( reps )
			{
				XieFloConstrain( &flograph[idx],
					idx,
					levels,
					constrainTech,
					( char * ) parms
				);
				idx++;
			}
		}
	
		if ( reps && monoflag )
		{
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

		if ( reps )	
		{
			XieFloExportDrawable(&flograph[idx],
				idx,     	/* source phototag number */
				xp->w,
				xp->fggc,
				0,       /* x offset in window */
				0        /* y offset in window */
			);
			idx++;
	
			flo = XieCreatePhotoflo( xp->d, flograph, 
				flo_elements );
			flo_notify = True;
		}
	}
	if ( !reps )
		FreeMathStuff( xp, p );
	return( reps );
}

void 
DoMath(XParms xp, Parms p, int reps)
{
    	int     i;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
		WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo, 0, False );	
    	}
}

void 
EndMath(XParms xp, Parms p)
{
	FreeMathStuff( xp, p );
}

void
FreeMathStuff(XParms xp, Parms p)
{
	if ( parms )
	{
		XFree( parms );
		parms = ( XieClipScaleParam * ) NULL;
	}
	if ( XIELut )
	{
                XieDestroyLUT( xp->d, XIELut );
                XIELut = ( XieLut ) NULL;
	}
	if ( XIERoi )
	{
                XieDestroyROI( xp->d, XIERoi );
                XIERoi = ( XieRoi ) NULL;
	}
	if ( XIEPhotomap && IsPhotomapInCache( XIEPhotomap ) == False )
	{
		XieDestroyPhotomap(xp->d, XIEPhotomap);
		XIEPhotomap = ( XiePhotomap ) NULL;
	}
        if ( ControlPlane != ( XiePhotomap ) NULL && IsPhotomapInCache( ControlPlane ) == False )
        {
                XieDestroyPhotomap( xp->d, ControlPlane );
                ControlPlane = ( XiePhotomap ) NULL;
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
}
