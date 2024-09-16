/*
 * Copyright (c) 2008, 2009 Apple Inc.  All rights reserved.
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

#include "memstats.h"
#include "generic.h"
#include "libtop.h"
#include "preferences.h"
#include "uinteger.h"
#include <inttypes.h>
#include <libutil.h>
#include <stdlib.h>
#include <string.h>

#define NA(buf)                                                                                    \
	do {                                                                                           \
		memcpy(buf, "N/A", 4);                                                                     \
	} while (0)

#ifndef TOP_ANONYMOUS_MEMORY
/*rsize*/
static bool
rsize_insert_cell(struct statistic *s, const void *sample)
{
	const libtop_psamp_t *psamp = sample;
	char buf[7];

	if (top_prefs_get_mmr()) {
		if (top_uinteger_format_mem_result(buf, sizeof(buf), psamp->rsize, psamp->p_rsize,
					/*There is no b_rsize*/ 0ULL)) {
			return true;
		}
	} else {
		NA(buf);
	}

	return generic_insert_cell(s, buf);
}

static struct statistic_callbacks rsize_callbacks = { .draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = generic_get_minimum_size,
	.insert_cell = rsize_insert_cell,
	.reset_insertion = generic_reset_insertion };

struct statistic *
top_rsize_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_RSIZE, parent, NULL, &rsize_callbacks, name);
}
#endif /* !TOP_ANONYMOUS_MEMORY */

/*vsize*/
static bool
vsize_insert_cell(struct statistic *s, const void *sample)
{
	const libtop_psamp_t *psamp = sample;
	char buf[7];

	if (top_prefs_get_mmr()) {
		if (top_uinteger_format_mem_result(buf, sizeof(buf), psamp->vsize, psamp->p_vsize, 0ULL)) {
			return true;
		}
	} else {
		NA(buf);
	}

	return generic_insert_cell(s, buf);
}

static struct statistic_callbacks vsize_callbacks = { .draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = generic_get_minimum_size,
	.insert_cell = vsize_insert_cell,
	.reset_insertion = generic_reset_insertion };

struct statistic *
top_vsize_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_VSIZE, parent, NULL, &vsize_callbacks, name);
}

/*rprvt*/
static bool
rprvt_insert_cell(struct statistic *s, const void *sample)
{
	const libtop_psamp_t *psamp = sample;
	char buf[7];

	if (top_prefs_get_mmr()) {
		if (top_uinteger_format_mem_result(buf, sizeof(buf), psamp->rprvt, psamp->p_rprvt, 0ULL)) {
			return true;
		}
	} else {
		NA(buf);
	}

	return generic_insert_cell(s, buf);
}

static struct statistic_callbacks rprvt_callbacks = { .draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = generic_get_minimum_size,
	.insert_cell = rprvt_insert_cell,
	.reset_insertion = generic_reset_insertion };

struct statistic *
top_rprvt_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_RPRVT, parent, NULL, &rprvt_callbacks, name);
}

/*vprvt*/
static bool
vprvt_insert_cell(struct statistic *s, const void *sample)
{
	const libtop_psamp_t *psamp = sample;
	char buf[7];

	if (top_prefs_get_mmr()) {
		if (top_uinteger_format_mem_result(buf, sizeof(buf), psamp->vprvt, psamp->p_vprvt, 0ULL)) {
			return true;
		}
	} else {
		NA(buf);
	}

	return generic_insert_cell(s, buf);
}

static struct statistic_callbacks vprvt_callbacks = { .draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = generic_get_minimum_size,
	.insert_cell = vprvt_insert_cell,
	.reset_insertion = generic_reset_insertion };

struct statistic *
top_vprvt_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_VPRVT, parent, NULL, &vprvt_callbacks, name);
}

#ifndef TOP_ANONYMOUS_MEMORY
/*rshrd*/
static bool
rshrd_insert_cell(struct statistic *s, const void *sample)
{
	const libtop_psamp_t *psamp = sample;
	char buf[7];

	if (top_prefs_get_mmr()) {
		if (top_uinteger_format_mem_result(buf, sizeof(buf), psamp->rshrd, psamp->p_rshrd, 0ULL)) {
			return true;
		}
	} else {
		NA(buf);
	}

	return generic_insert_cell(s, buf);
}

static struct statistic_callbacks rshrd_callbacks = { .draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = generic_get_minimum_size,
	.insert_cell = rshrd_insert_cell,
	.reset_insertion = generic_reset_insertion };

struct statistic *
top_rshrd_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_RSHRD, parent, NULL, &rshrd_callbacks, name);
}
#endif /* !TOP_ANONYMOUS_MEMORY */

/*reg/mregions*/
static bool
mregion_insert_cell(struct statistic *s, const void *sample)
{
	const libtop_psamp_t *psamp = sample;
	char buf[GENERIC_INT_SIZE];

	if (top_prefs_get_mmr()) {
		if (top_uinteger_format_result(buf, sizeof(buf), psamp->reg, psamp->p_reg, 0ULL)) {
			return true;
		}
	} else {
		NA(buf);
	}

	return generic_insert_cell(s, buf);
}

static struct statistic_callbacks mregion_callbacks = { .draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = generic_get_minimum_size,
	.insert_cell = mregion_insert_cell,
	.reset_insertion = generic_reset_insertion };

struct statistic *
top_mregion_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_MREGION, parent, NULL, &mregion_callbacks, name);
}

