/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef PARSE_H
#define PARSE_H

#include "defs.h"
#include "conf.h"


/*
 * $Id: parse.h,v 1.1.1.1 2002/01/31 07:09:05 zarzycki Exp $
 */

typedef enum { NO_ENTRY, 
               BAD_ENTRY, 
               SERVICE_ENTRY, 
               DEFAULTS_ENTRY, 
               INCLUDE_ENTRY,
               INCLUDEDIR_ENTRY
} entry_e ;

enum assign_op { SET_EQ, PLUS_EQ, MINUS_EQ } ;

struct attribute
{
   char		*a_name ;            /* name of attribute							*/
   unsigned a_id ;               /* attribute id								*/
   int		a_nvalues ;				/* number of values							*/
   status_e (*a_parser)() ;		/* function that parses the attribute	*/
} ;


#define ENTRY_BEGIN              '{'
#define ENTRY_END                '}'
#define COMMENT_BEGIN            '#'
#define KW_SERVICE					"service"
#define KW_DEFAULTS					"defaults"
#define KW_INCLUDE					"include"
#define KW_INCLUDEDIR				"includedir"

extern int line_count;

char *attr_name_lookup(int id);
void parse_end(void);
void parse_conf_file(int fd,struct configuration *confp);

#endif	/* PARSE_H */
