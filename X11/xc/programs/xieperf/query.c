/* $Xorg: query.c,v 1.4 2001/02/09 02:05:48 xorgcvs Exp $ */

/**** module query.c ****/
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
  
	query.c -- query flo element test 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/query.c,v 1.6 2001/12/14 20:01:51 dawes Exp $ */


#include "xieperf.h"
#include <stdio.h>

static XiePhotomap XIEPhotomap;
static XieLut   PhotofloTestLut1, PhotofloTestLut2;
static XieColorList XIEColorList;

static int flo_elements;
static XiePhotoElement *flograph;
static XiePhotoflo flo;

int 
InitQueryTechniques(XParms xp, Parms p, int reps)
{
	return reps;
}

int 
InitQueryColorList(XParms xp, Parms p, int reps)
{
	/* allocate a color list */

	if ( !(XIEColorList = XieCreateColorList( xp->d ) ) )
	{
		fprintf( stderr, "XieCreateColorList failed\n" );
		reps = 0;
	}
	return reps;
}

int 
InitQueryPhotomap(XParms xp, Parms p, int reps)
{
	if ( ( XIEPhotomap = 
		GetXIEPhotomap( xp, p, 1 ) ) == ( XiePhotomap ) NULL )
	{
		reps = 0;
	}
	return reps;
}

int 
InitQueryPhotoflo(XParms xp, Parms p, int reps)
{
        int     lutSize, lutLevels;
        unsigned char   *lut1, *lut2;
        Bool    merge;
        XieLTriplet start;

	flograph = ( XiePhotoElement * ) NULL;
	flo = ( XiePhotoflo ) NULL;
	lut1 = lut2 = ( unsigned char * ) NULL;
	PhotofloTestLut1 = PhotofloTestLut2 = ( XieLut ) NULL;

	flo_elements = 2;
        flograph = XieAllocatePhotofloGraph(flo_elements);
	lutSize = ( ( QueryParms * ) p->ts )->lutSize;
	lutLevels = ( ( QueryParms * ) p->ts )->lutLevels;
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
                fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
		reps = 0;
        }
	else
	{
		lut1 = (unsigned char *)
			malloc( lutSize * sizeof( unsigned char ) );
		if ( lut1 == ( unsigned char * ) NULL )
			reps = 0;
		else
		{
			lut2 = (unsigned char *)
				malloc( lutSize * sizeof( unsigned char ) );
			if ( lut2 == ( unsigned char * ) NULL )
			{
				reps = 0;
			}
		}
        }
	
	if ( reps )
	{
		if ( ( PhotofloTestLut1 = GetXIELut( xp, p, lut1, lutSize,
			lutLevels ) ) == ( XieLut ) NULL )
		{
			reps = 0;
		}
		else if ( ( PhotofloTestLut2 = GetXIELut( xp, p, lut2, lutSize,
			lutLevels ) ) == ( XieLut ) NULL )
		{
			reps = 0;
		}
        }

	if ( reps )
	{

		XieFloImportLUT(&flograph[0], PhotofloTestLut1 );

		merge = False;
		start[ 0 ] = 0;
		start[ 1 ] = 0;
		start[ 2 ] = 0;

		XieFloExportLUT(&flograph[1],
			1,              /* source phototag number */
			PhotofloTestLut2,
			merge,
			start
		);
		flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
	}

	if ( !reps )
		FreeQueryPhotofloStuff( xp, p );

	if ( lut1 )
		free( lut1 );
	if ( lut2 )
		free( lut2 );

	return reps;
}

void 
DoQueryTechniques(XParms xp, Parms p, int reps)
{
	XieTechniqueGroup	techGroup;
	XieTechnique		*techVector;
	int			j, i, numTech;

	techGroup = ( ( QueryParms * ) p->ts )->techGroup;

	for ( i = 0; i < reps; i++ )
	{
		if ( !XieQueryTechniques( xp->d, techGroup, &numTech, 
				&techVector ) )
		{
			fprintf( stderr, "XieQueryTechniques: failed\n" );
			break;
		}

		/* this bites */

		for ( j = 0; j < numTech; j++ )
		{
			if ( techVector[ j ].name )
				XFree( techVector[ j ].name );
		}
		if ( techVector )
		{
			XFree( techVector );
			techVector = ( XieTechnique * ) NULL;
		}
	}
}

