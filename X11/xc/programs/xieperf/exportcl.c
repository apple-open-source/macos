/* $Xorg: exportcl.c,v 1.4 2001/02/09 02:05:47 xorgcvs Exp $ */

/**** module exportcl.c ****/
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
  
	exportcl.c -- export client flo element tests 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/exportcl.c,v 1.6 2001/12/14 20:01:49 dawes Exp $ */

#include "xieperf.h"
#include <stdio.h>

static XiePhotomap XIEPhotomap;
static XiePhotomap ControlPlane;
static XieLut	XIELut;
static XieRoi	XIERoi;
static unsigned char *lut;
static int lutSize;
static XieRectangle *rects;
static int rectsSize;
extern unsigned int dataCheckSum1;

static XIEimage	*image;
static int flo_notify;
static XiePhotoElement *flograph;
static XiePhotoflo flo;
static XieHistogramData *histos;
static int flo_elements;
static int useDomain;
static int histoSrc;

extern Window monitorWindow;
extern Bool dontClear;
static char *data;

extern char *imagepath;

int 
InitExportClientPhoto(XParms xp, Parms p, int reps)
{
        char *encode_params=NULL;
	int *size;
	unsigned char pixel_stride[ 3 ];
	unsigned char scanline_pad[ 3 ];
	char buf[ 64 ];

	flograph = ( XiePhotoElement * ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;
	flo = ( XiePhotoflo ) NULL;

	image = p->finfo.image1;
	if ( !image )
		return ( 0 );

	sprintf( buf, "%s/%s", imagepath, image->fname );
        size = &image->fsize;
        *size = GetFileSize( buf );
	if ( *size == 0 )
		return( 0 );

	data = ( char * ) malloc( *size );
	if ( data == ( char * ) NULL )
		return( 0 );
	flograph = XieAllocatePhotofloGraph(2);	
	if ( flograph == ( XiePhotoElement * ) NULL )
	{
		fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
		reps = 0;
	}
	else
	{
		switch( image->decode )
		{
		case xieValDecodeJPEGBaseline:
			if ( ( XIEPhotomap = GetXIETriplePhotomap( xp, p, 1 ) ) == 
				( XiePhotomap ) NULL )
			{
				reps = 0;
			}
			else
			{
				unsigned char h_samples[3];
				unsigned char v_samples[3];

				h_samples[0] = h_samples[1] = h_samples[2] = 1;
				v_samples[0] = v_samples[1] = v_samples[2] = 1;

				encode_params = ( char * ) XieTecEncodeJPEGBaseline(
					image->interleave,
					image->band_order,
					h_samples,
					v_samples,
					( char * ) NULL,
					0,
					( char * ) NULL,
					0,
					( char * ) NULL,
					0 
				);
			}	
			break;
		case xieValDecodeUncompressedTriple:
			if ( ( XIEPhotomap = GetXIETriplePhotomap( xp, p, 1 ) ) == 
				( XiePhotomap ) NULL )
			{
				reps = 0;
			}
			else
			{
				pixel_stride[ 0 ] = image->pixel_stride[ 0 ];
				pixel_stride[ 1 ] = image->pixel_stride[ 1 ];
				pixel_stride[ 2 ] = image->pixel_stride[ 2 ];
				scanline_pad[ 0 ] = image->scanline_pad[ 0 ];
				scanline_pad[ 1 ] = image->scanline_pad[ 1 ];
				scanline_pad[ 2 ] = image->scanline_pad[ 2 ];

	        		encode_params = ( char * ) XieTecEncodeUncompressedTriple(
					image->fill_order,
					image->pixel_order,		
					image->band_order,
					xieValBandByPixel,	
					pixel_stride,
					scanline_pad
				);
			}
			break;
		case xieValDecodeUncompressedSingle:
			if ( ( XIEPhotomap = GetXIEPhotomap( xp, p, 1 ) ) == 
				( XiePhotomap ) NULL )
			{
				reps = 0;
			}
			else
			{
	        		encode_params = ( char * ) XieTecEncodeUncompressedSingle(
					image->fill_order,
					image->pixel_order,		
					image->pixel_stride[ 0 ],
					image->scanline_pad[ 0 ]
				);
			}
			break;
		case xieValDecodeG31D:
			if ( ( XIEPhotomap = GetXIEFAXPhotomap( xp, p, 1,
				False ) ) == ( XiePhotomap ) NULL )
			{
				reps = 0;
			}
			else
			{
	        		encode_params = ( char * ) XieTecEncodeG31D(
					False,
					False,		
					xieValLSFirst
				);
			}
			break;
		case xieValDecodeG32D:
			if ( ( XIEPhotomap = GetXIEFAXPhotomap( xp, p, 1,
				False ) ) == ( XiePhotomap ) NULL )
			{
				reps = 0;
			}
			else
			{
	        		encode_params = ( char * ) XieTecEncodeG32D(
					False,
					False,
					False,		
					xieValLSFirst,
					1
				);
			}
			break;
		case xieValDecodeG42D:
			if ( ( XIEPhotomap = GetXIEFAXPhotomap( xp, p, 1,
				False ) ) == ( XiePhotomap ) NULL )
			{
				reps = 0;
			}
			else
			{
	        		encode_params = ( char * ) XieTecEncodeG42D(
					False,
					False,
					xieValLSFirst
				);
			}
			break;
		case xieValDecodeTIFF2:
			if ( ( XIEPhotomap = GetXIEFAXPhotomap( xp, p, 1,
				False ) ) == ( XiePhotomap ) NULL )
			{
				reps = 0;
			}
			else
			{
	        		encode_params = ( char * ) XieTecEncodeTIFF2(
					xieValLSFirst,
					False		
				);
			}
			break;
		case xieValDecodeTIFFPackBits:
			if ( ( XIEPhotomap = GetXIEFAXPhotomap( xp, p, 1,
				False ) ) == ( XiePhotomap ) NULL )
			{
				reps = 0;
			}
			else
			{
	        		encode_params = ( char * ) XieTecEncodeTIFFPackBits(
					xieValLSFirst
				);
			}
			break;
		default:
			reps = 0;
			break;
		}

		if ( reps )
		{
			XieFloImportPhotomap(&flograph[0], XIEPhotomap, False );
			XieFloExportClientPhoto(&flograph[1],
				1,              /* source phototag number */
				xieValNewData,
				image->decode,
				encode_params
			);

			flo = XieCreatePhotoflo( xp->d, flograph, 2 );
			flo_notify = True;
		}
	}
	if ( encode_params )
	{
		if ( image->decode == xieValDecodeJPEGBaseline )
			XieFreeEncodeJPEGBaseline( 
			( XieEncodeJPEGBaselineParam * ) encode_params );
		else
			XFree( encode_params );	
	}
	if ( !reps )
		FreeExportClientPhotoStuff( xp, p );
	return( reps );
}

int 
InitExportClientLUT(XParms xp, Parms p, int reps)
{
	int	i;
        XieOrientation band_order = xieValLSFirst;
        XieLTriplet     start, length;

	XIELut = ( XieLut ) NULL;
	flograph = ( XiePhotoElement * ) NULL;
	flo = ( XiePhotoflo ) NULL;

	histos = ( XieHistogramData * ) malloc( sizeof( XieHistogramData ) *
		xp->vinfo.colormap_size );
	lutSize = ( xp->vinfo.colormap_size ) * sizeof( unsigned char );
	lut = (unsigned char *) malloc( lutSize );
	data = (char *) malloc( lutSize );
	if ( lut == ( unsigned char * ) NULL || data == ( char * ) NULL ||
		histos == ( XieHistogramData * ) NULL )
	{
		reps = 0;
	}
	else
	{
		for ( i = 0; i < lutSize; i++ )
		{
			lut[ i ] = ( xp->vinfo.colormap_size ) - i - 1;
		}
		if ( ( XIELut = GetXIELut( xp, p, lut, lutSize, lutSize ) ) ==
			( XieLut ) NULL )
		{
			reps = 0;
		}	
	}
	if ( reps )
	{
		flo_elements = 2;
		flograph = XieAllocatePhotofloGraph( flo_elements );	
		if ( flograph == ( XiePhotoElement * ) NULL )
		{
			fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
			reps = 0;
		}
	}
	if ( reps )
	{
		XieFloImportLUT(&flograph[0], XIELut);

		length[ 0 ] = xp->vinfo.colormap_size;
		length[ 1 ] = 0;
		length[ 2 ] = 0;

		start[ 0 ] = 0;
		start[ 1 ] = 0;
		start[ 2 ] = 0;

		XieFloExportClientLUT(&flograph[1],
			1,              /* source phototag number */
			band_order,
			xieValNewData,
			start,
			length
		);

		flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
		flo_notify = True;
	}
	if ( !reps )
	{
		FreeExportClientLUTStuff( xp, p );
	}
	else
	{
		XMapRaised( xp->d, monitorWindow );
		XSync( xp->d, 0 );
	}
	return( reps );
}

int 
InitExportClientROI(XParms xp, Parms p, int reps)
{
	int	i;

        XIERoi = ( XieRoi ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;

	rectsSize = (( ExportClParms * ) p->ts)->numROIs;
	rects = (XieRectangle *)malloc( rectsSize * sizeof( XieRectangle ) );
	data = (char *) NULL;
	if ( rects == ( XieRectangle * ) NULL )
	{
		reps = 0;
	}
	else
	{
		for ( i = 0; i < rectsSize; i++ )
		{
			rects[ i ].x = i * 10;
			rects[ i ].y = i * 10; 
			rects[ i ].width = i * 10;
			rects[ i ].height = i * 10;
			XFillRectangle( xp->d, xp->w, xp->fggc,
				rects[ i ].x,
				rects[ i ].y,
				rects[ i ].width,
				rects[ i ].height
			 );
		}
		if ( ( XIERoi = GetXIERoi( xp, p, rects, rectsSize ) ) ==
			( XieRoi ) NULL )
		{
			reps = 0;
		}
		else
		{
			flo_elements = 2;
			flograph = XieAllocatePhotofloGraph( flo_elements );	
			if ( flograph == ( XiePhotoElement * ) NULL )
			{
				fprintf( stderr, 
					"XieAllocatePhotofloGraph failed\n" );
				reps = 0;
			}
			else
			{
				XieFloImportROI(&flograph[0], XIERoi);
				XieFloExportClientROI(&flograph[1],
					1,       /* source phototag number */
					xieValNewData
				);
				flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
				flo_notify = True;
			}
		}
	}
	if ( !reps )
	{
		FreeExportClientROIStuff( xp, p );
	}
	else
		dontClear = True;
	return( reps );
}

int 
InitExportClientHistogram(XParms xp, Parms p, int reps)
{
	XieProcessDomain domain;
	int	idx, src;
        XieRectangle    rect;
	ExportClParms	*parms;
	XIEimage *image;
	int constrainflag = 0;

	parms = ( ExportClParms * )p->ts;
	XIELut = ( XieLut ) NULL;
	XIERoi = ( XieRoi ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;
	ControlPlane = ( XiePhotomap ) NULL;
	histos = ( XieHistogramData * ) NULL;
	flograph = ( XiePhotoElement * ) NULL;
	flo = ( XiePhotoflo ) NULL;

	if ( parms )	
        	useDomain = parms->useDomain;
	else
		useDomain = DomainNone;

	flo_elements = 3;

	image = p->finfo.image1;
	if ( !image )
	{
		reps = 0;
	}

	if ( reps && ( xp->screenDepth != image->depth[ 0 ] ) )
        {
		flo_elements+=2;
                constrainflag = 1;
                if ( ( XIELut = CreatePointLut( xp, p,
                        1 << image->depth[ 0 ], 
			1 << xp->screenDepth, False ) )
                        == ( XieLut ) NULL )
                {
                        reps = 0;
                }
        }

	if ( reps && useDomain == DomainROI )
	{
                rect.x = parms->x;
                rect.y = parms->y;
                rect.width = parms->width;
                rect.height = parms->height;

                if ( ( XIERoi = GetXIERoi( xp, p, &rect, 1 ) ) ==
                        ( XieRoi ) NULL )
                {
			reps = 0;
		}
		flo_elements+=1;
	}
	else if ( reps && useDomain == DomainCtlPlane )
	{
		ControlPlane = GetControlPlane( xp, 2 );
		if ( ControlPlane == ( XiePhotomap ) NULL )
			reps = 0;
		flo_elements+=1;
	}

	if ( reps )
	{
		histos = ( XieHistogramData * ) malloc( sizeof( XieHistogramData ) * ( 1 << image->depth[ 0 ] ) );
		if ( histos == ( XieHistogramData * ) NULL )
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
		else if ( ( XIEPhotomap = 
			GetXIEPhotomap( xp, p, 1 ) ) == ( XiePhotomap ) NULL )
		{
			reps = 0;
		}
		else
		{
			domain.offset_x = 0;
			domain.offset_y = 0;
			domain.phototag = 0;

			idx = 0;
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
			src = idx;

			XieFloExportClientHistogram(&flograph[idx],
				idx,              /* source phototag number */
				&domain,
				xieValNewData
			);
			idx++;
			histoSrc = idx;
	
			if ( constrainflag )
			{
				XieFloImportLUT(&flograph[idx], XIELut );
				idx++;

				domain.phototag = 0;
				domain.offset_x = 0;
				domain.offset_y = 0;
				XieFloPoint(&flograph[idx],
					src,
					&domain,
					idx,
					0x7
				);
				idx++;
				src = idx;
			}	

			XieFloExportDrawable(&flograph[idx],
				src,     	/* source phototag number */
				xp->w,
				xp->fggc,
				0,       /* x offset in window */
				0        /* y offset in window */
			);
			idx++;

			flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
			flo_notify = True;
		}
	}
	if ( reps )
	{
		XMapRaised( xp->d, monitorWindow );
		XSync( xp->d, 0 );
	}
	else 
	{
		FreeExportClientHistogramStuff( xp, p );
	}
	return( reps );
}

void 
DoExportClientPhotoCSum(XParms xp, Parms p, int reps)
{
    	int     i, done;
	unsigned int checksum;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
		if ( image->bandclass == xieValTripleBand && 
			image->interleave == xieValBandByPlane )
		{
			ReadNotifyExportTripleData( xp, p, 0, flo, 2, 1, 
				image->fsize, &data, &done );
		}
		else
		{
			ReadNotifyExportData( xp, p, 0, flo, 2, 1, 
				image->fsize, &data, &done );
		}
		WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo, 0, False );	
		checksum = CheckSum( data, image->fsize );
		if ( checksum != image->chksum )
		{
			fprintf( stderr, 
				"Photomap not read correctly by client\n" );
			break;
		}
    	}
}

void 
DoExportClientPhoto(XParms xp, Parms p, int reps)
{
    	int     i, done;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
		if ( image->bandclass == xieValTripleBand && 
			image->interleave == xieValBandByPlane )
		{
			ReadNotifyExportTripleData( xp, p, 0, flo, 2, 1, 
				image->fsize, &data, &done );
		}
		else
		{
			ReadNotifyExportData( xp, p, 0, flo, 2, 1, 
				image->fsize, &data, &done );
		}
		WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo, 0, False );	
    	}
}

