/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef LOG_H
#define LOG_H

#include <sys/socket.h>

#include "defs.h"
#include "access.h"

/*
 * $Id: log.h,v 1.1.1.3 2002/10/02 21:07:25 bbraun Exp $
 */

/*
 * Meaning of logtype flags:
 *
 *    L_NONE:           no logging
 *    L_FILE:           log output goes to a file
 *    L_SYSLOG:         log output goes to syslog(3)
 *    L_COMMON_FILE:    log output goes to the file specified in defaults
 */
typedef enum { L_NONE = 0, L_FILE, L_SYSLOG, L_COMMON_FILE } logtype_e ;

struct filelog
{
   char     *fl_filename ;                  /* always malloc'ed      */
   unsigned fl_soft_limit ;
   unsigned fl_hard_limit ;
} ;

#define FILELOG_SIZE_CONTROL( flp )      ( flp->fl_soft_limit != 0 )   


struct syslog
{
   int sl_facility ;
   int sl_level ;
} ;

struct log
{
   logtype_e      l_type ;
   struct filelog l_fl ;
   struct syslog  l_sl ;
} ;

#define LOG_GET_TYPE( lp )           (lp)->l_type
#define LOG_SET_TYPE( lp, type )     (lp)->l_type = (type)

#define LOG_GET_FILELOG( lp )        (&(lp)->l_fl)
#define LOG_GET_SYSLOG( lp )         (&(lp)->l_sl)

char *xaddrname(const union xsockaddr *inaddr);
uint16_t xaddrport(const union xsockaddr *inaddr);
void svc_log_success(struct service *sp, const connection_s *cp,pid_t pid);
void svc_log_failure(struct service *sp, const connection_s *cp,access_e access_failure);
void svc_log_exit(struct service *sp,const struct server *serp);
void svc_logprint(struct service *sp,char *line_id,const char *fmt,...)
#ifdef __GNUC__
	__attribute__ ((format (printf, 3, 4)));
#else
	;
#endif
#endif   /* LOG_H */

