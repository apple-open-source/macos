/* $Xorg: import.c,v 1.4 2001/02/09 02:05:47 xorgcvs Exp $ */

/**** module import.c ****/
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
  
	import.c -- import flo element tests 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/import.c,v 1.6 2001/12/14 20:01:50 dawes Exp $ */

#include "xieperf.h"
#include <stdio.h>

static XiePhotomap XIEPhotomap;
static XiePhotomap XIEPhotomap2;
static XieRoi	XIERoi;
static XieLut	XIELut;
extern Window 	drawableWindow;
static Pixmap	myPixmap;

static unsigned char *lut;
static XieRectangle *rects;

static int flo_notify;
static XiePhotoElement *flograph;
static int flo_elements;
static XiePhotoflo flo;

extern Bool dontClear;

int 
InitImportDrawablePixmap(XParms xp, Parms p, int reps)
{
	flo = ( XiePhotoflo ) NULL;
	flograph = ( XiePhotoElement * ) NULL;

	myPixmap = XCreatePixmap( xp->d, drawableWindow, WIDTH, HEIGHT, 
		xp->vinfo.depth );
	XFillRectangle( xp->d, myPixmap, xp->bggc, 0, 0, 
		WIDTH, HEIGHT );
	XSync( xp->d, 0 );
	GetXIEPixmap( xp, p, myPixmap, 1 );

	flo_elements = 2;
	flo_notify = False;	
	flograph = XieAllocatePhotofloGraph(flo_elements);	
	if ( flograph == ( XiePhotoElement * ) NULL )
	{
		fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
		reps = 0;
	}

	if ( reps )
	{
		XieFloImportDrawable(&flograph[0], 
			myPixmap,
			0, 
			0,
			WIDTH,
			HEIGHT,
			50,
			False
		);	

		XieFloExportDrawable(&flograph[flo_elements-1],
			flo_elements-1,       /* source phototag number */
			xp->w,
			xp->fggc,
			0,       /* x offset in window */
			0        /* y offset in window */
		);

		flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
	}

	if ( !reps )
		FreeImportDrawablePixmapStuff( xp, p );

	return( reps );
}

int 
InitImportDrawableWindow(XParms xp, Parms p, int reps)
{
	flo = ( XiePhotoflo ) NULL;
	flograph = ( XiePhotoElement * ) NULL;

        flo_elements = 2;
	flo_notify = False;	
	flograph = XieAllocatePhotofloGraph(flo_elements);	
	if ( flograph == ( XiePhotoElement * ) NULL )
	{
		fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
		reps = 0;
	}
	else
	{

		XieFloImportDrawable(&flograph[0], 
			xp->w,
			0, 
			0,
			WIDTH,
			HEIGHT,
			50,
			False
		);	

		XieFloExportDrawable(&flograph[1],
			1,              /* source phototag number */
			drawableWindow,
			xp->fggc,
			0,       /* x offset in window */
			0        /* y offset in window */
		);

		if ( ( ( ImportParms * )p->ts )->obscure == Obscuring )
			XMoveWindow( xp->d, drawableWindow, 100, 100 );
		else if ( ( ( ImportParms * )p->ts )->obscure == Obscured )
			XMoveWindow( xp->d, drawableWindow, 400, 300 );
		else
			XMoveWindow( xp->d, drawableWindow, WIDTH + 10, 0 );
		if ( ( ( ImportParms * )p->ts )->obscure == Obscured )
		{
			XMapWindow( xp->d, drawableWindow );
			XLowerWindow( xp->d, drawableWindow );
		}
		else
		{
			XMapRaised( xp->d, drawableWindow );
		}

		XSync( xp->d, 0 );
		GetXIEWindow( xp, p, xp->w, 1 );
		dontClear = True;

		flo = XieCreatePhotoflo( xp->d, flograph, 2 );
	}
	
	if ( !reps )
	{
		dontClear = False;
		FreeImportDrawableWindowStuff( xp, p );
	}

	return( reps );
}