void 
DoExportClientHistogram(XParms xp, Parms p, int reps)
{
    	int     i, done, numHistos;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
		XSync( xp->d, 0 );
		numHistos = ReadNotifyExportData( xp, p, 0, flo, histoSrc, 
			sizeof( XieHistogramData ), 0, ( char ** ) &histos, 
			&done ) / sizeof( XieHistogramData );
		WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo, 0, False );	
		DrawHistogram( xp, monitorWindow, ( XieHistogramData * ) histos,
			numHistos, 1<< xp->vinfo.depth ); 
    	}
}

void 
DoExportClientLUT(XParms xp, Parms p, int reps)
{
    	int     i, j, done;

    	for (i = 0; i != reps; i++) {
		XieExecutePhotoflo( xp->d, flo, flo_notify );
                XSync( xp->d, 0 );
                ReadNotifyExportData( xp, p, 0, flo, 2,
                        sizeof( unsigned char ), lutSize, 
			&data, &done );
               	WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo, 0, False ); 
		for ( j = 0; j < lutSize; j++ )
		{
			histos[ j ].value = j;
			histos[ j ].count = (( unsigned char * ) data)[ j ];
		}
                if ( memcmp( lut, data, lutSize * sizeof( unsigned char ) ) )
                {
                        fprintf( stderr, "ExportClientLUT failed\n" );
                        break;
                }
		DrawHistogram( xp, monitorWindow, ( XieHistogramData * ) histos,
			lutSize, xp->vinfo.colormap_size ); 
    	}
}

