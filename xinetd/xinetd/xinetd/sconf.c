/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include "config.h"
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#include "str.h"
#include "sio.h"
#include "sconf.h"
#include "timex.h"
#include "addr.h"
#include "nvlists.h"


#define NEW_SCONF()               NEW( struct service_config )
#define FREE_SCONF( scp )         FREE( scp )

/*
 * Conditional free; checks if the pointer is NULL
 */
#define COND_FREE( x )           if ( x )                   \
                                 {                          \
                                    *x = NUL ;              \
                                    free( (char *) x ) ;    \
                                    x = NULL ;              \
                                 }


/*
 * Allocate a new service_config and initialize the service name field
 * with 'name'; the rest of the fields are set to 0 which gives them
 * their default values.
 */
struct service_config *sc_alloc( char *name )
{
   struct service_config *scp ;
   const char *func = "sc_alloc" ;

   scp = NEW_SCONF() ;
   if ( scp == NULL )
   {
      out_of_memory( func ) ;
      return( NULL ) ;
   }
   CLEAR( *scp ) ;
   scp->sc_name = new_string( name ) ;
   return( scp ) ;
}


static void release_string_pset( pset_h pset )
{
   pset_apply( pset, free, NULL ) ;
   pset_destroy( pset ) ;
}


/*
 * Free all malloc'ed memory for the specified service
 */
void sc_free( struct service_config *scp )
{
   COND_FREE( scp->sc_name ) ;
   COND_FREE( scp->sc_id ) ;
   COND_FREE( scp->sc_protocol.name ) ;
   COND_FREE( scp->sc_server ) ;
   COND_FREE( (char *)scp->sc_bind_addr ) ;
   if ( scp->sc_server_argv )
   {
      char **pp ;

      /*
       * argv[ 0 ] is a special case because it may not have been allocated yet
       */
      if ( scp->sc_server_argv[ 0 ] != NULL)
         free( scp->sc_server_argv[ 0 ] ) ;
      for ( pp = &scp->sc_server_argv[ 1 ] ; *pp != NULL ; pp++ )
         free( *pp ) ;
      free( (char *) scp->sc_server_argv ) ;
   }
   COND_FREE( LOG_GET_FILELOG( SC_LOG( scp ) )->fl_filename ) ;

   if ( scp->sc_access_times != NULL )
   {
      ti_free( scp->sc_access_times ) ;
      pset_destroy( scp->sc_access_times ) ;
   }

   if ( scp->sc_only_from != NULL )
   {
      addrlist_free( scp->sc_only_from ) ;
      pset_destroy( scp->sc_only_from ) ;
   }

   if ( scp->sc_no_access != NULL )
   {
      addrlist_free( scp->sc_no_access ) ;
      pset_destroy( scp->sc_no_access ) ;
   }

   if ( scp->sc_env_var_defs != NULL )
      release_string_pset( scp->sc_env_var_defs ) ;
   if ( scp->sc_pass_env_vars != NULL )
      release_string_pset( scp->sc_pass_env_vars ) ;
   if ( SC_ENV( scp )->env_type == CUSTOM_ENV && 
                                    SC_ENV( scp )->env_handle != ENV_NULL )
      env_destroy( SC_ENV( scp )->env_handle ) ;
   
   CLEAR( *scp ) ;
   FREE_SCONF( scp ) ;
}


/*
 * Create a configuration for one of the special services
 */
struct service_config *sc_make_special( const char *service_name, 
                                        const builtin_s *bp, 
                                        int instances )
{
   char *name ;
   struct service_config *scp ;
   const char *func = "sc_make" ;

   name = new_string( service_name ) ;
   if ( name == NULL )
   {
      out_of_memory( func ) ;
      return( NULL ) ;
   }

   if ( ( scp = sc_alloc( name ) ) == NULL )
   {
      free( name ) ;
      return( NULL ) ;
   }

   scp->sc_id = new_string( scp->sc_name ) ;
   if ( scp->sc_id == NULL )
   {
      free( name ) ;
      out_of_memory( func ) ;
      return( NULL ) ;
   }
   SC_SPECIFY( scp, A_ID ) ;

   /*
    * All special services are internal
    */
   M_SET( scp->sc_type, ST_SPECIAL ) ;
   M_SET( scp->sc_type, ST_INTERNAL ) ;
   scp->sc_builtin = bp ;
   SC_SPECIFY( scp, A_TYPE ) ;

   M_SET( scp->sc_xflags, SF_NORETRY ) ;
   SC_SPECIFY( scp, A_FLAGS ) ;

   scp->sc_instances = instances ;
   SC_SPECIFY( scp, A_INSTANCES ) ;

   scp->sc_wait = NO ;
   SC_SPECIFY( scp, A_WAIT ) ;

   return( scp ) ;
}


