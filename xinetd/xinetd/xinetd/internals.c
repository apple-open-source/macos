/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include "sio.h"
#include "internals.h"
#include "msg.h"
#include "sconf.h"
#include "state.h"
#include "main.h"
#include "xconfig.h"
#include "xtimer.h"
#include "options.h"

static unsigned thread_check( register struct service *sp,unsigned running_servers, unsigned retry_servers );
static unsigned refcount_check( struct service *sp, unsigned *running_servers, unsigned *retry_servers );
static unsigned service_count_check( register struct service *sp, unsigned running_servers, unsigned retry_servers );
static void periodic_check(void);


static void dump_services( int fd )
{
   unsigned u ;

   /*
    * Dump the current configuration (services + defaults)
    */
   Sprint( fd, "Services + defaults:\n" ) ;
   sc_dump( DEFAULTS( ps ), fd, 0, TRUE ) ;

   for ( u = 0 ; u < pset_count( SERVICES( ps ) ) ; u++ )
      svc_dump( SP( pset_pointer( SERVICES( ps ), u ) ), fd ) ;
}

void dump_internal_state(void)
{
   int dump_fd ;
   const char *dump_file = DUMP_FILE ;
   time_t current_time ;
   int fd ;
   unsigned u ;
   const char *func = "dump_internal_state" ;

   if ( debug.on )
      msg( LOG_DEBUG, func, "Dumping State" ) ;

   dump_fd = open( dump_file, O_WRONLY | O_CREAT | O_APPEND, DUMP_FILE_MODE);
   if ( dump_fd == -1 )
   {
      msg( LOG_ERR, func, "failed to open %s: %m", dump_file ) ;
      return ;
   }
   
   if (Sbuftype( dump_fd, SIO_LINEBUF ) == SIO_ERR )
   {
       /*
	* If the above function failed, Sprint will most likely
	* fail, too. Output a message for troubleshooting and quit.
	*/
       msg( LOG_ERR, func, "failed setting up sio buffering: %m" ) ;
       return;
   }

   /*
    * Print the program name, version, and timestamp.
    * Note that the program_version variable contains the program name.
    */
   (void) time( &current_time ) ;
   Sprint( dump_fd, "INTERNAL STATE DUMP: %s\n", program_version ) ;
   Sprint( dump_fd, "Current time: %s\n", ctime( &current_time ) ) ;

   dump_services( dump_fd ) ;

   /*
    * Dump the server table
    */
   Sprint( dump_fd, "Server table dump:\n" ) ;
   for ( u = 0 ; u < pset_count( SERVERS( ps ) ) ; u++ )
      server_dump( SERP( pset_pointer( SERVERS( ps ), u ) ), dump_fd ) ;
   Sputchar( dump_fd, '\n' ) ;

   /*
    * Dump the retry_table
    */
   Sprint( dump_fd, "Retry table dump:\n" ) ;
   for ( u = 0 ; u < pset_count( RETRIES( ps ) ) ; u++ )
      server_dump( SERP( pset_pointer( RETRIES( ps ), u ) ), dump_fd ) ;
   Sputchar( dump_fd, '\n' ) ;

   /*
    * Dump the socket mask
    */
   Sprint( dump_fd, "Socket mask:" ) ;
   for ( fd = 0 ; fd < ps.ros.max_descriptors ; fd++ )
      if ( FD_ISSET( fd, &ps.rws.socket_mask ) )
         Sprint( dump_fd, " %d", fd ) ;
   Sputchar( dump_fd, '\n' ) ;
   Sprint( dump_fd, "mask_max = %d\n", ps.rws.mask_max ) ;

   /*
    * Dump the descriptors that are open and are *not* in the socket mask
    */
   Sprint( dump_fd, "Open descriptors (not in socket mask):" ) ;
   for ( fd = 0 ; fd < ps.ros.max_descriptors ; fd++ )
   {
      struct stat st ;

      if ( FD_ISSET( fd, &ps.rws.socket_mask ) )
         continue ;
      if ( fstat( fd, &st ) == -1 )
         continue ;
      Sprint( dump_fd, " %d", fd ) ;
   }
   Sputchar( dump_fd, '\n' ) ;
   Sputchar( dump_fd, '\n' ) ;

   Sprint( dump_fd, "active_services = %d\n", ps.rws.active_services ) ;
   Sprint( dump_fd, "available_services = %d\n", ps.rws.available_services ) ;
   Sprint( dump_fd, "descriptors_free = %d\n", ps.rws.descriptors_free ) ;
   Sprint( dump_fd, "running_servers = %d\n", pset_count( SERVERS( ps ) ) ) ;
   Sprint( dump_fd, "Logging service = %s\n",
            LOG_SERVICE( ps ) != NULL ? "enabled" : "not enabled" ) ;
   Sputchar( dump_fd, '\n' ) ;

   Sprint( dump_fd, "max_descriptors = %d\n", (int)ps.ros.max_descriptors ) ;
   Sprint( dump_fd, "process_limit = %d\n", (int)ps.ros.process_limit ) ;
   Sprint( dump_fd, "config_file = %s\n", ps.ros.config_file ) ;
   if ( debug.on )
      Sprint( dump_fd, "debug_fd = %d\n", debug.fd ) ;
   Sputchar( dump_fd, '\n' ) ;

   Sprint( dump_fd, "END OF DUMP\n\n" ) ;
   Sclose( dump_fd );

   msg( LOG_INFO, func, "generated state dump in file %s", dump_file ) ;
}


