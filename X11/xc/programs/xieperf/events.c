/* $Xorg: events.c,v 1.4 2001/02/09 02:05:47 xorgcvs Exp $ */
/**** module events.c ****/
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
  
	events.c -- event tests and XIE event routines 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/events.c,v 3.9 2001/12/14 20:01:49 dawes Exp $ */

#ifdef WIN32
#define _WILLWINSOCK_
#endif
#include <stdio.h>
#include <ctype.h>
#include <X11/Xpoll.h>
#include "xieperf.h"

static XieExtensionInfo *xieInfo=NULL;
static int timeout = 60;        /* in seconds */

static XiePhotomap XIEPhotomap;
static XiePhotomap XIEPhotomap2;
static XieLut XIELut;

static XiePhotoElement *flograph;
static XiePhotoflo flo;
static int flo_elements;
static char *data;

static int size;
extern Bool dontClear;
extern Bool showErrors;
extern Window drawableWindow;

static int XIEEventErrorHandler ( Display *d, XErrorEvent *ev );

/* general event stuff */

int
GetTimeout(void)
{
        return( timeout );
}

void
SetTimeout(int time)
{
	timeout = time;
}

void
InitEventInfo(Display *display, XieExtensionInfo *info)
{
        xieInfo = info;
}

void
GetExtensionInfo(XieExtensionInfo **info)
{
	*info = xieInfo;
}

Bool 
event_check(Display *display, XEvent *event, char *arg)
{
	int xie_event;

        if (xieInfo == NULL)
                return(False);

	if ( event == ( XEvent * ) NULL )
		return( False );

        xie_event = event->type - xieInfo->first_event;
        if (xie_event <= 0)
                return(False);
        if (xie_event > xieEvnNoPhotofloDone)
                return(False);
        return(True);
}

int
WaitForXIEEvent(XParms xp, int which, XiePhotoflo flo_id, 
		XiePhototag tag, Bool verbose )
{
	int	Xsocket;
	XEvent event;
	XieExportAvailableEvent *ExportAvailable = 
		(XieExportAvailableEvent *) &event;
	XiePhotofloDoneEvent *PhotofloDone = 
		(XiePhotofloDoneEvent *) &event;
	XieColorAllocEvent *ColorAlloc = 
		(XieColorAllocEvent *) &event;
	XieDecodeNotifyEvent *DecodeNotify = 
		(XieDecodeNotifyEvent *) &event;
	XieImportObscuredEvent *ImportObscured = 
		(XieImportObscuredEvent *) &event;
	int retval, xie_event;
	time_t endtime, curtime, delta;
	struct timeval tv;
	Bool done;
	fd_set rd;

	retval = 1;
	done = False;

	/* set up for the select */

	Xsocket = ConnectionNumber(xp->d);	
	endtime = time( ( time_t * ) NULL ) + timeout;
	while ( done == False )
	{
		/* see if there is anything for us in the event queue... */	
	
		if ( XCheckTypedEvent( xp->d, xieInfo->first_event + which, 
			&event ) == False )
		{
			curtime = time( ( time_t * ) NULL );
			delta = endtime - curtime;
			if ( delta <= 0 )
			{
				retval = 0;
				fprintf( stderr, "Timed out on receiving XIE event\n" );
				fflush( stderr );
				done = True;
				continue;
			}
			tv.tv_sec = delta;  
			tv.tv_usec = 0L;
			XFlush( xp->d );
			FD_ZERO(&rd);
			FD_SET(Xsocket, &rd);
			Select( Xsocket + 1, &rd, NULL, NULL, &tv );
			continue;
		}	
		xie_event = event.type - xieInfo->first_event;
		switch( xie_event )
		{
		case xieEvnNoExportAvailable:
			if ( ExportAvailable->flo_id == flo_id && 
				ExportAvailable->src == tag )
			{
				if ( verbose == True )
					fprintf( stderr, "ExportAvailable event received\n" );
				retval = ExportAvailable->data[ 0 ];
				done = True;
			}
			break;
		case xieEvnNoPhotofloDone:
			if ( PhotofloDone->flo_id == flo_id ) 
			{
				if ( verbose == True )
					fprintf( stderr, "PhotofloDone event received\n" );
				done = True;
			}
			break;
		case xieEvnNoColorAlloc:
			if ( ColorAlloc->flo_id == flo_id ) 
			{
				if ( verbose == True )
					fprintf( stderr, "ColorAlloc event received\n" );
				done = True;
			}
			break;
		case xieEvnNoDecodeNotify:
			if ( DecodeNotify->flo_id == flo_id ) 
			{
				if ( verbose == True )
					fprintf( stderr, "DecodeNotify event received\n" );
				done = True;
			}
			break;
		case xieEvnNoImportObscured:
			if ( ImportObscured->flo_id == flo_id ) 
			{
				if ( verbose == True )
					fprintf( stderr, "ImportObscured event received\n" );
				done = True;
			}
			break;
		}
	}
	return( retval );
}

