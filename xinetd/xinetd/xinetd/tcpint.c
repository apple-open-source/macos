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
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "tcpint.h"
#include "intcommon.h"
#include "msg.h"
#include "log.h"
#include "xconfig.h"
#include "sconf.h"

typedef enum { S_OK, S_SERVER_ERR, S_CLIENT_ERR } stream_status_e ;

struct istream_private
{
   unsigned accepted_connections ;
} ;

#define SIP( p )                  ((struct istream_private *)(p))

static struct istream_private istream ;
static struct intercept_s stream_intercept_state ;
static void si_mux(void) ;


static struct intercept_ops istream_ops =
{
      si_mux,
      si_exit
} ;



struct intercept_s *si_init( struct server *serp )
{
   struct intercept_s *ip = &stream_intercept_state ;

   ip->int_socket_type = SOCK_STREAM ;
   ip->int_priv = (void *) &istream ;
   ip->int_ops = &istream_ops ;
   int_init( ip, serp ) ;
   if ( signal( SIGPIPE, SIG_IGN ) == SIG_ERR )
      int_fail( ip, "signal" ) ;
   return( ip ) ;
}

static status_e handle_io( psi_h iter, channel_s *chp, fd_set *maskp, stream_status_e (*iofunc)() );
static stream_status_e tcp_local_to_remote( channel_s *chp );
static stream_status_e tcp_remote_to_local( channel_s *chp );
static void connection_request( struct intercept_s *ip, channel_s **chpp );

/* Unfortunatly, this can't be private... */
void si_exit(void)
{
   struct intercept_s *ip = &stream_intercept_state ;
   
   if ( SIP( ip->int_priv )->accepted_connections == 0 )
      (void) accept( INT_REMOTE( ip ), SA( NULL ), INT_NULL ) ;
   int_exit( ip ) ;
}


static void si_mux(void)
{
   struct intercept_s   *ip = &stream_intercept_state ;
   fd_set                     socket_mask ;
   int                        mask_max ;
   psi_h                      iter ;
   const char                *func = "si_mux" ;

   FD_ZERO( &socket_mask ) ;
   FD_SET( INT_REMOTE( ip ), &socket_mask ) ;
   mask_max = INT_REMOTE( ip ) ;

   iter = psi_create( INT_CONNECTIONS( ip ) ) ;
   if ( iter == NULL )
   {
      msg( LOG_ERR, func, ES_NOMEM ) ;
      return ;
   }

   for ( ;; )
   {
      channel_s *chp ;
      fd_set read_mask ;
      int n_ready ;

      read_mask = socket_mask ;
      n_ready = int_select( mask_max+1, &read_mask ) ;

      if ( n_ready == -1 )
         return ;
      
      if ( FD_ISSET( INT_REMOTE( ip ), &read_mask ) )
      {
         connection_request( ip, &chp ) ;
         if ( chp != NULL )
         {
            FD_SET( chp->ch_local_socket, &socket_mask ) ;
            if ( chp->ch_local_socket > mask_max )
               mask_max = chp->ch_local_socket ;
            FD_SET( chp->ch_remote_socket, &socket_mask ) ;
            if ( chp->ch_remote_socket > mask_max )
               mask_max = chp->ch_remote_socket ;
         }
         if ( --n_ready == 0 )
            continue ;
      }

      for ( chp = CHP( psi_start(iter) ) ; chp ; chp = CHP( psi_next(iter) ) )
      {
         if ( FD_ISSET( chp->ch_local_socket, &read_mask ) )
         {
#ifdef DEBUG_TCPINT
            if ( debug.on )
               msg( LOG_DEBUG, func, "Input available on local socket %d", 
                                                         chp->ch_local_socket ) ;
#endif
            if ( handle_io( iter, chp, &socket_mask, tcp_local_to_remote ) == FAILED )
               return ;
            if ( --n_ready == 0 )
               break ;
         }

         if ( FD_ISSET( chp->ch_remote_socket, &read_mask ) )
         {
#ifdef DEBUG_TCPINT
            msg( LOG_DEBUG, func, "Input available on remote socket %d", 
                                                      chp->ch_remote_socket ) ;
#endif
            if ( handle_io( iter, chp,
                        &socket_mask, tcp_remote_to_local ) == FAILED )
               return ;
            if ( --n_ready == 0 )
               break ;
         }
      }
   }
}


static status_e handle_io( psi_h iter, 
                            channel_s *chp, 
                            fd_set *maskp, 
                            stream_status_e (*iofunc)() )
{
   const char *func = "handle_io" ;

   switch ( (*iofunc)( chp ) )
   {
      case S_SERVER_ERR:
         return( FAILED ) ;
      
      case S_CLIENT_ERR:

         if ( debug.on )
            msg( LOG_DEBUG, func,
               "Closing channel to %s,%d using sockets %d(l),%d(r)",
                  xaddrname( &chp->ch_from ), ntohs(xaddrport( &chp->ch_from )),
                        chp->ch_local_socket, chp->ch_remote_socket ) ;

         FD_CLR( chp->ch_local_socket, maskp ) ;
         FD_CLR( chp->ch_remote_socket, maskp ) ;
         (void) Sclose( chp->ch_remote_socket ) ;
         (void) Sclose( chp->ch_local_socket ) ;
         psi_remove( iter ) ;
         FREE_CHANNEL( chp ) ;
         break ;
      case S_OK:
         break ;
   }
   return( OK ) ;
}


