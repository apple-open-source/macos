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
#include <errno.h>
#include <time.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#include "str.h"
#include "pset.h"
#include "sio.h"
#include "builtins.h"
#include "msg.h"
#include "connection.h"
#include "server.h"
#include "service.h"
#include "sconf.h"
#include "main.h"
#include "util.h"
#include "xconfig.h"
#include "state.h"
#include "libportable.h"
#include "signals.h"
#include "nvlists.h"
#include "child.h"

#define BUFFER_SIZE               1024

static void stream_echo(const struct server *) ;
static void dgram_echo(const struct server *) ;
static void stream_discard(const struct server *) ;
static void dgram_discard(const struct server *) ;
static void stream_time(const struct server *) ;
static void dgram_time(const struct server *) ;
static void stream_daytime(const struct server *) ;
static void dgram_daytime(const struct server *) ;
static void stream_chargen(const struct server *) ;
static void dgram_chargen(const struct server *) ;
static void stream_services(const struct server *) ;
static void tcpmux_handler(const struct server *) ;
static int bad_port_check(const union xsockaddr *, const char *);

/*
 * SG - This is the call sequence to get to a built-in service
 *
 *   main_loop                  main.c
 *    svc_request                  service.c
 *     svc_handle -- aka svc_handler_func -- aka svc_generic_handler   service.c
 *      server_run                      server.c
 *       server_internal               server.c
 *      SC_INTERNAL               service.h
 *       BUILTIN_INVOKE               sc_conf.h
 *        sc_builtin -- index into builtin_services   builtins.c
 */

static const struct builtin_service builtin_services[] =
   {
      { "echo",      SOCK_STREAM,   { stream_echo,     FORK    } },
      { "echo",      SOCK_DGRAM,    { dgram_echo,      NO_FORK } },
      { "discard",   SOCK_STREAM,   { stream_discard,  FORK    } },
      { "discard",   SOCK_DGRAM,    { dgram_discard,   NO_FORK } },
      { "time",      SOCK_STREAM,   { stream_time,     NO_FORK } },
      { "time",      SOCK_DGRAM,    { dgram_time,      NO_FORK } },
      { "daytime",   SOCK_STREAM,   { stream_daytime,  NO_FORK } },
      { "daytime",   SOCK_DGRAM,    { dgram_daytime,   NO_FORK } },
      { "chargen",   SOCK_STREAM,   { stream_chargen,  FORK    } },
      { "chargen",   SOCK_DGRAM,    { dgram_chargen,   NO_FORK } },
      { "services",  SOCK_STREAM,   { stream_services, FORK    } },
      { "sensor",    SOCK_STREAM,   { stream_discard,  NO_FORK } },
      { "sensor",    SOCK_DGRAM,    { dgram_discard,   NO_FORK } },
      { "tcpmux",    SOCK_STREAM,   { tcpmux_handler,  FORK    } },
      { NULL,        0,             { NULL,            0       } }
   } ;


const builtin_s *builtin_find( const char *service_name, int type )
{
   const builtin_s   *bsp ;
   const char        *func = "builtin_find" ;
   
   if ( (bsp = builtin_lookup( builtin_services, service_name, type )) )
      return( bsp ) ;
   else
   {
	const char *type_name;
   	const struct name_value *sock_type = nv_find_name( socket_types, type );
	if (sock_type == NULL)
		type_name = "Unknown socket type";
	else
		type_name = sock_type->name;
	msg( LOG_ERR, func, "No such internal service: %s/%s - DISABLING", 
			service_name, type_name ) ;
	return( NULL ) ;
   }
}


const builtin_s *builtin_lookup( const struct builtin_service services[], 
                           const char *service_name, 
                           int type )
{
   const struct builtin_service *bsp ;
   
   for ( bsp = services ; bsp->bs_name != NULL ; bsp++ )
      if ( EQ( bsp->bs_name, service_name ) && bsp->bs_socket_type == type )
         return( &bsp->bs_handle ) ;
   return( NULL ) ;
}


/*
 * The rest of this file contains the functions that implement the 
 * builtin services
 */


