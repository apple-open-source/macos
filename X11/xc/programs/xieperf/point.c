/* $Xorg: point.c,v 1.4 2001/02/09 02:05:48 xorgcvs Exp $ */

/**** module point.c ****/
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
  
	point.c -- point flo element tests 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/point.c,v 1.6 2001/12/14 20:01:51 dawes Exp $ */


#include "xieperf.h"
#include <stdio.h>

static int monoflag = 0;

static XiePhotomap XIEPhotomap;
static XiePhotomap XIEPhotomap2;
static XiePhotomap ControlPlane;
static XieLut XIELut;
static XieLut ClipLut;
static XieRoi XIERoi;

static int flo_notify;
static XiePhotoElement *flograph;
static XiePhotoflo flo;
static int flo_elements;
static unsigned char *lut;
static XStandardColormap stdCmap;

int 
InitPoint(XParms xp, Parms p, int reps)
{
	int lutSize, i, idx; 
	int src1, src2, levelsIn, levelsOut;
        XieEncodeTechnique encode_tech=xieValEncodeServerChoice;
        char *encode_params=NULL;
        Bool photoDest;
	int useDomain;
	XieRectangle rect;
	unsigned int bandMask;
        XieProcessDomain domain;
        int decode_notify;

        photoDest = ( ( PointParms * )p->ts )->photoDest;
        useDomain = ( ( PointParms * )p->ts )->useDomain;
        bandMask = ( ( PointParms * )p->ts )->bandMask;

        flograph = ( XiePhotoElement * ) NULL;
        XIEPhotomap = ( XiePhotomap ) NULL;
        XIEPhotomap2 = ( XiePhotomap ) NULL;
        ControlPlane = ( XiePhotomap ) NULL;
        XIERoi = ( XieRoi ) NULL;
	XIELut = ClipLut = ( XieLut ) NULL;
        flo = ( XiePhotoflo ) NULL;

	XIEPhotomap2 = XieCreatePhotomap( xp->d );
	if ( XIEPhotomap2 == ( XiePhotomap ) NULL )
		return( 0 );
	monoflag = 0;
	levelsIn = (( PointParms * ) p->ts )->levelsIn;
	levelsOut = (( PointParms * ) p->ts )->levelsOut;
        if ( xp->screenDepth != levelsOut && photoDest == False )
        {
                monoflag = 1;
                if ( ( ClipLut = CreatePointLut( xp, p, 1 << levelsOut, 
                        1 << xp->screenDepth, False ) ) == ( XieLut ) NULL )
                {
			reps = 0;
                }
        }

	if ( reps )
	{
		flo_elements = 4;
		if ( useDomain == DomainROI || useDomain == DomainCtlPlane )
			flo_elements++;
		if ( monoflag )
			flo_elements+=2;

		if ( useDomain == DomainROI && levelsIn != levelsOut )
		{
			fprintf( stderr, 
				"levelsIn must equal levelsOut with ROIs\n" );
			reps = 0;
		}
	}

	lutSize = 1 << levelsIn;
	if ( reps )
	{
		lut = (unsigned char *)
			malloc( lutSize * sizeof( unsigned char ) );
		if ( lut == ( unsigned char * ) NULL )
			reps = 0;
		else if ( levelsIn == 1 && levelsOut == 8 )
		{
			lut[ 0 ] = 0;
			lut[ 1 ] = 255;
		}
		else if ( levelsIn == 8 && levelsOut == 1 )
		{
			for ( i = 0; i < lutSize / 2; i++ )
			{
				lut[ i ] = 0;
			}

			for ( i = lutSize / 2; i < lutSize; i++ )
			{
				lut[ i ] = 1;
			}
		}
		else if ( levelsIn == 8 && levelsOut == 8 )
		{
			for ( i = 0; i < lutSize; i++ )
			{
				lut[ i ] = ( ( 1 << levelsIn ) - 1 ) - i;
			}
		}
	}
	if ( reps )
	{
		if ( levelsIn == 1 && IsDISServer() )
		{
			XIEPhotomap = GetXIEPointPhotomap( xp, p, 1, 2, False );
		}	
		else if ( levelsIn == 1 )
		{
			XIEPhotomap = GetXIEDitheredPhotomap( xp, p, 1, 2 );
		}
		else 
		{
			XIEPhotomap = GetXIEPhotomap( xp, p, 1 );
		}
		if ( XIEPhotomap == ( XiePhotomap ) NULL )
		{
			reps = 0;
		}
	}

	if ( reps != 0 )
	{
		if ( ( XIELut = GetXIELut( xp, p, lut, lutSize, 1 << levelsOut ) ) 
			== ( XieLut ) NULL )
		{
			reps = 0;
		}
	}
	if ( reps )
	{
		flograph = XieAllocatePhotofloGraph(flo_elements);	
		if ( flograph == ( XiePhotoElement * ) NULL )
		{
			fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
			reps = 0;
		}
		else
		{
			if ( useDomain == DomainROI )
			{
				rect.x = ( ( PointParms * )p->ts )->x;
				rect.y = ( ( PointParms * )p->ts )->y;
				rect.width = ( ( PointParms * )p->ts )->width;
				rect.height = ( ( PointParms * )p->ts )->height;

				if ( ( XIERoi = GetXIERoi( xp, p, &rect, 1 ) ) 
					== ( XieRoi ) NULL )
				{
					reps = 0;
				}
			}
			else if ( useDomain == DomainCtlPlane )
			{
				ControlPlane = GetControlPlane( xp, 2 );
				if ( ControlPlane == ( XiePhotomap ) NULL )
					reps = 0;
			}
		}
	}
	if ( reps )
	{
		idx = 0;

		decode_notify = False;
		XieFloImportPhotomap(&flograph[idx], XIEPhotomap, decode_notify);
		idx++;
		src1 = idx;

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
                        XieFloImportPhotomap(&flograph[idx], ControlPlane, False
);
                        idx++;
                        domain.phototag = idx;
                }
                else
                {
                        domain.phototag = 0;
                }

		XieFloImportLUT(&flograph[idx], XIELut );
		idx++;
		src2 = idx;

		XieFloPoint(&flograph[idx],
			src1,
			&domain,
			src2,
			bandMask
		);
		idx++;

		if ( photoDest == False )
		{
			if ( monoflag )
			{
				/* Point-to-point protocol :-) */

				XieFloImportLUT(&flograph[idx], ClipLut );
				idx++;

				domain.phototag = 0;

				XieFloPoint(&flograph[idx],
					idx - 1,
					&domain,
					idx,
					bandMask	
				);
				idx++;
			}

			XieFloExportDrawable(&flograph[idx],
				idx,     /* source phototag number */
				xp->w,
				xp->fggc,
				0,       /* x offset in window */
				0        /* y offset in window */
			);
		}
		else
		{
			XieFloExportPhotomap(&flograph[idx],
                                idx,              /* source phototag number */
                                XIEPhotomap2,
                                encode_tech,
                                encode_params
                        );
                        idx++;
		}

		flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
		flo_notify = False;
	}
	if ( !reps )
		FreePointStuff( xp, p );
	return( reps );
}

