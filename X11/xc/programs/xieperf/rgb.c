/* $Xorg: rgb.c,v 1.4 2001/02/09 02:05:48 xorgcvs Exp $ */

/**** module rgb.c ****/
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
  
	rgb.c -- ConvertToRGB/ConvertFromRGB flo element tests 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/rgb.c,v 1.6 2001/12/14 20:01:51 dawes Exp $ */

#include "xieperf.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <math.h>

static int CreateRGBFlo ( XParms xp, Parms p );

static XiePhotomap XIEPhotomap;

static XiePhotoElement *flograph;
static int flo_notify;
static XiePhotoflo flo;
static int flo_elements;
static int cclass;
static XStandardColormap stdCmap;
static int imageLevels;

int 
InitRGB(XParms xp, Parms p, int reps)
{
	static XIEimage *image;
	int atom;


#if     defined(__cplusplus) || defined(c_plusplus)
    	cclass = xp->vinfo.c_class;
#else
    	cclass = xp->vinfo.class;
#endif

	if ( xp->screenDepth == 1 )
		return( 0 );

	/* for now, do this */

	image = p->finfo.image1;
	imageLevels = image->levels[ 0 ];

	XIEPhotomap = ( XiePhotomap ) NULL;
	flograph = ( XiePhotoElement * ) NULL;
	flo = ( XiePhotoflo ) NULL;

	if ( IsGrayVisual( cclass ) )
		atom = XA_RGB_GRAY_MAP;
	else
		atom = XA_RGB_BEST_MAP;
	if ( !IsStaticVisual( cclass ) && !IsTrueColorOrDirectColor( cclass ) )
	{
		if ( !GetStandardColormap( xp, &stdCmap, atom ) )
			reps = 0;
		else
			InstallThisColormap( xp, stdCmap.colormap );
	}
	else 
	{
		if ( !IsStaticVisual( cclass ) )
			InstallColorColormap( xp );
		CreateStandardColormap( xp, &stdCmap, atom );
	}
		
	if ( reps )
	{
		XIEPhotomap = GetXIETriplePhotomap( xp, p, 1 ); 

		if ( XIEPhotomap == ( XiePhotomap ) NULL )
		{
			reps = 0;
		}	 
	}
	if ( reps )
	{
		if ( !CreateRGBFlo( xp, p ) ) 
		{
			fprintf( stderr, "Could not create RGB flo\n" );
			reps = 0;
		}
	}

	if ( !reps )
	{
		if ( !IsStaticVisual( cclass ) )
			InstallGrayColormap( xp );
		FreeRGBStuff( xp, p );
	}
	return( reps );
}

