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
#include <stdlib.h>
#include <unistd.h>

#include <netinet/tcp.h>

#include "sio.h"
#include "connection.h"
#include "sconf.h"
#include "msg.h"
#include "main.h"
#include "state.h"
#include "special.h"

#define NEW_CONN()            NEW( connection_s )
#define FREE_CONN( cop )      FREE( cop )

/*
 * Get a new connection request and initialize 'cp' appropriately
 */
static status_e get_connection( struct service *sp, connection_s *cp )
{
   struct service_config *scp = SVC_CONF( sp );
   int sin_len;
   const char *func = "get_connection" ;
   int on = 1;

   if( SC_IPV4(scp) ) sin_len = sizeof(struct sockaddr_in);
   if( SC_IPV6(scp) ) sin_len = sizeof(struct sockaddr_in6);

   if ( SVC_SOCKET_TYPE( sp ) == SOCK_STREAM ) {
      /* If it's a TCP socket, and we're set to wait, the accept is
       * done by the child process.  Don't set NEW_DESCRIPTOR, since
       * there isn't one.  The descriptor will be/was removed from
       * the descriptor set in svc_suspend and re-enabled in svc_resume.
       */
      if( SC_WAITS( scp ) ) {
         cp->co_descriptor = SVC_FD( sp );
      } else {
         cp->co_descriptor = accept( SVC_FD( sp ), &(cp->co_remote_address.sa), &sin_len ) ;
         M_SET( cp->co_flags, COF_NEW_DESCRIPTOR ) ;
      }

      if ( cp->co_descriptor == -1 )
      {
         msg( LOG_ERR, func, "service %s, accept: %m", SVC_ID( sp ) ) ;
         return( FAILED ) ;
      }

      if( SC_NODELAY( scp ) && (scp->sc_protocol.value == IPPROTO_TCP) )
         if( setsockopt(SVC_FD(sp), IPPROTO_TCP, TCP_NODELAY, 
                        (char *)&on, sizeof( on ) ) < 0 )
            msg( LOG_WARNING, func, "service %s, setsockopt: %m", SVC_ID(sp));

      if( SC_KEEPALIVE( scp ) && (scp->sc_protocol.value == IPPROTO_TCP) )
      {
         if( setsockopt(SVC_FD(sp), SOL_SOCKET, SO_KEEPALIVE, 
                        (char *)&on, sizeof( on ) ) < 0 )
            msg( LOG_WARNING, func, "service %s, setsockopt: %m", SVC_ID(sp));
      }
      
      if( SC_IPV6(scp) && !(SC_V6ONLY( scp ))  && 
         (IN6_IS_ADDR_V4MAPPED(&cp->co_remote_address.sa_in6.sin6_addr) || 
	  IN6_IS_ADDR_V4COMPAT(&cp->co_remote_address.sa_in6.sin6_addr)) ) 
      {
         int af = AF_INET;
         if( setsockopt(cp->co_descriptor, IPPROTO_IPV6,
               IPV6_ADDRFORM, &af, sizeof( af ) ) ) {
            msg( LOG_WARNING, func, "service %s, IPV6_ADDRFORM", SVC_ID( sp) );
         }
      }

      M_SET( cp->co_flags, COF_HAVE_ADDRESS ) ;
   }
   else
   {
      if ( SVC_SOCKET_TYPE( sp ) == SOCK_DGRAM )
      {
         char t_ch ;

         /*
          * This trick is done to get the remote address.
          * select(2) guaranteed that we won't block on the recvfrom
          */
         if ( recvfrom( SVC_FD( sp ), &t_ch, 1, MSG_PEEK,
                              &cp->co_remote_address.sa, &sin_len ) == -1 )
         {
            msg( LOG_ERR, func, "service %s, recvfrom: %m", SVC_ID( sp ) ) ;
            return( FAILED ) ;
         }
         M_SET( cp->co_flags, COF_HAVE_ADDRESS ) ;
      }

      cp->co_descriptor = SVC_FD( sp ) ;
   }

   return( OK ) ;
}



/*
 * Get a connection for the specified service and return a pointer
 * to a new connection_s
 */
connection_s *conn_new( struct service *sp )
{
   connection_s    new_conn ;
   connection_s   *cp ;
   const char     *func = "conn_new" ;

   CLEAR( new_conn ) ;

   /*
    * The reason we first get the connection and then allocate a
    * 'connection_s' is because we want to always consume some input.
    */
   if ( get_connection( sp, &new_conn ) == FAILED )
      return( NULL ) ;

   new_conn.co_sp = sp ;
   SVC_HOLD( sp ) ;
   
   if ( SVC_WAITS( sp ) )
      svc_suspend( sp ) ;

   cp = NEW_CONN() ;
   if ( cp == CONN_NULL )
   {
      out_of_memory( func ) ;
      CONN_CLEANUP( &new_conn ) ;
      conn_free( &new_conn, 0 ) ;
      return( CONN_NULL ) ;
   }
   memcpy(cp, &new_conn, sizeof(connection_s));
   return( cp ) ;
}


/*
 * Release the specified connection.
 * Certain actions may be performed before doing this:
 *      - drain of a single UDP packet if the socket type is SOCK_DGRAM
 */
void conn_free( connection_s *cp, int release_mem )
{
   struct service *sp = cp->co_sp ;

   if( cp == NULL )
      return;
      if( debug.on )
         msg( LOG_INFO, "conn_free", "freeing connection") ;

   drain( cp->co_descriptor ) ;

   if ( SVC_RELE( sp ) == 0 )
      pset_remove( SERVICES( ps ), sp ) ;

   conn_close( cp ) ;

   if (release_mem) {
      FREE_CONN( cp ) ;
      cp = NULL;
   }
}

/* This returns a pointer to a local static stack variable.
 * The behavior is a remnant of inet_ntoa() behavior.
 */ 
char *conn_addrstr( const connection_s *cp )
{
   static char name[NI_MAXHOST];
   int len = 0;

   if( !M_IS_SET( (cp)->co_flags, COF_HAVE_ADDRESS ) )
      return "<no address>";

   if( cp->co_remote_address.sa.sa_family == AF_INET ) 
      len = sizeof(struct sockaddr_in);
   else if( cp->co_remote_address.sa.sa_family == AF_INET6 )
      len = sizeof(struct sockaddr_in6);

   if( getnameinfo( &cp->co_remote_address.sa, len,
         name, NI_MAXHOST, NULL, 0, NI_NUMERICHOST ) ) {
      return "<no address>";
   }
   return name;
}

void conn_dump( const connection_s *cp, int fd )
{
   const char *name = conn_addrstr( cp );

   tabprint( fd, 1, "service = %s\n", SVC_ID( cp->co_sp ) ) ;
   tabprint( fd, 1, "descriptor = %d\n", cp->co_descriptor ) ;
#if defined(__GNUC__) && !defined(__arch64__) && !defined(__alpha__)
   tabprint( fd, 1, "flags = %#llx\n", cp->co_flags ) ;
#else
   tabprint( fd, 1, "flags = %#lx\n", cp->co_flags ) ;
#endif
   tabprint( fd, 1, "remote_address = %s,%d\n", name,
                              ntohs( cp->co_remote_address.sa_in.sin_port ) ) ;
}