void 
DoPoint(XParms xp, Parms p, int reps)
{
    	int     i;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
    	}
}

void 
EndPoint(XParms xp, Parms p)
{
	FreePointStuff( xp, p );
}

void
FreePointStuff(XParms xp, Parms p)
{
	if ( XIEPhotomap && IsPhotomapInCache( XIEPhotomap ) == False )
	{
		XieDestroyPhotomap( xp->d, XIEPhotomap );
		XIEPhotomap = ( XiePhotomap ) NULL;
	}

        if ( ControlPlane != ( XiePhotomap ) NULL && IsPhotomapInCache( ControlPlane ) == False )
        {
                XieDestroyPhotomap( xp->d, ControlPlane );
                ControlPlane = ( XiePhotomap ) NULL;
        }

	if ( XIEPhotomap2 )
	{
		XieDestroyPhotomap( xp->d, XIEPhotomap2 );
		XIEPhotomap2 = ( XiePhotomap ) NULL;
	}

	if ( XIELut )
	{
		XieDestroyLUT( xp->d, XIELut );
		XIELut = ( XieLut ) NULL;
	}
	if ( ClipLut )
	{
		XieDestroyLUT( xp->d, ClipLut );
		ClipLut = ( XieLut ) NULL;
	}
	if ( flograph )
	{
		XieFreePhotofloGraph( flograph, flo_elements );
		flograph = ( XiePhotoElement * ) NULL;
	}
        if ( flo )
	{
                XieDestroyPhotoflo( xp->d, flo );
		flo = ( XiePhotoflo ) NULL;
	}
	if ( lut )
	{
		free( lut );
		lut = ( unsigned char * ) NULL;
	}
}

