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
**	$Id: error.h,v 1.1.1.1 1999/05/02 03:57:41 wsanchez Exp $
**
** error.h                                               Error handling macros
**
** This program is in the public domain and may be used freely by anyone
** who wants to. 
**
** Last update: 19 Aug 1992
**
** Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
*/

#ifndef __ERROR_H__
#define __ERROR_H__

#include <syslog.h>

#define ERROR(fmt) \
    ((syslog_flag ? (syslog(LOG_ERR, fmt),0) : 0), \
     (debug_flag ? (fprintf(stderr, "%d , %d : ERROR : X-DBG : ", \
			    lport, fport), \
		    fprintf(stderr, fmt), perror(": "), 0) : \
      (printf("%d , %d : ERROR : UNKNOWN-ERROR\r\n", lport, fport), 0)), \
     fflush(stdout), fflush(stderr), exit(1), 0)


#define ERROR1(fmt,v1) \
    ((syslog_flag ? (syslog(LOG_ERR, fmt, v1),0) : 0), \
     (debug_flag ? (fprintf(stderr, "%d , %d : ERROR : X-DBG : ", \
			    lport, fport), \
		    fprintf(stderr, fmt, v1), perror(": "), 0) : \
      (printf("%d , %d : ERROR : UNKNOWN-ERROR\r\n", lport, fport), 0)), \
     fflush(stdout), fflush(stderr), exit(1), 0)

#define ERROR2(fmt,v1,v2) \
    ((syslog_flag ? (syslog(LOG_ERR, fmt, v1, v2),0) : 0), \
     (debug_flag ? (fprintf(stderr, "%d , %d : ERROR : X-DBG : ", \
			    lport, fport), \
		    fprintf(stderr, fmt, v1, v2), perror(": "), 0) : \
      (printf("%d , %d : ERROR : UNKNOWN-ERROR\r\n", lport, fport), 0)), \
     fflush(stdout), fflush(stderr), exit(1), 0)

#endif
