/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#ifndef NO_SYSLOG
#include <syslog.h>
#endif

#include "xlog.h"
#include "impl.h"
#include "slog.h"
#include "str.h"
#include <stdarg.h>

#define MSGBUFSIZE         2048


static int syslog_init(xlog_s *, va_list) ;
static void syslog_fini(xlog_s *) ;
static int syslog_control(xlog_s *, xlog_cmd_e, va_list ) ;
static int syslog_write(xlog_s *, const char buf[], int , int , va_list ) ;
static int syslog_parms(xlog_e, va_list) ;

static struct syslog_parms parms =
{
      0,
#ifndef NO_SYSLOG
      LOG_PID + LOG_NOWAIT,
      LOG_USER,
#else
      0,
      0,
#endif
      "XLOG",
} ;


struct xlog_ops __xlog_syslog_ops =
   {
      syslog_init,
      syslog_fini,
      syslog_write,
      syslog_control,
      syslog_parms
   } ;

#ifdef NO_SYSLOG

/*
 * Notice that the following functions will never be invoked since
 * the xlog_* functions will not call them. However, we need to define
 * them so that we don't have any unresolved references; and we define 
 * them without any arguments.
 */
static void syslog()
{
}

static void openlog()
{
}

static void closelog()
{
}

#endif   /* NO_SYSLOG */


/*
 * Expected arguments:
 *      facility, level
 */
static int syslog_init( xlog_s *xp, va_list ap )
{
   struct syslog_parms   *slp = &parms ;
   struct syslog_s                 *sp ;

   sp = NEW( struct syslog_s ) ;
   if ( sp == NULL )
      return( XLOG_ENOMEM ) ;
   sp->sl_facility = va_arg( ap, int ) ;
   sp->sl_default_level = va_arg( ap, int ) ;
   if ( slp->slp_n_xlogs++ == 0 )
      openlog( slp->slp_ident, slp->slp_logopts, slp->slp_facility ) ;
   xp->xl_data = sp ;
   return( XLOG_ENOERROR ) ;
}


static void syslog_fini( xlog_s *xp )
{
   if ( --parms.slp_n_xlogs == 0 )
      closelog() ;
   free( SYSLOG( xp ) ) ;
   xp->xl_data = NULL ;
}


static int syslog_control( xlog_s *xp, xlog_cmd_e cmd, va_list ap )
{
   switch ( cmd )
   {
      case XLOG_LEVEL:
         SYSLOG( xp )->sl_default_level = va_arg( ap, int ) ;
         break ;

      case XLOG_FACILITY:
         SYSLOG( xp )->sl_facility = va_arg( ap, int ) ;
         break ;
      
      case XLOG_PREEXEC:
         closelog() ;
         break ;

      case XLOG_POSTEXEC:
         if ( parms.slp_n_xlogs )
            openlog( parms.slp_ident, parms.slp_logopts, parms.slp_facility ) ;
         break ;

      /* These fall through ? */
      case XLOG_LINK:
      case XLOG_CALLBACK:
      case XLOG_GETFLAG:
      case XLOG_SETFLAG: 
      case XLOG_SIZECHECK:
      case XLOG_GETFD:
      case XLOG_LIMITS:
         break ;
   }
   return( XLOG_ENOERROR ) ;
}


static int syslog_write( xlog_s *xp, const char buf[], int len, int flags, va_list ap )
{
   int   level ;
   int   syslog_arg ;
   char   prefix[ MSGBUFSIZE ] ;
   int   prefix_size = sizeof( prefix ) ;
   int   prefix_len = 0 ;
   int   cc ;
   int   percent_m_pos ;
   int   action_flags = ( flags | xp->xl_flags ) ;

   if ( flags & XLOG_SET_LEVEL )
      level = va_arg( ap, int ) ;
   else
      level = SYSLOG( xp )->sl_default_level ;
   syslog_arg = SYSLOG( xp )->sl_facility + level ;

   if ( action_flags & XLOG_PRINT_ID )
   {
      cc = strx_nprint( &prefix[ prefix_len ], prefix_size, "%s: ",
                     xp->xl_id ) ;
      prefix_len += cc ;
      prefix_size -= cc ;
   }

   if ( ( action_flags & XLOG_NO_ERRNO ) ||
                  ( percent_m_pos = __xlog_add_errno( buf, len ) ) == -1 )
      syslog( syslog_arg, "%.*s%.*s", prefix_len, prefix, len, buf ) ;
   else
   {
      char *ep ;
      char errno_buf[ 100 ] ;
      unsigned size = sizeof( errno_buf ) ;
      
      ep = __xlog_explain_errno( errno_buf, &size ) ;
      syslog( syslog_arg, "%.*s%.*s%.*s%.*s",
            prefix_len, prefix,
               percent_m_pos, buf,
                  (int)size, ep,
                     len - percent_m_pos - 2, buf + percent_m_pos + 2 ) ;
   }
   return( XLOG_ENOERROR ) ;
}


static int syslog_parms( xlog_e type, va_list ap )
{
   char *id = NULL;

   id = __xlog_new_string( va_arg( ap, char * ) );
   if ( id == NULL )
      return( XLOG_ENOMEM ) ;
   parms.slp_ident = id ;
   parms.slp_logopts = va_arg( ap, int ) ;
   parms.slp_facility = va_arg( ap, int ) ;
   return( XLOG_ENOERROR ) ;
}

