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
 * Copyright (c) 1990 Regents of the University of Michigan.
 * Copyright (c) 1997 Apple Computer, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 *
 */

#ifndef _LDAP_H
#define _LDAP_H

#include <lber.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LDAP_PORT	389
/* For SSL */
#define	LDAPS_PORT	636 
#define LDAP_VERSION1	1
#define LDAP_VERSION2	2
#define LDAP_VERSION3	3
#define LDAP_VERSION	LDAP_VERSION2

#define COMPAT20
#define COMPAT30
#if defined(COMPAT20) || defined(COMPAT30)
#define COMPAT
#endif

#define LDAP_OPT_DESC                   0x01
#define LDAP_OPT_DEREF                  0x02
#define LDAP_OPT_SIZELIMIT              0x03
#define LDAP_OPT_TIMELIMIT              0x04
#define LDAP_OPT_THREAD_FN_PTRS         0x05
#define LDAP_OPT_REBIND_FN              0x06
#define LDAP_OPT_REBIND_ARG             0x07
#define LDAP_OPT_REFERRALS              0x08
#define LDAP_OPT_RESTART                0x09
#define LDAP_OPT_PROTOCOL_VERSION	0x11
#define LDAP_OPT_SERVER_CONTROLS	0x12
#define LDAP_OPT_CLIENT_CONTROLS	0x13
#define LDAP_OPT_REFERRAL_HOP_LIMIT	0x16
#define LDAP_OPT_HOST_NAME		0x30
#define LDAP_OPT_ERROR_NUMBER		0x31
#define LDAP_OPT_ERROR_STRING		0x32
#define LDAP_OPT_RES_INTERNAL		0x40
/* for on/off options */
#define LDAP_OPT_ON     ((void *)1)
#define LDAP_OPT_OFF    ((void *)0)

#define LDAP_DEREF_NEVER	0x00
#define LDAP_DEREF_SEARCHING	0x01
#define LDAP_DEREF_FINDING	0x02
#define LDAP_DEREF_ALWAYS	0x03

#define LDAP_NO_LIMIT		0

/* debugging stuff */
#ifdef LDAP_DEBUG
extern int	ldap_debug;
#ifdef LDAP_SYSLOG
extern int	ldap_syslog;
extern int	ldap_syslog_level;
#endif
#define LDAP_DEBUG_TRACE	0x001
#define LDAP_DEBUG_PACKETS	0x002
#define LDAP_DEBUG_ARGS		0x004
#define LDAP_DEBUG_CONNS	0x008
#define LDAP_DEBUG_BER		0x010
#define LDAP_DEBUG_FILTER	0x020
#define LDAP_DEBUG_CONFIG	0x040
#define LDAP_DEBUG_ACL		0x080
#define LDAP_DEBUG_STATS	0x100
#define LDAP_DEBUG_STATS2	0x200
#define LDAP_DEBUG_SHELL	0x400
#define LDAP_DEBUG_PARSE	0x800
#define LDAP_DEBUG_ANY		0xffff

#ifdef LDAP_SYSLOG
#define Debug( level, fmt, arg1, arg2, arg3 )	\
	{ \
		if ( ldap_debug & level ) \
			fprintf( stderr, fmt, arg1, arg2, arg3 ); \
		if ( ldap_syslog & level ) \
			syslog( ldap_syslog_level, fmt, arg1, arg2, arg3 ); \
	}
#else /* LDAP_SYSLOG */
#ifndef WINSOCK
#define Debug( level, fmt, arg1, arg2, arg3 ) \
		if ( ldap_debug & level ) \
			fprintf( stderr, fmt, arg1, arg2, arg3 );
#else /* !WINSOCK */
extern void Debug( int level, char* fmt, ... );
#endif /* !WINSOCK */
#endif /* LDAP_SYSLOG */
#else /* LDAP_DEBUG */
#define Debug( level, fmt, arg1, arg2, arg3 )
#endif /* LDAP_DEBUG */

