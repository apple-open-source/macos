/*
 * Copyright (c) 2008, 2019 Apple Computer, Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include "top.h"
#include "globalstats.h"
#include "layout.h"
#include "libtop.h"
#include "log.h"
#include "preferences.h"
#include "uinteger.h"
#include "userinput.h"
#include <curses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const libtop_tsamp_t *tsamp;

static int sort_subcomp(int a_key, const libtop_psamp_t *a_a, const libtop_psamp_t *a_b);

static bool top_do_relayout = false;

/* This may not be called in common update patterns. */
void
top_relayout_force(void)
{
	top_do_relayout = true;
}

void
top_relayout(struct statistics_controller *c, int type, int maxwidth)
{
	top_do_relayout = true;
}

bool
top_need_relayout(void)
{
	return top_do_relayout;
}

void *
top_create(WINDOW *wmain)
{
	void *gstats;
	struct statistics_controller *controller;

	gstats = top_globalstats_create(wmain);

	if (NULL == gstats) {
		endwin();
		fprintf(stderr, "unable to create global stats!\n");
		_exit(EXIT_FAILURE);
	}

	controller = create_statistics_controller(wmain);

	if (NULL == controller) {
		endwin();
		fprintf(stderr, "unable to create controller for main window!\n");
		_exit(EXIT_FAILURE);
	}

	controller->globalstats = gstats;

	return controller;
}

static int
top_sort(void *a_data, const libtop_psamp_t *a, const libtop_psamp_t *b)
{
	int retval;

	retval = sort_subcomp(top_prefs_get_sort(), a, b) * (top_prefs_get_ascending() ? 1 : -1);
	if (retval == 0) {
		retval = sort_subcomp(top_prefs_get_secondary_sort(), a, b)
				* (top_prefs_get_secondary_ascending() ? 1 : -1);
	}

	return retval;
}

#define COMP(a, b) (((a) == (b) ? 0 : ((a) < (b) ? -1 : 1)))

