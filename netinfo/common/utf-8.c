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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <NetInfo/utf-8.h>

static dsutil_utf8_callbacks _utf8_callbacks = { 0, NULL, NULL };

#undef ISASCII
#define ISASCII(uc)	((uc) < 0x100)
#undef UCS4_INVALID
#define UCS4_INVALID	0x80000000U

/*
 * Basic UTF-8 routines
 */

/*
 * return the number of bytes required to hold the
 * NULL-terminated UTF-8 string NOT INCLUDING the
 * termination.
 */
u_int32_t dsutil_utf8_bytes( const char * p )
{
	u_int32_t bytes;

	for( bytes=0; p[bytes]; bytes++ ) {
		/* EMPTY */ ;
	}

	return bytes;
}

u_int32_t dsutil_utf8_chars( const char * p )
{
	/* could be optimized and could check for invalid sequences */
	u_int32_t chars=0;

	for( ; *p ; DSUTIL_UTF8_INCR(p) ) {
		chars++;
	};

	return chars;
}

/* return offset to next character */
int dsutil_utf8_offset( const char * p )
{
	return DSUTIL_UTF8_NEXT(p) - p;
}

/*
 * Returns length indicated by first byte.
 *
 * This function should use a table lookup.
 */
int dsutil_utf8_charlen( const char * p )
{
	unsigned c = * (const unsigned char *) p;

	if ((c & 0xfe ) == 0xfc) {
		return 6;
	}

	if ((c & 0xfc ) == 0xf8) {
		return 5;
	}

	if ((c & 0xf8 ) == 0xf0) {
		return 4;
	}

	if ((c & 0xf0 ) == 0xe0) {
		return 3;
	}

	if ((c & 0xe0 ) == 0xc0) {
		return 2;
	}

	if ((c & 0x80 ) == 0x80) {
		/* INVALID */
		return 0;
	}

	return 1;
}

/* conv UTF-8 to UCS-4, useful for comparisons */
int32_t dsutil_utf8_to_ucs4( const char * p )
{
    const unsigned char *c = p;
    int32_t ch;
	int len, i;
	static unsigned char mask[] = {
		0, 0x7f, 0x1f, 0x0f, 0x07, 0x03, 0x01 };

	len = DSUTIL_UTF8_CHARLEN(p);

	if( len == 0 ) return UCS4_INVALID;

	ch = c[0] & mask[len];

	for(i=1; i < len; i++) {
		if ((c[i] & 0xc0) != 0x80) {
			return UCS4_INVALID;
		}

		ch <<= 6;
		ch |= c[i] & 0x3f;
	}

	return ch;
}

/* conv UCS-4 to UTF-8, not used */
int dsutil_ucs4_to_utf8( int32_t c, char *buf )
{
	int len=0;
	unsigned char* p = buf;
	if(buf == NULL) return 0;

	if ( c < 0 ) {
		/* not a valid Unicode character */

	} else if( c < 0x80 ) {
		p[len++] = c;

	} else if( c < 0x800 ) {
		p[len++] = 0xc0 | ( c >> 6 );
		p[len++] = 0x80 | ( c & 0x3f );

	} else if( c < 0x10000 ) {
		p[len++] = 0xe0 | ( c >> 12 );
		p[len++] = 0x80 | ( (c >> 6) & 0x3f );
		p[len++] = 0x80 | ( c & 0x3f );

	} else if( c < 0x200000 ) {
		p[len++] = 0xf0 | ( c >> 18 );
		p[len++] = 0x80 | ( (c >> 12) & 0x3f );
		p[len++] = 0x80 | ( (c >> 6) & 0x3f );
		p[len++] = 0x80 | ( c & 0x3f );

	} else if( c < 0x400000 ) {
		p[len++] = 0xf8 | ( c >> 24 );
		p[len++] = 0x80 | ( (c >> 18) & 0x3f );
		p[len++] = 0x80 | ( (c >> 12) & 0x3f );
		p[len++] = 0x80 | ( (c >> 6) & 0x3f );
		p[len++] = 0x80 | ( c & 0x3f );

	} else /* if( c < 0x80000000 ) */ {
		p[len++] = 0xfc | ( c >> 30 );
		p[len++] = 0x80 | ( (c >> 24) & 0x3f );
		p[len++] = 0x80 | ( (c >> 18) & 0x3f );
		p[len++] = 0x80 | ( (c >> 12) & 0x3f );
		p[len++] = 0x80 | ( (c >> 6) & 0x3f );
		p[len++] = 0x80 | ( c & 0x3f );
	}

	buf[len] = '\0';
	return len;
}