/* 
 * specific LDAP instantiations of BER types we know about
 */

/* general stuff */
#define LDAP_TAG_MESSAGE	0x30L	/* tag is 16 + constructed bit */
#define OLD_LDAP_TAG_MESSAGE	0x10L	/* forgot the constructed bit  */
#define LDAP_TAG_MSGID		0x02L

/* possible operations a client can invoke */
#define LDAP_REQ_BIND			0x60L	/* application + constructed */
#define LDAP_REQ_UNBIND			0x42L	/* application + primitive   */
#define LDAP_REQ_SEARCH			0x63L	/* application + constructed */
#define LDAP_REQ_MODIFY			0x66L	/* application + constructed */
#define LDAP_REQ_ADD			0x68L	/* application + constructed */
#define LDAP_REQ_DELETE			0x4aL	/* application + primitive   */
#define LDAP_REQ_MODRDN			0x6cL	/* application + constructed */
#define LDAP_REQ_COMPARE		0x6eL	/* application + constructed */
#define LDAP_REQ_ABANDON		0x50L	/* application + primitive   */

/* version 3.0 compatibility stuff */
#define LDAP_REQ_UNBIND_30		0x62L
#define LDAP_REQ_DELETE_30		0x6aL
#define LDAP_REQ_ABANDON_30		0x70L

/* 
 * old broken stuff for backwards compatibility - forgot application tag
 * and constructed/primitive bit
 */
#define OLD_LDAP_REQ_BIND		0x00L
#define OLD_LDAP_REQ_UNBIND		0x02L
#define OLD_LDAP_REQ_SEARCH		0x03L
#define OLD_LDAP_REQ_MODIFY		0x06L
#define OLD_LDAP_REQ_ADD		0x08L
#define OLD_LDAP_REQ_DELETE		0x0aL
#define OLD_LDAP_REQ_MODRDN		0x0cL
#define OLD_LDAP_REQ_COMPARE		0x0eL
#define OLD_LDAP_REQ_ABANDON		0x10L

/* possible result types a server can return */
#define LDAP_RES_BIND			0x61L	/* application + constructed */
#define LDAP_RES_SEARCH_ENTRY		0x64L	/* application + constructed */
#define LDAP_RES_SEARCH_RESULT		0x65L	/* application + constructed */
#define LDAP_RES_MODIFY			0x67L	/* application + constructed */
#define LDAP_RES_ADD			0x69L	/* application + constructed */
#define LDAP_RES_DELETE			0x6bL	/* application + constructed */
#define LDAP_RES_MODRDN			0x6dL	/* application + constructed */
#define LDAP_RES_COMPARE		0x6fL	/* application + constructed */
#define LDAP_RES_SESSION                0x72L	/* V3 */
#define LDAP_RES_SEARCH_REFERENCE       0x73L	/* V3 */
#define LDAP_RES_RESUME                 0x75L	/* V3 */
#define LDAP_RES_EXTENDED               0x78L	/* V3 */

#define LDAP_RES_ANY			(-1L)

/* old broken stuff for backwards compatibility */
#define OLD_LDAP_RES_BIND		0x01L
#define OLD_LDAP_RES_SEARCH_ENTRY	0x04L
#define OLD_LDAP_RES_SEARCH_RESULT	0x05L
#define OLD_LDAP_RES_MODIFY		0x07L
#define OLD_LDAP_RES_ADD		0x09L
#define OLD_LDAP_RES_DELETE		0x0bL
#define OLD_LDAP_RES_MODRDN		0x0dL
#define OLD_LDAP_RES_COMPARE		0x0fL

