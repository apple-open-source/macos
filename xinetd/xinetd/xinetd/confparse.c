/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include "config.h"
#include <sys/types.h>
#include <syslog.h>
#include <string.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef HAVE_RPC_RPC_H
#include <rpc/rpc.h>
#endif

#ifdef HAVE_RPC_RPCENT_H
#include <rpc/rpcent.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#include "str.h"
#include "sio.h"
#include "confparse.h"
#include "msg.h"
#include "xconfig.h"
#include "parse.h"
#include "special.h"
#include "sconst.h"
#include "env.h"
#include "sconf.h"
#include "sensor.h"
#include "inet.h"

extern int inetd_compat;

/*
 * Pset iterator used by functions in this file.
 * It lives only when get_configuration is called (i.e. it is created and
 * destroyed each time). This is because the pset it is iterating on
 * changes.
 */
static psi_h iter ;

static status_e fix_server_argv( struct service_config *scp )
{
   char *server_name ;
   const char *func = "fix_server_argv" ;

   if( scp->sc_server == NULL )
   {
      msg( LOG_ERR, func, 
           "Must specify a server in %s", scp->sc_name);
      return( FAILED );
   }
   
   if( SC_NAMEINARGS( scp ) ) {
      if( !SC_SPECIFIED(scp, A_SERVER_ARGS ) ){
         msg( LOG_ERR, func, 
              "Must specify server args if using NAMEINARGS flag");
         return( FAILED );
      }

      return ( OK );
   }

   /*
    * Check if the user specified any server arguments.
    * If not, then the server_argv has not been allocated yet,
    * so malloc it (size 2)
    * Put in argv[ 0 ] the last component of the server pathname
    */
   if ( ! SC_SPECIFIED( scp, A_SERVER_ARGS ) )
   {
      scp->sc_server_argv = (char **) malloc( 2 * sizeof( char * ) ) ;
      if ( scp->sc_server_argv == NULL )
      {
         out_of_memory( func ) ;
         return( FAILED ) ;
      }
      scp->sc_server_argv[ 0 ] = NULL ;
      scp->sc_server_argv[ 1 ] = NULL ;
      SC_PRESENT( scp, A_SERVER_ARGS ) ;
   }

   /*
    * Determine server name
    */
   server_name = strrchr( scp->sc_server, '/' ) ;
   if ( server_name == NULL )
      server_name = scp->sc_server ;
   else
      server_name++ ;      /* skip the '/' */

   /*
    * Place it in argv[ 0 ]
    */
   scp->sc_server_argv[ 0 ] = new_string( server_name ) ;
   if ( scp->sc_server_argv[ 0 ] == NULL )
   {
      out_of_memory( func ) ;
      return( FAILED ) ;
   }
   return( OK ) ;
}



#define USE_DEFAULT( scp, def, attr_id )   \
         ( ! SC_SPECIFIED( scp, attr_id ) && SC_SPECIFIED( def, attr_id ) )

/*
 * Fill the service configuration with attributes that were not
 * explicitly specified. These can be:
 *      1) implied attributes (like the server name in argv[0])
 *      2) attributes from 'defaults' so that we won't need to check
 *         'defaults' anymore.
 *      3) default values (like the service instance limit)
 */
static status_e service_fill( struct service_config *scp, 
                            struct service_config *def )
{
   const char *func = "service_fill" ;

   /* Note: if the service was specified, it won't be honored. */
   if( (scp->sc_redir_addr != NULL) ) {
       if ( SC_SPECIFIED( scp, A_SERVER ) && scp->sc_server)
          free(scp->sc_server);
       scp->sc_server = new_string( "/bin/true" );
       SC_SPECIFY(scp, A_SERVER);
   }

   if ( ! SC_IS_INTERNAL( scp ) && fix_server_argv( scp ) == FAILED )
      return( FAILED ) ;

   /* 
    * FIXME: Should all these set SPECIFY or PRESENT ? 
    * PRESENT makes more sense. Also, these sb documented on manpage. -SG
    */
   if ( ! SC_SPECIFIED( scp, A_INSTANCES ) )
   {
      scp->sc_instances = SC_SPECIFIED( def, A_INSTANCES ) ? def->sc_instances
                                                     : DEFAULT_INSTANCE_LIMIT ;
      SC_PRESENT( scp, A_INSTANCES ) ;
   }

