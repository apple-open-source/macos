/* $Xorg: errors.c,v 1.4 2001/02/09 02:05:47 xorgcvs Exp $ */
/**** module errors.c ****/
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
  
	errors.c -- XIE error tests 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/errors.c,v 1.6 2001/12/14 20:01:48 dawes Exp $ */

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "xieperf.h"

static XiePhotoElement *flograph;
static XiePhotoflo flo;
static int flo_elements;
static XiePhotospace photospace;

static XieRoi XIERoi;
static XieLut XIELut;
static XiePhotomap XIEPhotomap;
static XieColorList clist;

extern Bool showErrors;
static int errcnt;

static char *coreErrTab[] = { 
	"Success",
	"BadRequest",
	"BadValue",
	"BadWindow",
	"BadPixmap",
	"BadAtom",
	"BadCursor",
	"BadFont",
	"BadMatch",	
	"BadDrawable",
	"BadAccess",
	"BadAlloc",
	"BadColor",
	"BadGC",
	"BadIDChoice",
	"BadName",
	"BadLength",
	"BadImplementation",
};

#define MAX_CORE_ERROR 4

static char *errTab[] = { 
	"ColorList",
	"LUT",
	"Photoflo",
	"Photomap",
	"Photospace",
	"ROI",
};

#define MAX_ERROR 5

static char *floErrTab[] = {
	"Unassigned ( invalid error )",
	"FloAccess",
	"FloAlloc",        
	"FloColormap",    
	"FloColorList",  
	"FloDomain",    
	"FloDrawable",  
	"FloElement",   
	"FloGC", 
	"FloID", 
	"FloLength",
	"FloLUT", 
	"FloMatch", 
	"FloOperator",
	"FloPhotomap", 
	"FloROI", 
	"FloSource",
	"FloTechnique",
	"FloValue",
	"FloImplementation"
};

#define MAX_FLO_ERROR 19
 
static void
ErrorEventPrintError(Display *display, XErrorEvent *error, FILE *fp)
{
    XieExtensionInfo 	 *xieExtInfo;
    XieFloAccessError    *flo_error      = (XieFloAccessError *) error;
    int	idx;

    GetExtensionInfo( &xieExtInfo );
    if ( xieExtInfo == ( XieExtensionInfo * ) NULL )
    {
	fprintf( stderr, "Couldn't get XIE extension info struct\n" );
	return;
    }

    if (error->error_code == xieExtInfo->first_error + xieErrNoFlo)
    {
	/* flo error */

	fprintf (fp, "Flo error: ");
	if ( flo_error->flo_error_code > MAX_FLO_ERROR )
		fprintf( fp, "unknown.\n" );
	else
		fprintf( fp, "%s.\n", floErrTab[ flo_error->flo_error_code ] );
    }
    else if (error->error_code >= xieExtInfo->first_error)
    {
	/* other xie error */
 	idx = error->error_code - xieExtInfo->first_error;
	if ( idx < 0 || idx > MAX_ERROR )
		fprintf( fp, "Error code is out of range\n" );
	else
		fprintf( fp, "%s.\n", errTab[ idx ] ); 
    }
    else
    {
	/* core X error... hopefully */

	idx = error->error_code;
	if ( idx < 0 || idx > MAX_CORE_ERROR )
		fprintf( fp, "Error code is out of range\n" );
	else
		fprintf( fp, "%s.\n", coreErrTab[ idx ] ); 
    }
}

static int
XIEErrorHandler(Display *d, XErrorEvent *ev)
{
	if ( showErrors == True )
	{
		fprintf( stderr, "X Error received: major '%d' minor '%d': ", 
			ev->request_code, ev->minor_code );
		ErrorEventPrintError( d, ev, stderr );
		fflush( stderr );
	}
	errcnt++;
	return( 0 );
}

int
InitErrors(XParms xp, Parms p, int reps)
{
	int	which;

	which = ( ( ErrorParms * ) p->ts )->error;
	XSetErrorHandler( XIEErrorHandler );
	errcnt = 0;
	switch( which )
	{
	case xieErrNoColorList :
		break;
	case xieErrNoLUT :     
		break;
	case xieErrNoPhotoflo : 
		break;
	case xieErrNoPhotomap : 
		break;
	case xieErrNoROI :        
		break;
	case xieErrNoPhotospace :
		break;
	default:
		reps = 0;
		break;
	}
	if ( !reps )
		XSetErrorHandler( None );
	return( reps );
}

