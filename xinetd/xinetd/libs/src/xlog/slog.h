/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


/*
 * $Id: slog.h,v 1.1.1.3 2003/06/15 17:31:44 rbraun Exp $
 */
#ifndef __SLOG_H
#define __SLOG_H

struct syslog_s
{
	int sl_facility ;
	int sl_default_level ;
} ;


struct syslog_parms
{
   int 		slp_n_xlogs ;           /* # of xlogs using syslog */
   int 		slp_logopts ;           /* used at openlog */
   int 		slp_facility ;
   const char  *slp_ident ;		/* used at openlog */
   /* bool_int slp_ident_is_malloced ; */
} ;

#define SYSLOG( xp )         ((struct syslog_s *)xp->xl_data)

#endif

