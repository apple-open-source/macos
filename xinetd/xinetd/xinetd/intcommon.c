/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include "config.h"
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "intcommon.h"
#include "msg.h"
#include "signals.h"
#include "connection.h"
#include "sconf.h"
#include "state.h"
#include "main.h"
#include "xconfig.h"


void int_fail( const struct intercept_s *ip, const char *lsyscall )
{
   msg( LOG_ERR, "fail", "%s failed: %m", lsyscall ) ;
   (*ip->int_ops->exit)() ;
   /* NOTREACHED */
}


/*
 * Returns either a positive number or -1
 */
int int_select( int max, fd_set *read_mask )
{
   const char *func = "int_select" ;

   for ( ;; )
   {
      int n_ready ;

      n_ready = select( max+1, read_mask,
                                 FD_SET_NULL, FD_SET_NULL, TIMEVAL_NULL ) ;
      if ( n_ready > 0 )
         return( n_ready ) ;
      else if ( n_ready == -1 ) {
         if ( errno == EINTR )
            continue ;
         else
         {
            msg( LOG_ERR, func, "select: %m" ) ;
            return( -1 ) ;
         }
      }
   }
}


void int_exit( struct intercept_s *ip )
{
   int status = SERVER_EXITSTATUS( INT_SERVER( ip ) ) ;
   const char *func = "int_exit" ;

   if ( debug.on )
   {
      if ( PROC_EXITED( status ) )
         msg( LOG_DEBUG, func, "intercepted server died" ) ;
      else if ( PROC_SIGNALED( status ) )
         msg( LOG_DEBUG, func, "intercepted server received signal %s",
               sig_name( (int) PROC_TERMSIG( status ) ) ) ;
   }
   _exit( (int) PROC_EXITSTATUS( status ) ) ;
}


/*
 * The ops vector must be installed before invoking this function
 */
void int_init( struct intercept_s *ip, struct server *serp )
{
   unsigned u ;
   const char *func = "int_init" ;

   /*
    * Sanity test
    */
   if ( SERVER_SERVICE( serp ) != SERVER_CONNSERVICE( serp ) )
   {
      msg( LOG_ERR, func, "server service (%s) != connection service (%s)",
                           SVC_ID( SERVER_SERVICE( serp ) ),
                              SVC_ID( SERVER_CONNSERVICE( serp ) ) ) ;
      exit( 1 ) ;
   }

   /*
    * Close all unneeded descriptors
    */
   for ( u = 0 ; u < pset_count( SERVICES( ps ) ) ; u++ )
   {
      struct service *sp = SP( pset_pointer( SERVICES( ps ), u ) ) ;

      if ( sp == SERVER_SERVICE( serp ) )
         continue ;
      if ( LOG_GET_TYPE( SC_LOG( SVC_CONF( sp ) ) ) == L_FILE )
         xlog_destroy( sp->svc_log ) ;
      (void) close( SVC_FD( sp ) ) ;
   }

   /*
    * Setup signal handling
    */
   if ( signal( SERVER_EXIT_SIG, int_sighandler ) == SIG_ERR )
      int_fail( ip, "signal" ) ;
   if ( signal( INTERCEPT_SIG, int_sighandler ) == SIG_ERR )
      int_fail( ip, "signal" ) ;
   if ( signal( SIGTERM, int_sighandler ) == SIG_ERR )
      int_fail( ip, "signal" ) ;
   
   /*
    * Initialize state
    */
   INTERCEPT( ip ) = TRUE ;
   *INT_SERVER( ip ) = *serp ;
   INT_REMOTE( ip ) = SERVER_FD( serp ) ;

   INT_CONNECTIONS( ip ) = pset_create( 0, 0 ) ;
   if ( INT_CONNECTIONS( ip ) == NULL )
   {
      msg( LOG_ERR, func, ES_NOMEM ) ;
      (*ip->int_ops->exit)() ;
   }
}


/*
 * Make a new connection to the local server
 */
channel_s *int_newconn( struct intercept_s *ip, 
                        union xsockaddr *sinp,
                        int remote_socket )
{
   struct service       *sp          = SERVER_SERVICE( INT_SERVER( ip ) ) ;
   int                   socket_type = SVC_SOCKET_TYPE( sp ) ;
   union xsockaddr      *local       = INT_LOCALADDR( ip ) ;
   char                 *sid         = SVC_ID( sp ) ;
   channel_s            *chp ;
   int                   sd ;
   const char           *func = "int_newconn" ;

   /*
    * Get a socket and connect it to the local address
    *
    */
   if ( ( sd = socket( local->sa.sa_family, socket_type, SC_PROTOVAL(SVC_CONF(sp)) ) ) == -1 )
   {
      msg( LOG_ERR, func,"(intercepting %s) socket creation failed: %m", sid ) ;
      return( CHANNEL_NULL ) ;
   }

   if ( connect( sd, SA( local ), sizeof( *local ) ) == -1 )
   {
      msg( LOG_ERR, func, "(intercepting %s) connect failed: %m", sid ) ;
      (void) close( sd ) ;
      return( CHANNEL_NULL ) ;
   }

   chp = NEW_CHANNEL() ;
   if ( chp == CHANNEL_NULL )
   {
      msg( LOG_ERR, func, ES_NOMEM ) ;
      (void) close( sd ) ;
      return( CHANNEL_NULL ) ;
   }

   if ( pset_add( INT_CONNECTIONS( ip ), chp ) == NULL )
   {
      msg( LOG_ERR, func, ES_NOMEM ) ;
      FREE_CHANNEL( chp ) ;
      (void) close( sd ) ;
      return( CHANNEL_NULL ) ;
   }

   chp->ch_state = GOOD_CHANNEL ;
   chp->ch_from = *sinp ;
   chp->ch_local_socket = sd ;
   chp->ch_remote_socket = remote_socket ;
   return( chp ) ;
}



/*
 * Check if the (address,port) in sinp is already in the connection table.
 * Return value:
 *    a connection pointer if the address is found
 *    NULL if the address if not found
 *
 * *addr_checked is set to TRUE of FALSE depending on whether there
 * is already a connection from the same IP address in the table.
 */
channel_s *int_lookupconn( struct intercept_s *ip, 
                           union xsockaddr *sinp,
                           bool_int *addr_checked )
{
   unsigned      u ;
   pset_h      conntab = INT_CONNECTIONS( ip ) ;

   *addr_checked = FALSE ;

   for ( u = 0 ; u < pset_count( conntab ) ; u++ )
   {
      register channel_s *chp = CHP( pset_pointer( conntab, u ) ) ;

      if ( memcmp( &chp->ch_from, sinp, sizeof( *sinp ) ) == 0 )
      {
         *addr_checked = TRUE ;
         if ( xaddrport(&chp->ch_from) == xaddrport(sinp) )
            return( chp ) ;
      }
   }
   return( CHANNEL_NULL ) ;
}
