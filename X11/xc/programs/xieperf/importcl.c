/* $Xorg: importcl.c,v 1.4 2001/02/09 02:05:47 xorgcvs Exp $ */

/**** module importcl.c ****/
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
  
	importcl.c -- import client flo element tests 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/importcl.c,v 1.6 2001/12/14 20:01:50 dawes Exp $ */

#include "xieperf.h"
#include <stdio.h>

static XiePhotomap XIEPhotomap;
static XiePhotomap XIEPhotomap2;
static XieLut	XIELut;
static XieRoi	XIERoi;
static unsigned char *lut;
static int lutSize;
static XieRectangle *rects;
static int rectsSize;

static XiePhotoflo flo;
static int flo_notify;
static XiePhotoElement *flograph;
static int flo_elements;

int 
InitImportClientPhoto(XParms xp, Parms p, int reps)
{
	XIEimage *image;
        int decode_notify;
        char *encode_params=NULL;
        XieLTriplet width, height, levels;
	char *decode_params=NULL;
        unsigned char pixel_stride[ 3 ];
        unsigned char left_pad[ 3 ];
        unsigned char scanline_pad[ 3 ];
        XieEncodeTechnique encode_tech=xieValEncodeServerChoice;

        flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;
	XIEPhotomap2 = ( XiePhotomap ) NULL;

	image = p->finfo.image1;
	if ( !image )
	{
		reps = 0;
	}
	else
	{
		if ( TechniqueSupported( xp, xieValDecode, image->decode ) == False )
			return( XiePhotomap ) NULL;

		if (!GetImageData( xp, p, 1 ))
			reps = 0;
	}
	if ( reps )
	{
		XIEPhotomap = XieCreatePhotomap(xp->d);
		XIEPhotomap2 = XieCreatePhotomap(xp->d);
	}
	if ( reps )
	{
		decode_notify = False;
		flo_elements = 2;
		flograph = XieAllocatePhotofloGraph( flo_elements );	
		if ( flograph == ( XiePhotoElement * ) NULL )
		{
			fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
			reps = 0;
		}
		else if ( image->decode == xieValDecodeUncompressedTriple )
		{
			pixel_stride[ 0 ] = image->pixel_stride[ 0 ];
			pixel_stride[ 1 ] = image->pixel_stride[ 1 ];
			pixel_stride[ 2 ] = image->pixel_stride[ 2 ];
			left_pad[ 0 ] = image->left_pad[ 0 ];
			left_pad[ 1 ] = image->left_pad[ 1 ]; 
			left_pad[ 2 ] = image->left_pad[ 2 ];
			scanline_pad[ 0 ] = image->scanline_pad[ 0 ];
			scanline_pad[ 1 ] = image->scanline_pad[ 1 ];
			scanline_pad[ 2 ] = image->scanline_pad[ 2 ];

			decode_params = ( char * ) XieTecDecodeUncompressedTriple(
				image->fill_order,
				xieValLSFirst,
				xieValLSFirst,
				xieValBandByPixel,
				pixel_stride,
				left_pad,
				scanline_pad
			);
		}
		else if ( image->decode == xieValDecodeJPEGBaseline )
		{
			decode_params = ( char * ) XieTecDecodeJPEGBaseline(
				image->interleave,
				image->band_order,
				True
			);
		}
		else if ( image->decode == xieValDecodeJPEGLossless )
		{
			decode_params = ( char * ) XieTecDecodeJPEGLossless(
				image->interleave,
				image->band_order
			);
		}
		else if ( image->decode == xieValDecodeG31D )
		{
			decode_params = ( char * ) XieTecDecodeG31D(
				image->fill_order,
				True,
				False
			);
		}
		else if ( image->decode == xieValDecodeG32D )
		{
			decode_params = ( char * ) XieTecDecodeG32D(
				image->fill_order,
				True,
				False
			);
		}
		else if ( image->decode == xieValDecodeG42D )
		{
			decode_params = ( char * ) XieTecDecodeG42D(
				image->fill_order,
				True,
				False
			);
		}
		else if ( image->decode == xieValDecodeTIFF2 )
		{
			decode_params = ( char * ) XieTecDecodeTIFF2(
				image->fill_order,
				True,
				False
			);
		}
		else if ( image->decode == xieValDecodeTIFFPackBits )
		{
			decode_params = ( char * ) XieTecDecodeTIFFPackBits(
				image->fill_order,
				True
			);
		}
		else if ( image->decode == xieValDecodeUncompressedSingle )
		{
                        decode_params = ( char * ) XieTecDecodeUncompressedSingle(
                                image->fill_order,
                                image->pixel_order,
                                image->pixel_stride[ 0 ],
                                image->left_pad[ 0 ],
                                image->scanline_pad[ 0 ]
                        );
		}
		if ( decode_params == ( char * ) NULL )
		{
			reps = 0;
		}
		if ( reps )
		{
                       	width[ 0 ] = image->width[ 0 ];
			width[ 1 ] = image->width[ 1 ];
			width[ 2 ] = image->width[ 2 ];
                        height[ 0 ] = image->height[ 0 ];
			height[ 1 ] = image->height[ 1 ];
			height[ 2 ] = image->height[ 2 ];

                        levels[ 0 ] = image->levels[ 0 ];
			levels[ 1 ] = image->levels[ 1 ];
			levels[ 2 ] = image->levels[ 2 ];

			XieFloImportClientPhoto(&flograph[0],
				image->bandclass,
				width, height, levels,
				decode_notify,
				image->decode, (char *)decode_params
			);

			XieFloExportPhotomap(&flograph[1],
				1,              /* source phototag number */
				XIEPhotomap2,
				encode_tech,
				encode_params
			);

			flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
			flo_notify = True;
		}
	}
	if ( decode_params )
		XFree( decode_params );
	if ( !reps )
	{
		FreeImportClientPhotoStuff( xp, p );
	}	
	return( reps );
}

