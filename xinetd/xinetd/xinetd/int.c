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
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "int.h"
#include "msg.h"
#include "log.h"
#include "tcpint.h"
#include "udpint.h"
#include "sconf.h"
#include "intcommon.h"
#include "child.h"
#include "state.h"
#include "main.h"
#include "signals.h"
#include "xconfig.h"
#include <netdb.h>

static void start_server( struct intercept_s *ip );
static void terminate_server( struct intercept_s *ip );

typedef struct intercept_s *(*initfunc)() ;

struct lookup_table
{
   initfunc   initializer ;
   int        socket_type ;
} ;


static struct lookup_table intercept_lookup_table[] =
   {
      { di_init,         SOCK_DGRAM  },
      { si_init,         SOCK_STREAM },
      { NULL,            0           }
   } ;

         
/*
 * This variable has file scope for the benefit of the signal handler
 */
static struct intercept_s *intp = NULL;



static initfunc find_initializer( int type )
{
   struct lookup_table *ltp ;

   for ( ltp = intercept_lookup_table ; ltp->initializer ; ltp++ )
      if ( ltp->socket_type == type )
         return( ltp->initializer ) ;
   msg( LOG_ERR, "find_initializer", "No initializer for type %d", type ) ;
   _exit( 0 ) ;
   /* NOTREACHED */
   return (initfunc)0;
}


/*
 * This function is the interface of the intercept code with the rest of 
 * the program. 
 */
void intercept( struct server *serp )
{
   struct service *sp = SERVER_SERVICE( serp ) ;
   initfunc initializer ;

#ifdef DEBUG_INTERCEPTOR
   if ( debug.on )
   {
      msg( LOG_DEBUG, "intercept", "%d is sleeping", getpid() ) ;
      sleep( 10 ) ;
   }
#endif

   initializer = find_initializer( SVC_SOCKET_TYPE( sp ) ) ;
   intp = (*initializer)( serp ) ;
   start_server( intp ) ;
   (*intp->int_ops->mux)() ;
   terminate_server( intp ) ;
   /*
    * the terminate_server function should not return but even if it
    * does, child_process will do the _exit.
    */ 
}


/*
 * Create a socket and bind it to (INADDR_LOOPBACK,0)
 */
static int get_server_socket( struct intercept_s *ip )
{
   struct service *sp = SERVER_SERVICE( INT_SERVER( ip ) ) ;
   union xsockaddr *sinp = INT_LOCALADDR( ip ) ;
   int sd ;
   unsigned int size ;
   const char *func = "get_server_socket" ;

   if( SC_IPV6(SVC_CONF(sp)) ) {
      struct addrinfo hint, *res = NULL;
      memset(&hint, 0, sizeof(struct addrinfo));
      hint.ai_family = AF_INET6;
      hint.ai_flags = AI_NUMERICHOST;
      sinp->sa_in6.sin6_family = AF_INET6;
      sinp->sa_in6.sin6_port = 0;
      if( getaddrinfo("::1", NULL, &hint, &res) != 0  )
         int_fail( ip, "can't find ::1" );
      if( res == NULL )
         int_fail( ip, "no results for ::1" );
      if( res->ai_family != AF_INET6 )
         int_fail( ip, "non IPv6 result for ::1" );
      memcpy(sinp, res->ai_addr, sizeof( struct sockaddr_in6 ));
      freeaddrinfo(res);
      size = sizeof(struct sockaddr_in6);
   } else if( SC_IPV4(SVC_CONF(sp)) ) {
      sinp->sa_in.sin_family = AF_INET;
      sinp->sa_in.sin_port = 0;
      sinp->sa_in.sin_addr.s_addr = inet_addr( "127.0.0.1" );
      size = sizeof(struct sockaddr_in);
   } else
      int_fail( ip, "unknown socket family" );

   if ( ( sd = socket( sinp->sa.sa_family, SVC_SOCKET_TYPE( sp ), SC_PROTOVAL(SVC_CONF(sp)) ) ) == -1 )
      int_fail( ip, "socket creation" ) ;

   if ( bind( sd, SA( sinp ), size ) == -1 )
      int_fail( ip, "bind" ) ;
   
   size = sizeof( *sinp ) ;
   if ( getsockname( sd, SA( sinp ), &size ) == -1 )
      int_fail( ip, "getsockname" ) ;
   
   if ( debug.on )
      msg( LOG_DEBUG, func, "address = %s, port = %d",
         xaddrname( sinp ), ntohs( xaddrport( sinp ) ) ) ;
      
   if ( ip->int_socket_type == SOCK_STREAM )
      (void) listen( sd, LISTEN_BACKLOG ) ;
   
   return( sd ) ;
}


