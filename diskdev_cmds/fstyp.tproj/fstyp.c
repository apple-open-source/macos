/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysexits.h>

#define FSTYP_PREFIX	"fstyp_"
#define MAX_PATH_LEN	80
#define MAX_CMD_LEN	(MAX_PATH_LEN * 2)
#define NULL_REDIRECTION ">/dev/null 2>&1"

void usage(void);
int select_fstyp(const struct dirent * dp);
int test(const char *dir, const struct dirent * dp, const char *dev);
void dealloc(struct dirent ** dpp, int numElems);

char           *progname;

/*
 * The fstyp command iterates through the binary directories to look for
 * commands of the form fstyp_* and runs them, trying to find one that
 * matches the given device. Once one of the returns success, fstyp
 * prints out that file system type name and terminates. 1 is returned
 * if any match is found, and 0 is returned if no match is found.
 */

int
main(int argc, char *argv[])
{
	/* NULL-terminated list of directories to search for fstyp_* commands */
	const char     *DIRS[] = {	"/bin/",
					"/sbin/",
					"/usr/bin/",
					"/usr/sbin/",
					"/usr/local/bin/",
					"/usr/local/sbin/",
					NULL};

	int numMatches, i, j, found;
	struct stat sb;
	struct dirent **dpp;

	numMatches = 0;
	i = 0;
	j = 0;
	found = 0;

	if ((progname = strrchr(*argv, '/')))
		++progname;
	else
		progname = *argv;

	if (argc != 2) {
		usage();
		return EX_USAGE;
	}
	if (0 == stat(argv[1], &sb)) {
		for (i = 0; (!found && (NULL != DIRS[i])); i++) {
			/*
			 * scan DIRS[i] for files that start with
			 * "fstyp_"
			 */
			numMatches = scandir(DIRS[i], &dpp, select_fstyp, NULL);

			if (numMatches >= 0) {
				for (j = 0; (!found && (j < numMatches)); j++) {
					if (test(DIRS[i], dpp[j], argv[1]) == 1) {
						puts(dpp[j]->d_name + 6);

						found = 1;
					}
				}

				dealloc(dpp, numMatches);
				dpp = NULL;
			}
		}
	}
	return found;
}

int
select_fstyp(const struct dirent * dp)
{
	return ((dp != NULL) &&
		((dp->d_type == DT_REG) || (dp->d_type == DT_LNK)) &&
		(dp->d_namlen > strlen(FSTYP_PREFIX)) &&
		(!strncmp(FSTYP_PREFIX, dp->d_name, strlen(FSTYP_PREFIX))));
}

/* return dp if successful, else return NULL */
int
test(const char *dir, const struct dirent * dp, const char *dev)
{
	char cmd[MAX_CMD_LEN + 1] = {0};
	int status;
	FILE *fileptr;

	status = 0;

	/* + 1 for white space */
	if ((strlen(dir) + dp->d_namlen + 1 + strlen(dev) +
	    strlen(NULL_REDIRECTION)) > MAX_CMD_LEN) {
		return 0;
	}
	snprintf(cmd, sizeof(cmd), "%s%s %s", dir, dp->d_name, dev);

	if ((fileptr = popen(cmd, "r")) != NULL) {
		status = pclose(fileptr);

		if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) == 1) {
				return 1;
			}
		}
	}
	return 0;
}

void
dealloc(struct dirent ** dpp, int numElems)
{
	int i;

	for (i = 0; i < numElems; i++) {
		free(dpp[i]);
		dpp[i] = NULL;
	}

	free(dpp);

	return;
}


void
usage(void)
{
	fprintf(stdout, "usage: %s device\n", progname);
	return;
}
