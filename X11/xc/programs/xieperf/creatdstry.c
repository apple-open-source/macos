/* $Xorg: creatdstry.c,v 1.4 2001/02/09 02:05:47 xorgcvs Exp $ */

/**** module creatdstry.c ****/
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
  
	creatdstry.c -- create/destroy resource tests

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/creatdstry.c,v 1.6 2001/12/14 20:01:48 dawes Exp $ */

#include "xieperf.h"
#include <stdio.h>

static XieLut	PhotofloTestLut1, PhotofloTestLut2;
static XiePhotoElement *flograph;
static int flo_elements;

int 
InitCreateDestroyPhotoflo(XParms xp, Parms p, int reps)
{
	int	lutSize;
	unsigned char	*lut1, *lut2;
        Bool    merge;
        XieLTriplet start;

	lut1 = ( unsigned char * ) NULL;
	lut2 = ( unsigned char * ) NULL;
	PhotofloTestLut1 = PhotofloTestLut2 = ( XieLut ) NULL;
	flo_elements = 2;
        flograph = XieAllocatePhotofloGraph(flo_elements);

	lutSize = xp->vinfo.colormap_size;
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
	}
	if ( reps )
	{
		lut2 = (unsigned char *)
			malloc( lutSize * sizeof( unsigned char ) );
		if ( lut2 == ( unsigned char * ) NULL )
		{
			reps = 0;
		}
	}
	if ( reps )
	{
		if ( ( PhotofloTestLut1 = 
			GetXIELut( xp, p, lut1, lutSize, lutSize ) ) 
			== ( XieLut ) NULL )
		{
			reps = 0;
		}
        }
	if ( reps )
	{
		if ( ( PhotofloTestLut2 = 
			GetXIELut( xp, p, lut2, lutSize, lutSize ) ) 
			== ( XieLut ) NULL )
		{
			reps = 0;
		}
        }
	if ( lut1 )
	{
		free( lut1 );
	}
	if ( lut2 )
	{
		free( lut2 );
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
	}

	if ( !reps )
		FreeCreateDestroyPhotofloStuff( xp, p );
	return reps;

}

int 
InitCreateDestroy(XParms xp, Parms p, int reps)
{
	return( reps );
}

void 
DoCreateDestroyColorList(XParms xp, Parms p, int reps)
{
	int	i;
	XieColorList clist;

	for ( i = 0; i < reps; i++ )
	{
		if ( !( clist = XieCreateColorList( xp->d ) ) )
		{
			fprintf( stderr, "XieCreateColorList failed\n" );
			break;
		}
		else 
			XieDestroyColorList( xp->d, clist );
	}	
}

void 
DoCreateDestroyLUT(XParms xp, Parms p, int reps)
{
	XieLut	lut;
	int	i;

	for ( i = 0; i < reps; i++ )
	{
		if ( !(lut = XieCreateLUT( xp->d ) ) )
		{
			fprintf( stderr, "XieCreateLUT failed\n" );
			break;
		}
		else
			XieDestroyLUT( xp->d, lut );
	}
}

void 
DoCreateDestroyPhotomap(XParms xp, Parms p, int reps)
{
	XiePhotomap photomap;
	int	i;

	for ( i = 0; i < reps; i++ )
	{
		if ( !(photomap = XieCreatePhotomap( xp->d ) ) )
		{
			fprintf( stderr, "XieCreatePhotomap failed\n" );
			break;
		}
		else
			XieDestroyPhotomap( xp->d, photomap );
	}
}

void 
DoCreateDestroyROI(XParms xp, Parms p, int reps)
{
	XieRoi	roi;
	int	i;

	for ( i = 0; i < reps; i++ )
	{
		if ( !( roi = XieCreateROI( xp->d ) ) )
		{
			fprintf( stderr, "XieCreateROI failed\n" );
			break;
		}
		else 
			XieDestroyROI( xp->d, roi );
	}
}

void 
DoCreateDestroyPhotospace(XParms xp, Parms p, int reps)
{
	XiePhotospace photospace;
	int	i;

	for ( i = 0; i < reps; i++ )
	{
		if ( !( photospace = XieCreatePhotospace( xp->d ) ) )
		{
			fprintf( stderr, "XieCreatePhotospace failed\n" );
			break;
		}
		else
			XieDestroyPhotospace( xp->d, photospace );
	}
}

void 
DoCreateDestroyPhotoflo(XParms xp, Parms p, int reps)
{
	XiePhotoflo	flo;
	int	i;
	for ( i = 0; i < reps; i++ )
	{
        	if ( !( flo = XieCreatePhotoflo( xp->d, flograph, flo_elements ) ) )
		{
			fprintf( stderr, "XieCreatePhotoflo failed\n" );
			break;
		}
		else
			XieDestroyPhotoflo( xp->d, flo );
	}
}

void 
EndCreateDestroy(XParms xp, Parms p)
{
	return;
}

void 
EndCreateDestroyPhotoflo(XParms xp, Parms p)
{
	FreeCreateDestroyPhotofloStuff( xp, p );
}

void
FreeCreateDestroyPhotofloStuff(XParms xp, Parms p)
{
	if ( flograph )
	{
                XieFreePhotofloGraph(flograph,flo_elements);
                flograph = ( XiePhotoElement * ) NULL;
        }

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
}
