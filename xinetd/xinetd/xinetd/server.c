/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include "config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#include "pset.h"
#include "sio.h"
#include "server.h"
#include "util.h"
#include "msg.h"
#include "service.h"
#include "sconf.h"
#include "state.h"
#include "main.h"
#include "xconfig.h"
#include "retry.h"
#include "child.h"
#include "signals.h"


#define NEW_SERVER()                NEW( struct server )
#define FREE_SERVER( serp )         FREE( serp )


#ifndef DEBUG_RETRY
#define do_fork()         fork()
#else
#include <errno.h>
extern int errno ;

/*
 * 3 out of 4 times the do_fork() will fail
 */
#define do_fork()         ( random() & 0x11 ) ? ( errno = EAGAIN, -1 ) : fork()
#endif    /* DEBUG_RETRY */


/*
 * Allocate a server, initialize it from init_serp, and insert it in the server
 * table.
 */
struct server *server_alloc( const struct server *init_serp )
{
   struct server   *serp ;
   const char      *func = "server_alloc" ;

   serp = NEW_SERVER() ;
   if ( serp == NULL )
   {
      out_of_memory( func ) ;
      return( NULL ) ;
   }

   if ( pset_add( SERVERS(ps), serp ) == NULL )
   {
      msg( LOG_CRIT, func, "couldn't insert server in server table" ) ;
      CLEAR( *serp ) ;
      FREE_SERVER( serp ) ;
      return( NULL ) ;
   }

   *serp = *init_serp ;         /* initialize it */
   SVC_HOLD( serp->svr_sp ) ;

   return( serp ) ;
}


void server_release( struct server *serp )
{
   struct service   *sp   = SERVER_SERVICE( serp ) ;
   int              count = SVC_RELE( sp ) ;

   pset_remove(SERVERS(ps), serp);
   if ( count == 0 && ! SC_IS_SPECIAL( SVC_CONF( sp ) ) )
      pset_remove( SERVICES( ps ), sp ) ;
   
   CLEAR( *serp ) ;
   FREE_SERVER( serp ) ;
}


/*
 * If a service is internal and does not require forking a process:
 *      -    if it accepts connections, we put the accepted connection
 *       in non-blocking mode to avoid a possible block on 
 *       the write(2).
 *      -   the log flags that have to do with the server exiting are 
 *         ignored (i.e. nothing is logged).
 *      -   it can be identified in the log because the server pid is 0.
 */
static void server_internal( struct server *serp )
{
   struct service *sp = serp->svr_sp ;
   const char *func = "server_internal" ;

   serp->svr_pid = 0 ;
   if ( SVC_ACCEPTS_CONNECTIONS( sp ) &&
            fcntl( SERVER_FD( serp ), F_SETFL, FNDELAY ) == -1 )
   {
      msg( LOG_ERR, func, "%s: fcntl F_SETFL failed: %m", SVC_ID( sp ) ) ;
      return ;
   }
   svc_log_success( sp, SERVER_CONNECTION(serp), serp->svr_pid ) ;
   SVC_INTERNAL( sp, serp ) ;
}
 

/*
 * Attempt to start a server for service 'sp' to handle
 * connection 'cp'.
 * Return value:
 *   OK:   if a server is started or a retry attempt is scheduled
 *   FAILED:   otherwise (a log entry is also made)
 */
status_e server_run( struct service *sp, connection_s *cp )
{
   struct server    server ;
   struct server   *serp = NULL;
   const char      *func = "server_run" ;

   CLEAR( server ) ;
   server.svr_sp = sp ;
   server.svr_conn = cp ;

   if ( ! SVC_FORKS( sp ) )
   {  /*
       * SG - Added this check so that internal services get the
       * same protection that external services get. This is
       * mandatory for the sensor patch to work.
       */

      if (svc_child_access_control( sp, cp ) == OK)
         server_internal( &server ) ;
      else {
         if ( SVC_WAITS( sp ) )
            svc_resume( sp );
         return( FAILED );
      }
      if ( SVC_WAITS( sp ) )
         svc_resume( sp );
      return( OK ) ;
   }

   /*
    * Insert new struct server in server table first, to avoid the
    * possibility of running out of memory *after* the fork.
    */
   serp = server_alloc( &server ) ;
   if ( serp == NULL )
      return( FAILED ) ;

   if ( server_start( serp ) == OK )
   {
      if( !SVC_WAITS(sp) )
         CONN_CLOSE( cp ) ;
      return( OK ) ;
   }

   /* server will be removed in server_release() */

   /*
    * Currently, fork failures are the only reason for retrying.
    * There is no retry if we exceed the max allowed number of fork failures
    */
   if ( ! SERVER_FORKLIMIT( serp ) && SVC_RETRY( sp ) )
   {
      if ( schedule_retry( serp ) == OK )
         return( OK ) ;
      else
         msg( LOG_ERR, func, "Retry failure for %s service", SVC_ID( sp ) ) ;
   }
   else
      svc_log_failure( sp, cp, AC_FORK ) ;

   server_release( serp ) ;
   return( FAILED ) ;
}


