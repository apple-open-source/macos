/* $Xorg: band.c,v 1.4 2001/02/09 02:05:46 xorgcvs Exp $ */
/**** module band.c ****/
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
  
	band.c -- BandCombine/BandExtract flo element tests 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/band.c,v 1.6 2001/12/14 20:01:46 dawes Exp $ */

#include "xieperf.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <math.h>

static XiePhotomap XIEPhotomap1;
static XiePhotomap XIEPhotomap2;
static XiePhotomap XIEPhotomap3;
static XieLut XIELut;

static XiePhotoElement *flograph;
static int flo_notify;
static XiePhotoflo flo;
static int flo_elements;
static int cclass;

static XStandardColormap stdCmap;
static Bool useStdCmap;
extern Bool WMSafe;

static int CreateColormapFlo ( XParms xp, Parms p, Bool useStdCmap );
static int CreateBandCombineFlo ( XParms xp, Parms p, int cclass );

int 
InitBandSelectExtract(XParms xp, Parms p, int reps)
{
	XieLTriplet levels;
	int cube;
	int which;
	int type;
	int atom;

	which = (( BandParms * )p->ts)->which;
	type = p->finfo.image1->bandclass;

#if     defined(__cplusplus) || defined(c_plusplus)
    	cclass = xp->vinfo.c_class;
#else
    	cclass = xp->vinfo.class;
#endif
	XIEPhotomap1 = ( XiePhotomap ) NULL;
	XIEPhotomap2 = ( XiePhotomap ) NULL;
	XIEPhotomap3 = ( XiePhotomap ) NULL;
	XIELut = ( XieLut ) NULL;
	flograph = ( XiePhotoElement * ) NULL;
	flo = ( XiePhotoflo ) NULL;

	if ( xp->vinfo.depth < 4 ) 
		return( 0 );

	if ( IsColorVisual( cclass ) )
		atom = XA_RGB_BEST_MAP;
	else
		atom = XA_RGB_GRAY_MAP;
	if ( !IsStaticVisual( cclass ) && type == xieValTripleBand && !IsTrueColorOrDirectColor( cclass ) )
	{
		if ( !GetStandardColormap( xp, &stdCmap, atom ) )
		{
			fprintf( stderr, "Couldn't get a standard colormap\n" );
			reps = 0;
		}
		else
			InstallThisColormap( xp, stdCmap.colormap );
	}
	else if ( type == xieValTripleBand ) 
	{
		if ( !IsStaticVisual( cclass ) )
			InstallColorColormap( xp );
		CreateStandardColormap( xp, &stdCmap, atom );
	}
	else
		cube = icbrt( xp->vinfo.colormap_size );

	if ( reps )
	{
		if ( IsTrueColorOrDirectColor( cclass ) &&
			type == xieValTripleBand )	
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
	}
	if ( reps )
	{
		XIEPhotomap1 = GetXIEDitheredTriplePhotomap( xp, p, 1,
			xieValDitherDefault, 0, levels );

		if ( XIEPhotomap1 == ( XiePhotomap ) NULL )
		{
			reps = 0;
		}	 
	}

	if ( reps ) 
	{
		if ( !CreateColorBandSelectExtractFlo( xp, p ) )
		{
			fprintf( stderr, "CreateColorBandSelectFlo failed\n" );
			reps = 0;
		}
	}

	if ( !reps )
	{
		if ( !IsStaticVisual( cclass ) )
			InstallGrayColormap( xp );
		FreeBandStuff( xp, p );
	}
	return( reps );
}

