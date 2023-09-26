/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * NOTE: The contents of this file are constructed by genwrap(8); it should not
 * be manually modified.
 */

#include <sys/param.h>

#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xpc/xpc.h>

#if defined(WRAPPER_ANALYTICS_IDENT) && !defined(WRAPPER_ANALYTICS_TESTING)
#include <CoreAnalytics/CoreAnalytics.h>
#endif
#ifdef WRAPPER_NEEDS_XCSELECT
#include <xcselect_private.h>
#endif

#ifndef nitems
#define	nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

#if !defined(WRAPPER_ANALYTICS_IDENT) && defined(WRAPPER_ANALYTICS_TESTING)
#error shim was improperly modified to remove the analytics identifier
#endif

/*
 * The wrapper generator will provide an array of struct application that we
 * will sift through to determine which to choose.  We'll default to the first
 * element of the array, but may choose to use one of the others depending on
 * arguments used or if the environment is set to force one.
 */
struct application {
	const char		*app_name;
	const char		*app_path;

	/*
	 * If optstr is set, we'll use getopt(3) or getopt_long(3) to determine
	 * if we can use this application as the backend.
	 */
	const char		*app_optstr;
	const struct option	*app_longopts;

	/*
	 * Relative paths are relative to cwd, rather than to the selected
	 * developer tools.
	 */
	bool			 app_path_relcwd;

	/*
	 * Options are specified for logging purposes only, not to be considered
	 * for selecting an application to execute.
	 */
	bool			 app_opts_logonly;
};