static int
CreateRGBFlo(XParms xp, Parms p)
{
	RGBParms *rgb = ( RGBParms * )p->ts;
	int idx, decode_notify;
	int cube;
	char *techParms, *colorParm1, *colorParm2;
	char *whiteAdjustParm, *gamutParm;
	XieRGBToCIELabParam *RGBToCIELabParm;
	XieRGBToCIEXYZParam *RGBToCIEXYZParm;
	XieRGBToYCbCrParam *RGBToYCbCrParm;
	XieRGBToYCCParam *RGBToYCCParm;
	XieCIELabToRGBParam *CIELabToRGBParm;
	XieCIEXYZToRGBParam *CIEXYZToRGBParm;
	XieYCbCrToRGBParam *YCbCrToRGBParm;
	XieYCCToRGBParam *YCCToRGBParm;
	XieColorspace colorSpace;
	XieMatrix toMatrix, fromMatrix;
	XieWhiteAdjustTechnique whiteAdjust;
	double ycc_scale = 1.402;
	XieGamutTechnique gamut;
        XieLTriplet levels, hclevels, rgblevels;
        XieConstant in_low,in_high;
        XieLTriplet out_low,out_high;
	char *ditherTech;
	int retval;
        XieConstant c1;
        float bias;

	retval = 1;
	idx = 0;
	levels[ 0 ] = xp->vinfo.colormap_size;
	levels[ 1 ] = xp->vinfo.colormap_size;
	levels[ 2 ] = xp->vinfo.colormap_size;
	colorSpace = rgb->colorspace;
	memcpy( toMatrix, rgb->toMatrix, sizeof( XieMatrix ) );
	memcpy( fromMatrix, rgb->fromMatrix, sizeof( XieMatrix ) );
	whiteAdjust = rgb->whiteAdjust;
	gamut = rgb->gamut;
	cube = icbrt( xp->vinfo.colormap_size );

	whiteAdjustParm = ( char * ) NULL;
	gamutParm = ( char * ) NULL;
	colorParm1 = ( char * ) NULL;
	colorParm2 = ( char * ) NULL;
	techParms = ( char * ) NULL;
	ditherTech = ( char * ) NULL;

	switch (rgb->which) {
	case RGB_FF:	flo_elements = 8; break; /* + unconstrain, hardclip */
	case RGB_IF:	flo_elements = 7; break; /* + clipscale */
	case RGB_II:	flo_elements = 6; break; /* baseline */
	}

        flograph = XieAllocatePhotofloGraph( flo_elements );
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
                fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                return( 0 );
        }
	if ( whiteAdjust )
	{
		whiteAdjustParm = (char *)
			XieTecWhiteAdjustCIELabShift (rgb->whitePoint);
	}

	switch ( colorSpace )
	{
	case xieValRGBToCIELab:
		RGBToCIELabParm = XieTecRGBToCIELab( 
			toMatrix,
			whiteAdjust,
			whiteAdjustParm );
		colorParm1 = ( char * ) RGBToCIELabParm;
		break;
	case xieValRGBToCIEXYZ:
		RGBToCIEXYZParm = XieTecRGBToCIEXYZ( 
			toMatrix,
			whiteAdjust,
			whiteAdjustParm );
		colorParm1 = ( char * ) RGBToCIEXYZParm;
		break;
	case xieValRGBToYCbCr:
		rgblevels[0] = rgb->RGBLevels[0];
		rgblevels[1] = rgb->RGBLevels[1];
		rgblevels[2] = rgb->RGBLevels[2];
		RGBToYCbCrParm = XieTecRGBToYCbCr( 
			/* rgb->RGBLevels */ rgblevels,
			(double) rgb->luma[ 0 ],
			(double) rgb->luma[ 1 ],
			(double) rgb->luma[ 2 ],
			rgb->bias ); 
		colorParm1 = ( char * )  RGBToYCbCrParm;
		break;
	case xieValRGBToYCC:
		rgblevels[0] = rgb->RGBLevels[0];
		rgblevels[1] = rgb->RGBLevels[1];
		rgblevels[2] = rgb->RGBLevels[2];
		RGBToYCCParm = XieTecRGBToYCC( 
			/* rgb->RGBLevels */ rgblevels,
			(double) rgb->luma[ 0 ],
			(double) rgb->luma[ 1 ],
			(double) rgb->luma[ 2 ],
			(double) rgb->scale); 
		colorParm1 = ( char * ) RGBToYCCParm;
		break; 
	default:
		fprintf( stderr, "Unknown colorspace\n" );	
		fflush( stderr );
		return( 0 );
	}

	decode_notify = False;

        XieFloImportPhotomap(&flograph[idx],XIEPhotomap,decode_notify);
	idx++;
		
	if (rgb->which == RGB_FF)
	{
		XieFloUnconstrain( &flograph[idx], idx );
		idx++;
	}

	XieFloConvertFromRGB( &flograph[idx],
		idx,
		colorSpace,
		colorParm1
	);
	idx++;

	switch( colorSpace )
	{
	case xieValCIELabToRGB:
		CIELabToRGBParm = XieTecCIELabToRGB( 
			fromMatrix,
			whiteAdjust,
			whiteAdjustParm,
			gamut,
			gamutParm );
		colorParm2 = ( char * ) CIELabToRGBParm;
		break;
	case xieValCIEXYZToRGB:
		CIEXYZToRGBParm = XieTecCIEXYZToRGB( 
			fromMatrix,
			whiteAdjust,
			whiteAdjustParm,
			gamut,
			gamutParm );
		colorParm2 = ( char * ) CIEXYZToRGBParm;
		break;
	case xieValYCbCrToRGB:
		YCbCrToRGBParm = XieTecYCbCrToRGB( 
			levels,
			(double) rgb->luma[ 0 ],
			(double) rgb->luma[ 1 ],
			(double) rgb->luma[ 2 ],
			rgb->bias,
			gamut,
			gamutParm );
		colorParm2 = ( char * ) YCbCrToRGBParm;
		break;
	case xieValYCCToRGB:
		YCCToRGBParm = XieTecYCCToRGB( 
			levels,
			(double) rgb->luma[ 0 ],
			(double) rgb->luma[ 1 ],
			(double) rgb->luma[ 2 ],
			ycc_scale,
			gamut,
			gamutParm );
		colorParm2 = ( char * ) YCCToRGBParm;
		break; 
	default:
		fprintf( stderr, "Unknown colorspace\n" );
		fflush( stderr );
		retval = 0;
		goto out;
	}

	XieFloConvertToRGB( &flograph[idx],
		idx,
		colorSpace,
		colorParm2
	);
	idx++;

	techParms = ( char * ) NULL;
	if (rgb->which == RGB_FF )
	{
		/* or perhaps clipscale with in_high == 255.0 */

		hclevels[ 0 ] = imageLevels;
		hclevels[ 1 ] = imageLevels;
		hclevels[ 2 ] = imageLevels;

		XieFloConstrain( &flograph[idx],
			idx,
			hclevels,
			xieValConstrainHardClip,
			techParms
		);
		idx++;
	}
	else if (rgb->which == RGB_IF)
	{
		in_low[ 0 ] = in_low[ 1 ] = in_low[ 2 ] = 0.0;
		in_high[ 0 ] = in_high[ 1 ] = in_high[ 2 ] = 1.0;
		out_low[ 0 ] = out_low[ 1 ] = out_low[ 2 ] = 0;

		out_high[ 0 ] = ( xp->vinfo.colormap_size ) - 1;
		out_high[ 1 ] = ( xp->vinfo.colormap_size ) - 1;
		out_high[ 2 ] = ( xp->vinfo.colormap_size ) - 1;

		techParms = (char *) ( XieClipScaleParam * ) 
			XieTecClipScale( in_low, in_high, out_low, out_high);

		hclevels[ 0 ] = out_high[ 0 ] + 1; 
		hclevels[ 1 ] = out_high[ 1 ] + 1; 
		hclevels[ 2 ] = out_high[ 2 ] + 1; 
		XieFloConstrain( &flograph[idx],
			idx,
			hclevels,
			xieValConstrainClipScale,
			techParms
		);
		idx++;
	}

	if ( IsTrueColorOrDirectColor( cclass ) )
	{
		levels[ 0 ] = TripleTrueOrDirectLevels( xp );
		levels[ 1 ] = levels[ 2 ] = levels[ 0 ];
	}
	else
	{
		levels[ 0 ] = stdCmap.red_max + 1;
		levels[ 1 ] = stdCmap.green_max + 1;
		levels[ 2 ] = stdCmap.blue_max + 1;
	}

	ditherTech = ( char * ) NULL;
	XieFloDither( &flograph[ idx ],
		idx,
		0x7,
		levels,
		xieValDitherErrorDiffusion,
		( char * ) ditherTech
	);
	idx++;

        c1[ 0 ] = stdCmap.red_mult;
        c1[ 1 ] = stdCmap.green_mult;
        c1[ 2 ] = stdCmap.blue_mult;
        bias = ( float ) stdCmap.base_pixel;

        XieFloBandExtract(&flograph[idx], idx, 1 << xp->vinfo.depth, bias, c1);
	idx++;

        XieFloExportDrawable(&flograph[idx],
                idx,              /* source phototag number */
                xp->w,
                xp->fggc,
                0,       /* x offset in window */
                0        /* y offset in window */
        );
	idx++;

	flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
	XSync( xp->d, 0 );
out:
	if ( whiteAdjustParm )
		XFree( whiteAdjustParm );
	if ( colorParm1 )
		XFree( colorParm1 );
	if ( colorParm2 )
		XFree( colorParm2 );
	if ( techParms )
		XFree( techParms );
	if ( ditherTech )
		XFree( ditherTech );
	flo_notify = False;
	return( retval );
}

void 
DoRGB(XParms xp, Parms p, int reps)
{
	int	i;

	for ( i = 0; i < reps; i++ )
	{
                XieExecutePhotoflo(xp->d, flo, flo_notify );
        }
}

void 
EndRGB(XParms xp, Parms p)
{
	InstallGrayColormap( xp );
	FreeRGBStuff( xp, p );
}

void
FreeRGBStuff(XParms xp, Parms p)
{
	if ( XIEPhotomap && IsPhotomapInCache( XIEPhotomap ) == False )
	{
		XieDestroyPhotomap(xp->d, XIEPhotomap);
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
