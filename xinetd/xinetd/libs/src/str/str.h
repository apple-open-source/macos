/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#ifndef __STR_H
#define __STR_H

/*
 * $Id: str.h,v 1.1.1.5 2003/06/15 17:31:44 rbraun Exp $
 */

#include <stdarg.h>

#ifdef __GNUC__
#define PRINTF_FORMAT(n, m) \
   __attribute__ ((format (printf, n, m)))
#else
#define PRINTF_FORMAT(n, m)
#endif

typedef void *str_h ;

/*
 * strprint(3) functions
 */
char *strx_sprint ( char *buf, int len, const char *fmt, ... ) 
   PRINTF_FORMAT(3, 4);
int strx_nprint ( char *buf, int len, const char *fmt, ... ) 
   PRINTF_FORMAT(3, 4);
void strx_print ( int *count, char *buf, int len, const char *fmt, ... )
   PRINTF_FORMAT(4, 5);

int strx_nprintv ( char *buf, int len, const char *fmt, va_list )
   PRINTF_FORMAT(3, 0);
void strx_printv ( int *cnt, char *buf, int len, const char *fmt, va_list ) 
   PRINTF_FORMAT(4, 0);
 

/*
 * strparse(3) functions
 */
int str_setstr( str_h handle, char *newstr );
   
/*
 * Return values
 */
#define STR_OK                                  0
#define STR_ERR                                 (-1)
   

/* 
 * Flags for the string parsing functions
 */
#define STR_NOFLAGS			0x0
#define STR_RETURN_ERROR		0x1
#define STR_NULL_START			0x2
#define STR_NULL_END			0x4
#define STR_MALLOC			0x8

/*
 * Error values
 */
#define STR_ENULLSEPAR			1
#define STR_ENULLSTRING			2
#define STR_ENOMEM			3

char *new_string(const char *) ;
str_h str_parse ( char *str, const char *separ, int flags, int *errnop ) ;
void str_endparse ( str_h handle ) ;
char *str_component ( str_h handle ) ;


/*
 * strutil(3) functions
 */
char *str_casefind ( char *s1, const char *s2 ) ;
void str_fill ( char *s, char c ) ;


#endif 	/* __STR_H */

