/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <syslog.h>
#include <string.h>
#include <sys/types.h>

#include "pset.h"
#include "env.h"
#include "msg.h"

extern char **environ ;
env_h std_env ;            /* created from environ */

status_e initenv(void)
{
   std_env = env_make( environ ) ;
   return( ( std_env == NULL ) ? FAILED : OK ) ;
}

static status_e make_env_with_strings(struct environment *, env_h, pset_h);
static status_e make_env_from_vars(struct environment *, env_h, pset_h) ;
static status_e update_env_with_strings(env_h, pset_h) ;


status_e setup_environ( struct service_config *scp, struct service_config *def )
{
   struct environment   *ep = SC_ENV( scp ) ;

   if ( ! SC_SPECIFIED( scp, A_PASSENV ) )
   {
      if ( ! SC_SPECIFIED( def, A_PASSENV ) )
      {
         if ( ! SC_SPECIFIED( scp, A_ENV ) )
         {
            ep->env_type = STD_ENV ;
            ep->env_handle = std_env ;
            return( OK ) ;
         }
         else
            return( 
               make_env_with_strings( ep, std_env, scp->sc_env_var_defs ) ) ;
      }
      else   /* SC_SPECIFIED( def, A_PASSENV ) */
      {
         struct environment *dep = SC_ENV( def ) ;

         if ( dep->env_type == NO_ENV &&
                  make_env_from_vars( dep, std_env,
                                 def->sc_pass_env_vars ) == FAILED )
            return( FAILED ) ;

         if ( ! SC_SPECIFIED( scp, A_ENV ) )
         {
            ep->env_type = DEF_ENV ;
            ep->env_handle = dep->env_handle ;
            return( OK ) ;
         }
         else
            return( make_env_with_strings( ep, 
                           dep->env_handle, scp->sc_env_var_defs ) ) ;
      }
   }
   else   /* SC_SPECIFIED( scp, A_PASSENV ) */
   {
      if ( make_env_from_vars( ep, std_env, scp->sc_pass_env_vars ) == FAILED )
         return( FAILED ) ;

      if ( ! SC_SPECIFIED( scp, A_ENV ) )
         return( OK ) ;
      else
      {
         if ( update_env_with_strings( 
                     ep->env_handle, scp->sc_env_var_defs ) == FAILED )
         {
            env_destroy( ep->env_handle ) ;
            return( FAILED ) ;
         }
         return( OK ) ;
      }
   }
}


/*
 * Create a new environment from environ and env_strings
 * env_strings contains strings of the form "var=value"
 */
static status_e make_env_with_strings( struct environment *ep, 
                                        env_h env, 
                                        pset_h env_strings )
{
   env_h      new_env ;
   const char *func = "make_env_with_strings" ;

   if ( ( new_env = env_create( env ) ) == ENV_NULL )
   {
      out_of_memory( func ) ;
      return( FAILED ) ;
   }

   if ( update_env_with_strings( new_env, env_strings ) == FAILED )
   {
      env_destroy( new_env ) ;
      return( FAILED ) ;
   }

   ep->env_type = CUSTOM_ENV ;
   ep->env_handle = new_env ;
   return( OK ) ;
}


static status_e make_env_from_vars( struct environment *ep, 
                                     env_h env, 
                                     pset_h vars )
{
   env_h               new_env ;
   char               *varname ;
   unsigned            u ;
   const char         *func = "make_env_from_vars" ;

   if ( ( new_env = env_create( ENV_NULL ) ) == ENV_NULL )
   {
      out_of_memory( func ) ;
      return( FAILED ) ;
   }
   
   for ( u = 0 ; u < pset_count( vars ) ; u++ )
   {
      varname = (char *) pset_pointer( vars, u ) ;
      if ( env_addvar( new_env, env, varname ) == ENV_ERR )
         switch ( env_errno )
         {
            case ENV_EBADVAR:
               msg( LOG_ERR, func, "Unknown variable %s", varname ) ;
               break ;
            
            case ENV_ENOMEM:
               out_of_memory( func ) ;
               env_destroy( new_env ) ;
               return( FAILED ) ;
         }
   }

   ep->env_type = CUSTOM_ENV ;
   ep->env_handle = new_env ;
   return( OK ) ;
}


static status_e update_env_with_strings( env_h env, pset_h strings )
{
   unsigned u ;
   const char *func = "update_env_with_strings" ;

   for ( u = 0 ; u < pset_count( strings ) ; u++ )
   {
      char *p = (char *) pset_pointer( strings, u ) ;

      if ( env_addstr( env, p ) == ENV_ERR )
         switch ( env_errno )
         {
            case ENV_ENOMEM:
               out_of_memory( func ) ;
               return( FAILED ) ;
            
            case ENV_EBADSTRING:
               msg( LOG_ERR, func, "Bad environment string: %s", p ) ;
               break ;
         }
   }
   return( OK ) ;
}