/*
 * Advance to the next UTF-8 character
 *
 * Ignores length of multibyte character, instead rely on
 * continuation markers to find start of next character.
 * This allows for "resyncing" of when invalid characters
 * are provided provided the start of the next character
 * is appears within the 6 bytes examined.
 */
char* dsutil_utf8_next( const char * p )
{
	int i;
	const unsigned char *u = p;

	if( DSUTIL_UTF8_ISASCII(u) ) {
		return (char *) &p[1];
	}

	for( i=1; i<6; i++ ) {
		if ( ( u[i] & 0xc0 ) != 0x80 ) {
			return (char *) &p[i];
		}
	}

	return (char *) &p[i];
}

/*
 * Advance to the previous UTF-8 character
 *
 * Ignores length of multibyte character, instead rely on
 * continuation markers to find start of next character.
 * This allows for "resyncing" of when invalid characters
 * are provided provided the start of the next character
 * is appears within the 6 bytes examined.
 */
char* dsutil_utf8_prev( const char * p )
{
	int i;
	const unsigned char *u = p;

	for( i=-1; i>-6 ; i-- ) {
		if ( ( u[i] & 0xc0 ) != 0x80 ) {
			return (char *) &p[i];
		}
	}

	return (char *) &p[i];
}

/*
 * Copy one UTF-8 character from src to dst returning
 * number of bytes copied.
 *
 * Ignores length of multibyte character, instead rely on
 * continuation markers to find start of next character.
 * This allows for "resyncing" of when invalid characters
 * are provided provided the start of the next character
 * is appears within the 6 bytes examined.
 */
int dsutil_utf8_copy( char* dst, const char *src )
{
	int i;
	const unsigned char *u = src;

	dst[0] = src[0];

	if( DSUTIL_UTF8_ISASCII(u) ) {
		return 1;
	}

	for( i=1; i<6; i++ ) {
		if ( ( u[i] & 0xc0 ) != 0x80 ) {
			return i; 
		}
		dst[i] = src[i];
	}

	return i;
}

/*
 * UTF-8 ctype routines
 * Only deals with characters < 0x100 (ie: US-ASCII)
 */

int dsutil_utf8_isascii( const char * p )
{
	unsigned c = * (const unsigned char *) p;
	return ISASCII(c);
}

int dsutil_utf8_isdigit( const char * p )
{
	unsigned c = * (const unsigned char *) p;

	if(!ISASCII(c)) return 0;

	return c >= '0' && c <= '9';
}

int dsutil_utf8_isxdigit( const char * p )
{
	unsigned c = * (const unsigned char *) p;

	if(!ISASCII(c)) return 0;

	return ( c >= '0' && c <= '9' )
		|| ( c >= 'A' && c <= 'F' )
		|| ( c >= 'a' && c <= 'f' );
}

int dsutil_utf8_isspace( const char * p )
{
	unsigned c = * (const unsigned char *) p;

	if(!ISASCII(c)) return 0;

	switch(c) {
	case ' ':
	case '\t':
	case '\n':
	case '\r':
	case '\v':
	case '\f':
		return 1;
	}

	return 0;
}

#ifndef UTF8_ALPHA_CTYPE
/*
 * These are not needed by the C SDK and are
 * not "good enough" for general use.
 */
int dsutil_utf8_isalpha( const char * p )
{
	unsigned c = * (const unsigned char *) p;

	if(!ISASCII(c)) return 0;

	return ( c >= 'A' && c <= 'Z' )
		|| ( c >= 'a' && c <= 'z' );
}

int dsutil_utf8_isalnum( const char * p )
{
	unsigned c = * (const unsigned char *) p;

	if(!ISASCII(c)) return 0;

	return ( c >= '0' && c <= '9' )
		|| ( c >= 'A' && c <= 'Z' )
		|| ( c >= 'a' && c <= 'z' );
}

int dsutil_utf8_islower( const char * p )
{
	unsigned c = * (const unsigned char *) p;

	if(!ISASCII(c)) return 0;

	return ( c >= 'a' && c <= 'z' );
}

int dsutil_utf8_isupper( const char * p )
{
	unsigned c = * (const unsigned char *) p;

	if(!ISASCII(c)) return 0;

	return ( c >= 'A' && c <= 'Z' );
}
#endif


