/* $Xorg: constrain.c,v 1.4 2001/02/09 02:05:47 xorgcvs Exp $ */

/**** module constrain.c ****/
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
  
	constrain.c -- constrain flo element tests 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/constrain.c,v 1.6 2001/12/14 20:01:47 dawes Exp $ */

#include "xieperf.h"
#include <stdio.h>

static XieLut XIELut;
static XiePhotomap XIEPhotomap;
static XiePhotomap XIEPhotomap2;
static int flo_notify;
static XiePhotoElement *flograph;
static XiePhotoflo flo;
static int flo_elements;
static XieClipScaleParam *parms;
static int monoflag;

int 
InitConstrain(XParms xp, Parms p, int reps)
{
	XieLTriplet levels;
	XieConstrainTechnique tech;
	char *tech_parms=NULL;
        XieConstant in_low,in_high;
        XieLTriplet out_low,out_high;
	int InClampDelta = 0;
	int OutClampDelta = 0;
	int decode_notify = 0;
	XIEimage *image;
	Bool photoDest;
        XieEncodeTechnique encode_tech=xieValEncodeServerChoice;
        char *encode_params=NULL;
        XieProcessDomain domain;
	int	cclass;

#if     defined(__cplusplus) || defined(c_plusplus)
        cclass = xp->vinfo.c_class;
#else
        cclass = xp->vinfo.class;
#endif

	monoflag = 0;
	image = p->finfo.image1;
        XIELut = ( XieLut ) NULL;
        XIEPhotomap = ( XiePhotomap ) NULL;
        XIEPhotomap2 = ( XiePhotomap ) NULL;
        parms = ( XieClipScaleParam * ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;

	tech = ( ( ConstrainParms * ) p->ts )->constrain;
        photoDest = ( ( ConvolveParms * )p->ts )->photoDest;

	XIEPhotomap2 = XieCreatePhotomap( xp->d );

        if ( TechniqueSupported( xp, xieValConstrain, tech ) == False )
                reps = 0;
	else 
	{
		if ( ( XIEPhotomap = GetXIEPhotomap( xp, p, 1 ) ) == 
        		( XiePhotomap ) NULL )
        		reps = 0;
	}
	if ( reps )
	{
		flo_elements = 3;

		levels[ 0 ] = xp->vinfo.colormap_size;
		levels[ 1 ] = 0;
		levels[ 2 ] = 0;
		out_high[ 0 ] = xp->vinfo.colormap_size - 1;
		out_high[ 1 ] = 0; 
		out_high[ 2 ] = 0; 

		if ( levels[ 0 ] == 256 && 
			( ( ConstrainParms * ) p->ts )->clamp & ClampInputs )
		{
			InClampDelta = levels[ 0 ] / 2;
		}
		else
		{
			InClampDelta = 0;
		}
		if ( levels[ 0 ] == 256 && 
			( ( ConstrainParms * ) p->ts )->clamp & ClampOutputs )
		{
			OutClampDelta = out_high[ 0 ] / 2;
		}	
		else
		{
			OutClampDelta = 0;
		}
		in_low[ 0 ] = 0.0 + ( float ) InClampDelta; 
		in_low[ 1 ] = 0.0; 
		in_low[ 2 ] = 0.0; 
		in_high[ 0 ] = ( ( float ) image->levels[ 0 ] - 1.0 ) -
			( float ) InClampDelta;
		in_high[ 1 ] = 0.0; 
		in_high[ 2 ] = 0.0; 
		out_low[ 0 ] = 0 + OutClampDelta;
		out_low[ 1 ] = 0;
		out_low[ 2 ] = 0;
		out_high[ 0 ] = out_high[ 0 ] - OutClampDelta;
		out_high[ 1 ] = 0; 
		out_high[ 2 ] = 0; 
	}

	if ( reps && IsTrueColorOrDirectColor( cclass ) == True )
	{
                monoflag = 1;
                flo_elements+=2;
                if ( ( XIELut = CreatePointLut( xp, p, levels[ 0 ], 
			1 << xp->screenDepth, False ) ) == ( XieLut ) NULL )
                {
                        reps = 0;
                }
        }

	if ( reps )
	{
		flograph = XieAllocatePhotofloGraph( flo_elements );	
		if ( flograph == ( XiePhotoElement * ) NULL )
		{
			fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
			reps = 0;
		}
	}

	if ( reps )
	{
		XieFloImportPhotomap(&flograph[0],XIEPhotomap,decode_notify);

		if ( photoDest == False )
		{
			XieFloExportDrawable(&flograph[flo_elements - 1],
				flo_elements - 1, /* source phototag number */
				xp->w,
				xp->fggc,
				0,       /* x offset in window */
				0        /* y offset in window */
			);
		}
		else
		{
                        XieFloExportPhotomap(&flograph[flo_elements - 1],
                                flo_elements - 1, /* source phototag number */
                                XIEPhotomap2,
                                encode_tech,
                                encode_params
                        );
		}

		if ( tech == xieValConstrainHardClip )
		{
			tech_parms = ( char * ) NULL;
		}
		else
		{
			parms = XieTecClipScale( in_low, in_high,
				out_low, out_high);
			tech_parms = ( char * ) parms;
			if ( tech_parms == ( char * ) NULL )
			{
				fprintf( stderr, 
					"Trouble loading clipscale technique parameters\n" );
				reps = 0;
			}
		}
	}
	if ( reps )
	{
		XieFloConstrain( &flograph[1], 
			1,
			levels,
			tech,
			tech_parms
		);
	}
	if ( reps && monoflag )
	{
		XieFloImportLUT(&flograph[flo_elements - 3], XIELut );

		domain.phototag = 0;
		domain.offset_x = 0;
		domain.offset_y = 0;
		XieFloPoint(&flograph[flo_elements - 2],
			flo_elements - 3,
			&domain,
			flo_elements - 2,
			0x7
		);
	}
	if ( reps )
	{
		flo_notify = False;	
		flo = XieCreatePhotoflo( xp->d, flograph, flo_elements ); 
	}
	else
		FreeConstrainStuff( xp, p );
	return( reps );
}

void 
DoConstrain(XParms xp, Parms p, int reps)
{
    	int     i;

    	for (i = 0; i != reps; i++) {
        	XieExecutePhotoflo(xp->d, flo, flo_notify );
    	}
}

void 
EndConstrain(XParms xp, Parms p)
{
	FreeConstrainStuff( xp, p );
}

void
FreeConstrainStuff(XParms xp, Parms p)
{
        if ( XIEPhotomap && IsPhotomapInCache( XIEPhotomap ) == False )
        {
                XieDestroyPhotomap( xp->d, XIEPhotomap );
                XIEPhotomap = ( XiePhotomap ) NULL;
        }

        if ( XIELut ) 
        {
                XieDestroyLUT( xp->d, XIELut );
                XIELut = ( XieLut ) NULL;
        }

        if ( XIEPhotomap2 ) 
        {
                XieDestroyPhotomap( xp->d, XIEPhotomap2 );
                XIEPhotomap2 = ( XiePhotomap ) NULL;
        }

        if ( parms )
        {
                XFree( parms );
                parms = ( XieClipScaleParam * ) NULL;
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
