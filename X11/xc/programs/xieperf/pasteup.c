/* $Xorg: pasteup.c,v 1.4 2001/02/09 02:05:48 xorgcvs Exp $ */

/**** module pasteup.c ****/
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
  
	pasteup.c -- pasteup flo element tests 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/pasteup.c,v 1.6 2001/12/14 20:01:51 dawes Exp $ */

#include "xieperf.h"
#include <stdio.h>

#define NTILES 16	/* has an integer square root and be a power of 2 */
#define	SPLIT  4 	/* square root of NTILES */

static void InitSegments ( void );
static void CloseSegments ( XParms xp, Parms p );
static int SegmentPhotomap ( XParms xp, Parms p, XiePhotomap pm, 
			     XiePhotomap segvec[], int size, int split, 
			     int width, int height );
static int BuildPasteUpFlograph ( XParms xp, Parms p, 
				  XiePhotoElement **flograph, 
				  XiePhotomap segments[], int size, 
				  int split, int width, int height );
static int BuildMeSomeTiles ( XieTile tiles[], int split, int width, 
			      int height, int method, int overlap );
static void SetTileXY ( XieTile tiles[], int split, int width, int height, 
		       int method, int overlap );

static XieLut XIELut;
static XiePhotomap XIEPhotomap;
static XiePhotomap segments[NTILES];		
static XieTile	tiles[NTILES];
static XiePhotoElement *flograph;
static XiePhotoflo	flo;
static int	flo_elements;
static XieConstant constant = { 0.0, 0.0, 0.0 };

static int monoflag = 0;

static int pasteUpIdx;