/* authentication methods available */
#define LDAP_AUTH_NONE		0x00L	/* no authentication		  */
#define LDAP_AUTH_SIMPLE	0x80L	/* context specific + primitive   */
#define LDAP_AUTH_KRBV4		0xffL	/* means do both of the following */
#define LDAP_AUTH_KRBV41	0x81L	/* context specific + primitive   */
#define LDAP_AUTH_KRBV42	0x82L	/* context specific + primitive   */

/* 3.0 compatibility auth methods */
#define LDAP_AUTH_SIMPLE_30	0xa0L	/* context specific + constructed */
#define LDAP_AUTH_KRBV41_30	0xa1L	/* context specific + constructed */
#define LDAP_AUTH_KRBV42_30	0xa2L	/* context specific + constructed */

/* old broken stuff */
#define OLD_LDAP_AUTH_SIMPLE	0x00L
#define OLD_LDAP_AUTH_KRBV4	0x01L
#define OLD_LDAP_AUTH_KRBV42	0x02L

/* filter types */
#define LDAP_FILTER_AND		0xa0L	/* context specific + constructed */
#define LDAP_FILTER_OR		0xa1L	/* context specific + constructed */
#define LDAP_FILTER_NOT		0xa2L	/* context specific + constructed */
#define LDAP_FILTER_EQUALITY	0xa3L	/* context specific + constructed */
#define LDAP_FILTER_SUBSTRINGS	0xa4L	/* context specific + constructed */
#define LDAP_FILTER_GE		0xa5L	/* context specific + constructed */
#define LDAP_FILTER_LE		0xa6L	/* context specific + constructed */
#define LDAP_FILTER_PRESENT	0x87L	/* context specific + primitive   */
#define LDAP_FILTER_APPROX	0xa8L	/* context specific + constructed */

/* 3.0 compatibility filter types */
#define LDAP_FILTER_PRESENT_30	0xa7L	/* context specific + constructed */

/* old broken stuff */
#define OLD_LDAP_FILTER_AND		0x00L
#define OLD_LDAP_FILTER_OR		0x01L
#define OLD_LDAP_FILTER_NOT		0x02L
#define OLD_LDAP_FILTER_EQUALITY	0x03L
#define OLD_LDAP_FILTER_SUBSTRINGS	0x04L
#define OLD_LDAP_FILTER_GE		0x05L
#define OLD_LDAP_FILTER_LE		0x06L
#define OLD_LDAP_FILTER_PRESENT		0x07L
#define OLD_LDAP_FILTER_APPROX		0x08L

/* substring filter component types */
#define LDAP_SUBSTRING_INITIAL	0x80L	/* context specific */
#define LDAP_SUBSTRING_ANY	0x81L	/* context specific */
#define LDAP_SUBSTRING_FINAL	0x82L	/* context specific */

/* 3.0 compatibility substring filter component types */
#define LDAP_SUBSTRING_INITIAL_30	0xa0L	/* context specific */
#define LDAP_SUBSTRING_ANY_30		0xa1L	/* context specific */
#define LDAP_SUBSTRING_FINAL_30		0xa2L	/* context specific */

/* old broken stuff */
#define OLD_LDAP_SUBSTRING_INITIAL	0x00L
#define OLD_LDAP_SUBSTRING_ANY		0x01L
#define OLD_LDAP_SUBSTRING_FINAL	0x02L

/* search scopes */
#define LDAP_SCOPE_BASE		0x00
#define LDAP_SCOPE_ONELEVEL	0x01
#define LDAP_SCOPE_SUBTREE	0x02

/* URL results */
#define LDAP_URL_ERR_NOTLDAP	1	/* URL doesn't begin with "ldap://" */
#define LDAP_URL_ERR_NODN	2	/* URL has no DN (required) */
#define LDAP_URL_ERR_BADSCOPE	3	/* URL scope string is invalid */
#define LDAP_URL_ERR_MEM	4	/* can't allocate memory space */
#define LDAP_URL_ERR_PARAM	5	/* bad parameter to an URL function */

/* 
 * possible error codes we can return
 */