   if ( (! SC_SPECIFIED( scp, A_UMASK )) && SC_SPECIFIED( def, A_UMASK ) ) 
   {
      scp->sc_umask = def->sc_umask;
      SC_SPECIFY( scp, A_UMASK );
   }

   if ( ! SC_SPECIFIED( scp, A_PER_SOURCE ) )
   {
      scp->sc_per_source = SC_SPECIFIED( def, A_PER_SOURCE ) ? def->sc_per_source : DEFAULT_INSTANCE_LIMIT ;
      SC_SPECIFY( scp, A_PER_SOURCE ) ;
   }

   if ( ! SC_SPECIFIED( scp, A_GROUPS ) )
   {
      scp->sc_groups = SC_SPECIFIED( def, A_GROUPS ) ? def->sc_groups : NO;
      SC_SPECIFY( scp, A_GROUPS );
   }

   if ( ! SC_SPECIFIED( scp, A_CPS ) ) 
   {
      scp->sc_time_conn_max = SC_SPECIFIED( def, A_CPS ) ? 
         def->sc_time_conn_max : DEFAULT_LOOP_RATE;
      scp->sc_time_wait = SC_SPECIFIED( def, A_CPS ) ? 
         def->sc_time_wait : DEFAULT_LOOP_TIME;
      scp->sc_time_reenable = 0;
   }

   if ( ! SC_SPECIFIED( scp, A_MAX_LOAD ) ) {
      scp->sc_max_load = SC_SPECIFIED( def, A_MAX_LOAD ) ? def->sc_max_load : 0;
      SC_SPECIFY( scp, A_MAX_LOAD ) ;
   }

   if ( (! SC_SPECIFIED( scp, A_BIND )) && (scp->sc_orig_bind_addr == 0) ) {
      scp->sc_bind_addr = SC_SPECIFIED( def, A_BIND ) ? def->sc_bind_addr : 0 ;
      if (scp->sc_bind_addr != 0)
         SC_SPECIFY( scp, A_BIND ) ;
   }

   if ( ! SC_SPECIFIED( scp, A_V6ONLY ) ) {
      scp->sc_v6only = SC_SPECIFIED( def, A_V6ONLY ) ? def->sc_v6only : NO;
      SC_SPECIFY( scp, A_V6ONLY );
   }

   if ( ! SC_SPECIFIED( scp, A_DENY_TIME ) )
   {
      scp->sc_deny_time = SC_SPECIFIED( def, A_DENY_TIME ) ? 
         def->sc_deny_time  : 0 ;
      SC_SPECIFY( scp, A_DENY_TIME ) ;
   }

   if ( (!SC_IPV4( scp )) && (!SC_IPV6( scp )) )
   {
      /*
       * If bind is specified, check the address and see what family is
       * available. If not, then use default.
       */
      if ( SC_SPECIFIED( scp, A_BIND ) && !scp->sc_orig_bind_addr ) 
      {
	  if ( SAIN6(scp->sc_bind_addr)->sin6_family == AF_INET )
             M_SET(scp->sc_xflags, SF_IPV4);
	  else
             M_SET(scp->sc_xflags, SF_IPV6);
      }
      else
         M_SET(scp->sc_xflags, SF_IPV4);
   }

   if (scp->sc_orig_bind_addr)
   {
      /*
       * If we are here, we have a dual stack machine with multiple
       * entries for a domain name. We can finally use the flags for
       * a hint to see which one to use.
       */
      struct addrinfo hints, *res;

      memset(&hints, 0, sizeof(hints));
      hints.ai_flags = AI_CANONNAME;
      if (SC_IPV6(scp))
         hints.ai_family = AF_INET6;
      else
         hints.ai_family = AF_INET;

      if( getaddrinfo(scp->sc_orig_bind_addr, NULL, &hints, &res) < 0 ) 
      {
         msg(LOG_ERR, func, "bad address given for:%s", scp->sc_name);
         return( FAILED );
      }

      if( (res == NULL) || (res->ai_addr == NULL) ) 
      {
         msg(LOG_ERR, func, "no addresses returned for: %s", scp->sc_name);
	 return( FAILED );
      }

      if( (res->ai_family == AF_INET) || (res->ai_family == AF_INET6) )
      {
         scp->sc_bind_addr = (union xsockaddr *)
            malloc(sizeof(union xsockaddr));
         if( scp->sc_bind_addr == NULL )
         {
            msg(LOG_ERR, func, "can't allocate space for bind addr of:%s",
                scp->sc_name);
            return( FAILED );
         }
         memcpy(scp->sc_bind_addr, res->ai_addr, res->ai_addrlen);
         free(scp->sc_orig_bind_addr);
         scp->sc_orig_bind_addr = 0;
      }
      freeaddrinfo(res);
   }