int
InitCoreErrors(XParms xp, Parms p, int reps)
{
	int	which;

	which = ( ( ErrorParms * ) p->ts )->error;
	XSetErrorHandler( XIEErrorHandler );
	errcnt = 0;
	switch( which )
	{
	case BadAccess:
		fprintf( stderr, "XIE currently does not generate BadAccess\n" );
		fflush( stderr );
		reps = 0;
		break;
	case BadAlloc:
		fprintf( stderr, "Sorry, cannot reliably generate BadAlloc\n" );
		fflush( stderr );
		reps = 0;
		break;
	case BadIDChoice:
		break;
	case BadValue:
		InitBadValue( xp, p, reps );
		break;
	default:
		reps = 0;
		break;
	}
	if ( !reps )
		XSetErrorHandler( None );
	return( reps );
}

int
InitFloAccessError(XParms xp, Parms p, int reps)
{
	Bool merge;
	XieLTriplet start;

        flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;
	XIELut = ( XieLut ) NULL;

        flo_elements = 2;
        flograph = XieAllocatePhotofloGraph(flo_elements);
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
                fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                reps = 0;
        }
	else
	{
		XIELut = XieCreateLUT( xp->d );
                XieFloImportLUT(&flograph[0], XIELut );

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
	}
	if ( !reps )
	{
		FreeErrorWithLUTStuff( xp, p );
	}	
	return( reps );
}

int
InitBadValue(XParms xp, Parms p, int reps)
{
	return( reps );
}

int
InitFloErrors(XParms xp, Parms p, int reps)
{
	int	which;
	int	retval;

	which = ( ( ErrorParms * ) p->ts )->error;
	errcnt = 0;
        photospace = ( XiePhotospace ) NULL;
	retval = 0;
	switch( which )
	{
	case xieErrNoFloAccess :
		retval = InitFloAccessError( xp, p, reps );
		break;
	case xieErrNoFloROI :     
		retval = InitFloROIError( xp, p, reps );
		break;
	case xieErrNoFloDrawable :
		retval = InitFloDrawableError( xp, p, reps );
		break;
	case xieErrNoFloGC :  
		retval = InitFloGCError( xp, p, reps );
		break;
	case xieErrNoFloElement :  
		retval = InitFloElementError( xp, p, reps );
		break;
	case xieErrNoFloColorList :
		retval = InitFloColorListError( xp, p, reps );
		break;
	case xieErrNoFloID : 
		retval = InitFloIDError( xp, p, reps );
		break;
	case xieErrNoFloLUT :  
		retval = InitFloLUTError( xp, p, reps );
		break;
	case xieErrNoFloOperator :
		retval = InitFloOperatorError( xp, p, reps );
		break;
	case xieErrNoFloSource : 
		retval = InitFloSourceError( xp, p, reps );
		break;
	case xieErrNoFloPhotomap :
		retval = InitFloPhotomapError( xp, p, reps );
		break;
	case xieErrNoFloDomain :   
		retval = InitFloDomainError( xp, p, reps );
		break;
	case xieErrNoFloColormap :
		retval = InitFloColormapError( xp, p, reps );
		break;
	case xieErrNoFloMatch : 
		retval = InitFloMatchError( xp, p, reps );
		break;
	case xieErrNoFloTechnique :
		retval = InitFloTechniqueError( xp, p, reps );
		break;
	case xieErrNoFloValue : 
		retval = InitFloValueError( xp, p, reps );
		break;
	case xieErrNoFloLength : 
		fprintf( stderr, "Sorry - cannot reliably test for FloLength errors\n" );
		fflush( stderr );
		break;
	case xieErrNoFloAlloc :
		fprintf( stderr, "Sorry - cannot reliably test for FloAlloc errors\n" );
		fflush( stderr );
		break;
	case xieErrNoFloImplementation :
		fprintf( stderr, "Sorry - cannot reliably test for FloImplementation errors\n" );
		fflush( stderr );
		break;
	}
	if ( retval )
		XSetErrorHandler( XIEErrorHandler );
	return( retval );
}