#define LDAP_SUCCESS			0x00
#define LDAP_OPERATIONS_ERROR		0x01
#define LDAP_PROTOCOL_ERROR		0x02
#define LDAP_TIMELIMIT_EXCEEDED		0x03
#define LDAP_SIZELIMIT_EXCEEDED		0x04
#define LDAP_COMPARE_FALSE		0x05
#define LDAP_COMPARE_TRUE		0x06
#define LDAP_STRONG_AUTH_NOT_SUPPORTED	0x07
#define LDAP_STRONG_AUTH_REQUIRED	0x08
#define LDAP_PARTIAL_RESULTS		0x09

#define LDAP_REFERRAL 			0x0a	/* V3 */
#define LDAP_ADMINLIMIT_EXCEEDED	0x0b	/* V3 */
#define LDAP_UNAVAILABLE_CRITICAL_EXTENSION 0x0c	/* V3 */
#define LDAP_CONFIDENTIALITY_REQUIRED	0x0d	/* V3 */

#define LDAP_NO_SUCH_ATTRIBUTE		0x10
#define LDAP_UNDEFINED_TYPE		0x11
#define LDAP_INAPPROPRIATE_MATCHING	0x12
#define LDAP_CONSTRAINT_VIOLATION	0x13
#define LDAP_TYPE_OR_VALUE_EXISTS	0x14
#define LDAP_INVALID_SYNTAX		0x15

#define LDAP_NO_SUCH_OBJECT		0x20
#define LDAP_ALIAS_PROBLEM		0x21
#define LDAP_INVALID_DN_SYNTAX		0x22
#define LDAP_IS_LEAF			0x23 /* not used in V3 */
#define LDAP_ALIAS_DEREF_PROBLEM	0x24

#define NAME_ERROR(n)	((n & 0xf0) == 0x20)

#define LDAP_INAPPROPRIATE_AUTH		0x30
#define LDAP_INVALID_CREDENTIALS	0x31
#define LDAP_INSUFFICIENT_ACCESS	0x32
#define LDAP_BUSY			0x33
#define LDAP_UNAVAILABLE		0x34
#define LDAP_UNWILLING_TO_PERFORM	0x35
#define LDAP_LOOP_DETECT		0x36

#define LDAP_NAMING_VIOLATION		0x40
#define LDAP_OBJECT_CLASS_VIOLATION	0x41
#define LDAP_NOT_ALLOWED_ON_NONLEAF	0x42
#define LDAP_NOT_ALLOWED_ON_RDN		0x43
#define LDAP_ALREADY_EXISTS		0x44
#define LDAP_NO_OBJECT_CLASS_MODS	0x45
#define LDAP_RESULTS_TOO_LARGE		0x46
#define LDAP_AFFECTS_MULTIPLE_DSAS	0x47	/* V3 */

#define LDAP_OTHER			0x50
#define LDAP_SERVER_DOWN		0x51
#define LDAP_LOCAL_ERROR		0x52
#define LDAP_ENCODING_ERROR		0x53
#define LDAP_DECODING_ERROR		0x54
#define LDAP_TIMEOUT			0x55
#define LDAP_AUTH_UNKNOWN		0x56
#define LDAP_FILTER_ERROR		0x57
#define LDAP_USER_CANCELLED		0x58
#define LDAP_PARAM_ERROR		0x59
#define LDAP_NO_MEMORY			0x5a

#define LDAP_CONNECT_ERROR		0x5b	/* V3 */
#define LDAP_NOT_SUPPORTED		0x5c	/* V3 */
#define LDAP_CONTROL_NOT_FOUND		0x5d	/* V3 */
#define LDAP_NO_RESULTS_RETURNED	0x5e	/* V3 */
#define LDAP_MORE_RESULTS_TO_RETURN	0x5f	/* V3 */
#define LDAP_CLIENT_LOOP		0x60	/* V3 */
#define LDAP_REFERRAL_LIMIT_EXCEEDED	0x61	/* V3 */

