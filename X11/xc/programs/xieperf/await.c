/* $Xorg: await.c,v 1.4 2001/02/09 02:05:46 xorgcvs Exp $ */

/**** module await.c ****/
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
  
	await.c -- await flo element test 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/await.c,v 3.6 2001/12/14 20:01:46 dawes Exp $ */

#ifdef WIN32
#include <X11/Xthreads.h>
#endif
#include "xieperf.h"
#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>

static XieLut XIELut;
static XiePhotoElement *flograph;
static XiePhotoflo flo;
static unsigned char *lut;
static int lutSize;
static int AwaitHandlerSeen;
static XParms xplocal;		/* we can't pass args to signal handler */


int 
InitAwait(XParms xp, Parms p, int reps)
{
        XieDataClass    class;
        XieOrientation  band_order;
        XieLTriplet     length, levels;
        Bool    	merge;
        XieLTriplet     start;
        int	i;

	lut = ( unsigned char * ) NULL;
        flograph = ( XiePhotoElement * ) NULL;
        flo = ( XiePhotoflo ) NULL;
	XIELut = ( XieLut ) NULL;

	xplocal = xp;		/* so the signal handler can access it */

        lutSize = ( ( AwaitParms * ) ( p->ts ) )->lutSize;
        lut = (unsigned char *)malloc( lutSize );
        if ( lut == ( unsigned char * ) NULL )
	{
		fprintf( stderr, "malloc failed\n" );
		reps = 0;
	}
        else
        {
                for ( i = 0; i < lutSize; i++ )
                {
                        if ( i % 5 == 0 )
                        {
                                lut[ i ] = ( lutSize - 1 ) - i;
                        }
                        else
                        {
                                lut[ i ] = i;
                        }
                }
		if ( !(XIELut = XieCreateLUT( xp->d ) ) )
		{
			fprintf( stderr, "XieCreateLUT failed\n" );
			reps = 0;
		}
        }

	if ( reps )
	{
		/* set up a flo to read the lut from client */

		flograph = XieAllocatePhotofloGraph(2);
		if ( flograph == ( XiePhotoElement * ) NULL )
		{
			fprintf(stderr,"XieAllocatePhotofloGraph failed\n");
			return( 0 );
		}

		class = xieValSingleBand;
		band_order = xieValLSFirst;
		length[ 0 ] = lutSize;
		length[ 1 ] = 0;
		length[ 2 ] = 0;
		levels[ 0 ] = ( ( AwaitParms * ) ( p->ts ) )->lutLevels;
		levels[ 1 ] = 0;
		levels[ 2 ] = 0;

		XieFloImportClientLUT(&flograph[0],
			class,
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

		flo = XieCreatePhotoflo( xp->d, flograph, 2 );
	}
	if ( !reps )
	{
		FreeAwaitStuff( xp, p );
	}
	return( reps );
}

void
AbortFlo(XParms xp)
{
	XieExtensionInfo *xieInfo;

	xp->d = Open_Display( xp->displayName );
	if ( XieInitialize( xp->d, &xieInfo ) ) 
	{
		/* abort the flo */

		XieAbort( xp->d, 0, flo );
	}
	XCloseDisplay( xp->d );	
}

#ifdef SIGALRM
SIGNAL_T
AwaitHandler(int sig)
{
	int	pid;

	/* 
	   hmmmm... bad news. some possibilities include ( first time in this
	   function e.g. AwaitHandlerSeen is 0 on entry ): 

	   1 --> Signals are messed up and SIGALRM was triggered by mistake,
		 although this is an unlikely possibility.
	   2 --> It took more time than we expected to do the tasks in DoAwait()		 so increase the alarm() value or start pleading with your boss 
		 for a faster system or network
	   3 --> The flo didn't finish for some reason - maybe the child had
		 problems executing 
	   4 --> XieAwait is broken: possibly it did not notice that the flo 
                 had finished and thus XieQueryPhotoflo never was executed, or 
                 it *did* notice that the flo had finished but *still* did not 
	         allow XieQueryPhotoflo to get out.
	*/

	/* We have a big problem now: we could try waiting some more ( nah ),
           or we could just take the easy way out and exit, or we could 
	   spawn a child which makes a new connection to the server and
           aborts the photoflo. Let's try spawning and see if we can abort 
	   the flo. Also, we will restart this alarm and if we timeout 
 	   again we have no choice but to exit as things seem severly hosed.
	   First, however, let's check the value of AwaitHandlerSeen. If it
           happens to be 2 then we have already have timed out a second time
 	   and it is time to exit. */ 

	if ( ++AwaitHandlerSeen == 2 )
	{
		_exit( 1 );
	}
	if ( ( pid = fork() ) == -1 )	
	{
		_exit( 1 );
	}
	else if ( pid == 0 )			/* child */
	{
	        AbortFlo(xplocal); /* XXX is this safe ? */
		_exit( 0 );
	}
	else					/* parent */
	{
		/* if this alarm triggers, the XieAbort failed,
		   or it worked and we have a situation similar
                   to one of those listed above */

		alarm( GetTimeout() );
	}	
}
#endif

#ifdef WIN32
struct _data {XParms xp; Parms p;};

void 
ChildProc(struct _data *data)
{
	XieExtensionInfo *xieInfo;
	struct _XParms _xp;
	XParms  xp = &_xp;
	Parms   p = data->p;
	*xp = *data->xp;
	xp->d = Open_Display( xp->displayName );
	if ( XieInitialize( xp->d, &xieInfo ) )
	{
		PumpTheClientData( xp, p, flo, 0, 1,
			(char *)lut, lutSize, 0 );
	}
	XCloseDisplay(xp->d);
}
#endif

void 
DoAwait(XParms xp, Parms p, int reps)
{
	int	i, pid;
	int	status;
	XieExtensionInfo *xieInfo;
        XiePhotofloState state;
        XiePhototag *expected, *avail;
        unsigned int nexpected, navail;
#ifdef WIN32
	struct _data data;
	HANDLE child;
	DWORD threadId;
#endif

	/* this is fun! */

    	for (i = 0; i != reps; i++) {
		AwaitHandlerSeen = 0;
		XieExecutePhotoflo( xp->d, flo, 0 );
		XSync( xp->d, 0 );
		XieAwait( xp->d, 0, flo );
#ifdef WIN32
		data.xp = xp;
		data.p = p;
		child = CreateThread(NULL, 0,
				     (LPTHREAD_START_ROUTINE)ChildProc,
				     (LPVOID)&data, 0, &threadId);
		if (WaitForSingleObject(child, GetTimeout() * 1000) ==
		    WAIT_TIMEOUT)
		{
			HANDLE child2;
			fprintf(stderr, "transfer timed out\n");
			child2 = CreateThread(NULL, 0,
					      (LPTHREAD_START_ROUTINE)AbortFlo,
					      (LPVOID)xplocal, 0, &threadId);
			if (WaitForSingleObject(child2, GetTimeout() * 1000) ==
			    WAIT_TIMEOUT)
			{
				fprintf(stderr, "abort timed out\n");
				TerminateThread(child2, 0);
			}
			TerminateThread(child, 0);
		}
#else
		if ( ( pid = fork() ) == -1 )	/* gasp */
		{
			fprintf( stderr, "couldn't fork\n" );
			exit( 1 );
		}
		else if ( pid == 0 )		/* child */
		{
			/* connect to the server */

			xp->d = Open_Display( xp->displayName );
			if ( XieInitialize( xp->d, &xieInfo ) )
			{
				PumpTheClientData( xp, p, flo, 0, 1, 
					(char *)lut, lutSize, 0 ); 
			}
			XCloseDisplay(xp->d);
			exit( 0 );
		}
		else				/* parent */
		{
#ifdef SIGALRM
			signal( SIGALRM, AwaitHandler );
			alarm( GetTimeout() );		
#endif
#endif /* WIN32 */
			/* if we query the photoflo and it is active, then
			   XieAwait didn't really do what we wanted it to */

			if ( !XieQueryPhotoflo( xp->d, 0, flo, &state, 
				&expected, &nexpected, &avail, &navail ) )
			{
				fprintf( stderr, "XieQueryPhotoflo failed\n" );
				break;
			}				
			if ( expected )
				XFree( expected );
			if ( avail )
				XFree( avail );
			if ( state != xieValInactive )
			{
				fprintf( stderr, 
					"Flo state was not inactive\n" );
				break;
			}
#ifndef WIN32
			wait( &status );
			if ( status & 0xff )
			{
				fprintf( stderr, "Child process abnormal termination: %x\n", status & 0xff );	
				break;
			}
		}

		/* whew! somehow we got through this mess unscathed */
#ifdef SIGALRM
		alarm( 0 );	
#endif
#endif
    	}
}

void 
EndAwait(XParms xp, Parms p)
{
	FreeAwaitStuff( xp, p );
}

void
FreeAwaitStuff(XParms xp, Parms p)
{
	if ( lut )
	{
		free( lut );
		lut = ( unsigned char * ) NULL;
	}

        if ( flograph )
        {
                XieFreePhotofloGraph(flograph,2);
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