/*
 * Types of consistency checks
 */
enum check_type { PERIODIC, USER_REQUESTED } ;


static void consistency_check( enum check_type type )
{
   int         fd ;
   fd_set      socket_mask_copy ;
   unsigned    u ;
   int         errors ;
   unsigned    total_running_servers        = 0 ;
   unsigned    total_retry_servers          = 0 ;
   unsigned    error_count                  = 0 ;
   bool_int    service_count_check_failed   = FALSE ;
   const char  *func                        = "consistency_check" ;

   socket_mask_copy = ps.rws.socket_mask ;

   for ( u = 0 ; u < pset_count( SERVICES( ps ) ) ; u++ )
   {
      register struct service *sp = SP( pset_pointer( SERVICES( ps ), u ) ) ;
      char *sid = SVC_ID( sp ) ;
      unsigned   running_servers ;
      unsigned   retry_servers ;

      error_count += refcount_check( sp, &running_servers, &retry_servers ) ;

      if ( SVC_IS_AVAILABLE( sp ) || SVC_IS_DISABLED ( sp ) )
      {
         /*
          * In this case, there may be some servers running
          */
         if ( FD_ISSET( SVC_FD( sp ), &socket_mask_copy ) )
         {
            if ( SVC_IS_DISABLED( sp ) )
            {
               msg( LOG_ERR, func,
                  "fd of disabled service %s still in socket mask", sid ) ;
               error_count++ ;
            }
            FD_CLR( SVC_FD( sp ), &socket_mask_copy ) ;
         }
         error_count += thread_check( sp, running_servers, retry_servers ) ;

         errors = service_count_check( sp, running_servers, retry_servers ) ;
         if ( ! errors && ! service_count_check_failed )
         {
            total_retry_servers += retry_servers ;
            total_running_servers += running_servers ;
         }
         if ( errors )
         {
            service_count_check_failed = TRUE ;
            error_count += errors ;
         }

         if ( SVC_IS_DISABLED( sp ) && SVC_RUNNING_SERVERS( sp ) == 0 )
         {
            msg( LOG_ERR, func,
               "disabled service %s has 0 running servers\n", sid ) ;
            error_count++ ;
            continue ;
         }
      }
      else
      {
         msg( LOG_ERR, func, "service %s not started", SVC_ID( sp ) ) ;
         error_count++ ;
      }
   }

   if ( ! service_count_check_failed )
   {
      if ( total_running_servers != pset_count( SERVERS( ps ) ) )
      {
         msg( LOG_ERR, func,
            "total running servers (%d) != number of running servers (%d)",
               total_running_servers, pset_count( SERVERS( ps ) ) ) ;
         error_count++ ;
      }
      if ( total_retry_servers != pset_count( RETRIES( ps ) ) )
      {
         msg( LOG_ERR, func,
            "total retry servers (%d) != number of retry servers (%d)",
               total_retry_servers, pset_count( RETRIES( ps ) ) ) ;
         error_count++ ;
      }
   }

   /*
    * Check if there are any descriptors set in socket_mask_copy
    */
   for ( fd = 0 ; fd < ps.ros.max_descriptors ; fd++ )
      if ( FD_ISSET( fd, &socket_mask_copy ) && ((fd != signals_pending[0]) && fd != signals_pending[1]))
      {
         msg( LOG_ERR, func,
            "descriptor %d set in socket mask but there is no service for it",
               fd ) ;
         error_count++ ;
      }

   if ( error_count != 0 )
      msg( LOG_WARNING, func,
            "Consistency check detected %d errors", error_count ) ;
   else
      if ( type == USER_REQUESTED || debug.on )
         msg( LOG_INFO, func, "Consistency check passed" ) ;

   if( type == PERIODIC ) 
      if ( xtimer_add( periodic_check, ps.ros.cc_interval ) == -1 )
         msg( LOG_ERR, func, "Failed to start consistency timer" ) ;
}


/*
 * Check that the counts of running and retry servers stored in struct service
 * are accurate
 */
