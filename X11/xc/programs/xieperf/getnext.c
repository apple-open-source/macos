/* $Xorg: getnext.c,v 1.4 2001/02/09 02:05:47 xorgcvs Exp $ */

/**** module getnext.c ****/
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
  
	getnext.c -- Get a line from stream, and parse for command to run 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/getnext.c,v 1.5 2001/12/14 20:01:50 dawes Exp $ */

#include	<stdio.h>
#include	<ctype.h>
#include	<X11/Xos.h>
#include	"xieperf.h"

#define MAXARGS	5

int
GetNextTest(FILE *fp, int *repeat, int *reps)
{
	int	i, j, len, max, start;
	char	line[ 256 ];
	char	*args[ MAXARGS ];

otravez:				/* Spanish for 'once again'. */
	*repeat = -1;
	*reps = -1;
	line[ 0 ] = '\0';
	fprintf( stderr, "> " );
	fflush( stderr );
	fgets( line, sizeof( line ) - 1, fp );
	if ( feof( fp ) )
		return( -1 );
	len = strlen( line ) - 1;
	line[ len ] = '\0';
	for ( i = 0; i <= len; i++ )
	{
		if ( line[ i ] == '#' )
		{
			line[ i ] = '\0';
			break;
		}
	}

	if ( ( len = strlen( line ) ) == 0 )
		goto otravez;

	if ( fp != stdin )
		printf( "%s\n", line );

	if ( line[ 0 ] == '?' )
	{
		fprintf( stderr, "Enter command, with optional reps and repeat args\n" );
		fprintf( stderr, "\nExample: point1 -repeat 1 -reps 1\n" );
		fprintf( stderr, "Example: converttoindex -reps 3\n" );
		fprintf( stderr, "\nUse ^D to exit xieperf\n" );
		fflush( stderr );
		goto otravez;
	}

	/* grab up to MAXARGS */ 

	max = 0;
	for ( i = 0; i < MAXARGS; i++ )
		args[ i ] = ( char * ) NULL;

	args[ 0 ] = line;

	/* get rid of leading spaces */

	i = 0;
	while ( i < len && isspace( line[ i ] ) ) i++;
	start = i;
	args[ 0 ] = &line[ start ];
	if ( args[ 0 ] == ( char * ) NULL )
		goto otravez;
	else if ( strlen( args[ 0 ] ) == 0 )
		goto otravez;

	for ( i = 1, j = start; j < len && i < MAXARGS; i++ )
	{
		while ( j < len && !isspace( line[ j ] ) ) j++;
		if ( j != len ) 
		{
			line[ j ] = '\0'; j++;
			while ( j < len && isspace( line[ j ] ) ) j++;
			if ( j != len )
			{
				args[ i ] = &line[ j ];
				max = i;
				j++;
			}
		}
	}

	i = 1;
	while ( i <= max )
	{
		if ( !strcmp( args[ i ], "-repeat" ) )
		{
			i++;
			if ( i > max )
			{
				fprintf( stderr, "Improperly formed repeat argument: skipping line\n" );
				return( -1 );
			}
			else 	
			{
				*repeat = atoi( args[ i ] );
				i++;
			}
		}
		else if ( !strcmp( args[ i ], "-reps" ) )
		{
			i++;
			if ( i > max )
			{
				fprintf( stderr, "Improperly formed reps argument: skipping line\n" );
				return( -1 );
			}
			else 	
			{
				*reps = atoi( args[ i ] );
				i++;
			}
		}
		else
		{
			fprintf( stderr, "Unrecognized input: skipping line\n" );
			return( -1 );
		}
	}
	return( TestIndex( args[ 0 ] ) );
}

#ifdef STANDALONE_TEST
int
main(int argc, char *argv[])
{
	int	reps;
	int	repeat;
	int	retval;
	FILE	*fp;

	fp = stdin;
	while ( ( retval = GetNextTest( fp, &repeat, &reps ) ) >= 0 );
	exit(0);
}
#endif
