/* $OpenLDAP: pkg/ldap/libraries/libldap/utf-8.c,v 1.13 2000/06/09 04:48:43 mrv Exp $ */
/*
 * Copyright 1998-2000 The OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 */

/*
 * Basic UTF-8 routines
 *
 * These routines are "dumb".  Though they understand UTF-8,
 * they don't grok Unicode.  That is, they can push bits,
 * but don't have a clue what the bits represent.  That's
 * good enough for use with the LDAP Client SDK.
 *
 * These routines are not optimized.
 */

#ifndef __UTF_8_H__
#define __UTF_8_H__

#include <NetInfo/config.h>
#include <NetInfo/dsdata.h>

/* returns the number of bytes in the UTF-8 string */
u_int32_t dsutil_utf8_bytes( const char * );
/* returns the number of UTF-8 characters in the string */
u_int32_t dsutil_utf8_chars( const char * );
/* returns the length (in bytes) of the UTF-8 character */
int dsutil_utf8_offset( const char * );
/* returns the length (in bytes) indicated by the UTF-8 character */
int dsutil_utf8_charlen( const char * );
/* copies a UTF-8 character and returning number of bytes copied */
int dsutil_utf8_copy( char *, const char *);

/* returns pointer of next UTF-8 character in string */
char* dsutil_utf8_next( const char * );
/* returns pointer of previous UTF-8 character in string */
char* dsutil_utf8_prev( const char * );

/* primitive ctype routines -- not aware of non-ascii characters */
int dsutil_utf8_isascii( const char * );
int dsutil_utf8_isalpha( const char * );
int dsutil_utf8_isalnum( const char * );
int dsutil_utf8_isdigit( const char * );
int dsutil_utf8_isxdigit( const char * );
int dsutil_utf8_isspace( const char * );

/* span characters not in set, return bytes spanned */
u_int32_t dsutil_utf8_strcspn( const char* str, const char *set);
/* span characters in set, return bytes spanned */
u_int32_t dsutil_utf8_strspn( const char* str, const char *set);
/* return first occurance of character in string */
char * dsutil_utf8_strchr( const char* str, const char *chr);
/* return first character of set in string */
char * dsutil_utf8_strpbrk( const char* str, const char *set);
/* reentrant tokenizer */
char* dsutil_utf8_strtok( char* sp, const char* sep, char **last);

/* Optimizations */
#define DSUTIL_UTF8_ISASCII(p) ( * (const unsigned char *) (p) < 0x100 )
#define DSUTIL_UTF8_CHARLEN(p) ( DSUTIL_UTF8_ISASCII(p) \
	? 1 : dsutil_utf8_charlen((p)) )
#define DSUTIL_UTF8_OFFSET(p) ( DSUTIL_UTF8_ISASCII(p) \
	? 1 : dsutil_utf8_offset((p)) )

#define DSUTIL_UTF8_COPY(d,s) (	DSUTIL_UTF8_ISASCII(s) \
	? (*(d) = *(s), 1) : dsutil_utf8_copy((d),(s)) )

#define DSUTIL_UTF8_NEXT(p) (	DSUTIL_UTF8_ISASCII(p) \
	? (char *)(p)+1 : dsutil_utf8_next((p)) )

#define DSUTIL_UTF8_INCR(p) ((p) = DSUTIL_UTF8_NEXT(p))

/* For symmetry */
#define DSUTIL_UTF8_PREV(p) (dsutil_utf8_prev((p)))
#define DSUTIL_UTF8_DECR(p) ((p)=DSUTIL_UTF8_PREV((p)))

/* Optional character-set aware callbacks. */
#define DSUTIL_UTF8_CALLBACKS_VERSION 2
typedef struct {
	u_int32_t version;
	dsdata *(*normalize)(dsdata *, u_int32_t);
	int32_t (*compare)(dsdata *, dsdata *, u_int32_t);
} dsutil_utf8_callbacks;

void dsutil_utf8_set_callbacks(dsutil_utf8_callbacks *callbacks);
dsdata *dsutil_utf8_normalize(dsdata *, u_int32_t);
int32_t dsutil_utf8_compare(dsdata *, dsdata *, u_int32_t);

#endif __UTF_8_H__
