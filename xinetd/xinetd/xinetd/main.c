/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>

#include "main.h"
#include "init.h"
#include "msg.h"
#include "internals.h"
#include "signals.h"
#include "service.h"
#include "sconf.h"
#include "xtimer.h"
#include "sensor.h"

#ifdef __GNUC__
__attribute__ ((noreturn))
#endif
static void main_loop(void);
static void find_bad_fd(void) ;

/*
 * The following are the only global variables of this program
 */
struct program_state ps ;
struct debug debug ;
char program_version[] = XINETD_VERSION ;
int signals_pending[2] = {-1, -1} ;

/*
 * This is where the story starts...
 */
int main( int argc, char *argv[] )
{
   const char            *func = "main" ;

   init_daemon( argc, argv ) ;
   init_services() ;

   /* Do the chdir after reading the config file.  Relative path names
    * will work better.  
    */
   if (chdir("/") < 0) {
      msg(LOG_ERR, func, "Can't chdir to /: %m");
   }

   /* Print out all the options we're compiled with.  Makes support 
    * a tad easier.
    * Also, try to get them all into one syslog message for atomicity
    */
   msg( LOG_NOTICE, func, "%s started with "
#ifdef LIBWRAP
   "libwrap "
#endif
#ifdef HAVE_LOADAVG
   "loadavg "
#endif
#if !defined(LIBWRAP) && !defined(HAVE_LOADAVG)
   "no "
#endif
   "options compiled in."
   , XINETD_VERSION );

   msg( LOG_NOTICE, func, "Started working: %d available service%s",
      ps.rws.available_services,
         ( ps.rws.available_services != 1 ) ? "s" : "" ) ;

   /*
    * The reason for doing the setjmp here instead of in main_loop is
    * that setjmp is not guaranteed to restore register values which
    * can cause a problem for register variables
    */
   if ( sigsetjmp( ps.rws.env, 1 ) == 0 )
      ps.rws.env_is_valid = TRUE ;

   main_loop() ;

   /* NOTREACHED */
   exit(1);
}


/*
 * What main_loop does:
 *
 *      select on all active services
 *      for each socket where a request is pending
 *         try to start a server
 */
static void main_loop(void)
{
   const char      *func = "main_loop" ;
   struct timeval   tv, *tvptr = NULL;

   FD_SET(signals_pending[0], &ps.rws.socket_mask);
   if ( signals_pending[0] > ps.rws.mask_max )
      ps.rws.mask_max = signals_pending[0] ;
   if ( signals_pending[1] > ps.rws.mask_max )
      ps.rws.mask_max = signals_pending[1] ;

   for ( ;; )
   {
      fd_set read_mask ;
      int n_active ;
      unsigned u ;

      if ( debug.on ) 
         msg( LOG_DEBUG, func,
               "active_services = %d", ps.rws.active_services ) ;

      /* get the next timer value, if there is one, and select for that time */
      if( (tv.tv_sec = xtimer_nexttime()) >= 0 ) {
         tv.tv_usec = 0;
         tvptr = &tv;
      } else {
         tvptr = NULL;
      }

      read_mask = ps.rws.socket_mask ;
      n_active = select( ps.rws.mask_max+1, &read_mask,
                        FD_SET_NULL, FD_SET_NULL, tvptr ) ;
      if ( n_active == -1 )
      {
         if ( errno == EINTR ) {
            continue ;
         } else if ( errno == EBADF )
            find_bad_fd() ;
         continue ;
      }
      else if ( n_active == 0 ) {
         xtimer_poll();
         continue ;
      }

      if ( debug.on )
         msg( LOG_DEBUG, func, "select returned %d", n_active ) ;

      xtimer_poll();

      if( FD_ISSET(signals_pending[0], &read_mask) ) {
         check_pipe();
            if ( --n_active == 0 )
               continue ;
      }

      for ( u = 0 ; u < pset_count( SERVICES( ps ) ) ; u++ )
      {
         struct service *sp ;

         sp = SP( pset_pointer( SERVICES( ps ), u ) ) ;

         if ( ! SVC_IS_ACTIVE( sp ) )
            continue ;

         if ( FD_ISSET( SVC_FD( sp ), &read_mask ) )
         {
            svc_request( sp ) ;
            if ( --n_active == 0 )
               break ;
         }
      }
      if ( n_active > 0 )
         msg( LOG_ERR, func, "%d descriptors still set", n_active ) ;
   }
}


