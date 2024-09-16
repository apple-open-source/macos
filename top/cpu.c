/*
 * Copyright (c) 2002-2004, 2008, 2019 Apple Computer, Inc.  All rights reserved.
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

#include "cpu.h"
#include "generic.h"
#include "libtop.h"
#include "log.h"
#include "preferences.h"
#include "uinteger.h"
#include <mach/mach_time.h>
#include <stdlib.h>
#include <sys/time.h>

extern const libtop_tsamp_t *tsamp;
extern mach_timebase_info_data_t timebase_info;

static bool
cpu_insert_cell(struct statistic *s, const void *sample)
{
	const libtop_psamp_t *psamp = sample;
	char buf[10];
	unsigned long long elapsed_us = 0, used_us = 0;
	int whole = 0, part = 0;

	if (0 == psamp->p_seq) {
		whole = 0;
		part = 0;

		if (-1 == snprintf(buf, sizeof(buf), "%d.%1d", whole, part))
			return true;

		return generic_insert_cell(s, buf);
	}

	uint64_t last_timens = 0;
	uint64_t last_total_timens = 0;
	switch (top_prefs_get_mode()) {
	case STATMODE_ACCUM:
		last_timens = tsamp->b_timens;
		last_total_timens = psamp->b_total_timens;
		break;

	case STATMODE_EVENT:
	case STATMODE_DELTA:
	case STATMODE_NON_EVENT:
		last_timens = tsamp->p_timens;
		last_total_timens = psamp->p_total_timens;
		break;

	default:
		fprintf(stderr, "unhandled STATMODE in %s\n", __func__);
		abort();
	}

	elapsed_us = (tsamp->timens - last_timens) / NSEC_PER_USEC;
	used_us = (psamp->total_timens - last_total_timens) / NSEC_PER_USEC;

	/* Avoid a divide by 0 exception. */
	if (elapsed_us > 0) {
		whole = (used_us * 100ULL) / elapsed_us;
		part = (((used_us * 100ULL) - (whole * elapsed_us)) * 10ULL) / elapsed_us;
	}

	// top_log("command %s whole %d part %d\n", psamp->command, whole, part);

	if (-1 == snprintf(buf, sizeof(buf), "%d.%1d", whole, part))
		return true;

	return generic_insert_cell(s, buf);
}

static struct statistic_callbacks callbacks = { .draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = generic_get_minimum_size,
	.insert_cell = cpu_insert_cell,
	.reset_insertion = generic_reset_insertion };

struct statistic *
top_cpu_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_CPU, parent, NULL, &callbacks, name);
}

static bool
cpu_me_insert_cell(struct statistic *s, const void *sample)
{

	const libtop_psamp_t *psamp = sample;
	char buf[10];
	unsigned long long elapsed_ns = 0, used_ns = 0;

	if (0 == psamp->p_seq) {

		if (-1 == snprintf(buf, sizeof(buf), "%7.5f", 0.0))
			return true;

		return generic_insert_cell(s, buf);
	}

	switch (top_prefs_get_mode()) {
	case STATMODE_ACCUM:
		elapsed_ns = tsamp->timens - tsamp->b_timens;
		used_ns = psamp->cpu_billed_to_me - psamp->b_cpu_billed_to_me;
		break;

	case STATMODE_EVENT:
	case STATMODE_DELTA:
	case STATMODE_NON_EVENT:
		elapsed_ns = tsamp->timens - tsamp->p_timens;
		used_ns = psamp->cpu_billed_to_me - psamp->p_cpu_billed_to_me;
		break;

	default:
		fprintf(stderr, "unhandled STATMODE in %s\n", __func__);
		abort();
	}

	used_ns = used_ns * timebase_info.numer / timebase_info.denom;

	// top_log("command %s whole %d part %d\n", psamp->command, whole, part);

	if (-1 == snprintf(buf, sizeof(buf), "%7.5f", (((float)used_ns * 100) / (float)elapsed_ns)))
		return true;

	return generic_insert_cell(s, buf);
}

static void
cpu_me_get_minimum_size(struct statistic *s)
{
	s->minimum_size.width = 7;
	s->minimum_size.height = 1;
}

static struct statistic_callbacks callbacks1 = { .draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = cpu_me_get_minimum_size,
	.insert_cell = cpu_me_insert_cell,
	.reset_insertion = generic_reset_insertion };

struct statistic *
top_cpu_me_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_CPU_ME, parent, NULL, &callbacks1, name);
}

static bool
cpu_others_insert_cell(struct statistic *s, const void *sample)
{

	const libtop_psamp_t *psamp = sample;
	char buf[10];
	unsigned long long elapsed_ns = 0, used_ns = 0;

	if (0 == psamp->p_seq) {

		if (-1 == snprintf(buf, sizeof(buf), "%7.5f", 0.0))
			return true;

		return generic_insert_cell(s, buf);
	}

	switch (top_prefs_get_mode()) {
	case STATMODE_ACCUM:
		elapsed_ns = tsamp->timens - tsamp->b_timens;
		used_ns = psamp->cpu_billed_to_others - psamp->b_cpu_billed_to_others;
		break;

	case STATMODE_EVENT:
	case STATMODE_DELTA:
	case STATMODE_NON_EVENT:
		elapsed_ns = tsamp->timens - tsamp->p_timens;
		used_ns = psamp->cpu_billed_to_others - psamp->p_cpu_billed_to_others;
		break;

	default:
		fprintf(stderr, "unhandled STATMODE in %s\n", __func__);
		abort();
	}

	used_ns = used_ns * timebase_info.numer / timebase_info.denom;

	// top_log("command %s whole %d part %d\n", psamp->command, whole, part);

	if (-1 == snprintf(buf, sizeof(buf), "%7.5f", (((float)used_ns * 100) / (float)elapsed_ns)))
		return true;

	return generic_insert_cell(s, buf);
}

