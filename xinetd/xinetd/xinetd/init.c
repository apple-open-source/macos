/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include "config.h"
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include "sio.h"
#include "init.h"
#include "defs.h"
#include "msg.h"
#include "signals.h"
#include "env.h"
#include "confparse.h"
#include "options.h"
#include "main.h"
#include "xconfig.h"
#include "special.h"
#include "retry.h"
#include "internals.h"
#include "libportable.h"

struct module
{
   const char *name ;
   status_e (*initializer)() ;
} ;



static const struct module program_modules[] = 
   {
      { "signal",                       signal_init       },
      { "environment",                  initenv           },
      { CHAR_NULL,                      NULL              }
   } ;


static bool_int have_stderr ;

#define STDERR_FD                  2

static void set_fd_limit(void);

/*
 * This function is invoked when a system call fails during initialization.
 * A message is printed to stderr, and the program is terminated
 */
#ifdef __GNUC__
__attribute__ ((noreturn))
#endif
static void syscall_failed( const char *call )
{
   char *err ;

   if ( have_stderr )
   {
      err = strerror(errno);
      Sprint( STDERR_FD, "%s: %s failed: %s\n", program_name, call, err ) ;
   }
   exit( 1 ) ;
}



/*
 * Close all descriptors except STDERR_FD. We need this to report
 * errors and the process pid of the daemon.
 * Open all descriptors in the range 0..MAX_PASS_FD (except STDERR_FD)
 * to /dev/null.
 * STDERR_FD should not be 0.
 *
 * msg() cannot be used from this function, as it has not been initialized yet.
 */
static void setup_file_descriptors(void)
{
   int   fd ;
   int   new_fd ;
   int   null_fd ;

   if ( Smorefds(3) == SIO_ERR )
   {
      syscall_failed("Smorefds");
      exit( 1 ) ;
   }

   set_fd_limit() ;

   /*
    * Close all unneeded descriptors
    */
   for ( fd = STDERR_FD + 1 ; fd < ps.ros.max_descriptors ; fd++ )
      if ( Sclose( fd ) && errno != EBADF )
      {
         syscall_failed("Sclose");
         exit( 1 ) ;
      }
   
   /*
    * Check if the STDERR_FD descriptor is open.
    */
   new_fd = dup( STDERR_FD ) ;
   if ( new_fd != -1 )
   {
      have_stderr = TRUE ;
      (void) Sclose( new_fd ) ;
   }

   if ( ( null_fd = open( "/dev/null", O_RDONLY ) ) == -1 )
      syscall_failed( "open of '/dev/null'" ) ;

   for ( fd = 0 ; fd <= MAX_PASS_FD ; fd++ )
   {
      if ( have_stderr && fd == STDERR_FD )
         continue ;
      if ( fd != null_fd && dup2( null_fd, fd ) == -1 )
         syscall_failed( "dup2" ) ;
   }

   if ( null_fd > MAX_PASS_FD )
      (void) Sclose( null_fd ) ;
}


/* msg() cannot be used in this function, as it has not been initialized yet. */
static void set_fd_limit(void)
{
#ifdef RLIMIT_NOFILE
   struct rlimit rl ;
   rlim_t maxfd ;
    
   /*
    * Set the soft file descriptor limit to the hard limit.
    */
   if ( getrlimit( RLIMIT_NOFILE, &rl ) == -1 )
   {
      syscall_failed("getrlimit(RLIMIT_NOFILE)");
      exit( 1 ) ;
   }

   maxfd = rl.rlim_max;
   if ( rl.rlim_max == RLIM_INFINITY ) 
      rl.rlim_max = FD_SETSIZE;

   /* XXX: a dumb way to prevent fd_set overflow possibilities; the rest
    * of xinetd should be changed to use an OpenBSD inetd-like fd_grow(). */
   if ( rl.rlim_max > FD_SETSIZE )
      rl.rlim_max = FD_SETSIZE;
     
   rl.rlim_cur = rl.rlim_max ;
   if ( setrlimit( RLIMIT_NOFILE, &rl ) == -1 )
   {
      syscall_failed("setrlimit(RLIMIT_NOFILE)");
      ps.ros.max_descriptors = FD_SETSIZE;
      ps.ros.orig_max_descriptors = FD_SETSIZE;
      return ;
   }

   ps.ros.orig_max_descriptors = maxfd ;
   ps.ros.max_descriptors = rl.rlim_max ;
#else      /* ! RLIMIT_NOFILE */
   ps.ros.max_descriptors = getdtablesize() ;
#endif   /* RLIMIT_NOFILE */
}


static void init_common( int argc, char *argv[] )
{
   const struct module *mp = NULL;
   const char *func = "init_common" ;

   /*
    * Initialize the program state
    */

   ps.ros.Argv = argv ;
   ps.ros.Argc = argc ;
   ps.ros.is_superuser = ( geteuid() == 0 ) ;

   /*
    * Initialize the program modules
    */
   for ( mp = program_modules ; mp->name ; mp++ )
      if ( (*mp->initializer)() == FAILED )
      {
         msg( LOG_CRIT, func,
            "Initialization of %s facility failed. Exiting...", mp->name ) ;
         exit( 1 ) ;
      }
   (void) umask( umask( 077 ) | 022 ) ;
}