static void stream_echo( const struct server *serp )
{
   char   buf[ BUFFER_SIZE ] ;
   int    cc ;
   int    descriptor = SERVER_FD( serp ) ;
   const struct service *svc = SERVER_SERVICE( serp ) ;;

   if( SVC_WAITS( svc ) ) {
      descriptor = accept(descriptor, NULL, NULL);
      if ( descriptor == -1 ) return;
   }

   close_all_svc_descriptors();

   for ( ;; )
   {
      cc = read( descriptor, buf, sizeof( buf ) ) ;
      if ( cc == 0 )
         break ;
      if ( cc == -1 ) {
         if ( errno == EINTR )
            continue ;
         else
            break ;
      }

      if ( write_buf( descriptor, buf, cc ) == FAILED )
         break ;
   }
   if( SVC_WAITS( svc ) ) /* Service forks, so close it */
      Sclose(descriptor);
}

/* For internal UDP services, make sure we don't respond to our ports
 * on other servers and to low ports of other services (such as DNS).
 * This can cause looping.
 */
static int bad_port_check( const union xsockaddr *sa, const char *func )
{
   uint16_t port = 0;

   port = ntohs( xaddrport( sa ) );

   if ( port < 1024 ) {
      msg(LOG_WARNING, func,
         "Possible Denial of Service attack from %s %d", xaddrname(sa), port);
      return (-1);
   }

   return (0);
}

static void dgram_echo( const struct server *serp )
{
   char            buf[ DATAGRAM_SIZE ] ;
   union xsockaddr lsin;
   int             cc ;
   unsigned int    sin_len = 0;
   int             descriptor = SERVER_FD( serp ) ;
   const char     *func = "dgram_echo";

   if( SC_IPV4( SVC_CONF( SERVER_SERVICE( serp ) ) ) )
      sin_len = sizeof( struct sockaddr_in );
   else if( SC_IPV6( SVC_CONF( SERVER_SERVICE( serp ) ) ) )
      sin_len = sizeof( struct sockaddr_in6 );

   cc = recvfrom( descriptor, buf, sizeof( buf ), 0, SA( &lsin ), &sin_len ) ;
   if ( cc != -1 ) {
      if( bad_port_check(&lsin, func) != 0 ) return;
      (void) sendto( descriptor, buf, cc, 0, SA( &lsin ), sizeof( lsin ) ) ;
   }
}

static void stream_discard( const struct server *serp )
{
   char  buf[ BUFFER_SIZE ] ;
   int   cc ;
   int    descriptor = SERVER_FD( serp ) ;
   const struct service *svc = SERVER_SERVICE( serp ) ;;

   if( SVC_WAITS( svc ) ) {
      descriptor = accept(descriptor, NULL, NULL);
      if ( descriptor == -1 ) return;
   }

   close_all_svc_descriptors();

   for ( ;; )
   {
      cc = read( descriptor, buf, sizeof( buf ) ) ;
      if ( (cc == 0) || ((cc == -1) && (errno != EINTR)) )
         break ;
   }
   if( SVC_WAITS( svc ) ) /* Service forks, so close it */
      Sclose(descriptor);
}


static void dgram_discard( const struct server *serp )
{
   char buf[ 1 ] ;

   (void) recv( SERVER_FD( serp ), buf, sizeof( buf ), 0 ) ;
}


/*
 * Generate the current time using the SMTP format:
 *      02 FEB 1991 12:31:42 MST
 *
 * The result is placed in buf.
 * buflen is a value-result parameter. It indicates the size of
 * buf and on exit it has the length of the string placed in buf.
 */
static void daytime_protocol( char *buf, unsigned int *buflen )
{
   static const char *month_name[] =
      {
         "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
         "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
      } ;
   time_t      now ;
   struct tm   *tmp ;
   int         size = *buflen ;
#ifdef HAVE_STRFTIME
   int      cc ;
#endif

   (void) time( &now ) ;
   tmp = localtime( &now ) ;
#ifndef HAVE_STRFTIME
   strx_print( buflen, buf, size,
      "%02d %s %d %02d:%02d:%02d %s\r\n",
         tmp->tm_mday, month_name[ tmp->tm_mon ], 1900 + tmp->tm_year,
            tmp->tm_hour, tmp->tm_min, tmp->tm_sec, tmp->tm_zone ) ;
#else
   cc = strx_nprint( buf, size,
      "%02d %s %d %02d:%02d:%02d",
         tmp->tm_mday, month_name[ tmp->tm_mon ], 1900 + tmp->tm_year,
            tmp->tm_hour, tmp->tm_min, tmp->tm_sec ) ;
   if ( cc >= 0 ) { 
      *buflen = cc ;
      size -= cc ;
      cc = strftime( &buf[ *buflen ], size, " %Z\r\n", tmp ) ;
      *buflen += cc ;
   }
#endif
}