static void
cpu_others_get_minimum_size(struct statistic *s)
{
	s->minimum_size.width = 10;
	s->minimum_size.height = 1;
}

static struct statistic_callbacks callbacks2 = { .draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = cpu_others_get_minimum_size,
	.insert_cell = cpu_others_insert_cell,
	.reset_insertion = generic_reset_insertion };

struct statistic *
top_cpu_others_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_CPU_OTHERS, parent, NULL, &callbacks2, name);
}

static bool
boosts_insert_cell(struct statistic *s, const void *sample)
{

	const libtop_psamp_t *psamp = sample;
	char buf1[GENERIC_INT_SIZE];
	char buf2[GENERIC_INT_SIZE];
	char buf[GENERIC_INT_SIZE * 2 + 3];

	if (top_uinteger_format_result(
				buf1, sizeof(buf1), psamp->assertcnt, psamp->p_assertcnt, psamp->b_assertcnt)) {
		return true;
	}
	if (top_uinteger_format_result(
				buf2, sizeof(buf2), psamp->boosts, psamp->p_boosts, psamp->b_boosts)) {
		return true;
	}
	sprintf(buf, "%c%s[%s]", ((psamp->boost_donating) ? '*' : ' '), buf1, buf2);

	return generic_insert_cell(s, buf);
}

static void
boosts_get_minimum_size(struct statistic *s)
{
	generic_get_minimum_size(s);
	s->minimum_size.width += 4;
}

static struct statistic_callbacks callbacks3 = { .draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = boosts_get_minimum_size,
	.insert_cell = boosts_insert_cell,
	.reset_insertion = generic_reset_insertion };

struct statistic *
top_boosts_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_BOOSTS, parent, NULL, &callbacks3, name);
}

static bool
instrs_insert_cell(struct statistic *s, const void *sample)
{
	const libtop_psamp_t *psamp = sample;
	char buf[GENERIC_INT_SIZE];
	uint64_t value;

	if (psamp->p_seq == 0) {
		if (snprintf(buf, sizeof(buf), "%" PRIu64, (uint64_t)0) == -1) {
			return true;
		}

		return generic_insert_cell(s, buf);
	}

	switch (top_prefs_get_mode()) {
	case STATMODE_ACCUM:
		value = psamp->instructions - psamp->b_instructions;
		break;

	case STATMODE_EVENT: /* FALLTHROUGH */
	case STATMODE_DELTA: /* FALLTHROUGH */
	case STATMODE_NON_EVENT:
		value = psamp->instructions - psamp->p_instructions;
		break;

	default:
		fprintf(stderr, "unhandled STATMODE in %s\n", __func__);
		abort();
	}

	if (snprintf(buf, sizeof(buf), "%" PRIu64, value) == -1) {
		return true;
	}

	return generic_insert_cell(s, buf);
}

static struct statistic_callbacks instrs_callbacks = {
	.draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = generic_get_minimum_size,
	.insert_cell = instrs_insert_cell,
	.reset_insertion = generic_reset_insertion,
};

struct statistic *
top_instrs_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_INSTRS, parent, NULL, &instrs_callbacks, name);
}

static bool
cycles_insert_cell(struct statistic *s, const void *sample)
{
	const libtop_psamp_t *psamp = sample;
	char buf[GENERIC_INT_SIZE];
	uint64_t value;

	if (psamp->p_seq == 0) {
		buf[0] = '0';
		buf[1] = '\0';
		return generic_insert_cell(s, buf);
	}

	switch (top_prefs_get_mode()) {
	case STATMODE_ACCUM:
		value = psamp->cycles - psamp->b_cycles;
		break;

	case STATMODE_EVENT: /* FALLTHROUGH */
	case STATMODE_DELTA: /* FALLTHROUGH */
	case STATMODE_NON_EVENT:
		value = psamp->cycles - psamp->p_cycles;
		break;

	default:
		fprintf(stderr, "unhandled STATMODE in %s\n", __func__);
		abort();
	}

	if (snprintf(buf, sizeof(buf), "%" PRIu64, value) == -1) {
		return true;
	}

	top_log("command %s whole %llu\n", psamp->command, value);

	return generic_insert_cell(s, buf);
}

static struct statistic_callbacks cycles_callbacks = {
	.draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = generic_get_minimum_size,
	.insert_cell = cycles_insert_cell,
	.reset_insertion = generic_reset_insertion,
};

struct statistic *
top_cycles_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_CYCLES, parent, NULL, &cycles_callbacks, name);
}
