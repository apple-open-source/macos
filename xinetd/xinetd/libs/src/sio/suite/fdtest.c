/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include <sys/time.h>
#include <sys/resource.h>

#include "sio.h"

#ifndef NULL
#define NULL 0
#endif

int main( argc, argv )
	int argc ;
	char *argv[] ;
{
#ifdef RLIMIT_NOFILE
	struct rlimit rl ;
	int fd ;
	int duped_fd ;
	char *s ;

	if ( getrlimit( RLIMIT_NOFILE, &rl ) == -1 )
	{
		perror( "getrlimit" ) ;
		exit( 1 ) ;
	}
	if ( rl.rlim_cur != getdtablesize() )
	{
		printf( "rl.rlim_cur != getdtablesize()\n" ) ;
		exit( 1 ) ;
	}
	if ( rl.rlim_cur == rl.rlim_max )
		exit( 0 ) ;

	rl.rlim_cur++ ;
	if ( setrlimit( RLIMIT_NOFILE, &rl ) == -1 )
	{
		perror( "setrlimit" ) ;
		exit( 1 ) ;
	}
	if ( Smorefds() == SIO_ERR )
	{
		perror( "Smorefds" ) ;
		exit( 1 ) ;
	}
	fd = open( "/etc/passwd", 0 ) ;
	if ( fd == -1 )
	{
		perror( "open" ) ;
		exit( 1 ) ;
	}
	duped_fd = getdtablesize()-1 ;
	if ( dup2( fd, duped_fd ) == -1 )
	{
		perror( "dup2" ) ;
		exit( 1 ) ;
	}
	s = Srdline( duped_fd ) ;
	if ( s == NULL )
	{
		perror( "Srdline" ) ;
		exit( 1 ) ;
	}
#endif
	exit( 0 ) ;
}
