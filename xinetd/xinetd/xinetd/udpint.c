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
#include <netdb.h>
#include <sys/time.h>
#include <syslog.h>
#include <errno.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "udpint.h"
#include "intcommon.h"
#include "util.h"
#include "connection.h"
#include "access.h"
#include "log.h"
#include "msg.h"
#include "sconf.h"

/*
 * Datagrams greater than this will be truncated
 */
#define MAX_DATAGRAM_SIZE         ( 32 * 1024 )

struct packet
{
   union xsockaddr from ;
   char                  *data ;
   int                  size ;
} ;

typedef struct packet packet_s ;


struct idgram_private
{
   unsigned received_packets ;
} ;

#define IDP( p )               ((struct idgram_private *)(p))


static struct idgram_private idgram ;

static void di_mux(void) ;
static void udp_remote_to_local( struct intercept_s *ip, channel_s **chpp );
static status_e udp_local_to_remote( channel_s *chp );
static void send_data( int sd, char *buf, int len, union xsockaddr *addr );
static status_e get_incoming_packet( struct intercept_s *ip, packet_s *pp );

static const struct intercept_ops idgram_ops =
   {
      di_mux,
      di_exit
   } ;


static struct intercept_s dgram_intercept_state ;


struct intercept_s *di_init( struct server *serp )
{
   struct intercept_s *ip = &dgram_intercept_state ;
   
   ip->int_socket_type = SOCK_DGRAM ;
   ip->int_priv = (void *) &idgram ;
   ip->int_ops = &idgram_ops ;
   int_init( ip, serp ) ;
   return( ip ) ;
}


void di_exit(void)
{
   struct intercept_s *ip = &dgram_intercept_state ;

   if ( IDP( ip->int_priv )->received_packets == 0 )
      drain( INT_REMOTE( ip ) ) ;
   int_exit( ip ) ;
}


/*
 * Returns only if there is an I/O error while communicating with the server
 */
static void di_mux(void)
{
   struct intercept_s   *ip = &dgram_intercept_state ;
   fd_set                     socket_mask ;
   int                        mask_max ;

   FD_ZERO( &socket_mask ) ;
   FD_SET( INT_REMOTE( ip ), &socket_mask ) ;
   mask_max = INT_REMOTE( ip ) ;

   for ( ;; )
   {
      unsigned u ;
      channel_s *chp ;
      fd_set read_mask ;
      int n_ready ;

      read_mask = socket_mask ;
      n_ready = int_select( mask_max+1, &read_mask ) ;

      if ( n_ready == -1 )
         return ;
      
      if ( FD_ISSET( INT_REMOTE( ip ), &read_mask ) )
      {
         udp_remote_to_local( ip, &chp ) ;
         if ( chp != NULL )
         {
            FD_SET( chp->ch_local_socket, &socket_mask ) ;
            if ( chp->ch_local_socket > mask_max )
               mask_max = chp->ch_local_socket ;
         }
         if ( --n_ready == 0 )
            continue ;
      }

      for ( u = 0 ; u < pset_count( INT_CONNECTIONS( ip ) ) ; u++ )
      {
         chp = CHP( pset_pointer( INT_CONNECTIONS( ip ), u ) ) ;

         if ( FD_ISSET( chp->ch_local_socket, &read_mask ) )
         {
            if ( udp_local_to_remote( chp ) == FAILED )
               return ;
            if ( --n_ready == 0 )
               break ;
         }
      }
   }
}


/*
 * Read data from the remote socket and send it to the appropriate local 
 * socket.
 * If this is a new connection, insert it in the connection table and
 * place its handle in *chpp.
 */