/* event tests */

static int
XIEEventErrorHandler(Display *d, XErrorEvent *ev)
{
	if ( showErrors ==True )
	{
		fprintf( stderr, "X Event received: major %d minor %d\n", 
			ev->request_code, ev->minor_code );
		fflush( stderr );
	}
	return( 0 );
}

int
InitEvents(XParms xp, Parms p, int reps)
{
	int	which;

	which = ( ( EventParms * ) p->ts )->event;
	XSetErrorHandler( XIEEventErrorHandler );
	switch( which )
	{
        case xieEvnNoExportAvailable:
		reps = InitExportAvailableEvent( xp, p, reps );
		break;
        case xieEvnNoPhotofloDone:
		reps = InitPhotofloDoneEvent( xp, p, reps );
		break;
        case xieEvnNoColorAlloc:
		reps = InitColorAllocEvent( xp, p, reps );
		break;
        case xieEvnNoDecodeNotify:
		reps = InitDecodeNotifyEvent( xp, p, reps );
		break;
        case xieEvnNoImportObscured:
		reps = InitImportObscuredEvent( xp, p, reps );
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
InitPhotofloDoneEvent(XParms xp, Parms p, int reps)
{
        XIEPhotomap = ( XiePhotomap ) NULL;
        flo = ( XiePhotoflo ) NULL;
        flograph = ( XiePhotoElement * ) NULL;

        if ( ( XIEPhotomap = GetXIEDitheredPhotomap( xp, p, 1, 
		(1 << xp->vinfo.depth ) ) ) == ( XiePhotomap ) NULL )
        {
                reps = 0;
        }
        else
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
                        XieFloImportPhotomap(&flograph[0], XIEPhotomap,
                                False);

                        XieFloExportDrawable(&flograph[1],
                                1,              /* source phototag number */
                                xp->w,
                                xp->fggc,
                                0,
                                0
                        );

                        flo = XieCreatePhotoflo( xp->d, flograph, flo_elements )
;
                }
        }
        if ( !reps )
        {
                FreePhotofloDoneEventStuff( xp, p );
        }
        return( reps );
}

int
InitColorAllocEvent(XParms xp, Parms p, int reps)
{
	CvtToIndexParms cticfg;
	EventParms *evcfg;
	int	retval;

	/* the idea is to use the convert to index init and end
	   functions, but use our test function. These are located
	   in cvttoindex.c. What we use in this files are wrappers
	   to those routines. Probably should do this for other test 
	   code in this file too ( some of them are close or identical 
	   to already written code )...  */
 
	/* initialize a convert to index parameter struct */

	cticfg.dither = xieValDitherDefault;
	cticfg.useDefaultCmap = False;
	cticfg.flo_notify = True;
	cticfg.addCvtFromIndex = False;

	/* save the current one */

	evcfg = ( EventParms * )p->ts;

	/* set the new one */

	p->ts = ( XPointer ) &cticfg;

	/* call the convert to index test init function */

	retval = InitConvertToIndex( xp, p, reps );

	/* restore the original test configuration */

	p->ts = ( XPointer )evcfg;
	return( retval );
}

