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
 * Copyright (c) 1997 Apple Computer Inc.
 * Copyright (c) 1990 Regents of the University of Michigan.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

#ifndef _LBER_H
#define _LBER_H

#ifdef __cplusplus
extern "C" {
#endif


#ifdef LDAP_DEBUG
extern int lber_debug;
#endif

#ifndef LDAPFUNCDECL
#define LDAPFUNCDECL
#endif /* LDAPFUNCDECL */

/* BER classes and mask */
#define LBER_CLASS_UNIVERSAL	0x00
#define LBER_CLASS_APPLICATION	0x40
#define LBER_CLASS_CONTEXT	0x80
#define LBER_CLASS_PRIVATE	0xc0
#define LBER_CLASS_MASK		0xc0

/* BER encoding type and mask */
#define LBER_PRIMITIVE		0x00
#define LBER_CONSTRUCTED	0x20
#define LBER_ENCODING_MASK	0x20

#define LBER_BIG_TAG_MASK	0x1f
#define LBER_MORE_TAG_MASK	0x80

/*
 * Note that LBER_ERROR and LBER_DEFAULT are values that can never appear
 * as valid BER tags, and so it is safe to use them to report errors.  In
 * fact, any tag for which the following is true is invalid:
 *     (( tag & 0x00000080 ) != 0 ) && (( tag & 0xFFFFFF00 ) != 0 )
 */
#define LBER_ERROR		0xffffffffL
#define LBER_DEFAULT		0xffffffffL

/* general BER types we know about */
#define LBER_BOOLEAN		0x01L
#define LBER_INTEGER		0x02L
#define LBER_BITSTRING		0x03L
#define LBER_OCTETSTRING	0x04L
#define LBER_NULL		0x05L
#define LBER_ENUMERATED		0x0aL
#define LBER_SEQUENCE		0x30L	/* constructed */
#define LBER_SET		0x31L	/* constructed */

#define OLD_LBER_SEQUENCE	0x10L	/* w/o constructed bit - broken */
#define OLD_LBER_SET		0x11L	/* w/o constructed bit - broken */

#ifdef NEEDPROTOS
typedef int (*BERTranslateProc)( char **bufp, unsigned long *buflenp,
	int free_input );
#else /* NEEDPROTOS */
typedef int (*BERTranslateProc)();
#endif /* NEEDPROTOS */

typedef struct berelement {
	char		*ber_buf;
	char		*ber_ptr;
	char		*ber_end;
	struct seqorset	*ber_sos;
	unsigned long	ber_tag;
	unsigned long	ber_len;
	int		ber_usertag;
	char		ber_options;
#define LBER_USE_DER		0x01
#define LBER_USE_INDEFINITE_LEN	0x02
#define LBER_TRANSLATE_STRINGS	0x04
	char		*ber_rwptr;
	BERTranslateProc ber_encode_translate_proc;
	BERTranslateProc ber_decode_translate_proc;
} BerElement;
#define NULLBER	((BerElement *) 0)

typedef struct sockbuf {
#ifndef MACOS
	int		sb_sd;
#else /* MACOS */
	void		*sb_sd;
#endif /* MACOS */
	BerElement	sb_ber;

	int		sb_naddr;	/* > 0 implies using CLDAP (UDP) */
	void		*sb_useaddr;	/* pointer to sockaddr to use next */
	void		*sb_fromaddr;	/* pointer to message source sockaddr */
	void		**sb_addrs;	/* actually an array of pointers to
						sockaddrs */

	int		sb_options;	/* to support copying ber elements */
#define LBER_TO_FILE		0x01	/* to a file referenced by sb_fd   */
#define LBER_TO_FILE_ONLY	0x02	/* only write to file, not network */
#define LBER_MAX_INCOMING_SIZE	0x04	/* impose limit on incoming stuff  */
#define LBER_NO_READ_AHEAD	0x08	/* read only as much as requested  */
#define LBER_RES_NO_LOOKUPD	0x16	/* don't use lookupd for host resolution */
	int		sb_fd;
	long		sb_max_incoming;
} Sockbuf;
#define READBUFSIZ	8192

typedef struct seqorset {
	BerElement	*sos_ber;
	unsigned long	sos_clen;
	unsigned long	sos_tag;
	char		*sos_first;
	char		*sos_ptr;
	struct seqorset	*sos_next;
} Seqorset;
#define NULLSEQORSET	((Seqorset *) 0)

/* structure for returning a sequence of octet strings + length */
struct berval {
	unsigned long	bv_len;
	char		*bv_val;
};

LDAPFUNCDECL void lber_bprint( char *data, int len );

LDAPFUNCDECL unsigned long ber_get_tag( BerElement *ber );
LDAPFUNCDECL unsigned long ber_skip_tag( BerElement *ber, unsigned long *len );
LDAPFUNCDECL unsigned long ber_peek_tag( BerElement *ber, unsigned long *len );
LDAPFUNCDECL unsigned long ber_get_int( BerElement *ber, long *num );
LDAPFUNCDECL unsigned long ber_get_stringb( BerElement *ber, char *buf,
	unsigned long *len );
LDAPFUNCDECL unsigned long ber_get_stringa( BerElement *ber, char **buf );
LDAPFUNCDECL unsigned long ber_get_stringal( BerElement *ber, struct berval **bv );
LDAPFUNCDECL unsigned long ber_get_bitstringa( BerElement *ber, char **buf,
	unsigned long *len );
LDAPFUNCDECL unsigned long ber_get_null( BerElement *ber );
LDAPFUNCDECL unsigned long ber_get_boolean( BerElement *ber, int *boolval );
LDAPFUNCDECL unsigned long ber_first_element( BerElement *ber, unsigned long *len,
	char **last );
LDAPFUNCDECL unsigned long ber_next_element( BerElement *ber, unsigned long *len,
	char *last );
LDAPFUNCDECL unsigned long ber_scanf( BerElement *ber, char *fmt, ... );
LDAPFUNCDECL void ber_bvfree( struct berval *bv );
LDAPFUNCDECL void ber_bvecfree( struct berval **bv );
LDAPFUNCDECL struct berval *ber_bvdup( struct berval *bv );
#ifdef STR_TRANSLATION
LDAPFUNCDECL void ber_set_string_translators( BerElement *ber,
	BERTranslateProc encode_proc, BERTranslateProc decode_proc );
#endif /* STR_TRANSLATION */

LDAPFUNCDECL int ber_put_enum( BerElement *ber, long num, unsigned long tag );
LDAPFUNCDECL int ber_put_int( BerElement *ber, long num, unsigned long tag );
LDAPFUNCDECL int ber_put_ostring( BerElement *ber, char *str, unsigned long len,
	unsigned long tag );
LDAPFUNCDECL int ber_put_string( BerElement *ber, char *str, unsigned long tag );
LDAPFUNCDECL int ber_put_bitstring( BerElement *ber, char *str,
	unsigned long bitlen, unsigned long tag );
LDAPFUNCDECL int ber_put_null( BerElement *ber, unsigned long tag );
LDAPFUNCDECL int ber_put_boolean( BerElement *ber, int boolval,
	unsigned long tag );
LDAPFUNCDECL int ber_start_seq( BerElement *ber, unsigned long tag );
LDAPFUNCDECL int ber_start_set( BerElement *ber, unsigned long tag );
LDAPFUNCDECL int ber_put_seq( BerElement *ber );
LDAPFUNCDECL int ber_put_set( BerElement *ber );
LDAPFUNCDECL int ber_printf( BerElement *ber, char *fmt, ... );

LDAPFUNCDECL long ber_read( BerElement *ber, char *buf, unsigned long len );
LDAPFUNCDECL long ber_write( BerElement *ber, char *buf, unsigned long len,
	int nosos );
LDAPFUNCDECL void ber_free( BerElement *ber, int freebuf );
LDAPFUNCDECL int ber_flush( Sockbuf *sb, BerElement *ber, int freeit );
LDAPFUNCDECL BerElement *ber_alloc( void );
LDAPFUNCDECL BerElement *der_alloc( void );
LDAPFUNCDECL BerElement *ber_alloc_t( int options );
LDAPFUNCDECL BerElement *ber_dup( BerElement *ber );
LDAPFUNCDECL void ber_dump( BerElement *ber, int inout );
LDAPFUNCDECL void ber_sos_dump( Seqorset *sos );
LDAPFUNCDECL unsigned long ber_get_next( Sockbuf *sb, unsigned long *len,
	BerElement *ber );
LDAPFUNCDECL void ber_init( BerElement *ber, int options );
LDAPFUNCDECL void ber_reset( BerElement *ber, int was_writing );

#define LBER_HTONL( l )	htonl( l )
#define LBER_NTOHL( l )	ntohl( l )

#define SAFEMEMCPY( d, s, n )	bcopy( s, d, n )

#ifdef __cplusplus
}
#endif
#endif /* _LBER_H */
