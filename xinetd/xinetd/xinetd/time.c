/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <sys/types.h>
#include <syslog.h>
#include <ctype.h>
#include <time.h>

#include "sio.h"
#include "timex.h"
#include "msg.h"
#include "util.h"


#define IN_RANGE( val, low, high )     ( (low) <= (val) && (val) <= (high) )

struct time_interval
{
   int16_t min_start ;
   int16_t min_end ;
} ;

#define TIP( p )               ( (struct time_interval *) (p) )
#define NEW_TI()               NEW( struct time_interval )
#define FREE_TI( tip )         FREE( tip )


/*
 * Returns TRUE if the current time is within at least one of the intervals
 */
bool_int ti_current_time_check( const pset_h intervals )
{
   time_t      current_time ;
   unsigned    u ;
   int16_t     min_current ;
   struct tm   *tmp ;

   (void) time( &current_time ) ;
   tmp = localtime( &current_time ) ;
   min_current = tmp->tm_hour * 60 + tmp->tm_min ;

   for ( u = 0 ; u < pset_count( intervals ) ; u++ )
   {
      struct time_interval *tip ;
      
      tip = TIP( pset_pointer( intervals, u ) ) ;
      if ( IN_RANGE( min_current, tip->min_start, tip->min_end ) )
         return( TRUE ) ;
   }
   return( FALSE ) ;
}


static int get_num( int *nump, 
                       int min_val, 
                       int max_val, 
                       const char *s, 
                       char stop_char )
{
   const char *func = "get_num" ;
   int i = 0;

   for ( *nump = 0 ; isdigit( s[i] ) ; i++ )
   {
      *nump *= 10 ;
      *nump += s[i] - '0' ;
   }

   if ( s[i] != stop_char )
   {
      parsemsg( LOG_ERR, func, "incorrect time interval" ) ;
      return( -1 );
   }

   if ( ! IN_RANGE( *nump, min_val, max_val ) )
   {
      parsemsg( LOG_ERR, func, "invalid time interval" ) ;
      return( -1 ) ;
   }
   return( i ) ;
}


/*
 * Each interval should have the form:
 *    hour:min-hour:min
 * Example: 2:30-4:15
 */
status_e ti_add( pset_h iset, const char *interval_str )
{
   struct time_interval   *tip ;
   int                  hours ;
   int                  minutes ;
   int                  min_start ;
   int                  min_end ;
   int                  p, r = 0 ;
   const char          *func = "add_interval" ;

   while (interval_str[r] == ' ')
	   r++;		/* Eat white space */
   if ( ( p = get_num( &hours, 0, 23, interval_str+r, ':' ) ) == -1 )
      return( FAILED ) ;
   r += p;
   r++;			/* Get past : */
   if ( ( p = get_num( &minutes, 0, 59, interval_str+r, '-' ) ) == -1 )
      return( FAILED ) ;
   min_start = hours * 60 + minutes ;

   r += p;
   r++;			/* Get past - */
   if ( ( p = get_num( &hours, 0, 23, interval_str+r, ':' ) ) == -1 )
      return( FAILED ) ;
   r += p;
   r++;			/* Get past : */
   if ( get_num( &minutes, 0, 59, interval_str+r, NUL ) == -1 )
      return( FAILED ) ;
   min_end = hours * 60 + minutes ;
   if ( min_start >= min_end )
   {
      parsemsg( LOG_ERR, func, "invalid time interval: %s", interval_str ) ;
      return( FAILED ) ;
   }

   tip = NEW_TI() ;
   if ( tip == NULL )
   {
      out_of_memory( func ) ;
      return( FAILED ) ;
   }
   tip->min_start = min_start ;
   tip->min_end = min_end ;
   if ( pset_add( iset, tip ) == NULL )
   {
      FREE_TI( tip ) ;
      out_of_memory( func ) ;
      return( FAILED ) ;
   }
   return( OK ) ;
}


void ti_dump( pset_h iset, int fd )
{
   unsigned u ;

   for ( u = 0 ; u < pset_count( iset ) ; u++ )
   {
      struct time_interval *tip = TIP( pset_pointer( iset, u ) ) ;

      Sprint( fd, " %02d:%02d-%02d:%02d",
         tip->min_start / 60, tip->min_start % 60,
         tip->min_end / 60, tip->min_end % 60 ) ;
   }
}


void ti_free( pset_h iset )
{
   pset_apply( iset, free, NULL ) ;
}

