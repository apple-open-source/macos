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
#include <signal.h>
#include <syslog.h>
#include <setjmp.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>


#include "str.h"
#include "ident.h"
#include "msg.h"
#include "server.h"
#include "connection.h"
#include "util.h"
#include "log.h"
#include "sconst.h"


static char *get_line( int sd, register char *buf, unsigned bufsize );
static char *verify_line( char *line, unsigned local_port, unsigned remote_port );


#define IBUFSIZE      1024      /* RFC-1413 suggests 1000 */

#define START_TIMER( t )   (void) alarm( t )
#define STOP_TIMER()      (void) alarm( 0 )



static sigjmp_buf env ;

#ifdef __GNUC__
__attribute__ ((noreturn))
#endif
static void sigalrm_handler(int signum)
{
   siglongjmp( env, 1 ) ;
}


/*
 * This function always runs in a forked process.
 */
idresult_e log_remote_user( const struct server *serp, unsigned timeout )
{
   static char         buf[ IBUFSIZE ] ;
   int                 cc ;
   union xsockaddr     sin_local, sin_remote, sin_contact, sin_bind;
   volatile unsigned   local_port;
   volatile unsigned   remote_port;
   int                 sd ;
   int                 sin_len ;
   char               *p ;
   const char         *func = "log_remote_user" ;

   if ( timeout && signal( SIGALRM, sigalrm_handler ) == SIG_ERR )
   {
      msg( LOG_ERR, func, "signal: %m" ) ;
      return( IDR_ERROR ) ;
   }

   /*
    * Determine local and remote addresses
    */
   sin_len = sizeof( sin_local ) ;
   if ( getsockname( SERVER_FD( serp ), &sin_local.sa, &sin_len ) == -1 )
   {
      msg( LOG_ERR, func, "(%d) getsockname: %m", getpid() ) ;
      return( IDR_ERROR ) ;
   }

   if ( CONN_XADDRESS( SERVER_CONNECTION( serp ) ) == NULL )
   {
      /*
       * This shouldn't happen since identification only works for
       * connection-based services.
       */
      msg( LOG_ERR, func, "connection has no address" ) ;
      return( IDR_ERROR ) ;
   }

   CLEAR( sin_contact );
   sin_remote = *CONN_XADDRESS( SERVER_CONNECTION( serp ) ) ;
   sin_contact = sin_remote;
   memcpy( &sin_bind, &sin_local, sizeof(sin_bind) ) ;
   local_port = 0;
   remote_port = 0;
   if( sin_remote.sa.sa_family == AF_INET ) {
      local_port = ntohs( sin_local.sa_in6.sin6_port ) ;
      remote_port = ntohs( sin_remote.sa_in6.sin6_port ) ;
      sin_contact.sa_in6.sin6_port = htons( IDENTITY_SERVICE_PORT ) ;
      sin_bind.sa_in.sin_port = 0 ;
   } else if( sin_remote.sa.sa_family == AF_INET6 ) {
      local_port = ntohs( sin_local.sa_in.sin_port ) ;
      remote_port = ntohs( sin_remote.sa_in.sin_port ) ;
      sin_contact.sa_in.sin_port = htons( IDENTITY_SERVICE_PORT ) ;
      sin_bind.sa_in6.sin6_port = 0 ;
   }

   /*
    * Create a socket, bind it, and set the close-on-exec flag on the
    * descriptor. We set the flag in case we are called as part of a 
    * successful attempt to start a server (i.e. execve will follow). 
    * The socket must be bound to the receiving address or ident might 
    * fail for multi-homed hosts.
    */
   sd = socket( sin_remote.sa.sa_family, SOCK_STREAM, 0 ) ;
   if ( sd == -1 )
   {
      msg( LOG_ERR, func, "socket creation: %m" ) ;
      return( IDR_ERROR ) ;
   }
   if ( bind(sd, &sin_bind.sa, sizeof(sin_bind.sa)) == -1 )
   { 
      msg( LOG_ERR, func, "socket bind: %m" ) ;
      (void) Sclose( sd ) ;
      return( IDR_ERROR ) ;
   }
   if ( fcntl( sd, F_SETFD, FD_CLOEXEC ) == -1 )
   {
      msg( LOG_ERR, func, "fcntl F_SETFD: %m" ) ;
      (void) Sclose( sd ) ;
      return( IDR_ERROR ) ;
   }

   if ( timeout ) {
      if ( sigsetjmp( env, 1 ) == 0 )
         START_TIMER( timeout ) ;
      else {
         Sclose( sd ) ;
         return( IDR_TIMEDOUT ) ;
      }
   }

   if ( connect( sd, &sin_contact.sa, sizeof( sin_contact ) ) == -1 )
   {
      if ( timeout ) {
         STOP_TIMER() ;
         signal ( SIGALRM, SIG_DFL ) ;
      }
      Sclose( sd );
      return( IDR_NOSERVER ) ;
   }

   cc = strx_nprint( buf, sizeof( buf ), 
      "%d,%d\r\n", remote_port, local_port ) ;
   if ( write_buf( sd, buf, cc ) == FAILED )
   {
      if ( timeout ) {
         STOP_TIMER() ;
         signal ( SIGALRM, SIG_DFL ) ;
      }
      Sclose( sd );
      return( IDR_ERROR ) ;
   }

   p = get_line( sd, buf, sizeof( buf ) ) ;

   if ( timeout ) {
      STOP_TIMER() ;
      signal ( SIGALRM, SIG_DFL ) ;
   }

   if ( p == NULL ) {
      Sclose( sd );
      return( IDR_RESPERR ) ;
   }
   
   /*
    * Verify that the received line is OK
    */
   if ( ( p = verify_line( buf, local_port, remote_port ) ) == NULL )
   {
      msg(LOG_ERR, func, "Bad line received from identity server at %s: %s",
         xaddrname( &sin_remote ), buf ) ;
      Sclose( sd );
      return( IDR_BADRESP ) ;
   }

   svc_logprint( SERVER_CONNSERVICE( serp ), USERID_ENTRY, "%s", p ) ;
   return( IDR_OK ) ;
}


