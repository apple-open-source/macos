/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
**	$Id: proxy.c,v 1.1.1.1 1999/05/02 03:57:41 wsanchez Exp $
**
** proxy.c                         This file implements the proxy() call.
**
** This program is in the public domain and may be used freely by anyone
** who wants to. 
**
** Last update: 12 Dec 1992
**
** Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
*/

#include <stdio.h>
#include <errno.h>

#include "identd.h"


#ifdef INCLUDE_PROXY
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>

#include <ident.h>
#endif


/*
** This function should establish a connection to a remote IDENT
** server and query it for the information associated with the
** specified connection and the return that to the caller.
**
** Should there be three different timeouts (Connection Establishment,
** Query Transmit and Query Receive)?
*/
int proxy(laddr, faddr, lport, fport, timeout)
  struct in_addr *laddr;
  struct in_addr *faddr;
  int lport;
  int fport;
  struct timeval *timeout;
{
#ifndef INCLUDE_PROXY
  printf("%d , %d : ERROR : %s\r\n",
	 lport, fport,
	 unknown_flag ? "UNKNOWN-ERROR" : "X-NOT-YET-IMPLEMENTED");
  
  return -1;
#else
  id_t *idp;
  char *answer;
  char *opsys;
  char *charset;
  
  idp = id_open(laddr, faddr, timeout);
  if (!idp)
  {
    printf("%d , %d : ERROR : %s\r\n",
	   lport, fport,
	   unknown_flag ? "UNKNOWN-ERROR" : "X-CONNECTION-REFUSED");
    return -1;
  }

  if (id_query(idp, lport, fport, timeout) < 0)
  {
    printf("%d , %d : ERROR : %s\r\n",
	   lport, fport,
	   unknown_flag ? "UNKNOWN-ERROR" : "X-TRANSMIT-QUERY-ERROR");
    id_close(idp);
    return -1;
  }

  switch (id_parse(idp, timeout, &lport, &fport, &answer, &opsys, &charset))
  {
    case 1:
      printf("%d , %d : USERID : %s %s%s : %s\r\n",
	     lport, fport,
	     opsys,
	     charset ? "," : "",
	     charset ? charset : "",
	     answer);
      break;
      
    case 2:
      printf("%d , %d : ERROR : %s\r\n",
	     lport, fport, answer);
      break;
      
    case 0:  /* More to parse - fix this later! */
    case -1: /* Internal error */
    default:
      printf("%d , %d : ERROR : %s\r\n",
	     lport, fport,
	     unknown_flag ? "UNKNOWN-ERROR" : "X-PARSE-REPLY-ERROR");
  }

  id_close(idp);
#endif  
}