static void udp_remote_to_local( struct intercept_s *ip, channel_s **chpp )
{
   char               buf[ MAX_DATAGRAM_SIZE ] ;
   packet_s           packet ;
   channel_s          *chp ;
   bool_int           addr_checked ;

   *chpp = CHANNEL_NULL ;

   packet.data = buf ;
   packet.size = sizeof( buf ) ;
   if ( get_incoming_packet( ip, &packet ) == FAILED )
      return ;

   chp = int_lookupconn( ip, &packet.from, &addr_checked ) ;
   if ( chp == CHANNEL_NULL )
   {
      struct server      *serp = INT_SERVER( ip ) ;
      struct service    *sp = SERVER_SERVICE( serp ) ;
      connection_s      *cop = SERVER_CONNECTION( serp ) ;

      if ( ( chp = int_newconn( ip, &packet.from, INT_REMOTE( ip ) ) ) == NULL )
         return ;

      CONN_SETADDR( cop, &packet.from ) ;      /* for logging */

      if ( INTERCEPT( ip ) )
      {
         mask_t check_mask ;
         access_e result ;

         M_OR( check_mask, XMASK( CF_ADDRESS ), XMASK( CF_TIME ) ) ;
         result = access_control( sp, cop, &check_mask ) ;

         if ( result != AC_OK )
         {
            svc_log_failure( sp, cop, result ) ;
            chp->ch_state = BAD_CHANNEL ;
            return ;
         }
      }
      
      /*
       * Since we don't distinguish ports, there is no point to log
       * another successful attempt from the same address
       */
      if ( ! addr_checked )
         svc_log_success( sp, cop, SERVER_PID( serp ) ) ;
         
      *chpp = chp ;
   }
   else if ( chp->ch_state == BAD_CHANNEL )
      return ;
   
#ifdef DEBUG_UDPINT
   if ( debug.on )
      msg( LOG_DEBUG, "udp_remote_to_local",
               "sending %d bytes to server on port %d",
                     packet.size, ntohs( INT_LOCALADDR( ip )->sin_port ) ) ;
#endif

   send_data( chp->ch_local_socket,
         packet.data, packet.size, NULL ) ;
}


/*
 * Send the data in buf to destination addr using the socket sd.
 * If addr is NULL, use the default socket destination
 */
static void send_data( int sd, char *buf, int len, union xsockaddr *addr )
{
   char   *p ;
   int   left ;
   int   cc ;
   const char   *func = "send_data" ;

   for ( p = buf, left = len ; left > 0 ; left -= cc, p+= cc )
   {
      if ( addr == NULL )
         cc = send( sd, p, left, 0 ) ;
      else
         cc = sendto( sd, p, left, 0, SA( addr ), sizeof( *addr ) ) ;

      if ( cc == -1 ) {
         if ( errno == EINTR )
         {
            cc = 0 ;
            continue ;
         }
         else
         {
            msg( LOG_ERR, func, "%s: %m", addr ? "sendto" : "send" ) ;
            return ;
         }
      }
   }
}


static status_e get_incoming_packet( struct intercept_s *ip, packet_s *pp )
{
   int from_len = 0;
   const char *func = "get_incoming_packet" ;

   if( SC_IPV4( SVC_CONF( SERVER_SERVICE( INT_SERVER( ip ) ) ) ) )
      from_len = sizeof( struct sockaddr_in );
   if( SC_IPV6( SVC_CONF( SERVER_SERVICE( INT_SERVER( ip ) ) ) ) )
      from_len = sizeof( struct sockaddr_in6 );

   for ( ;; )
   {
      int cc ;

      from_len = sizeof( pp->from ) ;
      cc = recvfrom( INT_REMOTE( ip ), pp->data, pp->size,
                                    0, SA( &pp->from ), &from_len ) ;
      if ( cc == -1 )
      {
         if ( errno != EINTR )
         {
            msg( LOG_ERR, func, "recvfrom error: %m" ) ;
            return( FAILED ) ;
         }
      }
      else if ( cc == 0 )
         return( FAILED ) ;
      else
      {
         pp->size = cc ;
         IDP( ip->int_priv )->received_packets++ ;
         break ;
      }
   }

   if ( from_len == 0 )
   {
      msg( LOG_ERR, func, "incoming packet had 0 length address" ) ;
      return( FAILED ) ;
   }
   
#ifdef DEBUG_UDPINT
   if ( debug.on )
      msg( LOG_DEBUG, func, "Received %d bytes from address: %s,%d",
         pp->size, xaddrname( &pp->from ), ntohs( xaddrport(&pp->from) ) );
#endif

   return( OK ) ;
}


static status_e udp_local_to_remote( channel_s *chp )
{
   char   buf[ MAX_DATAGRAM_SIZE ] ;
   int    cc ;
   const char   *func = "udp_local_to_remote" ;

   for ( ;; )
   {
      cc = recv( chp->ch_local_socket, buf, sizeof( buf ), 0 ) ;
   
      if ( cc == -1 )
      {
         if ( errno != EINTR ) 
         {
            msg( LOG_ERR, func, "recv from daemon: %m" ) ;
            return( FAILED ) ;
         }
      }
      else if ( cc == 0 )
         return( FAILED ) ;
      else
         break ;
   }
   
#ifdef DEBUG_UDPINT
   if ( debug.on )
      msg( LOG_DEBUG, func, "sending %d bytes to address %s,%d",
         cc, xaddrname( &chp->ch_from ), ntohs( xaddrport(&chp->ch_from) ) ) ;
#endif

   send_data( chp->ch_remote_socket, buf, cc, &chp->ch_from ) ;
   return( OK ) ;
}