   /* This should be removed if sock_stream is ever something other than TCP,
    * or sock_dgram is ever something other than UDP.
    */
   if ( (! SC_SPECIFIED( scp, A_PROTOCOL )) && 
	( SC_SPECIFIED( scp, A_SOCKET_TYPE ) ) )
   {
      struct protoent *pep ;

      if( scp->sc_socket_type == SOCK_STREAM ) {
         if( (pep = getprotobyname( "tcp" )) != NULL ) {
            scp->sc_protocol.name = new_string ( "tcp" );
            if( scp->sc_protocol.name == NULL )
               return( FAILED );
            scp->sc_protocol.value = pep->p_proto ;
            SC_SPECIFY(scp, A_PROTOCOL);
         }
      }

      if( scp->sc_socket_type == SOCK_DGRAM ) {
         if( (pep = getprotobyname( "udp" )) != NULL ) {
            scp->sc_protocol.name = new_string ( "udp" );
            if( scp->sc_protocol.name == NULL )
               return( FAILED );
            scp->sc_protocol.value = pep->p_proto ;
            SC_SPECIFY(scp, A_PROTOCOL);
         }
      }
   }
   if ( ( SC_SPECIFIED( scp, A_PROTOCOL )) && 
        (! SC_SPECIFIED( scp, A_SOCKET_TYPE ) ) )
   {
      if( (scp->sc_protocol.name != NULL) && EQ("tcp", scp->sc_protocol.name) )
      {
            scp->sc_socket_type = SOCK_STREAM;
            SC_SPECIFY(scp, A_SOCKET_TYPE);
      }

      if( (scp->sc_protocol.name != NULL) && EQ("udp", scp->sc_protocol.name) )
      {
            scp->sc_socket_type = SOCK_DGRAM;
            SC_SPECIFY(scp, A_SOCKET_TYPE);
      }
   }

   /*
    * Next assign a port based on service name if not specified. Based
    * on the code immediately before this, if either a socket_type or a
    * protocol is specied, the other gets set appropriately. We will only
    * use protocol for this code.
    */
   if (! SC_SPECIFIED( scp, A_PORT ) && SC_SPECIFIED( scp, A_PROTOCOL ) &&
       ! SC_IS_UNLISTED( scp ) && ! SC_IS_MUXCLIENT( scp ) )
   {
       /*
        * Look up the service based on the protocol and service name.
	* If not found, don't worry. Message will be emitted in
	* check_entry().
        */
      struct servent *sep = getservbyname( scp->sc_name, 
                                           scp->sc_protocol.name ) ;
      if ( sep != NULL )
      {
         /* s_port is in network-byte-order */
         scp->sc_port = ntohs(sep->s_port);
         SC_SPECIFY(scp, A_PORT);
      }
   }
   
   if ( USE_DEFAULT( scp, def, A_LOG_ON_SUCCESS ) )
   {
      scp->sc_log_on_success = def->sc_log_on_success ;
      SC_SPECIFY( scp, A_LOG_ON_SUCCESS ) ;
   }

   if ( USE_DEFAULT( scp, def, A_LOG_ON_FAILURE ) )
   {
      scp->sc_log_on_failure = def->sc_log_on_failure ;
      SC_SPECIFY( scp, A_LOG_ON_FAILURE ) ;
   }

   if ( USE_DEFAULT( scp, def, A_LOG_TYPE ) )
   {
      struct log *dlp = SC_LOG( def ) ;
      struct log *slp = SC_LOG( scp ) ;

      switch ( LOG_GET_TYPE( dlp ) )
      {
         case L_NONE:
            LOG_SET_TYPE( slp, L_NONE ) ;
            break ;
         
         case L_SYSLOG:
            *slp = *dlp ;
            break ;
         
         case L_FILE:
            LOG_SET_TYPE( slp, L_COMMON_FILE ) ;
            break ;

         default:
            msg( LOG_ERR, func,
                        "bad log type: %d", (int) LOG_GET_TYPE( dlp ) ) ;
            return( FAILED ) ;
      }
      SC_SPECIFY( scp, A_LOG_TYPE ) ;
   }
   if ( setup_environ( scp, def ) == FAILED )
      return( FAILED ) ;
   return( OK ) ;
}


