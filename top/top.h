/*
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * System header includes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>

#define TOP_DBG
#ifndef TOP_DBG
/* Disable assertions. */
#ifndef NDEBUG
#define NDEBUG
#endif
#endif
#include <assert.h>

/*
 * Project includes.
 */
#include "libtop.h"

#include "disp.h"
#include "log.h"
#include "samp.h"

/* Fields by which process samples can be sorted. */
typedef enum {
	TOP_SORT_command,
	TOP_SORT_cpu,
	TOP_SORT_pid,
	TOP_SORT_prt,
	TOP_SORT_reg,
	TOP_SORT_rprvt,
	TOP_SORT_rshrd,
	TOP_SORT_rsize,
	TOP_SORT_th,
	TOP_SORT_time,
	TOP_SORT_uid,
	TOP_SORT_username,
	TOP_SORT_vprvt,
	TOP_SORT_vsize
} top_sort_key_t;

/* Return a pointer to a string representation of a sorting key. */
const char *
top_sort_key_str(top_sort_key_t a_key, boolean_t a_ascend);

/*
 * Program options.
 */

/* Event counting mode. [aden]. */
extern char		top_opt_c;

/* Report shared library statistics. */
extern boolean_t	top_opt_f;

/* Forced interactive mode. */
extern boolean_t	top_opt_L;

/* Logging mode. */
extern boolean_t	top_opt_l;

/* Number of log samples (if opt_l). */
extern unsigned		top_opt_l_samples;

/* Number of processes to display. */
extern unsigned		top_opt_n;
#define TOP_MAX_NPROCS	0x7fffffff

/* Secondary sorting key. */
extern top_sort_key_t top_opt_O;

/* Ascend/descend for opt_O. */
extern boolean_t	top_opt_O_ascend;

/* Primary sorting key. */
extern top_sort_key_t top_opt_o;

/* Ascend/descend for opt_o. */
extern boolean_t	top_opt_o_ascend;

/* Report memory object map for each process. */
extern boolean_t	top_opt_r;

/* Sample delay, in seconds. */
extern unsigned		top_opt_s;

/* Translate uid numbers to usernames. */
extern boolean_t	top_opt_t;

/* Only display processes owned by opt_uid. */
extern boolean_t	top_opt_U;

/* Display procs owned by uid (if opt_U). */
extern boolean_t	top_opt_U_uid;

/* Display deltas in wide format. */
extern boolean_t	top_opt_w;

#ifdef TOP_DEPRECATED
/* Enable deprecated functionality and display format. */
extern boolean_t	top_opt_x;
#endif
