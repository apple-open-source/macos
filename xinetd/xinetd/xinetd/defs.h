/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef DEFS_H
#define DEFS_H

/*
 * $Id: defs.h,v 1.1.1.1 2002/01/31 07:09:05 zarzycki Exp $
 */


#include "config.h"
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

union xsockaddr {
   struct sockaddr     sa;
   struct sockaddr_in  sa_in;
   struct sockaddr_in6 sa_in6;
   char                pad[128];
};

#ifndef FALSE
#define FALSE                           0
#define TRUE                            1
#endif

#define NUL                             '\0'

#define ES_NOMEM                        "out of memory"

#define INT_NULL                        ((int *)0)
#define CHAR_NULL                       ((char *)0)
#define VOID_NULL                       ((void *)0)
#define FD_SET_NULL                     ((fd_set *)0)
#define RUSAGE_NULL                     ((struct rusage *)0)
#define TIMEVAL_NULL                    ((struct timeval *)0)

#define EQ( s1, s2 )          ( strcasecmp( s1, s2 ) == 0 )
#define CLEAR( x )            (void) memset( (char *)&(x), 0, sizeof( x ) )
/* Apparently, some tcp wrapper header files export an SA definition.
 * make sure we use ours instead of some other one.
 */
#undef SA

#define SA( p )               ( (struct sockaddr *) (p) )
#define SAIN( p )             ( (struct sockaddr_in *) (p) )
#define SAIN6( p )            ( (struct sockaddr_in6 *) (p) )
#define NEW( type )           (type *) malloc( sizeof( type ) )
#define FREE( p )             (void) free( (char *)(p) )

/*
 * Value for unlimited server instances
 */
#define UNLIMITED                  (-1)

/*
 * We pass to the child the descriptors 0..MAX_PASS_FD
 */
#define MAX_PASS_FD                2

/*
 * Service port for the identification service
 */
#define IDENTITY_SERVICE_PORT      113

/*
 * This is the signal sent to interceptor processes to tell them
 * to stop intercepting
 */
#define INTERCEPT_SIG              SIGUSR1

/*
 * This is how many descriptors we reserve for ourselves:
 *
 *      3    for stdin, stdout, stderr
 *      1    for syslog/debug
 *
 * For the rest we just need to reserve the maximum of each category.
 *
 *   1    for doing accepts
 *   1    for registering rpc services (initialization phase)
 *   4    for reading the configuration file during reconfiguration
 *      1 for the configuration file
 *      1 for /etc/passwd
 *      1 for /etc/group
 *      1 for /etc/services, /etc/protocols, /etc/rpc
 *      NOTE: We need only 1 descriptor for the last 3 files because
 *         the functions get{serv,proto,rpc}byname close the
 *         respective files after accessing them.
 *      1    for dumping the internal state
 *      1   for talking to the portmapper (reconfiguration phase)
 *      1   for doing identification
 *
 * NOTE: we assume that the socket used for pmap_{set,unset} is closed
 *      after the operation is completed. If it stays open, then we
 *      need to increase DESCRIPTORS_RESERVED.
 */
#define DESCRIPTORS_RESERVED         8

/*
 * Used for listen(2)
 */
#define LISTEN_BACKLOG               7

/*
 * When explicit values are given for enum's, that is because the structures 
 * that the enum's are in may be initialized by a memory clear operation.
 */

typedef enum { FAILED = 0, OK } status_e ;
typedef enum { NO = 0, YES } boolean_e ;

/*
 * Possible outcomes of an identification attempt
 */
typedef enum
   {
      IDR_OK,
      IDR_NOSERVER,
      IDR_TIMEDOUT,
      IDR_RESPERR,
      IDR_BADRESP,
      IDR_ERROR
   } idresult_e ;

typedef int bool_int ;

typedef void (*voidfunc)() ;
typedef status_e (*statfunc)() ;


/*
 * A name-value list is exactly what its name says.
 * The functions nv_get_name() and nv_get_value() return a pointer to
 * the entry with the specified value or name respectively.
 * The list ends when an antry with a NULL name is encountered.
 * The value field of that entry is treated in a special manner: if it
 * is non-zero, it is assumed that there exists one more entry whose
 * name field will be returned by the nv_get_name function if it can't
 * find an entry whose value field is equal to its 2nd parameter.
 * If the value field of the NULL entry is 0, then nv_get_name() will
 * return NULL.
 */
struct name_value
{
   char   *name ;
   int    value ;
} ;


struct debug
{
   bool_int on ;
   int fd ;
} ;

/* This is some forward prototypes to work out a couple
 * circular dependencies in the data structures */
struct service;
struct server;
struct connection;
typedef struct connection connection_s ;


extern struct debug debug ;

#endif   /* DEFS_H */