int 
InitImportClientPhotoExportDrawable(XParms xp, Parms p, int reps)
{
	XIEimage *image;
        int decode_notify = 0;
        XieProcessDomain domain;
        XieLTriplet width, height, levels;
	XieDecodeUncompressedSingleParam *decode_params=NULL;
	int monoflag = 0;
	int idx;
        int cclass;

#if     defined(__cplusplus) || defined(c_plusplus)
        cclass = xp->vinfo.c_class;
#else
        cclass = xp->vinfo.class;
#endif
        flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;
	XIELut = ( XieLut ) NULL;

	image = p->finfo.image1;
	if (!GetImageData( xp, p, 1 ))
		reps = 0;
	else
		XIEPhotomap = XieCreatePhotomap(xp->d);
	if ( reps )
	{
		if ( !image )
		{
			reps = 0;
		}
		decode_notify = False;
		flo_elements = 2;
		monoflag = 0;
		if ( IsTrueColorOrDirectColor( cclass ) || xp->screenDepth != image->depth[ 0 ] )
		{
			monoflag = 1;
			flo_elements+=2;
			if ( ( XIELut = CreatePointLut( xp, p, 
				1 << image->depth[ 0 ], 
				1 << xp->screenDepth, False ) )
				== ( XieLut ) NULL )
			{
				reps = 0;
			}
                }
	}
	if ( reps )
	{
		if ( ( flograph = XieAllocatePhotofloGraph(flo_elements) ) 
			== ( XiePhotoElement * ) NULL )
		{
			fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
			reps = 0;
		}
		else
		{

			width[ 0 ] = image->width[ 0 ];
			height[ 0 ] = image->height[ 0 ];
			levels[ 0 ] = image->levels[ 0 ];

			decode_params = XieTecDecodeUncompressedSingle(
				image->fill_order,
				image->pixel_order,
				image->pixel_stride[ 0 ],
				image->left_pad[ 0 ],
				image->scanline_pad[ 0 ]
			);

			idx = 0;
			XieFloImportClientPhoto(&flograph[idx],
				image->bandclass,
				width, height, levels,
				decode_notify,
				image->decode, (char *)decode_params
			);
			idx++;

			if ( monoflag )
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
				0,
				0
			);

			flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
			flo_notify = True;
		}
	}
	if ( decode_params )
		XFree( decode_params );
	if ( !reps )
	{
		FreeImportClientPhotoStuff( xp, p );
	}	
	return( reps );
}