/* default limit on nesting of referrals */
#define LDAP_DEFAULT_REFHOPLIMIT	5

/*
 * This structure represents both ldap messages and ldap responses.
 * These are really the same, except in the case of search responses,
 * where a response has multiple messages.
 */
struct			ldap;
typedef struct ldap	LDAP;

struct			ldapmsg;
typedef struct ldapmsg	LDAPMessage;

#define NULLMSG		NULL

typedef int (ldap_rebindproc_t)( LDAP *ld, 
	char **dnp, char **passwdp, int *authmethodp, int freeit, void *arg);

/* for modifications */
typedef struct ldapmod {
	int		mod_op;
#define LDAP_MOD_ADD		0x00
#define LDAP_MOD_DELETE		0x01
#define LDAP_MOD_REPLACE	0x02
#define LDAP_MOD_BVALUES	0x80
	char		*mod_type;
	union {
		char		**modv_strvals;
		struct berval	**modv_bvals;
	} mod_vals;
#define mod_values	mod_vals.modv_strvals
#define mod_bvalues	mod_vals.modv_bvals
	struct ldapmod	*mod_next;
} LDAPMod;


/*
 * structures for ldap getfilter routines
 */

typedef struct ldap_filt_info {
	char			*lfi_filter;
	char			*lfi_desc;
	int			lfi_scope;	/* LDAP_SCOPE_BASE, etc */
	int			lfi_isexact;	/* exact match filter? */
	struct ldap_filt_info	*lfi_next;
} LDAPFiltInfo;

typedef struct ldap_filt_list {
    char			*lfl_tag;
    char			*lfl_pattern;
    char			*lfl_delims;
    LDAPFiltInfo		*lfl_ilist;
    struct ldap_filt_list	*lfl_next;
} LDAPFiltList;

#define LDAP_FILT_MAXSIZ	1024

typedef struct ldap_filt_desc {
	LDAPFiltList		*lfd_filtlist;
	LDAPFiltInfo		*lfd_curfip;
	LDAPFiltInfo		lfd_retfi;
	char			lfd_filter[ LDAP_FILT_MAXSIZ ];
	char			*lfd_curval;
	char			*lfd_curvalcopy;
	char			**lfd_curvalwords;
	char			*lfd_filtprefix;
	char			*lfd_filtsuffix;
} LDAPFiltDesc;

/*
 * structure for ldap friendly mapping routines
 */

typedef struct friendly {
	char	*f_unfriendly;
	char	*f_friendly;
} FriendlyMap;


/*
 * handy macro to check whether LDAP struct is set up for CLDAP or not
 */
#define LDAP_IS_CLDAP( ld )	( ld->ld_sb.sb_naddr > 0 )


/*
 * types for ldap URL handling
 */
typedef struct ldap_url_desc {
    char		*lud_host;
    int			lud_port;
    char		*lud_dn;
    char		**lud_attrs;
    int			lud_scope;
    char		*lud_filter;
    unsigned long	lud_options;
#define LDAP_URL_OPT_SECURE     0x01
    char		*lud_string;	/* for internal use only */
} LDAPURLDesc;
#define NULLLDAPURLDESC	((LDAPURLDesc *)NULL)

typedef struct ldap_version {
	/* Highest protocol version supported * 100 */
	int	protocol_version;
	int	reserved[4];
} LDAPVersion;

#ifndef LDAPFUNCDECL
#define LDAPFUNCDECL
#endif

LDAPFUNCDECL int ldap_abandon( LDAP *ld, int msgid );

LDAPFUNCDECL int ldap_add( LDAP *ld, char *dn, LDAPMod **attrs );
LDAPFUNCDECL int ldap_add_s( LDAP *ld, char *dn, LDAPMod **attrs );

