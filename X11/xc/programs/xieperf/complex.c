/* $Xorg: complex.c,v 1.4 2001/02/09 02:05:47 xorgcvs Exp $ */
/**** module complex.c ****/
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
  
	complex.c -- A complicated flo that does lots 'o stuff 

	Syd Logan -- AGE Logic, Inc. January 3, 1994 
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/complex.c,v 1.6 2001/12/14 20:01:47 dawes Exp $ */

#include "xieperf.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <math.h>

static XiePhotomap XIEPhotomap1;
static XiePhotomap XIEPhotomap2;
static XiePhotomap XIEPhotomap3;
static XiePhotomap XIEPhotomap4;
static XieLut XIELut1, XIELut2, XIELut3;

static XiePhotoElement *flograph;
static XiePhotoflo flo;
static int flo_elements;
static int cclass;
static char *techParms1;
static char *techParms2;
static char *techParms3;
extern Window drawableWindow;

#define NUMTILES 5

int 
InitComplex(XParms xp, Parms p, int reps)
{
#if     defined(__cplusplus) || defined(c_plusplus)
    	cclass = xp->vinfo.c_class;
#else
    	cclass = xp->vinfo.class;
#endif
	XIELut1 = ( XieLut ) NULL;
	XIELut2 = ( XieLut ) NULL;
	XIELut3 = ( XieLut ) NULL;
	techParms1 = ( char * ) NULL;
	techParms2 = ( char * ) NULL;
	techParms3 = ( char * ) NULL;
	flograph = ( XiePhotoElement * ) NULL;
	flo = ( XiePhotoflo ) NULL;

	XIEPhotomap1 = GetXIEPhotomap( xp, p, 1 );
	XIEPhotomap2 = GetXIEPhotomap( xp, p, 2 );
	XIEPhotomap3 = GetXIEPhotomap( xp, p, 4 );
	XIEPhotomap4 = GetXIEPhotomap( xp, p, 3 );
	if ( XIEPhotomap1 == ( XiePhotomap ) NULL || XIEPhotomap2 == 
	     ( XiePhotomap ) NULL || XIEPhotomap3 == ( XiePhotomap ) NULL ||
	     XIEPhotomap4 == ( XiePhotomap ) NULL )
		reps = 0; 
	if ( reps )
	{
        	if ( ( XIELut1 = CreatePointLut( xp, p, 256, 256, True ) ) 
                	== ( XieLut ) NULL )
		{
			reps = 0;
		}
		else if ( ( XIELut2 = CreatePointLut( xp, p, 8, 256, True ) )
   			== ( XieLut ) NULL )
		{
			reps = 0;
		}
	}

	if ( reps ) 
	{
		if ( !CreateComplexFlo( xp, p ) )
		{
			fprintf( stderr, "CreateComplexFlo failed\n" );
			reps = 0;
		}
	}

	if ( !reps )
	{
		FreeComplexStuff( xp, p );
	}
	else
	{
		XMoveWindow( xp->d, drawableWindow, 424, 120 );
                XMapRaised( xp->d, drawableWindow );
		XSync( xp->d, 0 );
	}
	return( reps );
}

