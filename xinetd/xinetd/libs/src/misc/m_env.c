/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <memory.h>
#include <string.h>
#include <stdlib.h>

#include "m_env.h"
#include "str.h"


typedef struct __env env_s ;

#define INITIAL_VARS            20
#define INCREASE                10


int env_errno ;

static char **lookup( const env_h env, const char *var, 
	register unsigned int len );

static env_s *alloc_env( unsigned max_vars )
{
   env_s *ep ;
   unsigned size ;
   char **pointers ;

   ep = (env_s *) malloc( sizeof( env_s ) ) ;
   if ( ep == ENV_NULL )
   {
      env_errno = ENV_ENOMEM ;
      return( ENV_NULL ) ;
   }

   memset( ep, 0, sizeof( env_s ) );
   size = ( max_vars + 1 ) * sizeof( char * ) ;
   pointers = (char **) malloc( size ) ;
   if ( pointers == NULL )
   {
      free( (char *)ep ) ;
      env_errno = ENV_ENOMEM ;
      return( ENV_NULL ) ;
   }
   (void) memset( (char *)pointers, 0, size ) ;

   ep->vars = pointers ;
   ep->max_vars = max_vars ;
   ep->n_vars = 0 ;
   return( ep ) ;
}


env_h env_create( const env_h init_env )
{
   unsigned u ;
   env_s *ep ;
   unsigned max_vars ;

   if ( init_env == ENV_NULL )
      max_vars = INITIAL_VARS ;
   else
      max_vars = init_env->n_vars + 5 ;
   
   ep = alloc_env( max_vars ) ;
   if ( ep == NULL )
   {
      env_errno = ENV_ENOMEM ;
      return( ENV_NULL ) ;
   }

   if ( init_env == ENV_NULL )
      return( ep ) ;

   for ( u = 0, ep->n_vars = 0 ; u < init_env->n_vars ; u++, ep->n_vars++ )
   {
      ep->vars[ ep->n_vars ] = new_string( init_env->vars[ u ] ) ;
      if ( ep->vars[ ep->n_vars ] == NULL )
      {
         env_destroy( ep ) ;
         env_errno = ENV_ENOMEM ;
         return( ENV_NULL ) ;
      }
   }
   return( ep ) ;
}


void env_destroy( env_h env )
{
   unsigned u ;

   for ( u = 0 ; u < env->n_vars ; u++ )
      free( env->vars[ u ] ) ;
   free( (char *)env->vars ) ;
   free( (char *)env ) ;
}


env_h env_make( char **env_strings )
{
   env_s *ep ;
   char **pp ;

   for ( pp = env_strings ; *pp ; pp++ ) ;

   ep = alloc_env( (unsigned) (pp-env_strings) ) ;
   if ( ep == NULL )
   {
      env_errno = ENV_ENOMEM ;
      return( ENV_NULL ) ;
   }

   for ( pp = env_strings ; *pp ; pp++ )
   {
      char *p = new_string( *pp ) ;

      if ( p == NULL )
      {
         env_destroy( ep ) ;
         env_errno = ENV_ENOMEM ;
         return( ENV_NULL ) ;
      }
      ep->vars[ ep->n_vars++ ] = p ;
   }
   return( ep ) ;
}


char *env_lookup( env_h env, const char *var )
{
   char **pp = lookup( env, var, strlen( var ) ) ;

   return( ( pp == NULL ) ? NULL : *pp ) ;
}


static char **lookup( const env_h env, const char * var, 
	register unsigned int len )
{
   char **pp ;

   for ( pp = env->vars ; *pp ; pp++ )
      if ( strncmp( *pp, var, len ) == 0 && (*pp)[ len ] == '=' )
         return( pp ) ;
   return( NULL ) ;
}


static int grow( env_s *ep )
{
   char **new_vars ;
   unsigned new_max_vars ;
   unsigned new_size ;
   new_max_vars = ep->max_vars + INCREASE ;
   new_size = ( new_max_vars+1 ) * sizeof( char * ) ;
   new_vars = (char **) realloc( (char *)ep->vars, new_size ) ;
   if ( new_vars == NULL )
      return( ENV_ERR ) ;

   ep->vars = new_vars ;
   ep->max_vars = new_max_vars ;
   
   memset(&ep->vars[ep->n_vars], 0, new_size - (ep->n_vars * sizeof(char *)));
   return( ENV_OK ) ;
}


/*
 * Add the variable string to the given environment.
 */
static int addstring( env_s *ep, const char *var_string, unsigned int len )
{
   char **pp ;
   char *p ;

   p = new_string( var_string ) ;
   if ( p == NULL )
      return( ENV_ERR ) ;
      
   pp = lookup( ep, var_string, len ) ;
   if ( pp == NULL )
   {
      if ( ep->n_vars >= ep->max_vars && grow( ep ) == ENV_ERR )
      {
         free( p ) ;
         env_errno = ENV_ENOMEM ;
         return( ENV_ERR ) ;
      }
      ep->vars[ ep->n_vars++ ] = p ;
      ep->vars[ ep->n_vars ] = NULL;
   }
   else
   {
      free( *pp ) ;
      *pp = p ;
   }
   return( ENV_OK ) ;
}


int env_addvar( env_h env, env_h from_env, char *var_name )
{
   char *var_string = env_lookup( from_env, var_name ) ;

   if ( var_string == NULL )
   {
      env_errno = ENV_EBADVAR ;
      return( ENV_ERR ) ;
   }

   return( addstring( env, var_string, strlen( var_name ) ) ) ;
}


int env_addstr( env_h env, char *var_string )
{
   char *p = strchr( var_string, '=' ) ;

   if ( p == NULL )
   {
      env_errno = ENV_EBADSTRING ;
      return( ENV_ERR ) ;
   }

   return( addstring( env, var_string, (unsigned int)(p-var_string) ) ) ;
}


#ifdef notdef
static int comparator( char **p1, char **p2 )
{
   return( strcmp( *p1, *p2 ) ) ;
}


void env_sort( env_h env )
{
   qsort( (char *)env->vars, env->n_vars, sizeof( char * ), comparator ) ;
}
#endif