int
InitBandColormap(XParms xp, Parms p, int reps)
{
	XieLTriplet levels;
        Atom atom;

	useStdCmap = ( ( BandParms * )p->ts )->useStdCmap;
	if ( xp->vinfo.depth == 1 && useStdCmap == True )
		return( 0 );

#if     defined(__cplusplus) || defined(c_plusplus)
    	cclass = xp->vinfo.c_class;
#else
    	cclass = xp->vinfo.class;
#endif

	if ( ( IsStaticVisual( cclass ) || IsTrueColorOrDirectColor( cclass ) )
		&& useStdCmap == True )
		return( 0 );
	XIEPhotomap1 = ( XiePhotomap ) NULL;
	XIELut = ( XieLut ) NULL;
	flograph = ( XiePhotoElement * ) NULL;
	flo = ( XiePhotoflo ) NULL;

	if ( useStdCmap == True )
	{
		atom = ( ( BandParms * )p->ts )->atom;
		if ( IsGrayVisual( cclass ) )
		{
			fprintf( stderr, "Switching to XA_RGB_GRAY_MAP\n" );
			atom = XA_RGB_GRAY_MAP; 
		}
		if ( GetStandardColormap( xp, &stdCmap, atom ) == False )
		{
			fprintf( stderr, "Couldn't get a standard colormap\n" );
			fflush( stderr );
			reps = 0;
		}
		else
			InstallThisColormap( xp, stdCmap.colormap );
	}

	if ( reps )
	{
		if ( useStdCmap == True )
		{
			levels[ 0 ] = stdCmap.red_max + 1; 
			levels[ 1 ] = stdCmap.green_max + 1; 
			levels[ 2 ] = stdCmap.blue_max + 1; 
		}
		else
		{
			levels[ 0 ] = xp->vinfo.colormap_size;
			levels[ 1 ] = xp->vinfo.colormap_size;
			levels[ 2 ] = xp->vinfo.colormap_size;
		}

		XIEPhotomap1 = GetXIEDitheredTriplePhotomap( xp, p, 1,
			xieValDitherDefault, 0, levels );
		if ( XIEPhotomap1 == ( XiePhotomap ) NULL )
		{
			reps = 0;
		}	 
	}

	if ( reps )
	{
		if ( !CreateColormapFlo( xp, p, useStdCmap ) )
		{
			fprintf( stderr, "CreateColormapFlo failed\n" );
			reps = 0;
		}
	}

	if ( !reps )
	{
		FreeBandStuff( xp, p );
		if ( useStdCmap == True )
                	InstallGrayColormap( xp );
	}

	return( reps );
}

static int
CreateColormapFlo(XParms xp, Parms p, Bool useStdCmap)
{
	int idx, decode_notify;
	unsigned int mylevels;
	XieConstant c1;
	float bias = 0.0;
	int pointflag = 0;
	XieProcessDomain domain;
	int	cclass;

#if     defined(__cplusplus) || defined(c_plusplus)
        cclass = xp->vinfo.c_class;
#else
        cclass = xp->vinfo.class;
#endif

	idx = 0;
	if ( useStdCmap == True )
	{
		c1[ 0 ] = stdCmap.red_mult;
		c1[ 1 ] = stdCmap.green_mult;
		c1[ 2 ] = stdCmap.blue_mult;
		bias = ( float ) stdCmap.base_pixel;
	}
	else
	{
		c1[ 0 ] = (( BandParms * )p->ts)->c1[ 0 ];
		c1[ 1 ] = (( BandParms * )p->ts)->c1[ 1 ];
		c1[ 2 ] = (( BandParms * )p->ts)->c1[ 2 ];
		bias = 0.0;
	}

	mylevels = xp->vinfo.colormap_size;

	flo_elements = 3;

	if ( IsTrueColorOrDirectColor( cclass ) )
	{
	       	pointflag++;
                flo_elements+=2;
                if ( ( XIELut = CreatePointLut( xp, p,
                        xp->vinfo.colormap_size, 
			1 << xp->screenDepth, False ) )
                        == ( XieLut ) NULL )
                {
                        return( 0 );
                }
        }
		

        flograph = XieAllocatePhotofloGraph( flo_elements );
        if ( flograph == ( XiePhotoElement * ) NULL )
	{
                fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                return( 0 );
        }

	decode_notify = False;

        XieFloImportPhotomap(&flograph[idx],XIEPhotomap1,decode_notify);
	idx++;

	XieFloBandExtract( &flograph[idx], idx, mylevels, bias, c1 ); idx++;

        if ( pointflag )
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
		idx,              /* source phototag number */
		xp->w,
		xp->fggc,
		0,       /* x offset in window */
		0        /* y offset in window */
	);

	flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
	XSync( xp->d, 0 );
	flo_notify = False;
	return( 1 );
}

