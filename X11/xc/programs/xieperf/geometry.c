/* $Xorg: geometry.c,v 1.4 2001/02/09 02:05:47 xorgcvs Exp $ */

/**** module geometry.c ****/
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
  
	geometry.c -- geometry flo element tests 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/geometry.c,v 1.6 2001/12/14 20:01:49 dawes Exp $ */

#include "xieperf.h"
#include <stdio.h>
#include <math.h>
#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

static XiePhotomap XIEPhotomap;
static XieLut	XIELut, XIELut2;

static int constrainflag = 0;
static int drawableplaneflag = 0;

static XieDecodeG42DParam *G42Ddecode_params=NULL;
static XieDecodeG32DParam *G32Ddecode_params=NULL;
static XieDecodeG31DParam *G31Ddecode_params=NULL;
static XieDecodeTIFFPackBitsParam *TIFFPackBitsdecode_params=NULL;
static XieDecodeTIFF2Param *TIFF2decode_params=NULL;

static XiePhotoElement *flograph;
static XiePhotoflo flo;
static int flo_elements;
static int flo_notify;
static int size;
static XIEimage *image;

int 
InitGeometry(XParms xp, Parms p, int reps)
{
	int     band_mask = 1;
	int	idx = 0;
	float	coeffs[ 6 ];
	static XieConstant constant = { 0.0, 0.0, 0.0 };
	XieProcessDomain domain;
	XieGeometryTechnique geo_tech = ( XieGeometryTechnique ) NULL;
	char	*geo_tech_params = NULL;

	XIELut = ( XieLut ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;
	flograph = ( XiePhotoElement * ) NULL;
	flo = ( XiePhotoflo ) NULL;
	
	image = p->finfo.image1;
	if ( !image )
		return 0;

        if ( TechniqueSupported( xp, xieValGeometry,
                ( ( GeometryParms * ) p->ts )->geoTech ) == False )
		return 0;

	constrainflag = drawableplaneflag = 0;
	if ( xp->screenDepth == 1 && !IsFaxImage( image->decode ) )
	{
		constrainflag = 1;
                if ( ( XIELut = CreatePointLut( xp, p, 1 << image->depth[ 0 ],
                        1 << xp->screenDepth, False ) ) == ( XieLut ) NULL )
                {
                        reps = 0;
                }
	}	
	else if ( xp->screenDepth != 1 && IsFaxImage( image->decode ) 
		&& ( ( GeometryParms * ) p->ts )->geoTech == xieValGeomAntialias )
	{
		constrainflag = 1;
                if ( ( XIELut = CreatePointLut( xp, p, 1 << image->depth[ 0 ],
                        1 << xp->screenDepth, False ) ) == ( XieLut ) NULL )
                {
                        reps = 0;
                }
	}
	else if ( xp->screenDepth != image->depth[ 0 ] )
	{
		constrainflag = 1;
                if ( ( XIELut = CreatePointLut( xp, p, 1 << image->depth[ 0 ],
                        1 << xp->screenDepth, False ) ) == ( XieLut ) NULL )
                {
                        reps = 0;
                }
	}
	if ( xp->vinfo.depth != 1 && IsFaxImage( image->decode ) 
		&& ( ( GeometryParms * ) p->ts )->geoTech != xieValGeomAntialias )
		drawableplaneflag = 1;

	if ( reps )
	{
		if ( IsFaxImage( image->decode ) )
			GetXIEFAXPhotomap( xp, p, 1, False );
		else
			XIEPhotomap = GetXIEPhotomap( xp, p, 1 );
		if ( XIEPhotomap == ( XiePhotomap ) NULL )
		{
			reps = 0;
		}
	}
	if ( reps )
	{
		if ( constrainflag )
			flo_elements = 5;
		else
			flo_elements = 3;
		flograph = XieAllocatePhotofloGraph(flo_elements);	
		if ( flograph == ( XiePhotoElement * ) NULL )
		{
			fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
			reps = 0;
		}

		XieFloImportPhotomap(&flograph[idx], XIEPhotomap, False); idx++;

		geo_tech = ( ( GeometryParms * ) p->ts )->geoTech;

		if ( !SetCoefficients( xp, p, 1, (GeometryParms*)p->ts, coeffs ) )
			reps = 0;
	}
	if ( reps )
	{
		XieFloGeometry(&flograph[idx], 
			idx,
			( ( GeometryParms * ) p->ts )->geoWidth,
			( ( GeometryParms * ) p->ts )->geoHeight,
			coeffs,
			constant,
			band_mask,
			geo_tech,
			geo_tech_params
		); idx++;

		if ( constrainflag )
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

		XieFloExportDrawable(&flograph[idx],
			idx, 		/* source phototag number */
			xp->w,
			xp->fggc,
			0,       /* x offset in window */
			0        /* y offset in window */
		); idx++;

		flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
		flo_notify = True;	
	}
	if ( !reps )
		FreeGeometryStuff( xp, p );
	return( reps );
}

void 
DoGeometry(XParms xp, Parms p, int reps)
{
    	int     i;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
		WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo, 0, False );
    	}
}

