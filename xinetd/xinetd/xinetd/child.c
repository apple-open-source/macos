/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <errno.h>
#include <pwd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#if defined (HAVE_GRP_H)
#include <grp.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_SESSIONCREATE
#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>
#include <Security/AuthSession.h>
#endif

#include "str.h"
#include "child.h"
#include "sconf.h"
#include "msg.h"
#include "main.h"
#include "xconfig.h"
#include "ident.h"
#include "sconst.h"
#include "signals.h"
#include "options.h"
#include "redirect.h"

/*
 * This function is running in the new process
 */
void exec_server( const struct server *serp )
{
   const struct service_config *scp = SVC_CONF( SERVER_SERVICE( serp ) ) ;
   struct rlimit rl ;
   int fd ;
   int descriptor = SERVER_FD( serp ) ;
   const char *server = SC_SERVER( scp ) ;
   const char *func = "exec_server" ;

   /*
    * The following code solves a problem with post-version-4.3
    * Ultrix systems (the bug was reported, and a fix was provided by
    * doug@seas.smu.edu; a slightly modified version of this
    * fix is included here).
    *
    * If this is a 'nowait' service, we pass the service descriptor
    * to the server. Note that we have set the close-on-exec flag
    * on all service descriptors. It is unclear whether the dup2()
    * will create a descriptor with the close-on-exec flag set,
    * so we explicitly clear the flag (since we are doing this
    * after the fork, it does not affect the descriptor of the
    * parent process).
    */
   if ( fcntl( descriptor, F_SETFD, 0 ) == -1 )
      msg( LOG_WARNING, func,
         "fcntl( %d, clear close-on-exec ) failed: %m", descriptor ) ;

   if ( debug.on )
      msg( LOG_DEBUG, func, "duping %d", descriptor ) ;

   for ( fd = 0 ; fd <= MAX_PASS_FD ; fd++ )
   {
      if ( dup2( descriptor, fd ) == -1 )
      {
         msg( LOG_ERR, func,
               "dup2( %d, %d ) failed: %m", descriptor, fd ) ;
         _exit( 1 ) ;
      }
   }


#ifdef RLIMIT_NOFILE
   rl.rlim_max = ps.ros.orig_max_descriptors ;
   rl.rlim_cur = ps.ros.max_descriptors ;
   (void) setrlimit( RLIMIT_NOFILE, &rl ) ;
#endif
#ifdef RLIMIT_AS
   if (SC_RLIM_AS (scp))
   {
      rl.rlim_cur = SC_RLIM_AS( scp );
      rl.rlim_max = SC_RLIM_AS( scp );
      (void) setrlimit( RLIMIT_AS, &rl );
   }
#endif
#ifdef RLIMIT_CPU
   if (SC_RLIM_CPU (scp))
   {
      rl.rlim_cur = SC_RLIM_CPU( scp );
      rl.rlim_max = SC_RLIM_CPU( scp );
      (void) setrlimit( RLIMIT_CPU, &rl );
   }
#endif
#ifdef RLIMIT_DATA
   if (SC_RLIM_DATA (scp))
   {
      rl.rlim_cur = SC_RLIM_DATA( scp );
      rl.rlim_max = SC_RLIM_DATA( scp );
      (void) setrlimit( RLIMIT_DATA, &rl );
   }
#endif
#ifdef RLIMIT_RSS
   if (SC_RLIM_RSS (scp))
   {
      rl.rlim_cur = SC_RLIM_RSS( scp );
      rl.rlim_max = SC_RLIM_RSS( scp );
      (void) setrlimit( RLIMIT_RSS, &rl );
   }
#endif
#ifdef RLIMIT_STACK
   if (SC_RLIM_STACK (scp))
   {
      rl.rlim_cur = SC_RLIM_STACK( scp );
      rl.rlim_max = SC_RLIM_STACK( scp );
      (void) setrlimit( RLIMIT_STACK, &rl );
   }
#endif

   (void) Sclose( descriptor ) ;

#ifndef solaris
   no_control_tty() ;
#endif

   msg_suspend() ;

   (void) execve( server, SC_SERVER_ARGV( scp ),
             env_getvars( SC_ENV( scp )->env_handle ) ) ;

   /*
    * The exec failed. Log the error and exit.
    */
   msg_resume() ;
   msg( LOG_ERR, func, "execv( %s ) failed: %m", server ) ;
   _exit( 0 ) ;
}


/*
 * Rename this process by changing the ps.ros.Argv vector
 * Try to put the name of the service in ps.ros.Argv[0], Argv[1]
 * until either the service name is exhausted or we run out
 * of ps.ros.Argv's. 
 * The rest of ps.ros.Argv is cleared to spaces
 */