int
InitPhotofloError(XParms xp, Parms p, int reps)
{
	flo = ( XiePhotoflo ) NULL;
	return( reps );
}

int
InitFloMatchError(XParms xp, Parms p, int reps)
{
	XieLTriplet levels;

        flo = ( XiePhotoflo ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;
        flo_elements = 3;
        flograph = XieAllocatePhotofloGraph(flo_elements);
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
        	fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                reps = 0;
        }
        else
        {
		XIEPhotomap = GetXIEPhotomap( xp, p, 1 );
		if ( XIEPhotomap == ( XiePhotomap ) NULL )
			reps = 0;
	}
	if ( reps )
	{
                XieFloImportPhotomap(&flograph[0], XIEPhotomap, False);

		levels[ 0 ] = 17;
		levels[ 1 ] = levels[ 2 ] = levels[ 0 ];

		XieFloDither( &flograph[ 1 ],
			1,
			0x07,
			levels,
			xieValDitherDefault,
			( char * ) NULL
		);

                XieFloExportDrawable(&flograph[2],
			2,
                	xp->w,              /* source phototag number */
                        xp->fggc,
                        0,       /* x offset in window */
                        0        /* y offset in window */
                );

                flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
	}
        if ( !reps )
        {
                FreeErrorWithPhotomapStuff( xp, p );
        }
	return( reps );
}

int
InitFloTechniqueError(XParms xp, Parms p, int reps)
{
	XieLTriplet levels;

        flo = ( XiePhotoflo ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;
        photospace = XieCreatePhotospace(xp->d);
        flo_elements = 3;
        flograph = XieAllocatePhotofloGraph(flo_elements);
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
        	fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                reps = 0;
        }
        else
        {
		XIEPhotomap = GetXIEPhotomap( xp, p, 1 );
		if ( XIEPhotomap == ( XiePhotomap ) NULL )
			reps = 0;
	}
	if ( reps )
	{
                XieFloImportPhotomap(&flograph[0], XIEPhotomap, False);

		levels[ 0 ] = ( 1 << xp->vinfo.depth ) >> 1;
		levels[ 1 ] = levels[ 2 ] = levels[ 0 ];

		XieFloDither( &flograph[ 1 ],
			1,
			0x07,
			levels,
			0xff,
			( char * ) NULL
		);

                XieFloExportDrawable(&flograph[2],
			2,
                	xp->w,              /* source phototag number */
                        xp->fggc,
                        0,       /* x offset in window */
                        0        /* y offset in window */
                );
	}
        if ( !reps )
        {
                FreeErrorWithPhotomapStuff( xp, p );
        }
	return( reps );
}

int
InitFloPhotomapError(XParms xp, Parms p, int reps)
{
        XieEncodeTechnique encode_tech=xieValEncodeServerChoice;
        char *encode_params=NULL;

        flo = ( XiePhotoflo ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;
        flo_elements = 2;
        flograph = XieAllocatePhotofloGraph(flo_elements);
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
        	fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                reps = 0;
        }
        else
        {
                XieFloImportPhotomap(&flograph[0], XIEPhotomap, False);

                XieFloExportPhotomap(&flograph[1],
                	1,              /* source phototag number */
                        XIEPhotomap,
                        encode_tech,
                        encode_params
                );
                flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
	}
        if ( !reps )
        {
                FreeFloErrorStuff( xp, p );
        }
	return( reps );
}

int
InitFloSourceError(XParms xp, Parms p, int reps)
{
        flo = ( XiePhotoflo ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;
        flo_elements = 2;
        flograph = XieAllocatePhotofloGraph(flo_elements);
	photospace = XieCreatePhotospace(xp->d);
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
        	fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                reps = 0;
        }
	else if ( ( XIEPhotomap = GetXIEPhotomap( xp, p, 1 ) ) ==
                ( XiePhotomap ) NULL )
        {
                reps = 0;
        }
        else
        {
                XieFloImportPhotomap(&flograph[0], XIEPhotomap, False);

                XieFloExportDrawable(&flograph[1],
                	~0,              /* source phototag number */
                        xp->w,
                        xp->fggc,
                        0,       /* x offset in window */
                        0        /* y offset in window */
                );
	}
        if ( !reps )
        {
                FreeErrorWithPhotomapStuff( xp, p );
        }
	return( reps );
}