LDAPFUNCDECL void ldap_ber_free( BerElement *ber, int freebuf );
LDAPFUNCDECL int ldap_get_lderrno( LDAP *ld, char **m, char **s );
LDAPFUNCDECL int ldap_set_lderrno( LDAP *ld, int e, char *m, char *s );

LDAPFUNCDECL int ldap_bind( LDAP *ld, char *who, char *passwd, int authmethod );
LDAPFUNCDECL int ldap_bind_s( LDAP *ld, char *who, char *cred, int method );
LDAPFUNCDECL void ldap_set_rebind_proc( LDAP *ld, ldap_rebindproc_t *proc, void *arg);

LDAPFUNCDECL int ldap_simple_bind( LDAP *ld, char *who, char *passwd );
LDAPFUNCDECL int ldap_simple_bind_s( LDAP *ld, char *who, char *passwd );

LDAPFUNCDECL int ldap_kerberos_bind_s( LDAP *ld, char *who );
LDAPFUNCDECL int ldap_kerberos_bind1( LDAP *ld, char *who );
LDAPFUNCDECL int ldap_kerberos_bind1_s( LDAP *ld, char *who );
LDAPFUNCDECL int ldap_kerberos_bind2( LDAP *ld, char *who );
LDAPFUNCDECL int ldap_kerberos_bind2_s( LDAP *ld, char *who );

#ifndef NO_CACHE
LDAPFUNCDECL int ldap_enable_cache( LDAP *ld, long timeout, long maxmem );
LDAPFUNCDECL void ldap_disable_cache( LDAP *ld );
LDAPFUNCDECL void ldap_set_cache_options( LDAP *ld, unsigned long opts );
LDAPFUNCDECL void ldap_destroy_cache( LDAP *ld );
LDAPFUNCDECL void ldap_flush_cache( LDAP *ld );
LDAPFUNCDECL void ldap_uncache_entry( LDAP *ld, char *dn );
LDAPFUNCDECL void ldap_uncache_request( LDAP *ld, int msgid );
#endif /* !NO_CACHE */

LDAPFUNCDECL int ldap_compare( LDAP *ld, char *dn, char *attr, char *value );
LDAPFUNCDECL int ldap_compare_s( LDAP *ld, char *dn, char *attr, char *value );

LDAPFUNCDECL int ldap_delete( LDAP *ld, char *dn );
LDAPFUNCDECL int ldap_delete_s( LDAP *ld, char *dn );

LDAPFUNCDECL int ldap_result2error( LDAP *ld, LDAPMessage *r, int freeit );
LDAPFUNCDECL char *ldap_err2string( int err );
LDAPFUNCDECL void ldap_perror( LDAP *ld, char *s );

LDAPFUNCDECL int ldap_modify( LDAP *ld, char *dn, LDAPMod **mods );
LDAPFUNCDECL int ldap_modify_s( LDAP *ld, char *dn, LDAPMod **mods );

LDAPFUNCDECL int ldap_modrdn( LDAP *ld, char *dn, char *newrdn );
LDAPFUNCDECL int ldap_modrdn_s( LDAP *ld, char *dn, char *newrdn );
LDAPFUNCDECL int ldap_modrdn2( LDAP *ld, char *dn, char *newrdn,
	int deleteoldrdn );
LDAPFUNCDECL int ldap_modrdn2_s( LDAP *ld, char *dn, char *newrdn,
	int deleteoldrdn);

LDAPFUNCDECL LDAP *ldap_open( char *host, int port );
LDAPFUNCDECL LDAP *ldap_init( char *defhost, int defport );

LDAPFUNCDECL LDAP *ldap_new( void );

LDAPFUNCDECL int ldap_set_option( LDAP *ld, int option, void *optdata );
LDAPFUNCDECL int ldap_get_option( LDAP *ld, int option, void *optdata );
LDAPFUNCDECL int ldap_version( LDAPVersion *ver );
LDAPFUNCDECL void ldap_memfree( void *mem );
LDAPFUNCDECL int ldap_msgtype( LDAPMessage *res );
LDAPFUNCDECL int ldap_msgid( LDAPMessage *res );

