/*
 * Copyright (c) 2012, 2019 Apple Computer, Inc.  All rights reserved.
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

#include <mach/clock_types.h>
#include <stdlib.h>
#include "libtop.h"
#include "statistic.h"
#include "generic.h"
#include "preferences.h"
#include "uinteger.h"

extern const libtop_tsamp_t *tsamp;

static bool idlewake_insert_cell(struct statistic *s, const void *sample) {
    const libtop_psamp_t *psamp = sample;
    char buf[GENERIC_INT_SIZE];

	if(top_uinteger_format_result(buf, sizeof(buf),
				      psamp->power.task_platform_idle_wakeups,
				      psamp->p_power.task_platform_idle_wakeups,
				      psamp->b_power.task_platform_idle_wakeups)) {
		return true;
	}

	return generic_insert_cell(s, buf);
}

// cribbed from cpu_insert_cell
static bool powerscore_insert_cell(struct statistic *s, const void *sample) {
    const libtop_psamp_t *psamp = sample;
    char buf[10];
    unsigned long long elapsed_us = 0, used_us = 0, idlew = 0, taxed_us = 0;
    int whole = 0, part = 0;

    if(0 == psamp->p_seq || 0 == psamp->pid) { // kernel gets a free ride
        whole = 0;
        part = 0;

        if(-1 == snprintf(buf, sizeof(buf), "%d.%1d", whole, part))
            return true;

        return generic_insert_cell(s, buf);
    }

    uint64_t last_timens = 0;
    uint64_t last_total_timens = 0;
    switch(top_prefs_get_mode()) {
    case STATMODE_ACCUM:
        last_timens = tsamp->b_timens;
        last_total_timens = psamp->b_total_timens;
        idlew = psamp->power.task_platform_idle_wakeups -
                psamp->b_power.task_platform_idle_wakeups;
        break;

    case STATMODE_EVENT:
    case STATMODE_DELTA:
    case STATMODE_NON_EVENT:
        last_timens = tsamp->p_timens;
        last_total_timens = psamp->p_total_timens;
        idlew = psamp->power.task_platform_idle_wakeups -
                psamp->p_power.task_platform_idle_wakeups;
        break;

    default:
        fprintf(stderr, "unhandled STATMODE in %s\n", __func__);
        abort();
    }

    elapsed_us = (tsamp->timens - last_timens) / NSEC_PER_USEC;
    taxed_us = (unsigned long long)idlew * 500ULL;
    used_us = (psamp->total_timens - last_total_timens) / NSEC_PER_USEC +
            taxed_us;
      
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

static struct statistic_callbacks idlewake_callbacks = {
    .draw = generic_draw,
    .resize_cells = generic_resize_cells,
    .move_cells = generic_move_cells,
    .get_request_size = generic_get_request_size,
    .get_minimum_size = generic_get_minimum_size,
    .insert_cell = idlewake_insert_cell,
    .reset_insertion = generic_reset_insertion
};

static struct statistic_callbacks powerscore_callbacks = {
    .draw = generic_draw,
    .resize_cells = generic_resize_cells,
    .move_cells = generic_move_cells,
    .get_request_size = generic_get_request_size,
    .get_minimum_size = generic_get_minimum_size,
    .insert_cell = powerscore_insert_cell,
    .reset_insertion = generic_reset_insertion
};

struct statistic *top_idlewake_create(WINDOW *parent, const char *name) {
    return create_statistic(STATISTIC_PORTS, parent, NULL, &idlewake_callbacks,
			    name);
}

struct statistic *top_powerscore_create(WINDOW *parent, const char *name)
{
    return create_statistic(STATISTIC_PORTS, parent, NULL, &powerscore_callbacks,
			    name);
}