int
InitFloElementError(XParms xp, Parms p, int reps)
{
        XieEncodeTechnique encode_tech=xieValEncodeServerChoice;
        char *encode_params=NULL;

        flo = ( XiePhotoflo ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
        flo_elements = 2;
        flograph = XieAllocatePhotofloGraph(flo_elements);
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
        	fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                reps = 0;
        }
	else if ( ( XIEPhotomap = GetXIEPhotomap( xp, p, 1 ) ) ==
                ( XiePhotomap ) NULL )
        {
                reps = 0;
        }
        else
        {
                XieFloImportPhotomap(&flograph[0], XIEPhotomap, False);
	        XieFloExportPhotomap(&flograph[1],
			1,              /* source phototag number */
			XIEPhotomap,
			encode_tech,
			encode_params
		);

                flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
	}
        if ( !reps )
        {
                FreeErrorWithPhotomapStuff( xp, p );
        }
	return( reps );
}

int
InitFloDrawableError(XParms xp, Parms p, int reps)
{
        flo = ( XiePhotoflo ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;
        flo_elements = 2;
        flograph = XieAllocatePhotofloGraph(flo_elements);
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
        	fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                reps = 0;
        }
	else if ( ( XIEPhotomap = GetXIEPhotomap( xp, p, 1 ) ) ==
                ( XiePhotomap ) NULL )
        {
                reps = 0;
        }
        else
        {
                XieFloImportPhotomap(&flograph[0], XIEPhotomap, False);
 
                XieFloExportDrawable(&flograph[1],
                	1,              /* source phototag number */
			( Window ) NULL,
			xp->fggc,
			0,
			0
		);

                flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
	}
        if ( !reps )
        {
                FreeErrorWithPhotomapStuff( xp, p );
        }
	return( reps );
}

