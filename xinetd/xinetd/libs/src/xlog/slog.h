/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


/*
 * $Id: slog.h,v 1.1.1.1 2002/01/31 07:09:05 zarzycki Exp $
 */
#ifndef __SLOG_H
#define __SLOG_H

struct syslog
{
	int sl_facility ;
	int sl_default_level ;
} ;


struct syslog_parms
{
   int 		slp_n_xlogs ;           /* # of xlogs using syslog */
   int 		slp_logopts ;           /* used at openlog */
   int 		slp_facility ;
   char		*slp_ident ;		/* used at openlog */
   /* bool_int slp_ident_is_malloced ; */
} ;

#define SYSLOG( xp )         ((struct syslog *)xp->xl_data)

#endif