static void rename_process( const char *name )
{
   const char *from = name ;
   char *to = ps.ros.Argv[ 0 ] ;
   int tmp_index = 1 ;

   while ( *from != NUL )
   {
      if ( *to != NUL )
         *to++ = *from++ ;
      else
         if ( tmp_index < ps.ros.Argc )
            to = ps.ros.Argv[ tmp_index++ ] ;
         else
            break ;
   }
   str_fill( to, ' ' ) ;
   while ( tmp_index < ps.ros.Argc )
      str_fill( ps.ros.Argv[ tmp_index++ ], ' ' ) ;
}


static void set_credentials( const struct service_config *scp )
{
   const char *func = "set_credentials" ;

   if ( SC_SPECIFIED( scp, A_GROUP ) || SC_SPECIFIED( scp, A_USER ) ) {
      if ( ps.ros.is_superuser )
      {
         gid_t gid = SC_GETGID( scp ) ;

         if ( setgid( gid ) == -1 )
         {
            msg( LOG_ERR, func, "setgid failed: %m" ) ;
            _exit( 1 ) ;
         }

#ifndef NO_INITGROUPS
         /*
          * Bug discovered by maf+@osu.edu (a bug fix was also provided;
          * a slightly modified version is included here):
          *      initgroups was not being invoked to set the remaining
          *      groups appropriately
          */
         /* Solar Designer's groups fix */
         if ( SC_SPECIFIED( scp, A_USER ) && SC_SPECIFIED( scp, A_GROUPS ) &&
            scp->sc_groups == YES )
         {
            struct passwd *pwd ;

            /*
             * Invoke getpwuid() to get the user's name.
             *
             * XXX:   we should not need to invoke getpwuid(); we should
             *         remember the user name in the configuration file.
             */
            if ( ( pwd = getpwuid( SC_UID( scp ) ) ) == NULL )
            {
               msg( LOG_ERR, func, "getpwuid( %d ) (service=%s) failed: %m",
                  SC_UID( scp ), SC_ID( scp ) ) ;
               _exit( 1 ) ;
            }
            str_fill( pwd->pw_passwd, ' ' );

            if ( initgroups( pwd->pw_name, pwd->pw_gid ) == -1 )
            {
               msg( LOG_ERR, func, "initgroups( %s, %d ) failed: %m",
                  pwd->pw_name, pwd->pw_gid ) ;
               _exit( 1 ) ;
            }
         }
         else
         {
            if ( setgroups( 0, NULL ) )
            {
               msg( LOG_ERR, func, "setgroups( 0, NULL ) failed: %m" ) ;
               msg( LOG_ERR, func, "Your system may require that 'groups = yes' be defined for this service: %s", scp->sc_name);
               _exit( 1 ) ;
            }
         }
#endif   /* ! NO_INITGROUPS */
      }
   }

   if ( SC_SPECIFIED( scp, A_USER ) ) {
         if ( setuid( SC_UID( scp ) ) == -1 )
         {
            msg( LOG_ERR, func, "setuid failed: %m" ) ;
            _exit( 1 ) ;
         }
      }

   if ( SC_SPECIFIED( scp, A_UMASK ) ) 
      umask(scp->sc_umask);
}



/*
 * This function is invoked in a forked process to run a server. 
 * If the service is internal the appropriate function is invoked
 * otherwise the server program is exec'ed.
 * This function also logs the remote user id if appropriate
 */
