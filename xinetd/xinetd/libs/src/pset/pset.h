/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef __PSET_H
#define __PSET_H

/*
 * $Id: pset.h,v 1.1.1.3 2003/05/22 01:16:35 rbraun Exp $
 */

#include <stdlib.h>

typedef void *__pset_pointer ;

struct __pset
{
   unsigned alloc_step ;
   __pset_pointer *ptrs ;   /* void Pointer to a Pointer */
   unsigned max ;
   unsigned count ;
} ;


/*
 * INTERFACE
 */

typedef struct __pset *pset_h ;

pset_h pset_create( unsigned alloc_start, unsigned alloc_step );
void pset_destroy( pset_h pset );

void pset_delete( pset_h pset, const __pset_pointer ptr );
__pset_pointer pset_add( pset_h pset, const __pset_pointer ptr );

/* These 2 are in ops.c   */
void pset_compact( pset_h pset );
void pset_apply( pset_h pset, void (*func)(), void *arg );

/*
 * Macros
 */

#define pset_remove( pset, ptr )   pset_delete( pset, (__pset_pointer)(ptr) )

#define pset_remove_index( pset, i )                          \
      {                                                       \
         if ( ((unsigned)i) < (pset)->count )                 \
             pset_delete(pset, (pset)->ptrs[ (unsigned)(i) ]);  \
      }

#define pset_clear( pset )           (pset)->count = 0
#define pset_count( pset )           (pset)->count
#define pset_pointer( pset, i )      (pset)->ptrs[ (unsigned)(i) ]

#define pset_sort( pset, compfunc )                         \
      (void) qsort( (char *) &pset_pointer( pset, 0 ),      \
            pset_count( pset ), sizeof( __pset_pointer ), compfunc )

/*
 * PSET iterators
 *
 * Note that the iterators do NOT use any knowledge about the internals
 * of pset's.
 */
struct __pset_iterator
{
   pset_h pset ;
   unsigned current ;
   int step ;
} ;

typedef struct __pset_iterator *psi_h ;
void psi_remove( psi_h iter );

#define __psi_current( iter )                             \
   ( (iter)->current < pset_count( (iter)->pset )         \
      ? pset_pointer( (iter)->pset, (iter)->current )     \
      : NULL )

#define psi_start( iter )                                 \
      ( (iter)->current = 0, (iter)->step = 1,            \
               __psi_current( iter ) )

#define psi_next( iter )                   \
      ( (iter)->current += (iter)->step, (iter)->step = 1,   \
               __psi_current( iter ) )

#define psi_destroy( iter )          free( (char *) iter )

psi_h psi_create( pset_h pset );

#endif   /* __PSET_H */