int
InitFloColorListError(XParms xp, Parms p, int reps)
{
        XieLTriplet levels;
	int	idx, cclass;
        int 	cube;
        XieColorAllocAllParam *color_param = NULL;
        XWindowAttributes xwa;

#if     defined(__cplusplus) || defined(c_plusplus)
    	cclass = xp->vinfo.c_class;
#else
    	cclass = xp->vinfo.class;
#endif
        if ( !IsColorVisual( cclass ) )
                return( 0 );
        cube = icbrt( 1 << xp->vinfo.depth );
        levels[ 0 ] = cube;
        levels[ 1 ] = cube;
        levels[ 2 ] = cube;
        clist = ( XieColorList ) NULL;
        XIEPhotomap = ( XiePhotomap ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;

        if ( !( clist = XieCreateColorList( xp->d ) ) )
                reps = 0;

        if ( reps )
	{
		XIEPhotomap = GetXIEDitheredTriplePhotomap( xp, p, 1,
			xieValDitherDefault, 0, levels );
		if ( XIEPhotomap == ( XiePhotomap ) NULL )
			reps = 0;
	}
	if ( reps )
	{
		InstallDefaultColormap( xp );
		flo_elements = 3; 
	        flograph = XieAllocatePhotofloGraph( flo_elements );
        	if ( flograph == ( XiePhotoElement * ) NULL )
        	{
			fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
			reps = 0;
		}
	        color_param = XieTecColorAllocAll( 123 ); 
        	if ( color_param == ( XieColorAllocAllParam * ) NULL )
		{
			fprintf( stderr, "XieTecColorAllocAll failed\n" );
			reps = 0;
		}
	}
	if ( reps )
	{
		idx = 0;
		XieFloImportPhotomap(&flograph[idx],XIEPhotomap,False);
		idx++;

                XGetWindowAttributes( xp->d, DefaultRootWindow( xp->d ), &xwa );
                XSetWindowColormap( xp->d, xp->w, xwa.colormap );
                XSync( xp->d, 0 );

		XieFloConvertToIndex(&flograph[idx],
			idx,
			xwa.colormap,
			clist,
			False,
			xieValColorAllocAll,
			(char *)color_param
		);
		idx++;

		XieFloExportDrawable(&flograph[idx],
			idx,              /* source phototag number */
			xp->w,
			xp->fggc,
			0,       /* x offset in window */
			0        /* y offset in window */
		);

		flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
		XFree( color_param );
	} 

	/* this should cause a FloColorList error */

        if ( clist )
        {
                XieDestroyColorList( xp->d, clist );
                clist = ( XieColorList ) NULL;
        }

	if ( !reps )
	{
		InstallGrayColormap( xp );
		FreeErrorWithPhotomapStuff( xp, p );
	}
	return( reps );
}

int
InitFloColormapError(XParms xp, Parms p, int reps)
{
        XieLTriplet levels;
	int	idx, cclass;
        int 	cube;
        XieColorAllocAllParam *color_param = NULL;

#if     defined(__cplusplus) || defined(c_plusplus)
    	cclass = xp->vinfo.c_class;
#else
    	cclass = xp->vinfo.class;
#endif

        if ( !IsColorVisual( cclass ) )
                return( 0 );
        cube = icbrt( 1 << xp->vinfo.depth );
        levels[ 0 ] = cube;
        levels[ 1 ] = cube;
        levels[ 2 ] = cube;
        clist = ( XieColorList ) NULL;
        XIEPhotomap = ( XiePhotomap ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;

        if ( !( clist = XieCreateColorList( xp->d ) ) )
                reps = 0;

        if ( reps )
	{
		XIEPhotomap = GetXIEDitheredTriplePhotomap( xp, p, 1,
			xieValDitherDefault, 0, levels );
		if ( XIEPhotomap == ( XiePhotomap ) NULL )
			reps = 0;
	}
	if ( reps )
	{
		flo_elements = 3; 
	        flograph = XieAllocatePhotofloGraph( flo_elements );
        	if ( flograph == ( XiePhotoElement * ) NULL )
        	{
			fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
			reps = 0;
		}
	        color_param = XieTecColorAllocAll( 123 ); 
        	if ( color_param == ( XieColorAllocAllParam * ) NULL )
		{
			fprintf( stderr, "XieTecColorAllocAll failed\n" );
			reps = 0;
		}
	}
	if ( reps )
	{
		idx = 0;
		XieFloImportPhotomap(&flograph[idx],XIEPhotomap,False);
		idx++;

		XieFloConvertToIndex(&flograph[idx],
			idx,
			( Colormap ) 0xffffffff,
			clist,
			False,
			xieValColorAllocAll,
			(char *)color_param
		);
		idx++;

		XieFloExportDrawable(&flograph[idx],
			idx,              /* source phototag number */
			xp->w,
			xp->fggc,
			0,       /* x offset in window */
			0        /* y offset in window */
		);

		flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
		XFree( color_param );
	} 
	if ( !reps )
	{
		FreeErrorWithPhotomapStuff( xp, p );
	}
	return( reps );
}

int
InitFloGCError(XParms xp, Parms p, int reps)
{
	static GC myGC;

        flo = ( XiePhotoflo ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
	XIEPhotomap = ( XiePhotomap ) NULL;
        flo_elements = 2;
        flograph = XieAllocatePhotofloGraph(flo_elements);
	myGC = XCreateGC( xp->d, xp->w, 0, 0 );
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
        	fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                reps = 0;
        }
	else if ( ( XIEPhotomap = GetXIEPhotomap( xp, p, 1 ) ) ==
                ( XiePhotomap ) NULL )
        {
                reps = 0;
        }
        else
        {
                XieFloImportPhotomap(&flograph[0], XIEPhotomap, False);
 
                XieFloExportDrawable(&flograph[1],
                	1,              /* source phototag number */
			xp->w,
			myGC,	
			0,
			0
		);

                flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
	}
        if ( !reps )
        {
                FreeErrorWithPhotomapStuff( xp, p );
        }
	XFreeGC( xp->d, myGC );
	return( reps );
}

int
InitFloOperatorError(XParms xp, Parms p, int reps)
{
	int	idx;
	XIEimage *image;
	XieProcessDomain domain;

        XIEPhotomap = ( XiePhotomap ) NULL;
        flo = ( XiePhotoflo ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
        image = p->finfo.image1;
	flo_elements = 3;
        flograph = XieAllocatePhotofloGraph( flo_elements );
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
                fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                reps = 0;
        }
        else if ( ( XIEPhotomap = GetXIEPhotomap( xp, p, 1 ) ) == 
		( XiePhotomap ) NULL )
        {
                reps = 0;
        }
	else
	{
		idx = 0;
                XieFloImportPhotomap(&flograph[idx], XIEPhotomap, False );
                idx++;

		domain.offset_x = 0;
                domain.offset_y = 0;
                domain.phototag = 0;

                XieFloMath(&flograph[idx],
               		idx,
                        &domain,
                        ~0,
                       	0x7 
                );
                idx++;
                XieFloExportDrawable(&flograph[idx],
                        idx,            /* source phototag number */
                        xp->w,
                        xp->fggc,
                        0,       	/* x offset in window */
                        0        	/* y offset in window */
                );
                idx++;

                flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
        }
        if ( !reps )
                FreeErrorWithPhotomapStuff( xp, p );
	return( reps );
}

int
InitFloDomainError(XParms xp, Parms p, int reps)
{
	int	idx;
	XIEimage *image;
	XieProcessDomain domain;

        XIEPhotomap = ( XiePhotomap ) NULL;
        flo = ( XiePhotoflo ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
        image = p->finfo.image1;
	flo_elements = 3;
        flograph = XieAllocatePhotofloGraph( flo_elements );
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
                fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                reps = 0;
        }
        else if ( ( XIEPhotomap = GetXIEPhotomap( xp, p, 1 ) ) == 
		( XiePhotomap ) NULL )
        {
                fprintf( stderr, "GetXIEPhotomap failed\n" );
                reps = 0;
        }
	else
	{
		idx = 0;
                XieFloImportPhotomap(&flograph[idx], XIEPhotomap, False );
                idx++;

		domain.offset_x = 0;
                domain.offset_y = 0;
                domain.phototag = 1;

                XieFloMath(&flograph[idx],
               		idx,
                        &domain,
                        xieValAdd,
                       	0x7 
                );
                idx++;
                XieFloExportDrawable(&flograph[idx],
                        idx,            /* source phototag number */
                        xp->w,
                        xp->fggc,
                        0,       	/* x offset in window */
                        0        	/* y offset in window */
                );
                idx++;

                flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
        }
        if ( !reps )
                FreeErrorWithPhotomapStuff( xp, p );
	return( reps );
}

int
InitFloIDError(XParms xp, Parms p, int reps)
{
	flo = ( XiePhotoflo ) NULL;
	return( reps );
}

int
InitFloValueError(XParms xp, Parms p, int reps)
{
	unsigned char	*lut;
        XieLTriplet     start, length;

        flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;
	XIELut = ( XieLut ) NULL;
	lut = ( unsigned char * ) NULL;

        flo_elements = 2;
        flograph = XieAllocatePhotofloGraph(flo_elements);
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
                fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                reps = 0;
        }
	else
	{
		lut = ( unsigned char * ) malloc( 128 );

		length[ 0 ] = 128;
		length[ 1 ] = 0;
		length[ 2 ] = 0;

		start[ 0 ] = 0;
		start[ 1 ] = 0;
		start[ 2 ] = 0;

                if ( ( XIELut = GetXIELut( xp, p, lut, 128, 128 ) ) ==
                        ( XieLut ) NULL )
                {
                        reps = 0;
                }
		else
		{
			XieFloImportLUT(&flograph[0], XIELut);
			XieFloExportClientLUT(&flograph[1],
				1,       /* source phototag number */
				xieValMSFirst,
				0xff,
				start,
				length	
			);
			flo = XieCreatePhotoflo( xp->d, 
				flograph, flo_elements );
		}
	}
	if ( !reps )
	{
		FreeErrorWithLUTStuff( xp, p );
	}	
	if ( lut )
		free( lut );	
	return( reps );
}

int
InitFloROIError(XParms xp, Parms p, int reps)
{
        flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;
	XIERoi = ( XieRoi ) NULL;

        flo_elements = 2;
        flograph = XieAllocatePhotofloGraph(flo_elements);
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
                fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                reps = 0;
        }
	else
	{
                XieFloImportROI(&flograph[0], XIERoi );

                XieFloExportROI(&flograph[1],
                        1,              /* source phototag number */
                        XIERoi
                );

                flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
	}
	if ( !reps )
	{
		FreeFloErrorStuff( xp, p );
	}	
	return( reps );
}