static void remove_disabled_services( struct configuration *confp )
{
   pset_h disabled_services ;
   pset_h enabled_services ;
   struct service_config *scp ;
   struct service_config *defaults = confp->cnf_defaults ;

   if( SC_SPECIFIED( defaults, A_ENABLED ) ) {
      enabled_services = defaults->sc_enabled ;
      

      /* Mark all the services disabled */
      for ( scp = SCP( psi_start( iter ) ) ; scp ; scp = SCP( psi_next(iter) ) )
         SC_DISABLE( scp );

      /* Enable the selected services */
      for ( scp = SCP( psi_start( iter ) ) ; scp ; scp = SCP( psi_next(iter) ) )
      {
         register char *sid = SC_ID( scp ) ;
         register unsigned u ;

         for ( u = 0 ; u < pset_count( enabled_services ) ; u++ ) {
            if ( EQ( sid, (char *) pset_pointer( enabled_services, u ) ) ) {
               SC_ENABLE( scp );
               break;
            }
         }
      }
   }

   /* Remove any services that are left marked disabled */
   for ( scp = SCP( psi_start( iter ) ) ; scp ; scp = SCP( psi_next(iter)) ){
      if( SC_IS_DISABLED( scp ) ) {
         msg(LOG_DEBUG, "remove_disabled_services", "removing %s", scp->sc_name);
         SC_DISABLE( scp );
         sc_free(scp);
         psi_remove(iter);
      }
   }

   if ( ! SC_SPECIFIED( defaults, A_DISABLED ) )
      return ;
   
   disabled_services = defaults->sc_disabled ;

   for ( scp = SCP( psi_start( iter ) ) ; scp ; scp = SCP( psi_next( iter ) ) )
   {
      register char *sid = SC_ID( scp ) ;
      register unsigned u ;

      for ( u = 0 ; u < pset_count( disabled_services ) ; u++ )
         if ( EQ( sid, (char *) pset_pointer( disabled_services, u ) ) )
         {
            sc_free( scp ) ;
            psi_remove( iter ) ;
            break ;
         }
   }
}


/*
 * Check if all required attributes have been specified
 */
static status_e service_attr_check( const struct service_config *scp )
{
   mask_t         necessary_and_specified ;
   mask_t         necessary_and_missing ;
   mask_t         must_specify = NECESSARY_ATTRS ; /* socket_type & wait */
   int            attr_id ;
   char          *attr_name ;
   const char    *func = "service_attr_check" ;

   /*
    * Determine what attributes must be specified
    */
   if ( ! SC_IS_INTERNAL( scp ) ) 
   {  /* user & server */
      M_OR( must_specify, must_specify, NECESSARY_ATTRS_EXTERNAL ) ;
      if ( SC_IS_UNLISTED( scp ) ) 
      {
         if ( ! SC_IS_MUXCLIENT( scp ) ) /* protocol, & port */
         {
            M_OR( must_specify, must_specify, NECESSARY_ATTRS_UNLISTED ) ;
         }
         else  /* Don't need port for TCPMUX CLIENT */
         {
           M_OR( must_specify, must_specify, NECESSARY_ATTRS_UNLISTED_MUX ) ;
         }
      }
   }

   if ( SC_IS_RPC( scp ) ) 
   {
      M_CLEAR( must_specify, A_PORT ); /* port is already known for RPC */
      /* protocol & rpc_version */
      M_OR( must_specify, must_specify, NECESSARY_ATTRS_RPC ) ;
      if ( SC_IS_UNLISTED( scp ) ) /* rpc_number */
         M_OR( must_specify, must_specify, NECESSARY_ATTRS_RPC_UNLISTED ) ;
   }
   else
   {
      if ( SC_SPECIFIED( scp, A_REDIR ) )
         M_CLEAR( must_specify, A_SERVER ); /* server isn't used */
   }

   if( SC_IPV4( scp ) && SC_IPV6( scp ) ) {
      msg( LOG_ERR, func, 
         "Service %s specified as both IPv4 and IPv6 - DISABLING", 
	 SC_NAME(scp));
      return FAILED ;
   }
   
   /*
    * Check if all necessary attributes have been specified
    *
    * NOTE: None of the necessary attributes can belong to "defaults"
    *         This is why we use the sc_attributes_specified mask instead
    *         of the sc_attributes_present mask.
    */

   M_AND( necessary_and_specified,
                  scp->sc_specified_attributes, must_specify ) ;
   M_XOR( necessary_and_missing, necessary_and_specified, must_specify ) ;

   if( (scp->sc_redir_addr != NULL) ) {
#define DEFAULT_SERVER "/bin/true"
       scp->sc_server = new_string( DEFAULT_SERVER );
       SC_PRESENT(scp, A_SERVER);
   }

   if ( M_ARE_ALL_CLEAR( necessary_and_missing) )
      return OK ;

   /*
    * Print names of missing attributes
    */
   for ( attr_id = 0 ; attr_id < SERVICE_ATTRIBUTES ; attr_id++ )
      if ( M_IS_SET( necessary_and_missing, attr_id ) && 
                  ( attr_name = attr_name_lookup( attr_id ) ) != NULL )
      {
         msg( LOG_ERR, func,
            "Service %s missing attribute %s - DISABLING", 
	    scp->sc_id, attr_name ) ;
      }
   return FAILED ;
}


