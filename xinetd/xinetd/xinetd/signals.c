/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#if defined(HAVE_STRSIGNAL)
#define _GNU_SOURCE
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

#include "str.h"
#include "signals.h"
#include "xconfig.h"
#include "msg.h"
#include "main.h"
#include "xtimer.h"
#include "child.h"
#include "retry.h"
#include "reconfig.h"
#include "internals.h"

#ifdef NO_POSIX_TYPES
/*
 * XXX:   here we assume that in the case that NO_POSIX_TYPES is not defined
 *   (i.e. the system has posix types) the sigset_t is also typedef'd
 *   to 'int'. Our goal is to work with systems that have defined
 *   sigset_t but do not yet support the posix signal interface.
 */
typedef int sigset_t ;

struct sigaction
{
   void         (*sa_handler)(int) ;
   sigset_t     sa_mask ;
   int          sa_flags ;
} ;
#endif   /* NO_POSIX_TYPES */

static void my_handler( int sig );
static void general_handler( int sig );
#ifndef NO_POSIX_SIGS
/* We only include this function if the system really supports posix
 * signals so we can get some debug information */
static void mem_fault_handler( int sig, siginfo_t * siginfo, void *context );
#endif
typedef void sigfunc( int );

#define SIGSET_NULL            ((sigset_t *)0)
#define SIGVEC_NULL            ((struct sigvec *)0)
#define SIGACTION_NULL         ((struct sigaction *)0)


#ifdef NO_POSIX_SIGS
#ifdef NO_SIGVEC
#define sigmask( sig )                  ( 1 << ( (sig) -1 ) )
typedef int (*sighandler_type)() ;
#define sigpause( x )
#define sigsetmask( x )
#endif   /* NO_SIGVEC */

#define sigsuspend( set )               sigpause( *set )
#define sigemptyset( set )              (*set) = 0
#define sigaddset( set, sig )           ( ( (*set) |= sigmask( sig ) ), 0 )
#define sigismember( set, sig )         ( ( (*set) & sigmask( sig ) ) != 0 )

/*
 * Only works for SIG_SETMASK and SIG_UNBLOCK. Also oset must be NULL.
 */
int sigprocmask( int how, sigset_t *set, sigset_t *oset )
{
   if ( how == SIG_BLOCK || oset != NULL )
   {
      msg( LOG_ERR, "sigprocmask",
                           "Bad args: how = %d, oset = %p", how, oset ) ;
      return( -1 ) ;
   }

   if ( how == SIG_SETMASK )
   {
      (void) sigsetmask( *set ) ;
      return( 0 ) ;
   }

   if ( how == SIG_UNBLOCK )
   {
      int current_mask = sigblock( 0 ) ;

      (void) sigsetmask( current_mask & ~*set ) ;
      return( 0 ) ;
   }
   /* NOTREACHED */
}


/*
 * NOTE: This is not a complete imitation of sigaction; in particular it
 *   expects that sap is never NULL and that osap is always NULL.
 */
int sigaction( int sig, struct sigaction *sap, struct sigaction *osap )
{
   if ( sap == NULL || osap != NULL )
   {
      msg( LOG_ERR, "sigaction", "Bad args: sap = %p, osap = %p", sap, osap ) ;
      return( -1 ) ;
   }

#ifndef NO_SIGVEC
   {
      struct sigvec sv ;

      sv.sv_handler = sap->sa_handler ;
      sv.sv_mask = sap->sa_mask ;
      sv.sv_flags = sap->sa_flags ;

      return( sigvec( sig, &sv, SIGVEC_NULL ) ) ;
   }
#else      /* NO_SIGVEC */
   {
      sighandler_type new_handler ;

      new_handler = sa.sa_handler ;
      return( signal( sig, new_handler ) ? 0 : -1 ) ;
   }
#endif   /* ! NO_SIGVEC */
}

#endif   /* NO_POSIX_SIGS */


/*
 * reset_sigs is the list of signals that we need to reset to SIG_DFL.
 * Currently, these are the signals whose actions we set to SIG_IGN.
 * In general, we should also include any signals that have a handler
 * that does anything other than setting a flag. We need to do this
 * in case such a signal occurs while a forked process is providing
 * an internal service.
 */
static sigset_t reset_sigs ;

/*
 * nsig is equal to the greatest signal number supported plus 1
 */
static int nsig ;