/* Create the pidfile. 
 * This is called after msg_init(), and potentially after 
 * we've become_daemon() (depending on if we're in debug or not-forking)
 */
static void create_pidfile(void)
{
   int    pidfd;
   FILE  *pidfile;

   if ( ps.ros.pid_file ) {
      unlink(ps.ros.pid_file);
      pidfd = open(ps.ros.pid_file, O_EXCL|O_CREAT|O_WRONLY,
         S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
      if (pidfd >= 0) { /* successfully created file */
         pidfile = fdopen(pidfd, "w");
         if (pidfile) {
            fchmod(pidfd, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
            fprintf(pidfile, "%d\n", getpid());
            fclose(pidfile);
         } else {
            msg(LOG_DEBUG, "create_pidfile", "fdopen failed: %m");
            Sclose(pidfd);
         }
      } else
         msg(LOG_DEBUG, "create_pidfile", "open failed: %m");
   }
}

/*
 * Become a daemon by forking a new process. The parent process exits.
 */
static void become_daemon(void)
{
   int   tries ;
   int   pid ;
   const char  *func = "become_daemon" ;

   /*
    * First fork so that the parent will think we have exited
    */
   for ( tries = 0 ;; tries++ )
   {
      if ( tries == 5 )
      {
         msg( LOG_CRIT, func, "fork: %m. Exiting..." ) ;
         exit( 0 ) ;
      }

      pid = fork() ;

      if ( pid == -1 )
      {
         sleep( 1 ) ;   /* wait for a second */
         continue ;      /* and then retry      */
      }
      else if ( pid == 0 )
         break ;
      else
         exit( 0 ) ;
   }

   (void) dup2( 0, STDERR_FD ) ;
   no_control_tty() ;

#ifdef DEBUG_DAEMON
   sleep( 20 ) ;       /* XXX: timers will probably not work after this */
#endif
}


static pset_h new_table( unsigned size )
{
   const char *func = "new_table" ;
   pset_h tab = pset_create( size, 0 ) ;

   if ( tab == NULL )
   {
      msg( LOG_CRIT, func, "Failed to create table" ) ;
      exit( 1 ) ;
   }
   return( tab ) ;
}


/*
 * Create tables
 */
static void init_rw_state( void )
{
   SERVERS( ps ) = new_table( 0 ) ;
   RETRIES( ps ) = new_table( 0 ) ;
   SERVICES( ps ) = new_table( 0 ) ;

   ps.rws.descriptors_free = ps.ros.max_descriptors - DESCRIPTORS_RESERVED ;

   FD_ZERO( &ps.rws.socket_mask ) ;
   ps.rws.mask_max = 0 ;

}


/*
 * Perform all necessary initializations
 */
void init_daemon( int argc, char *argv[] )
{
   const char *fail = NULL;

   debug.on = 0;
   memset(&ps, 0, sizeof(ps));

   setup_file_descriptors() ;
   ps.ros.config_file = DEFAULT_CONFIG_FILE ;
   (void) opt_recognize( argc, argv ) ;

   /*
    * XXX: we only use xlog_parms on XLOG_SYSLOG-type logs but in general
    *        we should do it for all types of xlog's we may use. We can get
    *        away with this now, because xlog_parms for XLOG_FILELOG is a noop.
    */
   (void) xlog_parms( XLOG_SYSLOG,
               program_name, LOG_PID + LOG_NOWAIT, LOG_DAEMON ) ;

   /*
    * Initialize the message facility; after this everything can use the
    * msg() interface
    */
   if ( (fail = msg_init()) )
   {
      if ( have_stderr )
         Sprint( STDERR_FD, "%s: msg_init failed: %s\n", program_name, fail ) ;
      exit( 1 ) ;
   }

   init_common( argc, argv ) ;

   if ( ! debug.on && !dont_fork )
      become_daemon() ;
   create_pidfile();
   
   init_rw_state() ;
}


/*
 * Initialize all services
 *
 * This function is either successful in starting some services 
 * or it terminates the program.
 */
void init_services( void )
{
   struct configuration conf ;
   const char *func = "init_services" ;

   if ( cnf_get( &conf ) == FAILED )
   {
      msg( LOG_CRIT, func, "couldn't get configuration. Exiting..." ) ;
      exit( 1 ) ;
   }

   DEFAULTS( ps ) = CNF_DEFAULTS( &conf ) ;
   (void) cnf_start_services( &conf ) ;
   CNF_DEFAULTS( &conf ) = NULL ;      /* to avoid the free by cnf_free */
   cnf_free( &conf ) ;

   /*
    * The number of available/active services is kept by the service functions
    */
   if ( stayalive_option == 0 ) {
      if ( ps.rws.available_services == 0 )
      {
         msg( LOG_CRIT, func, "no services. Exiting..." ) ;
         if ( ps.ros.pid_file ) {
            unlink(ps.ros.pid_file);
         }
         exit( 1 ) ;
      }
   }

   spec_include() ;      /* include special services */
}