int
InitDecodeNotifyEvent(XParms xp, Parms p, int reps)
{
	XIEimage *image;
	XieDecodeUncompressedSingleParam *decode_params=NULL;
        XieEncodeTechnique encode_tech=xieValEncodeServerChoice;
        char *encode_params=NULL;
	XieLTriplet width, height, levels;

	XIEPhotomap = ( XiePhotomap ) NULL;
        XIEPhotomap2 = ( XiePhotomap ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;

	image = p->finfo.image1; 
	if ( !GetImageData( xp, p, 1  ) )
		reps = 0;
	else
	{
		if ( !image )
			reps = 0;
		else
		{
			size = image->fsize;
			width[ 0 ] = image->width[ 0 ];
			height[ 0 ] = image->height[ 0 ];
			levels[ 0 ] = image->levels[ 0 ];
		}
	}

	if ( reps )
	{	
		XIEPhotomap = XieCreatePhotomap(xp->d);
		XIEPhotomap2 = XieCreatePhotomap(xp->d);

		decode_params = XieTecDecodeUncompressedSingle(
			image->fill_order,
			image->pixel_order,
			image->pixel_stride[ 0 ],
			image->left_pad[ 0 ],
			image->scanline_pad[ 0 ]
		);

		flo_elements = 2;
		flograph = XieAllocatePhotofloGraph(flo_elements);
		if ( flograph == ( XiePhotoElement * ) NULL )
		{
			reps = 0;
		}
	}
	if ( reps )
	{
		XieFloImportClientPhoto(&flograph[0],
			image->bandclass,
			width, height, levels,
			True,
			image->decode, (char *)decode_params
		);

		XieFloExportPhotomap(&flograph[1],
			1,              /* source phototag number */
			XIEPhotomap2,
			encode_tech,
			encode_params
		);
		flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
	}
	if ( !reps )
		FreeDecodeNotifyEventStuff( xp, p );
	if ( decode_params )
		XFree( decode_params );
    	return reps;
}

int
InitImportObscuredEvent(XParms xp, Parms p, int reps)
{
        flo = ( XiePhotoflo ) NULL;
        flograph = ( XiePhotoElement * ) NULL;

        flo_elements = 2;
        flograph = XieAllocatePhotofloGraph(flo_elements);
        if ( flograph == ( XiePhotoElement * ) NULL )
        {
                fprintf( stderr, "XieAllocatePhotofloGraph failed\n" );
                reps = 0;
        }
        else
        {

                XieFloImportDrawable(&flograph[0],
                        xp->w,
                        0,
                        0,
                        WIDTH,
                        HEIGHT,
                        0,
                        True
                );

                XieFloExportDrawable(&flograph[1],
                        1,              /* source phototag number */
                        drawableWindow,
                        xp->fggc,
                        0,       /* x offset in window */
                        0        /* y offset in window */
                );

                XMoveWindow( xp->d, drawableWindow, 100, 100 );
		XMapRaised( xp->d, drawableWindow );

                XSync( xp->d, 0 );
                GetXIEWindow( xp, p, xp->w, 1 );
                dontClear = True;

                flo = XieCreatePhotoflo( xp->d, flograph, 2 );
        }

        if ( !reps )
        {
		XUnmapWindow( xp->d, drawableWindow );
                FreeImportObscuredEventStuff( xp, p );
        }

        return( reps );
}

int
InitExportAvailableEvent(XParms xp, Parms p, int reps)
{
	int	lutSize;
	char	*lut;
	XieLTriplet start, length;

        XIELut = ( XieLut ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;

        lutSize = 10;
        lut = (char *)malloc( lutSize );
        data = (char *)malloc( lutSize );
        if ( lut == ( char * ) NULL || data == ( char * ) NULL )
        {
                reps = 0;
        }
        else
        {
                if ( ( XIELut = GetXIELut( xp, p,
					   (unsigned char *)lut,
					   lutSize, lutSize ) ) ==
                        ( XieLut ) NULL )
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
               			length[ 0 ] = lutSize;
                		length[ 1 ] = 0;
                		length[ 2 ] = 0;

                		start[ 0 ] = 0;
                		start[ 1 ] = 0;
                		start[ 2 ] = 0;

                                XieFloImportLUT(&flograph[0], XIELut);
                                XieFloExportClientLUT(&flograph[1],
                                        1,       /* source phototag number */
                                        xieValMSFirst,
					xieValNewData,
					start,
					length
                                );
                                flo = XieCreatePhotoflo( xp->d, flograph, flo_elements );
                        }
                }
        }
	if ( lut )
		free( lut );
        if ( !reps )
                FreeExportAvailableEventStuff( xp, p );
        return( reps );
}

