#!/usr/sbin/dtrace -F -s

/* Simple Directory Services API profiler. We trace each call into the
 * DirectoryServices module and show the elapsed time.
 *
 * Copyright (C) 2007 Apple Inc. All Rights Reserved.
 *
 * Usage: ds-profile.d <PID>
 */

BEGIN { printf("$1 is %d\n", $1); }

pid$1:DirectoryService::entry
{
	printf("ustackdepth = %u\n", (unsigned)ustackdepth);
	self->start[ustackdepth] = timestamp; /* nanoseconds */
}

pid$1:DirectoryService::return
{
	/* We need to tweak the stackdepth because we end up here
	 * AFTER the matching function return.
	 */
	elapsed = timestamp - self->start[ustackdepth + 1];
	printf("ustackdepth = %u %lluns\n", (unsigned)ustackdepth + 1, (unsigned long long)elapsed);
}
