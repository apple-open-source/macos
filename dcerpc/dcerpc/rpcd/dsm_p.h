/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this software have been released under the following terms:
 *
 * (c) Copyright 1989-1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989-1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989-1993 DIGITAL EQUIPMENT CORPORATION
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 * permission to use, copy, modify, and distribute this file for any
 * purpose is hereby granted without fee, provided that the above
 * copyright notices and this notice appears in all source code copies,
 * and that none of the names of Open Software Foundation, Inc., Hewlett-
 * Packard Company or Digital Equipment Corporation be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Neither Open Software
 * Foundation, Inc., Hewlett-Packard Company nor Digital
 * Equipment Corporation makes any representations about the suitability
 * of this software for any purpose.
 *
 * Copyright (c) 2007, Novell, Inc. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Novell Inc. nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
**
**  NAME:
**
**      dsm_p.h
**
**  FACILITY:
**
**      Data Storage Manager (DSM)
**
**  ABSTRACT:
**
**  Data storage manager private header file.
**
**
*/

#include  <dce/dce.h>
#define DCETHREAD_CHECKED
#define DCETHREAD_USE_THROW
#include  <dce/dcethread.h>
#include  <dsm.h>
#include  <assert.h>
#include  <stddef.h>
#include  <errno.h>
#include  <sys/file.h>

/*
 * lseek direction macros
 */
#ifndef L_SET
#define L_SET           0               /* absolute offset */
#endif

#ifndef L_INCR
#define L_INCR          1               /* relative to current offset */
#endif

#ifndef L_XTND
#define L_XTND          2               /* relative to end of file */
#endif

#define status_ok error_status_ok
#define boolean ndr_boolean

/*  Records in the data store file are laid out contiguously.  Each record begins with
    a preheader used by DSM (not seen by user).  Each record is padded to a length that
    ensures clean alignment (at least 8-byte); the preheader is of such a length that
    the user data is also cleanly aligned after the preheader.

    We want to arrange that our header fits within a page, as well as a reasonable chunk
    of the beginning of the user data, so the client can have atomic header writes.
*/

#if defined (vms)
#   define PAGE_SIZE   512              /* length of system page */
#else
#if defined(__linux__)
#   define PAGE_SIZE   4096
#elif !defined(PAGE_SIZE)
#   define PAGE_SIZE   1024             /* length of system page */
#endif
#endif

#define INFOSZ      256                 /* space reserved for client header info */
#define PREHEADER   (sizeof(block_t) - sizeof(double))                  /* length of preheader */
#define UNIT        64                  /* 1st UNIT of each block should fit within a page */
#define USER_HDR    (UNIT-PREHEADER)    /* leaving this for a user header */
#define MINBLOCK    (PREHEADER+8)       /* we'll deal with blocks as small as this */
#define GROW_PAGES  5                   /* growth unit */
#define HDR_COOKIE  0xA5                /* magic cookie in preheaders */
#define DSM_COOKIE  0xADEADBEEU          /* magic cookie in dsh */
#define MAX_PATH    1024                /* maximum pathname length */

#define MAGIC_MARKER (unsigned32)dsm_magic_marker  /* "magic", invalid marker */

/* rounding/modulus operations on powers of 2 */
#define ROUND_UP(n,po2)     (((n)+((po2)-1))&(~((po2)-1)))  /* round n up to next po2 */
#define ROUND_DOWN(n,po2)   ((n)&(~((po2)-1)))            /* round down to prev po2 */
#define MOD(n,po2)          ((n)&((po2)-1))             /* n mod po2 */

#ifndef MAX
#define MAX(a,b)    ((a>b)?a:b)
#endif
#ifndef MIN
#define MIN(a,b)    ((a<b)?a:b)
#endif

#define NEW(type)   (type *)malloc(sizeof(type))

#define ustrlcpy(a,b,c)    strlcpy((char *)(a),(char *)(b),c)
#define ustrlcat(a,b,c)    strlcat((char *)(a),(char *)(b),c)
#define ustrlen(a)      strlen((char *)(a))
#define ustrcmp(a,b)    strcmp((char *)(a),(char *)(b))

#define BAD_ST      ((*st) != status_ok)
#define GOOD_ST     ((*st) == status_ok)
#define CLEAR_ST    ((*st) = status_ok)

/*  Cleanup handling.  Earlier model was based on Apollo PFM, no current
    contender fits that model (combining exception handling and status codes)
    so here's a placeholder using strictly local gotos (pfm is based on
    nonlocal gotos aka longjmp).  Assumes error_status_t *st in scope.

    CLEANUP {
        stuff
        return;
    }
    ...
    if (bad_thing) SIGNAL(error_status);
*/

