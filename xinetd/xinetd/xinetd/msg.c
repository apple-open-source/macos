/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <syslog.h>
#include <fcntl.h>
#include <stdarg.h>

#include "xlog.h"
#include "str.h"
#include "msg.h"
#include "defs.h"
#include "options.h"
#include "xconfig.h"
#include "util.h"
#include "nvlists.h"
#include "main.h"
#include "parse.h"

static const struct name_value priorities[] =
   {
      { "WARNING",   LOG_WARNING  },
      { "ERROR",     LOG_ERR      },
      { "CRITICAL",  LOG_CRIT     },
      { "NOTICE",    LOG_NOTICE   },
      { "DEBUG",     LOG_DEBUG    },
      { "INFO",      LOG_INFO     },
      { NULL,        1            },
      { "UNKNOWN",   0            }
   } ;

#define BUFSIZE                     2048

#define DEFAULT_SYSLOG_LEVEL         LOG_INFO

char *msg_init(void)
{
   xlog_h      xh ;
   int         fd ;
   xlog_e      type_of_xlog ;
   bool_int    facility_error = FALSE ;
   const char *func            = "msg_init" ;

   if ( debug.on )
   {
      type_of_xlog = XLOG_FILELOG ;
      xh = xlog_create( type_of_xlog, program_name, XLOG_NOFLAGS,
                                 "/dev/tty", O_APPEND + O_WRONLY, 0 ) ;
      debug.fd = -1 ;
   }
   else
   {
      if ( filelog_option )
      {
         type_of_xlog = XLOG_FILELOG ;
         xh = xlog_create( type_of_xlog, program_name,
                  XLOG_PRINT_ID + XLOG_PRINT_PID,
                     filelog_option_arg, LOG_OPEN_FLAGS, LOG_FILE_MODE ) ;
      }
      else
      {
         int facility = DEFAULT_SYSLOG_FACILITY ;
         
         if ( syslog_option )
         {
            const struct name_value *nvp ;

            nvp = nv_find_value( syslog_facilities, syslog_option_arg ) ;
            if ( nvp != NULL )
               facility = nvp->value ;
            else
               facility_error = TRUE ;
         }

         type_of_xlog = XLOG_SYSLOG ;
         xh = xlog_create( type_of_xlog, program_name, XLOG_NOFLAGS,
                                       facility, DEFAULT_SYSLOG_LEVEL ) ;
      }
   }

   if ( xh == NULL )
   {
      /*
       * This simply returns the most likely reason for failure.
       * We can't do any better since xlog_create does not return an
       * error code.
       */
      if ( type_of_xlog == XLOG_SYSLOG )
         return( "can't connect to syslog" ) ;
      else if ( type_of_xlog == XLOG_FILELOG )
         return( "can't open log file" ) ;
      else
         return( "unknown reason" ) ;
   }
   
   /*
    * XXX:   we shouldn't have to check the xlog type.
    *         xlog_control should not succeed if the undelying logging
    *         object does not support the XLOG_GETFD operation.
    */
   if ( type_of_xlog == XLOG_FILELOG &&
            xlog_control( xh, XLOG_GETFD, &fd ) == XLOG_ENOERROR )
   {
      if ( fcntl( fd, F_SETFD, 1 ) == -1 )
      {
         xlog_destroy( xh ) ;
         return( "can't set close-on-exec flag of log file" ) ;
      }
      if ( debug.on )
         debug.fd = fd ;
   }
   
   ps.rws.program_log = xh ;

   if ( facility_error )
      msg( LOG_ERR, func, "Bad syslog facility: %s", syslog_option_arg ) ;
   return( CHAR_NULL ) ;
}


void msg_suspend(void)
{
   (void) xlog_control( ps.rws.program_log, XLOG_PREEXEC ) ;
}


void msg_resume(void)
{
   (void) xlog_control( ps.rws.program_log, XLOG_POSTEXEC ) ;
}


/*
 * The size argument is a value-result argument
 */
static int
#ifdef __GNUC__
__attribute__ ((format (printf, 5, 0)))
#endif
prepare_buffer( int level, 
                const char *func, 
                char *buf, 
                unsigned size, 
                const char *fmt, 
                va_list ap )
{
   int         cc ;
   char        *bufstart   = buf ;
   unsigned    bytes_left  = size ;

   /*
    * Check if we need to print the level name
    */
   if ( debug.on || filelog_option )
   {
      cc = strx_nprint( bufstart, bytes_left,
                                 "%s: ", nv_get_name( priorities, level ) ) ;
      bufstart += cc ;
      bytes_left -= cc ;
   }

   /*
    * Check if we need to print the function name
    */
   if ( debug.on || level == LOG_CRIT )
   {
      cc = strx_nprint( bufstart, bytes_left, "{%s} ", func ) ;
      bufstart += cc ;
      bytes_left -= cc ;
   }

   cc = strx_nprintv( bufstart, bytes_left, fmt, ap ) ;

   bytes_left -= cc ;

   return( size - bytes_left ) ;
}


/* VARARGS3 */
void msg( int level, const char *func, const char *fmt, ...)
{
   va_list   ap ;
   char      buf[ BUFSIZE ] ;
   int      len ;

   va_start( ap, fmt ) ;
   len = prepare_buffer( level, func, buf, sizeof( buf ), fmt, ap ) ;
   va_end( ap ) ;

   xlog_write( ps.rws.program_log, buf, len, XLOG_SET_LEVEL, level ) ;
}


/*
 * Parser message.
 * There are 2 differences from msg():
 *      1) parsemsg() prints the line #
 *      2) parsemsg() does not interpret %m
 */
/* VARARGS3 */
void parsemsg( int msg_level, const char *func, const char *fmt, ...)
{
   va_list   ap ;
   char      buf[ BUFSIZE ] ;
   int       cc ;
   int       len ;

   va_start( ap, fmt ) ;
   len = prepare_buffer( msg_level, func, buf, sizeof( buf ), fmt, ap ) ;
   va_end( ap ) ;

   cc = strx_nprint( &buf[ len ], sizeof(buf)-len, " [line=%d]", line_count ) ;
   len += cc ;

   xlog_write( ps.rws.program_log, buf, len, 
         XLOG_NO_ERRNO + XLOG_SET_LEVEL, msg_level ) ;
}

