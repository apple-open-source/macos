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
 *  Copyright (c) 1995 Regents of the University of Michigan.
 *  Copyright (c) 1997 Apple Computer, Inc.
 *  All rights reserved.
 *
 *  ldap-int.h - defines & prototypes internal to the LDAP library
 *
 *  We now use this to make the LDAP structure opaque to users of
 *  the library.
 */

#ifndef _LDAP_INT_H
#define _LDAP_INT_H

#include <sys/time.h>

#ifndef NEEDPROTOS
#define NEEDPROTOS
#endif /* NEEDPROTOS */

/* SDK version 3.5.  */
#define SDK_VERSION	(350)

struct ldapmsg {
	int		lm_msgid;	/* the message id */
	int		lm_msgtype;	/* the message type */
	BerElement	*lm_ber;	/* the ber encoded message contents */
	struct ldapmsg	*lm_chain;	/* for search - next msg in the resp */
	struct ldapmsg	*lm_next;	/* next response */
	unsigned long	lm_time;	/* used to maintain cache */
};

/*
 * structure for tracking LDAP server host, ports, DNs, etc.
 */
typedef struct ldap_server {
	char			*lsrv_host;
	char			*lsrv_dn;	/* if NULL, use default */
	int			lsrv_port;
	struct ldap_server	*lsrv_next;
} LDAPServer;

/*
 * structure for client cache
 */
#define LDAP_CACHE_BUCKETS	31	/* cache hash table size */
typedef struct ldapcache {
	struct ldapmsg	*lc_buckets[LDAP_CACHE_BUCKETS];/* hash table */
	struct ldapmsg	*lc_requests;			/* unfulfilled reqs */
	long		lc_timeout;			/* request timeout */
	long		lc_maxmem;			/* memory to use */
	long		lc_memused;			/* memory in use */
	int		lc_enabled;			/* enabled? */
	unsigned long	lc_options;			/* options */
#define LDAP_CACHE_OPT_CACHENOERRS	0x00000001
#define LDAP_CACHE_OPT_CACHEALLERRS	0x00000002
}  LDAPCache;
#define NULLLDCACHE ((LDAPCache *)NULL)

/*
 * structure for representing an LDAP server connection
 */
typedef struct ldap_conn {
	Sockbuf			*lconn_sb;
	int			lconn_refcnt;
	unsigned long		lconn_lastused;	/* time */
	int			lconn_status;
#define LDAP_CONNST_NEEDSOCKET		1
#define LDAP_CONNST_CONNECTING		2
#define LDAP_CONNST_CONNECTED		3
	LDAPServer		*lconn_server;
	char			*lconn_krbinstance;
	struct ldap_conn	*lconn_next;
} LDAPConn;

/*
 * structure used to track outstanding requests
 */
typedef struct ldapreq {
	int		lr_msgid;	/* the message id */
	int		lr_status;	/* status of request */
#define LDAP_REQST_INPROGRESS	1
#define LDAP_REQST_CHASINGREFS	2
#define LDAP_REQST_NOTCONNECTED	3
#define LDAP_REQST_WRITING	4
	int		lr_outrefcnt;	/* count of outstanding referrals */
	int		lr_origid;	/* original request's message id */
	int		lr_parentcnt;	/* count of parent requests */
	int		lr_res_msgtype;	/* result message type */
	int		lr_res_errno;	/* result LDAP errno */
	char		*lr_res_error;	/* result error string */
	char		*lr_res_matched;/* result matched DN string */
	BerElement	*lr_ber;	/* ber encoded request contents */
	LDAPConn	*lr_conn;	/* connection used to send request */
	struct ldapreq	*lr_parent;	/* request that spawned this referral */
	struct ldapreq	*lr_refnext;	/* next referral spawned */
	struct ldapreq	*lr_prev;	/* previous request */
	struct ldapreq	*lr_next;	/* next request */
} LDAPRequest;

