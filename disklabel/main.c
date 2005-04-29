/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/conf.h>
#include <sys/filio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <CoreFoundation/CoreFoundation.h>

#include "create.h"
#include "destroy.h"
#include "props.h"
#include "status.h"
#include "util.h"

char *gDeviceName;
int gDebug = 0, gVerbose = 0;

static void
usage(void) {
	static const char *ustr = "\
\t-status <diskname>\n\
\t-create <diskname> [-msize=<size>] [owner-uid=<uid>] [owner-gid=<gid>] \
[dev-name=<name>] [owner-mode=<mode>] [other-property=<value>]\n\
\t-destroy <diskname>\n\
\t[-properties] <diskname> [owner-uid[=<uid>]] [owner-gid[=<gid>]] \
[dev-name[=<name>]] [owner-mode[=<mode>]] [other-property[=<value>]]";

	errx(1, "usage: One of \n%s\n", ustr);
}

/*
 * There's probably a better way to do this using IOKit; when
 * I find out how, I'll make this code do it.
 */
static int
isDisk(const char *dev) {
	int fd;
	int retval = 0;
	int type;

	if (gDebug)
		return 1;

	fd = open(dev, O_RDONLY);
	if (fd == -1) {
		warn("cannot open %s", dev);
		goto done;
	}

	if (ioctl(fd, FIODTYPE, &type) == -1) {
		goto done;
	}

	if (type == D_DISK) {
		retval = 1;
	}
done:
	close(fd);
	return retval;
}

int main(int ac, char **argv) {
	char **args = NULL;
	int argLen;
	char *cp;
	char **av = argv;
	enum cmd  {
		CMD_UNKNOWN = 0,
		CMD_STATUS, CMD_CREATE, CMD_DESTROY, CMD_PROPS
	} cmd;

	if (ac == 1) {
		usage();
	}

	av++;

	while (*av != NULL) {
		if (!strcmp(av[0], "-D")) {
			gDebug++;
			av++;
			warnx("setting debug to level %d", gDebug);
			continue;
		}
		if (!strcmp(av[0], "-v")) {
			gVerbose++;
			av++;
			continue;
		}
		break;
	}
	cp = *av++;

	argLen = strlen(cp);

	if (strncasecmp(cp, "-status", argLen) == 0) {
		cmd = CMD_STATUS;
		gDeviceName = *av;
		args = av + 1;
	} else if (strncasecmp(cp, "-destroy", argLen) == 0) {
		cmd = CMD_DESTROY;
		gDeviceName = *av;
		args = av + 1;
	} else if (strncasecmp(cp, "-create", argLen) == 0) {
		cmd = CMD_CREATE;
		gDeviceName = *av;
		args = av + 1;
	} else if (strncasecmp(cp, "-properties", argLen) == 0 ||
		cp[0] != '-') {
		cmd = CMD_PROPS;
		if (cp[0] != '-') {
			gDeviceName = cp;
			args = av;
		} else {
			gDeviceName = *av;
			args = av + 1;
		}
	} else {
		cmd = CMD_UNKNOWN;
	}

	if (!isDisk(gDeviceName)) {
		errx(1, "%s must be a disk device", gDeviceName);
	}

	switch (cmd) {
	case CMD_UNKNOWN:	usage(); break;
	case CMD_STATUS:	doStatus(gDeviceName);	// no arguments
				break;
	case CMD_CREATE:	doCreate(gDeviceName, args);
				break;
	case CMD_DESTROY:	doDestroy(gDeviceName);  break;
	case CMD_PROPS:		doProps(gDeviceName, args);
				break;
	}
	return 0;
}

