/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
/*-
 * Copyright (c) 1987, 1990, 1993
 *     The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *     This product includes software developed by the University of
 *     California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1987, 1990, 1993\n\
       The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)update.c   8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>

#include "power_mgmt.h"
#include "disk_power.h"

extern char *optarg;

static void usage(const char *argv0) __attribute__((noreturn));
static int parsetime(const char *arg);

int main(int argc, char *const argv[])
{
	int normal_intr = 30;
	int save_pwr_intr = 30;
	int on_bat, disk_up, last_sync = 0, current_sync_interval = 0;

	if (argc > 3)
		usage(argv[0]);
	if (argc > 2)
		save_pwr_intr = parsetime(argv[2]);
	if (argc > 1)
		normal_intr = parsetime(argv[1]);

	if (normal_intr == -1 || save_pwr_intr == -1)
		usage(argv[0]);

	if (normal_intr > save_pwr_intr)
		normal_intr = save_pwr_intr;

	daemon(0, 0);

	for (;;) {
		if (last_sync >= current_sync_interval) {
			sync();
			last_sync = 0;
		}
		sleep(30);
		last_sync += 30;

		on_bat = on_battery_power();
		disk_up = is_disk_awake();

		current_sync_interval = normal_intr;
		if (on_bat && !disk_up)
			current_sync_interval = save_pwr_intr;
		
	}

}

static int parsetime(const char *arg)
{
	char *q = NULL;
	int r;

	r = strtol(arg, &q, 10);

	if (r < 1)
		return -1;
	if (arg == q)
		return -1;

	switch (*q) {
	case 'h':
		r *= 60;
	case 'm':
		r *= 60;
	case 's':
	case '\0':
		break;
	default:
		return -1;
	}

	return r;
}

static void usage(const char *argv0)
{
	fprintf(stderr, "usage: %s [normal_interval [power_saving_interval]]\n", argv0);
	exit(EXIT_FAILURE);
}
