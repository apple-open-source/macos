/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef BUILTINS_H
#define BUILTINS_H

/*
 * $Id: builtins.h,v 1.1.1.4 2003/05/22 01:16:36 rbraun Exp $
 */

#include "defs.h"


#define FORK            YES
#define NO_FORK         NO

struct builtin
{
   voidfunc    b_handler ;             /* builtin service handler           */
   boolean_e   b_fork_server ;         /* whether a server must be forked   */
} ;

typedef struct builtin builtin_s ;

/*
 * All builtins are invoked with a struct server argument
 */
#define BUILTIN_HANDLER( bp )          ( (bp)->b_handler ) 
#define BUILTIN_INVOKE( bp, serp )     (*(bp)->b_handler)( serp )
#define BUILTIN_FORKS( bp )            ( (bp)->b_fork_server == YES )


struct builtin_service
{
   const char  *bs_name ;               /* for identification purposes      */
   int         bs_socket_type ;         /* for identification purposes      */
   builtin_s   bs_handle ;
} ;

const builtin_s *builtin_find(const char *service_name,int type);
const builtin_s *builtin_lookup(const struct builtin_service services[],const char *service_name,int type);

#endif   /* BUILTIN_H */