int 
InitImportDrawablePlanePixmap(XParms xp, Parms p, int reps)
{
	int	planes;

	flo = ( XiePhotoflo ) NULL;
	flograph = ( XiePhotoElement * ) NULL;

	if ( xp->vinfo.depth == 1 )
	{
		planes = 1;
	}
	else
		planes = ( 1 << xp->vinfo.depth ) >> 1;
	myPixmap = XCreatePixmap( xp->d, drawableWindow, WIDTH, HEIGHT, 
		xp->vinfo.depth );
	XFillRectangle( xp->d, myPixmap, xp->bggc, 0, 0, WIDTH, HEIGHT );
	XSync( xp->d, 0 );
	GetXIEPixmap( xp, p, myPixmap, 1 );

	flo_elements = 2;
	flo_notify = False;	
	flograph = XieAllocatePhotofloGraph(flo_elements);	
	if ( flograph == ( XiePhotoElement * ) NULL )
	{
		fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
		reps = 0;
	}
	else
	{
		XieFloImportDrawablePlane(&flograph[0], 
			myPixmap,
			0, 
			0,
			WIDTH,
			HEIGHT,
			0,
			planes,
			False
		);	

		XieFloExportDrawablePlane(&flograph[flo_elements-1],
			flo_elements-1,       /* source phototag number */
			xp->w,
			xp->fggc,
			0,       /* x offset in window */
			0        /* y offset in window */
		);

		flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
	}
	if ( !reps )
	{
		FreeImportDrawablePixmapStuff( xp, p );
	}
	return( reps );
}

int 
InitImportDrawablePlaneWindow(XParms xp, Parms p, int reps)
{
	int	planes;

	flo = ( XiePhotoflo ) NULL;
	flograph = ( XiePhotoElement * ) NULL;

	if ( xp->vinfo.depth == 1 )
	{
		planes = 1;
	}
	else
	{
		planes = ( 1 << xp->vinfo.depth ) >> 1;
	}

	flo_elements = 2;
	flo_notify = False;	
	flograph = XieAllocatePhotofloGraph(flo_elements);	
	if ( flograph == ( XiePhotoElement * ) NULL )
	{
		fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
		reps = 0;
	}
	else
	{
		XieFloImportDrawablePlane(&flograph[0], 
			xp->w,
			0, 
			0,
			WIDTH,
			HEIGHT,
			0,
			planes,
			False
		);	

		XieFloExportDrawablePlane(&flograph[flo_elements-1],
			flo_elements-1,       /* source phototag number */
			drawableWindow,
			xp->fggc,
			0,       /* x offset in window */
			0        /* y offset in window */
		);

		if ( ( ( ImportParms * )p->ts )->obscure == Obscuring )
			XMoveWindow( xp->d, drawableWindow, 100, 100 );
		else if ( ( ( ImportParms * )p->ts )->obscure == Obscured )
			XMoveWindow( xp->d, drawableWindow, 400, 300 );
		else
			XMoveWindow( xp->d, drawableWindow, WIDTH + 10, 0 );
		if ( ( ( ImportParms * )p->ts )->obscure == Obscured )
		{
			XMapWindow( xp->d, drawableWindow );
			XLowerWindow( xp->d, drawableWindow );
		}
		else
		{
			XMapRaised( xp->d, drawableWindow );
		}

		GetXIEWindow( xp, p, xp->w, 1 );

		flo = XieCreatePhotoflo( xp->d, flograph, 2 );
		dontClear = True;
	}
	if ( !reps )
	{
		dontClear = False;
		FreeImportDrawableWindowStuff( xp, p );
	}

	return( reps );
}

int 
InitImportPhoto(XParms xp, Parms p, int reps)
{
        XieEncodeTechnique encode_tech=xieValEncodeServerChoice;
        char *encode_params=NULL;
	int	decode_notify;

	XIEPhotomap = ( XiePhotomap ) NULL;
	XIEPhotomap2 = ( XiePhotomap ) NULL;
	flo = ( XiePhotoflo ) NULL;
	flograph = ( XiePhotoElement * ) NULL;

	XIEPhotomap2 = XieCreatePhotomap( xp->d );
	if ( XIEPhotomap2 == ( XiePhotomap ) NULL )
		reps = 0;
	else if ( ( XIEPhotomap = GetXIEPhotomap( xp, p, 1 ) ) == 
		( XiePhotomap ) NULL )
	{
		reps = 0;
	}
	else
	{
		decode_notify = False;
		flo_elements = 2;
		flograph = XieAllocatePhotofloGraph(flo_elements);	
		if ( flograph == ( XiePhotoElement * ) NULL )
		{
			fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
			reps = 0;
		}
		else
		{
			XieFloImportPhotomap(&flograph[0], XIEPhotomap, 
				decode_notify);

			XieFloExportPhotomap(&flograph[1],
				1,              /* source phototag number */
				XIEPhotomap2,
				encode_tech,
				encode_params
			);

			flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );

			flo_notify = False;
		}
	}
	if ( !reps )
	{
		FreeImportPhotoStuff( xp, p );
	}
	return( reps );
}