int
CreateComplexFlo(XParms xp, Parms p)
{
	int	tech, idx, pasteSrc, lutSrc1, lutSrc2, lutSrc3;
	int	pntSrc1, pntSrc2, logSrc1, logSrc2, logSrc3, logSrc4;
	int	geoSrc1, geoSrc2, geoSrc3, geoSrc4, blendSrc1;
	int	selSrc1, selSrc2, selSrc3, mtchSrc1, mtchSrc2, ditherSrc1;
	int	selSrc1Prime, selSrc2Prime, selSrc3Prime;
	int	cstSrc1, cstSrc2, cstSrc3, tagSrc1;

	GeometryParms gp;
	XIEimage *image;
	XieLTriplet levels;
        float   coeffs[ 6 ];
        static XieConstant constant = { 0.0, 0.0, 0.0 };
	XieConstant logicalConstant;
	XieProcessDomain domain;
	XieConstant in_low,in_high;
	XieLTriplet out_low,out_high;
	static XieTile tiles[NUMTILES];
	int	pointflag;
	int	mylevels;
	int retval = 0;

	idx = 0;
	pointflag = 0;
	flo_elements = 39;
	image = p->finfo.image1;

	mylevels = 1 << image->depth[ 0 ];

       	if ( 1 << xp->screenDepth != mylevels )
        {
                pointflag++;
                flo_elements+=9;

                if ( ( XIELut3 = CreatePointLut( xp, p, mylevels,
                        1 << xp->screenDepth, False ) ) == ( XieLut ) NULL )
                {
                        return( 0 );
                }
        }

        flograph = XieAllocatePhotofloGraph( flo_elements );
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
                fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
		goto out;
        }
        XieFloImportPhotomap(&flograph[idx],XIEPhotomap1,False); /* 1 */
        idx++;	
        XieFloImportPhotomap(&flograph[idx],XIEPhotomap2,False); /* 2 */
        idx++;
        XieFloImportPhotomap(&flograph[idx],XIEPhotomap3,False); /* 3 */
        idx++;
        XieFloImportPhotomap(&flograph[idx],XIEPhotomap4,False); /* 4 */
        idx++;

	gp.geoType = GEO_TYPE_SCALE;
	gp.geoHeight = 256;
	gp.geoWidth = 256;
	gp.geoXOffset = 0;
	gp.geoYOffset = 0;

	if ( !SetCoefficients( xp, p, 4, &gp, coeffs ) )
		goto out;

        XieFloGeometry(&flograph[idx],			/* 5 */
	       	idx,
		256,
		256,
		coeffs,
		constant,
		0x1,
		xieValGeomNearestNeighbor,
		( char * ) NULL
	); idx++;
	geoSrc1 = idx;

        XieFloBandCombine( &flograph[ idx ], 1, 2, 3 ); /* 6 */
	idx++;

        XieFloGeometry(&flograph[idx],			/* 7 */
	       	idx,
		256,
		256,
		coeffs,
		constant,
		0x7,
		xieValGeomNearestNeighbor,
		( char * ) NULL
	); idx++;
	geoSrc2 = idx;

        XieFloBandSelect( &flograph[idx], idx, 0 ); idx++;	/* 8 */
	selSrc1 = selSrc1Prime = idx; 

	domain.offset_x = 0;
	domain.offset_y = 0;
	domain.phototag = 0;

	constant[ 0 ] = 120.0;
	XieFloCompare( &flograph[idx],			/* 9 */
		idx,
		0,
		&domain,
		constant,
		xieValLT,
		True,
		0x01
	);
	idx++;
	tagSrc1 = idx;

        XieFloBandSelect( &flograph[idx], 7, 1 ); idx++;	/* 10 */
	selSrc2 = selSrc2Prime = idx;

        XieFloBandSelect( &flograph[idx], 7, 2 ); idx++;	/* 11 */
	selSrc3 = selSrc3Prime = idx;

	lutSrc3 = 0; /* shut up gcc */

	if ( pointflag )
	{
                XieFloImportLUT(&flograph[idx], XIELut3 );
                idx++;
		lutSrc3 = idx;

                domain.phototag = 0;
                domain.offset_x = 0;
                domain.offset_y = 0;
                XieFloPoint(&flograph[idx],
                        selSrc1,
                        &domain,
                        lutSrc3,
                        0x7
                );
                idx++;
		selSrc1 = idx;

                XieFloPoint(&flograph[idx],
                        selSrc2,
                        &domain,
                        lutSrc3,
                        0x7
                );
                idx++;
		selSrc2 = idx;

                XieFloPoint(&flograph[idx],
                        selSrc3,
                        &domain,
                        lutSrc3,
                        0x7
                );
                idx++;
		selSrc3 = idx;
	}

        XieFloExportDrawable(&flograph[idx],		/* 12 */
                selSrc1,              /* source phototag number */
                xp->w,
                xp->fggc,
                0,
                0
        );
	idx++;

        XieFloExportDrawable(&flograph[idx],		/* 13 */
                selSrc2,              /* source phototag number */
                xp->w,
                xp->fggc,
                256,
                0
        );
	idx++;

        XieFloExportDrawable(&flograph[idx],		/* 14 */
                selSrc3,              /* source phototag number */
                xp->w,
                xp->fggc,
                0,
                256
        );
	idx++;

        XieFloImportLUT(&flograph[idx], XIELut1 );	/* 15 19 */
	idx++;
	lutSrc1 = idx;

        XieFloImportLUT(&flograph[idx], XIELut2 );	/* 16 */
	idx++;
	lutSrc2 = idx;

	domain.phototag = tagSrc1;
        XieFloMatchHistogram(&flograph[idx],		/* 17 */	
		geoSrc1,
		&domain,
		xieValHistogramFlat,
		( char * ) NULL
	);
	idx++;
	mtchSrc1 = mtchSrc2 = idx;

	levels[ 0 ] = 4;
	levels[ 1 ] = 8;
	levels[ 2 ] = 8;

	XieFloDither( &flograph[ idx ],			/* 18 */
		geoSrc2,
		0x07,	
		levels,
		xieValDitherErrorDiffusion,	
		( char * ) NULL	
	);
	idx++;

	domain.phototag = 0;
	XieFloPoint(&flograph[idx],			/* 19 */
		idx,
		&domain,
		lutSrc1,
		0x7
	);
	idx++;
	pntSrc1 = idx;

	logicalConstant[ 0 ] = 7.0;
	logicalConstant[ 1 ] = 0.0;
	logicalConstant[ 2 ] = 0.0;
	domain.phototag = 0;
	XieFloLogical(&flograph[idx],			/* 20 */
		pntSrc1,
		0,
		&domain,
		logicalConstant,
		GXand,
		0x01
	);
	idx++;
	logSrc1 = idx;

	logicalConstant[ 0 ] = 56.0;
	XieFloLogical(&flograph[idx],			/* 21 */
		pntSrc1,
		0,
		&domain,
		logicalConstant,
		GXand,
		0x01
	);
	idx++;
	logSrc2 = idx;

	logicalConstant[ 0 ] = 192.0;
	XieFloLogical(&flograph[idx],			/* 22 */
		pntSrc1,
		0,
		&domain,
		logicalConstant,
		GXand,
		0x01
	);
	idx++;
	logSrc3 = idx;

	levels[ 0 ] = 256;
	levels[ 1 ] = 0;
	levels[ 2 ] = 0;
	in_low[ 0 ] = 0.0;
	in_low[ 1 ] = 0.0;
	in_low[ 2 ] = 0.0;
	in_high[ 0 ] = 7.0;
	in_high[ 1 ] = 0.0;
	in_high[ 2 ] =  0.0;
	out_low[ 0 ] = 0;
	out_low[ 1 ] = 0;
	out_low[ 2 ] = 0;
	out_high[ 0 ] = 255;
	out_high[ 1 ] = 0;
	out_high[ 2 ] =	0;

	tech = xieValConstrainClipScale;
	techParms1 = ( char * ) XieTecClipScale( in_low, in_high, out_low, out_high);	
	if ( techParms1 == ( char * ) NULL )
		goto out;
	
	XieFloConstrain( &flograph[idx],		/* 23 */
		logSrc1,
		levels,
		tech,
		techParms1
	);
	idx++;
	cstSrc1 = idx;

	if ( pointflag )
	{
                domain.phototag = 0;
                domain.offset_x = 0;
                domain.offset_y = 0;
                XieFloPoint(&flograph[idx],
                        idx,
                        &domain,
                        lutSrc3,
                        0x1
                );
                idx++;
		cstSrc1 = idx;
        }

	in_high[ 0 ] = in_high[ 1 ] = in_high[ 2 ] = 56.0;
	techParms2 = ( char * ) XieTecClipScale( in_low, in_high, out_low, out_high);	
	if ( techParms2 == ( char * ) NULL )
		goto out;
	
	XieFloConstrain( &flograph[idx],		/* 24 */
		logSrc2,
		levels,
		tech,
		techParms2
	);
	idx++;
	cstSrc2 = idx;

	if ( pointflag )
	{
                domain.phototag = 0;
                domain.offset_x = 0;
                domain.offset_y = 0;
                XieFloPoint(&flograph[idx],
                        idx,
                        &domain,
                        lutSrc3,
                        0x1
                );
                idx++;
		cstSrc2 = idx;
        }

	in_high[ 0 ] = in_high[ 1 ] = in_high[ 2 ] = 192.0;
	techParms3 = ( char * ) XieTecClipScale( in_low, in_high, out_low, out_high);	
	if ( techParms3 == ( char * ) NULL )
		goto out;
	
	XieFloConstrain( &flograph[idx],		/* 25 */
		logSrc3,
		levels,
		tech,
		techParms3
	);
	idx++;
	cstSrc3 = idx;

	if ( pointflag )
	{
                domain.phototag = 0;
                domain.offset_x = 0;
                domain.offset_y = 0;
                XieFloPoint(&flograph[idx],
                        idx,
                        &domain,
                        lutSrc3,
                        0x1
                );
                idx++;
		cstSrc3 = idx;
        }

	XieFloExportDrawable(&flograph[idx],		/* 26 */
		cstSrc1,	
		xp->w,
		xp->fggc,
		256,       /* x offset in window */
		256        /* y offset in window */
	);
	idx++;

	XieFloExportDrawable(&flograph[idx],		/* 27 */
		cstSrc2,	
		drawableWindow,
		xp->fggc,
		0,       /* x offset in window */
		0        /* y offset in window */
	);
	idx++;

	XieFloExportDrawable(&flograph[idx],		/* 28 */
		cstSrc3,	
		drawableWindow,
		xp->fggc,
		256,       	/* x offset in window */
		0        	/* y offset in window */
	);
	idx++;

	if ( pointflag )
	{
                domain.phototag = 0;
                domain.offset_x = 0;
                domain.offset_y = 0;
                XieFloPoint(&flograph[idx],
                        mtchSrc2,
                        &domain,
                        lutSrc3,
                        0x1
                );
                idx++;
		mtchSrc2 = idx;
	}

	XieFloExportDrawable(&flograph[idx],		/* 29 */
		mtchSrc2,	
		drawableWindow,
		xp->fggc,
		0,       	/* x offset in window */
		256        	/* y offset in window */
	);
	idx++;

        gp.geoType = GEO_TYPE_SCALE;
        gp.geoHeight = 256;
        gp.geoWidth = 256;
        gp.geoXOffset = 0;
        gp.geoYOffset = 0;

        if ( !SetCoefficients( xp, p, 1, &gp, coeffs ) )
		goto out;

	constant[ 0 ] = 0.0;
        XieFloGeometry(&flograph[idx],  		/* 30 */
                selSrc1Prime,
                128,
                128,
                coeffs,
                constant,
                0x1,
                xieValGeomNearestNeighbor,
                ( char * ) NULL
        ); idx++;

	levels[ 0 ] = 8;
	levels[ 1 ] = 0;
	levels[ 2 ] = 0;

	XieFloDither( &flograph[ idx ],			/* 31 */
		idx,
		0x01,	
		levels,
		xieValDitherErrorDiffusion,	
		( char * ) NULL	
	);
	idx++;
	ditherSrc1 = idx;

	domain.phototag = 0;
	XieFloPoint(&flograph[idx],			/* 32 */
		idx,
		&domain,
		lutSrc2,
		0x1
	);
	idx++;
	pntSrc2 = idx;
	
        if ( !SetCoefficients( xp, p, 2, &gp, coeffs ) )
		goto out;

        XieFloGeometry(&flograph[idx],  		/* 33 */
                selSrc2Prime,
                128,
                128,
                coeffs,
                constant,
                0x1,
                xieValGeomNearestNeighbor,
                ( char * ) NULL
        ); idx++;
	geoSrc3 = idx;

	logicalConstant[ 0 ] = 7.0;
	logicalConstant[ 1 ] = 0.0;
	logicalConstant[ 2 ] = 0.0;
	domain.phototag = 0;
	XieFloLogical(&flograph[idx],			/* 34 */
		idx,
		0,
		&domain,
		logicalConstant,
		GXinvert,
		0x01
	);
	idx++;
	logSrc4 = idx;

	gp.geoType = GEO_TYPE_SCALEDROTATE;
	gp.geoHeight = 256;
	gp.geoWidth = 256;
	gp.geoXOffset = 0;
	gp.geoYOffset = 0;
	gp.geoAngle = 12.0;

	if ( !SetCoefficients( xp, p, 3, &gp, coeffs ) )
		goto out;

        XieFloGeometry(&flograph[idx],			/* 35 */
	       	selSrc3Prime,
		128,
		128,
		coeffs,
		constant,
		0x7,
		xieValGeomNearestNeighbor,
		( char * ) NULL
	); idx++;
	geoSrc4 = idx;

	constant[ 0 ] = 256.0;
	constant[ 1 ] = 0.0;
	constant[ 2 ] = 0.0;

	XieFloBlend(&flograph[idx],			/* 36 */
		pntSrc2,
		geoSrc3,
		constant,
		idx,	
		256.0,
		&domain,
		0x01 );
	idx++;
	blendSrc1 = idx;

	gp.geoType = GEO_TYPE_SCALE;
	gp.geoHeight = 256;
	gp.geoWidth = 256;
	gp.geoXOffset = 0;
	gp.geoYOffset = 0;

	if ( !SetCoefficients( xp, p, 4, &gp, coeffs ) )
		goto out;

	constant[ 0 ] = 0.0;
	constant[ 1 ] = 0.0;
	constant[ 2 ] = 0.0;

        XieFloGeometry(&flograph[idx],			/* 37 */
	       	mtchSrc1,
		128,
		128,
		coeffs,
		constant,
		0x7,
		xieValGeomNearestNeighbor,
		( char * ) NULL
	); idx++;
	mtchSrc1 = idx;

	if ( pointflag && 0)
	{
                domain.phototag = 0;
                domain.offset_x = 0;
                domain.offset_y = 0;
                XieFloPoint(&flograph[idx],
                        geoSrc4,
                        &domain,
                        lutSrc3,
                        0x1
                );
		idx++;
		geoSrc4 = idx;
	}

	tiles[ 0 ].src = mtchSrc1;
	tiles[ 0 ].dst_x = 0;
	tiles[ 0 ].dst_y = 0;
	tiles[ 1 ].src = pntSrc2;
	tiles[ 1 ].dst_x = 0;
	tiles[ 1 ].dst_y = 128;
	tiles[ 2 ].src = logSrc4;
	tiles[ 2 ].dst_x = 128;
	tiles[ 2 ].dst_y = 0;
	tiles[ 3 ].src = geoSrc4;
	tiles[ 3 ].dst_x = 128;
	tiles[ 3 ].dst_y = 128;
	tiles[ 4 ].src = blendSrc1;
	tiles[ 4 ].dst_x = 64;
	tiles[ 4 ].dst_y = 64;

        XieFloPasteUp( &flograph[idx], 256, 256, constant, tiles, 5 ); /* 38 */
	pasteSrc = idx;
	idx++;

	if ( pointflag )
	{
                domain.phototag = 0;
                domain.offset_x = 0;
                domain.offset_y = 0;
                XieFloPoint(&flograph[idx],
                        idx,
                        &domain,
			lutSrc3,
                        0x7
                );
                idx++;
        }

        XieFloExportDrawable(&flograph[idx],		/* 39 */
                idx,              /* source phototag number */
		drawableWindow,
                xp->fggc,
                256,
                256
        );
	idx++;

        flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );

        XieFreePasteUpTiles(&flograph[pasteSrc]);
	retval = 1;
