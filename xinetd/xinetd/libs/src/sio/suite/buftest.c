/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

static char RCSid[] = "$Id: buftest.c,v 1.1.1.1 2002/01/31 07:09:04 zarzycki Exp $" ;

#include "sio.h"

main()
{
	int i ;
	int sleep_interval = 3 ;

	if ( Sbuftype( 1, SIO_LINEBUF ) == SIO_ERR )
	{
		Sprint( 2, "Sbuftype failed\n" ) ;
		exit( 1 ) ;
	}

	for ( i = 0 ; i < 10 ; i++ )
	{
		Sprint( 1, "Line %d\n", i ) ;
		if ( i == 5 )
		{
			Sprint( 1, "Now switching to full buffering\n" ) ;
			sleep_interval = 2 ;
			if ( Sbuftype( 1, SIO_FULLBUF ) == SIO_ERR )
			{
				Sprint( 2, "2nd Sbuftype failed\n" ) ;
				exit( 1 ) ;
			}
		}
		sleep( sleep_interval ) ;
	}
	exit( 0 ) ;
}