#define LDAP_MAX_ATTR_LEN	100
/*
 * structure representing an ldap connection
 */
struct ldap {
	Sockbuf		ld_sb;		/* socket descriptor & buffer */
	char		*ld_host;
	int		ld_version;
	char		ld_lberoptions;
	int		ld_deref;
	int		ld_timelimit;
	int		ld_sizelimit;

	void		*ld_filtd;	/* from getfilter for ufn searches */
	char		*ld_ufnprefix;	/* for incomplete ufn's */

	int		ld_errno;
	char		*ld_error;
	char		*ld_matched;
	int		ld_msgid;

	/* do not mess with these */
	LDAPRequest	*ld_requests;	/* list of outstanding requests */
	struct ldapmsg	*ld_responses;	/* list of outstanding responses */
	int		*ld_abandoned;	/* array of abandoned requests */
	char		ld_attrbuffer[LDAP_MAX_ATTR_LEN];
	LDAPCache	*ld_cache;	/* non-null if cache is initialized */
#ifdef CLDAP
	char		*ld_cldapdn;	/* DN used in connectionless search */
#endif
	char		*ld_defbase;	/* default search base */
	/* it is OK to change these next four values directly */
	int		ld_cldaptries;	/* connectionless search retry count */
	int		ld_cldaptimeout;/* time between retries */
	int		ld_refhoplimit;	/* limit on referral nesting */
	unsigned long	ld_options;	/* boolean options */
#ifdef LDAP_DNS
#define LDAP_INTERNAL_OPT_DNS		0x00000001	/* use DN & DNS */
#endif /* LDAP_DNS */
#define LDAP_INTERNAL_OPT_REFERRALS	0x00000002	/* chase referrals */
#define LDAP_INTERNAL_OPT_RESTART	0x00000004	/* restart if EINTR occurs */

	/* do not mess with the rest though */
	char		*ld_defhost;	/* names of default server */
	int		ld_defport;	/* port of default server */
	BERTranslateProc ld_lber_encode_translate_proc;
	BERTranslateProc ld_lber_decode_translate_proc;
	LDAPConn	*ld_defconn;	/* default connection */
	LDAPConn	*ld_conns;	/* list of server connections */
	void		*ld_selectinfo;	/* platform specifics for select */
	int (*ld_rebindproc)(struct ldap *, char **, char **, int *, int, void *);	
	void		*ld_rebindarg;
};

#define LDAP_URL_PREFIX         "ldap://"
#define LDAP_URL_PREFIX_LEN     7
#define LDAP_URL_URLCOLON	"URL:"
#define LDAP_URL_URLCOLON_LEN	4

#ifdef LDAP_REFERRALS
#define LDAP_REF_STR		"Referral:\n"
#define LDAP_REF_STR_LEN	10
#define LDAP_LDAP_REF_STR	LDAP_URL_PREFIX
#define LDAP_LDAP_REF_STR_LEN	LDAP_URL_PREFIX_LEN
#ifdef LDAP_DNS
#define LDAP_DX_REF_STR		"dx://"
#define LDAP_DX_REF_STR_LEN	5
#endif /* LDAP_DNS */
#endif /* LDAP_REFERRALS */


/*
 * in cache.c
 */
void add_request_to_cache( struct ldap *ld, unsigned long msgtype,
        BerElement *request );
void add_result_to_cache( struct ldap *ld, struct ldapmsg *result );
int check_cache( struct ldap *ld, unsigned long msgtype, BerElement *request );

#ifdef KERBEROS
/*
 * in kerberos.c
 */
char *get_kerberosv4_credentials( struct ldap *ld, char *who, char *service,
        int *len );
#endif /* KERBEROS */


/*
 * in open.c
 */
int open_ldap_connection( struct ldap *ld, Sockbuf *sb, char *host, int defport,
	char **krbinstancep, int async );


/*
 * in os-ip.c
 */