void
DoPhotofloDoneEvent(XParms xp, Parms p, int reps)
{
        int     i;

        for (i = 0; i != reps; i++) {
                XieExecutePhotoflo( xp->d, flo, True );
                WaitForXIEEvent( xp, ( ( EventParms * ) p->ts )->event, flo, 0, showErrors );
	}
}

void
DoDecodeNotifyEvent(XParms xp, Parms p, int reps)
{
        int     i;

        for (i = 0; i != reps; i++) {
                XieExecutePhotoflo( xp->d, flo, True );

		/* send only part of the data - should cause the event */

       		PumpTheClientData( xp, p, flo, 0, 1, p->finfo.image1->data, 
			size / 2, 0 );
                WaitForXIEEvent( xp, ( ( EventParms * ) p->ts )->event, flo, 0, showErrors );
                WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo, 0, False );
	}
}

void
DoImportObscuredEvent(XParms xp, Parms p, int reps)
{
        int     i;

        for (i = 0; i != reps; i++) {
                XieExecutePhotoflo( xp->d, flo, True );
                WaitForXIEEvent( xp, ( ( EventParms * ) p->ts )->event, flo, 0, showErrors);
                WaitForXIEEvent( xp, xieEvnNoPhotofloDone, flo, 0, False );
	}
}

void
DoExportAvailableEvent(XParms xp, Parms p, int reps)
{
        int     i, done;

        for (i = 0; i != reps; i++) {
                XieExecutePhotoflo( xp->d, flo, True );
                WaitForXIEEvent( xp, ( ( EventParms * ) p->ts )->event, 
			flo, 2, showErrors );
		done = 0;
		while ( !done )
		{
			ReadNotifyExportData( xp, p, 0, flo, 2,
				sizeof( XieRectangle ), 1, &data, &done );
		}
                WaitForXIEEvent( xp, xieEvnNoPhotofloDone, 
			flo, 0, False );
	}
}

void
EndEvents(XParms xp, Parms p)
{
	int	which;

	XSetErrorHandler( None );
	which = ( ( EventParms * ) p->ts )->event;

	switch( which )
	{
        case xieEvnNoExportAvailable:
		FreeExportAvailableEventStuff( xp, p );
		break;
        case xieEvnNoPhotofloDone:
		FreePhotofloDoneEventStuff( xp, p );
		break;
        case xieEvnNoColorAlloc:
		EndConvertToIndex( xp, p );
		break;
        case xieEvnNoDecodeNotify:
		FreeDecodeNotifyEventStuff( xp, p );
		break;
        case xieEvnNoImportObscured:
		XUnmapWindow( xp->d, drawableWindow );
		FreeImportObscuredEventStuff( xp, p );
		break;
	}
}

void
FreePhotofloDoneEventStuff(XParms xp, Parms p)
{
        if ( data )
        {
                free( data );
                data = ( char * ) NULL;
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
FreeImportObscuredEventStuff(XParms xp, Parms p)
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
}

void
FreeExportAvailableEventStuff(XParms xp, Parms p)
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

	if ( XIELut )
	{
		XieDestroyLUT( xp->d, XIELut );
	        XIELut = ( XieLut ) NULL;
	}
}

void
FreeDecodeNotifyEventStuff(XParms xp, Parms p)
{
        if ( data )
        {
                free( data );
                data = ( char * ) NULL;
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