int 
InitTriplePoint(XParms xp, Parms p, int reps)
{
	int lutSize, idx; 
	int i, src1, src2;
        int useDomain;
	XieRectangle rect;
	unsigned int bandMask;
        XieProcessDomain domain;
        int decode_notify;
	XieLTriplet levels;
	int cclass, ditherTech;
	int threshold;
	Atom atom;

#if     defined(__cplusplus) || defined(c_plusplus)
        cclass = xp->vinfo.c_class;
#else
        cclass = xp->vinfo.class;
#endif
	if ( cclass != PseudoColor )
		return( 0 );
	if ( xp->vinfo.depth == 1 )
		return( 0 );

        atom = ( ( TriplePointParms * )p->ts )->atom;
	if ( GetStandardColormap( xp, &stdCmap, atom ) == False )
	{
		fprintf( stderr, "Couldn't get a standard colormap\n" );
		fflush( stderr );
		return( 0 );
	}

	fprintf( stderr, "Standard cmap found: dithering input to levels %ld, %ld, %ld\n", stdCmap.red_max + 1, stdCmap.green_max + 1, stdCmap.blue_max + 1 );
	fflush( stderr );
        useDomain = ( ( TriplePointParms * )p->ts )->useDomain;
        bandMask = ( ( TriplePointParms * )p->ts )->bandMask;
	levels[ 0 ] = stdCmap.red_max + 1; 
	levels[ 1 ] = stdCmap.green_max + 1; 
	levels[ 2 ] = stdCmap.blue_max + 1;
	ditherTech = ( ( TriplePointParms * )p->ts )->ditherTech;
	threshold = ( ( TriplePointParms * )p->ts )->threshold;

        flograph = ( XiePhotoElement * ) NULL;
        XIEPhotomap = ( XiePhotomap ) NULL;
        ControlPlane = ( XiePhotomap ) NULL;
        XIERoi = ( XieRoi ) NULL;
	XIELut = ( XieLut ) NULL;
        flo = ( XiePhotoflo ) NULL;

	flo_elements = 4;
	if ( useDomain == DomainROI || useDomain == DomainCtlPlane )
		flo_elements++;

	lutSize = 1 << xp->vinfo.depth;
	if ( reps )
	{
		lut = (unsigned char *)
			malloc( lutSize * sizeof( unsigned char ) );
		if ( lut == ( unsigned char * ) NULL )
			reps = 0;

		/* build a grayscale ramp lut */

	        for (i=0; i<lutSize; i++) {
       	        	lut[ i ] = i + stdCmap.base_pixel;
		}
	
	}
	if ( reps )
	{
		if ( ( XIEPhotomap = GetXIEDitheredTriplePhotomap( xp, p, 1, ditherTech, threshold, levels ) ) == ( XiePhotomap ) NULL )
		{
			reps = 0;
		}
	}

	if ( reps != 0 )
	{
		if ( ( XIELut = GetXIELut( xp, p, lut, lutSize, lutSize ) ) 
			== ( XieLut ) NULL )
		{
			reps = 0;
		}
	}
	if ( reps )
	{
		flograph = XieAllocatePhotofloGraph(flo_elements);	
		if ( flograph == ( XiePhotoElement * ) NULL )
		{
			fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
			reps = 0;
		}
		else
		{
			if ( useDomain == DomainROI )
			{
				rect.x = ( ( TriplePointParms * )p->ts )->x;
				rect.y = ( ( TriplePointParms * )p->ts )->y;
				rect.width = ( ( TriplePointParms * )p->ts )->width;
				rect.height = ( ( TriplePointParms * )p->ts )->height;

				if ( ( XIERoi = GetXIERoi( xp, p, &rect, 1 ) ) 
					== ( XieRoi ) NULL )
				{
					reps = 0;
				}
			}
			else if ( useDomain == DomainCtlPlane )
			{
				ControlPlane = GetControlPlane( xp, 2 );
				if ( ControlPlane == ( XiePhotomap ) NULL )
					reps = 0;
			}
		}
	}
	if ( reps )
	{
		idx = 0;

		decode_notify = False;
		XieFloImportPhotomap(&flograph[idx], XIEPhotomap, decode_notify);
		idx++;
		src1 = idx;

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
			XieFloImportPhotomap(&flograph[idx], 
				ControlPlane, False);
			idx++;
			domain.phototag = idx;
		}
                else
                {
                        domain.phototag = 0;
                }

		XieFloImportLUT(&flograph[idx], XIELut );
		idx++;
		src2 = idx;

		XieFloPoint(&flograph[idx],
			src1,
			&domain,
			src2,
			bandMask
		);
		idx++;

		XieFloExportDrawable(&flograph[idx],
			idx,     /* source phototag number */
			xp->w,
			xp->fggc,
			0,       /* x offset in window */
			0        /* y offset in window */
		);

		flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
		flo_notify = False;
	}
	if ( !reps )
		FreeTriplePointStuff( xp, p );
	else
		InstallThisColormap( xp, stdCmap.colormap );
	return( reps );
}

void 
DoTriplePoint(XParms xp, Parms p, int reps)
{
    	int     i;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
    	}
}

void 
EndTriplePoint(XParms xp, Parms p)
{
	InstallGrayColormap( xp );
	FreeTriplePointStuff( xp, p );
}

void
FreeTriplePointStuff(XParms xp, Parms p)
{
	if ( XIEPhotomap && IsPhotomapInCache( XIEPhotomap ) == False )
	{
		XieDestroyPhotomap( xp->d, XIEPhotomap );
		XIEPhotomap = ( XiePhotomap ) NULL;
	}

        if ( ControlPlane != ( XiePhotomap ) NULL && IsPhotomapInCache( ControlPlane ) == False )
        {
                XieDestroyPhotomap( xp->d, ControlPlane );
                ControlPlane = ( XiePhotomap ) NULL;
        }

	if ( XIELut )
	{
		XieDestroyLUT( xp->d, XIELut );
		XIELut = ( XieLut ) NULL;
	}
	if ( flograph )
	{
		XieFreePhotofloGraph( flograph, flo_elements );
		flograph = ( XiePhotoElement * ) NULL;
	}
        if ( flo )
	{
                XieDestroyPhotoflo( xp->d, flo );
		flo = ( XiePhotoflo ) NULL;
	}
	if ( lut )
	{
		free( lut );
		lut = ( unsigned char * ) NULL;
	}
}