static char *verify_line( char *line, 
                           unsigned local_port, 
                           unsigned remote_port )
{
   char   *p ;
   char   *start = line ;
   int     port;

   /*
    * Verify port numbers
    */
   p = strchr( start, ',' ) ;
   if ( p == NULL )
      return( NULL ) ;
   *p = NUL ;
   if ( parse_base10( start, &port ) || port != remote_port ) {
      *p = ',';
      return( NULL ) ;
   }
   *p = ',' ;
   
   start = p+1 ;
   p = strchr( start, ':' ) ;
   if ( p == NULL )
      return( NULL ) ;
   *p = NUL ;
   if ( parse_base10( start, &port ) || port != local_port ) {
      *p = ':';
      return( NULL ) ;
   }
   *p = ':';
   
   /*
    * Look for the 'USERID' string
    */
   {
      const char *line_id = "USERID" ;
      unsigned int line_id_len = strlen( line_id ) ;

      start = p+1 ;
      for ( p = start ; isspace( *p ) ; p++ ) ;
      if ( *p == NUL )
         return( NULL ) ;
      start = p ;
      if ( strncmp( start, line_id, line_id_len ) != 0 )
         return( NULL ) ;
      start += line_id_len ;      /* skip it */
   }

   for ( p = start ; isspace( *p ) ; p++ ) ;      /* skip any white-space */
   if ( *p != ':' )
      return( NULL ) ;
   for ( p++ ; isspace( *p ) ; p++ ) ;
   if ( *p == NUL )
      return( NULL ) ;
   return( p ) ;
}
   

/*
 * Get a line terminated by CR-LF.
 * Replace the CR-LF with NUL.
 */
static char *get_line( int sd, char *buf, unsigned bufsize )
{
   int     size ;
   int     cc ;
   char   *p ;
   char   *s ;
   const char *func = "get_line" ;

   for ( p = buf, size = bufsize ; size > 0 ; p += cc, size -= cc )
   {
      cc = read( sd, p, size ) ;
      if ( cc == -1 ) {
         if ( errno == EINTR )
         {
            cc = 0 ;
            continue ;
         }
         else
         {
            msg( LOG_ERR, func, "read: %m" ) ;
            return( CHAR_NULL ) ;
         }
      }

      if ( cc == 0 )
      {
         msg( LOG_ERR, func, "identd server reply missing ending CR-LF" ) ;
         return( CHAR_NULL ) ;
      }
      for ( s = p ; s < p + cc ; s++ )
      {
         if ( (*s == '\n') && (s != buf) && (*(s-1) == '\r') )
         {
            *(s-1) = NUL ;
            return( buf ) ;
         }
      }
   }
   msg( LOG_ERR, func, "Too much input from identity server" ) ;
   return( CHAR_NULL ) ;
}


const char *idresult_explain( idresult_e result )
{
   const char *reason = "UNKNOWN" ;

   switch ( result )
   {
      case IDR_OK:
         reason = "no error" ;
         break ;

      case IDR_NOSERVER:
         reason = "no server" ;
         break ;

      case IDR_TIMEDOUT:
         reason = "timeout" ;
         break ;
      
      case IDR_ERROR:
         reason = "system error" ;
         break ;
      
      case IDR_RESPERR:
         reason = "error while receiving response" ;
         break ;
      
      case IDR_BADRESP:
         reason = "bad response" ;
         break ;
   }
   return( reason ) ;
}

