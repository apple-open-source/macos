/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef __M_ENV_H
#define __M_ENV_H

/*
 * $Id: m_env.h,v 1.1.1.2 2002/06/17 22:51:06 zarzycki Exp $
 */

struct __env
{
   unsigned max_vars ;
   unsigned n_vars ;
   char **vars ;
} ;

typedef struct __env *env_h ;

#define ENV_NULL                  ((env_h)0)

/*
 * Return values
 */
#define ENV_ERR                   (-1)
#define ENV_OK                    0

/*
 * Error codes
 */
#define ENV_ENOMEM                1
#define ENV_EBADVAR               2
#define ENV_EBADSTRING            3


env_h env_create ( env_h ) ;
void env_destroy ( env_h ) ;
env_h env_make ( char **env_strings ) ;
int env_addvar ( env_h, env_h from_env, char *var ) ;
int env_addstr ( env_h, char *str ) ;
char *env_lookup ( env_h, const char *var ) ;

#define env_getvars( env )            (env)->vars

extern int env_errno ;

#endif   /* __M_ENV_H */

