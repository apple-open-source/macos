/* $Xorg: mtchhist.c,v 1.4 2001/02/09 02:05:48 xorgcvs Exp $ */

/**** module mtchhist.c ****/
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
  
	mtchhist.c -- match histogram flo element tests 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/mtchhist.c,v 1.6 2001/12/14 20:01:51 dawes Exp $ */


#include "xieperf.h"
#include <stdio.h>

static XiePhotomap XIEPhotomap;
static XieRoi XIERoi;
static XieLut XIELut;
static XiePhotomap ControlPlane;

static XIEimage	*image;
static int flo_notify;
static XiePhotoElement *flograph;
static XiePhotoflo flo;
static char *shape_parms;
static XieHistogramData *histos;
static int flo_elements;
static int histo1, histo2;

extern Window monitorWindow;
extern Window monitor2Window;
extern Window drawableWindow; 
extern Bool dontClear;

int 
InitMatchHistogram(XParms xp, Parms p, int reps)
{
	XieProcessDomain domain;
	XieHistogramShape shape;
	double	mean, sigma;
	double	constant;
	Bool	shape_factor;	
	int	useDomain;
	XieRectangle rect;
	int	idx;
	int	src1, src2;
	int	monoflag;

	XIEPhotomap = ( XiePhotomap ) NULL;
	ControlPlane = ( XiePhotomap ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;
	XIELut = ( XieLut ) NULL;
	XIERoi = ( XieRoi ) NULL;
	histos = ( XieHistogramData * ) NULL;
	shape_parms = ( char * ) NULL;	
	monoflag = 0;

	useDomain = ( ( MatchHistogramParms * )p->ts )->useDomain;
	shape = ( ( MatchHistogramParms * )p->ts )->shape;

	image = p->finfo.image1;
	flo_elements = 6;
	if ( !image )
		reps = 0;
	else
	{
		histos = ( XieHistogramData * ) 
			malloc( sizeof( XieHistogramData ) * 
			( 1 << image->depth[ 0 ] ) );
		if ( histos == ( XieHistogramData * ) NULL )
			reps = 0;
	}
	if ( reps )
	{
		if ( xp->screenDepth != image->depth[ 0 ] )
		{
			flo_elements += 4;
	                monoflag = 1;
			if ( ( XIELut = CreatePointLut( xp, p,
				1 << image->depth[0], 
				1 << xp->screenDepth, False ) )
				== ( XieLut ) NULL )
			{
				reps = 0;
			}
		}
	}
	if ( reps )
	{
		if ( useDomain == DomainROI )
		{
			flo_elements++;
			rect.x = (( MatchHistogramParms *)p->ts )->x;
			rect.y = (( MatchHistogramParms *)p->ts )->y;
			rect.width = (( MatchHistogramParms *)p->ts )->width;
			rect.height = (( MatchHistogramParms *)p->ts )->height;
			if ( ( XIERoi = GetXIERoi( xp, p, &rect, 1 ) ) ==
				( XieRoi ) NULL )
			{
				reps = 0;
			}
		}
                else if ( useDomain == DomainCtlPlane )
                {
			flo_elements++;
                        ControlPlane = GetControlPlane( xp, 2 );
                        if ( ControlPlane == ( XiePhotomap ) NULL )
                                reps = 0;
                }
	}
	if ( reps )
	{
		if ( shape == xieValHistogramFlat )
			shape_parms = ( char * ) NULL;
		else if ( shape == xieValHistogramGaussian )
		{
			mean = ( ( MatchHistogramParms * )p->ts )->mean;
			sigma = ( ( MatchHistogramParms * )p->ts )->sigma;
			shape_parms = 
				(char *)XieTecHistogramGaussian(mean,sigma);
			if ( !shape_parms )
			{
				fprintf( stderr, "NULL value returned by XieTecHistogramGaussian\n" );
				reps = 0;
			}
		}
		else if ( shape == xieValHistogramHyperbolic )
		{	
			constant = ( ( MatchHistogramParms * )p->ts )->constant;
			shape_factor = 
				((MatchHistogramParms *)p->ts)->shape_factor;
			shape_parms = (char *) XieTecHistogramHyperbolic(
				constant,shape_factor);
			if ( !shape_parms )
			{
				fprintf( stderr, "NULL value returned by XieTecHistogramGaussian\n" );
				reps = 0;
			}
		}
		else
		{
			fprintf( stderr, "Invalid histogram technique\n" );
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
		else if ( ( XIEPhotomap = 
			GetXIEPhotomap( xp, p, 1 ) ) == ( XiePhotomap ) NULL )
		{
			reps = 0;
		}
		else
		{
			idx = 0;

			domain.offset_x = 0;
			domain.offset_y = 0;
			domain.phototag = 0;
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

			XieFloImportPhotomap(&flograph[idx], 
				XIEPhotomap, False );
			idx++;
			src1 = idx;

			XieFloMatchHistogram(&flograph[idx], 
				src1,
				&domain,
				shape,
				shape_parms
			);
			idx++;
			src2 = idx;

			XieFloExportClientHistogram(&flograph[idx],
				src2,              /* source phototag number */
				&domain,
				xieValNewData
			);
			idx++;
			histo1 = idx;

			XieFloExportClientHistogram(&flograph[idx],
				src1,              /* source phototag number */
				&domain,
				xieValNewData
			);
			idx++;
			histo2 = idx;

			if ( monoflag )
			{
				XieFloImportLUT(&flograph[idx], XIELut );
				idx++;

				domain.phototag = 0;
				domain.offset_x = 0;
				domain.offset_y = 0;
				XieFloPoint(&flograph[idx],
					src1,
					&domain,
					idx,
					0x7
				);
				idx++;
				src1 = idx;
			}

			if ( monoflag )
			{
				XieFloImportLUT(&flograph[idx], XIELut );
				idx++;

				domain.phototag = 0;
				domain.offset_x = 0;
				domain.offset_y = 0;
				XieFloPoint(&flograph[idx],
					src2,
					&domain,
					idx,
					0x7
				);
				idx++;
				src2 = idx;
			}

			XieFloExportDrawable(&flograph[idx],
				src1,     	/* source phototag number */
				xp->w,
				xp->fggc,
				0,       /* x offset in window */
				0        /* y offset in window */
			);
			idx++;

			XieFloExportDrawable(&flograph[idx],
				src2,     	/* source phototag number */
				drawableWindow,
				xp->fggc,
				0,       /* x offset in window */
				0        /* y offset in window */
			);
			idx++;

			flo = XieCreatePhotoflo(xp->d, flograph, flo_elements);
			flo_notify = True;
		}
	}
	if ( reps )
	{
		XMoveWindow( xp->d, drawableWindow, WIDTH + 10, 0 );
		XMoveWindow( xp->d, monitorWindow, 0, HEIGHT - MONHEIGHT );
		XMoveWindow( xp->d, monitor2Window, WIDTH + 10, 
			HEIGHT - MONHEIGHT );
		XMapRaised( xp->d, drawableWindow );
		XMapRaised( xp->d, monitor2Window );
		XMapRaised( xp->d, monitorWindow );
		XSync( xp->d, 0 );
		dontClear = True;
	}
	else 
		FreeMatchHistogramStuff( xp, p );	
	return( reps );
}

void 
DoMatchHistogram(XParms xp, Parms p, int reps)
{
    	int     i, done, numHistos;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
		numHistos = ReadNotifyExportData( xp, p, 0, flo, histo2, 
			sizeof( XieHistogramData ), 0, (char **) &histos,
			&done ) / sizeof( XieHistogramData );
		DrawHistogram( xp, monitorWindow, 
			( XieHistogramData * ) histos,
			numHistos, 1<< xp->vinfo.depth ); 
		numHistos = ReadNotifyExportData( xp, p, 0, flo, histo1, 
			sizeof( XieHistogramData ), 0, (char **) &histos,
			&done ) / sizeof( XieHistogramData );
		DrawHistogram( xp, monitor2Window, 
			( XieHistogramData * ) histos,
			numHistos, 1 << xp->vinfo.depth ); 
		WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo, 0, False );
    	}
}

void 
EndMatchHistogram(XParms xp, Parms p)
{
	XUnmapWindow( xp->d, monitorWindow );
	XUnmapWindow( xp->d, monitor2Window );
	XUnmapWindow( xp->d, drawableWindow );
	dontClear = False;
	FreeMatchHistogramStuff( xp, p );
}

void
FreeMatchHistogramStuff(XParms xp, Parms p)
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

        if ( flograph )
        {
                XieFreePhotofloGraph(flograph,flo_elements);
                flograph = ( XiePhotoElement * ) NULL;
        }
        if ( flo )
        {
                XieDestroyPhotoflo( xp->d, flo );
                flo = ( XiePhotoflo ) NULL;
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
	if ( histos )
	{
		free( histos );
		histos = ( XieHistogramData * ) NULL;
	}
	if ( shape_parms )
	{
		XFree( shape_parms );
		shape_parms = ( char * ) NULL;
	}
}	