/*
 * Perform validity checks on the whole entry. At this point, all
 * attributes have been read and we can do an integrated check that
 * all parameters make sense.
 *
 * Also does the following:
 *      1. If this is an internal service, it finds the function that
 *         implements it
 *      2. For RPC services, it finds the program number
 *      3. For non-RPC services, it finds the port number.
 */
static status_e check_entry( struct service_config *scp, 
                             const struct configuration *confp )
{
   const char *func = "check_entry" ;
   unsigned int u;
   const pset_h sconfs = CNF_SERVICE_CONFS( confp ) ;

   /*
    * Make sure the service id is unique
    */
   for ( u = 0 ; u < pset_count( sconfs ) ; u++ ) 
   {
      int diff = 1;
      const struct service_config *tmp_scp = SCP( pset_pointer( sconfs, u ) );
      if (tmp_scp == scp)
         break; /* Don't check ourselves, or anything after us */
      if ( EQ( tmp_scp->sc_id, scp->sc_id ) )
      { 
         diff = 0;
      }
      if( tmp_scp->sc_bind_addr == NULL)
         continue; /* problem entry, skip it */
      if ( (scp->sc_port != tmp_scp->sc_port) || 
           (scp->sc_protocol.value != tmp_scp->sc_protocol.value) )
         continue; /* if port or protocol are different, its OK */
      if (scp->sc_bind_addr != NULL)
      {
         if (scp->sc_bind_addr->sa.sa_family != 
             tmp_scp->sc_bind_addr->sa.sa_family)
            continue;
         if (scp->sc_bind_addr->sa.sa_family == AF_INET)
         {
            if (memcmp(&scp->sc_bind_addr->sa_in.sin_addr, 
                       &tmp_scp->sc_bind_addr->sa_in.sin_addr, 
                       sizeof(struct in_addr) ) )
               continue;
         }
         else /* We assume that all bad address families are weeded out */
         {
            if (memcmp(&scp->sc_bind_addr->sa_in6.sin6_addr, 
                       &tmp_scp->sc_bind_addr->sa_in6.sin6_addr, 
                       sizeof(struct in6_addr) ) )
               continue;
         }
      } 
      if( SC_IS_DISABLED( tmp_scp ) ||
               SC_IS_DISABLED(scp) ) 
      {
         /* 
          * Allow multiple configs, as long as all but one are
          * disabled.
          */
         continue;
      }
      if (diff) 
         msg( LOG_ERR, func, 
           "service:%s id:%s is unique but its identical to service:%s id:%s "
	   "- DISABLING",
           scp->sc_name, scp->sc_id, tmp_scp->sc_name, tmp_scp->sc_id ) ;
      else
         msg( LOG_ERR, func, 
           "service:%s id:%s not unique or is a duplicate - DISABLING",
           scp->sc_name, scp->sc_id ) ;
      return FAILED ;
   } /* for */
   
   /*
    * Currently, we cannot intercept:
    *      1) internal services
    *      2) multi-threaded services
    * We clear the INTERCEPT flag without disabling the service.
    */
   if ( SC_IS_INTERCEPTED( scp ) )
   {
      if ( SC_IS_INTERNAL( scp ) )
      {
         msg( LOG_ERR, func,
            "Internal services cannot be intercepted: %s ", scp->sc_id ) ;
         M_CLEAR( scp->sc_xflags, SF_INTERCEPT ) ;
      }
      if ( scp->sc_wait == NO )
      {
         msg( LOG_ERR, func,
            "Multi-threaded services cannot be intercepted: %s", scp->sc_id ) ;
         M_CLEAR( scp->sc_xflags, SF_INTERCEPT ) ;
      }
   }
   
   /* Steer the lost sheep home */
   if ( SC_SENSOR( scp ) )
      M_SET( scp->sc_type, ST_INTERNAL );

   if ( SC_IS_INTERNAL( scp ) )
   {   /* If SENSOR flagged redirect to internal builtin function. */ 
      if ( SC_SENSOR( scp ) )
      {
	 init_sensor();
         scp->sc_builtin =
            builtin_find( "sensor", scp->sc_socket_type );
      }
      else
         scp->sc_builtin =
            builtin_find( scp->sc_name, scp->sc_socket_type );
      if (scp->sc_builtin == NULL )
         return( FAILED ) ;
   }

/* #ifndef NO_RPC */
#if defined(HAVE_RPC_RPCENT_H) || defined(HAVE_NETDB_H)
   if ( SC_IS_RPC( scp ) && !SC_IS_UNLISTED( scp ) )
   {
      struct rpcent *rep = (struct rpcent *)getrpcbyname( scp->sc_name ) ;

      if ( rep == NULL )
      {
         msg( LOG_ERR, func, "unknown RPC service: %s", scp->sc_name ) ;
         return( FAILED ) ;
      }
      SC_RPCDATA( scp )->rd_program_number = rep->r_number ;
   }
   else
#endif   /* ! NO_RPC */
   {
       if ( !SC_IS_UNLISTED( scp ) ) 
       { 
          uint16_t service_port ;
          struct servent *sep ;
  
          /*
           * Check if a protocol was specified. Based on the code in 
	   * service_fill, if either socket_type or protocol is specified,
	   * the other one is filled in. Protocol should therefore always
	   * be filled in unless they made a mistake. Then verify it is the
	   * proper protocol for the given service. 
           * We don't need to check MUXCLIENTs - they aren't in /etc/services.
           */
          if ( SC_SPECIFIED( scp, A_PROTOCOL ) && ! SC_IS_MUXCLIENT( scp ) )
          {
             sep = getservbyname( scp->sc_name, scp->sc_protocol.name ) ;
             if ( (sep == NULL) )
             {
                msg( LOG_ERR, func, 
                   "service/protocol combination not in /etc/services: %s/%s",
                   scp->sc_name, scp->sc_protocol.name ) ;
                return( FAILED ) ;
             }
          }
          else
          {
             msg( LOG_ERR, func,
                "A protocol or a socket_type must be specified for service:%s.",
                scp->sc_name ) ;
             return( FAILED ) ;
          }
 
          /* s_port is in network-byte-order */
          service_port = ntohs(sep->s_port);
 
          /*
           * If a port was specified, it must be the right one
           */
          if ( SC_SPECIFIED( scp, A_PORT ) && 
               scp->sc_port != service_port )
          {
             msg( LOG_ERR, func, "Service %s expects port %d, not %d",
                  scp->sc_name, service_port, scp->sc_port ) ;
             return( FAILED ) ;
          }
       } /* if not unlisted */
    }
    if ( SC_SPECIFIED( scp, A_REDIR ))
    {
       if ( SC_SOCKET_TYPE( scp ) != SOCK_STREAM )
       {
          msg( LOG_ERR, func, 
 	      "Only tcp sockets are supported for redirected service %s",
 	      scp->sc_name);
          return FAILED;
       }
       if ( SC_WAITS( scp ) )
       {
          msg( LOG_ERR, func, 
 	      "Redirected service %s must not wait", scp->sc_name);
          return FAILED;
       }
       if ( SC_NAMEINARGS( scp ) )
       {
          msg( LOG_ERR, func, 
 	      "Redirected service %s should not have NAMEINARGS flag set", 
	      scp->sc_name);
          return FAILED;
       }
    }
    
   if ( SC_NAMEINARGS(scp) )
   {
      if (SC_IS_INTERNAL( scp ) )
      {
         msg( LOG_ERR, func, 
              "Service %s is INTERNAL and has NAMEINARGS flag set", 
	      scp->sc_name );
         return FAILED;
      }
      else if (!SC_SPECIFIED( scp, A_SERVER_ARGS) )
      {
         msg( LOG_ERR, func, 
              "Service %s has NAMEINARGS flag set and no server_args", 
	      scp->sc_name );
         return FAILED;
      }
   }

   if ( service_attr_check( scp ) == FAILED )
      return( FAILED ) ;

   return( OK ) ;
}