LDAPFUNCDECL LDAPMessage *ldap_first_entry( LDAP *ld, LDAPMessage *chain );
LDAPFUNCDECL LDAPMessage *ldap_next_entry( LDAP *ld, LDAPMessage *entry );
LDAPFUNCDECL int ldap_count_entries( LDAP *ld, LDAPMessage *chain );

LDAPFUNCDECL LDAPMessage *ldap_delete_result_entry( LDAPMessage **list,
	LDAPMessage *e );
LDAPFUNCDECL void ldap_add_result_entry( LDAPMessage **list, LDAPMessage *e );

LDAPFUNCDECL char *ldap_get_dn( LDAP *ld, LDAPMessage *entry );
LDAPFUNCDECL char *ldap_dn2ufn( char *dn );
LDAPFUNCDECL char **ldap_explode_dn( char *dn, int notypes );
LDAPFUNCDECL char **ldap_explode_rdn( char *rdn, int notypes );

LDAPFUNCDECL char **ldap_explode_dns( char *dn );
LDAPFUNCDECL int ldap_is_dns_dn( char *dn );

LDAPFUNCDECL char *ldap_first_attribute( LDAP *ld, LDAPMessage *entry,
	BerElement **ber );
LDAPFUNCDECL char *ldap_next_attribute( LDAP *ld, LDAPMessage *entry,
	BerElement *ber );

LDAPFUNCDECL char **ldap_get_values( LDAP *ld, LDAPMessage *entry, char *target );
LDAPFUNCDECL struct berval **ldap_get_values_len( LDAP *ld, LDAPMessage *entry,
	char *target );
LDAPFUNCDECL int ldap_count_values( char **vals );
LDAPFUNCDECL int ldap_count_values_len( struct berval **vals );
LDAPFUNCDECL void ldap_value_free( char **vals );
LDAPFUNCDECL void ldap_value_free_len( struct berval **vals );

LDAPFUNCDECL int ldap_result( LDAP *ld, int msgid, int all,
	struct timeval *timeout, LDAPMessage **result );
LDAPFUNCDECL int ldap_msgfree( LDAPMessage *lm );
LDAPFUNCDECL int ldap_msgdelete( LDAP *ld, int msgid );

LDAPFUNCDECL int ldap_search( LDAP *ld, char *base, int scope, char *filter,
	char **attrs, int attrsonly );
LDAPFUNCDECL int ldap_search_s( LDAP *ld, char *base, int scope, char *filter,
	char **attrs, int attrsonly, LDAPMessage **res );
LDAPFUNCDECL int ldap_search_st( LDAP *ld, char *base, int scope, char *filter,
    char **attrs, int attrsonly, struct timeval *timeout, LDAPMessage **res );

LDAPFUNCDECL int ldap_ufn_search_c( LDAP *ld, char *ufn, char **attrs,
	int attrsonly, LDAPMessage **res, int (*cancelproc)( void *cl ),
	void *cancelparm );
LDAPFUNCDECL int ldap_ufn_search_ct( LDAP *ld, char *ufn, char **attrs,
	int attrsonly, LDAPMessage **res, int (*cancelproc)( void *cl ),
	void *cancelparm, char *tag1, char *tag2, char *tag3 );
LDAPFUNCDECL int ldap_ufn_search_s( LDAP *ld, char *ufn, char **attrs,
	int attrsonly, LDAPMessage **res );
LDAPFUNCDECL LDAPFiltDesc *ldap_ufn_setfilter( LDAP *ld, char *fname );
LDAPFUNCDECL void ldap_ufn_setprefix( LDAP *ld, char *prefix );
LDAPFUNCDECL int ldap_ufn_timeout( void *tvparam );

