/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef __STRPARSE_H
#define __STRPARSE_H

/*
 * $Id: strparse.h,v 1.1.1.2 2003/05/22 01:16:36 rbraun Exp $
 */

struct str_handle
{
   char *string ;
   char *separator ;
   char *pos ;
   int flags ;
   int *errnop ;
   int no_more ;
} ;


#ifndef NULL
#define NULL         0
#endif

#ifndef FALSE
#define FALSE        0
#define TRUE         1
#endif

#endif /* __STRPARSE_H */

