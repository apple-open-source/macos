/* $Xorg: modify.c,v 1.4 2001/02/09 02:05:47 xorgcvs Exp $ */

/**** module modify.c ****/
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
  
	modify.c -- modify flo element tests 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/modify.c,v 3.9 2001/12/14 20:01:50 dawes Exp $ */

#include "xieperf.h"
#include <stdio.h>

static int BuildModifyROIFlograph ( XParms xp, Parms p, 
				    XiePhotoElement **flograph );
static int BuildModifyPointFlograph ( XParms xp, Parms p, 
				      XiePhotoElement **flograph );
static int BuildModifySimpleFlograph ( XParms xp, Parms p, 
				       XiePhotoElement **flograph );
static int BuildModifyLong1Flograph ( XParms xp, Parms p, 
				      XiePhotoElement **flograph );
static int BuildModifyLong2Flograph ( XParms xp, Parms p, 
				      XiePhotoElement **flograph );

static XiePhotomap XIEPhotomap;
static XieRoi XIERoi1, XIERoi2;
static XieLut XIELut1, XIELut2;
static XiePhotoElement *flograph;
static XiePhotoflo flo;
static int flo_elements;

static XieLut XIELut;
static int monoflag = 0;
static XieRectangle *rects;
static unsigned char *lut; 

