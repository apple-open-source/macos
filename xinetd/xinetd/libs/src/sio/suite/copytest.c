/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

static char RCSid[] = "$Id: copytest.c,v 1.1.1.1 2002/01/31 07:09:04 zarzycki Exp $" ;

#include "sio.h"
#include <stdio.h>
#include <syscall.h>


/*************************************************************/

#ifdef TEST_Sread

#define BUFFER_SIZE  4096

main()
{
	char buf[ BUFFER_SIZE ] ;
	int cc ;
	int nbytes ;

	for ( ;; )
	{
		nbytes = random() & ( BUFFER_SIZE - 1 ) ;
		if ( nbytes == 0 )
			nbytes = 1 ;
		cc = Sread( 0, buf, nbytes ) ;
		if ( cc == 0 )
			break ;
		if ( cc == SIO_ERR )
			exit( 1 ) ;
		write( 1, buf, cc ) ;
	}
	exit( 0 ) ;
}
#endif /* TEST_Sread */

/*************************************************************/

#ifdef TEST_Swrite

#define BUFFER_SIZE  4096

main()
{
	char buf[ BUFFER_SIZE ] ;
	int cc ;
	int nbytes ;

	for ( ;; )
	{
		nbytes = random() & ( BUFFER_SIZE - 1 ) ;
		if ( nbytes == 0 )
			nbytes = 1 ;
		cc = read( 0, buf, nbytes ) ;
		if ( cc == 0 )
			break ;
		if ( Swrite( 1, buf, cc ) != cc )
			exit( 1 ) ;
	}
	exit( 0 ) ;
}
#endif /* TEST_Swrite */

/*************************************************************/

#ifdef TEST_Srdline

main()
{
	char *s ;
	int count=0 ;

	while ( s = Srdline( 0 ) )
	{
		puts( s ) ;
		count++ ;
	}
	Sdone( 0 ) ;
	exit( 0 ) ;
}

#endif  /* TEST_Srdline */

/*************************************************************/

#ifdef TEST_Sputchar

main()
{
	int c ;

	while ( ( c = getchar() ) != EOF )
		if ( Sputchar( 1, c ) != c )
			exit( 1 ) ;
	exit( 0 ) ;
}

#endif /* TEST_Sputchar */

/*************************************************************/

#ifdef TEST_Sgetchar

main()
{
	int c ;

	while ( ( c = Sgetchar( 0 ) ) != SIO_EOF )
		putchar( c ) ;
	exit( 0 ) ;
}

#endif	/* TEST_Sgetchar */

/*************************************************************/

#ifdef TEST_Sputc

main()
{
   int c ;
 
   while ( ( c = getchar() ) != EOF )
      if ( Sputc( 1, c ) != c )
         exit( 1 ) ;
   exit( 0 ) ;
}

#endif /* TEST_Sputc */

/*************************************************************/

#ifdef TEST_Sgetc

main()
{
   int c ;

   while ( ( c = Sgetc( 0 ) ) != SIO_EOF )
      putchar( c ) ;
   exit( 0 ) ;
}

#endif /* TEST_Sgetc */

/*************************************************************/

#ifdef TEST_Sfetch

main()
{
	char *s ;
	int len ;

	while ( s = Sfetch( 0, &len ) )
		fwrite( s, 1, len, stdout ) ;
	exit( 0 ) ;
}

#endif /* TEST_Sfetch */

/*************************************************************/

#ifdef TEST_Sflush

#define MAX_COUNT		100

main()
{
	int c ;
	int errval ;
	int count = 0 ;
	int max_count = random() % MAX_COUNT + 1 ;

	while ( ( c = getchar() ) != EOF )
		if ( Sputchar( 1, c ) != c )
			exit( errval ) ;
		else
		{
			count++ ;
			if ( count >= max_count )
			{
				errval = Sflush( 1 ) ;
				if ( errval != 0 )
					exit( 1 ) ;
				max_count = random() % MAX_COUNT + 1 ;
				count = 0 ;
			}
		}
	exit( 0 ) ;
}

#endif /* TEST_Sflush */

/*************************************************************/

#ifdef TEST_Sundo

main()
{
	int c ;
	char *s ;
	int errval ;

	for ( ;; )
	{
		if ( random() % 1 )
		{
			s = Srdline( 0 ) ;
			if ( s == NULL )
				break ;
			if ( random() % 16 < 5 )
			{
				errval = Sundo( 0, SIO_UNDO_LINE ) ;
				if ( errval == SIO_ERR )
					exit( 1 ) ;
			}
			else
				puts( s ) ;
		}
		else
		{
			c = Sgetchar( 0 ) ;
			if ( c == SIO_EOF )
				break ;
			if ( random() % 16 < 5 )
			{
				errval = Sundo( 0, SIO_UNDO_CHAR ) ;
				if ( errval == SIO_ERR )
					exit( 2 ) ;
			}
			else
				putchar( c ) ;
		}
	}
	exit( 0 ) ;
}

#endif /* TEST_Sundo */


#if defined( TEST_switch ) || defined( TEST_switch2 )

main()
{
	int c ;
	char *s ;
	int lines = 4000 ;

	for ( ;; )
	{
		c = Sgetchar( 0 ) ;
		if ( c == SIO_EOF )
			exit( 0 ) ;
		if ( c == SIO_ERR )
			exit( 1 ) ;
		putchar( c ) ;
		if ( c == '\n' )
		{
			lines-- ;
			if ( lines == 0 )
				break ;
		}
	}
	while ( s = Srdline( 0 ) )
		puts( s ) ;
	exit( 0 ) ;
}

#ifdef TEST_switch2

char *mmap( addr, len, prot, type, fd, off )
	char *addr ;
	int len, prot, type, fd, off ;
{
	return( (char *)-1 ) ;
}

#endif	/* TEST_switch2 */

#endif 	/* TEST_switch */