int 
InitImportPhotoExportDrawable(XParms xp, Parms p, int reps)
{
	int	decode_notify;
	XIEimage *image;

	XIEPhotomap = ( XiePhotomap ) NULL;
	flo = ( XiePhotoflo ) NULL;
	flograph = ( XiePhotoElement * ) NULL;
	image = p->finfo.image1;

	if ( xp->screenDepth != image->depth[ 0 ] )
	{
		XIEPhotomap = GetXIEPointPhotomap( xp, p, 1, 1 << xp->screenDepth, False );
	}
	else
	{
		XIEPhotomap = GetXIEPhotomap( xp, p, 1 );
	}
 
	if ( XIEPhotomap == ( XiePhotomap ) NULL )
	{
		reps = 0;
	}
	else
	{
		decode_notify = False;
		flo_elements = 2;
		flograph = XieAllocatePhotofloGraph(flo_elements);	
		if ( flograph == ( XiePhotoElement * ) NULL )
		{
			fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
			reps = 0;
		}
		else
		{
			XieFloImportPhotomap(&flograph[0], XIEPhotomap, 
				decode_notify);

			XieFloExportDrawable(&flograph[1],
				1,              /* source phototag number */
				xp->w,
				xp->fggc,
				0,
				0
			);

			flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );

			flo_notify = False;
		}
	}
	if ( !reps )
	{
		FreeImportPhotoStuff( xp, p );
	}
	return( reps );
}

int 
InitImportLUT(XParms xp, Parms p, int reps)
{
	int lutSize, i;
        Bool    merge;
        XieLTriplet start;

	flograph = ( XiePhotoElement * ) NULL;
	lut = ( unsigned char * ) NULL;
	flo = ( XiePhotoflo ) NULL;
	XIELut = ( XieLut ) NULL;

	flo_elements = 2;	
	flograph = XieAllocatePhotofloGraph(flo_elements);	
	if ( flograph == ( XiePhotoElement * ) NULL )
	{
		fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
		reps = 0;
	}
	else
	{
		lutSize = xp->vinfo.colormap_size * sizeof( unsigned char );
		lut = (unsigned char *)malloc( lutSize );
		if ( lut == ( unsigned char * ) NULL )
		{
			reps = 0;
		}
		else
		{
			for ( i = 0; i < lutSize; i++ )
			{
				if ( i % 5 == 0 )
				{
					lut[ i ] = (  xp->vinfo.colormap_size - 1 ) - i;
				}
				else
				{
					lut[ i ] = i;
				}
			}
			if ( ( XIELut = 
				GetXIELut( xp, p, lut, lutSize, lutSize ) ) == 
				( XieLut ) NULL )
			{
				reps = 0;
			}
		}
	}

	if ( reps )
	{

		XieFloImportLUT(&flograph[0], XIELut );

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

		flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );

		flo_notify = False;
	}
	if ( !reps )
	{
		FreeImportLUTStuff( xp, p );
	}

	return( reps );
}

