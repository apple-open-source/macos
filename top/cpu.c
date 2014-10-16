/*
 * Copyright (c) 2002-2004, 2008 Apple Computer, Inc.  All rights reserved.
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

#include <stdlib.h>
#include <sys/time.h>
#include <mach/mach_time.h>
#include "libtop.h"
#include "cpu.h"
#include "generic.h"
#include "preferences.h"
#include "log.h"
#include "uinteger.h"

extern const libtop_tsamp_t *tsamp;
extern mach_timebase_info_data_t timebase_info;

static bool cpu_insert_cell(struct statistic *s, const void *sample) {
	const libtop_psamp_t *psamp = sample;
	struct timeval elapsed, used;
	char buf[10];
	unsigned long long elapsed_us = 0, used_us = 0;
	int whole = 0, part = 0;

	if(0 == psamp->p_seq) {
		whole = 0;
		part = 0;

		if(-1 == snprintf(buf, sizeof(buf), "%d.%1d", whole, part))
			return true;

		return generic_insert_cell(s, buf);
	}


	switch(top_prefs_get_mode()) {
		case STATMODE_ACCUM:
			timersub(&tsamp->time, &tsamp->b_time, &elapsed);
			timersub(&psamp->total_time, &psamp->b_total_time, &used);
			break;


		case STATMODE_EVENT:
		case STATMODE_DELTA:
		case STATMODE_NON_EVENT:
			timersub(&tsamp->time, &tsamp->p_time, &elapsed);
			timersub(&psamp->total_time, &psamp->p_total_time, &used);
			break;

		default:
			fprintf(stderr, "unhandled STATMOMDE in %s\n", __func__);
			abort();
	}

	elapsed_us = (unsigned long long)elapsed.tv_sec * 1000000ULL
		+ (unsigned long long)elapsed.tv_usec;

	used_us = (unsigned long long)used.tv_sec * 1000000ULL
		+ (unsigned long long)used.tv_usec;

	/* Avoid a divide by 0 exception. */
	if(elapsed_us > 0) {
		whole = (used_us * 100ULL) / elapsed_us;
		part = (((used_us * 100ULL) - (whole * elapsed_us)) * 10ULL) / elapsed_us;
	}

	//top_log("command %s whole %d part %d\n", psamp->command, whole, part);

	if(-1 == snprintf(buf, sizeof(buf), "%d.%1d", whole, part))
		return true;

	return generic_insert_cell(s, buf);
}

static struct statistic_callbacks callbacks = {
	.draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = generic_get_minimum_size,
	.insert_cell = cpu_insert_cell,
	.reset_insertion = generic_reset_insertion
};

struct statistic *top_cpu_create(WINDOW *parent, const char *name) {
	return create_statistic(STATISTIC_CPU, parent, NULL, &callbacks, name);
}


static bool cpu_me_insert_cell(struct statistic *s, const void *sample) {

	const libtop_psamp_t *psamp = sample;
	struct timeval elapsed;
	char buf[10];
	unsigned long long elapsed_ns = 0, used_ns = 0;

	if(0 == psamp->p_seq) {

		if(-1 == snprintf(buf, sizeof(buf), "%7.5f", 0))
			return true;

		return generic_insert_cell(s, buf);
	}

	switch(top_prefs_get_mode()) {
		case STATMODE_ACCUM:
			timersub(&tsamp->time, &tsamp->b_time, &elapsed);
			used_ns = psamp->cpu_billed_to_me - psamp->b_cpu_billed_to_me;
			break;


		case STATMODE_EVENT:
		case STATMODE_DELTA:
		case STATMODE_NON_EVENT:
			timersub(&tsamp->time, &tsamp->p_time, &elapsed);
			used_ns = psamp->cpu_billed_to_me - psamp->p_cpu_billed_to_me;
			break;

		default:
			fprintf(stderr, "unhandled STATMOMDE in %s\n", __func__);
			abort();
	}

	used_ns = used_ns * timebase_info.numer / timebase_info.denom;

	elapsed_ns = (unsigned long long)elapsed.tv_sec * 1000000000ULL
		+ (unsigned long long)elapsed.tv_usec* 1000ULL;

	//top_log("command %s whole %d part %d\n", psamp->command, whole, part);

	if(-1 == snprintf(buf, sizeof(buf), "%7.5f", (((float)used_ns * 100) / (float)elapsed_ns)))
		return true;

	return generic_insert_cell(s, buf);
}

