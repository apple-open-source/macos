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

#include "top.h"

static boolean_t
log_p_skipl(void);
static boolean_t
log_p_printl(const char *a_format, ...);
static boolean_t
log_p_println(const char *a_format, ...);
static boolean_t
log_p_vprintln(boolean_t a_newline, const char *a_format, va_list a_p);

/* Main entry point for logging. */
boolean_t
log_run(void)
{
	boolean_t	retval;
	unsigned	i, remain;

	if (samp_init(log_p_skipl, log_p_printl, log_p_println, log_p_vprintln,
	    log_p_vprintln)) {
		retval = TRUE;
		goto RETURN;
	}

	i = 0;
	while (1) {
		/* Take a sample and print it. */
		if (samp_run()) {
			retval = TRUE;
			goto RETURN;
		}

		/* Flush the output. */
		if (fflush(stdout) == EOF) {
			retval = TRUE;
			goto RETURN;
		}

		/*
		 * Increment the loop counter.  Do this here instead of using a
		 * for loop so that there is no pause after the last sample.
		 */
		i++;
		if (top_opt_l && i == top_opt_l_samples) {
			break;
		}

		/* Print a blank line to separate samples. */
		if (log_p_skipl()) {
			retval = TRUE;
			goto RETURN;
		}

		/*
		 * Loop on sleep() until we've slept for the full sample
		 * interval.
		 */
		for (remain = top_opt_s; (remain = sleep(remain)) != 0;) {
			/* Do nothing. */
		}
	}

	samp_fini();
	retval = FALSE;
	RETURN:
	return retval;
}

/* Print a blank line. */
static boolean_t
log_p_skipl(void)
{
	boolean_t	retval;

	if (fwrite("\n", 1, 1, stdout) != 1) {
		retval = TRUE;
		goto RETURN;
	}

	retval = FALSE;
	RETURN:
	return retval;
}

/* Print a formatted string. */
static boolean_t
log_p_printl(const char *a_format, ...)
{
	boolean_t	retval;
	va_list		ap;

	va_start(ap, a_format);
	retval = log_p_vprintln(FALSE, a_format, ap);
	va_end(ap);

	return retval;
}

/* Print a formatted string, followed by a newline. */
static boolean_t
log_p_println(const char *a_format, ...)
{
	boolean_t	retval;
	va_list		ap;

	va_start(ap, a_format);
	retval = log_p_vprintln(TRUE, a_format, ap);
	va_end(ap);

	return retval;
}

/* Print a formatted string, and append a newline if a_newline is TRUE. */
static boolean_t
log_p_vprintln(boolean_t a_newline, const char *a_format, va_list a_p)
{
	boolean_t	retval;

	if (vprintf(a_format, a_p) == -1) {
		retval = TRUE;
		goto RETURN;
	}

	if (a_newline) {
		if (fwrite("\n", 1, 1, stdout) != 1) {
			retval = TRUE;
			goto RETURN;
		}
	}

	retval = FALSE;
	RETURN:
	return retval;
}