out:
	if ( techParms1 )
	{
		XFree( techParms1 );
		techParms1 = ( char * ) NULL;
	}
	if ( techParms2 )
	{
		XFree( techParms2 );
		techParms2 = ( char * ) NULL;
	}
	if ( techParms3 )
	{
		XFree( techParms3 );
		techParms3 = ( char * ) NULL;
	}
	return( retval );
}

void 
DoComplex(XParms xp, Parms p, int reps)
{
	int	i;

	for ( i = 0; i < reps; i++ )
	{
                XieExecutePhotoflo(xp->d, flo, True );
		WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo, 0, False );
        }
	XClearWindow( xp->d, drawableWindow );
	XSync( xp->d, 0 );
}

void 
EndComplex(XParms xp, Parms p)
{
        XUnmapWindow( xp->d, drawableWindow );
	FreeComplexStuff( xp, p );
}

void
FreeComplexStuff(XParms xp, Parms p)
{
	if ( XIEPhotomap1 && IsPhotomapInCache( XIEPhotomap1 ) == False )
	{
		XieDestroyPhotomap(xp->d, XIEPhotomap1);
		XIEPhotomap1 = ( XiePhotomap ) NULL;
	}

	if ( XIEPhotomap2 && IsPhotomapInCache( XIEPhotomap2 ) == False )
	{
		XieDestroyPhotomap(xp->d, XIEPhotomap2);
		XIEPhotomap2 = ( XiePhotomap ) NULL;
	}

	if ( XIEPhotomap3 && IsPhotomapInCache( XIEPhotomap3 ) == False )
	{
		XieDestroyPhotomap(xp->d, XIEPhotomap3);
		XIEPhotomap3 = ( XiePhotomap ) NULL;
	}

	if ( XIEPhotomap4 && IsPhotomapInCache( XIEPhotomap4 ) == False )
	{
		XieDestroyPhotomap(xp->d, XIEPhotomap4);
		XIEPhotomap4 = ( XiePhotomap ) NULL;
	}

	if ( XIELut1 )
	{
		XieDestroyLUT(xp->d, XIELut1);
		XIELut1 = ( XieLut ) NULL;
	}

	if ( XIELut2 )
	{
		XieDestroyLUT(xp->d, XIELut2);
		XIELut2 = ( XieLut ) NULL;
	}

	if ( XIELut3 )
	{
		XieDestroyLUT(xp->d, XIELut3);
		XIELut3 = ( XieLut ) NULL;
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
