/*
 * Copyright (c) 2003-2004,2006-2007,2013 Apple Inc. All Rights Reserved.
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
 *
 * security.h
 */

#ifndef _SECURITYTOOL_H_
#define _SECURITYTOOL_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

typedef int(*command_func)(int argc, char * const *argv);

/* Entry in commands array for a command. */
typedef struct command
{
    const char *c_name;    /* name of the command. */
    command_func c_func;   /* function to execute the command. */
    const char *c_usage;   /* usage sting for command. */
    const char *c_help;    /* help string for (or description of) command. */
} command;

/*
 * The command array itself.
 * Add commands here at will.
 * Matching is done on a prefix basis.  The first command in the array
 * gets matched first.
 */
extern const command commands[];

/* Our one builtin command.
 */
int help(int argc, char * const *argv);
    
/* If 1 attempt to be as quiet as possible. */
extern int do_quiet;

/* If 1 attempt to be as verbose as possible. */
extern int do_verbose;

extern const char* prog_name;

__END_DECLS

#endif /*  _SECURITY_H_ */
