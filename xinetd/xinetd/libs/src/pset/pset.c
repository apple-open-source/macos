/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <stdlib.h>
#include "pset.h"


#define ALLOC_START				20
#define ALLOC_STEP				10


static __pset_pointer pset_insert( pset_h pset, const __pset_pointer p );


/*
 * Create a pointer set and return a handle to it.
 * Some space is initially allocated for the set.
 */
pset_h pset_create( unsigned alloc_start, unsigned alloc_step )
{
	pset_h pset ;
	unsigned start ;

	pset = (pset_h) malloc( sizeof( struct __pset ) ) ;
	if ( pset == NULL )
		return( NULL ) ;
	
	start = ( alloc_start == 0 ) ? ALLOC_START : alloc_start ;
	pset->ptrs = (__pset_pointer *) malloc( start * sizeof( __pset_pointer ) ) ;
	if ( pset->ptrs == NULL )
	{
		free( (char *) pset ) ;
		return( NULL ) ;
	}

	pset->max = start ;
	pset->count = 0 ;
	pset->alloc_step = ( alloc_step == 0 ) ? ALLOC_STEP : alloc_step ;
	return( pset ) ;
}


/*
 * Destroy a pset
 */
void pset_destroy( pset_h pset )
{
	if ( pset == NULL )
		return;
	free( (char *) pset->ptrs ) ;
	free( (char *) pset ) ;
}

__pset_pointer pset_add( pset_h pset, const __pset_pointer ptr )
{
	return (pset->count < pset->max )
		? (pset->ptrs[ pset->count++ ] = ptr)
			: pset_insert( pset, ptr) ;
}

/*
 * Append a pointer to a pset
 */
static __pset_pointer pset_insert( pset_h pset, const __pset_pointer p )
{
	if ( pset->count >= pset->max )
	{
		unsigned new_max = pset->max + pset->alloc_step ;
		__pset_pointer *new_ptrs ;

		new_ptrs = (__pset_pointer *) realloc(
			(char *)pset->ptrs, new_max * sizeof( __pset_pointer ) ) ;
		if ( new_ptrs == NULL )
			return( NULL ) ;
		pset->max = new_max ;
		pset->ptrs = new_ptrs ;
	}
	return( pset->ptrs[ pset->count++ ] = p ) ;
}


/*
 * Remove a pointer from a pset by moving every thing above it down 1 spot.
 */
void pset_delete( register pset_h pset, register const __pset_pointer ptr )
{
	register unsigned u = 0;
	register int found_it = 0;

	if ( pset->count == 0 )
		return ;

	while ( u < pset->count )
	{
		if ( pset->ptrs[ u ] == ptr )
			found_it = 1;
		if ( found_it )
		{	/* If not the last one, copy it */
			if ( (u+1) < pset->count )
				pset->ptrs[ u ] = pset->ptrs[ u+1 ];
		}
		u++;
	}
	pset->count--;
}


/*
 * Create a pset iterator
 */
psi_h psi_create( pset_h pset )
{
	psi_h iter = (psi_h) malloc( sizeof( struct __pset_iterator ) ) ;

	if ( iter == NULL )
		return( NULL ) ;
	iter->pset = pset ;
	return( iter ) ;
}

/*
 * Remove an element from a pset
 */
void psi_remove( psi_h iter )
{
	if ( iter->current < pset_count( iter->pset ) )
	{
		pset_remove_index( iter->pset, iter->current ) ;
		iter->step = 0;
	}
}