int 
InitImportROI(XParms xp, Parms p, int reps)
{
	int     rectsSize, i;

	flograph = ( XiePhotoElement * ) NULL;
	rects = ( XieRectangle * ) NULL;
	flo = ( XiePhotoflo ) NULL;
	XIERoi = ( XieRoi ) NULL;
	
	flo_elements = 2;
	flograph = XieAllocatePhotofloGraph(flo_elements);	
	if ( flograph == ( XiePhotoElement * ) NULL )
	{
		fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
		reps = 0;
	}
	else
	{
		rectsSize = (( ImportParms * ) p->ts )->numROIs;
		rects = (XieRectangle *)
			malloc( rectsSize * sizeof( XieRectangle ) );
		if ( rects == ( XieRectangle * ) NULL )
		{
			reps = 0;
		}
		else
		{
			/* who cares what the data is */

			for ( i = 0; i < rectsSize; i++ )
			{
				rects[ i ].x = i;
				rects[ i ].y = i;
				rects[ i ].width = i + 10;
				rects[ i ].height = i + 10;
			}
		}
		if ( ( XIERoi = 
			GetXIERoi( xp, p, rects, rectsSize ) ) == ( XieRoi ) NULL )
		{
			reps = 0;
		}
	}

	if ( reps )
	{

		XieFloImportROI(&flograph[0], XIERoi );

		XieFloExportROI(&flograph[1],
			1,              /* source phototag number */
			XIERoi
		);

		flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
		flo_notify = False;
	}

	if ( !reps )
	{
		FreeImportROIStuff( xp, p );
	}

	return( reps );
}

void 
DoImportPhoto(XParms xp, Parms p, int reps)
{
    	int     i;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
    	}
}

void 
DoImportDrawablePixmap(XParms xp, Parms p, int reps)
{
    	int     i;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
    	}
}

void 
DoImportDrawableWindow(XParms xp, Parms p, int reps)
{
    	int     i;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
    	}
	XClearWindow( xp->d, drawableWindow );
}

void 
DoImportDrawablePlanePixmap(XParms xp, Parms p, int reps)
{
    	int     i;

	for ( i = 0; i < reps; i++ ) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
    	}
}

void 
DoImportDrawablePlaneWindow(XParms xp, Parms p, int reps)
{
    	int     i;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
    	}
	XClearWindow( xp->d, drawableWindow );
}

void 
DoImportLUT(XParms xp, Parms p, int reps)
{
    	int     i;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
    	}
}

void 
DoImportROI(XParms xp, Parms p, int reps)
{
    	int     i;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
    	}
}

void 
EndImportLUT(XParms xp, Parms p)
{
	FreeImportLUTStuff( xp, p );
}

void 
EndImportPhoto(XParms xp, Parms p)
{
	FreeImportPhotoStuff( xp, p );
}

void 
EndImportROI(XParms xp, Parms p)
{
	FreeImportROIStuff( xp, p );
}

void 
EndImportDrawableWindow(XParms xp, Parms p)
{
	XUnmapWindow( xp->d, drawableWindow );
	dontClear = False;
	FreeImportDrawableWindowStuff( xp, p );
}

void 
EndImportDrawablePixmap(XParms xp, Parms p)
{
	FreeImportDrawablePixmapStuff( xp, p );
}

void
FreeImportPhotoStuff(XParms xp, Parms p)
{
	if ( XIEPhotomap && IsPhotomapInCache( XIEPhotomap ) == False )
	{
		XieDestroyPhotomap( xp->d, XIEPhotomap );
		XIEPhotomap = ( XiePhotomap ) NULL;
	}

	if ( XIEPhotomap2 )
	{
		XieDestroyPhotomap( xp->d, XIEPhotomap2 );
		XIEPhotomap2 = ( XiePhotomap ) NULL;
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
}

void
FreeImportDrawableWindowStuff(XParms xp, Parms p)
{
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
}

void
FreeImportDrawablePixmapStuff(XParms xp, Parms p)
{
	XFreePixmap( xp->d, myPixmap );
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
}

void
FreeImportLUTStuff(XParms xp, Parms p)
{
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
	
	if ( lut )
	{
		free( lut );
		lut = ( unsigned char * ) NULL;
	}

	if ( XIELut )
	{
		XieDestroyLUT( xp->d, XIELut );
		XIELut = ( XieLut ) NULL;
	}
}

void
FreeImportROIStuff(XParms xp, Parms p)
{
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
	
	if ( rects )
	{
		free( rects );
		rects = ( XieRectangle * ) NULL;
	}

	if ( XIERoi )
	{
		XieDestroyROI( xp->d, XIERoi );
		XIERoi = ( XieRoi ) NULL;
	}
}

