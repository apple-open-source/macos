/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

static char RCSid[] = "$Id: example.c,v 1.1.1.1 2002/01/31 07:09:04 zarzycki Exp $" ;

#include "sio.h"

main( argc, argv )
   int argc ;
   char *argv[] ;
{
   char *file = (argc > 1) ? argv[ 1 ] : "tee.file" ;
   int fd = creat( file, 0644 ) ;
   long length ;
   char *s ;

   while ( s = Sfetch( 0, &length ) )
   {
      Swrite( 1, s, length ) ;
      Swrite( fd, s, length ) ;
   }
   exit( 0 ) ;
}