static void cpu_me_get_minimum_size(struct statistic *s) {
	s->minimum_size.width = 7;
	s->minimum_size.height = 1;
}

static struct statistic_callbacks callbacks1 = {
	.draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = cpu_me_get_minimum_size,
	.insert_cell = cpu_me_insert_cell,
	.reset_insertion = generic_reset_insertion
};

struct statistic *top_cpu_me_create(WINDOW *parent, const char *name) {
	return create_statistic(STATISTIC_CPU_ME, parent, NULL, &callbacks1, name);
}

static bool cpu_others_insert_cell(struct statistic *s, const void *sample) {

	const libtop_psamp_t *psamp = sample;
	struct timeval elapsed;
	char buf[10];
	unsigned long long elapsed_ns = 0, used_ns = 0;

	if(0 == psamp->p_seq) {

		if(-1 == snprintf(buf, sizeof(buf), "%7.5f", 0))
			return true;

		return generic_insert_cell(s, buf);
	}


	switch(top_prefs_get_mode()) {
		case STATMODE_ACCUM:
			timersub(&tsamp->time, &tsamp->b_time, &elapsed);
			used_ns = psamp->cpu_billed_to_others - psamp->b_cpu_billed_to_others;
			break;


		case STATMODE_EVENT:
		case STATMODE_DELTA:
		case STATMODE_NON_EVENT:
			timersub(&tsamp->time, &tsamp->p_time, &elapsed);
			used_ns = psamp->cpu_billed_to_others - psamp->p_cpu_billed_to_others;
			break;

		default:
			fprintf(stderr, "unhandled STATMOMDE in %s\n", __func__);
			abort();
	}

	used_ns = used_ns * timebase_info.numer / timebase_info.denom;

	elapsed_ns = (unsigned long long)elapsed.tv_sec * 1000000000ULL
		+ (unsigned long long)elapsed.tv_usec * 1000ULL;

	//top_log("command %s whole %d part %d\n", psamp->command, whole, part);

	if(-1 == snprintf(buf, sizeof(buf), "%7.5f", (((float)used_ns * 100) / (float)elapsed_ns)))
		return true;

	return generic_insert_cell(s, buf);
}

static void cpu_others_get_minimum_size(struct statistic *s) {
	s->minimum_size.width =10 ;
	s->minimum_size.height = 1;
}

static struct statistic_callbacks callbacks2 = {
	.draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = cpu_others_get_minimum_size,
	.insert_cell = cpu_others_insert_cell,
	.reset_insertion = generic_reset_insertion
};

struct statistic *top_cpu_others_create(WINDOW *parent, const char *name) {
	return create_statistic(STATISTIC_CPU_OTHERS, parent, NULL, &callbacks2, name);
}

static bool boosts_insert_cell(struct statistic *s, const void *sample) {

	const libtop_psamp_t *psamp = sample;
	char buf1[GENERIC_INT_SIZE];
	char buf2[GENERIC_INT_SIZE];
	char buf[GENERIC_INT_SIZE * 2 + 3];

	if (top_uinteger_format_result(buf1, sizeof(buf1),
				psamp->assertcnt, psamp->p_assertcnt,
				psamp->b_assertcnt)) {
		return true;
	}
	if (top_uinteger_format_result(buf2, sizeof(buf2),
				psamp->boosts, psamp->p_boosts,
				psamp->b_boosts)) {
		return true;
	}
	sprintf(buf, "%c%s[%s]", ((psamp->boost_donating) ? '*' : ' '), buf1, buf2);

	return generic_insert_cell(s, buf);
}

static void boosts_get_minimum_size(struct statistic *s) {
	generic_get_minimum_size(s);
	s->minimum_size.width += 4;
}

static struct statistic_callbacks callbacks3 = {
	.draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = boosts_get_minimum_size,
	.insert_cell = boosts_insert_cell,
	.reset_insertion = generic_reset_insertion
};

struct statistic *top_boosts_create(WINDOW *parent, const char *name) {
	return create_statistic(STATISTIC_BOOSTS, parent, NULL, &callbacks3, name);
}