static void dump_log_data( int fd, struct service_config *scp, int tab_level )
{
   struct log *lp = SC_LOG( scp ) ;
   struct filelog *flp ;
   int i ;

   switch ( LOG_GET_TYPE( lp ) )
   {
      case L_NONE:
         tabprint( fd, tab_level, "No logging\n" ) ;
         return ;

      case L_COMMON_FILE:
         tabprint( fd, tab_level, "Logging to common log file\n" ) ;
         break ;

      case L_FILE:
         flp = LOG_GET_FILELOG( lp ) ;
         tabprint( fd, tab_level, "Logging to file: %s", flp->fl_filename ) ;

         if ( FILELOG_SIZE_CONTROL( flp ) )
            Sprint( fd, " (soft=%d hard=%d)\n",
                        flp->fl_soft_limit, flp->fl_hard_limit ) ;
         else
            Sprint( fd, " (no limits)\n" ) ;
         break ;
      
      case L_SYSLOG:
         tabprint( fd, tab_level,
            "Logging to syslog. Facility = %s, level = %s\n",
               nv_get_name( syslog_facilities, lp->l_sl.sl_facility ),
               nv_get_name( syslog_levels, lp->l_sl.sl_level ) ) ;
         break ;
   }

   tabprint( fd, tab_level, "Log_on_success flags =" ) ;
   for ( i = 0 ; success_log_options[ i ].name != NULL ; i++ )
      if ( M_IS_SET( scp->sc_log_on_success, success_log_options[ i ].value ) )
         Sprint( fd, " %s", success_log_options[ i ].name ) ;
   Sputchar( fd, '\n' ) ;

   tabprint( fd, tab_level, "Log_on_failure flags =" ) ;
   for ( i = 0 ; failure_log_options[ i ].name != NULL ; i++ )
      if ( M_IS_SET( scp->sc_log_on_failure, failure_log_options[ i ].value ) )
         Sprint( fd, " %s", failure_log_options[ i ].name ) ;
   Sputchar( fd, '\n' ) ;
}


/*
 * Print info about service scp to file descriptor fd
 */