int 
InitGeometryFAX(XParms xp, Parms p, int reps)
{
	int     band_mask = 1;
	int	idx = 0;
	Bool radiometric;
	float	coeffs[ 6 ];
	static XieConstant constant = { 0.0, 0.0, 0.0 };
        XieLTriplet width, height, mylevels;
	XieGeometryTechnique geo_tech = ( XieGeometryTechnique ) NULL;
	char	*geo_tech_params = NULL;
	XieProcessDomain domain;
	char	*decode_tech = ( char * ) NULL;

        if ( TechniqueSupported( xp, xieValGeometry,
                ( ( GeometryParms * ) p->ts )->geoTech ) == False )
		return 0;

	image = p->finfo.image1;
	if ( !image )
		return( 0 );

	if ( !IsFaxImage( image->decode ) )
		return( 0 );

	FlushCache();
	G42Ddecode_params = ( XieDecodeG42DParam * ) NULL;
	G32Ddecode_params = ( XieDecodeG32DParam * ) NULL;
	G31Ddecode_params = ( XieDecodeG31DParam * ) NULL;
	TIFFPackBitsdecode_params = ( XieDecodeTIFFPackBitsParam * ) NULL;
	TIFF2decode_params = ( XieDecodeTIFF2Param * ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;
	XIELut = ( XieLut ) NULL;
	XIELut2 = ( XieLut ) NULL;
	flograph = ( XiePhotoElement * ) NULL;
	flo = ( XiePhotoflo ) NULL;
	constrainflag = drawableplaneflag = 0;

	if ( xp->screenDepth != 1 && ( ( GeometryParms * ) p->ts )->geoTech 
		== xieValGeomAntialias )
	{
		constrainflag = 1;
                if ( ( XIELut = CreatePointLut( xp, p, 1 << image->depth[ 0 ],
                        256, True ) ) == ( XieLut ) NULL )
                {
                        reps = 0;
                }
                if ( ( XIELut2 = CreatePointLut( xp, p, 256,
                        1 << xp->screenDepth, False ) ) == ( XieLut ) NULL )
                {
                        reps = 0;
                }
	}

	if ( xp->screenDepth != 1 
		&& ((GeometryParms *) p->ts )->geoTech != xieValGeomAntialias)
		drawableplaneflag = 1;

	if ( reps )	
	{
        	if ( !GetImageData( xp, p, 1 ) )
			reps = 0;
		else  
		{
		    	radiometric = (( GeometryParms *) p->ts )->radiometric;
		    	if ( image->decode == xieValDecodeG42D )
		    	{
       				G42Ddecode_params = XieTecDecodeG42D(
                			image->fill_order,
					True,
					radiometric    
				);
				decode_tech = ( char * ) G42Ddecode_params;
			}
			else if ( image->decode == xieValDecodeG32D )
			{
				G32Ddecode_params = XieTecDecodeG32D(
					image->fill_order,
					True,
					radiometric    
				);
				decode_tech = ( char * ) G32Ddecode_params;
			}
		    	else if ( image->decode == xieValDecodeG31D )
			{
				G31Ddecode_params = XieTecDecodeG31D(
					image->fill_order,
					True,
					radiometric    
				);
				decode_tech = ( char * ) G31Ddecode_params;
			}
		    	else if ( image->decode == xieValDecodeTIFF2 )
			{
				TIFF2decode_params = XieTecDecodeTIFF2(
					image->fill_order,
					True,
					radiometric    
				);
				decode_tech = ( char * ) TIFF2decode_params;
			}
		    	else if ( image->decode == xieValDecodeTIFFPackBits )
			{
       				TIFFPackBitsdecode_params = 
					XieTecDecodeTIFFPackBits(
						image->fill_order,
						True
					);
				decode_tech = ( char * ) TIFFPackBitsdecode_params;
			}
			else 
			{
				fprintf(stderr,"Invalid decode\n" );
				reps = 0;
			}
			if ( decode_tech == ( char * ) NULL )
			{
				reps = 0;
			}
		}
	}
	if ( reps )
	{
		if ( constrainflag )
			flo_elements = 7;
		else
			flo_elements = 3;
       		flograph = XieAllocatePhotofloGraph(flo_elements);
        	if ( flograph == ( XiePhotoElement * ) NULL )
        	{
			fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
			reps = 0;
		}
	} 
	if ( reps )
	{
		width[ 0 ] = image->width[ 0 ];
		height[ 0 ] = image->height[ 0 ];
		mylevels[ 0 ] = image->levels[ 0 ];

		XieFloImportClientPhoto(&flograph[idx],
			image->bandclass,
			width, height, mylevels,
			False,
			image->decode, decode_tech 
		); 
		idx++;

		geo_tech = ( ( GeometryParms * ) p->ts )->geoTech;

		if ( !SetCoefficients( xp, p, 1, (GeometryParms*)p->ts, coeffs ) )
			reps = 0;
	}
	
	if ( reps )
	{
		if ( constrainflag )
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

		XieFloGeometry(&flograph[idx], 
			idx,
			( ( GeometryParms * ) p->ts )->geoWidth,
			( ( GeometryParms * ) p->ts )->geoHeight,
			coeffs,
			constant,
			band_mask,
			geo_tech,
			geo_tech_params
		); idx++;

		if ( constrainflag )
		{
	                XieFloImportLUT(&flograph[idx], XIELut2 );
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

		if ( drawableplaneflag ) {
			XieFloExportDrawablePlane(&flograph[idx],
				idx, 	 /* source phototag number */
				xp->w,
				xp->bggc,
				0,       /* x offset in window */
				0        /* y offset in window */
			); idx++;
		}
		else {
			XieFloExportDrawable(&flograph[idx],
				idx, 	 /* source phototag number */
				xp->w,
				xp->bggc,
				0,       /* x offset in window */
				0        /* y offset in window */
			); idx++;
		}
		flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
		flo_notify = True;	
		size = image->fsize;
	}
	if ( !reps )
		FreeGeometryFAXStuff( xp, p );
	return( reps );
}

void 
DoGeometryFAX(XParms xp, Parms p, int reps)
{
	int	i;

    	for (i = 0; i != reps; i++) {
       		XieExecutePhotoflo(xp->d, flo, flo_notify ); 
		XSync( xp->d, 0 );
	        PumpTheClientData( xp, p, flo, 0, 1, image->data, size, 0 );
		WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo, 0, False );	
    	}
}

int
SetCoefficients(XParms xp, Parms p, int which, 
		GeometryParms *gp, float coeffs[])
{
	double	sf, rad;
	XIEimage *image;
	int type;

	switch( which )
	{
	case 1:
		image = p->finfo.image1;
		break;
	case 2:
		image = p->finfo.image2;
		break;
	case 3:
		image = p->finfo.image3;
		break;
	case 4:
		image = p->finfo.image4;
		break;
	default:
		image = ( XIEimage * ) NULL;
		return( 0 );	
	}
	type = gp->geoType;
	if ( !image || !gp )
	{
		fprintf( stderr, 
			"SetCoefficients: invalid image or test parameters\n" );
		return ( 0 );
	}

	switch( type )
	{
	case GEO_TYPE_CROP:
		coeffs[ 0 ] = 1;
		coeffs[ 1 ] = 0;
		coeffs[ 2 ] = 0;
		coeffs[ 3 ] = 1;
		coeffs[ 4 ] = gp->geoXOffset;
		coeffs[ 5 ] = gp->geoYOffset;
		break;
	case GEO_TYPE_SCALE:
		coeffs[ 0 ] = image->width[ 0 ] / ( float )gp->geoWidth;
		coeffs[ 1 ] = 0;
		coeffs[ 2 ] = 0;
		coeffs[ 3 ] = image->height[ 0 ] / ( float )gp->geoHeight;
		coeffs[ 4 ] = 0;
		coeffs[ 5 ] = 0;
		break;
	case GEO_TYPE_SCALEDROTATE:
		rad = M_PI * gp->geoAngle / 180.0;
		sf = ( image->width[ 0 ] / ( float )gp->geoWidth );
		coeffs[ 0 ] = sf * cos( rad );
		coeffs[ 1 ] = sf * sin( rad );
		coeffs[ 2 ] = -sf * sin( rad );
		coeffs[ 3 ] = sf * cos( rad );
		coeffs[ 4 ] = image->width[ 0 ] / 2.0 - sf/2 * (cos(rad)*gp->geoWidth + sin(rad) * gp->geoHeight); 
		coeffs[ 5 ] = image->height[ 0 ] / 2.0 - sf/2 * (-sin(rad)*gp->geoWidth + cos(rad) * gp->geoHeight); 
		break;
	case GEO_TYPE_MIRRORX:
		coeffs[ 0 ] = -1;
		coeffs[ 1 ] = 0;
		coeffs[ 2 ] = 0;
		coeffs[ 3 ] = 1;
		coeffs[ 4 ] = gp->geoWidth - 1;
		coeffs[ 5 ] = 0;
		break;
	case GEO_TYPE_MIRRORY:
		coeffs[ 0 ] = 1;
		coeffs[ 1 ] = 0;
		coeffs[ 2 ] = 0;
		coeffs[ 3 ] = -1;
		coeffs[ 4 ] = 0;
		coeffs[ 5 ] = gp->geoHeight - 1;
		break;
	case GEO_TYPE_MIRRORXY:
		coeffs[ 0 ] = -1;
		coeffs[ 1 ] = 0;
		coeffs[ 2 ] = 0;
		coeffs[ 3 ] = -1;
		coeffs[ 4 ] = gp->geoWidth - 1;
		coeffs[ 5 ] = gp->geoHeight - 1;
		break;
	case GEO_TYPE_ROTATE:
		rad = M_PI * gp->geoAngle / 180.0;
		coeffs[ 0 ] = cos( rad );
		coeffs[ 1 ] = sin( rad );
		coeffs[ 2 ] = -sin( rad );
		coeffs[ 3 ] = cos( rad );
		coeffs[ 4 ] = gp->geoWidth/2.0 - 0.5 * ( cos( rad ) * image->width[ 0 ] + sin( rad ) * image->height[ 0 ] );
		coeffs[ 5 ] = gp->geoHeight/2.0 - 0.5 * ( -sin( rad ) * image->width[ 0 ] + cos( rad ) * image->height[ 0 ] );
		break;
	case GEO_TYPE_DEFAULT:
	default:
		coeffs[ 0 ] = 1;
		coeffs[ 1 ] = 0;
		coeffs[ 2 ] = 0;
		coeffs[ 3 ] = 1;
		coeffs[ 4 ] = 0;
		coeffs[ 5 ] = 0;
		break;
	}
	return( 1 );
}

void
EndGeometry(XParms xp, Parms p)
{
	FreeGeometryStuff( xp, p );
}

void
FreeGeometryStuff(XParms xp, Parms p)
{
	if ( XIELut )
	{
		XieDestroyLUT( xp->d, XIELut );
		XIELut = ( XieLut ) NULL;
	}

        if ( XIEPhotomap && !IsPhotomapInCache( XIEPhotomap ) )
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

void
EndGeometryFAX(xp, p)
XParms  xp;
Parms   p;
{
	FreeGeometryFAXStuff( xp, p );
}

void
FreeGeometryFAXStuff( xp, p )
XParms	xp;
Parms	p;
{
	if ( XIELut )
	{
		XieDestroyLUT( xp->d, XIELut );
		XIELut = ( XieLut ) NULL;
	}

	if ( XIELut2 )
	{
		XieDestroyLUT( xp->d, XIELut2 );
		XIELut2 = ( XieLut ) NULL;
	}

        if ( XIEPhotomap )
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

	if ( G42Ddecode_params )
	{
		XFree( G42Ddecode_params );
		G42Ddecode_params = ( XieDecodeG42DParam * ) NULL;
	}
	else if ( G32Ddecode_params )
	{
		XFree( G32Ddecode_params );
		G32Ddecode_params = ( XieDecodeG32DParam * ) NULL;
	}
	else if ( G31Ddecode_params )
	{
		XFree( G31Ddecode_params );
		G31Ddecode_params = ( XieDecodeG31DParam * ) NULL;
	}
	else if ( TIFF2decode_params )
	{
		XFree( TIFF2decode_params );
		TIFF2decode_params = ( XieDecodeTIFF2Param * ) NULL;
	}
	else if ( TIFFPackBitsdecode_params )
	{
		XFree( TIFFPackBitsdecode_params );
		TIFFPackBitsdecode_params = 
			( XieDecodeTIFFPackBitsParam * ) NULL;
	}
}