/*
 * Get a configuration from the specified file.
 */
static status_e get_conf( int fd, struct configuration *confp )
{
   parse_conf_file( fd, confp ) ;
   parse_end() ;
   return( OK ) ;
}


#define CHECK_AND_CLEAR( scp, mask, mask_name )                               \
   if ( M_IS_SET( mask, LO_USERID ) )                                         \
   {                                                                          \
      msg( LOG_WARNING, func,                                                 \
      "%s service: clearing USERID option from %s", scp->sc_id, mask_name ) ; \
      M_CLEAR( mask, LO_USERID ) ;                                            \
   }

/*
 * Get a configuration by reading the configuration file.
 */
status_e cnf_get( struct configuration *confp )
{
   int config_fd ;
   struct service_config *scp ;
   const char *func = "get_configuration" ;

   if ( cnf_init( confp, &config_fd, &iter ) == FAILED )
      return( FAILED ) ;

   else if ( get_conf( config_fd, confp ) == FAILED )
   {
      Sclose( config_fd ) ;
      cnf_free( confp ) ;
      psi_destroy( iter ) ;
      return( FAILED ) ;
   }

   Sclose( config_fd ) ;
   if( inetd_compat ) {
      config_fd = open("/etc/inetd.conf", O_RDONLY);
      if( config_fd >= 0 ) {
         parse_inet_conf_file( config_fd, confp );
         parse_end() ;
         Sclose(config_fd);
      }
   }

   remove_disabled_services( confp ) ;

   for ( scp = SCP( psi_start( iter ) ) ; scp ; scp = SCP( psi_next( iter ) ) )
   {
      /*
       * Fill the service configuration from the defaults.
       * We do this so that we don't have to look at the defaults any more.
       */
      if ( service_fill( scp, confp->cnf_defaults ) == FAILED )
      {
         sc_free( scp ) ;
         psi_remove( iter ) ;
         continue ;
      }

      if ( check_entry( scp, confp ) == FAILED )
      {
         sc_free( scp ) ;
         psi_remove( iter ) ;
         continue ;
      }

      /*
       * If the INTERCEPT flag is set, change this service to an internal 
       * service using the special INTERCEPT builtin.
       */
      if ( SC_IS_INTERCEPTED( scp ) )
      {
         const builtin_s *bp ;

         bp = spec_find( INTERCEPT_SERVICE_NAME, scp->sc_socket_type ) ;
         if ( bp == NULL )
         {
            msg( LOG_ERR, func, "removing service %s", SC_ID( scp ) ) ;
            sc_free( scp ) ;
            psi_remove( iter ) ;
            continue ;
         }

         scp->sc_builtin = bp ;
         M_SET( scp->sc_type, ST_INTERNAL ) ;
      }

      /*
       * Clear the USERID flag for the identity service because
       * it may lead to loops (for example, remote xinetd issues request,
       * local xinetd issues request to remote xinetd etc.)
       * We identify the identity service by its (protocol,port) combination.
       */
      if ( scp->sc_port == IDENTITY_SERVICE_PORT && 
                                       scp->sc_protocol.value == IPPROTO_TCP )
      {
         CHECK_AND_CLEAR( scp, scp->sc_log_on_success, "log_on_success" ) ;
         CHECK_AND_CLEAR( scp, scp->sc_log_on_failure, "log_on_failure" ) ;
      }
   }

   psi_destroy( iter ) ;

   if ( debug.on && debug.fd != -1 )
      cnf_dump( confp, debug.fd ) ;

   endservent() ;
   endprotoent() ;
#ifndef NO_RPC
   endrpcent() ;
#endif
   return( OK ) ;
}