int
InitFloLUTError(XParms xp, Parms p, int reps)
{
        Bool    merge;
        XieLTriplet start;

        flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;

        flo_elements = 2;
        flograph = XieAllocatePhotofloGraph(flo_elements);
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
                fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                reps = 0;
        }
      	else 
        {
                XieFloImportLUT(&flograph[0], ( XieLut ) NULL );

                merge = False;
                start[ 0 ] = 0;
                start[ 1 ] = 0;
                start[ 2 ] = 0;

                XieFloExportLUT(&flograph[1],
                        1,              /* source phototag number */
                        ( XieLut ) NULL,
                        merge,
                        start
                );

                flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
        }
        if ( !reps )
        {
                FreeFloErrorStuff( xp, p );
        }
        return( reps );
}

void
DoBadValueError(XParms xp, Parms p, int reps)
{
        XieTechniqueGroup       techGroup;
        XieTechnique            *techVector;
        int                     j, i, numTech;

        techGroup = 0xff; 

        for ( i = 0; i < reps; i++ )
        {
                if ( !XieQueryTechniques( xp->d, techGroup, &numTech,
                                &techVector ) )
                {
			continue;
                }

		/* we shouldn't get here, however, clean up if so... */

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
DoColorListError(XParms xp, Parms p, int reps)
{
	int	i;

        for (i = 0; i != reps; i++) {
		XieDestroyColorList( xp->d, ( XieColorList ) NULL );
	}
}

void
DoFloErrorImmediate(XParms xp, Parms p, int reps)
{
	int	i;
	int	flo_id;

	flo_id = 1;
        for (i = 0; i != reps; i++) {
                XieExecuteImmediate(xp->d, photospace,
                        flo_id,
                        False,
                        flograph,       /* photoflo specification */
                        flo_elements    /* number of elements */
                );
		flo_id++;
	}
}

void
DoPhotospaceError(XParms xp, Parms p, int reps)
{
	int	i;

        for (i = 0; i != reps; i++) {
		XieDestroyPhotospace( xp->d, ( XiePhotospace ) NULL );
	}
}

void
DoROIError(XParms xp, Parms p, int reps)
{
	int	i;

        for (i = 0; i != reps; i++) {
		XieDestroyROI( xp->d, ( XieRoi ) NULL );
	}
}

void	
DoFloElementError(XParms xp, Parms p, int reps)
{
	int	i;

        for (i = 0; i != reps; i++) {
                XieFloImportLUT(&flograph[1], ( XieLut ) NULL);
                XieModifyPhotoflo( xp->d, flo, 2, &flograph[1], 1 );
	}
}

void
DoLUTError(XParms xp, Parms p, int reps)
{
	int	i;

        for (i = 0; i != reps; i++) {
		XieDestroyLUT( xp->d, ( XieLut ) NULL );
	}
}

void
DoFloIDError(XParms xp, Parms p, int reps)
{
	int	i;
	char	data[ 16 ];

        for (i = 0; i != reps; i++) {
                XiePutClientData (
                        xp->d,
                        0,		/* photospace */
                        flo,		/* flo id */
                        1,              /* element */
                        True,           /* signal that this is all the data */
                        0,              /* band_number */
                        (unsigned char *)data,
			sizeof( data )
                );
	}
}

void
DoPhotomapError(XParms xp, Parms p, int reps)
{
	int	i;

        for (i = 0; i != reps; i++) {
		XieDestroyPhotomap( xp->d, ( XieLut ) NULL );
	}
}

void
DoErrors(XParms xp, Parms p, int reps)
{
        int     i;

        for (i = 0; i != reps; i++) {
                XieExecutePhotoflo( xp->d, flo, False );
	}
}

void
EndErrors(XParms xp, Parms p)
{
	int	which;

	XSetErrorHandler( None );
	which = ( ( ErrorParms * ) p->ts )->error;
	fprintf( stderr, "There were %d error events received\n", errcnt );
	fflush( stderr );
	switch( which )
	{
	case xieErrNoColorList :
		break;
	case xieErrNoLUT :     
		break;
	case xieErrNoPhotoflo : 
		break;
	case xieErrNoPhotomap : 
		break;
	case xieErrNoROI :        
		break;
	case xieErrNoPhotospace :
		break;
	}
}

void
EndCoreErrors(XParms xp, Parms p)
{
	int	which;

	XSetErrorHandler( None );
	which = ( ( ErrorParms * ) p->ts )->error;
	fprintf( stderr, "There were %d error events received\n", errcnt );
	fflush( stderr );
	switch( which )
	{
	case BadAccess:
		/* not implemented */
		break;
	case BadAlloc:
		/* not implemented */
		break;
	case BadIDChoice:
		/* not implemented */
		break;
	case BadValue:
		/* nothing to do for this test */
		break;
	default:
		break;
	}
}

void
EndFloErrors(XParms xp, Parms p)
{
	int	which;

	XSetErrorHandler( None );
	which = ( ( ErrorParms * ) p->ts )->error;
	fprintf( stderr, "There were %d error events received\n", errcnt );
	fflush( stderr );
	switch( which )
	{
	case xieErrNoFloAccess :
		FreeErrorWithLUTStuff( xp, p );
		break;
	case xieErrNoFloDrawable :
		FreeErrorWithPhotomapStuff( xp, p );
		break;
	case xieErrNoFloGC :  
		FreeErrorWithPhotomapStuff( xp, p );
		break;
	case xieErrNoFloElement :  
		FreeErrorWithPhotomapStuff( xp, p );
		break;
	case xieErrNoFloColorList :
		InstallGrayColormap( xp );
		FreeErrorWithPhotomapStuff( xp, p );
		break;
	case xieErrNoFloDomain :   
		FreeErrorWithPhotomapStuff( xp, p );
		break;
	case xieErrNoFloColormap :
		FreeErrorWithPhotomapStuff( xp, p );
		break;
	case xieErrNoFloID : 
		break;
	case xieErrNoFloLUT :  
		FreeFloErrorStuff( xp, p );
		break;
	case xieErrNoFloROI :     
		FreeFloErrorStuff( xp, p );
		break;
	case xieErrNoFloOperator :
		FreeErrorWithPhotomapStuff( xp, p );
		break;
	case xieErrNoFloSource : 
		FreeErrorWithPhotomapStuff( xp, p );
		break;
	case xieErrNoFloPhotomap :
		FreeFloErrorStuff( xp, p );
		break;
	case xieErrNoFloMatch : 
		FreeErrorWithPhotomapStuff( xp, p );
		break;
	case xieErrNoFloTechnique :
		FreeErrorWithPhotomapStuff( xp, p );
		break;
	case xieErrNoFloValue : 
		FreeErrorWithLUTStuff( xp, p );
		break;
	default:
		break;
	}
}

void
FreeErrorWithPhotomapStuff(XParms xp, Parms p)
{
        if ( photospace )
        {
                XieDestroyPhotospace( xp->d, photospace );
                photospace = ( XiePhotospace ) NULL;
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
FreeErrorWithROIStuff(XParms xp, Parms p)
{
        if ( photospace )
        {
                XieDestroyPhotospace( xp->d, photospace );
                photospace = ( XiePhotospace ) NULL;
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
}

void
FreeErrorWithLUTStuff(XParms xp, Parms p)
{
        if ( photospace )
        {
                XieDestroyPhotospace( xp->d, photospace );
                photospace = ( XiePhotospace ) NULL;
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
	if ( XIELut )
	{
		XieDestroyLUT( xp->d, XIELut );
		XIELut = ( XieLut ) NULL;
	}
}

void
FreeFloErrorStuff(XParms xp, Parms p)
{
        if ( photospace )
        {
                XieDestroyPhotospace( xp->d, photospace );
                photospace = ( XiePhotospace ) NULL;
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


