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

#include <mach/clock_types.h>
#include <sys/time.h>
#include "timestat.h"
#include "libtop.h"
#include "generic.h"
#include "preferences.h"

static bool time_insert_cell(struct statistic *s, const void *sample) {
    const libtop_psamp_t *psamp = sample;
    unsigned int usec, sec, min, hour, day;
    char buf[GENERIC_INT_SIZE];

    uint64_t timens = 0;
    if(STATMODE_ACCUM == top_prefs_get_mode()) {
        timens = psamp->total_timens - psamp->b_total_timens;
    } else {
        timens = psamp->total_timens;
    }

    sec = timens / NSEC_PER_SEC;
    usec = (timens % NSEC_PER_SEC) / NSEC_PER_USEC ;
    min = sec / 60;
    hour = min / 60;
    day = hour / 24;
    
    if(min < 100) {
        snprintf(buf, sizeof(buf), "%02u:%02u.%02u", min, sec % 60,
                usec / 10000);
    } else if(hour < 100) {
        snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hour, min % 60, sec % 60);
    } else {
        snprintf(buf, sizeof(buf), "%u hrs", hour);
    }

    return generic_insert_cell(s, buf);
}

static struct statistic_callbacks callbacks = {
    .draw = generic_draw,
    .resize_cells = generic_resize_cells,
    .move_cells = generic_move_cells,
    .get_request_size = generic_get_request_size,
    .get_minimum_size = generic_get_minimum_size,
    .insert_cell = time_insert_cell,
    .reset_insertion = generic_reset_insertion
};

struct statistic *top_time_create(WINDOW *parent, const char *name) {
    return create_statistic(STATISTIC_TIME, parent, NULL, &callbacks, name);
}