void child_process( struct server *serp )
{
   struct service          *sp  = SERVER_SERVICE( serp ) ;
   connection_s            *cp  = SERVER_CONNECTION( serp ) ;
   struct service_config   *scp = SVC_CONF( sp ) ;
   const char              *func = "child_process" ;

   signal_default_state();

   if ((signals_pending[0] >= 0 && Sclose(signals_pending[0])) ||
       (signals_pending[1] >= 0 && Sclose(signals_pending[1])))
   {
      msg(LOG_ERR, func, "Failed to close the signal pipe: %m");
      _exit(1);
   }
   signals_pending[0] = -1;
   signals_pending[1] = -1;

   Sclose(0);
   Sclose(1);
   Sclose(2);


#ifdef DEBUG_SERVER
   if ( debug.on )
   {
      msg( LOG_DEBUG, func, "Process %d is sleeping", getpid() ) ;
      sleep( 10 ) ;
   }
#endif

   if ( ! SC_IS_INTERCEPTED( scp ) )
   {
      set_credentials( scp ) ;
      if ( SC_SPECIFIED( scp, A_NICE ) )
         (void) nice( SC_NICE( scp ) ) ;
   }

   if ( svc_child_access_control(sp, cp) != OK )
      exit(0);

   if ( SERVER_LOGUSER( serp ) )
   {
      unsigned   timeout ;
      idresult_e result ;
      
      /*
       * We use LOGUSER_SUCCESS_TIMEOUT unless the service requires
       * identification, in which case we use an infinite timeout
       */
      timeout = SC_MUST_IDENTIFY( scp ) ? 0 : LOGUSER_SUCCESS_TIMEOUT ;
      result = log_remote_user( serp, timeout ) ;

      if ( result != IDR_OK && SC_MUST_IDENTIFY( scp ) )
      {
         svc_logprint( sp, NOID_ENTRY, "%s %s",
                  conn_addrstr( SERVER_CONNECTION( serp ) ),
                     idresult_explain( result ) ) ;
         _exit( 0 ) ;
      }
   }

#ifdef HAVE_SESSIONCREATE
   if ( scp->sc_sessioncreate == YES ) 
   {
      if ( SessionCreate(0, sessionHasTTY|sessionIsRemote) != noErr )
         svc_logprint( sp, "SessionCreate", "SessionCreate() failed!" );
   }
#endif

   /* this is where the server gets executed  -bbraun */
   if ( ! SC_IS_INTERNAL( scp ) )
   {
      if( scp->sc_redir_addr != NULL )
      {
         redir_handler( serp );
      }
      else
      {
#if defined(HAVE_SETENV)
         char buff[1024];

         strx_sprint(buff, sizeof(buff)-1, "REMOTE_HOST=%s", conn_addrstr(cp));
         if( env_addstr(SC_ENV(scp)->env_handle, buff) != ENV_OK ) {
            msg( LOG_ERR, func, "Error adding REMOTE_HOST variable for %s: %m", SC_NAME(scp) );
            _exit( 1 ) ;
         }
#endif
         exec_server( serp ) ;
      }
   }
   else
   {
      char name[ 180 ] ;
      /*
       * We don't bother to disassociate from the controlling terminal
       *   (we have a controlling terminal only if debug.on is TRUE)
       *
       * Also, for interceptor processes, we give them the name:
       *            <program_name> <service-id> interceptor
       */
      if ( SC_IS_INTERCEPTED( scp ) )
         strx_print( INT_NULL, name, sizeof( name ) - 1,
                           "%s %s interceptor", program_name, SC_ID( scp ) ) ;
      else
      {
         int namelen = sizeof( name ) - 1 ;      /* leave space for the NUL */
         char host[NI_MAXHOST];
         size_t hostlen = NI_MAXHOST;
         socklen_t addrlen = 0;
         union xsockaddr *sinp = CONN_XADDRESS(SERVER_CONNECTION(serp));
         int len;

         if( sinp == NULL )
            exit(0);

         if( SC_IPV6(scp) ) addrlen = sizeof(struct sockaddr_in6);
         else if( SC_IPV4(scp) ) addrlen = sizeof(struct sockaddr_in);

         len = strx_nprint(name, namelen, "(%s service) %s", program_name,
            SC_ID( scp ) ) ;

         if( getnameinfo( SA(sinp), addrlen, host, hostlen, NULL, 0, 0) != 0 )
               strcpy(host, "unknown");

         if ( SC_IPV6(scp) && SC_ACCEPTS_CONNECTIONS( scp ) && 
               !IN6_IS_ADDR_UNSPECIFIED(&sinp->sa_in6.sin6_addr) )
            strx_print( INT_NULL, &name[ len ], namelen - len, " %s" , host ) ;
         if ( SC_IPV4(scp) && SC_ACCEPTS_CONNECTIONS( scp ) )
            strx_print( INT_NULL, &name[ len ], namelen - len, " %s", host ) ;
      }
      rename_process( name ) ;
      SVC_INTERNAL( sp, serp ) ;
   }
   _exit( 0 ) ;
   /* NOTREACHED */
}


/*
 * This function is invoked when a SIGCLD is received
 */
void child_exit(void)
{
   const char *func = "child_exit" ;

   for ( ;; )         /* Find all children that exited */
   {
      int status ;
      pid_t pid ;
      struct server *serp ;
      
#ifdef HAVE_WAITPID
      pid = waitpid( -1, &status, WNOHANG ) ;
#else
#if defined( sun ) && defined( lint )
      pid = wait3( (union wait *)&status, WNOHANG, RUSAGE_NULL ) ;
#else
      pid = wait3( &status, WNOHANG, RUSAGE_NULL ) ;
#endif
#endif

      if ( debug.on )
#ifdef HAVE_WAITPID
         msg( LOG_DEBUG, func, "waitpid returned = %d", pid ) ;
#else
         msg( LOG_DEBUG, func, "wait3 returned = %d", pid ) ;
#endif
      
      if ( pid == -1 ) {
         if ( errno == EINTR )
            continue ;
         else
            break ;
      }

      if ( pid == 0 )
         break ;
      
      if ( ( serp = server_lookup( pid ) ) != NULL )
      {
         serp->svr_exit_status = status ;
         server_end( serp ) ;
      }
      else
         msg( LOG_NOTICE, func, "unknown child process %d %s", pid,
            PROC_STOPPED( status ) ? "stopped" : "died" ) ;
   }
}

