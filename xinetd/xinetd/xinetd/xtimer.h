/*
 * (c) Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef _X_TIMER_H
#define _X_TIMER_H

#include <sys/time.h>
#include <unistd.h>

struct xtime {
	void     (*timerfunc)(void);
	time_t   when;
	int xtid;
};
typedef struct xtime xtime_h;

int xtimer_add( void (*func)(void), time_t );
int xtimer_poll(void);
int xtimer_remove(int);
time_t xtimer_nexttime(void);

#endif /* _X_TIMER_H */