#define CLEANUP     if (0) CH_LABEL:
#define SIGNAL(s)   { (*st) = (s); goto CH_LABEL; }
#define PROP_BAD_ST if (BAD_ST) SIGNAL(*st) /* propagate (via signal) bad status */

#define private static
#define public

#if defined(_HPUX)
/* HP-UX hack around system-defined page_t */
#define page_t my_page_t
#endif

typedef struct page_t {     /* generic page */
    unsigned char page_contents[PAGE_SIZE];
} page_t;

/*  Strong assumptions are made about the size and alignments of this
    structure!  The important thing is that it the 'data' field be
    naturally aligned for all potential user data (8-byte alignment),
    and the preheader should occupy the PREHEADER bytes just before
    the user data.  It currently looks like: (16 bytes)

        +--------+--------+--------+--------+  \
        |         space for link ptr        |   |
        +--------+--------+--------+--------+   |
        |         size of user data         |   |
        +--------+--------+--------+--------+    > preheader (16 bytes)
        |     offset in file of preheader   |   |
        +--------+--------+--------+--------+   |
        |  FREE  | cookie |    (unused)     |   |
        +--------+--------+--------+--------+  /
        |  user data...
        +--------+
*/

typedef struct block_t {        /* block preheader */
    struct block_t *link;       /* link to next block on (free) list [meaningless in file] */
    unsigned long   size;       /* size of user data */
    unsigned long   loc;        /* location (offset) of preheader in file */
    boolean         isfree;     /* true iff free */
    unsigned char   cookie;     /* magic number basic identification */
    unsigned char   unused[2];  /* preheader ends here */
    double          data;       /* user data begins here -- double to align */
} block_t;

typedef struct file_hdr_t {     /* first page of file contains global info */
    long            version;    /* file format version */
    long            pages;      /* number of initialized data pages */
    long            pad1[20];   /* reserve for DSM header expansion */
    unsigned char   info[INFOSZ];   /* space for client info */
    page_t          padding;    /* pad out past page boundary */
} file_hdr_t;

/*  Upon opening a data store file we allocate a chunk of memory big
    enough to hold the whole thing and read it in.  As the file grows
    we allocate smaller chunks corresponding to new pieces of the
    file.  In memory most access will be via lists, but for sequential
    traversal we have to find all chunks (also for freeing upon close)
    so we keep a list of chunks in order in the file.
*/
typedef struct file_map_t {     /* file chunk descriptor */
    struct file_map_t  *link;   /* next in list */
    block_t            *ptr;    /* pointer to first block in chunk */
    unsigned long       loc;    /* location in file (should be on page boundary) */
    unsigned long       size;   /* bytes total (should be in page multiple) */
} file_map_t;

typedef struct cache_t {        /* dsm_read cache element */
    dsm_marker_t   loc;        /* location in file */
    block_t        *p;          /* block in memory */
} cache_t;

typedef struct dsm_db_t {       /* dsm handle info (what dsm_handle_t really points to) */
    block_t        *freelist;   /* free block list */
    int             fd;         /* the file descriptor */
    char           *fname;      /* pointer to malloc'd copy of filename */
    file_map_t     *map;        /* the file map (head of list) */
    long            pages;      /* initialized pages (from file header) */
    long            cookie;     /* magic cookie for detecting bogus dsh's */
    int             pending;    /* # blocks allocated but not written */
    cache_t         cache;      /* dsm_read cache */
    boolean         coalesced;  /* true once coalesced */
} dsm_db_t;

typedef struct dsm_db_t * dsm_handle;   /* internal version of opaque handle */

/* private function prototypes; copied here to force type checking and loosen
   sequence.
*/

#ifdef _I_AM_DSM_C_
private block_t *   get_free_block      (dsm_handle,unsigned long);
private block_t *   grow_file           (dsm_handle,unsigned long, error_status_t *);
private void        write_header        (dsm_handle,block_t *, error_status_t *);
private void        write_block         (dsm_handle,block_t *,unsigned long, error_status_t *);
private void        update_file_header  (dsm_handle, error_status_t *);
private int         create_file         (unsigned char *);
private void        make_free           (dsm_handle,block_t *, error_status_t *);
private void        free_block          (dsm_handle,block_t *);
private void        free_map            (file_map_t *);
private void        coalesce            (dsm_handle, error_status_t *);
private void        build_freelist      (dsm_handle);
private block_t *   get_next_block      (dsm_handle,block_t *);
private block_t *   block_from_ptr      (void *, error_status_t *);
private block_t *   get_earlier_block   (dsm_handle,dsm_marker_t);
private void        cache_clear         (dsm_handle);
private void        cache_add           (dsm_handle,block_t *,dsm_marker_t);
private block_t *   cache_lookup        (dsm_handle,dsm_marker_t);
#endif
public void         dsm__lock_file      (int, error_status_t *);
public int          dsm__flush_file     (int);