/*
 * UTF-8 string routines
 */

/* like strchr() */
char * (dsutil_utf8_strchr)( const char *str, const char *chr )
{
	for( ; *str != '\0'; DSUTIL_UTF8_INCR(str) ) {
		if( dsutil_utf8_to_ucs4( str ) == dsutil_utf8_to_ucs4( chr ) ) {
			return (char *) str;
		} 
	}

	return NULL;
}

/* like strcspn() but returns number of bytes, not characters */
u_int32_t (dsutil_utf8_strcspn)( const char *str, const char *set )
{
	const char *cstr;
	const char *cset;

	for( cstr = str; *cstr != '\0'; DSUTIL_UTF8_INCR(cstr) ) {
		for( cset = set; *cset != '\0'; DSUTIL_UTF8_INCR(cset) ) {
			if( dsutil_utf8_to_ucs4( cstr ) == dsutil_utf8_to_ucs4( cset ) ) {
				return cstr - str;
			} 
		}
	}

	return cstr - str;
}

/* like strspn() but returns number of bytes, not characters */
u_int32_t (dsutil_utf8_strspn)( const char *str, const char *set )
{
	const char *cstr;
	const char *cset;

	for( cstr = str; *cstr != '\0'; DSUTIL_UTF8_INCR(cstr) ) {

		for( cset = set; ; DSUTIL_UTF8_INCR(cset) ) {
			if( *cset == '\0' ) {
				return cstr - str;
			}

			if( dsutil_utf8_to_ucs4( cstr ) == dsutil_utf8_to_ucs4( cset ) ) {
				break;
			} 
		}
	}

	return cstr - str;
}

/* like strpbrk(), replaces strchr() as well */
char *(dsutil_utf8_strpbrk)( const char *str, const char *set )
{
	for( ; *str != '\0'; DSUTIL_UTF8_INCR(str) ) {
		const char *cset;

		for( cset = set; *cset != '\0'; DSUTIL_UTF8_INCR(cset) ) {
			if( dsutil_utf8_to_ucs4( str ) == dsutil_utf8_to_ucs4( cset ) ) {
				return (char *) str;
			} 
		}
	}

	return NULL;
}

/* like strtok_r(), not strtok() */
char *(dsutil_utf8_strtok)(char *str, const char *sep, char **last)
{
	char *begin;
	char *end;

	if( last == NULL ) return NULL;

	begin = str ? str : *last;

	begin += dsutil_utf8_strspn( begin, sep );

	if( *begin == '\0' ) {
		*last = NULL;
		return NULL;
	}

	end = &begin[ dsutil_utf8_strcspn( begin, sep ) ];

	if( *end != '\0' ) {
		char *next = DSUTIL_UTF8_NEXT( end );
		*end = '\0';
		end = next;
	}

	*last = end;
	return begin;
}

void dsutil_utf8_set_callbacks(dsutil_utf8_callbacks *callbacks)
{
	if (callbacks != NULL)
	{
		if (callbacks->version == DSUTIL_UTF8_CALLBACKS_VERSION)
			_utf8_callbacks = *callbacks;
	}
	else
	{
		_utf8_callbacks.version = 0;
		_utf8_callbacks.normalize = 0;
		_utf8_callbacks.compare = 0;
	}
}

dsdata *dsutil_utf8_normalize(dsdata *str, u_int32_t casefold)
{
	u_int32_t i;
	dsdata *out;

	if (_utf8_callbacks.normalize != NULL)
	{
		return (_utf8_callbacks.normalize)(str, casefold);
	}

	if (casefold == 0)
	{
		return dsdata_retain(str);
	}

	/*
	 * Copy input string and normalize each character
	 * to upper-case.
	 */
	out = dsdata_copy(str);
	if (out == NULL)
	{
		return NULL;
	}

	for (i = 0; i < out->length; i++)
	{
		out->data[i] = toupper(out->data[i]);
	}

	return out;
}

int32_t dsutil_utf8_compare(dsdata *a, dsdata *b, u_int32_t casefold)
{
	u_int32_t len;

	if (_utf8_callbacks.compare != NULL)
	{
		return (_utf8_callbacks.compare)(a, b, casefold);
	}

	len = (a->length > b->length) ? b->length - 1 : a->length - 1;

	return casefold ? strncasecmp(a->data, b->data, len) : strncmp(a->data, b->data, len);
}