int 
InitBandCombine(XParms xp, Parms p, int reps)
{
#if     defined(__cplusplus) || defined(c_plusplus)
    	cclass = xp->vinfo.c_class;
#else
    	cclass = xp->vinfo.class;
#endif

	XIELut = ( XieLut ) NULL;
	XIEPhotomap1 = ( XiePhotomap ) NULL;
	XIEPhotomap2 = ( XiePhotomap ) NULL;
	XIEPhotomap3 = ( XiePhotomap ) NULL;
	flograph = ( XiePhotoElement * ) NULL;
	flo = ( XiePhotoflo ) NULL;

	if ( reps )
	{
		XIEPhotomap1 = GetXIEPhotomap( xp, p, 1 );
		XIEPhotomap2 = GetXIEPhotomap( xp, p, 2 );
		XIEPhotomap3 = GetXIEPhotomap( xp, p, 3 );

		if ( XIEPhotomap1 == ( XiePhotomap ) NULL ||
		     XIEPhotomap2 == ( XiePhotomap ) NULL ||
		     XIEPhotomap3 == ( XiePhotomap ) NULL )
		{
			reps = 0;
		}	 
	}

	if ( reps )
	{
		if ( !CreateBandCombineFlo( xp, p, cclass ) )
		{
			fprintf( stderr, "CreateBandCombineFlo failed\n" );
			reps = 0;
		}
	}

	if ( !reps )
	{
		FreeBandStuff( xp, p );
	}
	return( reps );
}

static int
CreateBandCombineFlo(XParms xp, Parms p, int cclass )
{
	int idx, decode_notify;
	float bias;
	XieConstant c;
	XieProcessDomain domain;
	unsigned int mylevels;
	int	pointflag;
	XIEimage *image;

	pointflag = 0;

	mylevels = 1 << xp->screenDepth;

	idx = 0;
	image = p->finfo.image1;

	flo_elements = 6;
	if ( 1 << image->depth[ 0 ] != mylevels )
	{
		pointflag++;
		flo_elements+=2;

	       	if ( ( XIELut = CreatePointLut( xp, p, 1 << image->depth[ 0 ], 
			mylevels, False ) ) == ( XieLut ) NULL )
                {
                        return( 0 );
                }
	}	

	c[ 0 ] = ( ( BandParms * )p->ts )->c1[ 0 ];
	c[ 1 ] = ( ( BandParms * )p->ts )->c1[ 1 ];
	c[ 2 ] = ( ( BandParms * )p->ts )->c1[ 2 ];
	bias = ( ( BandParms * )p->ts )->bias;

        flograph = XieAllocatePhotofloGraph( flo_elements );
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
                fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                return( 0 );
        }

	decode_notify = False;

        XieFloImportPhotomap(&flograph[idx],XIEPhotomap1,decode_notify);
	idx++;

        XieFloImportPhotomap(&flograph[idx],XIEPhotomap2,decode_notify);
	idx++;

        XieFloImportPhotomap(&flograph[idx],XIEPhotomap3,decode_notify);
	idx++;

	XieFloBandCombine( &flograph[ idx ], idx - 2, idx - 1, idx ); idx++;

	XieFloBandExtract( &flograph[idx], idx, 1 << image->depth[ 0 ], bias, c ); idx++;

	if ( pointflag )
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
		idx,              /* source phototag number */
		xp->w,
		xp->fggc,
		0,       /* x offset in window */
		0        /* y offset in window */
	);

	flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
	XSync( xp->d, 0 );
	flo_notify = False;
	return( 1 );
}

