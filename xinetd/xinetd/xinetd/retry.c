/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <sys/time.h>
#include <syslog.h>

#include "pset.h"
#include "retry.h"
#include "state.h"
#include "main.h"
#include "server.h"
#include "service.h"
#include "connection.h"
#include "xconfig.h"
#include "msg.h"
#include "sconf.h"
#include "xtimer.h"

static int retry_timer_running ;
static void cancel_retry(struct server * serp );
static void stop_retry_timer(void) ;
static void start_retry_timer(void) ;


/*
 * Attempt to start all servers in the retry table
 */
static void server_retry(void)
{
   unsigned          servers_started = 0 ;
   unsigned          u ;
   const char       *func = "server_retry" ;

   for ( u = 0 ; u < pset_count( RETRIES( ps ) ) ; u++ )
   {
      struct server *retry = SERP( pset_pointer( RETRIES( ps ), u ) ) ;
      struct service *sp = SERVER_SERVICE( retry ) ;
      connection_s *cp = SERVER_CONNECTION( retry ) ;

      /*
       * Drop the retry if access control fails or we have
       * a memory allocation problem
       */
      if ( svc_parent_access_control( sp, cp ) == FAILED ||
         svc_child_access_control (sp, cp) == FAILED ||
         pset_add( SERVERS( ps ), retry ) == NULL )
      {
         cancel_retry( retry ) ;
         pset_pointer( RETRIES( ps ), u ) = NULL ;
         continue ;
      }

      if ( server_start( retry ) == OK )
      {
         servers_started++ ;
         SVC_DEC_RETRIES( sp ) ;
         if ( !SVC_WAITS( sp ) )
            CONN_CLOSE( cp ) ;
         pset_pointer( RETRIES( ps ), u ) = NULL ;
         continue ;
      }
      else
      {
         pset_remove( SERVERS( ps ), retry ) ;
         if ( SERVER_FORKLIMIT( retry ) )
         {
            /*
             * give up retrying
             */
            msg( LOG_ERR, func,
               "service %s: too many consecutive fork failures", SVC_ID(sp) ) ;
            svc_log_failure( sp, cp, AC_FORK ) ;
            cancel_retry( retry ) ;
            pset_pointer( RETRIES( ps ), u ) = NULL ;
            continue ;
         }
         else
         {
            if ( debug.on )
               msg( LOG_DEBUG, func,
                  "fork failed for service %s. Retrying...", SVC_ID( sp ) ) ;
         }
      }
   }

   pset_compact( RETRIES( ps ) ) ;

   if ( debug.on )
      msg( LOG_DEBUG, func,
         "%d servers started, %d left to retry",
            servers_started, pset_count( RETRIES( ps ) ) ) ;

   /* If there's more, start another callback */
   if ( pset_count( RETRIES( ps ) ) > 0 ) {
      if ((retry_timer_running=xtimer_add(server_retry, RETRY_INTERVAL)) == -1)
      {
         msg( LOG_ERR, func, "xtimer_add: %m" ) ;
         retry_timer_running = 0;
      }
   }
   else
      retry_timer_running = 0;
}


/*
 * Schedule a retry by inserting the struct server in the retry table
 * and starting the timer if necessary
 */
status_e schedule_retry( struct server *serp )
{
   struct service *sp = SERVER_SERVICE( serp ) ;
   const char *func = "schedule_retry" ;

   if ( pset_add( RETRIES( ps ), serp ) == NULL )
   {
      out_of_memory( func ) ;
      return( FAILED ) ;
   }
   SVC_INC_RETRIES( sp ) ;
   start_retry_timer() ;
   if ( debug.on )
      msg( LOG_DEBUG, func, "Scheduled retry attempt for %s", SVC_ID( sp ) ) ;
   return( OK ) ;
}


/*
 * This function should not be called for servers that correspond to
 * services not in the service table because server_release will result
 * in releasing all memory associated with the service (since the ref
 * count will drop to 0).
 */
static void cancel_retry( struct server *serp )
{
   struct service *sp = SERVER_SERVICE( serp ) ;

   conn_free( SERVER_CONNECTION( serp ), 1 ) ;
   SVC_DEC_RETRIES( sp ) ;
   server_release( serp ) ;
}



/*
 * Cancel all retry attempts for the specified service
 */
void cancel_service_retries( struct service *sp )
{
   unsigned u ;
   const char *func = "cancel_service_retries" ;

   if ( SVC_RETRIES( sp ) == 0 )
      return ;

   u = 0 ;
   while ( u < pset_count( RETRIES( ps ) ) )
   {
      struct server *serp ;

      serp = SERP( pset_pointer( RETRIES( ps ), u ) ) ;
      if ( SERVER_SERVICE( serp ) == sp )
      {
         msg( LOG_NOTICE, func,
            "dropping retry attempt for service %s", SVC_ID( sp ) ) ;
         cancel_retry( serp ) ;
         pset_remove_index( RETRIES( ps ), u ) ;
         continue ;
      }
      u++ ;
   }

   if ( pset_count( RETRIES( ps ) ) == 0 )
      stop_retry_timer() ;
}


static void start_retry_timer(void)
{
   const char *func = "start_retry_timer" ;

   /*
    * Enable timer if necessary.
    */

   if ( retry_timer_running == 0 )
      if((retry_timer_running=xtimer_add(server_retry, RETRY_INTERVAL)) == -1 ){
         msg( LOG_ERR, func, "xtimer_add: %m" ) ;
         retry_timer_running = 0;
      }
}


static void stop_retry_timer(void)
{
   if ( retry_timer_running != 0)
   {
      xtimer_remove(retry_timer_running);
      retry_timer_running = 0 ;
   }
}