static unsigned service_count_check( struct service *sp, 
                                      unsigned running_servers, 
                                      unsigned retry_servers )
{
   char *sid = SVC_ID( sp ) ;
   int error_count = 0 ;
   const char *func = "service_count_check" ;

   if ( SVC_RUNNING_SERVERS( sp ) != running_servers )
   {
      msg( LOG_ERR, func,
         "service %s: actual running servers = %d, known running servers = %d",
            sid, running_servers, SVC_RUNNING_SERVERS( sp ) ) ;
      error_count++ ;
   }
   if ( SVC_RETRIES( sp ) != retry_servers )
   {
      msg( LOG_ERR, func,
         "service %s: actual retry servers = %d, known retry servers = %d",
            sid, retry_servers, SVC_RETRIES( sp ) ) ;
      error_count++ ;
   }

   if ( error_count && debug.on )
      msg( LOG_DEBUG, func, "%s: %d errors detected", sid, error_count ) ;

   return( error_count ) ;
}



/*
 * If the service is single-threaded:
 *         if the descriptor is set in the socket mask, there must
 *         be a server running (or to be retried)
 *   If the service is multi-threaded:
 *         the descriptor must be always set
 */
static unsigned thread_check( struct service *sp, 
                               unsigned running_servers, 
                               unsigned retry_servers )
{
   unsigned error_count = 0 ;
   int sd = SVC_FD( sp ) ;
   char *sid = SVC_ID( sp ) ;
   const char *func = "thread_check" ;

   if ( SVC_WAITS( sp ) )
   {
      bool_int has_servers = ( running_servers + retry_servers != 0 ) ;

      if ( has_servers && FD_ISSET( sd, &ps.rws.socket_mask ) )
      {
         msg( LOG_ERR, func,
"Active single-threaded service %s: server running, descriptor set", sid ) ;
         error_count++ ;
      }
      if ( !has_servers && !FD_ISSET( sd, &ps.rws.socket_mask ) )
      {
         msg( LOG_ERR, func,
"Active single-threaded service %s: no server running, descriptor not set",
            sid ) ;
         error_count++ ;
      }
   }
   else
      if ( ! FD_ISSET( sd, &ps.rws.socket_mask ) )
      {
         msg( LOG_ERR, func,
            "Active multi-threaded service %s: descriptor not set", sid ) ;
         error_count++ ;
      }

   if ( error_count && debug.on )
      msg( LOG_DEBUG, func, "%s: %d errors detected", sid, error_count ) ;

   return( error_count ) ;
}



/*
 * Count the number of references to the specified service contained
 * in the specified table of servers; put the number of servers
 * in *countp
 */
static int count_refs( struct service *sp, pset_h servers, unsigned *countp )
{
   unsigned u ;
   struct server *serp ;
   int refs = 0 ;
   unsigned count = 0 ;

   for ( u = 0 ; u < pset_count( servers ) ; u++ )
   {
      serp = SERP( pset_pointer( SERVERS( ps ), u ) ) ;
      if ( SERVER_SERVICE( serp ) == sp )
      {
         refs++ ;
         count++ ;
      }
      if ( SERVER_CONNSERVICE( serp ) == sp )
         refs++ ;
      /*
       * XXX:   in the future we may want to check if the given service
       *         is any of the alternative services (currently only SPECIAL
       *         services can be alternative services and SPECIAL services
       *         are not included in the service table)
       */
   }
   *countp = count ;
   return( refs ) ;
}


/*
 * Check for reference counting errors.
 * Returns number of errors found.
 * Always set the number of running and retry servers.
 */
static unsigned refcount_check( struct service *sp, 
                                 unsigned *running_servers, 
                                 unsigned *retry_servers )
{
   char *sid = SVC_ID( sp ) ;
   unsigned errors = 0 ;
   int refs ;
   int refcount = SVC_REFCOUNT( sp ) ;
   const char *func = "refcount_check" ;

   if ( refcount <= 0 )
   {
      msg( LOG_ERR, func, "%s service has bad refcount: %d",
               sid, refcount ) ;
      errors++ ;
   }

   /*
    * The service table holds a reference to the service. The remaining
    * references must be from servers and connections.
    */
   refcount-- ;

   refs = count_refs( sp, SERVERS( ps ), running_servers ) ;
   if ( ! errors && refs > refcount )
   {
      msg( LOG_ERR, func,
         "running servers: too many references for %s (%d with max=%d)",
            sid, refs, refcount ) ;
      errors++ ;
   }

   refs = count_refs( sp, RETRIES( ps ), retry_servers ) ;
   if ( ! errors && refs > refcount )
   {
      msg( LOG_ERR, func,
         "retry servers: too many references for %s (%d with max=%d)",
            sid, refs, refcount ) ;
      errors++ ;
   }

   if ( errors && debug.on )
      msg( LOG_DEBUG, func, "%s: %d errors detected", sid, errors ) ;

   return( errors ) ;
}


void user_requested_check(void)
{
   consistency_check( USER_REQUESTED ) ;
} 


static void periodic_check(void)
{
   consistency_check( PERIODIC ) ;
}

/* This actually gets called during initialization, so be careful what
 * gets put in here.
 */
void enable_periodic_check( unsigned interval )
{
   const char *func = "enable_periodic_check" ;

   if ( xtimer_add( periodic_check, interval ) == -1 )
   {
      msg( LOG_ERR, func, "Failed to start consistency timer" ) ;
      return ;
   }
}