int 
InitImportClientLUT(XParms xp, Parms p, int reps)
{
	int i;
        XieOrientation band_order = xieValLSFirst;
        XieDataClass cclass;
        XieLTriplet start, length, levels;
        Bool merge;

	XIELut = ( XieLut ) NULL;
	flograph = ( XiePhotoElement * ) NULL;
	flo = ( XiePhotoflo ) NULL;

	lutSize = xp->vinfo.colormap_size * sizeof( unsigned char );
	lut = (unsigned char *) malloc( lutSize );

	if ( lut == ( unsigned char * ) NULL )
	{
		reps = 0;
	}
	else
	{
		for ( i = 0; i < lutSize; i++ )
		{
			lut[ i ] = ( xp->vinfo.colormap_size - 1 ) - i;
		}

		if ( ( XIELut = XieCreateLUT(xp->d) ) == ( XieLut ) NULL )
			reps = 0;
	}
	if ( reps )
	{
		flo_elements = 2;
		flograph = XieAllocatePhotofloGraph( flo_elements );	
		if ( flograph == ( XiePhotoElement * ) NULL )
		{
			reps = 0;
		}
		else
		{
			cclass = xieValSingleBand;
			band_order = xieValLSFirst;
			length[ 0 ] = xp->vinfo.colormap_size;
			length[ 1 ] = 0;
			length[ 2 ] = 0;
			levels[ 0 ] = xp->vinfo.colormap_size;
			levels[ 1 ] = 0;
			levels[ 2 ] = 0;

			XieFloImportClientLUT(&flograph[0],
				cclass,
				band_order,
				length,
				levels
			);

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
			flo_notify = True;
		}
	}
	if ( !reps )
		FreeImportClientLUTStuff( xp, p );
	return( reps );
}

int 
InitImportClientROI(XParms xp, Parms p, int reps)
{
	int	i;

	XIERoi = ( XieRoi ) NULL;
	flograph = ( XiePhotoElement * ) NULL;
	flo = ( XiePhotoflo ) NULL;

	rectsSize = (( ImportClParms * )p->ts)->numROIs;
	rects = (XieRectangle *)malloc( rectsSize * sizeof( XieRectangle ) );
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

		if ( ( XIERoi = XieCreateROI( xp->d ) ) == ( XieRoi ) NULL )
			reps = 0;
	}

	if ( reps )
	{
		flo_elements = 2;
		flograph = XieAllocatePhotofloGraph(flo_elements);	
		if ( flograph == ( XiePhotoElement * ) NULL )
		{
			fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
			reps = 0;
		}
		else
		{
			XieFloImportClientROI(&flograph[0], rectsSize);

			XieFloExportROI(&flograph[1],
				1,              /* source phototag number */
				XIERoi
			);

			flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );

			flo_notify = True;
		}
	}
	if ( !reps )
	{
		FreeImportClientROIStuff( xp, p );
	}
	return( reps );
}

void 
DoImportClientPhoto(XParms xp, Parms p, int reps)
{
    	int     i;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
                PumpTheClientData( xp, p, flo, 0, 1,
                        p->finfo.image1->data, p->finfo.image1->fsize, 0 );
               	WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo, 0, False ); 
    	}
}

void 
DoImportClientLUT(XParms xp, Parms p, int reps)
{
    	int     i;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
                PumpTheClientData( xp, p, flo, 0, 1,
                        (char *)lut, lutSize, 0 );
              	WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo, 0, False ); 
    	}
}

void 
DoImportClientROI(XParms xp, Parms p, int reps)
{
    	int     i;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
                PumpTheClientData( xp, p, flo, 0, 1,
                        (char *)rects, rectsSize * sizeof( XieRectangle ), 0 );
               	WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo, 0, False ); 
    	}
}

void 
EndImportClientLUT(XParms xp, Parms p)
{
	FreeImportClientLUTStuff( xp, p );
}

void
FreeImportClientLUTStuff(XParms xp, Parms p)
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
EndImportClientPhoto(XParms xp, Parms p)
{
	FreeImportClientPhotoStuff( xp, p );
}

void
FreeImportClientPhotoStuff(XParms xp, Parms p)
{
        if ( XIELut )
        {
                XieDestroyLUT( xp->d, XIELut );
                XIELut = ( XieLut ) NULL;
        }

        if ( XIEPhotomap )
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
EndImportClientROI(XParms xp, Parms p)
{
	FreeImportClientROIStuff( xp, p );
}

void
FreeImportClientROIStuff(XParms xp, Parms p)
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
