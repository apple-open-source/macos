/* $Xorg: funcode.c,v 1.4 2001/02/09 02:05:47 xorgcvs Exp $ */

/**** module funcode.c ****/
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
  
	funcode.c -- "Let's mess up the encode parameters and make sure
		      things are lossless" tests. Both SingleBand and
		      TripleBand.

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/funcode.c,v 1.6 2001/12/14 20:01:49 dawes Exp $ */

#include "xieperf.h"
#include <stdio.h>
#include <math.h>

static XiePhotomap XIEPhotomap;
static XiePhotomap XIEPhotomap2;

static int flo_notify;
static XiePhotoElement *flograph1, *flograph2;
static int flo1_elements;
static int flo2_elements;
static XiePhotoflo *flo;
static XieLut XIELut;
static char **encode_parms;
static int testSize;
static int type;
static char *parms;
extern Bool WMSafe;
static XStandardColormap stdCmap;
static int cclass;

extern Window drawableWindow;
extern Bool dontClear;

int 
InitFunnyEncode(XParms xp, Parms p, int reps)
{
	int decode_notify = 0;
	int i, idx;
	XIEimage *image;
	XieProcessDomain domain;
	XieEncodeTechnique encode_tech = ( XieEncodeTechnique ) NULL;
	XieEncodeUncompressedSingleParam *encode_single;
	XieEncodeUncompressedTripleParam *encode_triple;
	XieLTriplet levels;
	int techParmOffset, floIdx;
	FunnyEncodeParms *ptr;
	int depthmismatch;
        XieConstant in_low,in_high;
        XieLTriplet out_low,out_high;
	XieLTriplet mult;
	XieConstant c1;
	float bias = 0.0;
	int constrainFlag = 0;
        GeometryParms gp;
	int atom;

	depthmismatch = 0;
	parms = ( char * ) NULL;
	image = p->finfo.image1;
	type = image->bandclass;
#if     defined(__cplusplus) || defined(c_plusplus)
        cclass = xp->vinfo.c_class;
#else
        cclass = xp->vinfo.class;
#endif
	ptr = ( FunnyEncodeParms * ) p->ts; 

        if ( type == xieValTripleBand && xp->screenDepth == 1 )
                return( 0 );

	if ( IsDISServer() )
		return( 0 );

	XIEPhotomap = ( XiePhotomap ) NULL;
	XIEPhotomap2 = ( XiePhotomap ) NULL;
	XIELut = ( XieLut ) NULL;
	flo = ( XiePhotoflo * ) NULL;
	flograph1 = ( XiePhotoElement * ) NULL;
	flograph2 = ( XiePhotoElement * ) NULL;
	encode_parms = ( char ** ) NULL;

	/* check for match errors in the ED */

	if ( type == xieValSingleBand && xp->screenDepth != image->depth[ 0 ] )
	{
		depthmismatch = 1;
		XIELut = CreatePointLut( xp, p, 1 << image->depth[ 0 ],
                	1 << xp->screenDepth, False ); 
	}	

	if ( IsGrayVisual( cclass ) )
		atom = XA_RGB_GRAY_MAP;
	else
		atom = XA_RGB_BEST_MAP;
	if ( type == xieValTripleBand && !IsTrueColorOrDirectColor( cclass ) && !IsStaticVisual( cclass ) )
	{
		if ( !GetStandardColormap( xp, &stdCmap, atom ) )
			reps = 0;
		else
			InstallThisColormap( xp, stdCmap.colormap );
	}
	else if ( type == xieValTripleBand )
	{
		if ( !IsStaticVisual( cclass ) )
			InstallColorColormap( xp );
		CreateStandardColormap( xp, &stdCmap, atom );
	}

	if ( reps )
	{
		if ( type == xieValSingleBand ) 
		{
			XIEPhotomap = GetXIEPhotomap( xp, p, 1 );
			encode_tech = xieValEncodeUncompressedSingle;
		}
		else
		{
			encode_tech = xieValEncodeUncompressedTriple;

			gp.geoType = GEO_TYPE_SCALE;
			gp.geoHeight = 128;
			gp.geoWidth = 128;
			gp.geoXOffset = 0;
			gp.geoYOffset = 0;
			gp.geoTech = xieValGeomNearestNeighbor;

			if ( ptr->useMyLevelsPlease == True )
			{
				levels[ 0 ] = 1 << ptr->myBits[ 0 ];
				levels[ 1 ] = 1 << ptr->myBits[ 1 ];
				levels[ 2 ] = 1 << ptr->myBits[ 2 ];

				out_high[ 0 ] = ( float ) ( ( 1 << ptr->myBits[ 0 ] ) - 1 );
				out_high[ 1 ] = ( float ) ( ( 1 << ptr->myBits[ 1 ] ) - 1 );
				out_high[ 2 ] = ( float ) ( ( 1 << ptr->myBits[ 2 ] ) - 1 );
				in_low[ 0 ] = 0;
				in_low[ 1 ] = 0;
				in_low[ 2 ] = 0;
				in_high[ 0 ] = ( 1 << image->depth[ 0 ] ) - 1;
				in_high[ 1 ] = ( 1 << image->depth[ 1 ] ) - 1;
				in_high[ 2 ] = ( 1 << image->depth[ 2 ] ) - 1;
				out_low[ 0 ] = 0.0;
				out_low[ 1 ] = 0.0;
				out_low[ 2 ] = 0.0;

				XIEPhotomap = GetXIEConstrainedGeometryTriplePhotomap( 
					xp, p, 1, levels, 
					xieValConstrainClipScale, in_low, 
					in_high, out_low, out_high, &gp );
			}
			else
			{
				XIEPhotomap = GetXIEGeometryTriplePhotomap( xp, p, 1, &gp );
			}
		}
	}
	if ( reps )
	{
		XIEPhotomap2 = XieCreatePhotomap( xp->d ); 
		if ( XIEPhotomap == ( XiePhotomap ) NULL || 
			XIEPhotomap2 == ( XiePhotomap ) NULL )
		{
			reps = 0;
		}
		else if ( reps )
		{
			decode_notify = False;
			testSize = ptr->floElements;

			if ( type == xieValSingleBand ) 
			{
				flo1_elements = 2;
				flo2_elements = 2;
			}
			else
			{
				flo1_elements = 2;
				flo2_elements = 4;
			}
			if ( depthmismatch == 1 && type == xieValSingleBand  )
				flo2_elements+=2;

			flograph1 = XieAllocatePhotofloGraph(flo1_elements);	
			if ( flograph1 == ( XiePhotoElement * ) NULL )
			{
				fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
				reps = 0;
			}
		}
	}
	if ( reps )
	{
		flograph2 = XieAllocatePhotofloGraph(flo2_elements);	
		if ( flograph2 == ( XiePhotoElement * ) NULL )
		{
			fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
			reps = 0;
		}

		flo = ( XiePhotoflo * ) malloc( ( testSize << 1 ) * sizeof( XiePhotoflo ) );
		if ( flo == ( XiePhotoflo * ) NULL )
		{
			fprintf( stderr, "Couldn't allocate vector of flos\n" );
			reps = 0;
		}
		else
		{
			for ( i = 0; i < testSize; i++ )
			{
				flo[ i ] = ( XiePhotoflo ) NULL;
			}
		}

		encode_parms = (char **) malloc( sizeof( char * ) * testSize );
		if ( encode_parms == ( char ** ) NULL )
		{
			fprintf( stderr, "Couldn't allocate encode parameter table\n" );
                        reps = 0;
		}
		else
		{
			for ( i = 0; i < testSize; i++ )
			{
				encode_parms[ i ] = ( char * ) NULL;
			}
		}
	}

	if ( reps && type == xieValTripleBand )
	{
		in_low[ 0 ] = 0.0;
		in_low[ 1 ] = 0.0;
		in_low[ 2 ] = 0.0;
		if ( ptr->useMyLevelsPlease == True )
		{	
			in_high[ 0 ] = ( float ) ( 1 << ptr->myBits[ 0 ] ) - 1;
			in_high[ 1 ] = ( float ) ( 1 << ptr->myBits[ 1 ] ) - 1;
			in_high[ 2 ] = ( float ) ( 1 << ptr->myBits[ 2 ] ) - 1;
		}
		else
		{
			in_high[ 0 ] = ( float ) ( 1 << image->depth[ 0 ] ) - 1;
			in_high[ 1 ] = ( float ) ( 1 << image->depth[ 1 ] ) - 1;
			in_high[ 2 ] = ( float ) ( 1 << image->depth[ 2 ] ) - 1;
		}
		out_low[ 0 ] = 0; 
		out_low[ 1 ] = 0;
		out_low[ 2 ] = 0;

		if ( IsTrueColorOrDirectColor( cclass ) )
		{
			out_high[ 0 ] = TripleTrueOrDirectLevels( xp ) - 1;
			out_high[ 1 ] = out_high[ 2 ] = out_high[ 0 ];

			/* make a fake StandardColormap */

			mult[ 0 ] = stdCmap.red_mult;
			mult[ 1 ] = stdCmap.green_mult;
			mult[ 2 ] = stdCmap.blue_mult;
			bias = ( float ) stdCmap.base_pixel;
		}
		else
		{
			mult[ 0 ] = stdCmap.red_mult;
			mult[ 1 ] = stdCmap.green_mult;
			mult[ 2 ] = stdCmap.blue_mult;
			bias = ( float ) stdCmap.base_pixel;

			out_high[ 0 ] = stdCmap.red_max;
			out_high[ 1 ] = stdCmap.green_max;
			out_high[ 2 ] = stdCmap.blue_max;
		}

		if ( in_high[ 0 ] < out_high[ 0 ] ||
			in_high[ 1 ] < out_high[ 1 ] ||
			in_high[ 2 ] < out_high[ 2 ] )
		{
			constrainFlag = 1;
                	parms = ( char * ) XieTecClipScale( in_low, in_high, out_low, out_high);
		}
		else
			constrainFlag = 0;
	}

	if ( reps )	
	{
		techParmOffset = 0;
		floIdx = 0;

		for ( i = 0; i < testSize; i++ )
		{
			idx = 0;
			
			XieFloImportPhotomap(&flograph1[idx], 
				( i == 0 ? XIEPhotomap : XIEPhotomap2 ), 
				decode_notify);
			idx++; 
			if ( type == xieValSingleBand )
			{
				encode_single = XieTecEncodeUncompressedSingle(
					ptr->fillOrder[ techParmOffset ],
					ptr->pixelOrder[ techParmOffset ],
					ptr->pixelStride[ techParmOffset ],
					ptr->scanlinePad[ techParmOffset ]
				);
				encode_parms[i] = ( char * ) encode_single;
			}
			else
			{
				encode_triple = XieTecEncodeUncompressedTriple(
					ptr->fillOrder[ techParmOffset ],
					ptr->pixelOrder[ techParmOffset ],
					ptr->bandOrder[ techParmOffset ],
					ptr->interleave[ techParmOffset ],
					&ptr->pixelStride[ techParmOffset * 3],
					&ptr->scanlinePad[ techParmOffset * 3 ]
				);
				encode_parms[i] = ( char * ) encode_triple;
			}

			techParmOffset++;
			if ( encode_parms[i] == ( char * ) NULL )
			{
				fprintf( stderr, "Could not get encode technique parameters\n" );
				reps = 0;
				break;
			}

			XieFloExportPhotomap(&flograph1[idx],
				idx,
				XIEPhotomap2,
				encode_tech,
				encode_parms[i]
			);

			idx = 0;
			XieFloImportPhotomap(&flograph2[idx], XIEPhotomap2, 
				decode_notify);
			idx++; 

			if ( type == xieValSingleBand && depthmismatch == 1 ) 
			{
				XieFloImportLUT(&flograph2[idx], XIELut );
				idx++;

				domain.offset_x = 0;
				domain.offset_y = 0;
				domain.phototag = 0;

				XieFloPoint(&flograph2[idx],
					idx - 1,
					&domain,
					idx,
					0x7
				);
				idx++;	
			}
			else if (type == xieValTripleBand)
			{
				levels[ 0 ] = ( long ) out_high[ 0 ] + 1;
				levels[ 1 ] = ( long ) out_high[ 1 ] + 1;
				levels[ 2 ] = ( long ) out_high[ 2 ] + 1;

				if ( constrainFlag )
				{
					XieFloConstrain( &flograph2[idx],
						idx,
						levels,
						xieValConstrainClipScale,
						parms
					);
				}
				else
				{
					XieFloDither(&flograph2[idx],
						idx, 
						0x07,
						levels,
						xieValDitherDefault,
						( char * ) NULL
					);
				}
				idx++;
			}

			if ( type == xieValTripleBand )
			{
				c1[ 0 ] = mult[ 0 ]; 
				c1[ 1 ] = mult[ 1 ]; 
				c1[ 2 ] = mult[ 2 ];

			        XieFloBandExtract( &flograph2[idx], idx, 
					1 << xp->vinfo.depth, bias, c1 ); 
				idx++;
			}

			if ( reps )
			{
				XieFloExportDrawable(&flograph2[idx],
					idx,     /* source phototag number */
					drawableWindow,
					xp->fggc,
					0,
					0
				);
				flo[floIdx] = XieCreatePhotoflo( xp->d, flograph1, flo1_elements );
				floIdx++;
				flo[floIdx] = XieCreatePhotoflo( xp->d, flograph2, flo2_elements );
				floIdx++;
			}
		}
	}

	if ( reps )
	{
		flo_notify = False;
		dontClear = True;

		/* display the normal image, dithered to the levels of
		   the output screen, in the left hand window */

		if ( type == xieValSingleBand )
		{
			GetXIEWindow( xp, p, xp->w, 1 );
		}
		else 
		{
			levels[ 0 ] = out_high[ 0 ] + 1;
			levels[ 1 ] = out_high[ 1 ] + 1;
                        levels[ 2 ] = out_high[ 2 ] + 1;

			GetXIEDitheredStdTripleWindow( xp, p, xp->w, 1,
				xieValDitherDefault, 0, levels, &stdCmap );
		}
                XMoveWindow( xp->d, drawableWindow, WIDTH + 10, 0 );
                XMapRaised( xp->d, drawableWindow );
                XSync( xp->d, 0 );
	}
	if ( !reps )
		EndFunnyEncode( xp, p );
	return( reps );
}

