/*++
/* NAME
/*      sdbm 3h
/* SUMMARY
/*      SDBM Simple DBM: ndbm work-alike hashed database library
/* SYNOPSIS
/*      include "sdbm.h"
/* DESCRIPTION
/* .nf
/*--*/

#ifndef UTIL_SDBM_H
#define UTIL_SDBM_H

/*
 * sdbm - ndbm work-alike hashed database library
 * based on Per-Ake Larson's Dynamic Hashing algorithms. BIT 18 (1978).
 * author: oz@nexus.yorku.ca
 * status: public domain.
 */

#define DUFF    /* go ahead and use the loop-unrolled version */

#include <stdio.h>

#define DBLKSIZ 16384                   /* SSL cert chains require more */
#define PBLKSIZ 8192                    /* SSL cert chains require more */
#define PAIRMAX 8008                    /* arbitrary on PBLKSIZ-N */
#define SPLTMAX 10                      /* maximum allowed splits */
                                        /* for a single insertion */
#define DIRFEXT ".dir"
#define PAGFEXT ".pag"

typedef struct {
        int dirf;                      /* directory file descriptor */
        int pagf;                      /* page file descriptor */
        int flags;                     /* status/error flags, see below */
        long blkptr;                   /* current block for nextkey */
        int keyptr;                    /* current key for nextkey */
        char pagbuf[PBLKSIZ];          /* page file block buffer */
        char dirbuf[DBLKSIZ];          /* directory file block buffer */
} SDBM;

#define DBM_RDONLY      0x1            /* data base open read-only */
#define DBM_IOERR       0x2            /* data base I/O error */

/*
 * utility macros
 */
#define sdbm_rdonly(db)         ((db)->flags & DBM_RDONLY)
#define sdbm_error(db)          ((db)->flags & DBM_IOERR)

#define sdbm_clearerr(db)       ((db)->flags &= ~DBM_IOERR)  /* ouch */

#define sdbm_dirfno(db) ((db)->dirf)
#define sdbm_pagfno(db) ((db)->pagf)

typedef struct {
        char *dptr;
        int dsize;
} datum;

extern datum nullitem;

/*
 * flags to sdbm_store
 */
#define DBM_INSERT      0
#define DBM_REPLACE     1

/*
 * ndbm interface
 */
extern SDBM *sdbm_open(char *, int, int);
extern void sdbm_close(SDBM *);
extern datum sdbm_fetch(SDBM *, datum);
extern int sdbm_delete(SDBM *, datum);
extern int sdbm_store(SDBM *, datum, datum, int);
extern datum sdbm_firstkey(SDBM *);
extern datum sdbm_nextkey(SDBM *);

/*
 * sdbm - ndbm work-alike hashed database library
 * tuning and portability constructs [not nearly enough]
 * author: oz@nexus.yorku.ca
 */

#define BYTESIZ         8

/*
 * important tuning parms (hah)
 */

#define SEEDUPS                 /* always detect duplicates */
#define BADMESS                 /* generate a message for worst case:
                                   cannot make room after SPLTMAX splits */
#endif /* UTIL_SDBM_H */
