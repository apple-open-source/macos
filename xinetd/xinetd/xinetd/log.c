/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include "config.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#include "str.h"
#include "log.h"
#include "sconf.h"
#include "sconst.h"
#include "msg.h"


#define LOGBUF_SIZE                  1024

static char ipv6_ret[NI_MAXHOST];

const char *xaddrname(const union xsockaddr *inaddr)
{
   unsigned int len = 0;
   if( inaddr->sa.sa_family == AF_INET )  len = sizeof(struct sockaddr_in);
   if( inaddr->sa.sa_family == AF_INET6 ) len = sizeof(struct sockaddr_in6);
   memset(ipv6_ret, 0, sizeof(ipv6_ret));
   if( getnameinfo(&inaddr->sa, len, ipv6_ret, sizeof(ipv6_ret), NULL, 
         0, NI_NUMERICHOST) )
      strncpy(ipv6_ret, "<unknown>", NI_MAXHOST);
   return ipv6_ret;
}

uint16_t xaddrport(const union xsockaddr *inaddr)
{
   if( inaddr->sa.sa_family == AF_INET )  return inaddr->sa_in.sin_port;
   if( inaddr->sa.sa_family == AF_INET6 ) return inaddr->sa_in6.sin6_port;
   return 0;
}

static int log_common(mask_t *, char *, int, const connection_s *) ;

/*
 * This function writes log records of the form:
 *
 *      START: service [pid] [from_address]
 */
void svc_log_success( struct service *sp, const connection_s *cp, pid_t pid )
{
   char                    buf[ LOGBUF_SIZE ] ;
   int                     bufsize ;
   struct service_config   *scp = SVC_CONF( sp ) ;
   int                     len ;
   int                     cc ;

   if ( ! SVC_LOGS_ON_SUCCESS( sp ) )
      return ;
   
   bufsize = sizeof( buf ) ;
   len = 0 ;
   
   cc = strx_nprint( buf, bufsize, "%s: %s", START_ENTRY, SC_ID( scp ) ) ;
   len += cc ;
   bufsize -= cc ;

   if ( SC_LOGS_PID( scp ) )
   {
      cc = strx_nprint( &buf[ len ], bufsize, " pid=%d", pid ) ;
      len += cc ;
      bufsize -= cc ;
   }

   cc = log_common( &SC_LOG_ON_SUCCESS( scp ), &buf[len], bufsize, cp ) ;
   len += cc ;
   bufsize -= cc ;

   xlog_write( sp->svc_log, buf, len, XLOG_NO_ERRNO ) ;
}


/*
 * This function writes log records of the form:
 *
 *      FAIL: service failure-type [from_address]
 *
 */
void svc_log_failure( struct service *sp, 
                      const connection_s *cp, 
                      access_e access_failure )
{
   char                    buf[ LOGBUF_SIZE ] ;
   int                     bufsize ;
   struct service_config   *scp = SVC_CONF( sp ) ;
   int                     len = 0 ;
   int                     cc ;
   
   if ( ! SVC_LOGS_ON_FAILURE( sp ) )
      return ;
   
   bufsize = sizeof( buf ) ;
   cc = strx_nprint( buf, bufsize, "%s: %s", FAIL_ENTRY, SC_ID( scp ) ) ;
   len += cc ;
   bufsize -= cc ;

   cc = strx_nprint( &buf[ len ], bufsize,
                        " %s", ACCESS_EXPLAIN( access_failure ) ) ;
   len += cc ;
   bufsize -= cc ;

   cc = log_common( &SC_LOG_ON_FAILURE( scp ), &buf[ len ], bufsize, cp ) ;
   len += cc ;
   bufsize -= cc ;

   xlog_write( sp->svc_log, buf, len, XLOG_NO_ERRNO ) ;
}



static int log_common( mask_t *logmask, 
                        char *buf, 
                        int bufsize, 
                        const connection_s *cp )
{
   int len = 0 ;

   if ( M_IS_SET( *logmask, LO_HOST ) )
      len = strx_nprint( buf, bufsize, " from=%s", conn_addrstr( cp ) ) ;
   return( len ) ;
}


void svc_log_exit( struct service *sp, const struct server *serp )
{
   char                    buf[ LOGBUF_SIZE ] ;
   int                     bufsize ;
   int                     cc ;
   int                     len ;
   int                     exit_status = SERVER_EXITSTATUS( serp ) ;
   struct service_config   *scp = SVC_CONF( sp ) ;
   const char                    *func = "log_exit" ;

   if ( ! SVC_LOGS_ON_EXIT( sp ) )
      return ;

   bufsize = sizeof( buf ) ;
   len = 0 ;

   cc = strx_nprint( buf, bufsize, "%s: %s", EXIT_ENTRY, SC_ID( scp ) ) ;
   bufsize -= cc ;
   len += cc ;

   /*
    * If the EXIT flag was used, log the exit status or the signal that
    * killed the process. We assume that these are the only reasons
    * for process termination.
    */
   if ( SC_LOGS_EXITS( scp ) )
   {
      int num  = 0;
      const char *s ;

      if ( PROC_EXITED( exit_status ) )
      {
         s = "status" ;
         num = PROC_EXITSTATUS( exit_status ) ;
      }
      else if ( PROC_SIGNALED( exit_status ) )
      {
         s = "signal" ;
         num = PROC_TERMSIG( exit_status ) ;
      }
      else
      {
         msg( LOG_ERR, func, "Bad exit status" ) ;
         s = NULL ;
      }

      if ( s )
      {
         cc = strx_nprint( &buf[ len ], bufsize, " %s=%d", s, num ) ;
         len += cc ;
         bufsize -= cc ;
      }
   }

   if ( SC_LOGS_PID( scp ) )
   {
      cc = strx_nprint( &buf[ len ], bufsize, " pid=%d", SERVER_PID( serp ) ) ;
      len += cc ;
      bufsize -= cc ;
   }

   if ( SC_LOGS_DURATION( scp ) )
   {
      time_t current_time ;

      (void) time( &current_time ) ;
      cc = strx_nprint( &buf[ len ], bufsize, " duration=%ld(sec)",
         (long)(current_time - SERVER_STARTTIME( serp )) ) ;
      len += cc ;
      bufsize -= cc ;
   }
   xlog_write( sp->svc_log, buf, len, XLOG_NO_ERRNO ) ;
}



/*
 * Used by other parts of xinetd that want to log something without
 * going through the proper channels (i.e. log_{success,failure} and log_exit)
 */
/* VARARGS3 */
void svc_logprint( struct service *sp, const char *line_id, 
                   const char *fmt, ...)
{
   char     buf[ LOGBUF_SIZE ] ;
   int      bufsize = sizeof( buf ) ;
   int      len ;
   int      cc ;
   va_list  ap ;

   if ( ! SVC_IS_LOGGING( sp ) )
      return ;

   len = strx_nprint( buf, bufsize, "%s: %s ", line_id, SVC_ID( sp ) ) ;
   va_start( ap, fmt ) ;
   cc = strx_nprintv( &buf[ len ], bufsize-len, fmt, ap ) ;
   va_end( ap ) ;
   xlog_write( sp->svc_log, buf, len+cc, XLOG_NO_ERRNO | XLOG_NO_SIZECHECK ) ;
}