static bool
pageins_insert_cell(struct statistic *s, const void *sample)
{
	const libtop_psamp_t *psamp = sample;
	char buf[GENERIC_INT_SIZE];

	if (top_uinteger_format_result(buf, sizeof(buf), psamp->pageins.now, psamp->pageins.previous,
				psamp->pageins.began)) {
		return true;
	}

	return generic_insert_cell(s, buf);
}

static struct statistic_callbacks pageins_callbacks = { .draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = generic_get_minimum_size,
	.insert_cell = pageins_insert_cell,
	.reset_insertion = generic_reset_insertion };

struct statistic *
top_pageins_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_PAGEINS, parent, NULL, &pageins_callbacks, name);
}

/*kprvt*/
static bool
kprvt_insert_cell(struct statistic *s, const void *sample)
{
	const libtop_psamp_t *psamp = sample;
	char buf[7];

	if (top_prefs_get_mmr()) {
		if (top_uinteger_format_mem_result(buf, sizeof(buf), psamp->palloc - psamp->pfree,
					psamp->p_palloc - psamp->p_pfree, 0ULL)) {
			return true;
		}
	} else {
		NA(buf);
	}

	return generic_insert_cell(s, buf);
}

static struct statistic_callbacks kprvt_callbacks = { .draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = generic_get_minimum_size,
	.insert_cell = kprvt_insert_cell,
	.reset_insertion = generic_reset_insertion };

struct statistic *
top_kprvt_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_KPRVT, parent, NULL, &kprvt_callbacks, name);
}

/*kshrd*/
static bool
kshrd_insert_cell(struct statistic *s, const void *sample)
{
	const libtop_psamp_t *psamp = sample;
	char buf[7];

	if (top_prefs_get_mmr()) {
		if (top_uinteger_format_mem_result(buf, sizeof(buf), psamp->salloc - psamp->sfree,
					psamp->p_salloc - psamp->p_sfree, 0ULL)) {
			return true;
		}
	} else {
		NA(buf);
	}

	return generic_insert_cell(s, buf);
}

static struct statistic_callbacks kshrd_callbacks = { .draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = generic_get_minimum_size,
	.insert_cell = kshrd_insert_cell,
	.reset_insertion = generic_reset_insertion };

struct statistic *
top_kshrd_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_KSHRD, parent, NULL, &kshrd_callbacks, name);
}

#ifdef TOP_ANONYMOUS_MEMORY
static bool
pmem_insert_cell(struct statistic *s, const void *sample)
{
	const libtop_psamp_t *psamp = sample;
	char buf[7];

	if (top_uinteger_format_mem_result(
				buf, sizeof(buf), psamp->pfootprint, psamp->p_pfootprint, 0ULL)) {
		return true;
	}

	return generic_insert_cell(s, buf);
}

static struct statistic_callbacks pmem_callbacks = {
	.draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = generic_get_minimum_size,
	.insert_cell = pmem_insert_cell,
	.reset_insertion = generic_reset_insertion,
};

struct statistic *
top_pmem_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_PMEM, parent, NULL, &pmem_callbacks, name);
}

static bool
purg_insert_cell(struct statistic *s, const void *sample)
{
	const libtop_psamp_t *psamp = sample;
	char buf[7];

	if (top_uinteger_format_mem_result(
				buf, sizeof(buf), psamp->purgeable, psamp->p_purgeable, 0ULL)) {
		return true;
	}

	return generic_insert_cell(s, buf);
}

static struct statistic_callbacks purg_callbacks = {
	.draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = generic_get_minimum_size,
	.insert_cell = purg_insert_cell,
	.reset_insertion = generic_reset_insertion,
};

struct statistic *
top_purg_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_PURG, parent, NULL, &purg_callbacks, name);
}

static bool
compressed_insert_cell(struct statistic *s, const void *sample)
{
	const libtop_psamp_t *psamp = sample;
	char buf[7];

	if (top_uinteger_format_mem_result(
				buf, sizeof(buf), psamp->compressed, psamp->p_compressed, 0ULL)) {
		return true;
	}

	return generic_insert_cell(s, buf);
}

static struct statistic_callbacks compressed_callbacks = {
	.draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = generic_get_minimum_size,
	.insert_cell = compressed_insert_cell,
	.reset_insertion = generic_reset_insertion,
};

struct statistic *
top_compressed_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_COMPRESSED, parent, NULL, &compressed_callbacks, name);
}
#endif /* TOP_ANONYMOUS_MEMORY */

static bool
jetsam_priority_insert_cell(struct statistic *s, const void *sample)
{
	const libtop_psamp_t *psamp = sample;
	char buf[7];

	if (top_uinteger_format_result(buf, sizeof(buf), psamp->jetsam_priority,
				psamp->p_jetsam_priority, psamp->b_jetsam_priority)) {
		return true;
	}

	return generic_insert_cell(s, buf);
}

static struct statistic_callbacks jetsam_priority_callbacks = {
	.draw = generic_draw,
	.resize_cells = generic_resize_cells,
	.move_cells = generic_move_cells,
	.get_request_size = generic_get_request_size,
	.get_minimum_size = generic_get_minimum_size,
	.insert_cell = jetsam_priority_insert_cell,
	.reset_insertion = generic_reset_insertion,
};

struct statistic *
top_jetsam_priority_create(WINDOW *parent, const char *name)
{
	return create_statistic(STATISTIC_JETSAM_PRI, parent, NULL, &jetsam_priority_callbacks, name);
}