static int
sort_subcomp(int a_key, const libtop_psamp_t *a_a, const libtop_psamp_t *a_b)
{
	unsigned long long used_ns_a, used_ns_b;
	const char *user_a, *user_b;

	switch (a_key) {
	case STATISTIC_PID:
		return COMP(a_a->pid, a_b->pid);

	case STATISTIC_COMMAND:
		return COMP(strcmp(a_a->command, a_b->command), 0);

	case STATISTIC_CPU:;
		uint64_t time_ns_a = a_a->total_timens - a_a->p_total_timens;
		uint64_t time_ns_b = a_b->total_timens - a_b->p_total_timens;
		return COMP(time_ns_a, time_ns_b);

	case STATISTIC_CPU_ME:
		used_ns_a = a_a->cpu_billed_to_me - a_a->p_cpu_billed_to_me;
		used_ns_b = a_b->cpu_billed_to_me - a_b->p_cpu_billed_to_me;
		return COMP(used_ns_a, used_ns_b);

	case STATISTIC_CPU_OTHERS:
		used_ns_a = a_a->cpu_billed_to_others - a_a->p_cpu_billed_to_others;
		used_ns_b = a_b->cpu_billed_to_others - a_b->p_cpu_billed_to_others;
		return COMP(used_ns_a, used_ns_b);

	case STATISTIC_BOOSTS: {
		int res;
		res = COMP(a_a->boost_last_donating_seq, a_b->boost_last_donating_seq);
		if (res)
			return res;
		res = COMP(!a_a->boost_donating, !a_b->boost_donating);
		if (res)
			return res;
		return COMP(a_a->boosts - a_a->p_boosts, a_b->boosts - a_b->p_boosts);
	}

	case STATISTIC_TIME:
		return COMP(a_a->total_timens, a_b->total_timens);

	case STATISTIC_THREADS:
		return COMP(a_a->th, a_b->th);

	case STATISTIC_WORKQUEUE:
		return COMP(a_a->wq_nthreads, a_b->wq_nthreads);

	case STATISTIC_PORTS:
		return COMP(a_a->prt, a_b->prt);

	case STATISTIC_MREGION:
		return COMP(a_a->reg, a_b->reg);

#ifdef TOP_ANONYMOUS_MEMORY
	case STATISTIC_PMEM:
		return COMP(a_a->pfootprint, a_b->pfootprint);

	case STATISTIC_RPRVT:
		return COMP(a_a->rprvt, a_b->rprvt);

	case STATISTIC_PURG:
		return COMP(a_a->purgeable, a_b->purgeable);

	case STATISTIC_COMPRESSED:
		return COMP(a_a->compressed, a_b->compressed);
#else
	case STATISTIC_RPRVT:
		return COMP(a_a->rprvt, a_b->rprvt);

	case STATISTIC_RSHRD:
		return COMP(a_a->rshrd, a_b->rshrd);

	case STATISTIC_RSIZE:
		return COMP(a_a->rsize, a_b->rsize);
#endif

	case STATISTIC_VSIZE:
		return COMP(a_a->vsize, a_b->vsize);

	case STATISTIC_VPRVT:
		return COMP(a_a->vprvt, a_b->vprvt);

	case STATISTIC_PGRP:
		return COMP(a_a->pgrp, a_b->pgrp);

	case STATISTIC_PPID:
		return COMP(a_a->ppid, a_b->ppid);

	case STATISTIC_KPRVT:
		return COMP(a_a->palloc - a_a->pfree, a_b->palloc - a_b->pfree);

	case STATISTIC_KSHRD:
		return COMP(a_a->salloc - a_a->sfree, a_b->salloc - a_b->sfree);

	case STATISTIC_JETSAM_PRI:
		return COMP(a_a->jetsam_priority, a_b->jetsam_priority);

	case STATISTIC_PSTATE: {
		const char *a = libtop_state_str(a_a->state);
		const char *b = libtop_state_str(a_b->state);

		return COMP(strcmp(a, b), 0);
	}
	case STATISTIC_UID:
		return COMP(a_a->uid, a_b->uid);

	case STATISTIC_FAULTS:
		return COMP(a_a->faults.now, a_b->faults.now);

	case STATISTIC_COW_FAULTS:
		return COMP(a_a->cow_faults.now, a_b->cow_faults.now);

	case STATISTIC_MESSAGES_SENT:
		return COMP(a_a->messages_sent.now, a_b->messages_sent.now);

	case STATISTIC_MESSAGES_RECEIVED:
		return COMP(a_a->messages_recv.now, a_b->messages_recv.now);

	case STATISTIC_SYSBSD:
		return COMP(a_a->syscalls_bsd.now, a_b->syscalls_bsd.now);

	case STATISTIC_SYSMACH:
		return COMP(a_a->syscalls_mach.now, a_b->syscalls_mach.now);

	case STATISTIC_CSW:
		return COMP(a_a->csw.now, a_b->csw.now);

	case STATISTIC_PAGEINS:
		return COMP(a_a->pageins.now, a_b->pageins.now);

	case STATISTIC_IDLEWAKE:
		return COMP(a_a->power.task_platform_idle_wakeups, a_b->power.task_platform_idle_wakeups);

	case STATISTIC_POWERSCORE: {
		uint64_t a_idlew
				= a_a->power.task_platform_idle_wakeups - a_a->p_power.task_platform_idle_wakeups;
		uint64_t b_idlew
				= a_b->power.task_platform_idle_wakeups - a_b->p_power.task_platform_idle_wakeups;
		uint64_t timens_a = a_a->total_timens - a_a->p_total_timens;
		uint64_t timens_b = a_b->total_timens - a_b->p_total_timens;

		uint64_t cmp_a = timens_a + (UINT64_C(500) * a_idlew);
		uint64_t cmp_b = timens_b + (UINT64_C(500) * b_idlew);

		// kernel gets a free ride
		if (a_a->pid == 0) {
			cmp_a = UINT64_C(0);
		} else if (a_b->pid == 0) {
			cmp_b = UINT64_C(0);
		}
		return COMP(cmp_a, cmp_b);
	}

	case STATISTIC_INSTRS:
		return COMP(
				a_a->instructions - a_a->p_instructions, a_b->instructions - a_b->p_instructions);

	case STATISTIC_CYCLES:
		return COMP(a_a->cycles - a_a->p_cycles, a_b->cycles - a_b->p_cycles);

	case STATISTIC_USER:
		/* Handle the == case first, since it's common. */
		if (a_a->uid == a_b->uid)
			return 0;

		user_a = libtop_username(a_a->uid);
		user_b = libtop_username(a_b->uid);

		return COMP(strcmp(user_a, user_b), 0);
	}

	return 0;
}

