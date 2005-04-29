/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

#pragma prototyped
/* Lefteris Koutsofios - AT&T Bell Laboratories */

#ifndef _IO_H
#define _IO_H

typedef enum {
    IO_FILE, IO_PTY, IO_PIPE, IO_SOCKET,
#ifdef FEATURE_CS
    IO_CS,
#endif
    IO_SIZE
} iotype_t;

typedef struct io_t {
    int inuse, ismonitored;
    iotype_t type;
    FILE *ifp, *ofp;
    int pid;
    char *buf;
} io_t;

#ifdef FEATURE_MS
#define IOmonitor(ioi, set) do { \
    iop[ioi].ismonitored = TRUE; \
} while (0)
#define IOunmonitor(ioi, set) do { \
    iop[ioi].ismonitored = FALSE; \
} while (0)
#else
#define IOmonitor(ioi, set) do { \
    iop[ioi].ismonitored = TRUE; \
    FD_SET (fileno (iop[ioi].ifp), &set); \
} while (0)
#define IOunmonitor(ioi, set) do { \
    iop[ioi].ismonitored = FALSE; \
    FD_CLR (fileno (iop[ioi].ifp), &set); \
} while (0)
#endif

#define IOismonitored(ioi) (iop[ioi].ismonitored == TRUE)
#define IOINCR 5
#define IOSIZE sizeof (io_t)
#define IOBUFSIZE 2048

extern io_t *iop;
extern int ion;

void IOinit (void);
void IOterm (void);
int IOopen (char *, char *, char *, char *);
int IOclose (int, char *);
int IOreadline (int, char *, int);
int IOread (int, char *, int);
int IOwriteline (int, char *);

#endif /* _IO_H */
