/*
 * (c) Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include <time.h>

#include "config.h"
#include "xtimer.h"
#include "pset.h"
#include "msg.h"

/* A note on the usage of timers in these functions:
 * The timers are composed of 3 elements, a pointer to a callback function,
 * the expire time of the timer, and a unique (pseudo-monotomically increasing)
 * identifier for the timer.
 * Timers are kept in a pset_h (linked list) and are sorted at insertion
 * time, with the nearest expire time first.  The sort at insertion is to
 * avoid examining non-expired timers in the xtimer_poll() function, and
 * to be able to easily retrieve the next expiring timer.  
 * The timers are set in the main event loop, using select() as the
 * timing device.  select() sleeps until the next timer is set to expire
 * (or until an FD is ready, in which case it also examines the timer list).
 */

static pset_h xtimer_list = NULL;

static int xtimer_init( void )
{
	if( xtimer_list != NULL )
		return 0;

	xtimer_list = pset_create( 0, 0 );
	if( xtimer_list == NULL ) 
		return -1;

	return 0;
}

static int xtimer_compfunc( const void *_a, const void *_b )
{
	xtime_h **a = (xtime_h **)_a;
	xtime_h **b = (xtime_h **)_b;
	return ((*a)->when - (*b)->when);
}

/* xtimer_add:
 * Adds a timer to the pset_h of timers, and sorts the timer list (least time
 * remaining first).
 * Return values:
 * Success: the timer ID which can be used to later remove the timer (>0)
 * Failure: -1
 */
int xtimer_add( void (*func)(void), time_t secs )
{
	xtime_h *new_xtimer = NULL;
	time_t tmptime;
	unsigned count;

	if( xtimer_list == NULL ) {
		if( xtimer_init() < 0 )
			return -1;
	}

	new_xtimer = (xtime_h *)malloc(sizeof(xtime_h));
	if( new_xtimer == NULL ) {
		return -1;
	}

	tmptime = time(NULL);
	if( tmptime == -1 ) {
		free( new_xtimer );
		return -1;
	}
	
	new_xtimer->timerfunc = func;
	new_xtimer->when =  tmptime + secs;

	if( (count = pset_count( xtimer_list )) == 0 ) {
	   new_xtimer->xtid = 1;
	} else {
	   new_xtimer->xtid = ((xtime_h *)(pset_pointer(xtimer_list, count-1)))->xtid + 1;
	}

	if( pset_add( xtimer_list, new_xtimer ) == NULL ) {
		free( new_xtimer );
		return -1;
	}
		
	pset_sort( xtimer_list, xtimer_compfunc );
	
	return(new_xtimer->xtid);
}

/* xtimer_poll:
 * Traverses the pset_h of timers, and executes the callback for expired
 * timers.  Timers that are expired, and have their callback executed
 * are removed from the list of timers. 
 */
int xtimer_poll(void)
{
	int i;
	
	if( xtimer_list == NULL )
		return(0);

	for( i = 0; i < pset_count( xtimer_list ); i++ ) {
		xtime_h *cur_timer = pset_pointer( xtimer_list, i );
		time_t cur_time    = time(NULL);

		/* The list is sorted, low to high.  If there's no
		 * timers left, return.
		 */
		if( cur_timer->when > cur_time ) {
			return(0);
		}
		cur_timer->timerfunc();
		pset_delete( xtimer_list, cur_timer );
		free(cur_timer);
		cur_timer = NULL;
	}

	return(0);
}

/* xtimer_remove:
 * Removes a timer from the list of timers.  Takes the timer id as an argument
 * Returns:
 * Success: 0
 * Failure: -1
 */
int xtimer_remove(int xtid)
{
	int i;
	int ret = -1;

	for( i = 0; i < pset_count( xtimer_list ); i++ ) {
		xtime_h *cur_timer = pset_pointer( xtimer_list, i );
		if( cur_timer->xtid == xtid ) {
			pset_delete( xtimer_list, cur_timer );
			free( cur_timer );
			cur_timer = NULL;
			ret = 0;
		}
	}

	return(ret);
}

/* xtimer_nexttime:
 * Returns the number of seconds until the next timer expires.
 * Returns -1 when no timers are active.
 */
time_t xtimer_nexttime(void)
{
	time_t ret;

	if(xtimer_list == NULL)
		return -1;

	if( pset_count(xtimer_list) == 0 )
		return -1;

	ret = ((xtime_h *)pset_pointer(xtimer_list, 0))->when - time(NULL) ;
	if( ret < 0 )
		ret = 0;
	return( ret );
}

