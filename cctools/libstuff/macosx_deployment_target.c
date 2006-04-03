/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
#ifndef RLD
#include <stdlib.h>
#include <string.h>
#include <mach/mach.h>
#include "stuff/errors.h"
#include "stuff/macosx_deployment_target.h"

struct macosx_deployment_target_pair {
    const char *name;
    enum macosx_deployment_target_value value;
};

static const struct macosx_deployment_target_pair
    macosx_deployment_target_pairs[] = {
    { "10.1", MACOSX_DEPLOYMENT_TARGET_10_1 },
    { "10.2", MACOSX_DEPLOYMENT_TARGET_10_2 },
    { "10.3", MACOSX_DEPLOYMENT_TARGET_10_3 },
    { "10.4", MACOSX_DEPLOYMENT_TARGET_10_4 },
    { "10.5", MACOSX_DEPLOYMENT_TARGET_10_5 },
    { NULL, 0 }
};

/* last value passed to put_macosx_deployment_target() */
static char *command_line_macosx_deployment_target = NULL;

/*
 * put_macosx_deployment_target() is called with the command line argument to
 * -macosx_version_min which the compiler uses to allow the user to asked for
 * a particular macosx deployment target on the command-line to override the
 * environment variable. This simpley saves away the value requested.  The
 * string passed is not copied. If NULL is passed, it removes any previous
 * setting.
 */
__private_extern__
void
put_macosx_deployment_target(
char *target)
{
	command_line_macosx_deployment_target = target;
}

/*
 * get_macosx_deployment_target() indirectly sets the value and the name with
 * the specified MACOSX_DEPLOYMENT_TARGET environment variable or the current
 * default if not specified for the cputype.
 */
__private_extern__
void
get_macosx_deployment_target(
enum macosx_deployment_target_value *value,
const char **name,
cpu_type_t cputype)
{
    unsigned long i;
    char *p;

	/* set the current default for the cputype */
	if(cputype == CPU_TYPE_I386){
	    *value = MACOSX_DEPLOYMENT_TARGET_10_4;
	    *name = "10.4";
	}
	else{
	    *value = MACOSX_DEPLOYMENT_TARGET_10_1;
	    *name = "10.1";
	}

	/*
	 * Pick up the Mac OS X deployment target environment variable.
	 */
	if(command_line_macosx_deployment_target != NULL)
	    p = command_line_macosx_deployment_target;
	else
	    p = getenv("MACOSX_DEPLOYMENT_TARGET");
	if(p != NULL){
	    for(i = 0; macosx_deployment_target_pairs[i].name != NULL; i++){
		if(strcmp(macosx_deployment_target_pairs[i].name, p) == 0){
		    *value = macosx_deployment_target_pairs[i].value;
		    *name = macosx_deployment_target_pairs[i].name;
		    break;
		}
	    }
	    if(macosx_deployment_target_pairs[i].name == NULL){
	        if(command_line_macosx_deployment_target != NULL)
		    warning("unknown -macosx_version_min parameter value: "
			    "%s ignored (using %s)", p, *name);
		else
		    warning("unknown MACOSX_DEPLOYMENT_TARGET environment "
			    "variable value: %s ignored (using %s)", p, *name);
	    }
	}
}
#endif /* !defined(RLD) */