static void stream_daytime( const struct server *serp )
{
   char  time_buf[ BUFFER_SIZE ] ;
   unsigned int buflen = sizeof( time_buf ) ;
   int    descriptor = SERVER_FD( serp ) ;
   const struct service *svc = SERVER_SERVICE( serp ) ;;

   if( SVC_WAITS( svc ) ) {
      descriptor = accept(descriptor, NULL, NULL);
      if ( descriptor == -1 ) return;
   }
   daytime_protocol( time_buf, &buflen ) ;
   (void) write_buf( descriptor, time_buf, buflen ) ;
   Sclose(descriptor);
}


static void dgram_daytime( const struct server *serp )
{
   char            time_buf[ BUFFER_SIZE ] ;
   union xsockaddr lsin ;
   unsigned int    sin_len     = 0 ;
   unsigned int    buflen      = sizeof( time_buf ) ;
   int             descriptor  = SERVER_FD( serp ) ;
   const char     *func       = "dgram_daytime";

   if ( SC_IPV4( SVC_CONF( SERVER_SERVICE( serp ) ) ) ) 
      sin_len = sizeof( struct sockaddr_in );
   else if ( SC_IPV6( SVC_CONF( SERVER_SERVICE( serp ) ) ) ) 
      sin_len = sizeof( struct sockaddr_in6 );

   if ( recvfrom( descriptor, time_buf, sizeof( time_buf ), 0,
            SA( &lsin ), &sin_len ) == -1 )
      return ;

   if( bad_port_check(&lsin, func) != 0 ) return;

   daytime_protocol( time_buf, &buflen ) ;
   
   (void) sendto( descriptor, time_buf, buflen, 0, SA(&lsin), sizeof( lsin ) ) ;
}


#define TIME_OFFSET         2208988800UL

/*
 * We always report the time as 32 bits in network-byte-order
 */
static void time_protocol( unsigned char *timep )
{
   time_t now ;
   unsigned long base1900;

   (void) time( &now ) ;
   base1900 = (unsigned long)now + TIME_OFFSET ;
   timep[0] = base1900 >> 24;
   timep[1] = base1900 >> 16;
   timep[2] = base1900 >> 8;
   timep[3] = base1900;

}


static void stream_time( const struct server *serp )
{
   unsigned char time_buf[4];
   int descriptor = SERVER_FD( serp );
   const struct service *svc = SERVER_SERVICE( serp );

   if( SVC_WAITS( svc ) ) {
      descriptor = accept(descriptor, NULL, NULL);
      if ( descriptor == -1 ) return;
   }

   time_protocol( time_buf ) ;
   (void) write_buf( descriptor, (char *) time_buf, 4 ) ;

   Sclose(descriptor);
}


static void dgram_time( const struct server *serp )
{
   char     buf[ 1 ] ;
   unsigned char time_buf[4];
   union xsockaddr lsin ;
   unsigned int    sin_len = 0 ;
   int             fd      = SERVER_FD( serp ) ;
   const char     *func    = "dgram_daytime";

   if ( SC_IPV4( SVC_CONF( SERVER_SERVICE( serp ) ) ) ) 
      sin_len = sizeof( struct sockaddr_in );
   else if ( SC_IPV6( SVC_CONF( SERVER_SERVICE( serp ) ) ) ) 
      sin_len = sizeof( struct sockaddr_in6 );

   if ( recvfrom( fd, buf, sizeof( buf ), 0, SA( &lsin ), &sin_len ) == -1 )
      return ;
   if( bad_port_check(&lsin, func) != 0 ) return;

   time_protocol( time_buf ) ;
   (void) sendto( fd, (char *) time_buf, 4, 0, SA( &lsin ), sin_len ) ;
}


