/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"
#include "sio.h"
#include "options.h"
#include "main.h"
#include "util.h"
#include "internals.h" /* for enable_periodic_check() */

/*
 * $Id: options.c,v 1.1.1.3 2002/10/02 21:07:27 bbraun Exp $
 */


int filelog_option ;
char * filelog_option_arg ;
int syslog_option ;
char * syslog_option_arg ;
int logprocs_option ;
unsigned logprocs_option_arg ;
int stayalive_option=0;
char *program_name ;
int inetd_compat = 0 ;

static void usage(void);

int opt_recognize( int argc, char *argv[] )
{
   int arg, arg_1 ;

   program_name = strrchr( argv[ 0 ], '/' ) ;
   program_name = ( program_name == NULL ) ? argv[ 0 ] : program_name + 1 ;

   for ( arg = 1 ; arg < argc ; arg++ )
      if ( argv[ arg ][ 0 ] == '-' && argv[ arg ][ 1 ] != 0 )
      {
         if ( strcmp( &argv[ arg ][ 1 ], "d" ) == 0 ) 
            debug.on = 1 ;
         else if ( strcmp( &argv[ arg ][ 1 ], "f" ) == 0 ) 
         {
            if ( ++arg == argc )
               usage() ;
            ps.ros.config_file = argv[ arg ];
         }
         else if ( strcmp( &argv[ arg ][ 1 ], "filelog" ) == 0 ) 
         {
            if ( ++arg == argc )
               usage() ;
            filelog_option_arg = ( argv[ arg ] ) ;
            filelog_option = 1 ;
         }
         else if ( strcmp( &argv[ arg ][ 1 ], "syslog" ) == 0 ) 
         {
            if ( ++arg == argc )
               usage() ;
            syslog_option_arg = ( argv[ arg ] ) ;
            syslog_option = 1 ;
         }
         else if ( strcmp( &argv[ arg ][ 1 ], "reuse" ) == 0 ) 
            ; /* This is now a null option, kept for compatibility */
         else if ( strcmp( &argv[ arg ][ 1 ], "limit" ) == 0 ) 
         {
            if ( ++arg == argc )
               usage() ;
            if ( parse_int( argv[ arg ], 10, NUL, &arg_1 ) || arg_1 < 0 )
               usage() ;
            ps.ros.process_limit = arg_1 ;
         }
         else if ( strcmp( &argv[ arg ][ 1 ], "pidfile" ) == 0 ){
                            if( ++arg ==argc )
                                usage () ;
                            ps.ros.pid_file = (char *)new_string( argv[arg] );
                        }
         else if ( strcmp( &argv[ arg ][ 1 ], "stayalive" )==0) {
            stayalive_option = 1;
                        }
         else if ( strcmp( &argv[ arg ][ 1 ], "logprocs" ) == 0 ) 
         {
            if ( ++arg == argc )
               usage() ;
            if ( parse_int( argv[ arg ], 10, NUL, &arg_1 ) || arg_1 < 0 )
               usage() ;
            logprocs_option_arg = arg_1 ;
            logprocs_option = 1 ;
         }
         else if ( strcmp( &argv[ arg ][ 1 ], "shutdownprocs" ) == 0 ) 
         {
            if ( ++arg == argc )
               usage() ;
            Sprint(2, "The shutdownprocs option has been deprecated.\n");
         }
         else if ( strcmp( &argv[ arg ][ 1 ], "cc" ) == 0 ) 
         {
            if ( ++arg == argc )
               usage() ;
            if ( parse_int( argv[ arg ], 10, NUL, &arg_1 ) || arg_1 < 0 )
               usage() ;
            ps.ros.cc_interval = arg_1;
            enable_periodic_check( arg_1 ) ;
         }
         else if ( strcmp( &argv[ arg ][ 1 ], "version" ) == 0 )
         {
            fprintf(stderr, "%s", program_version);
#ifdef LIBWRAP       
            fprintf(stderr, " libwrap");
#endif            
#ifdef HAVE_LOADAVG
            fprintf(stderr, " loadavg");
#endif          
            fprintf(stderr, "\n");
            exit(0);
         }
         else if ( strcmp ( &argv[ arg ][ 1 ], "inetd_compat" ) == 0 )
         {
            inetd_compat = 1;
         }
      }
      else
         break ;

   if ( filelog_option + syslog_option > 1 )
      usage() ;


   if ( argc - arg != 0 )
      usage() ;
   return( arg ) ;
}

static void usage(void)
{
   Sprint( 2, "Usage: %s [-d] [-f config_file] [-filelog filename] [-syslog facility] [-reuse] [-limit proc_limit] [-pidfile filaneme] [-logprocs limit] [-shutdownprocs limit] [-cc interval]\n", program_name ) ;
   exit( 1 ) ;
}