void 
DoQueryColorList(XParms xp, Parms p, int reps)
{
	int	i;
	Colormap cmap;
	unsigned int ncolors;
	unsigned long *colors;

	for ( i = 0; i < reps; i++ )
	{
		colors = ( unsigned long * ) NULL;
		if ( !XieQueryColorList( xp->d, XIEColorList, &cmap, 
			&ncolors, &colors ) )
		{
			fprintf( stderr, "XieQueryColorList failed\n" );
			break;
		}
		
		/* for alpha, the colorlist is empty. This will change
		   later */

		if ( cmap != ( Colormap ) 0 )
		{
			fprintf( stderr, "XieQueryColorList returned non-zero colormap\n" );
			break;
		}
		if ( ncolors != 0 )
		{
			fprintf( stderr, "XieQueryColorList returned non-zero ncolors\n" );
			break;
		}
		if ( colors != ( unsigned long * ) NULL )
		{
			fprintf( stderr, "XieQueryColorList returned non-NULL colorlist\n" );
			break;
		}
	}
}

void 
DoQueryPhotomap(XParms xp, Parms p, int reps)
{
	int	i;
	XieLTriplet	width;
	XieLTriplet	height;
	XieLTriplet	levels;
	XieDataType	data_type;
	XieDataClass	cclass;
	Bool 	pop, error;
	XieDecodeTechnique decode;
        XIEimage *image;

        image = p->finfo.image1;
        if ( !image )
                return;

	error = False;
	for ( i = 0; i < reps && error == False; i++ )
	{
		if ( !XieQueryPhotomap( xp->d, XIEPhotomap, &pop, &data_type,
			&cclass, &decode, width, height, levels ) )
		{
			fprintf( stderr, "XieQueryPhotomap failed\n" );
			fflush( stderr );
			error = True;
		}
		if ( levels[ 0 ] != image->levels[ 0 ] )
		{
			fprintf( stderr, "XieQueryPhotomap levels return invalid should be 0x%lx got 0x%lx\n", image->levels[ 0 ], levels[ 0 ] );
			fflush( stderr );
			error = True;
		}
		if ( width[ 0 ] != image->width[ 0 ] )
		{
			fprintf( stderr, "XieQueryPhotomap width return invalid should be 0x%x got 0x%lx\n", image->width[ 0 ], width[ 0 ] );
			fflush( stderr );
			error = True;
		}
		if ( height[ 0 ] != image->height[ 0 ] )
		{
			fprintf( stderr, "XieQueryPhotomap height return invalid should be 0x%x got 0x%lx\n", image->height[ 0 ], height[ 0 ] );
			fflush( stderr );
			error = True;
		}
	}
}

void 
DoQueryPhotoflo(XParms xp, Parms p, int reps)
{
	int	i;
	XiePhotofloState state;
	XiePhototag *expected, *avail;
	unsigned int nexpected, navail;

	for ( i = 0; i < reps; i++ )
	{
		if ( !XieQueryPhotoflo( xp->d, 0, flo, &state, &expected,
			&nexpected, &avail, &navail ) )
		{
			fprintf( stderr, "XieQueryPhotoflo failed\n" );
		}
		if ( expected )
		{
			XFree( expected );
		}
		if ( avail )
		{
			XFree( avail );
		}
	}
}

void
EndQueryTechniques(XParms xp, Parms p)
{
}

void
EndQueryColorList(XParms xp, Parms p)
{
	FreeQueryColorListStuff( xp, p );
}

void
FreeQueryColorListStuff(XParms xp, Parms p)
{
	if ( XIEColorList )
	{
		XieDestroyColorList( xp->d, XIEColorList );
		XIEColorList = ( XieColorList ) NULL;
	}
}

void
EndQueryPhotomap(XParms xp, Parms p)
{
	FreeQueryPhotomapStuff( xp, p );
}

void
FreeQueryPhotomapStuff(XParms xp, Parms p)
{
	if ( XIEPhotomap && IsPhotomapInCache( XIEPhotomap ) == False )
	{
		XieDestroyPhotomap( xp->d, XIEPhotomap );
		XIEPhotomap = ( XiePhotomap ) NULL;
	}
}

void
EndQueryPhotoflo(XParms xp, Parms p)
{
	FreeQueryPhotofloStuff( xp, p );
}

void
FreeQueryPhotofloStuff(XParms xp, Parms p)
{
	if ( PhotofloTestLut1 )
	{
		XieDestroyLUT( xp->d, PhotofloTestLut1 );
		PhotofloTestLut1 = ( XieLut ) NULL;
	}

	if ( PhotofloTestLut2 )
	{
		XieDestroyLUT( xp->d, PhotofloTestLut2 );
		PhotofloTestLut2 = ( XieLut ) NULL;
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