/*
 * This function identifies if any of the fd's in the socket mask
 * is bad. We use it in case select(2) returns EBADF
 * When we identify such a bad fd, we remove it from the mask
 * and deactivate the service.
 */
static void find_bad_fd(void)
{
   int fd ;
   struct stat st ;
   unsigned bad_fd_count = 0 ;
   const char *func = "find_bad_fd" ;

   for ( fd = 0 ; fd < ps.ros.max_descriptors ; fd++ )
      if ( FD_ISSET( fd, &ps.rws.socket_mask ) && fstat( fd, &st ) == -1 )
      {
         int found = FALSE ;
         unsigned u ;

         for ( u = 0 ; u < pset_count( SERVICES( ps ) ) ; u++ )
         {
            register struct service *sp ;

            sp = SP( pset_pointer( SERVICES( ps ), u ) ) ;

            if ( ! SVC_IS_AVAILABLE( sp ) )
               continue ;

            if ( SVC_FD( sp ) == fd )
            {
               msg( LOG_ERR, func,
                  "file descriptor of service %s has been closed",
                              SVC_ID( sp ) ) ;
               svc_deactivate( sp ) ;
               found = TRUE ;
               break ;
            }
         }
         if ( ! found )
         {
            FD_CLR( fd, &ps.rws.socket_mask ) ;
            msg( LOG_ERR, func,
               "No active service for file descriptor %d\n", fd ) ;
            bad_fd_count++ ;
         }
      }
   if ( bad_fd_count == 0 )
      msg( LOG_NOTICE, func,
         "select reported EBADF but no bad file descriptors were found" ) ;
}


/*
 * Deactivates all active processes.
 * The real reason for doing this instead of just exiting is
 * to deregister the RPC services
 */
void quit_program(void)
{
   unsigned u ;
   struct service_config *scp = NULL;
   const char *func = "quit_program" ;

   destroy_global_access_list() ;
   
   for ( u = 0 ; u < pset_count( SERVICES( ps ) ) ; u++ ) {
      scp = SVC_CONF( SP(pset_pointer(SERVICES(ps), u)) );
      
      /* This is essentially the same as the following function, 
       * Except we forcibly deactivate them, rather than just 
       * send signals.
       */
      if( SC_IS_INTERNAL( scp ) )
         svc_deactivate( SP( pset_pointer( SERVICES( ps ), u ) ) ) ;
      if( scp->sc_redir_addr != NULL )
         svc_deactivate( SP( pset_pointer( SERVICES( ps ), u ) ) ) ;
      if( SC_IS_RPC( scp ) )
         svc_deactivate( SP( pset_pointer( SERVICES( ps ), u ) ) ) ;
   }

   if ( ps.ros.pid_file ) {
      unlink(ps.ros.pid_file);
   }

   msg( LOG_WARNING, func, "Exiting..." ) ;
   exit( 0 ) ;
}


void terminate_program(void)
{
   unsigned u ;
   struct service_config *scp = NULL;
   void terminate_servers(struct service *);

   for ( u = 0 ; u < pset_count( SERVICES( ps ) ) ; u++ ) {
      scp = SVC_CONF( SP(pset_pointer(SERVICES(ps), u)) );

      /* Terminate the service if it is:
       * 1) internal (if we don't, it'll zombie)
       * 2) a redirector (again, if we don't it'll zombie)
       * 3) It's RPC (we must deregister it.
       */
      if( SC_IS_INTERNAL( scp ) )
         terminate_servers( SP( pset_pointer( SERVICES( ps ), u ) ) ) ;
      if( scp->sc_redir_addr != NULL )
         terminate_servers( SP( pset_pointer( SERVICES( ps ), u ) ) ) ;
      if( SC_IS_RPC( scp ) )
         terminate_servers( SP( pset_pointer( SERVICES( ps ), u ) ) ) ;
   }
   quit_program() ;
}