void sc_dump( struct service_config *scp, 
              int fd, 
              int tab_level, 
              bool_int is_defaults )
{
   const struct name_value    *nvp ;
   unsigned             u ;
   char                 **pp ;

   if ( is_defaults )
      tabprint( fd, tab_level, "Service defaults\n" ) ;
   else
      tabprint( fd, tab_level, "Service configuration: %s\n", scp->sc_name ) ;

   if ( ! is_defaults )
   {
      tabprint( fd, tab_level+1, "id = %s\n", scp->sc_id ) ;

      if ( ! M_ARE_ALL_CLEAR( scp->sc_xflags ) )
      {
         tabprint( fd, tab_level+1, "flags =" ) ;
         for ( nvp = &service_flags[ 0 ] ; nvp->name != NULL ; nvp++ )
            if ( M_IS_SET( scp->sc_xflags, nvp->value ) )
               Sprint( fd, " %s", nvp->name ) ;
         Sputchar( fd, '\n' ) ;
      }

      if ( ! M_ARE_ALL_CLEAR( scp->sc_type ) )
      {
         tabprint( fd, tab_level+1, "type =" ) ;
         for ( nvp = &service_types[ 0 ] ; nvp->name != NULL ; nvp++ )
            if ( M_IS_SET( scp->sc_type, nvp->value ) )
               Sprint( fd, " %s", nvp->name ) ;
         Sputchar( fd, '\n' ) ;
      }

      tabprint( fd, tab_level+1, "socket_type = %s\n",
         nv_get_name( socket_types, scp->sc_socket_type ) ) ;

      tabprint( fd, tab_level+1, "Protocol (name,number) = (%s,%d)\n",
            scp->sc_protocol.name, scp->sc_protocol.value ) ;
      
      if ( SC_SPECIFIED( scp, A_PORT ) )
         tabprint( fd, tab_level+1, "port = %d\n", scp->sc_port ) ;
   }

   if ( SC_SPECIFIED( scp, A_INSTANCES ) ) {
      if ( scp->sc_instances == UNLIMITED )
         tabprint( fd, tab_level+1, "Instances = UNLIMITED\n" ) ;
      else
         tabprint( fd, tab_level+1, "Instances = %d\n", scp->sc_instances ) ;
   }

   if ( SC_SPECIFIED( scp, A_UMASK ) )
      tabprint( fd, tab_level+1, "umask = %o\n", scp->sc_umask ) ;
      
   if ( SC_SPECIFIED( scp, A_NICE ) )
      tabprint( fd, tab_level+1, "Nice = %d\n", scp->sc_nice ) ;

   if ( SC_SPECIFIED( scp, A_GROUPS ) )
   {
      if (scp->sc_groups == 1)
         tabprint( fd, tab_level+1, "Groups = yes\n" );
      else
         tabprint( fd, tab_level+1, "Groups = no\n" );
   }

   if ( SC_SPECIFIED( scp, A_CPS ) )
      tabprint( fd, tab_level+1, "CPS = max conn:%lu wait:%lu\n", 
         scp->sc_time_conn_max, scp->sc_time_wait );

   if ( SC_SPECIFIED( scp, A_PER_SOURCE ) )
      tabprint( fd, tab_level+1, "PER_SOURCE = %d\n", 
         scp->sc_per_source );

   if ( SC_SPECIFIED( scp, A_BIND ) && scp->sc_bind_addr ) {
      char bindname[NI_MAXHOST];
      int len = 0;
      if( scp->sc_bind_addr->sa.sa_family == AF_INET ) 
         len = sizeof(struct sockaddr_in);
      else  
         len = sizeof(struct sockaddr_in6);
      memset(bindname, 0, sizeof(bindname));
      if( getnameinfo(&scp->sc_bind_addr->sa, len, bindname, NI_MAXHOST, 
            NULL, 0, 0) != 0 ) 
         strcpy(bindname, "unknown");
      tabprint( fd, tab_level+1, "Bind = %s\n", bindname );
   }
   else
      tabprint( fd, tab_level+1, "Bind = All addresses.\n" );

   if ( ! is_defaults )
   {
      if ( (! SC_IS_INTERNAL( scp )) && (scp->sc_redir_addr == NULL) )
      {
         tabprint( fd, tab_level+1, "Server = %s\n", scp->sc_server ) ;
         tabprint( fd, tab_level+1, "Server argv =" ) ;
	 if ( scp->sc_server_argv )
	 {
            for ( pp = scp->sc_server_argv ; *pp ; pp++ )
               Sprint( fd, " %s", *pp ) ;
	 }
	 else
	    Sprint( fd, " (NULL)");
         Sputchar( fd, '\n' ) ;
      } 

      if ( scp->sc_redir_addr != NULL ) 
      {
         char redirname[NI_MAXHOST];
         int len = 0;
         if( scp->sc_redir_addr->sa.sa_family == AF_INET ) 
            len = sizeof(struct sockaddr_in);
         if( scp->sc_redir_addr->sa.sa_family == AF_INET6 ) 
            len = sizeof(struct sockaddr_in6);
         memset(redirname, 0, sizeof(redirname));
         if( getnameinfo(&scp->sc_redir_addr->sa, len,  redirname, NI_MAXHOST, 
               NULL, 0, 0) != 0 ) 
            strcpy(redirname, "unknown");
         tabprint( fd, tab_level+1, "Redirect = %s:%d\n", redirname, 
	    scp->sc_redir_addr->sa_in.sin_port );
      }

      if ( SC_IS_RPC( scp ) )
      {
         struct rpc_data *rdp = SC_RPCDATA( scp ) ;

         tabprint( fd, tab_level+1, "RPC data\n" ) ;
         tabprint( fd, tab_level+2,
                           "program number = %ld\n", rdp->rd_program_number ) ;
         tabprint( fd, tab_level+2, "rpc_version = " ) ;
         if ( rdp->rd_min_version == rdp->rd_max_version )
            Sprint( fd, "%ld\n", rdp->rd_min_version ) ;
         else
            Sprint( fd, "%ld-%ld\n",
                           rdp->rd_min_version, rdp->rd_max_version ) ;
      }

      if ( SC_SPECIFIED( scp, A_ACCESS_TIMES ) )
      {
         tabprint( fd, tab_level+1, "Access times =" ) ;
         ti_dump( scp->sc_access_times, fd ) ;
         Sputchar ( fd, '\n' ) ;
      }
   }

   /* This is important enough that each service should list it. */
   tabprint( fd, tab_level+1, "Only from: " ) ;
   if ( scp->sc_only_from )
   {  /* Next check is done since -= doesn't zero out lists. */
      if ( pset_count(scp->sc_only_from) == 0)
         Sprint( fd, "All sites" );
      else
         addrlist_dump( scp->sc_only_from, fd ) ;
   }
   else
      Sprint( fd, "All sites" );
   Sputchar( fd, '\n' ) ;

   /* This is important enough that each service should list it. */
   tabprint( fd, tab_level+1, "No access: " ) ;
   if ( scp->sc_no_access )
   {  /* Next check is done since -= doesn't zero out lists. */
      if ( pset_count(scp->sc_no_access) == 0)
         Sprint( fd, "No blocked sites" );
      else
         addrlist_dump( scp->sc_no_access, fd ) ;
   }
   else
      Sprint( fd, "No blocked sites" );
   Sputchar( fd, '\n' ) ;

   if ( SC_SENSOR(scp) )
   {
      tabprint( fd, tab_level+1, "Deny Time: " ) ;
      Sprint( fd, "%d\n", scp->sc_deny_time);
   }
   
   dump_log_data( fd, scp, tab_level+1 ) ;

   if ( SC_IS_PRESENT( scp, A_PASSENV ) )
   {
      tabprint( fd, tab_level+1, "Passenv =" ) ;
      for ( u = 0 ; u < pset_count( scp->sc_pass_env_vars ) ; u++ )
         Sprint( fd, " %s",
                  (char *) pset_pointer( scp->sc_pass_env_vars, u ) ) ;
      Sputchar ( fd, '\n' ) ;
   }

   if ( ! is_defaults )
      if ( SC_SPECIFIED( scp, A_ENV ) )
      {
         tabprint( fd, tab_level+1, "Environment additions:\n" ) ;
         for ( u = 0 ; u < pset_count( scp->sc_env_var_defs ) ; u++ )
            tabprint( fd, tab_level+2,
                  "%s\n", (char *) pset_pointer( scp->sc_env_var_defs, u ) ) ;
      }
   
   if ( SC_ENV( scp )->env_type == CUSTOM_ENV )
   {
      tabprint( fd, tab_level+1, "Environment strings:\n" ) ;
      for ( pp = env_getvars( SC_ENV( scp )->env_handle ) ; *pp ; pp++ )
         tabprint( fd, tab_level+2, "%s\n", *pp ) ;
   }
   Sflush( fd ) ;
}