static void connection_request( struct intercept_s *ip, channel_s **chpp )
{
   union xsockaddr      csin ;
   int                  sin_len = 0;
   channel_s           *chp ;
   int                  sd ;
   bool_int             addr_checked ;
   const char          *func = "connection_request" ;

   *chpp = NULL ;
   if( SC_IPV4( SVC_CONF( SERVER_SERVICE( INT_SERVER( ip ) ) ) ) )
      sin_len = sizeof(struct sockaddr_in);
   if( SC_IPV6( SVC_CONF( SERVER_SERVICE( INT_SERVER( ip ) ) ) ) )
      sin_len = sizeof(struct sockaddr_in6);

   if ( ( sd = accept( INT_REMOTE( ip ), SA( &csin ), &sin_len ) ) == -1 ) 
      return ;
   
   SIP( ip->int_priv )->accepted_connections++ ;

   if ( debug.on )
      msg( LOG_DEBUG, func, "connection request from %s,%d",
         xaddrname( &csin ), ntohs( xaddrport( &csin ) ) ) ;

   chp = int_lookupconn( ip, &csin, &addr_checked ) ;
   if ( chp == NULL )
   {
      struct server  *serp  = INT_SERVER( ip ) ;
      struct service *sp    = SERVER_SERVICE( serp ) ;
      connection_s   *cop   = SERVER_CONNECTION( serp ) ;

      CONN_SETADDR( cop, &csin ) ;

      if ( INTERCEPT( ip ) )
      {
         mask_t check_mask ;
         access_e result ;
         
         M_OR( check_mask, XMASK( CF_ADDRESS ), XMASK( CF_TIME ) ) ;
         result = access_control( sp, cop, &check_mask ) ;

         if ( result != AC_OK )
         {
            svc_log_failure( sp, cop, result ) ;
            (void) Sclose( sd ) ;
            return ;
         }
      }

      if ( ( chp = int_newconn( ip, &csin, sd ) ) == NULL )
      {
         (void) Sclose( sd ) ;
         return ;
      }
      
      if ( ! addr_checked )
         svc_log_success( sp, cop, SERVER_PID( serp ) ) ;

#if defined( TCP_NODELAY )
      {
         int on = 1 ;

         (void) setsockopt( chp->ch_local_socket, IPPROTO_TCP,
                              TCP_NODELAY, (char *) &on, sizeof( on ) ) ;
         (void) setsockopt( chp->ch_remote_socket, IPPROTO_TCP,
                              TCP_NODELAY, (char *) &on, sizeof( on ) ) ;
      }
#endif   /* TCP_NODELAY */
      
      *chpp = chp ;
   }
   else
      msg( LOG_ERR, func,
         "Received another connection request from %s,%d",
            xaddrname( &csin ), ntohs( xaddrport( &csin ) ) ) ;
}


static stream_status_e tcp_local_to_remote( channel_s *chp )
{
   char  buf[ DATAGRAM_SIZE ] ;
   int   rcc, wcc ;
   char *p ;
   int   left ;
   const char *func = "tcp_local_to_remote" ;

   for ( ;; )
   {
      rcc = recv( chp->ch_local_socket, buf, sizeof( buf ), 0 ) ;
      if ( rcc == 0 )
         return( S_SERVER_ERR ) ;
      else if ( rcc == -1 )
      {
         if ( errno != EINTR )
         {
            msg( LOG_ERR, func, "recv: %m" ) ;
            return( S_SERVER_ERR ) ;
         }
      }
      else
         break ;
   }

   for ( p = buf, left = rcc ; left ; p += wcc, left -= wcc )
   {
      wcc = send( chp->ch_remote_socket, p, left, 0 ) ;
      if ( wcc == 0 )
         return( S_CLIENT_ERR ) ;
      else if ( wcc == -1 )
      {
         if ( errno == EINTR )
            wcc = 0 ;
         else
         {
            msg( LOG_ERR, func, "send: %m" ) ;
            return( S_CLIENT_ERR ) ;
         }
      }
   }

#ifdef DEBUG_TCPINT
   if ( debug.on )
      msg( LOG_DEBUG, func,
         "Transferred %d bytes from local socket %d to remote socket %d",
            rcc, chp->ch_local_socket, chp->ch_remote_socket ) ;
#endif

   return( S_OK ) ;
}


static stream_status_e tcp_remote_to_local( channel_s *chp )
{
   char  buf[ DATAGRAM_SIZE ] ;
   int   rcc, wcc ;
   int   left ;
   char *p ;
   const char *func = "tcp_remote_to_local" ;

   for ( ;; )
   {
      rcc = recv( chp->ch_remote_socket, buf, sizeof( buf ), 0 ) ;
      if ( rcc == 0 )
         return( S_CLIENT_ERR ) ;
      else if ( rcc == -1 )
      {
         if ( errno != EINTR )
         {
            msg( LOG_ERR, func, "recv: %m" ) ;
            return( S_CLIENT_ERR ) ;
         }
      }
      else
         break ;
   }

   for ( p = buf, left = rcc ; left ; p += wcc, left -= wcc )
   {
      wcc = send( chp->ch_local_socket, p, left, 0 ) ;
      if ( wcc == 0 ) {
         return( S_SERVER_ERR ) ;
      } else if ( wcc == -1 ) {
         if ( errno == EINTR ) {
            rcc = 0 ;
         } else {
            msg( LOG_ERR, func, "send: %m" ) ;
            return( S_SERVER_ERR ) ;
         }
      }
   }

#ifdef DEBUG_TCPINT
   if ( debug.on )
      msg( LOG_DEBUG, func,
         "Transferred %d bytes from remote socket %d to local socket %d",
            rcc, chp->ch_remote_socket, chp->ch_local_socket ) ;
#endif

   return( S_OK ) ;
}