/*
 * When this function returns FAILED, we check the errno to determine
 * if it failed because the signal number specified was invalid.
 * This allows us to determine the number of supported signals.
 */
static status_e handle_signal( int sig )
{
   struct sigaction      sa ;
   sigfunc              *sig_handler ;

   sa.sa_flags = 0 ;

   switch ( sig )
   {
      case RECONFIG_HARD_SIG:
      case OLD_RECONFIG_HARD_SIG:
      case TERMINATION_SIG:
      case STATE_DUMP_SIG:
      case CONSISTENCY_CHECK_SIG:
      case SERVER_EXIT_SIG:
      case QUIT_SIG:
         sig_handler = my_handler ;
         break ;

      case SIGTTIN:
      case SIGTTOU:
      case SIGTSTP:
         if ( debug.on )
            return( OK ) ;
         /* FALL THROUGH */
          
      /*
       * We may receive a SIGPIPE when handling an internal stream 
       * service and the other end closes the connection.
       * We only care about internal services that don't require forking.
       */
      case SIGPIPE:
         sig_handler = SIG_IGN ;
         sigaddset( &reset_sigs, sig ) ;
         break ;

      case SIGKILL:
      case SIGSTOP:
         return( OK ) ;         /* we can't catch these two */
      
      /*
       * If the following two cases are included, SIGSEGV and SIGBUS will
       * cause core dumps. We want that to happen when we are debugging
       * xinetd (i.e. DEBUG is defined) and we are not debugging the
       * signal recovery code (i.e. DEBUG_SIGNALS is not defined).
       */
      case SIGSEGV:
      case SIGBUS:
#if defined( DEBUG ) && !defined( DEBUG_SIGNALS )
         return( OK ) ;
#else
#ifdef NO_POSIX_SIGS
         sig_handler = general_handler ;
#else
         sa.sa_sigaction= mem_fault_handler ;
         sig_handler = NULL ;
         sa.sa_flags = SA_SIGINFO ;
#endif
         break;
#endif
      case SIGTRAP:
         if ( debug.on )
            return( OK ) ;
      
      default:
         sig_handler = general_handler ;
   }

   sigemptyset( &sa.sa_mask ) ;
   if ( sig_handler )
      sa.sa_handler = sig_handler ;
   return( ( sigaction( sig, &sa, SIGACTION_NULL ) == -1 ) ? FAILED : OK ) ;
}


/*
 * Install signal handlers for all signals that can be caught.
 * This implies that no core dumps are generated by default.
 */
status_e signal_init(void)
{
   int      sig ;
   const char    *func = "install_signal_handlers" ;

   sigemptyset( &reset_sigs ) ;

   if ( pipe(signals_pending) ||
        fcntl(signals_pending[0], F_SETFD, FD_CLOEXEC) ||
        fcntl(signals_pending[1], F_SETFD, FD_CLOEXEC) ) {
      msg( LOG_CRIT, func, "Failed to create signal pipe: %m" );
      return( FAILED );
   }

   for ( sig = 1 ;; sig++ )
      if ( handle_signal( sig ) == FAILED ) {
         if ( errno == EINVAL )
         {
            nsig = sig ;
            break ;
         }
         else
         {
            msg( LOG_CRIT, func,
               "Failed to install signal handler for signal %s: %m",
                  sig_name( sig ) ) ;
            return( FAILED ) ;
         }
      }
   return( OK ) ;
}

#define MAX_SIGNAL_COUNT               50
#define MAX_INTERVAL_SIGNAL_COUNT      10
#define SIGNAL_INTERVAL                 1      /* second */
/*
 * This function handles SIGSEGV and SIGBUS.
 * Emergency action is taken if a certain number (MAX_SIGNAL_COUNT) of 
 * these signals is received over the lifetime of the program OR 
 * if a certain number (MAX_INTERVAL_SIGNAL_COUNT) of these signals 
 * is received within a certain time interval (SIGNAL_INTERVAL).
 *
 * The action depends on the type of the emergency:
 *      Case 1: MAX_INTERVAL_SIGNAL_COUNT is exceeded
 *         If a setjmp environment is available, do a longjmp, otherwise exit
 *      Case 2: MAX_SIGNAL_COUNT is exceeded
 *         Exit
 *
 * NOTE: We try to send a message to the log only once to avoid
 *         looping in this function (in case there is a bug in msg())
 */
