/*
 *  dtrace_dyld.c
 *  dtrace
 *
 *  Created by luser on 3/9/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <mach-o/dyld.h>

#include "dtrace_dyld_types.h"

static mach_port_t mp;

static
void image_activity(const struct mach_header* mh, intptr_t vmaddr_slide)
{
	kern_return_t kr;
	rd_event_msg_t rdm;

	rdm.type = RD_DLACTIVITY;
	rdm.u.state = RD_CONSISTENT;
	
	kr = report_activity(mp, rdm);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "dyld_dtrace image_activity(): %s\n", mach_error_string(kr));
		exit(EXIT_FAILURE);
	}
}

static
void exitDTraceRPC(void)
{
	(void)mach_port_deallocate(mach_task_self(), mp);
}

#define MAX_BOOTSTRAP_NAME_CHARS 32 /* _DTRACE_DYLD_BOOTSTRAP_NAME with pid digits tacked on. */

static
void prepareDTraceRPC() __attribute__((constructor));

static
void prepareDTraceRPC()
{
	kern_return_t kr;
	rd_event_msg_t rdm;
	char bsname[MAX_BOOTSTRAP_NAME_CHARS];
	
	unsetenv("DYLD_INSERT_LIBRARIES"); /* children must not have this present in their env */

	(void) snprintf(bsname, sizeof(bsname), "%s_%d", _DTRACE_DYLD_BOOTSTRAP_NAME, getpid());
	
	kr = bootstrap_look_up(bootstrap_port, bsname, &mp);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "bootstrap_look_up(): %s\n", bootstrap_strerror(kr));
		exit(EXIT_FAILURE);
	}

	(void)atexit(exitDTraceRPC);
	
	_dyld_register_func_for_add_image( image_activity );
	_dyld_register_func_for_remove_image( image_activity );

	rdm.type = RD_PREINIT;
	rdm.u.state = RD_CONSISTENT;
	
	kr = report_activity(mp, rdm);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "dyld_dtrace prepareDTraceRPC(): %s\n", mach_error_string(kr));
		exit(EXIT_FAILURE);
	}

}