void 
DoFunnyEncode(XParms xp, Parms p, int reps)
{
    	int     i, j;

    	for (i = 0; i != reps; i++) 
	{
		for ( j = 0; j < testSize; j++ )
		{
			XieExecutePhotoflo( xp->d, flo[ j ], True );
		        WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo[ j ], 
				0, False );
		}
    	}
}

void 
EndFunnyEncode(XParms xp, Parms p)
{

	dontClear = False;
	if ( type == xieValTripleBand && !IsStaticVisual( cclass ) )
                InstallGrayColormap( xp );

        XUnmapWindow( xp->d, drawableWindow );
	FreeFunnyEncodeStuff( xp, p );
}

void
FreeFunnyEncodeStuff(XParms xp, Parms p)
{
	int	i;

	if ( parms )
	{
		XFree( parms );
		parms = ( char * ) NULL;
	}
	if ( encode_parms )
	{
		for ( i = 0; i < testSize; i++ )
		{
			if ( encode_parms[ i ] )
				XFree( encode_parms[ i ] ); 
		}
		free( encode_parms );
		encode_parms = ( char ** ) NULL;
	}

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

        if ( flo )
        {
		for ( i = 0; i < testSize; i++ )
		{
                       if ( flo[ i ] )
                                XieDestroyPhotoflo( xp->d, flo[ i ] );
		}
		free( flo );
                flo = ( XiePhotoflo * ) NULL;
        }

        if ( flograph1 )
        {
                XieFreePhotofloGraph(flograph1, flo1_elements);
                flograph1 = ( XiePhotoElement * ) NULL;
        }

        if ( flograph2 )
        {
                XieFreePhotofloGraph(flograph2, flo2_elements);
                flograph2 = ( XiePhotoElement * ) NULL;
        }
}

