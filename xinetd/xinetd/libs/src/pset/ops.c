/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include "pset.h"
#include <stdlib.h>

#define POINTER				__pset_pointer


/*
 * Remove all NULL pointers from a pset
 */
void pset_compact( register pset_h pset )
{
	register unsigned u ;

	for ( u = 0 ; u < pset_count( pset ) ; )
	{
		POINTER ptr = pset_pointer( pset, u );
		if ( ptr != NULL )
			u++ ;
		else
			pset_delete( pset, ptr ) ;
	}

	/* See if we can reclaim some memory, make sure we are 2 below for some hysteresis */
	if ((int)( pset->max - pset->alloc_step - 2) > (int)pset_count( pset ))
	{	/* This rounds up to the next unit of steps */
		POINTER *new_ptrs ;
		unsigned new_max = ((pset_count( pset ) / pset->alloc_step) + 1)*pset->alloc_step;

		new_ptrs = (POINTER *) realloc(
								(char *)pset->ptrs, new_max * sizeof( POINTER ) ) ;
		if ( new_ptrs == NULL )
			return;
		pset->max = new_max ;
		pset->ptrs = new_ptrs ;
	}
}


/*
 * Apply a function to all pointers of a pset
 */
void pset_apply( register pset_h pset, void (*func)(), register void *arg )
{
	register unsigned u ;

	for ( u = 0 ; u < pset_count( pset ) ; u++ )
		if ( arg )
			(*func)( arg, pset_pointer( pset, u ) ) ;
		else
			(*func)( pset_pointer( pset, u ) ) ;
}

