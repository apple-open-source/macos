/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include "config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>

#include "special.h"
#include "server.h"
#include "msg.h"
#include "sconst.h"
#include "int.h"
#include "util.h"
#include "nvlists.h"
#include "service.h"
#include "state.h"
#include "main.h"
#include "connection.h"
#include "sconf.h"
#include "options.h"
#include "xconfig.h"
#include "ident.h"


static void stream_logging( struct server *) ;

static const struct builtin_service special_services[] =
   {
      { LOG_SERVICE_NAME,        SOCK_STREAM,   { stream_logging,    FORK   } },
      { INTERCEPT_SERVICE_NAME,  SOCK_STREAM,   { intercept,         FORK   } },
      { INTERCEPT_SERVICE_NAME,  SOCK_DGRAM,    { intercept,         FORK   } },
      { NULL,                    0,             { NULL,              0      } }
   } ;


const builtin_s *spec_find( const char *service_name, int type )
{
   const builtin_s *bp ;
   const struct name_value *nvp ;
   const char *func = "spec_find" ;

   if ( (bp = builtin_lookup( special_services, service_name, type )) )
      return( bp ) ;

   nvp = nv_find_name( socket_types, type ) ;
   if ( nvp == NULL )
   {
      msg( LOG_ERR, func, "unknown socket type: %d", type ) ;
      return( NULL ) ;
   }

   msg( LOG_ERR, func,
            "special service %s,%s not supported", service_name, nvp->name ) ;
   return( NULL ) ;
}


status_e spec_service_handler( struct service *sp, connection_s *cp )
{
   return(server_run( sp, cp ));
}


static struct service *spec_setup( const char *name, int socket_type, 
                                   int instances )
{
   const builtin_s *bp ;
   struct service_config *scp ;

   bp = spec_find( name, socket_type ) ;
   if ( bp == NULL )
      return( NULL ) ;

   if ( ( scp = sc_make_special( name, bp, instances ) ) == NULL )
      return( NULL ) ;

   return( svc_make_special( scp ) ) ;
}


/*
 * Initialize the special services and the corresponding entries in
 * the program state structure.
 */
void spec_include(void)
{
   int instances ;

   instances = logprocs_option ? logprocs_option_arg : DEFAULT_LOGPROCS ;
   LOG_SERVICE( ps ) = spec_setup( LOG_SERVICE_NAME, SOCK_STREAM, instances ) ;
}


static void stream_logging( struct server *serp )
{
   const char *func = "stream_logging" ;
   idresult_e result ;

#ifdef DEBUG_LOGGING
   if ( debug.on )
   {
      msg( LOG_DEBUG, func, "%d is sleeping", getpid() ) ;
      sleep( 10 ) ;
   }
#endif

   result = log_remote_user( serp, LOGUSER_FAILURE_TIMEOUT ) ;
   if ( (result != IDR_OK) && (result != IDR_NOSERVER) )
      msg( LOG_ERR, func, "Failed to contact identity server at %s: %s", conn_addrstr( SERVER_CONNECTION( serp ) ), idresult_explain( result ) ) ;
}

