/*
 * Copyright (c) 2005-2006 Apple Computer, Inc. All Rights Reserved.
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
 * testenv.c
 */

#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "testmore.h"
#include "testenv.h"

static int current_dir = -1;
static char scratch_dir[50];
static char *home_var;

int
rmdir_recursive(const char *path)
{
	char command_buf[256];
	if (strlen(path) + 10 > sizeof(command_buf) || strchr(path, '\''))
	{
		fprintf(stderr, "# rmdir_recursive: invalid path: %s", path);
		return -1;
	}

	sprintf(command_buf, "rm -rf '%s'", path);
	return system(command_buf);
}

int
tests_begin(int argc, char * const *argv)
{
	char library_dir[70];
	char preferences_dir[80];

	setup("tests_begin");

	/* Create scratch dir for tests to run in. */
	sprintf(scratch_dir, "/tmp/tst-%d", getpid());
	sprintf(library_dir, "%s/Library", scratch_dir);
	sprintf(preferences_dir, "%s/Preferences", library_dir);
	return (ok_unix(mkdir(scratch_dir, 0755), "mkdir") &&
		ok_unix(current_dir = open(".", O_RDONLY), "open") &&
		ok_unix(chdir(scratch_dir), "chdir") &&
		ok_unix(setenv("HOME", scratch_dir, 1), "setenv") &&
		/* @@@ Work around a bug that the prefs code in
		  libsecurity_keychain never creates the Library/Preferences
		  dir. */
		ok_unix(mkdir(library_dir, 0755), "mkdir") &&
		ok_unix(mkdir(preferences_dir, 0755), "mkdir") &&
		ok_unix(home_var = getenv("HOME"), "getenv"));
}

int
tests_end(int result)
{
	setup("tests_end");
	/* Restore previous cwd and remove scratch dir. */
	return (ok_unix(fchdir(current_dir), "fchdir") &&
		ok_unix(close(current_dir), "close") &&
		ok_unix(rmdir_recursive(scratch_dir), "rmdir_recursive"));
}
