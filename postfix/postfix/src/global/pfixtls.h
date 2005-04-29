/*++
/* NAME
/*      pfixtls 3h
/* SUMMARY
/*      TLS routines
/* SYNOPSIS
/*      include "pfixtls.h"
/* DESCRIPTION
/* .nf
/*--*/

#ifndef PFIXTLS_H_INCLUDED
#define PFIXTLS_H_INCLUDED

#if defined(HAS_SSL) && !defined(USE_SSL)
#define USE_SSL
#endif

typedef struct {
    int     peer_verified;
    int     hostname_matched;
    char   *peer_subject;
    char   *peer_issuer;
    char   *peer_fingerprint;
    char   *peer_CN;
    char   *issuer_CN;
    const char *protocol;
    const char *cipher_name;
    int     cipher_usebits;
    int     cipher_algbits;
} tls_info_t;

extern const tls_info_t tls_info_zero;

#ifdef USE_SSL

typedef struct {
    long scache_db_version;
    long openssl_version;
    time_t timestamp;		/* We could add other info here... */
    int enforce_peername;
} pfixtls_scache_info_t;

extern const long scache_db_version;
extern const long openssl_version;

int     pfixtls_timed_read(int fd, void *buf, unsigned len, int timout,
			   void *unused_timeout);
int     pfixtls_timed_write(int fd, void *buf, unsigned len, int timeout,
			    void *unused_timeout);

extern int pfixtls_serverengine;
int     pfixtls_init_serverengine(int verifydepth, int askcert);
int     pfixtls_start_servertls(VSTREAM *stream, int timeout,
				const char *peername, const char *peeraddr,
				tls_info_t *tls_info, int require_cert);
int     pfixtls_stop_servertls(VSTREAM *stream, int timeout, int failure,
			       tls_info_t *tls_info);

extern int pfixtls_clientengine;
int     pfixtls_init_clientengine(int verifydepth);
int     pfixtls_start_clienttls(VSTREAM *stream, int timeout,
				int enforce_peername,
				const char *peername,
				tls_info_t *tls_info);
int     pfixtls_stop_clienttls(VSTREAM *stream, int timeout, int failure,
			       tls_info_t *tls_info);

#endif /* PFIXTLS_H_INCLUDED */
#endif

/* LICENSE
/* .ad
/* .fi
/* AUTHOR(S)
/*	Lutz Jaenicke
/*	BTU Cottbus
/*	Allgemeine Elektrotechnik
/*	Universitaetsplatz 3-4
/*	D-03044 Cottbus, Germany
/*--*/