void 
DoExportClientROI(XParms xp, Parms p, int reps)
{
    	int     i, j, done, n;
	GC	gc;

	gc = xp->fggc;
    	for (i = 0; i != reps; i++) 
	{
		done = 0;
		XieExecutePhotoflo( xp->d, flo, flo_notify );
                XSync( xp->d, 0 );
               	n = ReadNotifyExportData( xp, p, 0, flo, 2, 
                       	sizeof( XieRectangle ), 0, &data, &done );
	
		if ( n == 0 )
			continue;

		for ( j = 0; j < n / sizeof( XieRectangle ); j++ )	
		{
			XDrawRectangle( xp->d, xp->w, gc,
				( (( XieRectangle * )data)[ j ] ).x,
				( (( XieRectangle * )data)[ j ] ).y,
				( (( XieRectangle * )data)[ j ] ).width,
				( (( XieRectangle * )data)[ j ] ).height );
			if ( gc == xp->fggc )
				gc = xp->bggc;
			else
				gc = xp->fggc;
		}
               	WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo, 0, False ); 
    	}
}

void 
EndExportClientLUT(XParms xp, Parms p)
{
	XUnmapWindow( xp->d, monitorWindow );
	FreeExportClientLUTStuff( xp, p );
}

void 
EndExportClientPhoto(XParms xp, Parms p)
{
	FreeExportClientPhotoStuff( xp, p );
}

