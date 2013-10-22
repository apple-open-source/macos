/*	$NetBSD: schedule.c,v 1.4 2006/09/09 16:22:10 manu Exp $	*/

/*	$KAME: schedule.c,v 1.19 2001/11/05 10:53:19 sakane Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/socket.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <dispatch/dispatch.h>

#include "misc.h"
#include "plog.h"
#include "schedule.h"
#include "var.h"
#include "gcmalloc.h"
#include "power_mgmt.h"
#include "localconf.h"

#if !defined(__LP64__)
// year 2038 problem and fix for 32-bit only
#define FIXY2038PROBLEM
#endif


extern int		terminated;

#ifdef FIXY2038PROBLEM
#define Y2038TIME_T	0x7fffffff
static time_t launched;		/* time when the program launched. */
static time_t deltaY2038;
#endif

static TAILQ_HEAD(_schedtree, sched) sctree;


void
timer_handler(struct sched *sched)
{
	if (slept_at || woke_at)
        sched->dead = 1;
   
    TAILQ_REMOVE(&sctree, sched, chain);
    
    if (!sched->dead) {
        if (sched->func != NULL && !terminated) {
			(sched->func)(sched->param);
        }
    }
    racoon_free(sched);
}

/*
 * add new schedule to schedule table.
 */
schedule_ref
sched_new(time_t tick, void (*func) (void *), void *param)
{
    static schedule_ref next_ref = 1;
	struct sched *new_sched;
    
    if (next_ref == 0)
        next_ref++;
    
	new_sched = (struct sched *)racoon_malloc(sizeof(*new_sched));
	if (new_sched == NULL)
		return 0;
    
    new_sched->ref = next_ref++;
    new_sched->dead = 0;
	new_sched->func = func;
	new_sched->param = param;
    new_sched->xtime = current_time() + tick;
    
	/* add to schedule table */
	TAILQ_INSERT_TAIL(&sctree, new_sched, chain);    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, tick * NSEC_PER_SEC), dispatch_get_main_queue(),
                   ^{
                       timer_handler(new_sched);
                   });
    
	return new_sched->ref;
}

/* get current time.
 * if defined FIXY2038PROBLEM, base time is the time when called sched_init().
 * Otherwise, conform to time(3).
 */
time_t
current_time()
{
	time_t n;
#ifdef FIXY2038PROBLEM
	time_t t;
    
	time(&n);
	t = n - launched;
	if (t < 0)
		t += deltaY2038;
    
	return t;
#else
	return time(&n);
#endif
}

int
sched_is_dead(schedule_ref ref)
{
    struct sched *sc;
    
    if (ref == 0)
        return 1;
    TAILQ_FOREACH(sc, &sctree, chain) {
		if (sc->ref == ref) {
            if (sc->dead)
                return 1;
            return 0;
        }
	}
    return 1;
}


int
sched_get_time(schedule_ref ref, time_t *time)
{
    struct sched *sc;
    
    if (ref != 0) {
        TAILQ_FOREACH(sc, &sctree, chain) {
            if (sc->ref == ref) {
                if (sc->dead)
                    return 0;
                *time = sc->xtime;
                return 1;
            }
        }
    }
    return 0;
}

void
sched_kill(schedule_ref ref)
{
    struct sched *sc;
    
    if (ref != 0) {
        TAILQ_FOREACH(sc, &sctree, chain) {
            if (sc->ref == ref) {
                sc->dead = 1;
                return;
            }
        }
	}
}

void
sched_killall(void)
{
	struct sched *sc;
    
	TAILQ_FOREACH(sc, &sctree, chain)
        sc->dead = 1;
}


/* XXX this function is probably unnecessary. */
void
sched_scrub_param(void *param)
{
	struct sched *sc;
    
	TAILQ_FOREACH(sc, &sctree, chain) {
		if (sc->param == param) {
			sc->dead = 1;
        }
	}
}

/* initialize schedule table */
void
sched_init()
{
#ifdef FIXY2038PROBLEM
	time(&launched);
    
	deltaY2038 = Y2038TIME_T - launched;
#endif
    
	TAILQ_INIT(&sctree);
	return;
}