static void bad_signal(void)
{
   static time_t   interval_start ;
   static volatile int interval_signal_count ;
   static volatile int total_signal_count ;
   time_t          current_time ;
   const char     *func = "bad_signal" ;

   total_signal_count++ ;
   if ( total_signal_count == MAX_SIGNAL_COUNT )
   {
      msg( LOG_CRIT, func,
            "Received %d bad signals. Exiting...", total_signal_count ) ;
      exit( 1 ) ;
   }
   else if ( total_signal_count > MAX_SIGNAL_COUNT )
      _exit( 1 ) ;      /* in case of a problem in exit(3) */
   
   (void) time( &current_time ) ;

   if ( interval_signal_count > 0 &&
            current_time - interval_start <= SIGNAL_INTERVAL )
   {
      interval_signal_count++ ;
      if ( interval_signal_count == MAX_INTERVAL_SIGNAL_COUNT )
      {
         if ( ps.rws.env_is_valid )
         {
            interval_start = current_time ;
            interval_signal_count = 1 ;
            msg( LOG_ERR, func, "Resetting..." ) ;
            siglongjmp( ps.rws.env, 1 ) ;
            /* NOTREACHED */
         }
         msg( LOG_CRIT, func,
            "Received %d signals in %d seconds. Exiting...",
               interval_signal_count, SIGNAL_INTERVAL ) ;
         exit( 1 ) ;
      }
      else if ( interval_signal_count > MAX_INTERVAL_SIGNAL_COUNT )
         _exit( 1 ) ;         /* shouldn't happen */
   }
   else
   {
      interval_start = current_time ;
      interval_signal_count = 1 ;
   }
}
#ifndef NO_POSIX_SIGS
/* If a SEGV or BUS fault occur, output some basic debug information */
static void mem_fault_handler( int sig, siginfo_t * siginfo, void *context )
{
   if (sig == SIGSEGV || sig == SIGBUS)
   {
      const char *func = "mem_fault_handler";
      msg(LOG_CRIT, func, "Address of fault: %p", siginfo->si_addr);
      if (siginfo->si_errno)
         msg(LOG_CRIT, func, "si_errno: %s", strerror(siginfo->si_errno));

      switch (sig) {
         case SIGSEGV:
            switch (siginfo->si_code) {
#ifdef SEGV_MAPERR
               case SEGV_MAPERR:
                  msg(LOG_CRIT, func, "address is not mapped for object");
                  break;
#endif
#ifdef SEGV_ACCERR
               case SEGV_ACCERR:
                  msg(LOG_CRIT, func, 
			     "invalid permissions for mapped object");
                  break;
#endif
               default:
                  msg(LOG_CRIT, func, "unknown fault code %d",siginfo->si_code);
            }
            break;

         case SIGBUS:
            switch(siginfo->si_code) {
#ifdef BUS_ADRALN
               case BUS_ADRALN:
                  msg(LOG_CRIT, func, "invalid address alignment");
                  break;
#endif
#ifdef BUS_ADRERR
               case BUS_ADRERR:
                  msg(LOG_CRIT, func, "nonexistent physical address");
                  break;
#endif
#ifdef BUS_OBJERR
               case BUS_OBJERR:
                  msg(LOG_CRIT, func, "object-specific hardware error");
                  break;
#endif
               default:
                  msg(LOG_CRIT, func, "unknown fault code %d",siginfo->si_code);
            }
            break;
      }
   }

   /* Now call old function for handling */
   general_handler( sig );
}
#endif /* NO_POSIX_SIGS */

char *sig_name( int sig )
{
   static char signame_buf[ 30 ] ;

#if defined(HAVE_STRSIGNAL)
   /* Use strsignal and remove the old sys_siglist stuff */
   if ( sig < NSIG )
      return( strx_sprint( signame_buf, sizeof( signame_buf ) - 1,
               "%d (%s)", sig, strsignal(sig) ) ) ;
#else
#if defined(HAVE_SYS_SIGLIST)
   if ( sig < NSIG )
      return( strx_sprint( signame_buf, sizeof( signame_buf ) - 1,
               "%d (%s)", sig, sys_siglist[sig] ) ) ;
#endif
#endif
   return( strx_sprint( signame_buf, sizeof( signame_buf ) - 1, "%d", sig ) ) ;
}