int 
InitPasteUp(XParms xp, Parms p, int reps)
{	
	XIEimage *image;

        flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;
	XIELut = ( XieLut ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;
	InitSegments();

	image = p->finfo.image1;
        if ( !image )
		reps = 0;

	if ( reps )
	{
		monoflag = 0;
		if ( xp->screenDepth != image->depth[ 0 ] )
		{
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
		if ( ( XIEPhotomap = GetXIEPhotomap( xp, p, 1 ) ) == 
			( XiePhotomap ) NULL )
		{
			reps = 0;
		}

		else if ( !SegmentPhotomap( xp, p, XIEPhotomap, 
			segments, NTILES, SPLIT, image->width[ 0 ], 
			image->height[ 0 ] ) )
		{
			reps = 0;
		}

		else if ( !BuildPasteUpFlograph( xp, p, 
			&flograph, segments, NTILES, SPLIT, image->width[ 0 ], 
			image->height[ 0 ] ) )
		{
			reps = 0;
		}
	}
	if ( !reps )
		FreePasteUpStuff( xp, p );
        return( reps );
}

/* cut a photoflo into "size" little pieces. Assumption is that the values
   width and height are each congruent to 0 mod size. Caller allocates segvec */

static void
InitSegments(void)
{
	int	i;

	for ( i = 0; i < NTILES; i++ )
	{
		segments[ i ] = ( XiePhotomap ) NULL;
	}
} 

static void
CloseSegments(XParms xp, Parms p)
{
	int	i;

	for ( i = 0; i < NTILES; i++ )
	{
		if ( segments[ i ] != ( XiePhotomap ) NULL )
		{
			XieDestroyPhotomap( xp->d, segments[ i ] );
			segments[ i ] = ( XiePhotomap ) NULL;
		}
	}
}

static int
SegmentPhotomap(XParms xp, Parms p, XiePhotomap pm, XiePhotomap segvec[], 
		int size, int split, int width, int height)
{
	int	xoff, yoff;
	int     flo_elements;
	XiePhotoElement *flograph;
	int	segidx; 
	XiePhotospace photospace;
        XieEncodeTechnique encode_tech=xieValEncodeServerChoice;
        char *encode_param=NULL;
	float coefficients[ 6 ];
	int	i, retval;
	XieGeometryTechnique sample_tech = xieValGeomNearestNeighbor;
	char	*sample_param = NULL;
	unsigned long flo_id;
	int	piecewide, piecehigh;

	retval = 1;
	piecehigh = height / split; 
	piecewide = width / split; 
	photospace = XieCreatePhotospace(xp->d);
	flo_elements = 3;
	flograph = XieAllocatePhotofloGraph(flo_elements);

        if ( flograph == ( XiePhotoElement * ) NULL )
        {
                fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
		retval = 0;
        }

	/* build the photomaps */

	if ( retval )
	{
		for ( i = 0; i < size; i++ )
		{
			if ( ( segments[ i ] = XieCreatePhotomap( xp->d ) ) ==
				( XiePhotomap ) NULL )
			{
				fprintf( stderr,"XieCreatePhotomap failed\n" );
				retval = 0;
			}
		}
	}
	if ( retval )
	{			
		segidx = 0;
		coefficients[ 0 ] = 1.0;
		coefficients[ 1 ] = 0.0;
		coefficients[ 2 ] = 0.0;
		coefficients[ 3 ] = 1.0;
		flo_id = 1;
		for ( xoff = 0; xoff < width; xoff+= piecewide )
		{
			for (yoff = 0;yoff < width;yoff += piecehigh)
			{
				XieFloImportPhotomap( &flograph[ 0 ], 
					pm,
					False );
				coefficients[ 4 ] = (float) xoff;
				coefficients[ 5 ] = (float) yoff;
				XieFloGeometry( &flograph[ 1 ],
					1,
					piecewide, 
					piecehigh,
					coefficients,
					constant,
					7,
					sample_tech,
					sample_param
				);
				XieFloExportPhotomap( &flograph[ 2 ],
					2,
					segvec[ segidx++ ],
					encode_tech,
					encode_param
				);
				XieExecuteImmediate(xp->d, photospace,
					flo_id,
					True,
					flograph,   /* photoflo specification */
					flo_elements/* number of elements */
				);
				XSync( xp->d, 0 );
				WaitForXIEEvent( xp, xieEvnNoPhotofloDone, 
					flo_id, 0, False );	
				flo_id++;
			}
		}			
		XieFreePhotofloGraph(flograph, flo_elements);
		XieDestroyPhotospace(xp->d, photospace);
	}
	return( retval );
}

static int
BuildPasteUpFlograph(XParms xp, Parms p, XiePhotoElement **flograph, 
		     XiePhotomap segments[], int size, int split, 
		     int width, int height)
{
	int	i, flo_elements;
	int	tile_width, tile_height;
        XieProcessDomain domain;

	tile_width = width / split;
	tile_height = height / split;

	flo_elements = size + 2;	
	if ( monoflag )
		flo_elements+=2;

	*flograph = XieAllocatePhotofloGraph(flo_elements);	
	if ( *flograph == ( XiePhotoElement * ) NULL )
	{
		fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
		return( 0 );
	}

	/* do all of the import photo elements */

	for ( i = 0; i < size; i++ )
	{
		XieFloImportPhotomap(&(*flograph)[i], segments[ i ], False);
	}

	/* build all of the tiles, with initial dst_x and dst_y */

	if ( !BuildMeSomeTiles( tiles, split, tile_width, tile_height, 1,
		( ( PasteUpParms * ) p->ts )->overlap ) )
		return( 0 );

	/* gee, that was easy */

	/* now, for the pasteup element */ 	

	pasteUpIdx = size;
	XieFloPasteUp( &(*flograph)[ size ], width, height, constant, tiles, 
		size );
	
        if ( monoflag )
        {
	        XieFloImportLUT(&(*flograph)[size + 1], XIELut );

		domain.phototag = 0;
		domain.offset_x = 0;
		domain.offset_y = 0;
		XieFloPoint(&(*flograph)[size + 2],
			size + 1,
			&domain,
			size + 2,
			0x7
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

static int
BuildMeSomeTiles(XieTile tiles[], int split, int width, int height, 
		 int method, int overlap)
{
	int i, j, idx;

	SetTileXY( tiles, split, width, height, method, overlap );

        idx = 0;
        for ( i = 0; i < split; i++ )
        {
                for ( j = 0; j < split; j++ )
                {
                        tiles[ idx ].src = idx + 1;
                        idx++;
                }
        }
	return( 1 );
}

static void
SetTileXY(XieTile tiles[], int split, int width, int height, 
	  int method, int overlap)
{
	int	i, j, idx;

	idx = 0;
	if ( method == 0 ) 
	{
		for ( i = 0; i < split; i++ )
		{
			for ( j = 0; j < split; j++ )
			{
				if ( overlap == Overlap && i == j  )
				{
					tiles[ idx ].dst_x = i * width - ( width / 2 );
					tiles[ idx ].dst_y = j * height - ( height / 2 );
				}
				else
				{
					tiles[ idx ].dst_x = i * width;
					tiles[ idx ].dst_y = j * height;
				}
				idx++;
			}
		}
	}
	else if ( method == 1 )
	{
		for ( i = 0; i < split; i++ )
		{
			for ( j = 0; j < split; j++ )
			{
				if ( overlap == Overlap && i == j )
				{
					tiles[ idx ].dst_x = j * width - ( width / 2 );
					tiles[ idx ].dst_y = i * height - ( height / 2 );
				}
				else
				{
					tiles[ idx ].dst_x = j * width;
					tiles[ idx ].dst_y = i * height;
				}
				idx++;
			}
		}
	}
}

void 
DoPasteUp(XParms xp, Parms p, int reps)
{
    	int     i, method;
	int	flo_notify;
	XIEimage *image;
	int	width, height, overlap;

	flo_notify = True;	
	image = p->finfo.image1;
	if ( !image )
		return;
	method = 0;
	width = image->width[ 0 ];
	height = image->height[ 0 ];
	overlap = ( ( PasteUpParms * ) p->ts )->overlap;
	for ( i = 0; i != reps; i++ )
	{
                XieExecutePhotoflo( xp->d, flo, flo_notify );
		WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo, 0, False );
		XSync( xp->d, 0 );

		if ( i < reps ) 
		{
			XieFreePasteUpTiles(&flograph[ pasteUpIdx ] );
			BuildMeSomeTiles( tiles, SPLIT, width / SPLIT,
				height / SPLIT, method++, overlap );
			if ( method == 2 )
				method = 0;
			XieFloPasteUp( &flograph[ pasteUpIdx ], width, height, 
				constant, tiles, NTILES );
			XieModifyPhotoflo( xp->d, flo, pasteUpIdx + 1, 
				&flograph[pasteUpIdx], 1 );
			XSync( xp->d, 0 );
		}
    	}
}

void
EndPasteUp(XParms xp, Parms p)
{
	FreePasteUpStuff( xp, p );
}

void
FreePasteUpStuff(XParms xp, Parms p)
{
	XieFreePasteUpTiles(&flograph[ pasteUpIdx ] );

	if ( XIELut ) 
	{
		XieDestroyLUT( xp->d, XIELut );
		XIELut = ( XieLut ) NULL;
	}

	if ( XIEPhotomap && IsPhotomapInCache( XIEPhotomap ) == False )
	{
		XieDestroyPhotomap( xp->d, XIEPhotomap );
		XIEPhotomap = ( XiePhotomap ) NULL;
	}

	CloseSegments( xp, p );

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

