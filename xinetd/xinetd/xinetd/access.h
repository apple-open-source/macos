/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef ACCESS_H
#define ACCESS_H

/*
 * $Id: access.h,v 1.1.1.3 2002/10/02 21:07:18 bbraun Exp $
 */


#include "util.h"	/* for nv_get_name() */
#include "defs.h"	/* bool_int */
#include "connection.h"


/*
 * These flags are used to form a mask for access_control.
 * The mask determines which checks will be performed.
 */
#define CF_ADDRESS            1
#define CF_TIME               2
#define CF_SERVICE_LIMIT      3

typedef enum 
   {
      AC_OK,               /* ok to start a server                         */
      AC_FORK,             /* tried to start a server but fork failed      */
      AC_ADDRESS,          /* we do not accept requests from that address  */
      AC_TIME,             /* we do not accept requests at this time       */
      AC_SERVICE_LIMIT,    /* server limit would be exceeded for this      */
                           /* service                                      */
      AC_PER_SOURCE_LIMIT, /* server limit would be exceeded for this      */
                           /* service and source address                   */
      AC_PROCESS_LIMIT,    /* total process limit would be exceeded        */
      AC_LIBWRAP,
      AC_LOAD,
      AC_CPS
   } access_e ;


#define ACCESS_EXPLAIN( code )   nv_get_name( access_code_names, (int) (code) )


extern const struct name_value access_code_names[];
access_e access_control(struct service *sp,
	const connection_s *cp,const mask_t *check_mask);
access_e parent_access_control(struct service *sp,const connection_s *cp);


#endif   /* ACCESS_H */