int connect_to_host( Sockbuf *sb, char *host, unsigned long address, int port,
	int async );
void close_connection( Sockbuf *sb );

#ifdef KERBEROS
char *host_connected_to( Sockbuf *sb );
#endif /* KERBEROS */

int do_ldap_select( struct ldap *ld, struct timeval *timeout );
void *new_select_info( void );
void free_select_info( void *sip );
void mark_select_write( struct ldap *ld, Sockbuf *sb );
void mark_select_read( struct ldap *ld, Sockbuf *sb );
void mark_select_clear( struct ldap *ld, Sockbuf *sb );
int is_read_ready( struct ldap *ld, Sockbuf *sb );
int is_write_ready( struct ldap *ld, Sockbuf *sb );

/*
 * in request.c
 */
int send_initial_request( struct ldap *ld, unsigned long msgtype,
	char *dn, BerElement *ber );
BerElement *alloc_ber_with_options( struct ldap *ld );
void set_ber_options( struct ldap *ld, BerElement *ber );

int send_server_request( struct ldap *ld, BerElement *ber, int msgid,
	LDAPRequest *parentreq, LDAPServer *srvlist, LDAPConn *lc,
	int bind );
LDAPConn *new_connection( struct ldap *ld, LDAPServer **srvlistp, int use_ldsb,
	int connect, int bind );
LDAPRequest *find_request_by_msgid( struct ldap *ld, int msgid );
void free_request( struct ldap *ld, LDAPRequest *lr );
void free_connection( struct ldap *ld, LDAPConn *lc, int force, int unbind );
void dump_connection( struct ldap *ld, LDAPConn *lconns, int all );
void dump_requests_and_responses( struct ldap *ld );

int chase_referrals( struct ldap *ld, LDAPRequest *lr, char **errstrp, int *hadrefp );
int append_referral( struct ldap *ld, char **referralsp, char *s );


/*
 * in search.c
 */
BerElement *ldap_build_search_req( struct ldap *ld, char *base, int scope,
	char *filter, char **attrs, int attrsonly );

/*
 * in unbind.c
 */
int ldap_ld_free( struct ldap *ld, int close );
int send_unbind( struct ldap *ld, Sockbuf *sb );

#ifdef LDAP_DNS
/*
 * in getdxbyname.c
 */
char **getdxbyname( char *domain );
#endif /* LDAP_DNS */

#if defined( STR_TRANSLATION ) && defined( LDAP_DEFAULT_CHARSET )
/*
 * in charset.c
 *
 * added-in this stuff so that libldap.a would build, i.e. refs to 
 * these routines from open.c would resolve. 
 * hodges@stanford.edu 5-Feb-96
 */
#if LDAP_CHARSET_8859 == LDAP_DEFAULT_CHARSET
extern 
int ldap_t61_to_8859( char **bufp, unsigned long *buflenp, int free_input );
extern 
int ldap_8859_to_t61( char **bufp, unsigned long *buflenp, int free_input );
#endif /* LDAP_CHARSET_8859 == LDAP_DEFAULT_CHARSET */
#endif /* STR_TRANSLATION && LDAP_DEFAULT_CHARSET */

/* 
 * Paths for Rhapsody. /System/Library/LDAP/Configuration/foo.conf.
 */
char *ldap_locate_path( char *pathbuf, const char *file );

#define SUFFIX_LIBRARYDIR	"/LDAP/Configuration"
#define SUFFIX_FILTERFILE	SUFFIX_LIBRARYDIR"/ldapfilter.conf"
#define SUFFIX_TEMPLATEFILE	SUFFIX_LIBRARYDIR"/ldaptemplates.conf"
/*
#define SUFFIX_SEARCHFILE	SUFFIX_LIBRARYDIR"/ldapsearchprefs.conf"
#define SUFFIX_FRIENDLYFILE	SUFFIX_LIBRARYDIR"/ldapfriendly"
 */
#endif /* _LDAP_INT_H */