/*
 * For SIGSEGV and SIGBUS we invoke the bad_signal() function
 *
 * For other signals, we just log the fact that they occured.
 * SIGINT is a special case since in debug.on mode, it will 
 * cause termination.
 */

static void general_handler( int sig )
{
   sigset_t badsigs ;
   const char *func = "general_handler" ;

   /*
    * Do this here to catch problems like SIGSEGV in msg()
    */
   sigemptyset( &badsigs ) ;
   sigaddset( &badsigs, sig ) ;
   (void) sigprocmask( SIG_UNBLOCK, &badsigs, SIGSET_NULL ) ;

   switch ( sig )
   {
      case SIGBUS:
      case SIGSEGV:
         msg( LOG_CRIT, func, "(%d) Unexpected signal: %s", 
                        getpid(), sig_name( sig ) ) ;
         if ( debug.on )
	 {
	    /* Generate a core dump */
	    signal(SIGABRT, SIG_DFL);
	    abort();
	 }
	 else
            bad_signal() ;
         break ;
      
      default:
         msg( LOG_NOTICE, func, "Unexpected signal %s", sig_name( sig ) ) ;
         if ( debug.on && sig == SIGINT )
            exit( 1 ) ;
   }
}


/*
 * The job of this function is to write the signal received to the
 * pipe of pending signals, which is in the main select loop.
 */
static void my_handler( int sig )
{
   int ret_val;
   int saved_errno = errno;
#if NSIG < 256
   unsigned char sig_byte;
#ifdef __APPLE__
   if (sig == SIGTERM)
      _exit(0);
#endif
   if (signals_pending[1] < 0) return;
   if (sig >= 256) return;
   sig_byte = sig;
   do
   {
      ret_val = write(signals_pending[1], &sig_byte, 1);
   } while (ret_val == -1 && errno == EINTR);
#else
   if (signals_pending[1] < 0) return;
   do
   {
      ret_val = write(signals_pending[1], &sig, sizeof(int));
   } while (ret_val == -1 && errno == EINTR);
#endif
   errno = saved_errno;
}


/*
 * Reset all signals to default action. Reset the signal mask
 *
 * This function is invoked from a forked process. That is why we
 * invoke _exit instead of exit (to avoid the possible stdio buffer flushes)
 */
void signal_default_state(void)
{
   int sig ;
   sigset_t empty ;

   for ( sig = 1 ; sig < nsig ; sig++ )
      if ( sigismember( &reset_sigs, sig ) == 1 )
         if ( signal( sig, SIG_DFL ) == SIG_ERR )
         {
            msg( LOG_ERR, "reset_signals",
               "signal failed for signal %s: %m", sig_name( sig ) ) ;
            if ( debug.on )
               _exit( 1 ) ;
         }
      
   sigemptyset( &empty ) ;
   (void) sigprocmask( SIG_SETMASK, &empty, SIGSET_NULL ) ;
}

void check_pipe(void)
{
   int i;
#if NSIG < 256
   unsigned char sig;
#else
   int sig;
#endif
   const char *func = "check_pipe";

   if (signals_pending[0] < 0) return;

   if( ioctl(signals_pending[0], FIONREAD, &i) != 0 ) {
      msg(LOG_ERR, func, "Can't get the number of pending signals: %m");
      return;
   }
#if NSIG >= 256
   i /= sizeof(int);
#endif

   while( --i >= 0 ) {
      int ret_val;
      do
      {
         ret_val = read(signals_pending[0], &sig, sizeof(sig));
      } while (ret_val == -1 && errno == EINTR);
      if (ret_val != sizeof(sig) ) {
         msg(LOG_ERR, func, "Error retrieving pending signal: %m");
         return;
      }

      if( debug.on ) {
         msg(LOG_DEBUG, func, "Got signal %s", sig_name(sig));
      }

      switch(sig) {
         case SERVER_EXIT_SIG:       child_exit();           break;
         case RECONFIG_HARD_SIG:     hard_reconfig();        break;
         case OLD_RECONFIG_HARD_SIG: hard_reconfig();        break;
         case TERMINATION_SIG:       terminate_program();    break;
         case STATE_DUMP_SIG:        dump_internal_state();  break;
         case CONSISTENCY_CHECK_SIG: user_requested_check(); break;
         case QUIT_SIG:              quit_program();         break;
         default:
            msg(LOG_ERR, func, "unexpected signal: %s in signal pipe", 
               sig_name(sig));
      }
   }
}