#define ASCII_PRINTABLE_CHARS     94
#define LINE_LENGTH               72

#define RING_BUF_SIZE             ASCII_PRINTABLE_CHARS + LINE_LENGTH

static char *ring_buf = NULL ;
static char *ring ;


#define ASCII_START          ( ' ' + 1 )
#define ASCII_END            126

#define min( a, b )          ((a)<(b) ? (a) : (b))

static char *generate_line( char *buf, unsigned int len )
{
   unsigned int line_len = min( LINE_LENGTH, len-2 ) ;

   if ( len < 2 )       /* If len < 2, min will be wrong */
      return( NULL ) ;

   /* This never gets freed.  That's ok, because the reference to it is
    * always kept for future reference.
    */
   if ( (ring_buf == NULL) && ((ring_buf = malloc(RING_BUF_SIZE)) == NULL) ) 
      return(NULL);

   if ( ring == NULL )
   {
      char ch ;
      char *p ;

      for ( p = ring_buf, ch = ASCII_START ;
            p <= &ring_buf[ RING_BUF_SIZE - 1 ] ; p++ )
      {
         *p = ch++ ;
         if ( ch == ASCII_END )
            ch = ASCII_START ;
      }
      ring = ring_buf ;
   }
   (void) memcpy( buf, ring, line_len ) ;
   buf[ line_len   ] = '\r' ;
   buf[ line_len+1 ] = '\n' ;

   ring++ ;
   if ( &ring_buf[ RING_BUF_SIZE - 1 ] - ring + 1 < LINE_LENGTH )
      ring = ring_buf ;
   return( buf ) ;
}


static void stream_chargen( const struct server *serp )
{
   char   line_buf[ LINE_LENGTH+2 ] ;
   int    descriptor = SERVER_FD( serp ) ;
   const struct service *svc = SERVER_SERVICE( serp );

   if( SVC_WAITS( svc ) ) {
      descriptor = accept(descriptor, NULL, NULL);
      if ( descriptor == -1 ) return;
   }

   (void) shutdown( descriptor, 0 ) ;
   close_all_svc_descriptors();

   for ( ;; )
   {
      if ( generate_line( line_buf, sizeof( line_buf ) ) == NULL )
         break ;
      if ( write_buf( descriptor, line_buf, sizeof( line_buf ) ) == FAILED )
         break ;
   }
   if( SVC_WAITS( svc ) ) /* Service forks, so close it */
      Sclose(descriptor);
}


static void dgram_chargen( const struct server *serp )
{
   char            buf[ BUFFER_SIZE ] ;
   char            *p ;
   unsigned int    len ;
   union xsockaddr lsin ;
   unsigned int    sin_len = 0 ;
   int             fd      = SERVER_FD( serp ) ;
   unsigned int    left    = sizeof( buf ) ;
   const char     *func    = "dgram_chargen";

   if ( SC_IPV4( SVC_CONF( SERVER_SERVICE( serp ) ) ) ) 
      sin_len = sizeof( struct sockaddr_in );
   else if ( SC_IPV6( SVC_CONF( SERVER_SERVICE( serp ) ) ) ) 
      sin_len = sizeof( struct sockaddr_in6 );

   if ( recvfrom( fd, buf, sizeof( buf ), 0, SA( &lsin ), &sin_len ) == -1 )
      return ;

#if BUFFER_SIZE < LINE_LENGTH+2
   bad_variable = 1 ;      /* this will cause a compilation error */
#endif

   if( bad_port_check(&lsin, func) != 0 ) return;

   for ( p = buf ; left > 2 ; left -= len, p += len )
   {
      len = min( LINE_LENGTH+2, left ) ;
      if ( generate_line( p, len ) == NULL )
         break ;
   }
   (void) sendto( fd, buf, p-buf, 0, SA( &lsin ), sin_len ) ;
}