static void start_server( struct intercept_s *ip )
{
   struct server      *serp = INT_SERVER( ip ) ;
   struct service     *sp = SERVER_SERVICE( serp ) ;
   int                server_socket ;
   pid_t              pid ;

   server_socket = get_server_socket( ip ) ;
   
   pid = fork() ;

   switch ( pid )
   {
      case -1:
         int_fail( ip, "fork" ) ;
         /* NOTREACHED */
      
      case 0:
         CONN_SET_DESCRIPTOR( SERVER_CONNECTION( serp ), server_socket ) ;
         SVC_MAKE_EXTERNAL( sp ) ;            /* avoid looping */
         child_process( serp ) ;
         /* NOTREACHED */
      
      default:
         SERVER_SET_PID( serp, pid ) ;
         (void) Sclose( server_socket ) ;
   }
}



/*
 * Return value:
 *         OK          if the server died
 *         FAILED       otherwise
 */
static status_e wait_child( struct intercept_s *ip )
{
   const char *func = "wait_child" ;
   int status ;
   status_e ret = FAILED;
   pid_t pid ;

   while( (pid = waitpid( -1, &status, WNOHANG )) != 0 )
   {

      if ( pid == -1 )
      {
         if ( errno != EINTR )
         {
            msg( LOG_ERR, func, "wait: %m" ) ;
            return( ret ) ;
         }
      }
      else if ( pid == SERVER_PID( INT_SERVER( ip ) ) )
      {
         if ( PROC_STOPPED( status ) )
            ret = FAILED;
         SERVER_SET_EXIT_STATUS( INT_SERVER( ip ), status ) ;
         ret = OK;
      }
      else
      {
         unsigned u;

         /* Ideally, this will never be executed */
         msg( LOG_ERR, func,
            "wait returned pid of unknown process: %d", pid ) ;

         /* Since we don't have the intercept pointer to this service,
          * do our best to shut it down safely...
          */
         for( u = 0; u < pset_count( SERVERS(ps) ); u++ ) {
            struct server *p = SERP( pset_pointer( SERVERS(ps), u) );

            if( (p != NULL) && (SERVER_PID(p) == pid) ) {
               struct service *sp = SERVER_SERVICE(p);
               struct service_config *scp = SVC_CONF(sp);

               if( SC_PROTOVAL(scp) == IPPROTO_TCP ) {
                  SERVER_SET_EXIT_STATUS( p, status );
                  si_exit();
               } else if( SC_PROTOVAL(scp) == IPPROTO_UDP ) {
                  SERVER_SET_EXIT_STATUS( p, status );
                  di_exit();
               } else {
                  msg( LOG_ERR, func, "Don't know how to exit %d", pid);
               }
               break;
            }
         }
      }
   }

   return ret;
}


static void terminate_server( struct intercept_s *ip )
{
   pid_t pid = SERVER_PID( INT_SERVER( intp ) ) ;

   if ( pid > 0 )
      (void) kill( pid, SIGKILL ) ;

   /*
    * Normally, wait_child should never return since a SIGCHLD will 
    * invoke the signal handler which will then call the exit function.
    */
   if ( wait_child( ip ) == OK )
      (*intp->int_ops->exit)() ;
}


void int_sighandler( int sig )
{
   const char *func = "int_sighandler" ;

   if ( debug.on )
      msg( LOG_DEBUG, func, "Received signal %s", sig_name( sig ) ) ;

   if ( sig == SERVER_EXIT_SIG )
   {
      if ( wait_child( intp ) == OK )
         (*intp->int_ops->exit)() ;
   }
   else if ( sig == INTERCEPT_SIG )
      INTERCEPT( intp ) = FALSE ;
   else if ( sig == SIGTERM )
      terminate_server( intp ) ;
}