int 
InitModifyROI(XParms xp, Parms p, int reps)
{
	XIEimage *image;	
        int rectsSize, i;

	monoflag = 0;
	XIELut = ( XieLut ) NULL;
	rects = (XieRectangle *) NULL;
	XIERoi1 = ( XieRoi ) NULL;
	XIERoi2 = ( XieRoi ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;

	image = p->finfo.image1;
	if ( !image )
		reps = 0;
	else if ( xp->screenDepth != image->depth[ 0 ] )
        {
		monoflag = 1;
	        if ( ( XIELut = CreatePointLut( xp, p, 1 << image->depth[ 0 ],
			1 << xp->screenDepth, False ) ) == ( XieLut ) NULL )
		{
			reps = 0;
		}
        }
	if ( reps )
	{
		rectsSize = 4;
		rects = (XieRectangle *)
			malloc( rectsSize * sizeof( XieRectangle ) );
		if ( rects == ( XieRectangle * ) NULL )
		{
			reps = 0;
		}
		else
		{
			for ( i = 0; i < rectsSize; i++ )
			{
				rects[ i ].x = i * 100;
				rects[ i ].y = i * 100;
				rects[ i ].width = 50;
				rects[ i ].height = 50;
			}
			if ( ( XIERoi1 = GetXIERoi( xp, p, rects, rectsSize ) ) 
				== ( XieRoi ) NULL )
			{
				reps = 0;
			}
			else
			{
				for ( i = 0; i < rectsSize; i++ )
				{
					rects[ i ].x = i * 100 + 50;
					rects[ i ].y = i * 100 + 50;
					rects[ i ].width = 50;
					rects[ i ].height = 50;
				}
				if ( ( XIERoi2 = GetXIERoi( xp, p, rects, 
					rectsSize ) ) == ( XieRoi ) NULL )
				{
					reps = 0;
				}
			}
		}
	}
	if ( reps )
	{
		if ( ( XIEPhotomap = GetXIEPhotomap( xp, p, 1 ) ) == 
			( XiePhotomap ) NULL )
		{
			reps = 0;
		}
		else if ( !BuildModifyROIFlograph( xp, p, &flograph ) )
		{
			reps = 0;
		}
	}
	if ( !reps )
		FreeModifyROIStuff( xp, p );	
        return( reps );
}

static int
BuildModifyROIFlograph(XParms xp, Parms p, XiePhotoElement **flograph )
{
	XieProcessDomain domain;

        if ( monoflag )
                flo_elements = 6;
        else
                flo_elements = 4;
	*flograph = XieAllocatePhotofloGraph(flo_elements);	
	if ( *flograph == ( XiePhotoElement * ) NULL )
	{
		fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
		return( 0 );
	}

	XieFloImportPhotomap(&(*flograph)[0], XIEPhotomap, False);

	XieFloImportROI(&(*flograph)[1], XIERoi1); /* as good a ROI as any */

	domain.offset_x = 0;
	domain.offset_y = 0;
	domain.phototag = 2;

	XieFloLogical(&(*flograph)[2], 
		1,
		0,
		&domain,
		((ModifyParms * )p->ts)->constant,
		((ModifyParms * )p->ts)->op,
		((ModifyParms * )p->ts)->bandMask );

        if ( monoflag )
        {
                XieFloImportLUT(&(*flograph)[3], XIELut );

		domain.phototag = 0;
                XieFloPoint(&(*flograph)[4],
                        3,
                        &domain,
                        4,
                        0x1
                );
	}

	XieFloExportDrawable(&(*flograph)[flo_elements - 1],
		flo_elements - 1,       /* source phototag number */
		xp->w,
		xp->fggc,
		0,       /* x offset in window */
		0        /* y offset in window */
	);

	flo = XieCreatePhotoflo( xp->d, *flograph, flo_elements );
	return( 1 );
}

void 
DoModifyROI(XParms xp, Parms p, int reps)
{
    	int     i;
	int	flo_notify;
	int	toggle;

	flo_notify = False;	
	toggle = 0;

	for ( i = 0; i != reps; i++ )
	{
                XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
		if ( !toggle )
		{
			toggle = 1;
			XieFloImportROI(&flograph[1], XIERoi2);
		}
		else
		{
			toggle = 0;
			XieFloImportROI(&flograph[1], XIERoi1);
		}
		XieModifyPhotoflo( xp->d, flo, 2, &flograph[1], 1 );
		XSync( xp->d, 0 );
    	}
}

void
EndModifyROI(XParms xp, Parms p)
{
	FreeModifyROIStuff( xp, p );
}

void
FreeModifyROIStuff(XParms xp, Parms p)
{
	if ( XIELut )
	{
		XieDestroyLUT( xp->d, XIELut );
		XIELut = ( XieLut ) NULL;
	}
	if ( rects )
	{
		free( rects );
		rects = (XieRectangle *) NULL;
	}
	if ( XIERoi1 )
	{
		XieDestroyROI( xp->d, XIERoi1 );
		XIERoi1 = ( XieRoi ) NULL;
	}
	if ( XIERoi2 )
	{
		XieDestroyROI( xp->d, XIERoi2 );
		XIERoi2 = ( XieRoi ) NULL;
	}
	if ( XIEPhotomap && IsPhotomapInCache( XIEPhotomap ) == False )
	{
		XieDestroyPhotomap( xp->d, XIEPhotomap );
		XIEPhotomap = ( XiePhotomap ) NULL;
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

int 
InitModifyPoint(XParms xp, Parms p, int reps)
{
        int     lutSize, i;
	XIEimage *image;

	lut = ( unsigned char * ) lut;
	XIELut1 = ( XieLut ) NULL;
	XIELut2 = ( XieLut ) NULL;
	XIELut = ( XieLut ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;
	image = p->finfo.image1;
	if ( !image )
		return( 0 );

        lutSize = xp->vinfo.colormap_size;
        lut = (unsigned char *)malloc( lutSize * sizeof( unsigned char ) );
        if ( lut == ( unsigned char * ) NULL )
		reps = 0;
	else
	{
		for ( i = 0; i < lutSize; i++ )
			if ( i % 2 )
				lut[ i ] = 1;
			else
				lut[ i ] = 0;
		if ( ( XIELut1 = GetXIELut( xp, p, lut, lutSize, 2 ) ) == 
			( XieLut ) NULL )
		{
			reps = 0;
		}
		else
		{
			for ( i = 0; i < lutSize; i++ )
				if ( i % 2 )
					lut[ i ] = 0;
				else
					lut[ i ] = 1;
			if ( ( XIELut2 = GetXIELut( xp, p, lut, lutSize, 2 ) ) 
				== ( XieLut ) NULL )
			{
				reps = 0;
			}
		}
	}

	if ( reps )
	{
		if ( image->depth[ 0 ] == xp->screenDepth )
		{
			XIEPhotomap = GetXIEPhotomap( xp, p, 1 );	
		} 
		else if ( !IsDISServer() )
		{
			XIEPhotomap = GetXIEDitheredPhotomap( xp, p, 1, 2 );
		}	
		else 
		{
			XIEPhotomap = GetXIEPointPhotomap( xp, p, 1,
				xp->screenDepth, False );
		}
		if ( XIEPhotomap == ( XiePhotomap ) NULL )
		{
			reps = 0;
		}
	}

	if ( reps )
	{
		if ( !BuildModifyPointFlograph( xp, p, &flograph ) )
		{
			reps = 0;
		}
	}

	if ( !reps )
	{
		FreeModifyPointStuff( xp, p );
	}	
        return( reps );
}

static int
BuildModifyPointFlograph(XParms xp, Parms p, XiePhotoElement **flograph )
{
	XieProcessDomain domain;
        int     band_mask = 1;

        flo_elements = 4;
	*flograph = XieAllocatePhotofloGraph(flo_elements);	
	if ( *flograph == ( XiePhotoElement * ) NULL )
	{
		fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
		return( 0 );
	}

	XieFloImportPhotomap(&(*flograph)[0], XIEPhotomap, False);

	XieFloImportLUT(&(*flograph)[1], XIELut1); /* as good a LUT as any */

	domain.offset_x = 0;
	domain.offset_y = 0;
	domain.phototag = 0;

	XieFloPoint(&(*flograph)[2], 1, &domain, 2, band_mask ); 

	XieFloExportDrawablePlane(&(*flograph)[3],
		3,       /* source phototag number */
		xp->w,
		xp->fggc,
		0,       /* x offset in window */
		0        /* y offset in window */
	);

	flo = XieCreatePhotoflo( xp->d, *flograph, flo_elements );
	return( 1 );
}

void 
DoModifyPoint(XParms xp, Parms p, int reps)
{
    	int     i;
	int	flo_notify;
	int	toggle;

	flo_notify = False;	
	toggle = 0;

	for ( i = 0; i != reps; i++ )
	{
                XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
		if ( !toggle )
		{
			toggle = 1;
			XieFloImportLUT(&flograph[1], XIELut2);
		}
		else
		{
			toggle = 0;
			XieFloImportLUT(&flograph[1], XIELut1);
		}
		XieModifyPhotoflo( xp->d, flo, 2, &flograph[1], 1 );
		XSync( xp->d, 0 );
    	}
}

void
EndModifyPoint(XParms xp, Parms p)
{
	FreeModifyPointStuff( xp, p );
}

void
FreeModifyPointStuff(XParms xp, Parms p)
{
	if ( lut )
	{
		free( lut );
		lut = ( unsigned char * ) NULL;
	}
	if ( XIEPhotomap && IsPhotomapInCache( XIEPhotomap ) == False )
	{
		XieDestroyPhotomap( xp->d, XIEPhotomap );
		XIEPhotomap = ( XiePhotomap ) NULL;
	}
	if ( XIELut1 )
	{
		XieDestroyLUT( xp->d, XIELut1 );
		XIELut1 = ( XieLut ) NULL;
	}
	if ( XIELut1 )
	{
		XieDestroyLUT( xp->d, XIELut2 );
		XIELut2 = ( XieLut ) NULL;
	}
	if ( XIELut )
	{
		XieDestroyLUT( xp->d, XIELut );
		XIELut = ( XieLut ) NULL;
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

int 
InitModifySimple(XParms xp, Parms p, int reps)
{
	XIEimage *image;

	monoflag = 0;
	XIEPhotomap = ( XiePhotomap ) NULL;
       	flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;
	XIELut = ( XieLut ) NULL;

	image = p->finfo.image1;
        if ( !image )
		reps = 0;
        else if ( xp->screenDepth != image->depth[ 0 ] )
        {
		monoflag = 1;
        	if ( ( XIELut = CreatePointLut( xp, p, 1 << image->depth[ 0 ],
			1 << xp->screenDepth, False ) ) == ( XieLut ) NULL )
		{
			reps = 0;
		}
        }

	if ( reps )
	{
		if ( ( XIEPhotomap = GetXIEPhotomap( xp, p, 1 ) ) 
			== ( XiePhotomap ) NULL )
		{
			reps = 0;
		}
		else if ( !BuildModifySimpleFlograph( xp, p, &flograph ) )
		{
			reps = 0;
		}
	}
	if ( !reps )
		FreeModifySimpleStuff( xp, p );
        return( reps );
}

static int
BuildModifySimpleFlograph(XParms xp, Parms p, XiePhotoElement **flograph)
{
	XieProcessDomain domain;

	if ( monoflag )
		flo_elements = 4;
	else
        	flo_elements = 2;
	*flograph = XieAllocatePhotofloGraph(flo_elements);	
	if ( *flograph == ( XiePhotoElement * ) NULL )
	{
		fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
		return( 0 );
	}

	XieFloImportPhotomap(&(*flograph)[0], XIEPhotomap, False);

        if ( monoflag )
        {
	        XieFloImportLUT(&(*flograph)[1], XIELut );

		domain.offset_x = 0;
		domain.offset_y = 0;
		domain.phototag = 0;

		XieFloPoint(&(*flograph)[2],
			1,
			&domain,
			2,
			0x1
		);
        }

	XieFloExportDrawable(&(*flograph)[flo_elements - 1],
		flo_elements - 1,       /* source phototag number */
		xp->w,
		xp->fggc,
		0,       /* x offset in window */
		0        /* y offset in window */
	);

	flo = XieCreatePhotoflo( xp->d, *flograph, flo_elements );
	return( 1 );
}

void 
DoModifySimple(XParms xp, Parms p, int reps)
{
    	int     i;
	int	flo_notify;
	int	toggle;

	flo_notify = False;	
	toggle = 0;

	for ( i = 0; i != reps; i++ )
	{
                XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
		if ( !toggle )
		{
			toggle = 1;
			XieFloExportDrawable(&flograph[ flo_elements - 1],
				flo_elements - 1,  /* source phototag number */
				xp->w,
				xp->fggc,
				0,       /* x offset in window */
				0        /* y offset in window */
			);
		}
		else
		{
			toggle = 0;
			XieFloExportDrawable(&flograph[ flo_elements - 1],
				flo_elements - 1,  /* source phototag number */
				xp->w,
				xp->fggc,
				100,       /* x offset in window */
				100        /* y offset in window */
			);
		}
		XClearWindow( xp->d, xp->w );
		XieModifyPhotoflo( xp->d, flo, flo_elements, 
			&flograph[flo_elements - 1], 1 );
		XSync( xp->d, 0 );
    	}
}

void
EndModifySimple(XParms xp, Parms p)
{
	FreeModifySimpleStuff( xp, p );
}

void
FreeModifySimpleStuff(XParms xp, Parms p)
{
        if ( XIEPhotomap && IsPhotomapInCache( XIEPhotomap ) == False )
        {
                XieDestroyPhotomap( xp->d, XIEPhotomap );
                XIEPhotomap = ( XiePhotomap ) NULL;
        }
        if ( XIELut ) 
        {
                XieDestroyLUT( xp->d, XIELut );
                XIELut = ( XieLut ) NULL;
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

int 
InitModifyLong1(XParms xp, Parms p, int reps)
{
	XIEimage *image;
	GeometryParms gp;

	monoflag = 0;
	XIEPhotomap = ( XiePhotomap ) NULL;
       	flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;
	XIELut = ( XieLut ) NULL;
	
	image = p->finfo.image1;
        if ( !image )
		reps = 0;
        else if ( xp->screenDepth != image->depth[ 0 ] )
        {
		monoflag = 1;
        	if ( ( XIELut = CreatePointLut( xp, p, 1 << image->depth[ 0 ],
			1 << xp->screenDepth, False ) ) == ( XieLut ) NULL )
		{
			reps = 0;
		}
        }

	if ( reps )
	{
		gp.geoType = GEO_TYPE_SCALE;
		gp.geoHeight = 64;
		gp.geoWidth = 128;
		gp.geoXOffset = 0;
		gp.geoYOffset = 0;
		gp.geoTech = xieValGeomNearestNeighbor;

		if ( ( XIEPhotomap = GetXIEGeometryPhotomap( xp, p, &gp, 1 ) ) 
			== ( XiePhotomap ) NULL )
		{
			reps = 0;
		}
		else if ( !BuildModifyLong1Flograph( xp, p, &flograph ) )
		{
			reps = 0;
		}
	}
	if ( !reps )
		FreeModifyLongStuff( xp, p );
        return( reps );
}

static int
BuildModifyLong1Flograph(XParms xp, Parms p, XiePhotoElement **flograph)
{
	int	i, idx;
        XieProcessDomain domain;
	XieConstant constant;
	int	band_mask;

	idx = 0;
	band_mask = 7;
	flo_elements = 10;
	if ( monoflag )
		flo_elements+=2;
	*flograph = XieAllocatePhotofloGraph(flo_elements);	
	if ( *flograph == ( XiePhotoElement * ) NULL )
	{
		fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
		return( 0 );
	}

	XieFloImportPhotomap(&(*flograph)[idx], XIEPhotomap, False);
	idx++;

	/* now, a bunch of silly monadic arithmetic elements */

     	domain.offset_x = 0;
	domain.offset_y = 0;
	domain.phototag = 0;

	/* just add 1 */

	constant[ 0 ] = 1.0; 
	constant[ 1 ] = 0.0; 
	constant[ 2 ] = 0.0; 

	for ( i = 0; i < flo_elements - 2 - ( monoflag ? 2 : 0 ); i++ )
	{
		XieFloArithmetic(&(*flograph)[idx],
			idx,
			0,
			&domain,
			constant,
			xieValAdd,
			band_mask
		);
		idx++;
	}

	if ( monoflag )
	{
	        XieFloImportLUT(&(*flograph)[idx], XIELut );
		idx++;

		XieFloPoint(&(*flograph)[idx],
			idx - 1,
			&domain,
			idx,
			0x1
		);
		idx++;
	}
	XieFloExportDrawable(&(*flograph)[idx],
		idx,     /* source phototag number */
		xp->w,
		xp->fggc,
		0,       /* x offset in window */
		0        /* y offset in window */
	);

	flo = XieCreatePhotoflo( xp->d, *flograph, flo_elements );
	return( 1 );
}

#ifdef WIN32
#define RAND( x, y ) ( ( rand() / ((RAND_MAX + 1) / 2.0) ) * ( y - x ) + x )
#else
#include <math.h>
#include <stdlib.h>
#if defined(SYSV) || defined(SVR4) || defined(__osf__) || defined(CSRG_BASED)
#define random lrand48
#endif
#define RAND( x, y ) ( ( random() / 2147483648.0 ) * ( y - x ) + x )
#endif

void 
DoModifyLong1(XParms xp, Parms p, int reps)
{
    	int     x, y, i, flo_notify;

	flo_notify = False;	
	for ( i = 0; i != reps; i++ )
	{
                XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
		x = RAND( -64, WIDTH - 64 );
		y = RAND( -32, HEIGHT - 32 );
		XieFloExportDrawable(&flograph[flo_elements - 1],
			flo_elements - 1, /* source phototag number */
			xp->w,
			xp->fggc,
			x,      /* x offset in window */
			y       /* y offset in window */
		);
		XieModifyPhotoflo( xp->d, flo, flo_elements, 
			&flograph[flo_elements - 1], 1 );
		XSync( xp->d, 0 );
    	}
}

void
EndModifyLong(XParms xp, Parms p)
{
	FreeModifyLongStuff( xp, p );
}

void
FreeModifyLongStuff(XParms xp, Parms p)
{
        if ( XIEPhotomap && IsPhotomapInCache( XIEPhotomap ) == False )
        {
                XieDestroyPhotomap( xp->d, XIEPhotomap );
                XIEPhotomap = ( XiePhotomap ) NULL;
        }
        if ( XIELut ) 
        {
                XieDestroyLUT( xp->d, XIELut );
                XIELut = ( XieLut ) NULL;
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

int 
InitModifyLong2(XParms xp, Parms p, int reps)
{
	XIEimage *image;

	monoflag = 0;
	XIEPhotomap = ( XiePhotomap ) NULL;
       	flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;
	XIELut = ( XieLut ) NULL;
	
	image = p->finfo.image1;
        if ( !image )
		reps = 0;
        else if ( xp->screenDepth != image->depth[ 0 ] )
        {
		monoflag = 1;
        	if ( ( XIELut = CreatePointLut( xp, p, 1 << image->depth[ 0 ],
			1 << xp->screenDepth, False ) ) == ( XieLut ) NULL )
		{
			reps = 0;
		}
        }

	if ( reps )
	{
		if ( ( XIEPhotomap = GetXIEPhotomap( xp, p, 1 ) ) 
			== ( XiePhotomap ) NULL )
		{
			reps = 0;
		}
		else if ( !BuildModifyLong2Flograph( xp, p, &flograph ) )
		{
			reps = 0;
		}
	}
	if ( !reps )
		FreeModifyLongStuff( xp, p );
        return( reps );
}

static int
BuildModifyLong2Flograph(XParms xp, Parms p, XiePhotoElement **flograph)
{
	int i, idx;
        XieProcessDomain domain;
	XieConstant constant;
	float coeffs[ 6 ];
	int band_mask;
	GeometryParms gp;

	idx = 0;
	band_mask = 7;
	flo_elements = 11;
	if ( monoflag )
		flo_elements+=2;
	*flograph = XieAllocatePhotofloGraph(flo_elements);	
	if ( *flograph == ( XiePhotoElement * ) NULL )
	{
		fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
		return( 0 );
	}

	XieFloImportPhotomap(&(*flograph)[idx], XIEPhotomap, False);
	idx++;

	/* now, a bunch of silly monadic arithmetic elements */

     	domain.offset_x = 0;
	domain.offset_y = 0;
	domain.phototag = 0;

	/* just add 1 */

	constant[ 0 ] = 1.0; 
	constant[ 1 ] = 0.0; 
	constant[ 2 ] = 0.0; 

	for ( i = 0; i < flo_elements - 3 - ( monoflag ? 2 : 0 ); i++ )
	{
		XieFloArithmetic(&(*flograph)[idx],
			idx,
			0,
			&domain,
			constant,
			xieValAdd,
			band_mask
		);
		idx++;
	}

	if ( monoflag )
	{
	        XieFloImportLUT(&(*flograph)[idx], XIELut );
		idx++;

		XieFloPoint(&(*flograph)[idx],
			idx - 1,
			&domain,
			idx,
			0x1
		);
		idx++;
	}

	constant[ 0 ] = 0.0;
	constant[ 1 ] = 0.0;
	constant[ 2 ] = 0.0;

	gp.geoType = GEO_TYPE_CROP;
	gp.geoHeight = 64;
	gp.geoWidth = 128;
	gp.geoXOffset = 0;
	gp.geoYOffset = 0;
	gp.geoTech = xieValGeomNearestNeighbor;

        SetCoefficients( xp, p, 1, &gp, coeffs );
        XieFloGeometry(&(*flograph)[idx],
                idx,
                gp.geoWidth,
                gp.geoHeight,
                coeffs,
                constant,
                7,
                gp.geoTech,
                ( char * ) NULL
        );
	idx++;

	XieFloExportDrawable(&(*flograph)[idx],
		idx,     /* source phototag number */
		xp->w,
		xp->fggc,
		0,       /* x offset in window */
		0        /* y offset in window */
	);

	flo = XieCreatePhotoflo( xp->d, *flograph, flo_elements );
	return( 1 );
}

void 
DoModifyLong2(XParms xp, Parms p, int reps)
{
    	int     x, y, i, flo_notify;
	GeometryParms gp;
	XieConstant constant;
	float coeffs[ 6 ];

	constant[ 0 ] = 0.0;
	constant[ 1 ] = 0.0;
	constant[ 2 ] = 0.0;

	gp.geoType = GEO_TYPE_CROP;
	gp.geoWidth = 128;
	gp.geoHeight = 64;
	gp.geoTech = xieValGeomNearestNeighbor;

	flo_notify = False;	
	for ( i = 0; i != reps; i++ )
	{
                XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
		x = ( ( int ) RAND( 0, WIDTH ) >> 7 ) << 7;
		y = ( ( int ) RAND( 0, HEIGHT ) >> 6 ) << 6;

		gp.geoXOffset = x;
		gp.geoYOffset = y;

        	SetCoefficients( xp, p, 1, &gp, coeffs );
        	XieFloGeometry(&flograph[flo_elements - 2],
                	flo_elements - 2,
                	gp.geoWidth,
			gp.geoHeight,
			coeffs,
			constant,
			7,
			gp.geoTech,
			( char * ) NULL
		);

		XieFloExportDrawable(&flograph[flo_elements - 1],
			flo_elements - 1, /* source phototag number */
			xp->w,
			xp->fggc,
			x,      /* x offset in window */
			y       /* y offset in window */
		);
		XieModifyPhotoflo( xp->d, flo, flo_elements - 1, 
			&flograph[flo_elements - 2], 2 );
		XSync( xp->d, 0 );
    	}
}