static void stream_services( const struct server *serp )
{
   unsigned u ;
   int      fd = SERVER_FD( serp ) ;

   close_all_svc_descriptors();

   for ( u = 0 ; u < pset_count( SERVICES( ps ) ) ; u++ )
   {
      int cc ;
      char buf[ BUFFER_SIZE ] ;
      const struct service_config *scp ;
      
      scp = SVC_CONF( SP( pset_pointer( SERVICES( ps ), u ) ) ) ;

      strx_print( &cc, buf, sizeof( buf ), "%s %s %d\n",
         SC_NAME( scp ), SC_PROTONAME( scp ), SC_PORT( scp ) ) ;
      if ( cc >= 0) {         
         if ( write_buf( fd, buf, cc ) == FAILED )
            break ;
      }
   }
}

/*  Handle a request for a tcpmux service. 
 *  It's helpful to remember here that we are now a child of the original
 *  xinetd process. We were forked to keep the parent from blocking
 *  when we try to read the service name off'n the socket connection.
 *  Serp still points to an actual tcpmux 'server', or at least the
 *  service pointer of serp is valid.
 */

static void tcpmux_handler( const struct server *serp )
{
   char      svc_name[ BUFFER_SIZE ] ;
   int       cc ;
   int       descriptor = SERVER_FD( serp ) ;
   const     struct service *svc = SERVER_SERVICE( serp ) ;
   unsigned  u;
   struct    service *sp = NULL;
   struct    server server, *nserp;
   struct    service_config *scp = NULL;

   close_all_svc_descriptors();

   /*  Read in the name of the service in the format "svc_name\r\n".
    *
    *  XXX: should loop on partial reads (could probably use Sread() if
    *  it wasn't thrown out of xinetd source code a few revisions back).
    */
   do
   {
      cc = read( descriptor, svc_name, sizeof( svc_name ) ) ;
   } while (cc == -1 && errno == EINTR);

   if ( cc <= 0 )
   {
      msg(LOG_ERR, "tcpmux_handler", "read failed");
      exit(0);
   }

   if ( ( cc <= 2 ) ||
        ( ( svc_name[cc - 1] != '\n' ) || ( svc_name[cc - 2] != '\r' ) ) )
   {
      if ( debug.on )
         msg(LOG_DEBUG, "tcpmux_handler", "Invalid service name format.");
      
      exit(0);
   }

   svc_name[cc - 2] = '\0';  /*  Remove \r\n for compare */

   if ( debug.on )
   {
      msg(LOG_DEBUG, "tcpmux_handler", "Input (%d bytes) %s as service name.",
          cc, svc_name);
   }

   /*  Search the services for the a match on name.
    */

   for ( u = 0 ; u < pset_count( SERVICES( ps ) ) ; u++ )
   {
      sp = SP( pset_pointer( SERVICES( ps ), u ) ) ;

      if ( strcasecmp( svc_name, SC_NAME( SVC_CONF( sp ) ) ) == 0 )
      {
         /*  Found the pointer. Validate its type.
          */
         scp = SVC_CONF( sp );
/*
         if ( ! SVC_IS_MUXCLIENT( sp ) )
         {
            if ( debug.on )
            {
               msg(LOG_DEBUG, "tcpmux_handler", "Non-tcpmux service name: %s.",
                   svc_name);
            }
            exit(0);
         }
*/

         /*  Send the accept string if we're a PLUS (+) client.
          */

         if ( SVC_IS_MUXPLUSCLIENT( sp ) )
         {
            if ( Swrite( descriptor, TCPMUX_ACK, sizeof( TCPMUX_ACK ) ) !=
                 sizeof( TCPMUX_ACK ) )
            {
                msg(LOG_ERR, "tcpmux_handler", "Ack write failed for %s.",
		    svc_name);
                exit(0);
            }
         }
         break;  /*  Time to get on with the service */
      }
      continue;  /*  Keep looking */
   }

   if ( u >= pset_count( SERVICES( ps ) ) )
   {
      if ( debug.on )
      {
         msg(LOG_DEBUG, "tcpmux_handler", "Service name %s not found.",
             svc_name);
      }
      exit(0);
   }

   if( SVC_WAITS( svc ) ) /* Service forks, so close it */
      Sclose(descriptor);

   server.svr_sp = sp;
   server.svr_conn = serp->svr_conn;
   nserp = server_alloc(&server);
   if( SC_IS_INTERNAL( scp ) ) {
      SC_INTERNAL(scp, nserp);
   } else {
      exec_server(nserp);
   }
}