int
CreateColorBandSelectExtractFlo(XParms xp, Parms p)
{
	int idx, which, decode_notify;
	unsigned int mylevels;
	XieConstant c1, c2, c3;
	float bias = 0.0;
	XIEimage *image;

	which = (( BandParms * )p->ts)->which;

	image = p->finfo.image1;
	idx = 0;

	if ( which == BandExtract )
	{
		c1[ 0 ] = (( BandParms * )p->ts)->c1[ 0 ];
		c1[ 1 ] = (( BandParms * )p->ts)->c1[ 1 ];
		c1[ 2 ] = (( BandParms * )p->ts)->c1[ 2 ];

		c2[ 0 ] = (( BandParms * )p->ts)->c2[ 0 ];
		c2[ 1 ] = (( BandParms * )p->ts)->c2[ 1 ];
		c2[ 2 ] = (( BandParms * )p->ts)->c2[ 2 ];

		c3[ 0 ] = (( BandParms * )p->ts)->c3[ 0 ];
		c3[ 1 ] = (( BandParms * )p->ts)->c3[ 1 ];
		c3[ 2 ] = (( BandParms * )p->ts)->c3[ 2 ];

		bias = (( BandParms * )p->ts)->bias;
	}
	flo_elements = 7;

        flograph = XieAllocatePhotofloGraph( flo_elements );
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
                fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                return( 0 );
        }

	decode_notify = False;

        XieFloImportPhotomap(&flograph[idx],XIEPhotomap1,decode_notify);
	idx++;

	if ( which == BandExtract && IsTrueColorOrDirectColor( cclass ) )
	{
		mylevels = stdCmap.red_max + 1;
		XieFloBandExtract( &flograph[idx], 1, mylevels, bias, c1 ); idx++;
		mylevels = stdCmap.green_max + 1;
		XieFloBandExtract( &flograph[idx], 1, mylevels, bias, c2 ); idx++;
		mylevels = stdCmap.blue_max + 1;
		XieFloBandExtract( &flograph[idx], 1, mylevels, bias, c3 ); idx++;
	}
	else
	{
		XieFloBandSelect( &flograph[idx], 1, 0 ); idx++;
		XieFloBandSelect( &flograph[idx], 1, 1 ); idx++;
		XieFloBandSelect( &flograph[idx], 1, 2 ); idx++;
	}

	XieFloBandCombine( &flograph[ idx ], idx - 2, idx - 1, idx ); idx++;

        c1[ 0 ] = stdCmap.red_mult; 
        c1[ 1 ] = stdCmap.green_mult;
	c1[ 2 ] = stdCmap.blue_mult;
	bias = ( float ) stdCmap.base_pixel; 

	XieFloBandExtract( &flograph[ idx ], idx,
		1 << xp->vinfo.depth, bias, c1 );
	idx++;

       	XieFloExportDrawable(&flograph[idx],
		idx,              /* source phototag number */
		xp->w,
		xp->fggc,
		0,       /* x offset in window */
		0        /* y offset in window */
	);

	flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
	XSync( xp->d, 0 );
	flo_notify = False;
	return( 1 );
}

void 
DoBand(XParms xp, Parms p, int reps)
{
	int	i;

	for ( i = 0; i < reps; i++ )
	{
                XieExecutePhotoflo(xp->d, flo, flo_notify );
        }
}

void 
EndBandCombine(XParms xp, Parms p)
{
	FreeBandStuff( xp, p );
}

void 
EndBandColormap(XParms xp, Parms p)
{
	if ( useStdCmap == True )
		InstallGrayColormap( xp );
	FreeBandStuff( xp, p );
}

void 
EndBandSelectExtract(XParms xp, Parms p)
{
	if ( IsColorVisual( cclass ) )
		InstallGrayColormap( xp );
	FreeBandStuff( xp, p );
}

void
FreeBandStuff(XParms xp, Parms p)
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

	if ( XIELut )
	{
		XieDestroyLUT(xp->d, XIELut);
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