LDAPFUNCDECL int ldap_unbind( LDAP *ld );
LDAPFUNCDECL int ldap_unbind_s( LDAP *ld );

LDAPFUNCDECL LDAPFiltDesc *ldap_init_getfilter( char *fname );
LDAPFUNCDECL LDAPFiltDesc *ldap_init_getfilter_buf( char *buf, long buflen );
LDAPFUNCDECL LDAPFiltInfo *ldap_getfirstfilter( LDAPFiltDesc *lfdp, char *tagpat,
	char *value );
LDAPFUNCDECL LDAPFiltInfo *ldap_getnextfilter( LDAPFiltDesc *lfdp );
LDAPFUNCDECL void ldap_setfilteraffixes( LDAPFiltDesc *lfdp, char *prefix, char *suffix );
LDAPFUNCDECL void ldap_build_filter( char *buf, unsigned long buflen,
	char *pattern, char *prefix, char *suffix, char *attr,
	char *value, char **valwords );

LDAPFUNCDECL void ldap_getfilter_free( LDAPFiltDesc *lfdp );
LDAPFUNCDECL void ldap_mods_free( LDAPMod **mods, int freemods );

LDAPFUNCDECL char *ldap_friendly_name( char *filename, char *uname,
	FriendlyMap **map );
LDAPFUNCDECL void ldap_free_friendlymap( FriendlyMap **map );

LDAPFUNCDECL LDAP *cldap_open( char *host, int port );
LDAPFUNCDECL void cldap_close( LDAP *ld );
LDAPFUNCDECL int cldap_search_s( LDAP *ld, char *base, int scope, char *filter,
	char **attrs, int attrsonly, LDAPMessage **res, char *logdn );
LDAPFUNCDECL void cldap_setretryinfo( LDAP *ld, int tries, int timeout );

LDAPFUNCDECL int ldap_sort_entries( LDAP *ld, LDAPMessage **chain, char *attr,
	int (*cmp)() );
LDAPFUNCDECL int ldap_sort_values( LDAP *ld, char **vals, int (*cmp)() );
LDAPFUNCDECL int ldap_sort_strcasecmp( char **a, char **b );

LDAPFUNCDECL int ldap_is_ldap_url( char *url );
LDAPFUNCDECL int ldap_url_parse( char *url, LDAPURLDesc **ludpp );
LDAPFUNCDECL void ldap_free_urldesc( LDAPURLDesc *ludp );
LDAPFUNCDECL int ldap_url_search( LDAP *ld, char *url, int attrsonly );
LDAPFUNCDECL int ldap_url_search_s( LDAP *ld, char *url, int attrsonly,
	LDAPMessage **res );
LDAPFUNCDECL int ldap_url_search_st( LDAP *ld, char *url, int attrsonly,
	struct timeval *timeout, LDAPMessage **res );

#ifdef STR_TRANSLATION
LDAPFUNCDECL void ldap_set_string_translators( LDAP *ld,
	BERTranslateProc encode_proc, BERTranslateProc decode_proc );
LDAPFUNCDECL int ldap_translate_from_t61( LDAP *ld, char **bufp,
	unsigned long *lenp, int free_input );
LDAPFUNCDECL int ldap_translate_to_t61( LDAP *ld, char **bufp,
	unsigned long *lenp, int free_input );
LDAPFUNCDECL void ldap_enable_translation( LDAP *ld, LDAPMessage *entry,
	int enable );

#ifdef LDAP_CHARSET_8859
LDAPFUNCDECL int ldap_t61_to_8859( char **bufp, unsigned long *buflenp,
	int free_input );
LDAPFUNCDECL int ldap_8859_to_t61( char **bufp, unsigned long *buflenp,
	int free_input );
#endif /* LDAP_CHARSET_8859 */
#endif /* STR_TRANSLATION */

#ifdef __cplusplus
}
#endif
#endif /* _LDAP_H */