#define SC_RPCPROGNUM( s )    RD_PROGNUM( SC_RPCDATA( s ) )
#define SAME_RPC( s1, s2 )    ( SC_RPCPROGNUM( s1 ) == SC_RPCPROGNUM( s2 ) )
#define SAME_NONRPC( s1, s2 ) ( (s1)->sc_socket_type == (s2)->sc_socket_type \
                                 && (s1)->sc_port == (s2)->sc_port )

/*
 * Two service configurations are considered different if any of the
 * following is TRUE:
 *      1) only one is unlisted
 *      2) only one is internal
 *      3) only one is RPC
 *      4) they have different values for the 'wait' attribute
 *      5) they use different protocols
 *      6) they are both RPC services but have different program numbers
 *      7) neither is an RPC service and they have different socket_types or
 *         use diffent ports
 *
 * This function returns TRUE if the specified configurations are different.
 *
 * Note that this function is closely related to the 'readjust' function
 * that is invoked on reconfiguration; that function will not change 
 * attributes that this function checks to determine if two configurations
 * are different.
 */
bool_int sc_different_confs( struct service_config *scp1, 
                             struct service_config *scp2 )
{
   if ( SC_IS_UNLISTED( scp1 ) != SC_IS_UNLISTED( scp2 ) ||
            SC_IS_INTERNAL( scp1 ) != SC_IS_INTERNAL( scp2 ) ||
               SC_IS_RPC( scp1 ) != SC_IS_RPC( scp2 ) )
      return( TRUE ) ;

   if ( scp1->sc_wait != scp2->sc_wait )
      return( TRUE ) ;
  
   if ( scp1->sc_protocol.value != scp2->sc_protocol.value )
      return( TRUE ) ;

   if ( SC_IS_RPC( scp1 ) )
   {
      if ( ! SAME_RPC( scp1, scp2 ) )
         return( TRUE ) ;
   }
   else
   {
      if ( ! SAME_NONRPC( scp1, scp2 ) )
         return( TRUE ) ;
   }
   return( FALSE ) ;
}