/*
 *  Try to fork a server process.
 *  Actually, we won't fork if tcpmux_child is set, becuase we have
 *  already forked to keep the xinetd parent from blocking on the
 *  read of the service name.
 */
status_e server_start( struct server *serp )
{
   struct service   *sp = serp->svr_sp ;
   const char       *func = "server_start" ;

   if( debug.on )
      msg( LOG_DEBUG, func, "Starting service %s", SC_NAME( SVC_CONF( sp ) ) );
   serp->svr_log_remote_user = SVC_LOGS_USERID_ON_SUCCESS( sp ) ;
   
   serp->svr_pid = do_fork() ;

   switch ( serp->svr_pid )
   {
      case 0:
         ps.rws.env_is_valid = FALSE ;
         child_process( serp ) ;

         msg( LOG_ERR, func, "INTERNAL ERROR: child_process returned" ) ;
         _exit( 0 ) ;
         /* NOTREACHED */
      
      case -1:
         msg( LOG_ERR, func, "%s: fork failed: %m", SVC_ID( sp ) ) ;
         serp->svr_fork_failures++ ;
         return( FAILED ) ;

      default:
         (void) time( &serp->svr_start_time ) ;
         SVC_INC_RUNNING_SERVERS( sp ) ;

         /*
          * Log the start of another server (if it is not an interceptor).
          * Determine if the server writes to the log (because in that case
          * we will have to check the log size).
          */
         if ( ! SVC_IS_INTERCEPTED( sp ) )
            svc_log_success( sp, SERVER_CONNECTION(serp), serp->svr_pid ) ;
         else
            serp->svr_writes_to_log = SVC_IS_LOGGING( sp ) ;
         serp->svr_writes_to_log |= serp->svr_log_remote_user ;
         return( OK ) ;
   }
}


void server_dump( const struct server *serp, int fd )
{
   const struct service *sp = serp->svr_sp ;

   Sprint( fd, "%s server\n", SVC_ID( sp ) ) ;
   Sprint( fd, "pid = %d\n", serp->svr_pid ) ;
   Sprint( fd, "start_time = %s", ctime( &serp->svr_start_time ) ) ;
   Sprint( fd, "Connection info:\n" ) ;
   conn_dump( serp->svr_conn, fd ) ;
   if ( serp->svr_fork_failures )
      Sprint( fd, "fork_failures = %d\n", serp->svr_fork_failures ) ;
   Sprint( fd,
         "log_remote_user = %s\n", serp->svr_log_remote_user ? "YES" : "NO" ) ;
   Sprint( fd,
         "writes_to_log = %s\n", serp->svr_writes_to_log ? "YES" : "NO" ) ;
   Sputchar( fd, '\n' ) ;
   Sflush( fd ) ;
}


/*
 * Invoked when a server dies, either because of a signal or in case of
 * a normal exit.
 */
void server_end( struct server *serp )
{
   struct service *sp = serp->svr_sp ;
   const char *func = "server_end" ;

   if ( PROC_EXITED( serp->svr_exit_status ) || 
         PROC_SIGNALED( serp->svr_exit_status ) )
   {
      const char *death_type = PROC_EXITED( serp->svr_exit_status ) ? "exited"
           : "died" ;
      if ( debug.on )
      {
         struct service *conn_sp = CONN_SERVICE( serp->svr_conn ) ;

         if ( conn_sp == sp )
            msg( LOG_DEBUG, func,
               "%s server %d %s", SVC_ID( sp ) , serp->svr_pid, death_type ) ;
         else
            msg( LOG_DEBUG, func,
               "%s server %d running on behalf of service %s %s",
                  SVC_ID( sp ), serp->svr_pid, SVC_ID( conn_sp ), death_type ) ;
      }
      
      /* Added this for when accepting wait=yes services */
      if( SVC_WAITS( sp ) )
         FD_SET( SVC_FD( sp ), &ps.rws.socket_mask ) ;

      svc_postmortem( sp, serp ) ;
      server_release( serp ) ;
   }
   else if ( PROC_STOPPED( serp->svr_exit_status ) )
      msg( LOG_WARNING, func, "service %s: server with pid %d stopped",
         SVC_ID( sp ), serp->svr_pid ) ;
}


/*
 * Find the running server with the specified pid
 */
struct server *server_lookup( pid_t pid )
{
   unsigned u ;

   for ( u = 0 ; u < pset_count( SERVERS( ps ) ) ; u++ )
   {
      register struct server *serp ;

      serp = SERP( pset_pointer( SERVERS( ps ), u ) ) ;
      if ( serp->svr_pid == pid )
         return( serp ) ;
   }
   return( NULL ) ;
}