#undef COMP

void
top_sample(void)
{
	if (libtop_sample(/*calculate mreg, vprvt and more see libtop.h*/
				top_prefs_get_mmr(),
				/*framework stats*/
				top_prefs_get_frameworks())) {
		endwin();
		fprintf(stderr,
				"error: while gathering a libtop sample.\n"
				"The permissions and/or ownership are incorrect "
				"for this executable, or you are testing without sudo.\n");
		_exit(EXIT_FAILURE);
	}
}

void
top_insert(void *ptr)
{
	struct statistics_controller *c = ptr;
	const libtop_psamp_t *psample;
	char *user;
	unsigned long uid = 0;
	int nprocs;

	c->reset_insertion(c);

	top_sample();

	/*
	 * The ordering is important here, because the libtop_psort actually
	 * updates the tsamp->nprocs.
	 */
	libtop_psort(top_sort, NULL);

	tsamp = libtop_tsamp();

	if (top_globalstats_update(c->globalstats, tsamp))
		top_log("An error occurred while updating global stats.\n");

	/* The user has requested only displaying processes owned by user. */
	user = top_prefs_get_user();

	if (user)
		uid = top_prefs_get_user_uid();

	nprocs = top_prefs_get_nprocs();

	if (0 == nprocs)
		return;

	for (psample = libtop_piterate(); psample; psample = libtop_piterate()) {
		if (user && psample->uid != uid)
			continue;

		if (!top_prefs_want_pid(psample->pid)) {
			continue;
		}

		c->insert_sample(c, psample);

		/*
		 * If nprocs is -1 (the default), or otherwise negative,
		 * then display all.
		 */
		if (nprocs > 0)
			--nprocs;

		if (0 == nprocs) {
			break;
		}
	}
	first_sample = false;
}

/* Return true if a non-fatal error occurred. */
bool
top_layout(void *ptr)
{
	struct statistics_controller *c = ptr;
	int lines = LINES, cols = COLS;
	int consumed_height = 0;
	int pstatheight;

	// top_log("%s\n", __func__);

	top_do_relayout = false;

	// fprintf(stderr, "laying out: lines %d cols %d\n", lines, cols);
	// werase(c->parent);

	if (ERR == wresize(c->parent, lines, cols)) {
		top_log("error: wresizing parent!\n");
		return true;
	}

	top_globalstats_reset(c->globalstats);

	if (top_globalstats_resize(c->globalstats, cols, lines, &consumed_height)) {
		top_log("error: performing global stats resize!\n");
		return true;
	}

	user_input_set_position(consumed_height, 0);

	pstatheight = lines - consumed_height - 1;
	if (pstatheight <= 0)
		return true;

	// fprintf(stderr, "consumed_height %d\n", consumed_height);

	if (layout_statistics(c, cols, pstatheight,
				/*y*/ consumed_height + 1)) {
		top_log("error: performing statistic layout!\n");
		return true;
	}

	return false;
}

struct draw_state {
	int xoffset;
};

static bool
top_draw_iterator(struct statistic *s, void *ptr)
{
	struct draw_state *state = ptr;

	s->callbacks.draw(s, /*x*/ state->xoffset);

	state->xoffset = 1;

	/* more iterations */
	return true;
}

void
top_draw(void *ptr)
{
	struct statistics_controller *c = ptr;
	struct draw_state state;

	werase(c->parent);

	top_globalstats_draw(c->globalstats);

	user_input_draw(ptr, stdscr);

	state.xoffset = 0;

	c->iterate(c, top_draw_iterator, &state);

	/* This moves the insertion cursor to the lower right. */
	wmove(stdscr, LINES - 1, COLS - 1);

	update_panels();
	doupdate();
}