void 
EndExportClientHistogram(XParms xp, Parms p)
{
	XUnmapWindow( xp->d, monitorWindow );
	FreeExportClientHistogramStuff( xp, p );
}

void 
EndExportClientROI(XParms xp, Parms p)
{
	dontClear = False;
	FreeExportClientROIStuff( xp, p );
}

void
FreeExportClientPhotoStuff(XParms xp, Parms p)
{
	if ( data )
	{
		free( data );
		data = ( char * ) NULL;
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

	if ( XIEPhotomap && IsPhotomapInCache( XIEPhotomap ) == False )
	{
                XieDestroyPhotomap( xp->d, XIEPhotomap );
                XIEPhotomap = ( XiePhotomap ) NULL;
	}
}

void
FreeExportClientROIStuff(XParms xp, Parms p)
{
        if ( data )
        {
                free( data );
                data = ( char * ) NULL;
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

        if ( XIERoi )
        {
                XieDestroyROI( xp->d, XIERoi );
                XIERoi = ( XieRoi ) NULL;
        }

	if ( rects )
	{
		free( rects );
		rects = ( XieRectangle * ) NULL;
	}
}

void
FreeExportClientLUTStuff(XParms xp, Parms p)
{
	if ( data )
	{
		free( data );
		data = ( char * ) NULL;
	}

	if ( histos )
	{
		free( histos );
		histos = ( XieHistogramData * ) NULL;
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
FreeExportClientHistogramStuff( xp, p )
XParms	xp;
Parms	p;
{
        if ( XIERoi )
        {
                XieDestroyROI( xp->d, XIERoi );
                XIERoi = ( XieRoi ) NULL;
        }
	if ( XIEPhotomap && IsPhotomapInCache( XIEPhotomap ) == False )
	{
		XieDestroyPhotomap( xp->d, XIEPhotomap );
		XIEPhotomap = ( XiePhotomap ) NULL;
	}
	if ( ControlPlane && IsPhotomapInCache( ControlPlane ) == False )
	{
		XieDestroyPhotomap( xp->d, ControlPlane );
		ControlPlane = ( XiePhotomap ) NULL;
	}
	if ( XIELut )
	{
                XieDestroyLUT( xp->d, XIELut );
                XIELut = ( XieLut ) NULL;
	}
	if ( histos )
	{
		free( histos );
		histos = ( XieHistogramData * ) NULL;
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

